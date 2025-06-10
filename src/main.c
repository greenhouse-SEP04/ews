/*********************************************************************
 *  Smart Greenhouse – full firmware (SEP4 drivers, dynamic settings)
 *********************************************************************/

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

/* ---------------- User items ------------------------------------ */
#define WIFI_SSID            "YOUR-SSID"
#define WIFI_PASS            "YOUR-PASS"
#define API_HOST             "api.example.com"
#define API_PORT             443
#define CFG_USE_EEPROM       1           /* 0 = RAM-only          */
/* ---------------------------------------------------------------- */

#include "pc_comm.h"
#include "wifi.h"
#include "timestamp.h"
#include "clock.h"

/* sensors */
#include "dht11.h"
#include "soil.h"
#include "light.h"
#include "hc_sr04.h"
#include "pir.h"
#include "adxl345.h"
/* actuators / ui */
#include "pump.h"
#include "servo.h"
#include "lightbulb.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"
#include "buttons.h"
#include "tone.h"
/* scheduler */
#include "periodic_task.h"

/* ====== ENDPOINT PATHS ============================================== */
#define SETTINGS_EP        "/v1/settings"
#define TELEMETRY_EP       "/v1/telemetry"
#define COMMAND_EP         "/v1/commands"
#define DEVICE_LOGIN_EP    "/v1/device/login"
#define DEVICE_REGISTER_EP "/v1/device/register"
/* ==================================================================== */

/* ====== CONFIG STRUCT =================================================== */
typedef struct {
    bool     watering_manual;
    uint8_t  soil_min, soil_max;
    uint16_t fert_hours;
    bool     lighting_manual;
    uint16_t lux_low;
    uint8_t  on_h, off_h;
} gh_cfg_t;

static volatile gh_cfg_t CFG = {
    .watering_manual=false, .soil_min=35, .soil_max=55,
    .fert_hours=336,
    .lighting_manual=false, .lux_low=300, .on_h=18, .off_h=6
};

/* ---------- EEPROM persistence (optional) ------------------------------ */
#if CFG_USE_EEPROM
EEMEM static gh_cfg_t ee_cfg;
static void cfg_load(void){ eeprom_read_block((void*)&CFG,&ee_cfg,sizeof(CFG)); }
static void cfg_save(void){ eeprom_update_block((const void*)&CFG,&ee_cfg,sizeof(CFG)); }
#else
#define cfg_load()
#define cfg_save()
#endif

/* ====== GLOBAL STATE ==================================================== */
static volatile Clock clk;

volatile uint8_t  S_temp=0, S_hum=0, S_soil=0;
volatile uint16_t S_lux=0, S_lvl_cm=0;
volatile int16_t  S_ax=0,S_ay=0,S_az=0;
volatile bool     S_motion=false, S_tamper=false;

volatile bool A_pump=false, A_light=false, A_fert_done=false;
volatile uint32_t hours_since_fert=0;

/* I/O buffers */
static char txbuf[512], rxbuf[256], json[384];

/* runtime identities */
static char device_mac[18]    = {0};   /* "AA:BB:CC:DD:EE:FF" */
static char g_auth_token[128] = {0};   /* "Bearer …"          */

/* ====== HELPERS ========================================================= */
static void dbg(const char *fmt, ...)
{
    char b[128];
    va_list ap; va_start(ap,fmt); vsnprintf(b,sizeof(b),fmt,ap); va_end(ap);
    pc_comm_send_string_blocking(b);
}

/* ------------------------------------------------------------------------ */
/*  HTTP POST helper (returns body in rxbuf)                                */
/* ------------------------------------------------------------------------ */
static bool http_post(const char *path,const char *body,char *rxbuf,size_t rxlen)
{
    char ip[32]="";
    if (wifi_command_get_ip_from_URL(API_HOST,ip)!=WIFI_OK) return false;
    if (wifi_command_create_TCP_connection(ip,API_PORT,NULL,rxbuf)!=WIFI_OK) return false;

    int blen=strlen(body);
    int hlen=snprintf(txbuf,sizeof(txbuf),
        "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\n\r\n", path,API_HOST,blen);
    wifi_command_TCP_transmit((uint8_t*)txbuf,hlen);
    wifi_command_TCP_transmit((uint8_t*)body,blen);
    _delay_ms(500);
    wifi_command_close_TCP_connection();

    char *b=strstr(rxbuf,"\r\n\r\n"); if(!b) return false;
    memmove(rxbuf,b+4,strlen(b+4)+1);
    return true;
}

