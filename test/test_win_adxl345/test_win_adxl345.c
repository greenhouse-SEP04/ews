#include "unity.h"
#include "adxl345.h"
#include "mock_avr_io.h"
#include <string.h>

/* ---------- SPI spy (linked via --wrap=spi_transfer) -------------------- */
static uint8_t spi_buffer[16];
static uint8_t spi_tx_cnt;
static uint8_t spi_rx_cnt;
static uint8_t spi_script[16];

/*  IMPORTANT: name must be __wrap_spi_transfer so the linker substitutes it
 *  for the real spi_transfer() when tests are built.                        */
uint8_t __wrap_spi_transfer(uint8_t byte)
{
    spi_buffer[spi_tx_cnt++] = byte;
    return spi_script[spi_rx_cnt++];
}

/* ---------- Helpers ----------------------------------------------------- */
static void reset_spy(void)
{
    spi_tx_cnt = spi_rx_cnt = 0;
    memset(spi_buffer,  0, sizeof spi_buffer);
    memset(spi_script,  0, sizeof spi_script);
}

/* ---------- Unity fixtures --------------------------------------------- */
void setUp(void)   { reset_spy(); }
void tearDown(void){}

/* ---------- Tests ------------------------------------------------------- */
void test_init_writes_correct_registers(void)
{
    adxl345_init();

    TEST_ASSERT_EQUAL_UINT8(4,    spi_tx_cnt);
    TEST_ASSERT_EQUAL_UINT8(0x2D, spi_buffer[0]);   /* POWER_CTL */
    TEST_ASSERT_EQUAL_UINT8(0x08, spi_buffer[1]);   /* MEASURE   */
    TEST_ASSERT_EQUAL_UINT8(0x31, spi_buffer[2]);   /* DATA_FMT  */
    TEST_ASSERT_EQUAL_UINT8(0x05, spi_buffer[3]);   /* ±16 g - 13 bit */
}

void test_xyz_read_assembles_int16_correctly(void)
{
    /* script the six data bytes returned by the device                       */
    spi_script[0] = 0xAA;           /* dummy for command phase               */
    spi_script[1] = 0x34; spi_script[2] = 0x12;   /* X = 0x1234 */
    spi_script[3] = 0x78; spi_script[4] = 0x56;   /* Y = 0x5678 */
    spi_script[5] = 0xBC; spi_script[6] = 0x9A;   /* Z = 0x9ABC */

    int16_t x, y, z;
    adxl345_read_xyz(&x, &y, &z);

    TEST_ASSERT_EQUAL_INT16(0x1234, x);
    TEST_ASSERT_EQUAL_INT16(0x5678, y);
    TEST_ASSERT_EQUAL_INT16(0x9ABC, z);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_writes_correct_registers);
    RUN_TEST(test_xyz_read_assembles_int16_correctly);
    return UNITY_END();
}
