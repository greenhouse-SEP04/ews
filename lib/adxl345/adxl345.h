#ifndef ADXL345_H
#define ADXL345_H

#include <inttypes.h>

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */

void adxl345_init(void);

/*  Write one register on the ADXL345                                         */
void adxl345_write_register(uint8_t reg, uint8_t value);

/**
 * Read acceleration on all three axes.
 *
 * @param x  Signed 16-bit raw count for X-axis
 * @param y  Signed 16-bit raw count for Y-axis
 * @param z  Signed 16-bit raw count for Z-axis
 *
 * Each count spans −32 768 … +32 767, where ±32 767 corresponds to ±4 g
 * with the default ±16 g / 13-bit mode.  To convert to m/s² divide the
 * returned value by 834 ≈ (32767 / (9.82×4)).
 */
void adxl345_read_xyz(int16_t *x, int16_t *y, int16_t *z);

#endif /* ADXL345_H */
