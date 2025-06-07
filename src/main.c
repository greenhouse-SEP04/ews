#include "adxl345.h"
#include "buttons.h"
#include "buzzer.h"
#include "dht11.h"
#include "display.h"
#include "hc_sr04.h"
#include "leds.h"
#include "light.h"
#include "pc_comm.h"
#include "periodic_task.h"
#include "pir.h"
#include "servo.h"
#include "soil.h"
#include "tone.h"
#include "uart.h"
#include "wifi.h"
#include "pump.h"         // <-- new driver for the pump
#include "api_client.h"   // <-- for api_get_settings, api_send_reading, api_predict

#include "sensor_state.h"
#include "thresholds.h"

#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Wi-Fi credentials & API host are in api_client.c                      */
#define WIFI_SSID "H158-381_5B79"
#define WIFI_PASS "MGNn93gDda5"

/* Pump?on duration in milliseconds (60 seconds) */
#define PUMP_DURATION_MS 60000UL

/* ------------------------------------------------------------------ */
/* Buffers & globals                                                  */
/* ------------------------------------------------------------------ */
static char debug_buf[128];
static volatile SensorState g_state = {0};
static volatile bool send_flag = false;

/* Pump control state */
static bool     pump_running   = false;
static uint32_t pump_start_ms  = 0;

/* ------------------------------------------------------------------ */
/* Sensor read callbacks                                              */
/* ------------------------------------------------------------------ */
static void read_sensors_A(void)     /* every 2 s */
{
    if (dht11_get(&g_state.hum_i, &g_state.hum_d,
                  &g_state.tmp_i, &g_state.tmp_d) != DHT11_OK) {
        g_state.hum_i = g_state.tmp_i = 255;
    }

    g_state.soil  = soil_read();
    g_state.light = light_read();
    g_state.dist  = hc_sr04_takeMeasurement();
}

static void read_sensors_B(void)     /* every 5 s */
{
    adxl345_read_xyz(&g_state.acc_x, &g_state.acc_y, &g_state.acc_z);
}

static void buttons_demo(void)       /* every 300 ms */
{
    /* Button 1 & 2: (unchanged) move servo to angles 0 / 90 / 180 for testing */
    if (buttons_1_pressed()) servo(0);
    if (buttons_2_pressed()) servo(90);
    if (buttons_3_pressed()) {
        /* Button 3 now = “fertilize” gesture: 
           rotate to 0°, hold 500 ms, return to 90° */
        servo(0);
        _delay_ms(500);
        servo(90);
    }
}

static void mark_send(void)          /* every 10 s */
{
    send_flag = true;
}

static void pir_cb(void)
{
    g_state.motion = true;
}

/* ------------------------------------------------------------------ */
/* Timing helper: get milliseconds since boot (CAN USE your existing util) */
/* ------------------------------------------------------------------ */
extern uint32_t millis(void);

/* ------------------------------------------------------------------ */
/* “Pump?timeout” checker: if pump has been running > PUMP_DURATION_MS, stop it */
/* ------------------------------------------------------------------ */
static void check_pump_timeout(void)
{
    if (pump_running) {
        uint32_t now = millis();
        if ((now - pump_start_ms) >= PUMP_DURATION_MS) {
            pump_off();
            pump_running = false;
        }
    }
}

/* ------------------------------------------------------------------ */
/* wifi_ok helper (unchanged)                                         */
/* ------------------------------------------------------------------ */
static bool wifi_ok(WIFI_ERROR_MESSAGE_t e, const char *ctx)
{
    if (e != WIFI_OK) {
        snprintf(debug_buf, sizeof(debug_buf),
                 "[WIFI ERR] %s:%d\r\n", ctx, e);
        uart_send_string_blocking(USART_0, debug_buf);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Main loop                                                         */
/* ------------------------------------------------------------------ */
int main(void)
{
    /* UARTs */
    uart_init(USART_0, 115200, NULL);          /* PC console */
    pc_comm_init(115200, NULL);

    uart_send_string_blocking(USART_0, "\r\nBooting greenhouse...\r\n");

    /* Drivers */
    dht11_init();
    soil_init();
    light_init();
    hc_sr04_init();
    adxl345_init();
    buttons_init();
    display_init();
    leds_init();
    tone_init();
    pir_init(pir_cb);
    servo(90);       /* default “servo safe” position */
    display_int(0);

    pump_init();     /* initialize pump driver */

    /* WiFi */
    wifi_init();
    _delay_ms(2000);
    wifi_ok(wifi_command_AT(), "AT");
    wifi_ok(wifi_command_disable_echo(), "ECHO");
    wifi_ok(wifi_command_set_mode_to_1(), "MODE1");
    wifi_ok(wifi_command_join_AP((char*)WIFI_SSID, (char*)WIFI_PASS), "JOIN");
    uart_send_string_blocking(USART_0, "WiFi ready\r\n");

    /* Timers */
    sei();
    periodic_task_init_a(read_sensors_A, 2000);
    periodic_task_init_b(read_sensors_B, 5000);
    periodic_task_init_c(buttons_demo, 300);
    periodic_task_init_b(mark_send, 10000);

    /* Main loop */
    while (1) {
        /* 1) Check pump timeout each iteration */
        check_pump_timeout();

        /* 2) When it’s time to send: */
        if (send_flag) {
            send_flag = false;

            /* A) Push sensor reading to backend */
            api_send_reading((SensorState*)&g_state);

            /* B) Pull settings from backend */
            GreenhouseSettings gh;
            bool got_settings = api_get_settings(&gh);

            /* C) Apply non?soil thresholds (temp, hum, light, motion) */
            /*    NOTE: thresholds_apply no longer handles soil/valve.       */
            thresholds_apply((SensorState*)&g_state, &gh);

            /* D) IRRIGATION DECISION: first try ML prediction API */
            bool should_irrigate = false, predict_ok = false;
            predict_ok = api_predict((SensorState*)&g_state, &should_irrigate);

            if (predict_ok) {
                if (should_irrigate && !pump_running) {
                    pump_on();
                    pump_running   = true;
                    pump_start_ms  = millis();
                }
            } else {
                /* FALLBACK: ML call failed */
                if (got_settings && gh.soil_min > 0) {
                    if (g_state.soil < gh.soil_min && !pump_running) {
                        pump_on();
                        pump_running   = true;
                        pump_start_ms  = millis();
                    }
                } else {
                    /* No soil_min to fall back on ? alert user */
                    leds_toggle(3);
                    buzzer_beep();
                }
            }

            /* E) Clear motion flag */
            g_state.motion = false;
        }
    }
}