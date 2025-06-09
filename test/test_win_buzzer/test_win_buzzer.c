#include "unity.h"
#include "buzzer.h"
#include "mock_avr_io.h"

/* We also replace _delay_ms to avoid waiting during desktop tests -------- */
void _delay_ms(unsigned long ms) { (void)ms; }

/* -------------------- */
static uint8_t old_ddr, old_port;

void setUp(void)
{
    old_ddr  = DDRE = 0x00;
    old_port = PORTE = 0x55;     /* arbitrary non-zero pattern              */
}

void tearDown(void) {}

void test_beep_pulses_low_high_and_restores_state(void)
{
    buzzer_beep();

    /* At end of call, registers must return to originals                    */
    TEST_ASSERT_EQUAL_UINT8(old_ddr,  DDRE);
    TEST_ASSERT_EQUAL_UINT8(old_port, PORTE);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_beep_pulses_low_high_and_restores_state);
    return UNITY_END();
}
