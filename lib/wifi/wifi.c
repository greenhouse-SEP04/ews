#include "wifi.h"
#include "includes.h"
#include "uart.h"

#define WIFI_DATABUFFERSIZE 128
static uint8_t  wifi_dataBuffer[WIFI_DATABUFFERSIZE];
static uint8_t  wifi_dataBufferIndex;
static uint32_t wifi_baudrate;

/* -------------------------------------------------------------------------- */
void wifi_init(void)
{
    wifi_baudrate = 115200;
    uart_init(USART_WIFI, wifi_baudrate, NULL);
}

/* -------------------------------------------------------------------------- */
static void wifi_clear_databuffer_and_index(void)
{
    for (uint16_t i = 0; i < WIFI_DATABUFFERSIZE; i++)
        wifi_dataBuffer[i] = 0;
    wifi_dataBufferIndex = 0;
}

/* ––––– generic AT-command helper –––––------------------------------------ */
static void wifi_command_callback(uint8_t received_byte)
{
    wifi_dataBuffer[wifi_dataBufferIndex++] = received_byte;
}

static WIFI_ERROR_MESSAGE_t wifi_command(const char *cmd, uint16_t timeout_s)
{
    UART_Callback_t old_cb = uart_get_rx_callback(USART_WIFI);
    uart_init(USART_WIFI, wifi_baudrate, wifi_command_callback);

    char buf[128];
    strcpy(buf, cmd);
    uart_send_string_blocking(USART_WIFI, strcat(buf, "\r\n"));

    for (uint16_t i = 0; i < timeout_s * 100UL; i++) {
        _delay_ms(10);
        if (strstr((char *)wifi_dataBuffer, "OK\r\n"))
            break;
    }

    WIFI_ERROR_MESSAGE_t err;
    if (wifi_dataBufferIndex == 0)
        err = WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "OK"))
        err = WIFI_OK;
    else if (strstr((char *)wifi_dataBuffer, "ERROR"))
        err = WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL"))
        err = WIFI_FAIL;
    else
        err = WIFI_ERROR_RECEIVING_GARBAGE;

    wifi_clear_databuffer_and_index();
    uart_init(USART_WIFI, wifi_baudrate, old_cb);
    return err;
}

/* ––––– public convenience wrappers –––––---------------------------------- */
WIFI_ERROR_MESSAGE_t wifi_command_AT(void)                    { return wifi_command("AT", 1); }

WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *pw)
{
    char buf[128];
    sprintf(buf, "AT+CWJAP=\"%s\",\"%s\"", ssid, pw);
    return wifi_command(buf, 20);
}
WIFI_ERROR_MESSAGE_t wifi_command_disable_echo(void)          { return wifi_command("ATE0", 1); }
WIFI_ERROR_MESSAGE_t wifi_command_set_mode_to_1(void)         { return wifi_command("AT+CWMODE=1", 1); }
WIFI_ERROR_MESSAGE_t wifi_command_set_to_single_Connection(void){return wifi_command("AT+CIPMUX=0", 1); }
WIFI_ERROR_MESSAGE_t wifi_command_quit_AP(void)               { return wifi_command("AT+CWQAP", 5); }
WIFI_ERROR_MESSAGE_t wifi_command_close_TCP_connection(void)  { return wifi_command("AT+CIPCLOSE", 5); }

/* ––––– resolve IP from URL –––––------------------------------------------ */
WIFI_ERROR_MESSAGE_t wifi_command_get_ip_from_URL(char *url, char *ip_out)
{
    char cmd[128];
    sprintf(cmd, "AT+CIPDOMAIN=\"%s\"", url);

    UART_Callback_t old_cb = uart_get_rx_callback(USART_WIFI);
    uart_init(USART_WIFI, wifi_baudrate, wifi_command_callback);

    uart_send_string_blocking(USART_WIFI, strcat(cmd, "\r\n"));

    for (uint16_t i = 0; i < 5 * 100UL; i++) {
        _delay_ms(10);
        if (strstr((char *)wifi_dataBuffer, "OK\r\n"))
            break;
    }

    WIFI_ERROR_MESSAGE_t err;
    if (wifi_dataBufferIndex == 0)
        err = WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "OK"))
        err = WIFI_OK;
    else if (strstr((char *)wifi_dataBuffer, "ERROR"))
        err = WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL"))
        err = WIFI_FAIL;
    else
        err = WIFI_ERROR_RECEIVING_GARBAGE;

    char *start = strstr((char *)wifi_dataBuffer, "CIPDOMAIN:");
    if (start) {
        start += 10;
        char *end = strchr(start, '\r');
        if (end && (end - start) < 16) {
            strncpy(ip_out, start, end - start);
            ip_out[end - start] = '\0';
        }
    }

    wifi_clear_databuffer_and_index();
    uart_init(USART_WIFI, wifi_baudrate, old_cb);
    return err;
}

/* ––––– TCP helpers –––––-------------------------------------------------- */
static WIFI_TCP_Callback_t tcp_user_cb   = NULL;
static char               *tcp_user_buf  = NULL;

static void wifi_TCP_callback(uint8_t byte)
{
    static enum { IDLE, MATCH, LEN, DATA } st = IDLE;
    static int len = 0, idx = 0, pi = 0;
    static const char pfx[] = "+IPD,";

    switch (st) {
    case IDLE:
        if (byte == pfx[0]) { st = MATCH; pi = 1; }
        break;
    case MATCH:
        if (byte == pfx[pi]) {
            if (++pi == 5) st = LEN;
        } else { st = IDLE; pi = 0; }
        break;
    case LEN:
        if (byte >= '0' && byte <= '9') {
            len = len * 10 + (byte - '0');
        } else if (byte == ':') {
            st = DATA; idx = 0;
        } else { st = IDLE; len = 0; }
        break;
    case DATA:
        if (idx < len) tcp_user_buf[idx++] = byte;
        if (idx == len) {
            tcp_user_buf[idx] = '\0';
            st = IDLE; len = idx = 0;
            wifi_clear_databuffer_and_index();
            if (tcp_user_cb) tcp_user_cb();
        }
        break;
    }
}

WIFI_ERROR_MESSAGE_t wifi_command_create_TCP_connection(char *ip, uint16_t port,
                                                        WIFI_TCP_Callback_t cb,
                                                        char *rx_buf)
{
    tcp_user_cb  = cb;
    tcp_user_buf = rx_buf;

    char cmd[128];
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%u", ip, port);

    WIFI_ERROR_MESSAGE_t err = wifi_command(cmd, 20);
    if (err != WIFI_OK) return err;

    uart_init(USART_WIFI, wifi_baudrate, wifi_TCP_callback);
    wifi_clear_databuffer_and_index();
    return err;
}

/* -------------------------------------------------------------------------- */
/*                ****  UPDATED FUNCTION  ****                               */
WIFI_ERROR_MESSAGE_t wifi_command_TCP_transmit(uint8_t *data, uint16_t len)
{
    char cmd[32];
    sprintf(cmd, "AT+CIPSEND=%u", len);

    WIFI_ERROR_MESSAGE_t err = wifi_command(cmd, 20);
    if (err != WIFI_OK)
        return err;

    /* Use the NON-blocking variant so the unit tests can inspect the buffer */
    uart_send_array_nonBlocking(USART_WIFI, data, len);
    return WIFI_OK;
}
