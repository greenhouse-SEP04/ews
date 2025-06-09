/*  test_win_hcsr04.c – desktop unit-tests for lib/hc_sr04                   */
#include "unity.h"
#include "../fff.h"

#include "hc_sr04.h"          /* unit under test                             */
#include "mock_avr_io.h"      /* AVR-register names / externs                */

#include <pthread.h>
#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                         FFF fake-function definitions                      */
DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(sei);
FAKE_VOID_FUNC(cli);
FAKE_VOID_FUNC(_delay_us, int);

/* -------------------------------------------------------------------------- */
/*        Materialise the AVR registers this driver touches for the host      */
uint8_t DDRC, DDRL, PORTL, PINL, TCCR1B;
uint16_t TCNT1;

/* -------------------------------------------------------------------------- */
/*              Custom _delay_us fake – drives the simulated echo             */
static int delay_call_cnt;

/*  After the 2nd call (right at the end of the 10 µs trigger pulse) we       *
 *  raise Echo HIGH and spin a background thread that – a few CPU cycles      *
 *  later – drops Echo LOW again and presets TCNT1 to a deterministic value.  */
static void *echo_low_thread(void *arg)
{
    (void)arg;

    /* brief busy-spin so the SUT enters its “while(Echo HIGH)” loop          */
    volatile int junk = 0;
    for (int i = 0; i < 2000; i++) junk++;

    TCNT1 = 400;                        /* measured timer counts              */
    PINL  &= ~(1 << PL6);               /* Echo goes LOW – pulse ends         */
    return NULL;
}

static void _delay_us_stub(int us)
{
    (void)us;
    delay_call_cnt++;

    if (delay_call_cnt == 2)            /* trigger pulse just finished        */
    {
        PINL |= (1 << PL6);             /* Echo rises so first wait ends      */

        pthread_t tid;
        pthread_create(&tid, NULL, echo_low_thread, NULL);
        pthread_detach(tid);            /* no join required                   */
    }
}

/* -------------------------------------------------------------------------- */
/*                               Unity hooks                                  */
void setUp(void)
{
    /* Clear fake registers and counters                                      */
    memset(&DDRC, 0, sizeof DDRC);      /* DDRC .. is contiguous globals      */
    DDRL = PORTL = PINL = TCCR1B = 0;
    TCNT1 = 0;
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
    TEST_ASSERT_BITS_HIGH((1 << PL7), DDRL);   /* Trigger pin now output      */
}

void test_takeMeasurement_returns_expected_distance(void)
{
    PORTL &= ~(1 << PL7);                       /* make sure Trigger LOW      */

    uint16_t dist = hc_sr04_takeMeasurement();

    /* 1) Trigger pin back LOW and both 10 µs delays executed                 */
    TEST_ASSERT_BITS_LOW((1 << PL7), PORTL);
    TEST_ASSERT_EQUAL(2, _delay_us_fake.call_count);

    /* 2) We forced TCNT1 = 400 → distance = 400 × 343 / 125 = 1097 cm        */
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
