/*********************************************************************
 *  Smart Greenhouse – full firmware  (no command polling)
 *  TOKEN-EXPIRY SAFE  (2025-06-10)
 *  Rev: 2025-06-18  – GET-based ML watering
 *  • maxPumpSeconds fail-safe
 *  • security.armed + alarmWindow (HH:MM-HH:MM)
 *  • cfgRev tracking (meta.updatedAt)
 *  • Accelerometer X/Y/Z in telemetry ("accel")
 *  • BTN-1: force watering   BTN-2: toggle light manual/auto
 *    BTN-3: silence alarm    BTN-4: one-shot fertilizer dispense
 *  • LEDs : L1=pump  L2=light  L3=alarm  L4=heartbeat
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

 /* ---------------- Wi-Fi + API hosts -------------------------------- */
#define WIFI_SSID   "YOUR-SSID"
#define WIFI_PASS   "YOUR-PASS"

#define API_HOST    "api.example.com"
#define API_PORT    443

/* ML predictor (GET) */
#define PREDICT_HOST "soilmoist.com"
#define PREDICT_PORT 443
#define PREDICT_EP   "/v1/predict"

#define CFG_USE_EEPROM 1          /* 0 = RAM-only                  */
/* ------------------------------------------------------------------ */

#include "pc_comm.h"
#include "wifi.h"
#include "clock.h"

/* sensors */
#include "dht11.h"
#include "soil.h"
#include "light.h"
#include "hc_sr04.h"
#include "pir.h"
#include "adxl345.h"
/* actuators / UI */
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

/* endpoints we still use */
#define SETTINGS_EP   "/v1/settings"
#define TELEMETRY_EP  "/v1/telemetry"
#define LOGIN_EP      "/v1/device/login"
#define REGISTER_EP   "/v1/device/register"

/* ================= CONFIG STRUCT ================================= */
typedef struct {
    bool     watering_manual;
    uint8_t  soil_min, soil_max;
    uint16_t fert_hours;
    uint16_t max_pump_seconds;

    bool     lighting_manual;
    uint16_t lux_low;
    uint8_t  on_h, off_h;

    bool     security_armed;
    uint8_t  alarm_start_h, alarm_start_m;
    uint8_t  alarm_end_h, alarm_end_m;
} gh_cfg_t;

static volatile gh_cfg_t CFG = {
    .watering_manual = false, .soil_min = 35, .soil_max = 55,
    .fert_hours = 336, .max_pump_seconds = 60,
    .lighting_manual = false, .lux_low = 300, .on_h = 18, .off_h = 6,
    .security_armed = true, .alarm_start_h = 22, .alarm_start_m = 0,
    .alarm_end_h = 6,  .alarm_end_m = 0
};

static char cfg_rev[32] = "1970-01-01T00:00:00Z";

/* ---------- EEPROM persistence ---------------------------------- */
#if CFG_USE_EEPROM
EEMEM static gh_cfg_t ee_cfg;
EEMEM static char     ee_cfg_rev[32];
static void cfg_load(void) {
    eeprom_read_block((void*)&CFG, &ee_cfg, sizeof(CFG));
    eeprom_read_block((void*)cfg_rev, ee_cfg_rev, sizeof(cfg_rev));
}
static void cfg_save(void) {
    eeprom_update_block((void*)&CFG, &ee_cfg, sizeof(CFG));
    eeprom_update_block((void*)cfg_rev, ee_cfg_rev, sizeof(cfg_rev));
}
#else
#define cfg_load()
#define cfg_save()
#endif
/* ---------------------------------------------------------------- */

/* ================= RUNTIME STATE ================================ */
static volatile Clock clk;

volatile uint8_t  S_temp = 0, S_hum = 0, S_soil = 0;
volatile uint16_t S_lux = 0, S_lvl_cm = 0;
volatile int16_t  S_ax = 0, S_ay = 0, S_az = 0;
volatile bool     S_motion = false, S_tamper = false;

static bool A_pump = false, A_light = false, alarm_active = false;
static bool A_fert_done = false;
static uint16_t pump_runtime_s = 0;
static uint32_t hours_since_fert = 0;

/* ML flag set by predictor task */
static bool ml_recommend_water = false;

static char device_mac[18] = "";
static char g_auth_token[128] = "";

static char txbuf[512], rxbuf[512], json[768];

/* ===================== HELPERS ================================== */
static void dbg(const char* fmt, ...) {
    char b[128]; va_list ap; va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    pc_comm_send_string_blocking(b);
}
#ifndef buttons_4_pressed
static inline uint8_t buttons_4_pressed(void) { return 0; }
#endif

