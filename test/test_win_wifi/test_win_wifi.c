#include "unity.h"
#include "../fff.h"

#include "wifi.h"
#include "uart.h"
#include <stdio.h>
#include <string.h>     /* strlen */

FAKE_VOID_FUNC(sei);
FAKE_VOID_FUNC(cli);
FAKE_VOID_FUNC(_delay_ms, int);

FAKE_VOID_FUNC(uart_send_string_blocking, USART_t, char *);
FAKE_VOID_FUNC(uart_init, USART_t, uint32_t, UART_Callback_t);
FAKE_VOID_FUNC(uart_send_array_blocking, USART_t, uint8_t *, uint16_t);
FAKE_VALUE_FUNC(UART_Callback_t, uart_get_rx_callback, USART_t);
FAKE_VOID_FUNC(uart_send_array_nonBlocking, USART_t, uint8_t *, uint16_t);

uint8_t TEST_BUFFER[128];
void TCP_Received_callback_func();
FAKE_VOID_FUNC(TCP_Received_callback_func);

/* -------------------------------------------------------------------------- */
void setUp(void)
{
    wifi_init();
    RESET_FAKE(uart_init);
    RESET_FAKE(TCP_Received_callback_func);
}

void tearDown(void) {}

/* Helpers ------------------------------------------------------------------ */
static void fake_wifiModule_send(char *cArray, int length)
{
    wifi_command_AT();
    UART_Callback_t cb = uart_init_fake.arg2_history[0];   /* save first cb */
    for (int i = 0; i < length; i++)
        cb((uint8_t)cArray[i]);
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

/* Unit tests ----------------------------------------------------------------*/
void test_wifi_default_callback_func_is_null(void)
{
    TEST_ASSERT_EQUAL(NULL, uart_init_fake.arg2_val);
}

void test_wifi_command_AT_sends_correct_stuff_to_uart(void)
{
    wifi_init();
    uart_get_rx_callback_fake.return_val = uart_init_fake.arg2_val;

    WIFI_ERROR_MESSAGE_t err = wifi_command_AT();
    (void)err;          /* silence “set but not used” if optimised */

    TEST_ASSERT_EQUAL(USART_2, uart_send_string_blocking_fake.arg0_val);
    TEST_ASSERT_EQUAL(1,        uart_send_string_blocking_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("AT\r\n", uart_send_string_blocking_fake.arg1_val);
}

void test_wifi_command_AT_error_code_is_ok_when_receiving_OK_from_hardware(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK, wifi_command_AT());
}

void test_wifi_command_AT_error_code_is_WIFI_ERROR_RECEIVED_ERROR_when_receiving_nothing(void)
{
    TEST_ASSERT_EQUAL(WIFI_ERROR_NOT_RECEIVING, wifi_command_AT());
}

void test_wifi_command_AT_error_code_is_WIFI_ERROR_RECEIVED_ERROR_when_receiving_ERROR(void)
{
    fake_wifiModule_send("ERROR", 6);
    TEST_ASSERT_EQUAL(WIFI_ERROR_RECEIVED_ERROR, wifi_command_AT());
}

void test_wifi_command_AT_error_code_is_WIFI_ERROR_GARBAGE_when_receiving_garbage(void)
{
    fake_wifiModule_send("ER\0OR", 6);
    TEST_ASSERT_EQUAL(WIFI_ERROR_RECEIVING_GARBAGE, wifi_command_AT());
}

/* ---- AP join ------------------------------------------------------------- */
void test_wifi_join_AP_OK(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK,
                      wifi_command_join_AP("correct ssid", "correct Password"));
}

void test_wifi_join_AP_FAIL_wrong_ssid_or_password(void)
{
    fake_wifiModule_send("FAIL\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_FAIL,
                      wifi_command_join_AP("Incorrect ssid", "Incorrect Password"));
}

/* ---- TCP connection ------------------------------------------------------ */
void test_wifi_TCP_connection_OK(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK,
                      wifi_command_create_TCP_connection(
                          "The IP adress", 8000, NULL, NULL));

    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK,
                      wifi_command_create_TCP_connection(
                          "The IP adress", 8000,
                          TCP_Received_callback_func, TEST_BUFFER));
}

void test_wifi_TCP_connection_failed(void)
{
    fake_wifiModule_send("FAIL\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_FAIL,
                      wifi_command_create_TCP_connection(
                          "The IP adress", 8000, NULL, NULL));
}

