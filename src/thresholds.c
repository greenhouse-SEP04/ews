#include "thresholds.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"
#include "sensor_state.h"
#include "greenhouse_settings.h"

/*
    New priority order:
      1) Temperature (too hot/too cold) – highest urgency
      2) Soil moisture (too low = dry, too high = waterlogged)
      3) Humidity (too low/too high)
      4) Motion (intrusion)
      5) Light (just LED indicator, no buzzer)

    As soon as a higher?priority condition triggers, we send that alert and return.
*/

void thresholds_apply(const SensorState *r,
                      const GreenhouseSettings *s)
{
    /* 1) Temperature check (highest urgency) */
    if (r->tmp_i > (int)s->temp_max) {
        // Too hot: blink LED1 twice and beep twice
        for (int i = 0; i < 2; i++) {
            leds_toggle(1);
            _delay_ms(100);
            leds_toggle(1);
            _delay_ms(100);
        }
        buzzer_beep();
        _delay_ms(100);
        buzzer_beep();
        display_int((int16_t)r->tmp_i);
        return;
    }
    else if (r->tmp_i < (int)s->temp_min) {
        // Too cold: turn LED1 on steady + single beep
        leds_turnOn(1);
        buzzer_beep();
        display_int((int16_t)r->tmp_i);
        return;
    }
    else {
        // Temperature is OK: ensure LED1 is off if it was on
        leds_turnOff(1);
    }

    /* 2) Soil?moisture check */
    // If soil_min is zero, we assume “no threshold set” and skip soil alerts.
    if (s->soil_min > 0) {
        if (r->soil < (int)s->soil_min) {
            // Soil too dry: blink LED3 twice + single beep
            for (int i = 0; i < 2; i++) {
                leds_toggle(3);
                _delay_ms(100);
                leds_toggle(3);
                _delay_ms(100);
            }
            buzzer_beep();
            display_int((int16_t)r->tmp_i);
            return;
        }
        // If soil goes more than 100 units above soil_min, consider it waterlogged
        else if (r->soil > (int)(s->soil_min + 100)) {
            // Soil waterlogged: blink LED3 three times + two beeps
            for (int i = 0; i < 3; i++) {
                leds_toggle(3);
                _delay_ms(100);
                leds_toggle(3);
                _delay_ms(100);
            }
            buzzer_beep();
            _delay_ms(100);
            buzzer_beep();
            display_int((int16_t)r->tmp_i);
            return;
        }
        else {
            // Soil is within the expected range; ensure LED3 is off
            leds_turnOff(3);
        }
    }

    /* 3) Humidity check */
    if (r->hum_i < (int)s->hum_min) {
        // Humidity too low: turn LED1 on + buzz once
        leds_turnOn(1);
        buzzer_beep();
        display_int((int16_t)r->tmp_i);
        return;
    }
    else if (r->hum_i > (int)s->hum_max) {
        // Humidity too high: blink LED1 once (no buzzer)
        leds_toggle(1);
        display_int((int16_t)r->tmp_i);
        return;
    }
    else {
        // Humidity OK: ensure LED1 is off
        leds_turnOff(1);
    }

    /* 4) Motion check */
    if (r->motion) {
        // Motion detected: buzz + toggle LED3 once
        buzzer_beep();
        leds_toggle(3);
        display_int((int16_t)r->tmp_i);
        return;
    }
    else {
        // No motion: make sure LED3 is off
        leds_turnOff(3);
    }

    /* 5) Light check (lowest urgency; no buzzer) */
    if (r->light < s->light_min) {
        // Too dark: LED2 on
        leds_turnOn(2);
    }
    else if (r->light > s->light_max) {
        // Too bright: LED2 off
        leds_turnOff(2);
    }
    else {
        // Light OK: ensure LED2 is off
        leds_turnOff(2);
    }

    /* Always show current temperature integer part */
    display_int((int16_t)r->tmp_i);
}