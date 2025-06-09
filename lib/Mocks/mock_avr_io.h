// mock_avr_io.h — merged + deduplicated
// ------------------------------------------------------------
// Minimal AVR‑register stub for native/unit‑test builds.
// Add new registers *once* here; keep header self‑contained and
// DO NOT redeclare the same extern symbol twice.
// ------------------------------------------------------------

#pragma once
#include <stdint.h>

void cli(void);
void sei(void);
void _delay_ms(int ms);
void _delay_us(int us);

// ------------------------------------------------------------
// CPU / core constants
// ------------------------------------------------------------
#define F_CPU      16000000L

// ------------------------------------------------------------
// Port‑bit constants (combine once)
// ------------------------------------------------------------
#define PB0 0
#define PB1 1
#define PB2 2
#define PB3 3
#define PB4 4
#define PB5 5
#define PB6 6
#define PB7 7

#define PC0 0
#define PC2 2
#define PC4 4
#define PC6 6
#define PC7 7

#define PD0 0
#define PD1 1
#define PD2 2
#define PD7 7

#define PE3 3
#define PE5 5

#define PF1 1
#define PF2 2
#define PF3 3

#define PG1 1
#define PG5 5

#define PH4 4
#define PH5 5

#define PK0 0
#define PK1 1
#define PK2 2
#define PK3 3
#define PK4 4
#define PK5 5
#define PK6 6
#define PK7 7

#define PL1 1
#define PL6 6
#define PL7 7

// ------------------------------------------------------------
// Interrupt / control bits
// ------------------------------------------------------------
#define TXEN0  3
#define RXEN0  4
#define RXCIE0 7
#define TXEN1  3
#define RXEN1  4
#define RXCIE1 7
#define TXEN2  3
#define RXEN2  4
#define RXCIE2 7
#define TXEN3  3
#define RXEN3  4
#define RXCIE3 7
#define UDRIE0 5
#define UDRIE1 5
#define UDRIE2 5
#define UDRIE3 5
#define UDRE0  5
#define UDRE1  5
#define UDRE2  5
#define UDRE3  5
#define UCSZ00 1
#define UCSZ01 2
#define UCSZ10 1
#define UCSZ11 2
#define UCSZ20 1
#define UCSZ21 2
#define UCSZ30 1
#define UCSZ31 2
#define ISC20  4
#define ISC21  5
#define INT2   2
#define PCIE2  2
#define PCINT20 4
#define CS11   1
#define CS12   2
#define CS20   0
#define CS21   1
#define CS22   2
#define CS30   0
#define CS32   2
#define WGM12  3
#define OCIE1A 1
#define OCIE3A 1
#define OCIE3B 2
#define OCIE3C 3
#define OCIE4B 2
#define OCIE5A 1
#define OCF4B  2

// ------------------------------------------------------------
// Core AVR I/O regs (extern so one .c file defines them)
// ------------------------------------------------------------
extern uint8_t DDRB, PORTB, PINB;
extern uint8_t DDRC, PORTC, PINC;
extern uint8_t DDRD, PORTD, PIND;
extern uint8_t DDRE, PORTE;
extern uint8_t DDRF, PORTF, PINF;
extern uint8_t DDRG, PORTG;
extern uint8_t DDRA, PORTA;
extern uint8_t DDRH, PORTH;
extern uint8_t DDRK, PORTK, PINK;
extern uint8_t DDRL, PORTL, PINL;

// Timer / counter registers
extern uint8_t TCCR1B, TIMSK1;   extern uint16_t OCR1A, TCNT1;
extern uint8_t TCCR2A, TCCR2B;   extern uint8_t TCNT2;
extern uint8_t TCCR3A, TCCR3B, TIMSK3; extern uint16_t OCR3A, OCR3B, OCR3C;
extern uint8_t TCCR4A, TCCR4B, TIMSK4, TIFR4; extern uint16_t TCNT4, OCR4B;
extern uint8_t TCCR5A, TCCR5B, TIMSK5; extern uint16_t OCR5A;

// ADC registers
extern uint8_t ADMUX, ADCSRA, ADCSRB, ADCL, ADCH, DIDR2;

// USART registers
extern uint8_t UCSR0A, UCSR0B, UCSR0C, UDR0, UBRR0H, UBRR0L;
extern uint8_t UCSR1A, UCSR1B, UCSR1C, UDR1, UBRR1H, UBRR1L;
extern uint8_t UCSR2A, UCSR2B, UCSR2C, UDR2, UBRR2H, UBRR2L;
extern uint8_t UCSR3A, UCSR3B, UCSR3C, UDR3, UBRR3H, UBRR3L;

// External‑interrupt control
extern uint8_t EICRA, EIMSK, PCICR, PCMSK2;

// Misc
extern uint8_t ADCSRA2; /* alias for duplicate declaration guard */
