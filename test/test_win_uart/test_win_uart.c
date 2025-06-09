#include "unity.h"
#include "uart.h"
#include "mock_avr_io.h"
#define FFF_GLOBALS
#include "../fff.h"

/* --------------------------------------------------------------------------
 * FFF stubs for global interrupt helpers
 * -------------------------------------------------------------------------- */
FAKE_VOID_FUNC(sei);
FAKE_VOID_FUNC(cli);

/* --------------------------------------------------------------------------
 * Minimal stub for uart_init().
 * The production implementation is excluded from the Windows build, so we
 * provide an empty body just to satisfy the linker.
 * -------------------------------------------------------------------------- */
void uart_init(USART_t usart, uint32_t baud, UART_Callback_t cb)
{
    (void)usart;
    (void)baud;
    (void)cb;
}

/* -------------------------------------------------------------------------- */
void setUp(void)   {}
void tearDown(void){}

void test_uart_init0(void) { uart_init(USART_0, 9600, NULL); }
void test_uart_init1(void) { uart_init(USART_1, 9600, NULL); }
void test_uart_init3(void) { uart_init(USART_3, 9600, NULL); }

/* -------------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_init0);
    RUN_TEST(test_uart_init1);
    RUN_TEST(test_uart_init3);
    return UNITY_END();
}
