#ifndef __HA_MQTT_H_
#define __HA_MQTT_H_

/*
 * Home Assistant integration over MQTT with auto-discovery.
 * Entities: cover (garage), light (motor LED), button (restart),
 * sensors (position %, operation, Wi-Fi RSSI). Availability via LWT.
 */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
	HA_CMD_OPEN,
	HA_CMD_CLOSE,
	HA_CMD_STOP,
	HA_CMD_LED_ON,
	HA_CMD_LED_OFF,
	HA_CMD_RESTART,
} ha_cmd_t;

typedef enum {
	HA_OP_UNKNOWN,
	HA_OP_OPENING,
	HA_OP_CLOSING,
	HA_OP_STOPPED,
} ha_op_t;

typedef struct {
	int position; /* 0..100, -1 unknown */
	ha_op_t op;
	int led;      /* 1/0, -1 unknown */
} ha_door_state_t;

typedef void (*ha_cmd_cb_t)(ha_cmd_t cmd);
typedef void (*ha_conn_cb_t)(bool connected);

typedef struct {
	const char *uri;          /* mqtt://host:1883 or mqtts://host:8883 */
	const char *username;     /* Optional */
	const char *password;     /* Optional */
	const char *device_name;  /* Friendly name shown in HA */
	const char *node_id;      /* Unique, [a-z0-9_] - used in topics and unique_ids */
	const char *sw_version;
	const char *discovery_prefix; /* Usually "homeassistant" */
	const char *base_topic;       /* e.g. "garage_door" -> garage_door/<node_id>/... */
	ha_cmd_cb_t on_command;
	ha_conn_cb_t on_connection;
} ha_mqtt_config_t;

/** @brief Start (or restart with new settings) the MQTT client. Empty uri just stops it. */
esp_err_t ha_mqtt_start(const ha_mqtt_config_t *cfg);

/** @brief Publish "offline" and stop the client. */
esp_err_t ha_mqtt_stop(void);

bool ha_mqtt_is_connected(void);

/** @brief Publish door state (only what changed since the last call). */
void ha_mqtt_publish_state(const ha_door_state_t *state);

void ha_mqtt_publish_rssi(int rssi);

#endif
