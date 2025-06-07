
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* raw values straight from drivers */
    int16_t  acc_x, acc_y, acc_z;
    uint8_t  hum_i, hum_d, tmp_i, tmp_d;
    uint16_t soil, light, dist;
    bool     motion;
} SensorState;
