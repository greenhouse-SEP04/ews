#include "unity.h"
#include "hc_sr04.h"
#include "mock_avr_io.h"

/*  The delay stubs are now supplied by hc_sr04.c for the Windows build,
 *  so we no longer define _delay_us here.                                   */

/*-----------------------------------------------------------------------*/
void setUp(void)
{
    /* Minimal reset of the mocked registers                                */
    TCCR1B = 0;
    TCNT1  = 0;
    PINL   = 0;
}

void tearDown(void) {}

/*-----------------------------------------------------------------------*/
void test_timeout_returns_zero(void)
{
    /* Echo never goes high; force timer near the overflow threshold        */
    TCNT1 = (F_CPU / 256) * 0.12;   /* >0.1 s                               */
    TEST_ASSERT_EQUAL_UINT16(0, hc_sr04_takeMeasurement());
}

/*-----------------------------------------------------------------------*/
void test_distance_math_10cm(void)
{
    /* For 10 cm round-trip the tick count ≈ 4 (see driver comment)         */
    const uint16_t fake_cnt = 4U;

    TCNT1 = fake_cnt;
    PINL  |=  (1 << PL6);   /* echo high -> measurement loop begins        */
    PINL  &= ~(1 << PL6);   /* immediately low so counting stops           */

    uint16_t cm = hc_sr04_takeMeasurement();
    TEST_ASSERT_INT_WITHIN(1, 10, cm);   /* accept ±1 cm                    */
}

/*-----------------------------------------------------------------------*/
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_timeout_returns_zero);
    RUN_TEST(test_distance_math_10cm);
    return UNITY_END();
}
