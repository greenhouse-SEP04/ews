#include "api_client.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>
#include <avr/pgmspace.h>

#define API_HOST  "192.168.1.42"
#define API_PORT  80
#define API_USER  "tcp_worker"
#define API_PASS  "supersecret"
#define TOKEN_TTL 1800UL          /* 30 minutes */

static char jwt[256];
static uint32_t jwt_exp = 0;      /* millis() when it dies */

/* Very small helper that re-uses your existing AT commands.
   Blocks for ≤150 ms on a typical LAN.
*/
static bool http_request(const char *req, char *resp, size_t resp_sz)
{
    if (wifi_command_create_TCP_connection(API_HOST, API_PORT, NULL, NULL) != WIFI_OK)
        return false;

    if (wifi_command_TCP_transmit((uint8_t *)req, strlen(req)) != WIFI_OK)
        return false;

    /* crude RX – we only care about the body part that fits in resp */
    size_t i = 0;
    uint8_t b;
    while (uart_get_rx_callback(USART_WIFI) == NULL) ; /* ensure driver alive */
    while (uart_get_rx_callback(USART_WIFI) != NULL && i < resp_sz - 1) {
        /* blocking read wrapper */
        if (uart_get_rx_callback(USART_WIFI)(&b), 1) {
            resp[i++] = (char)b;
        }
    }
    resp[i] = '\0';
    wifi_command_close_TCP_connection();
    return true;
}

/* ------------------------------------------------------------------ */
bool api_authenticate(void)
{
    extern uint32_t millis(void);   /* from util.c */
    if (millis() < jwt_exp && jwt[0])      /* still fresh */
        return true;

    char body[64];
    snprintf(body, sizeof(body),
             "username=%s&password=%s", API_USER, API_PASS);

    char req[256];
    snprintf(req, sizeof(req),
        "POST /auth/token HTTP/1.1\r\n"
        "Host: " API_HOST "\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %d\r\n\r\n%s",
        (int)strlen(body), body);

    char resp[256];
    if (!http_request(req, resp, sizeof(resp)))
        return false;

    char *p = strstr(resp, "\"access_token\":\"");
    if (!p) return false;
    p += 16;
    char *q = strchr(p, '"');
    if (!q) return false;
    size_t n = q - p;
    memcpy(jwt, p, n); jwt[n] = '\0';
    jwt_exp = millis() + TOKEN_TTL * 1000UL;
    return true;
}

/* ------------------------------------------------------------------ */
static void add_auth_hdr(char *hdr)
{
    strcat(hdr, "Authorization: Bearer ");
    strcat(hdr, jwt);
    strcat(hdr, "\r\n");
}

/* ------------------------------------------------------------------ */
bool api_send_reading(const SensorState *s)
{
    if (!api_authenticate()) return false;

    char json[160];
    /* Build JSON exactly as before */
    snprintf(json, sizeof(json),
        "{\"temp\":%u.%u,\"hum\":%u.%u,\"soil\":%u,\"light\":%u,"
        "\"dist\":%u,\"motion\":%s,\"acc\":[%d,%d,%d]}",
        s->tmp_i, s->tmp_d, s->hum_i, s->hum_d,
        s->soil, s->light, s->dist,
        s->motion ? "true" : "false",
        s->acc_x, s->acc_y, s->acc_z);

    char req[400];
    snprintf(req, sizeof(req),
        "POST /sensors/ HTTP/1.1\r\n"
        "Host: " API_HOST "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n",
        (int)strlen(json));
    add_auth_hdr(req);
    strcat(req, "\r\n");
    strcat(req, json);

    char dummy[32];
    return http_request(req, dummy, sizeof(dummy));
}

/* ------------------------------------------------------------------ */
bool api_get_settings(GreenhouseSettings *gs)
{
    if (!api_authenticate()) return false;

    char req[200];
    strcpy(req,
        "GET /settings/ HTTP/1.1\r\n"
        "Host: " API_HOST "\r\n");
    add_auth_hdr(req);
    strcat(req, "\r\n");

    char resp[256];
    if (!http_request(req, resp, sizeof(resp)))
        return false;

    /* Tiny JSON payload – parse with sscanf + strstr for speed */
    int tmin, tmax, lmin, lmax, hmin, hmax, soil;
    if (sscanf(resp,
        "%*[^t]temp_min\":%d%*[^t]temp_max\":%d%*[^l]light_min\":%d"
        "%*[^l]light_max\":%d%*[^h]hum_min\":%d%*[^h]hum_max\":%d"
        "%*[^s]soil_min\":%d",
        &tmin, &tmax, &lmin, &lmax, &hmin, &hmax, &soil) != 7)
        return false;

    gs->temp_min  = tmin;   gs->temp_max  = tmax;
    gs->light_min = lmin;   gs->light_max = lmax;
    gs->hum_min   = hmin;   gs->hum_max   = hmax;
    gs->soil_min  = soil;
    return true;
}

/* ------------------------------------------------------------------ */
/* NEW: api_predict – POST /ml/predict with {"soil":…,"hum":…,"temp":…,"light":…}  */
/* Returns true on success, sets *should_irrigate.  False = we couldn’t parse. */
bool api_predict(const SensorState *s, bool *should_irrigate)
{
    if (!api_authenticate()) return false;

    /* Build a JSON body with exactly the four fields the ML expects */
    char json[128];
    /* humidity and temp have integer + decimal; match how server expects them */
    snprintf(json, sizeof(json),
        "{\"soil\":%u,\"hum\":%u.%u,\"temp\":%u.%u,\"light\":%u}",
        s->soil,
        s->hum_i, s->hum_d,
        s->tmp_i, s->tmp_d,
        s->light);

    /* Compute length and send a single HTTP request string */
    char req[256];
    snprintf(req, sizeof(req),
        "POST /ml/predict HTTP/1.1\r\n"
        "Host: " API_HOST "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n",
        (int)strlen(json));
    add_auth_hdr(req);
    strcat(req, "\r\n");
    strcat(req, json);

    /* Read at most 256 bytes of response */
    char resp[256];
    if (!http_request(req, resp, sizeof(resp))) {
        return false;
    }

    /* Parse “\"should_irrigate\":true” or false */
    char *p = strstr(resp, "\"should_irrigate\":");
    if (!p) return false;
    p += strlen("\"should_irrigate\":");
    if (strncmp(p, "true", 4) == 0) {
        *should_irrigate = true;
    } else {
        *should_irrigate = false;
    }

    return true;
}