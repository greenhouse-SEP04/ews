#pragma once
#include <stdint.h>

typedef struct {
    char     owner[16];
    char     name[16];
    float    temp_min, temp_max;
    float    light_min, light_max;
    float    hum_min,  hum_max;
    uint16_t soil_min;
} GreenhouseSettings;