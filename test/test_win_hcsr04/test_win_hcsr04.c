/*  test_win_hcsr04.c – desktop unit-tests for lib/hc_sr04                  */
#include "unity.h"
#include "../fff.h"

/*  Alias the 16-bit counter *before* pulling in the mock header so the
 *  header keeps its 8-bit symbol while our test uses the wide one.         */
#define TCNT1 TCNT1_word
extern uint16_t TCNT1_word;

#include "hc_sr04.h"           /* unit under test                            */
#include "mock_avr_io.h"       /* AVR-register names / externs               */

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
uint8_t  DDRC, DDRL, PORTL, PINL, TCCR1B;
uint16_t TCNT1_word;                     /* real 16-bit counter used by SUT   */

/* -------------------------------------------------------------------------- */
/*              Custom _delay_us fake – drives the simulated echo             */
static int delay_call_cnt;

static void *echo_low_thread(void *arg)
{
    (void)arg;

    /* brief busy-spin so the SUT sits in its “echo HIGH” loop                */
    for (volatile int i = 0; i < 2000; i++) ;

    TCNT1_word = 400;                    /* predetermined timer counts        */
    PINL      &= ~(1 << PL6);            /* Echo goes LOW – pulse ends        */
    return NULL;
}

static void _delay_us_stub(int us)
{
    (void)us;
    delay_call_cnt++;

    if (delay_call_cnt == 2) {           /* trigger pulse just finished       */
        PINL |= (1 << PL6);              /* Echo rises so first wait ends     */

        pthread_t tid;
        pthread_create(&tid, NULL, echo_low_thread, NULL);
        pthread_detach(tid);             /* fire-and-forget                   */
    }
}

/* -------------------------------------------------------------------------- */
/*                               Unity hooks                                  */
void setUp(void)
{
    memset(&DDRC, 0, sizeof DDRC);       /* DDRC .. is contiguous globals     */
    DDRL = PORTL = PINL = TCCR1B = 0;
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
    TEST_ASSERT_BITS_HIGH((1 << PL7), DDRL);   /* Trigger pin now output      */
}

void test_takeMeasurement_returns_expected_distance(void)
{
    PORTL &= ~(1 << PL7);                       /* ensure Trigger LOW          */

    uint16_t dist = hc_sr04_takeMeasurement();

    /* 1) Trigger pin back LOW and both 10 µs delays executed                */
    TEST_ASSERT_BITS_LOW((1 << PL7), PORTL);
    TEST_ASSERT_EQUAL(2, _delay_us_fake.call_count);

    /* 2) We forced TCNT1 = 400 → distance = 400 × 343 / 125 = 1097 cm       */
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
