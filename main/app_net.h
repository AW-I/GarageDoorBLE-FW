#ifndef __APP_NET_H_
#define __APP_NET_H_

#include <stddef.h>
#include "esp_err.h"

/** @brief Bring up Wi-Fi + Home Assistant from the stored settings. */
esp_err_t app_net_init(void);

/** @brief Re-read settings and reconnect Wi-Fi and MQTT. */
esp_err_t app_net_apply(void);

/** @brief Stable id derived from the MAC, e.g. "gdc_a1b2c3". */
void app_net_node_id(char *out, size_t out_len);

#endif
