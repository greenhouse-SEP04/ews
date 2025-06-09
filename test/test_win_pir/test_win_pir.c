#include "unity.h"
#include "pir.h"
#include "mock_avr_io.h"

static volatile uint8_t callback_hits;

static void fake_cb(void) { callback_hits++; }

/* Provide stub for sei() so native build links                            */
void sei(void) {}

void setUp(void)  { callback_hits = 0; }
void tearDown(void){}

void test_isr_invokes_callback_once_per_edge(void)
{
    pir_init(fake_cb);

    /* Manually call ISR as desktop runner won’t fire real interrupts       */
    extern void INT2_vect(void);      /* from pir.c                         */
    INT2_vect();
    INT2_vect();

    TEST_ASSERT_EQUAL_UINT8(2, callback_hits);
}

void test_null_callback_safe(void)
{
    pir_init(NULL);
    extern void INT2_vect(void);
    /* Should not crash                                                     */
    INT2_vect();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_isr_invokes_callback_once_per_edge);
    RUN_TEST(test_null_callback_safe);
    return UNITY_END();
}