static bool parse_hhmm(const char* s, uint8_t* h, uint8_t* m) {
    if (strlen(s) < 5) return false;
    *h = (uint8_t)atoi(s); *m = (uint8_t)atoi(s + 3); return true;
}
static bool time_in_window(uint16_t cur, uint16_t st, uint16_t en) {
    if (st <= en) return (cur >= st && cur < en);
    return (cur >= st || cur < en);
}
static inline bool in_light_window(uint8_t h, uint8_t on, uint8_t off) {
    return (h >= on) || (h < off);
}

/* ============ BASIC HTTP (GET + POST) =========================== */
static bool http_basic_get(const char* host, uint16_t port,
    const char* path,
    char* buf, size_t len) {
    char ip[32] = "";
    if (wifi_command_get_ip_from_URL((char*)host, ip) != WIFI_OK) return false;
    if (wifi_command_create_TCP_connection(ip, port, NULL, buf) != WIFI_OK) return false;
    int hl = snprintf(txbuf, sizeof(txbuf),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);
    wifi_command_TCP_transmit((uint8_t*)txbuf, hl);
    _delay_ms(500);
    wifi_command_close_TCP_connection();
    char* p = strstr(buf, "\r\n\r\n"); if (!p) return false;
    memmove(buf, p + 4, strlen(p + 4) + 1);
    return true;
}
static bool http_basic_post(const char* host, uint16_t port,
    const char* path, const char* body,
    char* buf, size_t len) {
    char ip[32] = "";
    if (wifi_command_get_ip_from_URL((char*)host, ip) != WIFI_OK) return false;
    if (wifi_command_create_TCP_connection(ip, port, NULL, buf) != WIFI_OK) return false;
    int bl = strlen(body);
    int hl = snprintf(txbuf, sizeof(txbuf),
        "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n", path, host, bl);
    wifi_command_TCP_transmit((uint8_t*)txbuf, hl);
    wifi_command_TCP_transmit((uint8_t*)body, bl);
    _delay_ms(500);
    wifi_command_close_TCP_connection();
    char* p = strstr(buf, "\r\n\r\n"); if (!p) return false;
    memmove(buf, p + 4, strlen(p + 4) + 1);
    return true;
}
/* auth helpers identical to previous version (POST login/register, GET/POST with Bearer) */
static int http_auth_xfer(bool is_post, const char* path_q,
    const char* body, char* buf, size_t len) {
    char ip[32] = "";
    if (wifi_command_get_ip_from_URL(API_HOST, ip) != WIFI_OK) return -1;
    if (wifi_command_create_TCP_connection(ip, API_PORT, NULL, buf) != WIFI_OK) return -1;
    int bl = body ? strlen(body) : 0;
    int hl = snprintf(txbuf, sizeof(txbuf),
        "%s %s HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        is_post ? \"POST\":\"GET\",path_q,API_HOST,g_auth_token,bl);
        wifi_command_TCP_transmit((uint8_t*)txbuf, hl);
    if (is_post && bl) wifi_command_TCP_transmit((uint8_t*)body, bl);
    _delay_ms(500);
    wifi_command_close_TCP_connection();
    int status = 0; char* l = strstr(buf, "HTTP/"); if (l) sscanf(l, "HTTP/%*s %d", &status);
    char* p = strstr(buf, "\r\n\r\n"); if (p) memmove(buf, p + 4, strlen(p + 4) + 1); else buf[0] = '\0';
    return status;
}
static int http_get_auth(const char* path_q) { return http_auth_xfer(false, path_q, NULL, rxbuf, sizeof(rxbuf)); }
static int http_post_auth(const char* path_q, const char* body) { return http_auth_xfer(true, path_q, body, rxbuf, sizeof(rxbuf)); }

/* ---------- AUTHENTICATE DEVICE -------------------------------- */
static void authenticate_device(void) {
    char payload[64]; snprintf(payload, sizeof(payload),
        "{\"username\":\"%s\",\"password\":\"worker\"}", device_mac);
    if (http_basic_post(API_HOST, API_PORT, LOGIN_EP, payload, rxbuf, sizeof(rxbuf))) {
        char* p = strstr(rxbuf, "\"token\":\""); if (p) {
            p += 9; char* q = strchr(p, '\"');
            if (q && (q - p) < (int)(sizeof(g_auth_token) - 8)) {
                snprintf(g_auth_token, sizeof(g_auth_token), "Bearer %.*s", (int)(q - p), p);
                dbg("AUTH login OK\n"); return;
            }
        }
    }
    memset(rxbuf, 0, sizeof(rxbuf));
    if (http_basic_post(API_HOST, API_PORT, REGISTER_EP, payload, rxbuf, sizeof(rxbuf))) {
        char* p = strstr(rxbuf, "\"token\":\""); if (p) {
            p += 9; char* q = strchr(p, '\"');
            if (q && (q - p) < (int)(sizeof(g_auth_token) - 8)) {
                snprintf(g_auth_token, sizeof(g_auth_token), "Bearer %.*s", (int)(q - p), p);
                dbg("AUTH register OK\n"); return;
            }
        }
    }
    dbg("AUTH failed\n");
}

/* ---------- SETTINGS FETCH / PARSE ------------------------------ */
static void cfg_parse_json(const char* js) {
    char* p;
    if ((p = strstr(js, "\"watering\""))) {
        CFG.watering_manual = strstr(p, "\"manual\":true") != NULL;
        if ((p = strstr(p, "soilMin"))) CFG.soil_min = atoi(p + 8);
        if ((p = strstr(p, "soilMax"))) CFG.soil_max = atoi(p + 8);
        if ((p = strstr(p, "maxPumpSeconds"))) CFG.max_pump_seconds = (uint16_t)atoi(p + 15);
        if ((p = strstr(p, "fertHours")))      CFG.fert_hours = (uint16_t)atoi(p + 10);
    }
    if ((p = strstr(js, "\"lighting\""))) {
        CFG.lighting_manual = strstr(p, "\"manual\":true") != NULL;
        if ((p = strstr(p, "luxLow"))) CFG.lux_low = (uint16_t)atoi(p + 7);
        if ((p = strstr(p, "onHour"))) CFG.on_h = (uint8_t)atoi(p + 7);
        if ((p = strstr(p, "offHour")))CFG.off_h = (uint8_t)atoi(p + 8);
    }
    if ((p = strstr(js, "\"security\""))) {
        CFG.security_armed = strstr(p, "\"armed\":true") != NULL;
        if ((p = strstr(p, "alarmWindow"))) {
            if ((p = strstr(p, "start\""))) {
                char t[6] = { 0 }; strncpy(t, p + 8, 5);
                parse_hhmm(t, &CFG.alarm_start_h, &CFG.alarm_start_m);
            }
            if ((p = strstr(p, "end\""))) {
                char t[6] = { 0 }; strncpy(t, p + 6, 5);
                parse_hhmm(t, &CFG.alarm_end_h, &CFG.alarm_end_m);
            }
        }
    }
    if ((p = strstr(js, "updatedAt\""))) {
        p += 11; char* q = strchr(p, '\"');
        if (q && (q - p) < (int)sizeof(cfg_rev)) { memcpy(cfg_rev, p, q - p); cfg_rev[q - p] = '\0'; }
    }
}
static void fetch_settings(void) {
    char path[64]; snprintf(path, sizeof(path), "%s?dev=%s", SETTINGS_EP, device_mac);
    int s = http_get_auth(path);
    if (s >= 200 && s < 300) { cfg_parse_json(rxbuf); cfg_save(); }
    else dbg("SET HTTP %d\n", s);
    memset(rxbuf, 0, sizeof(rxbuf));
}

/* ---------- ML PREDICT (GET) ------------------------------------ */
static bool ml_predict_water(void) {
    char path[128]; snprintf(path, sizeof(path), "%s?dev=%s", PREDICT_EP, device_mac);
    if (!http_basic_get(PREDICT_HOST, PREDICT_PORT, path, rxbuf, sizeof(rxbuf)))
        return false;
    char* p = strstr(rxbuf, "\"recommendWater\":");
    if (!p) return false;
    return strstr(p, "true") != NULL;
}

/* ==================== TASKS ===================================== */
static void task_tick_1s(void) {
    clock_tick(&clk);
    static bool hb; hb = !hb; hb ? leds_turnOn(4) : leds_turnOff(4);
    display_int(clk.second);
    if (A_pump) pump_runtime_s++;
}
static void task_sample_5s(void) {
    uint8_t d; dht11_get(&S_hum, &d, &S_temp, &d);
    S_soil = soil_read(); S_lux = light_read();
    S_lvl_cm = hc_sr04_takeMeasurement();
    adxl345_read_xyz(&S_ax, &S_ay, &S_az);
    if (abs(S_ax) > 800 || abs(S_ay) > 800 || abs(S_az - 1024) > 800) S_tamper = true;
}
static void task_logic_5s(void) {
    /* -------- WATERING ---------- */
    if (!CFG.watering_manual) {
        bool need = (S_soil < CFG.soil_min) || ml_recommend_water;
        if (!A_pump && need && S_lvl_cm > 5) {
            A_pump = true; pump_on(); leds_turnOn(1); pump_runtime_s = 0;
        }
        if (A_pump) {
            if (S_soil > CFG.soil_max || pump_runtime_s >= CFG.max_pump_seconds) {
                A_pump = false; pump_off(); leds_turnOff(1);
            }
        }
    }
    /* -------- LIGHTING ---------- */
    uint8_t h = clk.hour, m = clk.minute;
    if (!CFG.lighting_manual) {
        bool dark = S_lux < CFG.lux_low;
        if (dark && in_light_window(h, CFG.on_h, CFG.off_h) && !A_light) {
            lightbulb_on(); A_light = true; leds_turnOn(2);
        }
        if ((!dark || !in_light_window(h, CFG.on_h, CFG.off_h)) && A_light) {
            lightbulb_off(); A_light = false; leds_turnOff(2);
        }
    }
    /* -------- FERTILIZER --------- */
    if (!A_fert_done && hours_since_fert >= CFG.fert_hours) {
        servo(90); _delay_ms(600); servo(0);
        A_fert_done = true; hours_since_fert = 0;
    }
    /* -------- SECURITY ---------- */
    uint16_t cur = h * 60 + m;
    uint16_t st = CFG.alarm_start_h * 60 + CFG.alarm_start_m;
    uint16_t en = CFG.alarm_end_h * 60 + CFG.alarm_end_m;
    if ((S_motion || S_tamper) && CFG.security_armed && time_in_window(cur, st, en)) {
        alarm_active = true; S_motion = false; S_tamper = false;
    }
    if (alarm_active) { leds_toggle(3); buzzer_beep(); }
    else leds_turnOff(3);
}
static void task_predict_10m(void) {
    ml_recommend_water = ml_predict_water();
}
static void pir_cb(void) { S_motion = true; }

static void task_cloud_60s(void) {
    char ts[32]; clock_to_string(&clk, ts, sizeof(ts));
    snprintf(json, sizeof(json),
        "{\"ts\":\"%s\",\"cfgRev\":\"%s\",\"temp\":%d,\"hum\":%d,"
        "\"soil\":%d,\"lux\":%u,\"lvl\":%u,"
        "\"accel\":[%d,%d,%d],\"motion\":%s,\"tamper\":%s}",
        ts, cfg_rev, S_temp, S_hum, S_soil, S_lux, S_lvl_cm,
        S_ax, S_ay, S_az, S_motion ? \"true\":\"false\",S_tamper?\"true\":\"false\"");
    S_motion = false; S_tamper = false;
    char path[64]; snprintf(path, sizeof(path), "%s?dev=%s", TELEMETRY_EP, device_mac);
    int s = http_post_auth(path, json);
    if (s < 200 || s >= 300) dbg("TEL HTTP %d\n", s);
}

/* ==================== INIT / MAIN ================================== */
static void init_all(void) {
    pc_comm_init(115200, NULL); cfg_load();
    buttons_init(); leds_init(); display_init(); buzzer_beep();
    dht11_init(); soil_init(); light_init();
    hc_sr04_init(); adxl345_init(); pir_init(pir_cb);
    pump_init(); servo(0); lightbulb_init(); tone_init();
    clock_init(&clk, 2025, 6, 18, 12, 0, 0);

    wifi_init(); wifi_command_disable_echo();
    wifi_command_set_mode_to_1(); wifi_command_set_to_single_Connection();
    wifi_command_join_AP(WIFI_SSID, WIFI_PASS); _delay_ms(500);

    if (wifi_command_get_MAC(device_mac) == WIFI_OK) dbg("MAC %s\n", device_mac);
    else { strcpy(device_mac, "UNKNOWN"); dbg("MAC ERR\n"); }

    authenticate_device();
    tone_play_starwars();
    fetch_settings();
}
static void start_tasks(void) {
    periodic_task_init_a(task_tick_1s, 1000);
    periodic_task_init_b(task_sample_5s, 5000);
    periodic_task_init_c(task_logic_5s, 5000);
    periodic_task_init_d(task_cloud_60s, 60000);
    periodic_task_init_d(task_predict_10m, 600000);
    periodic_task_init_d(fetch_settings, 3600000);
}

int main(void) {
    init_all(); start_tasks(); sei();
    for (;;) {
        poll_buttons();
        if (alarm_active && buttons_3_pressed()) {
            alarm_active = false; buzzer_beep(); leds_turnOff(3);
        }
        if (buttons_2_pressed()) {
            CFG.lighting_manual = !CFG.lighting_manual;
            if (CFG.lighting_manual) {
                if (A_light) { lightbulb_off(); A_light = false; leds_turnOff(2); }
                else { lightbulb_on(); A_light = true; leds_turnOn(2); }
            }
        }
        if (buttons_1_pressed()) {
            if (!A_pump && S_lvl_cm > 5) {
                A_pump = true; pump_on(); leds_turnOn(1); pump_runtime_s = 0;
            }
        }
        if (buttons_4_pressed()) {
            servo(90); _delay_ms(600); servo(0);
            A_fert_done = true; hours_since_fert = 0; buzzer_beep();
        }
        if (A_pump && S_lvl_cm <= 5) {
            pump_off(); A_pump = false; buzzer_beep(); leds_turnOff(1);
        }
    }
}