/* ------------------------------------------------------------------------ */
/*  Authenticate with MAC + “worker”                                        */
/* ------------------------------------------------------------------------ */
static void authenticate_device(void)
{
    char payload[64];
    snprintf(payload,sizeof(payload),
             "{\"username\":\"%s\",\"password\":\"worker\"}",device_mac);

    if (http_post(DEVICE_LOGIN_EP,payload,rxbuf,sizeof(rxbuf))) {
        char *p=strstr(rxbuf,"\"token\":\"");
        if(p){ p+=9; char *q=strchr(p,'"');
            if(q&&(q-p)<(int)(sizeof(g_auth_token)-8)){
                snprintf(g_auth_token,sizeof(g_auth_token),
                         "Bearer %.*s",(int)(q-p),p);
                dbg("AUTH: login ok\n"); return;}}}

    memset(rxbuf,0,sizeof(rxbuf));
    if (http_post(DEVICE_REGISTER_EP,payload,rxbuf,sizeof(rxbuf))) {
        char *p=strstr(rxbuf,"\"token\":\"");
        if(p){ p+=9; char *q=strchr(p,'"');
            if(q&&(q-p)<(int)(sizeof(g_auth_token)-8)){
                snprintf(g_auth_token,sizeof(g_auth_token),
                         "Bearer %.*s",(int)(q-p),p);
                dbg("AUTH: registered ok\n"); return;}}}

    dbg("AUTH: failed\n");
}

/* ------------------------------------------------------------------------ */
/*  CONFIG fetch / parse                                                    */
/* ------------------------------------------------------------------------ */
static void cfg_parse_json(const char *js)
{
    char *p;
    if ((p=strstr(js,"\"watering\""))) {
        CFG.watering_manual = strstr(p,"\"manual\"")!=NULL;
        if ((p=strstr(p,"soilMin"))) CFG.soil_min = atoi(p+8);
        if ((p=strstr(p,"soilMax"))) CFG.soil_max = atoi(p+8);
    }
    if ((p=strstr(js,"\"fertilizer\"")) && (p=strstr(p,"hours")))
        CFG.fert_hours = atoi(p+6);

    if ((p=strstr(js,"\"lighting\""))) {
        CFG.lighting_manual = strstr(p,"\"manual\"")!=NULL;
        if ((p=strstr(p,"luxLow"))) CFG.lux_low = atoi(p+7);
        if ((p=strstr(p,"\"on\"")))  CFG.on_h  = atoi(p+4);
        if ((p=strstr(p,"\"off\""))) CFG.off_h = atoi(p+5);
    }
}

static void fetch_settings(void)
{
    char ip[32]="";
    if (wifi_command_get_ip_from_URL(API_HOST,ip)!=WIFI_OK) return;
    if (wifi_command_create_TCP_connection(ip,API_PORT,NULL,rxbuf)!=WIFI_OK) return;

    snprintf(txbuf,sizeof(txbuf),
        "GET %s?dev=%s HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\n"
        "Connection: close\r\n\r\n",
        SETTINGS_EP,device_mac,API_HOST,g_auth_token);
    wifi_command_TCP_transmit((uint8_t*)txbuf,strlen(txbuf));
    wifi_command_close_TCP_connection();

    char *body=strstr(rxbuf,"\r\n\r\n");
    if(body){ cfg_parse_json(body+4); cfg_save(); }
    memset(rxbuf,0,sizeof(rxbuf));
}

/* ====== TASKS =========================================================== */
static void task_tick_1s(void)
{
    clock_tick(&clk);
    static bool hb; hb=!hb; hb?leds_turnOn(4):leds_turnOff(4);
    display_int(clk.second);
}

static void task_sample_5s(void)
{
    uint8_t d;
    dht11_get(&S_hum,&d,&S_temp,&d);
    S_soil   = soil_read();
    S_lux    = light_read();
    S_lvl_cm = hc_sr04_takeMeasurement();
    adxl345_read_xyz(&S_ax,&S_ay,&S_az);
    if(abs(S_ax)>800||abs(S_ay)>800||abs(S_az-1024)>800) S_tamper=true;
}

static inline bool within_window(uint8_t h,uint8_t on,uint8_t off)
{ return (h>=on)||(h<off); }

