#include "pir.h"
#include "includes.h"

/* -------------------------------------------------------------------------- */
/*  Desktop-test support: when we are NOT compiling for AVR, we need to       */
/*  *define* the AVR-style registers that the driver touches.  The mock       */
/*  header only declares them as extern.                                      */
#if !defined(__AVR__)      /* i.e. Windows/native unit-test build */
#include "mock_avr_io.h"

/* ---- GPIO & interrupt registers ---------------------------------------- */
uint8_t DDRD, PORTD, PIND;
uint8_t EICRA, EIMSK;

/*  The test file already supplies a stub for sei(), so we don’t define it   */
#endif
/* -------------------------------------------------------------------------- */


/* ---------------- Pin / register aliases --------------------------------- */
#define DDR_sig   DDRD
#define P_sig     PD2
#define PORT_sig  PORTD
#define PIN_sig   PIND

static pir_callback_t pir_callback = NULL;


/* ---------------- ISR definitions ---------------------------------------- */
/*  In the production build we use the real AVR ISR syntax.  In the Windows  */
/*  build we compile a normal function so the unit test can call it.         */
#if defined(__AVR__)
ISR(INT2_vect)
{
    if (pir_callback) pir_callback();
}
#else
void INT2_vect(void)
{
    if (pir_callback) pir_callback();
}
#endif


/* ---------------- Driver init ------------------------------------------- */
void pir_init(pir_callback_t callback)
{
    /* Configure PD2 as input with pull-up                                   */
    DDR_sig  &= ~(1 << P_sig);
    PORT_sig |=  (1 << P_sig);

    /* Trigger INT2 on both edges (ISC21:20 = 11)                            */
    EICRA |= (1 << ISC21) | (1 << ISC20);
    EIMSK |= (1 << INT2);                 /* Enable external interrupt      */

    pir_callback = callback;

    sei();                                /* Enable global interrupts       */
}
