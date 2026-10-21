#ifndef __NET_WIFI_H_
#define __NET_WIFI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
	NET_WIFI_DISABLED,   /* No SSID configured. */
	NET_WIFI_CONNECTING,
	NET_WIFI_CONNECTED,  /* Associated and has an IP. */
} net_wifi_state_t;

typedef void (*net_wifi_state_cb_t)(net_wifi_state_t state);

/** @brief Create netif/event loop and initialise the Wi-Fi driver in STA mode (not started). */
esp_err_t net_wifi_init(net_wifi_state_cb_t cb);

/**
 * @brief (Re)connect using the given credentials. Empty ssid stops Wi-Fi.
 * Retries every minute after any drop.
 */
esp_err_t net_wifi_apply(const char *ssid, const char *pass, const char *hostname);

net_wifi_state_t net_wifi_get_state(void);

/** @brief Seconds spent trying to connect without success, 0 when connected or disabled. */
uint32_t net_wifi_down_s(void);

/** @brief RSSI of the current AP in dBm, 0 when not connected. */
int8_t net_wifi_rssi(void);

/** @brief Dotted IPv4 address, "" when not connected. */
void net_wifi_ip_str(char *out, size_t out_len);

#endif
