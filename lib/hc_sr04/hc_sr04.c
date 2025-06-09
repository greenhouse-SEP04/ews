/*  hc_sr04.c – HC-SR04 ultrasonic sensor driver
 *  Builds on AVR targets **and** in the Windows / native unit-test runner.
 */
#include "includes.h"
#include <inttypes.h>

/* --------------------------------------------------------------------------
 *  Desktop-test glue
 *  -----------------
 *  The mock header declares an 8-bit `extern uint8_t TCNT1;`, but the
 *  Windows tests need a full 16-bit counter.  Rather than touching the
 *  header we                   *alias* the wide variable to a new name
 *  (`TCNT1_word`) and map all driver references through a macro.  The mock
 *  symbol is left untouched and will simply be ignored.
 * ------------------------------------------------------------------------ */
#if !defined(__AVR__)                       /* -------- host / unit-tests ---- */

#define TCNT1            TCNT1_word         /* all later code sees “word”     */
extern uint16_t          TCNT1_word;        /* storage comes from test file   */

#include "mock_avr_io.h"                    /* brings in register names       */

#else                                       /* -------- on real AVR ---------- */
#include "mock_avr_io.h"
#endif
/* ------------------------------------------------------------------------- */

/* ---- Pin aliases (Arduino Mega 2560: 42 = PL7 / 43 = PL6) ---------------- */
#define DDR_TRIG   DDRL
#define PORT_TRIG  PORTL
#define BIT_TRIG   PL7

#define PIN_ECHO   PINL
#define BIT_ECHO   PL6

/* ------------------------------------------------------------------------- */
void hc_sr04_init(void)
{
    DDR_TRIG |= (1 << BIT_TRIG);            /* Trigger pin → output          */
}

/* ------------------------------------------------------------------------- */
uint16_t hc_sr04_takeMeasurement(void)
{
    /* 1 · Generate the 10 µs trigger pulse -------------------------------- */
    _delay_us(10);
    PORT_TRIG |=  (1 << BIT_TRIG);
    _delay_us(10);
    PORT_TRIG &= ~(1 << BIT_TRIG);

    /* 2 · Borrow Timer-1 (prescaler 256) ---------------------------------- */
    uint8_t saved_TCCR1B = TCCR1B;
    TCCR1B  = (1 << CS12);                  /* 16 MHz / 256 ≈ 62.5 kHz        */

    /* 3 · Wait for rising edge – abort ≥100 ms ---------------------------- */
    while (!(PIN_ECHO & (1 << BIT_ECHO))) {
        if (TCNT1 >= (uint16_t)((F_CPU / 256) * 0.10)) {
            TCCR1B = saved_TCCR1B;
            return 0;                       /* sensor missing / timeout      */
        }
    }

    /* 4 · Measure HIGH pulse – clamp at 24 ms (~4 m) ---------------------- */
    TCNT1 = 0;
    while (PIN_ECHO & (1 << BIT_ECHO)) {
        if (TCNT1 >= (uint16_t)((F_CPU / 256) * 0.024))
            break;
    }

    uint16_t ticks = TCNT1;
    TCCR1B = saved_TCCR1B;                  /* restore prescaler             */

    /* 5 · ticks·256 / 16 MHz = time → distance [cm] = time·34300 / 2
     *    ⇒ ticks·343 / 125                                              */
    return (uint16_t)(ticks * 343UL / 125UL);
}
