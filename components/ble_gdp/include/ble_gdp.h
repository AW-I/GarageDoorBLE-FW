#ifndef __BLE_GDP_H_
#define __BLE_GDP_H_

/*
 * Garage Door Protocol (GDP) BLE service on NimBLE. Transport only - the command
 * set lives in the application.
 *
 *  Service  713D0000-503E-4C75-BA94-3148F18D941E
 *  Command  713D0003-...  write / write-no-rsp a command, then read its reply
 *  State    713D0002-...  read / notify, BLE_GDP_STATE_LEN bytes
 *  Data     713D0004-...  write / write-no-rsp bulk data (firmware image chunks)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define BLE_GDP_RX_MAX 256
#define BLE_GDP_RESP_MAX 256
#define BLE_GDP_STATE_LEN 4

typedef void (*ble_gdp_conn_cb_t)(bool connected);

/** @return 0 to accept the write, anything else rejects it (only seen by write-with-response). */
typedef int (*ble_gdp_data_cb_t)(const uint8_t *data, size_t len);

typedef struct {
	const char *device_name;
	ble_gdp_conn_cb_t on_connection; /* Optional, called from the NimBLE host task. */
	ble_gdp_data_cb_t on_data;       /* Optional, called from the NimBLE host task in write order. */
} ble_gdp_config_t;

esp_err_t ble_gdp_init(const ble_gdp_config_t *cfg);

/**
 * @brief Wait for the next command written by the phone.
 * @return Bytes copied into buf (buf is NUL terminated, so max_len must leave room), 0 on timeout.
 */
size_t ble_gdp_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms);

/** @brief Publish the reply the phone reads back from the command characteristic. */
esp_err_t ble_gdp_set_response(const uint8_t *data, size_t len);

/** @brief Update the state characteristic (notifies subscribers on change). */
void ble_gdp_set_state(const uint8_t state[BLE_GDP_STATE_LEN]);

/** @brief Change the advertised name (re-advertises if idle). */
esp_err_t ble_gdp_set_name(const char *name);

esp_err_t ble_gdp_disconnect(void);

/** @brief Forget every bonded phone (they will pair again on next connect). */
esp_err_t ble_gdp_clear_bonds(void);

bool ble_gdp_is_connected(void);

/** @brief Milliseconds since connect or the last write / state read, 0 when not connected. */
uint32_t ble_gdp_idle_ms(void);

#endif
