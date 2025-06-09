#include "buttons.h"
#include "includes.h"

/* -------------------------------------------------------------------------- */
/*  Windows/unit-test build: provide dummy I/O-register storage               */
#ifndef __AVR__
#include "mock_avr_io.h"
uint8_t DDRF, PORTF, PINF;
#endif
/* -------------------------------------------------------------------------- */

#define B_1   PF1
#define B_2   PF2
#define B_3   PF3

#define B_DDR  DDRF
#define B_PORT PORTF
#define B_PIN  PINF

void buttons_init(void)
{
    /* inputs with pull-ups */
    B_DDR  &= ~((1 << B_1) | (1 << B_2) | (1 << B_3));
    B_PORT |=  ((1 << B_1) | (1 << B_2) | (1 << B_3));
}

uint8_t buttons_1_pressed(void) { return !(B_PIN & (1 << B_1)); }
uint8_t buttons_2_pressed(void) { return !(B_PIN & (1 << B_2)); }
uint8_t buttons_3_pressed(void) { return !(B_PIN & (1 << B_3)); }
