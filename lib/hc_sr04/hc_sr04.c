#include "includes.h"
#include <inttypes.h>

/* -------------------------------------------------------------------------- */
/*  When we are *not* compiling for the AVR (i.e. Windows-unit-test build),
 *  we must provide storage for the AVR I/O registers the driver touches and
 *  simple delay stubs so the linker can resolve them.
 */
#ifndef __AVR__
#include "mock_avr_io.h"

/* ----------- dummy registers -------------------------------------------- */
uint8_t DDRL, PORTL, PINL;
uint8_t TCCR1B;           /* 8-bit timer-1 control register               */
uint16_t TCNT1;           /* 16-bit timer-1 counter                       */

/* ----------- delay stubs ------------------------------------------------ */
void _delay_us(int us) { (void)us; }
void _delay_ms(int ms) { (void)ms; }
#endif  /* !__AVR__ */
/* -------------------------------------------------------------------------- */


/* ---------- pin aliases -------------------------------------------------- */
/* Vcc / GND were commented out originally – they stay that way here          */

#define DDR_Trig  DDRL     /* PL7 */
#define P_Trig    PL7
#define PORT_trig PORTL

#define PIN_Echo  PINL     /* PL6 */
#define P_Echo    PL6


/* -------------------------------------------------------------------------- */
void hc_sr04_init(void)
{
    /* Configure trigger pin as output                                        */
    DDR_Trig |= (1 << P_Trig);
}

/* -------------------------------------------------------------------------- */
uint16_t hc_sr04_takeMeasurement(void)
{
    /* ---------- send 10 µs pulse ----------------------------------------- */
    _delay_us(10);
    PORT_trig |=  (1 << P_Trig);
    _delay_us(10);
    PORT_trig &= ~(1 << P_Trig);

    /* ---------- borrow Timer-1 ------------------------------------------- */
    uint8_t TCCR1B_state = TCCR1B;
    TCCR1B = (1 << CS12);           /* prescaler = 256                        */

    TCNT1 = 0;
    while (!(PIN_Echo & (1 << P_Echo))) {
        if (TCNT1 >= (F_CPU / 256) * 0.1)      /* >100 ms timeout             */
            return 0;
    }

    TCNT1 = 0;
    while (PIN_Echo & (1 << P_Echo)) {
        if (TCNT1 >= (F_CPU / 256) * 0.024)    /* cap at ~24 ms               */
            break;
    }

    uint16_t cnt = TCNT1;
    TCCR1B = TCCR1B_state;          /* give Timer-1 back to whoever used it   */

    /* ---------- convert ticks to centimetres ----------------------------- */
    /* cnt × 256 cycles / 16 MHz / 2 × 34300 cm/s  ->  cnt × 343 / 125       */
    return (uint16_t)(cnt * 343UL / 125UL);
}