/* ---- Receiving ----------------------------------------------------------- */
void test_wifi_can_receive(void)
{
    string_send_from_TCP_server("\r\n+IPD,4:1234");
    TEST_ASSERT_EQUAL_STRING("1234", TEST_BUFFER);
}

void test_wifi_TCP_receives_after_garbage(void)
{
    string_send_from_TCP_server("thisIsAlotOfGarbage\r\n+IPD,4:1234");
    TEST_ASSERT_EQUAL_STRING("1234", TEST_BUFFER);

    string_send_from_TCP_server("\r\n\r\n+IPD,4:1234");
    TEST_ASSERT_EQUAL_STRING("1234", TEST_BUFFER);
}

void test_wifi_TCP_callback_invoked_when_message_complete(void)
{
    string_send_from_TCP_server("\r\n+IPD,1:1");
    TEST_ASSERT_EQUAL(1, TCP_Received_callback_func_fake.call_count);
}

void test_wifi_TCP_callback_not_yet_called_for_incomplete_message(void)
{
    const char *partial = "\r\n+IPD,1:";

    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(
        WIFI_OK,
        wifi_command_create_TCP_connection(
            "The IP adress", 8000,
            TCP_Received_callback_func, TEST_BUFFER));

    UART_Callback_t cb = uart_init_fake.arg2_val;
    for (size_t i = 0; i < strlen(partial); i++)
        cb((uint8_t)partial[i]);

    TEST_ASSERT_EQUAL(0, TCP_Received_callback_func_fake.call_count);

    cb('a');    /* deliver last byte */
    TEST_ASSERT_EQUAL(1, TCP_Received_callback_func_fake.call_count);
}

void test_wifi_TCP_robust_against_prefix_fragment_beforehand(void)
{
    string_send_from_TCP_server("thisIsAlo++IPD,Garbage\r\n+IPD,4:1234");
    TEST_ASSERT_EQUAL_STRING("1234", TEST_BUFFER);
    TEST_ASSERT_EQUAL(1, TCP_Received_callback_func_fake.call_count);

    string_send_from_TCP_server("\r\n\r\n+IPD,4:1234");
    TEST_ASSERT_EQUAL_STRING("1234", TEST_BUFFER);
}

void test_wifi_zeroes_in_data(void)
{
    array_send_from_TCP_server("\0+IPD,4:B2\0a", 12);
    TEST_ASSERT_EQUAL_STRING("B2\0a", TEST_BUFFER);
    TEST_ASSERT_EQUAL(1, TCP_Received_callback_func_fake.call_count);
}

/* ---- Sending ------------------------------------------------------------- */
void test_wifi_send(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK, wifi_command_TCP_transmit((uint8_t *)"sendThis", 8));
    TEST_ASSERT_EQUAL_STRING("AT+CIPSEND=8\r\n",
                             uart_send_string_blocking_fake.arg1_val);
    TEST_ASSERT_EQUAL_STRING("sendThis", uart_send_array_nonBlocking_fake.arg1_val);
}

void test_wifi_send_data_with_zero(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    const char *msg = "sendTh\0s!A!";
    TEST_ASSERT_EQUAL(WIFI_OK, wifi_command_TCP_transmit((uint8_t *)msg, 11));
    TEST_ASSERT_EQUAL_STRING("AT+CIPSEND=11\r\n",
                             uart_send_string_blocking_fake.arg1_val);
    TEST_ASSERT_EQUAL_INT8_ARRAY(msg,
                                 uart_send_array_nonBlocking_fake.arg1_val, 11);
}

/* ---- Quit AP ------------------------------------------------------------- */
void test_wifi_quit_AP(void)
{
    fake_wifiModule_send("OK\r\n", 5);
    TEST_ASSERT_EQUAL(WIFI_OK, wifi_command_quit_AP());
    TEST_ASSERT_EQUAL_STRING("AT+CWQAP\r\n",
                             uart_send_string_blocking_fake.arg1_val);
}

/* Runner ------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();

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

    RUN_TEST(test_wifi_can_receive);
    RUN_TEST(test_wifi_TCP_receives_after_garbage);
    RUN_TEST(test_wifi_TCP_callback_invoked_when_message_complete);
    RUN_TEST(test_wifi_TCP_callback_not_yet_called_for_incomplete_message);
    RUN_TEST(test_wifi_TCP_robust_against_prefix_fragment_beforehand);
    RUN_TEST(test_wifi_zeroes_in_data);

    RUN_TEST(test_wifi_send);
    RUN_TEST(test_wifi_send_data_with_zero);

    RUN_TEST(test_wifi_quit_AP);

    return UNITY_END();
}
