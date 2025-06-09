#include "unity.h"
#include "../fff.h"
#include "timestamp.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* lightweight stand-in types */
typedef enum { WIFI_OK, WIFI_FAIL, WIFI_ERROR_NOT_RECEIVING,
               WIFI_ERROR_RECEIVED_ERROR, WIFI_ERROR_RECEIVING_GARBAGE }
               WIFI_ERROR_MESSAGE_t;

typedef void (*WIFI_TCP_Callback_t)(void);
typedef enum { USART_0 } USART_t;

/* -------------------------------------------------------------------------- */
/* FFF fakes */
FAKE_VALUE_FUNC(WIFI_ERROR_MESSAGE_t, wifi_command_create_TCP_connection,
                char*, uint16_t, WIFI_TCP_Callback_t, char*)
FAKE_VALUE_FUNC(WIFI_ERROR_MESSAGE_t, wifi_command_TCP_transmit,
                uint8_t*, uint16_t)
FAKE_VOID_FUNC(uart_send_string_blocking, USART_t, char*)

void setUp(void)
{
    FFF_RESET_HISTORY();
    RESET_FAKE(wifi_command_create_TCP_connection);
    RESET_FAKE(wifi_command_TCP_transmit);
    RESET_FAKE(uart_send_string_blocking);
}

void tearDown(void) {}

/* -------------------------------------------------------------------------- */
/* helper fakes */
static WIFI_ERROR_MESSAGE_t
wifi_command_create_TCP_connection_success(char *ip, uint16_t port,
                                           WIFI_TCP_Callback_t cb, char *buf)
{
    strcpy(buf,
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 22 May 2024 15:34:12 GMT\r\n"
        "Content-Type: text/plain\r\n\r\n");
    cb();                      /* simulate “data arrived” */
    return WIFI_OK;
}

static WIFI_ERROR_MESSAGE_t
wifi_command_create_TCP_connection_fail(char *ip, uint16_t port,
                                        WIFI_TCP_Callback_t cb, char *buf)
{
    (void)ip; (void)port; (void)cb; (void)buf;
    return WIFI_FAIL;
}

/* -------------------------------------------------------------------------- */
/* tests */
void test_timestamp_sync_success(void)
{
    wifi_command_create_TCP_connection_fake.custom_fake
        = wifi_comm_
