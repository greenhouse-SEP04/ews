#pragma once
#include <stdbool.h>
#include "sensor_state.h"
#include "greenhouse_settings.h"

bool api_authenticate(void);                       
bool api_send_reading(const SensorState *s);       
bool api_get_settings(GreenhouseSettings *dst);    

/* NEW: return true if successful and `*should_irrigate` is set; else false */
bool api_predict(const SensorState *s, bool *should_irrigate);