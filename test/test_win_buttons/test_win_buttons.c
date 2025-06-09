#include "unity.h"
#include "buttons.h"
#include <avr/io.h>

/* Unity fixture ---------------------------------------------------------- */
void setUp(void)
{
    /*  Reset mocked registers to known state before each test              */
    DDRF  = 0xFF;      /* everything output → buttons_init() must clear    */
    PORTF = 0x00;      /* pull-ups off → buttons_init() must set           */
    PINF  = 0xFF;      /* default high (input not pressed)                 */
}

void tearDown(void) {}

/* Tests ------------------------------------------------------------------ */
void test_buttons_init_sets_inputs_and_pullups(void)
{
    buttons_init();

    uint8_t mask = (1 << PF1) | (1 << PF2) | (1 << PF3);

    /* inputs?  -> DDRF bits LOW  */
    TEST_ASSERT_BITS_LOW(mask, DDRF);

    /* pull-ups? -> PORTF bits HIGH */
    TEST_ASSERT_BITS_HIGH(mask, PORTF);
}

void test_button1_pressed_detects_low_level(void)
{
    buttons_init();

    /* simulate press (active-low) */
    PINF &= ~(1 << PF1);
    TEST_ASSERT_TRUE(buttons_1_pressed());

    /* release */
    PINF |= (1 << PF1);
    TEST_ASSERT_FALSE(buttons_1_pressed());
}

void test_button2_pressed_detects_low_level(void)
{
    buttons_init();
    PINF &= ~(1 << PF2);
    TEST_ASSERT_TRUE(buttons_2_pressed());
}

void test_simultaneous_two_button_presses(void)
{
    buttons_init();
    PINF &= ~((1 << PF1) | (1 << PF3));
    TEST_ASSERT_TRUE(buttons_1_pressed());
    TEST_ASSERT_TRUE(buttons_3_pressed());
}

/* ----------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_buttons_init_sets_inputs_and_pullups);
    RUN_TEST(test_button1_pressed_detects_low_level);
    RUN_TEST(test_button2_pressed_detects_low_level);
    RUN_TEST(test_simultaneous_two_button_presses);
    return UNITY_END();
}
