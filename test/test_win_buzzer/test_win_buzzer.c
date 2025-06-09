#include "unity.h"
#include "buzzer.h"
#include "mock_avr_io.h"
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Provide minimal stubs / definitions that the production driver needs when
 * we build for the desktop (WIN) test environment.
 * -------------------------------------------------------------------------- */

/* The mock header only DECLARES these registers; we must DEFINE them here.  */
uint8_t DDRE;
uint8_t PORTE;

/* Replace the AVR delay with a no-op stub that matches the prototype in
 * mock_avr_io.h  (int, not unsigned long).                                  */
void _delay_ms(int ms) { (void)ms; }

/* -------------------------------------------------------------------------- */
static uint8_t old_ddr, old_port;

void setUp(void)
{
    /* Initialise registers to known values before each test.                */
    old_ddr  = DDRE  = 0x00;
    old_port = PORTE = 0x55;    /* arbitrary non-zero pattern               */
}

void tearDown(void) {}

/* -------------------------------------------------------------------------- */
void test_beep_pulses_low_high_and_restores_state(void)
{
    buzzer_beep();

    /* On exit the driver must restore the original DDR and PORT settings.   */
    TEST_ASSERT_EQUAL_UINT8(old_ddr,  DDRE);
    TEST_ASSERT_EQUAL_UINT8(old_port, PORTE);
}

/* -------------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_beep_pulses_low_high_and_restores_state);
    return UNITY_END();
}
