#include "unity.h"
#include "hc_sr04.h"
#include "mock_avr_io.h"

void _delay_us(unsigned long us) { (void)us; }

/*-----------------------------------------------------------------------*/
void setUp(void)
{
    /* Minimal reset */
    TCCR1B = 0;
    TCNT1  = 0;
    PINL   = 0;
}

void tearDown(void) {}

void test_timeout_returns_zero(void)
{
    /* Echo never goes high; TCNT1 forced to overflow threshold             */
    TCNT1 = (F_CPU/256)*0.12;   /* >0.1 s                                  */
    TEST_ASSERT_EQUAL_UINT16(0, hc_sr04_takeMeasurement());
}

void test_distance_math_10cm(void)
{
    /* Pre-compute ticks for 10 cm round-trip                               */
    /* distance  = cnt * 343/125                                            */
    /* 10 = cnt * 2.744 → cnt ≈ 4                                           */
    const uint16_t fake_cnt = 4U;

    /* simulate quick high pulse: wait loop increments counter by itself    */
    TCNT1 = fake_cnt;
    PINL  |= (1 << PL6);        /* echo high → inner while will start      */
    PINL  &= ~(1 << PL6);       /* immediately low so cnt stops            */

    uint16_t cm = hc_sr04_takeMeasurement();
    TEST_ASSERT_INT_WITHIN(1, 10, cm);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_timeout_returns_zero);
    RUN_TEST(test_distance_math_10cm);
    return UNITY_END();
}
