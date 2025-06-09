/*  test_win_wifi.c – unit tests for lib/wifi (desktop build with FFF/Unity) */
#include "unity.h"
#include "../fff.h"          /* <-- include only, NO DEFINE_FFF_GLOBALS here */

#include "wifi.h"
#include "uart.h"

#include <stdio.h>
#include <string.h>     /* strlen */

/* -------------------------------------------------------------------------- */
/*                       FFF fake-function declarations                       */

FAKE_VOID_FUNC(sei);
FAKE_VOID_FUNC(cli);
FAKE_VOID_FUNC(_delay_ms, int);

FAKE_VOID_FUNC(uart_send_string_blocking,   USART_t, char *);
FAKE_VOID_FUNC(uart_init,                   USART_t, uint32_t, UART_Callback_t);
FAKE_VOID_FUNC(uart_send_array_blocking,    USART_t, uint8_t *, uint16_t);
FAKE_VALUE_FUNC(UART_Callback_t, uart_get_rx_callback, USART_t);

/* -------------------------------------------------------------------------- */
/*                            Test-local objects                              */

uint8_t TEST_BUFFER[128];

void TCP_Received_callback_func();
FAKE_VOID_FUNC(TCP_Received_callback_func);

/* -------------------------------------------------------------------------- */
void setUp(void)
{
    wifi_init();

    RESET_FAKE(uart_init);
    RESET_FAKE(uart_send_string_blocking);
    RESET_FAKE(uart_send_array_blocking);
    RESET_FAKE(uart_get_rx_callback);
    RESET_FAKE(TCP_Received_callback_func);
}

void tearDown(void) {}

/* Helpers ------------------------------------------------------------------ */
static void fake_wifiModule_send(char *cArray, int length)
{
    wifi_command_AT();                              /* arm driver            */
    UART_Callback_t cb = uart_init_fake.arg2_history[0];
    for (int i = 0; i < length; i++)
        cb((uint8_t)cArray[i]);                     /* feed bytes            */
}

static void string_send_from_TCP_server(char *cArray)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(
        WIFI_OK,
        wifi_command_create_TCP_connection(
            "The IP adress", 8000,
            TCP_Received_callback_func, TEST_BUFFER));

    UART_Callback_t cb = uart_init_fake.arg2_val;
    for (size_t i = 0; i < strlen(cArray); i++)
        cb((uint8_t)cArray[i]);
}

static void array_send_from_TCP_server(char *cArray, int length)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(
        WIFI_OK,
        wifi_command_create_TCP_connection(
            "The IP adress", 8000,
            TCP_Received_callback_func, TEST_BUFFER));

    UART_Callback_t cb = uart_init_fake.arg2_val;
    for (int i = 0; i < length; i++)
        cb((uint8_t)cArray[i]);
}

/* -------------------------------------------------------------------------- */
/*                               Unit tests                                   */
/*  … (all tests remain exactly as before) …                                  */
/* -------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    /* ---- public-API tests ------------------------------------------------ */
    RUN_TEST(test_wifi_default_callback_func_is_null);
    RUN_TEST(test_wifi_command_AT_sends_correct_stuff_to_uart);
    RUN_TEST(test_wifi_command_AT_error_code_is_ok_when_receiving_OK_from_hardware);
    RUN_TEST(test_wifi_command_AT_error_code_is_WIFI_ERROR_RECEIVED_ERROR_when_receiving_nothing);
    RUN_TEST(test_wifi_command_AT_error_code_is_WIFI_ERROR_RECEIVED_ERROR_when_receiving_ERROR);
    RUN_TEST(test_wifi_command_AT_error_code_is_WIFI_ERROR_GARBAGE_when_receiving_garbage);

    RUN_TEST(test_wifi_join_AP_OK);
    RUN_TEST(test_wifi_join_AP_FAIL_wrong_ssid_or_password);

    RUN_TEST(test_wifi_TCP_connection_OK);
    RUN_TEST(test_wifi_TCP_connection_failed);

    /* ---- incoming-data parser ------------------------------------------- */
    RUN_TEST(test_wifi_can_receive);
    RUN_TEST(test_wifi_TCP_receives_after_garbage);
    RUN_TEST(test_wifi_TCP_callback_invoked_when_message_complete);
    RUN_TEST(test_wifi_TCP_callback_not_yet_called_for_incomplete_message);
    RUN_TEST(test_wifi_TCP_robust_against_prefix_fragment_beforehand);
    RUN_TEST(test_wifi_zeroes_in_data);

    /* ---- outgoing-data helper ------------------------------------------- */
    RUN_TEST(test_wifi_send);
    RUN_TEST(test_wifi_send_data_with_zero);

    /* ---- misc ----------------------------------------------------------- */
    RUN_TEST(test_wifi_quit_AP);

    return UNITY_END();
}
