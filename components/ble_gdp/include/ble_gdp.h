#ifndef __BLE_GDP_H_
#define __BLE_GDP_H_

/*
 * Garage Door Protocol (GDP) BLE service on NimBLE.
 * UUIDs, advertising payload and characteristic behaviour match the V1 Bluedroid
 * firmware so the existing phone app keeps working.
 *
 *  Service  71 3D 00 00 50 3E 4C 75 BA 94 31 48 F1 8D 94 1E
 *  ..0003   Data receive : write / write-no-rsp (command), read (last response)
 *  ..0002   State        : read / notify, 1 byte  (closed | input_mode << 1)
 *  Notifications reach the connected peer even without a CCCD write (V1 behaviour).
 *  ..0004   Ext. state   : read / notify, BLE_GDP_EXT_STATE_LEN bytes (new in V3)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define BLE_GDP_RX_MAX 256
#define BLE_GDP_RESP_MAX 256
#define BLE_GDP_EXT_STATE_LEN 4

typedef void (*ble_gdp_conn_cb_t)(bool connected);

typedef struct {
	const char *device_name;
	ble_gdp_conn_cb_t on_connection; /* Optional, called from the NimBLE host task. */
} ble_gdp_config_t;

esp_err_t ble_gdp_init(const ble_gdp_config_t *cfg);

/**
 * @brief Wait for the next command written by the phone.
 * @return Bytes copied into buf (buf is NUL terminated, so max_len must leave room), 0 on timeout.
 */
size_t ble_gdp_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms);

/** @brief Publish the reply the phone reads back from the data characteristic. */
esp_err_t ble_gdp_set_response(const uint8_t *data, size_t len);

/** @brief Update the legacy 1 byte state characteristic (notifies on change). */
void ble_gdp_set_state(uint8_t state);

/** @brief Update the extended state characteristic (notifies on change). */
void ble_gdp_set_ext_state(const uint8_t state[BLE_GDP_EXT_STATE_LEN]);

/** @brief Re-send both state notifications (V1 sent them every 200 ms). */
void ble_gdp_resend_state(void);

/** @brief Change the advertised name (re-advertises if idle). */
esp_err_t ble_gdp_set_name(const char *name);

esp_err_t ble_gdp_disconnect(void);
bool ble_gdp_is_connected(void);

/** @brief Milliseconds since connect or the last command, 0 when not connected. */
uint32_t ble_gdp_idle_ms(void);

#endif
