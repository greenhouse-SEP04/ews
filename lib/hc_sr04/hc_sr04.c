/*  HC-SR04 “bit-banged” driver
 *  – works on AVR hardware and in the Windows unit-test build.           */
#include <inttypes.h>
#include "includes.h"

#ifndef __AVR__
/* -------------------------------------------------------------------- */
/*  When compiled for the Windows test runner we must provide dummy AVR
 *  registers *and* delay stubs so the driver links.                     */
#include "mock_avr_io.h"

/* ------- mocked I/O registers --------------------------------------- */
uint8_t  DDRL,  PORTL,  PINL;
uint8_t  TCCR1B;                 /* Timer-1 control register            */
uint8_t TCNT1;                  /* 8-bit software “counter”           */

/* ------- delay stubs ------------------------------------------------ */
void _delay_us(int us) { (void)us; }
void _delay_ms(int ms) { (void)ms; }
#endif  /* !__AVR__ */
/* -------------------------------------------------------------------- */

/*  Pin assignments (ATmega2560 – pins 42 & 43 on the Mega)             */
#define DDR_TRIG  DDRL           /* PL7 */
#define PORT_TRIG PORTL
#define BIT_TRIG  PL7

#define PIN_ECHO  PINL           /* PL6 */
#define BIT_ECHO  PL6

/* -------------------------------------------------------------------- */
void hc_sr04_init(void)
{
    /* Trigger pin as output                                               */
    DDR_TRIG |= (1 << BIT_TRIG);
}

/* -------------------------------------------------------------------- */
uint16_t hc_sr04_takeMeasurement(void)
{
    /* ----- generate 10 µs trigger pulse -------------------------------- */
    _delay_us(10);
    PORT_TRIG |=  (1 << BIT_TRIG);
    _delay_us(10);
    PORT_TRIG &= ~(1 << BIT_TRIG);

    /* ----- borrow Timer-1 (prescaler 256) ----------------------------- */
    uint8_t saved_TCCR1B = TCCR1B;
    TCCR1B = (1 << CS12);            /* prescale 16 MHz → 62.5 kHz         */

    /* ------------------------------------------------------------------ */
    /*   1.  wait for echo to go HIGH –- but abort after 100 ms            */
    /* ------------------------------------------------------------------ */
    /* NOTE: **do not** clear TCNT1 here – unit tests pre-load it so the
     *       software counter “advances” while we spin.                   */

    while (!(PIN_ECHO & (1 << BIT_ECHO))) {
        if (TCNT1 >= (F_CPU / 256) * 0.10) {   /* ≈100 ms timeout          */
            TCCR1B = saved_TCCR1B;
            return 0;                          /* sensor missing?          */
        }
    }

    /* ------------------------------------------------------------------ */
    /*   2.  measure the HIGH pulse width –- cap at 24 ms (~4 m distance)  */
    /* ------------------------------------------------------------------ */
    TCNT1 = 0;                                 /* now start “stop-watch”   */

    while (PIN_ECHO & (1 << BIT_ECHO)) {
        if (TCNT1 >= (F_CPU / 256) * 0.024)     /* ≈24 ms max              */
            break;                              /* clamp to ~4 m           */
    }

    uint16_t ticks = TCNT1;                     /* copy before restoring   */
    TCCR1B = saved_TCCR1B;                      /* give Timer-1 back       */

    /* ------------------------------------------------------------------ */
    /*   3.  ticks → distance (cm)                                         */
    /*       ticks·256 / 16 MHz  = time (s)                                */
    /*       distance(cm) = time·34300 / 2                                 */
    /*       ⇒  ticks·343 / 125                                            */
    /* ------------------------------------------------------------------ */
    return (uint16_t)(ticks * 343UL / 125UL);
}
