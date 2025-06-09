#include "unity.h"
#include "adxl345.h"
#include "mock_avr_io.h"
#include <string.h>

/* ---------- FFF-style spy ------------------------------------------------ */
static uint8_t spi_buffer[16];
static uint8_t spi_tx_count;
static uint8_t spi_rx_count;
static uint8_t spi_script[16];

uint8_t spi_transfer(uint8_t byte)
{
    spi_buffer[spi_tx_count++] = byte;
    return spi_script[spi_rx_count++];
}

/* ---------- Helpers ----------------------------------------------------- */
static void reset_spy(void)
{
    spi_tx_count = spi_rx_count = 0;
    memset(spi_buffer, 0, sizeof spi_buffer);
    memset(spi_script, 0, sizeof spi_script);
}

/* ---------- Tests ------------------------------------------------------- */
void setUp(void)  { reset_spy(); }
void tearDown(void){}

void test_init_writes_correct_registers(void)
{
    adxl345_init();

    TEST_ASSERT_EQUAL_UINT8(4, spi_tx_count);
    TEST_ASSERT_EQUAL_UINT8(0x2D, spi_buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0x08, spi_buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0x31, spi_buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0x05, spi_buffer[3]);
}

void test_xyz_read_assembles_int16_correctly(void)
{
    spi_script[0] = 0xAA;
    spi_script[1] = 0x34;
    spi_script[2] = 0x12;
    spi_script[3] = 0x78;
    spi_script[4] = 0x56;
    spi_script[5] = 0xBC;
    spi_script[6] = 0x9A;

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
