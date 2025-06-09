#include "adxl345.h"
#include "includes.h"

/* -------------------------------------------------------------------------- */
/*  When we are **not** building for the AVR target (i.e. unit tests running
 *  on the host), we must provide storage for the GPIO registers that the
 *  driver touches and simple delay stubs so the linker can resolve them.
 *  We also expose a thin shim so that the production code keeps calling
 *  spi_transfer(), but the unit-test spy (defined in the test file as
 *  __wrap_spi_transfer) actually runs.
 */
#ifndef __AVR__
#include "mock_avr_io.h"

/* ------------- dummy I/O registers --------------------------------------- */
uint8_t DDRB, PORTB, PINB;
uint8_t DDRG, PORTG;
uint8_t DDRD, PORTD;

/* ------------- delay stubs ----------------------------------------------- */
void _delay_ms(int ms) { (void)ms; }
void _delay_us(int us) { (void)us; }

/* ------------- connect to the spy ---------------------------------------- */
extern uint8_t __wrap_spi_transfer(uint8_t data);   /* provided by the test */

uint8_t spi_transfer(uint8_t data)                  /* used by driver      */
{
    return __wrap_spi_transfer(data);
}
#endif  /* !__AVR__ */
/* -------------------------------------------------------------------------- */


/* === pin / register definitions – unchanged ============================== */
/* (for readability the big comment-block with the alternative pin-mapping
 *  has been left out, but you can keep it if you like)                      */
#define GND_BIT PG1
#define GND_DDR DDRG
#define GND_PORT PORTG

#define VCC_BIT PD7
#define VCC_DDR DDRD
#define VCC_PORT PORTD

#define CS_BIT  PB1
#define CS_DDR  DDRB
#define CS_PORT PORTB

#define MISO_BIT PB2
#define MISO_DDR DDRB
#define MISO_PIN PINB

#define MOSI_BIT PB3
#define MOSI_DDR DDRB
#define MOSI_PORT PORTB

#define SCL_BIT  PB0
#define SCL_DDR  DDRB
#define SCL_PORT PORTB

/* ADXL345 register addresses and constants */
#define ADXL345_POWER_CTL     0x2D
#define ADXL345_DATA_FORMAT   0x31
#define ADXL345_DATAX0        0x32
#define ADXL345_MEASURE_MODE  0x08

/* -------------------------------------------------------------------------- */
void adxl345_init(void)
{
    /* configure pins */
    MOSI_DDR |= (1 << MOSI_BIT);
    SCL_DDR  |= (1 << SCL_BIT);
    CS_DDR   |= (1 << CS_BIT);
    MISO_DDR &= ~(1 << MISO_BIT);

    /* deselect chip, set clock high (idle) */
    CS_PORT  |= (1 << CS_BIT);
    SCL_PORT |= (1 << SCL_BIT);

    _delay_ms(20);

    /* put the device in measurement mode and ±16 g range / 13-bit */
    adxl345_write_register(ADXL345_POWER_CTL,    ADXL345_MEASURE_MODE);
    adxl345_write_register(ADXL345_DATA_FORMAT,  0b00000101);
}


/* -------------------------------------------------------------------------- */
#ifdef __AVR__
/*  Real bit-bang SPI implementation – compiled only for the embedded build  */
uint8_t spi_transfer(uint8_t data)
{
    uint8_t received = 0;

    for (uint8_t i = 0; i < 8; i++) {
        if (data & (1 << (7 - i)))
            MOSI_PORT |=  (1 << MOSI_BIT);
        else
            MOSI_PORT &= ~(1 << MOSI_BIT);

        /* clock low → high */
        SCL_PORT &= ~(1 << SCL_BIT);
        _delay_us(1);

        received <<= 1;
        if (MISO_PIN & (1 << MISO_BIT))
            received |= 1;

        SCL_PORT |=  (1 << SCL_BIT);
        _delay_us(1);
    }
    return received;
}
#endif  /* __AVR__ */
/* -------------------------------------------------------------------------- */


void adxl345_write_register(uint8_t reg, uint8_t value)
{
    CS_PORT &= ~(1 << CS_BIT);      /* select */
    _delay_us(1);

    spi_transfer(reg);
    spi_transfer(value);

    CS_PORT |=  (1 << CS_BIT);      /* deselect */
}

uint8_t adxl345_read_register(uint8_t reg)
{
    CS_PORT &= ~(1 << CS_BIT);
    spi_transfer(0x80 | reg);       /* read bit set */
    uint8_t v = spi_transfer(0x00);
    CS_PORT |=  (1 << CS_BIT);
    return v;
}

void adxl345_read_xyz(int16_t *x, int16_t *y, int16_t *z)
{
    CS_PORT &= ~(1 << CS_BIT);
    spi_transfer(0xC0 | ADXL345_DATAX0);   /* multibyte read starting at DATAX0 */

    *x  =  spi_transfer(0x00);
    *x |= (spi_transfer(0x00) << 8);

    *y  =  spi_transfer(0x00);
    *y |= (spi_transfer(0x00) << 8);

    *z  =  spi_transfer(0x00);
    *z |= (spi_transfer(0x00) << 8);

    CS_PORT |=  (1 << CS_BIT);
}
