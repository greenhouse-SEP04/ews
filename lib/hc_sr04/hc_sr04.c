/*  hc_sr04.c – HC-SR04 ultrasonic sensor driver                              */
#include "includes.h"
#include <inttypes.h>

/* -------------------------------------------------------------------------- */
/*  Desktop-test support: when we are NOT compiling for AVR, we need to       */
/*  *define* the AVR-style registers that the driver touches.  The mock       */
/*  header only declares them as extern.                                      */
#if !defined(__AVR__)                      /* i.e. Windows/native unit tests */
#include "mock_avr_io.h"

/* ---- GPIO & timer registers used in this driver ------------------------- */
uint8_t DDRC, DDRL, PORTL, PINL, TCCR1B;
uint16_t TCNT1;                           /* Timer/Counter1 (16-bit)         */

/*  The test files already supply stubs for cli(), sei() and _delay_us(),    */
/*  so we don’t define them here.                                            */
#endif
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                          Pin / register aliases                            */
/*  (Vcc and GND pins are hard-wired; driver only touches Trigger/Echo)       */

#define DDR_Trig  DDRL   /* Trigger – L7 on Mega2560 header                  */
#define P_Trig    PL7
#define PORT_trig PORTL

#define PIN_Echo  PINL   /* Echo – L6 on Mega2560 header                     */
#define P_Echo    PL6

/* -------------------------------------------------------------------------- */
/*                               Driver API                                   */
void hc_sr04_init(void)
{
    /* Configure Trigger as output (Echo stays input)                         */
    DDR_Trig |= (1 << P_Trig);
}

uint16_t hc_sr04_takeMeasurement(void)
{
    uint16_t cnt = 0;

    _delay_us(10);
    PORT_trig |=  (1 << P_Trig);      /* 10 µs HIGH pulse on Trigger          */
    _delay_us(10);
    PORT_trig &= ~(1 << P_Trig);

    /* ---------- Borrow Timer1 -------------------------------------------- */
    uint8_t TCCR1B_state = TCCR1B;     /* Save current prescaler               */
    TCCR1B  = (1 << CS12);             /* Prescaler = 256                      */

    TCNT1 = 0;
    /* Wait for Echo to go HIGH, timeout ≈100 ms                              */
    while (!(PIN_Echo & (1 << P_Echo)))
    {
        if (TCNT1 >= (uint16_t)((F_CPU / 256) * 0.10))
            return 0;                  /* No echo → error                      */
    }

    TCNT1 = 0;                         /* Measure pulse width                  */
    while (PIN_Echo & (1 << P_Echo))
    {
        if (TCNT1 >= (uint16_t)((F_CPU / 256) * 0.024))
            break;                     /* >24 ms → out-of-range                */
    }
    cnt = TCNT1;                       /* Elapsed timer counts                 */

    TCCR1B = TCCR1B_state;             /* Restore Timer1 settings              */

    /* Integer math: distance [cm] = cnt * 343 / 125                          */
    return (uint16_t)(cnt * 343UL / 125UL);
}
