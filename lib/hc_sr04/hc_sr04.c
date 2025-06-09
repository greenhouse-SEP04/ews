/*  HC-SR04 “bit-banged” driver
 *  Works on the ATmega2560 **and** in the Windows-hosted unit-tests.
 */
#include <inttypes.h>

#ifdef __AVR__                          /* --------- AVR build -------- */
#   include <avr/io.h>                  /* real HW registers            */
#   include <util/delay.h>
#else                                   /* --------- Windows tests ---- */
#   include "includes.h"                /* brings F_CPU                 */
#   include "mock_avr_io.h"             /* mocked I/O regs / stubs      */

/* Provide storage for the mocked registers that this driver touches   */
uint8_t DDRL, PORTL, PINL;
uint8_t TCCR1B;                         /* 8-bit control register       */
uint8_t TCNT1;                          /* 8-bit “software counter”     */

/* very small, no-op delay stubs                                        */
void _delay_us(int us) { (void)us; }
void _delay_ms(int ms) { (void)ms; }
#endif /* __AVR__ */

/* -------- Pin aliases (Mega 2560 – pins 42/43) ---------------------- */
#define DDR_TRIG  DDRL
#define PORT_TRIG PORTL
#define BIT_TRIG  PL7            /* trigger → PL7 */

#define PIN_ECHO  PINL
#define BIT_ECHO  PL6            /* echo    → PL6 */

/* -------------------------------------------------------------------- */
void hc_sr04_init(void)
{
    DDR_TRIG |= (1 << BIT_TRIG);         /* trigger pin is an output    */
}

/* -------------------------------------------------------------------- */
uint16_t hc_sr04_takeMeasurement(void)
{
    /* ----- generate the 10 µs trigger pulse --------------------------- */
    _delay_us(10);
    PORT_TRIG |=  (1 << BIT_TRIG);
    _delay_us(10);
    PORT_TRIG &= ~(1 << BIT_TRIG);

    /* ----- borrow Timer-1 (prescaler = 256) -------------------------- */
    uint8_t saved_TCCR1B = TCCR1B;
    TCCR1B = (1 << CS12);                /* 16 MHz / 256 = 62.5 kHz     */

    /* ----- 1) wait for echo to go HIGH (abort ≈100 ms) -------------- */
#ifdef __AVR__
    const uint16_t TIMEOUT_TICKS = (F_CPU / 256) * 0.10;   /* ≈6 250     */
#else
    const uint8_t  TIMEOUT_TICKS = 100;                    /* fit in 8 b */
#endif
    while (!(PIN_ECHO & (1 << BIT_ECHO))) {
        if (TCNT1 >= TIMEOUT_TICKS) {      /* sensor absent?            */
            TCCR1B = saved_TCCR1B;
            return 0;
        }
    }

    /* ----- 2) time the HIGH pulse (clamp ≈24 ms ⇒ ~4 m) ------------- */
    TCNT1 = 0;
#ifdef __AVR__
    const uint16_t ECHO_MAX_TICKS = (F_CPU / 256) * 0.024; /* ≈1 500      */
#else
    const uint8_t  ECHO_MAX_TICKS = 200;                   /* arbitrary  */
#endif
    while (PIN_ECHO & (1 << BIT_ECHO)) {
        if (TCNT1 >= ECHO_MAX_TICKS)
            break;
    }

    /* copy the tick count *before* we restore the timer                */
    uint16_t ticks = TCNT1;
    TCCR1B = saved_TCCR1B;

    /* ----- 3) ticks  →  distance (cm) ------------------------------- *
     * ticks · 256 / 16 MHz  gives seconds
     * distance = time · 34 300 cm/s ÷ 2  ⇒  ticks·343 / 125            */
    return (uint16_t)(ticks * 343UL / 125UL);
}
