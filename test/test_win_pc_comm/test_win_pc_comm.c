#include "unity.h"
#include "pc_comm.h"
#include "fff.h"

/* NOTE: DEFINE_FFF_GLOBALS is *already* provided once in test_fff_globals.c.
 * Do **not** repeat it here – repeating would create multiple-definition
 * linker errors.
 */

/* ---------- fake the underlying UART layer ------------------------------ */
FAKE_VOID_FUNC(uart_init,                 USART_t, uint32_t, UART_Callback_t);
FAKE_VOID_FUNC(uart_send_array_blocking,  USART_t, uint8_t*, uint16_t);
FAKE_VOID_FUNC(uart_send_string_blocking, USART_t, char*);
FAKE_VOID_FUNC(uart_send_array_nonBlocking, USART_t, uint8_t*, uint16_t);

/* ------------------------------------------------------------------------ */
void setUp(void)
{
    RESET_FAKE(uart_init);
    RESET_FAKE(uart_send_array_blocking);
    RESET_FAKE(uart_send_string_blocking);
    RESET_FAKE(uart_send_array_nonBlocking);
}

void tearDown(void) {}

static void dummy_cb(char c) { (void)c; }

/* ------------------------------------------------------------------------ */
void test_init_passes_args(void)
{
    pc_comm_init(115200, dummy_cb);

    TEST_ASSERT_EQUAL(1, uart_init_fake.call_count);
    TEST_ASSERT_EQUAL(USART_PC_COMM, uart_init_fake.arg0_val);
    TEST_ASSERT_EQUAL(115200,        uart_init_fake.arg1_val);
    TEST_ASSERT_EQUAL_PTR(dummy_cb,  uart_init_fake.arg2_val);
}

void test_blocking_send_delegates(void)
{
    uint8_t data[4] = {1, 2, 3, 4};

    pc_comm_send_array_blocking(data, 4);
    TEST_ASSERT_EQUAL(1, uart_send_array_blocking_fake.call_count);

    pc_comm_send_string_blocking("OK");
    TEST_ASSERT_EQUAL(1, uart_send_string_blocking_fake.call_count);
}

void test_nonblocking_send_delegates(void)
{
    uint8_t d[2] = {9, 9};

    pc_comm_send_array_nonBlocking(d, 2);
    TEST_ASSERT_EQUAL(1, uart_send_array_nonBlocking_fake.call_count);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_passes_args);
    RUN_TEST(test_blocking_send_delegates);
    RUN_TEST(test_nonblocking_send_delegates);
    return UNITY_END();
}
