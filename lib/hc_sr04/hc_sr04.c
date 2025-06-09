/*  hc_sr04.c – HC-SR04 ultrasonic driver
 *  Builds on AVR **and** in the Windows/native unit-test runner.
 */
#include "includes.h"
#include <inttypes.h>

/* --------------------------------------------------------------------------
 *  –––––  Native / Windows build glue  –––––
 *  We must:
 *    • provide storage for the fake AVR registers,
 *    • keep the original 8-bit declaration in mock_avr_io.h untouched, and
 *    • end up with **one** symbol called TCNT1 that is 16-bit (so the
 *      unit-tests can assign values like 400 without overflow).
 * ------------------------------------------------------------------------ */
#if !defined(__AVR__)                /* -------------- desktop tests -------- */
    /* 1) Temporarily rename TCNT1 so mock_avr_io.h declares
     *    “extern uint8_t TCNT1_8;” instead of TCNT1.                        */
    #define  TCNT1  TCNT1_8
    #include "mock_avr_io.h"
    #undef   TCNT1                   /* back to the public name              */

    /* 2) Materialise **all** registers we touch -------------------------- */
    uint8_t  DDRC, DDRL, PORTL, PINL, TCCR1B;
    uint8_t  TCNT1_8;               /* 8-bit shadow (satisfies the header)  */
    uint16_t TCNT1;                 /* real 16-bit counter used by tests    */

#else                                /* -------------- real AVR build ------ */
    #include "mock_avr_io.h"         /* only brings in the register names   */
#endif
/* ------------------------------------------------------------------------- */

/* ---- Pin aliases (Arduino Mega 2560: 42 = PL7, 43 = PL6) ---------------- */
#define DDR_TRIG  DDRL
#define PORT_TRIG PORTL
#define BIT_TRIG  PL7

#define PIN_ECHO  PINL
#define BIT_ECHO  PL6

/* ------------------------------------------------------------------------- */
void hc_sr04_init(void)
{
    DDR_TRIG |= (1 << BIT_TRIG);          /* Trigger pin → output           */
}

/* ------------------------------------------------------------------------- */
uint16_t hc_sr04_takeMeasurement(void)
{
    /* 1 · 10 µs trigger pulse ------------------------------------------- */
    _delay_us(10);
    PORT_TRIG |=  (1 << BIT_TRIG);
    _delay_us(10);
    PORT_TRIG &= ~(1 << BIT_TRIG);

    /* 2 · Borrow Timer-1 (prescaler 256) -------------------------------- */
    uint8_t saved_TCCR1B = TCCR1B;
    TCCR1B  = (1 << CS12);                /* 16 MHz / 256 ≈ 62.5 kHz        */

    /* 3 · Wait for rising edge (abort ≥100 ms) -------------------------- */
    while (!(PIN_ECHO & (1 << BIT_ECHO))) {
        if (TCNT1 >= (F_CPU / 256) * 0.10) {
            TCCR1B = saved_TCCR1B;
            return 0;                     /* sensor missing / timed out     */
        }
    }

    /* 4 · Measure pulse width – clamp at 24 ms (~4 m) ------------------- */
    TCNT1 = 0;
    while (PIN_ECHO & (1 << BIT_ECHO)) {
        if (TCNT1 >= (F_CPU / 256) * 0.024)
            break;
    }

    uint16_t ticks = TCNT1;
    TCCR1B = saved_TCCR1B;                /* restore Timer-1 settings       */

    /* 5 · ticks·256 / 16 MHz  →  time [s]
     *     distance [cm] = time·34300 / 2  →  ticks·343 / 125              */
    return (uint16_t)(ticks * 343UL / 125UL);
}
