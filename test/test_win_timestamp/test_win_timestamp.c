#include "unity.h"
#include "../fff.h"
#include "timestamp.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*  Minimal stand-in types so we don’t have to pull in the full driver stack  */
typedef enum { WIFI_OK, WIFI_FAIL } WIFI_ERROR_MESSAGE_t;
typedef void (*WIFI_TCP_Callback_t)(void);
typedef enum { USART_0 } USART_t;

/* -------------------------------------------------------------------------- */
/*  FFF fakes for the three functions timestamp_sync_via_http() relies on    */
FAKE_VALUE_FUNC(WIFI_ERROR_MESSAGE_t, wifi_command_create_TCP_connection,
                char*, uint16_t, WIFI_TCP_Callback_t, char*)
FAKE_VALUE_FUNC(WIFI_ERROR_MESSAGE_t, wifi_command_TCP_transmit,
                uint8_t*, uint16_t)
FAKE_VOID_FUNC(uart_send_string_blocking, USART_t, char*)

/* -------------------------------------------------------------------------- */
void setUp(void)
{
    FFF_RESET_HISTORY();
    RESET_FAKE(wifi_command_create_TCP_connection);
    RESET_FAKE(wifi_command_TCP_transmit);
    RESET_FAKE(uart_send_string_blocking);
}

void tearDown(void){}

/* -------------------------------------------------------------------------- */
/* Helper fakes that mimic the Wi-Fi driver behaviour                         */
static WIFI_ERROR_MESSAGE_t
tcp_connect_success(char *ip, uint16_t port,
                    WIFI_TCP_Callback_t cb, char *rx_buf)
{
    (void)ip; (void)port;
    strcpy(rx_buf,
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 22 May 2024 15:34:12 GMT\r\n"
        "Content-Type: text/plain\r\n\r\n");
    cb();                              /* simulate data arrival               */
    return WIFI_OK;
}

static WIFI_ERROR_MESSAGE_t
tcp_connect_fail(char *ip, uint16_t port,
                 WIFI_TCP_Callback_t cb, char *rx_buf)
{
    (void)ip; (void)port; (void)cb; (void)rx_buf;
    return WIFI_FAIL;
}

/* -------------------------------------------------------------------------- */
void test_timestamp_sync_success(void)
{
    wifi_command_create_TCP_connection_fake.custom_fake = tcp_connect_success;
    wifi_command_TCP_transmit_fake.return_val            = WIFI_OK;

    bool ok = timestamp_sync_via_http();
    TEST_ASSERT_TRUE(ok);

    uint8_t h, m, s, d, mo; uint16_t y;
    timestamp_get(&h, &m, &s);
    timestamp_get_date(&d, &mo, &y);

    TEST_ASSERT_EQUAL_UINT8 (15, h);
    TEST_ASSERT_EQUAL_UINT8 (34, m);
    TEST_ASSERT_EQUAL_UINT8 (12, s);
    TEST_ASSERT_EQUAL_UINT8 (22, d);
    TEST_ASSERT_EQUAL_UINT8 (5,  mo);   /* May */
    TEST_ASSERT_EQUAL_UINT16(2024, y);
}

void test_timestamp_sync_tcp_fail(void)
{
    wifi_command_create_TCP_connection_fake.custom_fake = tcp_connect_fail;

    bool ok = timestamp_sync_via_http();
    TEST_ASSERT_FALSE(ok);
}

/* -------------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_timestamp_sync_success);
    RUN_TEST(test_timestamp_sync_tcp_fail);
    return UNITY_END();
}
