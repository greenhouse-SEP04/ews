/*  test_win_hcsr04.c – desktop unit-tests for lib/hc_sr04                   */
#include "unity.h"
#include "../fff.h"          /* <-- only include – do NOT define globals     */

#include "hc_sr04.h"         /* unit under test                             */
#include "mock_avr_io.h"     /* AVR-register names / externs                */

#include <pthread.h>
#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                         FFF fake-function definitions                      */
FAKE_VOID_FUNC(sei);
FAKE_VOID_FUNC(cli);
FAKE_VOID_FUNC(_delay_us, int);

/* -------------------------------------------------------------------------- */
/*    Materialise every register the driver or mock refers to on the host     */
uint8_t  DDRC, DDRL, PORTL, PINL, TCCR1B;
uint8_t  TCNT1;                 /* 8-bit symbol declared in mock header      */
uint16_t TCNT1_word;            /* 16-bit counter used in the driver macro   */

/* -------------------------------------------------------------------------- */
/*                    Custom _delay_us fake – drives Echo line                */
static int delay_call_cnt;

static void *echo_low_thread(void *arg)
{
    (void)arg;

    for (volatile int i = 0; i < 2000; ++i) ;   /* tiny busy-spin            */

    TCNT1_word = 400;                           /* deterministic distance    */
    PINL      &= ~(1 << PL6);                   /* Echo LOW – pulse ends     */
    return NULL;
}

static void _delay_us_stub(int us)
{
    (void)us;
    delay_call_cnt++;

    if (delay_call_cnt == 2) {                  /* after trigger pulse       */
        PINL |= (1 << PL6);                     /* Echo rises                */

        pthread_t tid;
        pthread_create(&tid, NULL, echo_low_thread, NULL);
        pthread_detach(tid);
    }
}

/* -------------------------------------------------------------------------- */
/*                               Unity hooks                                  */
void setUp(void)
{
    memset(&DDRC, 0, sizeof DDRC);              /* clear all fake registers  */
    DDRL = PORTL = PINL = TCCR1B = TCNT1 = 0;
    TCNT1_word   = 0;
    delay_call_cnt = 0;

    _delay_us_fake.custom_fake = _delay_us_stub;
}

void tearDown(void) {}

/* -------------------------------------------------------------------------- */
/*                               Test cases                                   */

void test_init_sets_trigger_pin_output(void)
{
    DDRL = 0x00;
    hc_sr04_init();
    TEST_ASSERT_BITS_HIGH((1 << PL7), DDRL);    /* Trigger pin is output     */
}

void test_takeMeasurement_returns_expected_distance(void)
{
    PORTL &= ~(1 << PL7);                       /* ensure Trigger LOW        */

    uint16_t dist = hc_sr04_takeMeasurement();

    /* Trigger back LOW and both delays executed */
    TEST_ASSERT_BITS_LOW((1 << PL7), PORTL);
    TEST_ASSERT_EQUAL(2, _delay_us_fake.call_count);

    /* With TCNT1_word = 400 → distance = 400 × 343 / 125 = 1097 cm          */
    TEST_ASSERT_EQUAL_UINT16(1097, dist);
}

/* -------------------------------------------------------------------------- */
/*                                   Runner                                   */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_sets_trigger_pin_output);
    RUN_TEST(test_takeMeasurement_returns_expected_distance);

    return UNITY_END();
}