static void task_logic_5s(void)
{
    if(!CFG.watering_manual){
        if(!A_pump&&S_soil<CFG.soil_min&&S_lvl_cm>5){
            A_pump=true; pump_on(); leds_turnOn(1);}
        if(A_pump&&S_soil>CFG.soil_max){
            A_pump=false; pump_off(); leds_turnOff(1);}
    }

    uint8_t h=clk.hour;
    if(!CFG.lighting_manual){
        bool dark=S_lux<CFG.lux_low;
        if(dark&&within_window(h,CFG.on_h,CFG.off_h)&&!A_light){
            lightbulb_on();A_light=true;leds_turnOn(2);}
        if((!dark||!within_window(h,CFG.on_h,CFG.off_h))&&A_light){
            lightbulb_off();A_light=false;leds_turnOff(2);}
    }

    if(!A_fert_done&&hours_since_fert>=CFG.fert_hours){
        servo(90);_delay_ms(600);servo(0);
        A_fert_done=true;hours_since_fert=0;
    }

    if(S_motion&&(h>=22||h<6))
        for(uint8_t i=0;i<6;i++){leds_toggle(3);buzzer_beep();_delay_ms(120);}
    if(S_tamper){buzzer_beep();leds_turnOn(3);}
    S_motion=false;
}
static void pir_cb(void){ S_motion=true; }

/* ====== TELEMETRY & COMMANDS (60 s) ==================================== */
static void task_cloud_60s(void)
{
    char ts[32]; clock_to_string(&clk,ts,sizeof(ts));

    /* pump / bulb removed */
    snprintf(json,sizeof(json),
        "{\"ts\":\"%s\",\"temp\":%d,\"hum\":%d,\"soil\":%d,"
        "\"lux\":%u,\"lvl\":%u}",
        ts,S_temp,S_hum,S_soil,S_lux,S_lvl_cm);

    char ip[32]="";
    if (wifi_command_get_ip_from_URL(API_HOST,ip)!=WIFI_OK) return;
    if (wifi_command_create_TCP_connection(ip,API_PORT,NULL,rxbuf)!=WIFI_OK) return;

    /* POST telemetry */
    snprintf(txbuf,sizeof(txbuf),
        "POST %s?dev=%s HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s",
        TELEMETRY_EP,device_mac,API_HOST,g_auth_token,
        (int)strlen(json),json);
    wifi_command_TCP_transmit((uint8_t*)txbuf,strlen(txbuf));

    /* GET ML command */
    snprintf(txbuf,sizeof(txbuf),
        "GET %s?dev=%s HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\n\r\n",
        COMMAND_EP,device_mac,API_HOST,g_auth_token);
    wifi_command_TCP_transmit((uint8_t*)txbuf,strlen(txbuf));

    wifi_command_close_TCP_connection();

    if(strstr(rxbuf,"WATER:ON")){A_pump=false;S_soil=0;}
    if(strstr(rxbuf,"FERT:ON")) A_fert_done=false;
    memset(rxbuf,0,sizeof(rxbuf));
}

/* ====== INIT =========================================================== */
static void init_all(void)
{
    pc_comm_init(115200,NULL); cfg_load();

    buttons_init(); leds_init(); display_init(); buzzer_beep();
    dht11_init(); soil_init(); light_init();
    hc_sr04_init(); adxl345_init(); pir_init(pir_cb);
    pump_init(); servo(0); lightbulb_init(); tone_init();
    clock_init(&clk,2025,6,10,12,0,0);

    wifi_init(); wifi_command_disable_echo();
    wifi_command_set_mode_to_1(); wifi_command_set_to_single_Connection();
    wifi_command_join_AP(WIFI_SSID,WIFI_PASS); _delay_ms(500);

    if(wifi_command_get_MAC(device_mac)==WIFI_OK) dbg("MAC: %s\n",device_mac);
    else{ dbg("MAC: ERR\n"); strcpy(device_mac,"UNKNOWN"); }

    authenticate_device();
    tone_play_starwars();
    fetch_settings();
}

/* ====== TASK SCHEDULER ================================================== */
static void start_tasks(void)
{
    periodic_task_init_a(task_tick_1s,   1000);
    periodic_task_init_b(task_sample_5s, 5000);
    periodic_task_init_c(task_logic_5s,  5000);
    periodic_task_init_d(task_cloud_60s,60000);
    periodic_task_init_d(fetch_settings,3600000);
}

/* ====== MAIN =========================================================== */
int main(void)
{
    init_all(); start_tasks(); sei();
    for(;;){
        poll_buttons();
        if(A_pump&&S_lvl_cm<=5){pump_off();A_pump=false;buzzer_beep();}
    }
}
