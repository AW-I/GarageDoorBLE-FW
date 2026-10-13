#include "esp_log.h"
#include "sdkconfig.h"

#include "app_door.h"
#include "motor.h"
#include "ble_gdp.h"
#include "ha_mqtt.h"
#include "net_wifi.h"
#include "settings.h"

static const char *TAG = "DOOR";

/* Legacy state byte (V1): bit0 = door closed, bit1 = input mode. */
#define LEGACY_DOOR_OPEN 0x00
#define LEGACY_DOOR_CLOSED 0x01

/* Extended state flags byte. */
#define EXT_FLAG_WIFI_CONFIGURED (1 << 0)
#define EXT_FLAG_WIFI_CONNECTED (1 << 1)
#define EXT_FLAG_MQTT_CONNECTED (1 << 2)

static const motor_cmd_t s_action_cmd[] = {
	[DOOR_ACT_OPEN] = MOTOR_CMD_OPEN,       [DOOR_ACT_STOP] = MOTOR_CMD_STOP,
	[DOOR_ACT_CLOSE] = MOTOR_CMD_CLOSE,     [DOOR_ACT_PARTIAL] = MOTOR_CMD_PARTIAL,
	[DOOR_ACT_LED_ON] = MOTOR_CMD_LED_ON,   [DOOR_ACT_LED_OFF] = MOTOR_CMD_LED_OFF,
};

static void update_ble(const motor_state_t *st) {
	/* Unknown counts as open so the app never shows a false "closed". */
	uint8_t closed = (st->valid && st->position == 0) ? LEGACY_DOOR_CLOSED : LEGACY_DOOR_OPEN;
	ble_gdp_set_state(closed | (uint8_t)(settings_get_input_mode() << 1));

	uint8_t flags = 0;
	net_wifi_state_t ws = net_wifi_get_state();
	if (ws != NET_WIFI_DISABLED) {
		flags |= EXT_FLAG_WIFI_CONFIGURED;
	}
	if (ws == NET_WIFI_CONNECTED) {
		flags |= EXT_FLAG_WIFI_CONNECTED;
	}
	if (ha_mqtt_is_connected()) {
		flags |= EXT_FLAG_MQTT_CONNECTED;
	}
	const uint8_t ext[BLE_GDP_EXT_STATE_LEN] = {
		st->valid ? st->position : 0xFF,
		st->valid ? (uint8_t)st->op : 0xFF,
		st->led < 0 ? 0xFF : (uint8_t)st->led,
		flags,
	};
	ble_gdp_set_ext_state(ext);
}

static void update_ha(const motor_state_t *st) {
	ha_door_state_t hs = {.position = -1, .op = HA_OP_UNKNOWN, .led = st->led};
	if (st->valid) {
		hs.position = st->position;
		switch (st->op) {
		case MOTOR_OP_OPENING:
			hs.op = HA_OP_OPENING;
			break;
		case MOTOR_OP_CLOSING:
			hs.op = HA_OP_CLOSING;
			break;
		case MOTOR_OP_STOPPED:
			hs.op = HA_OP_STOPPED;
			break;
		default:
			break;
		}
	}
	ha_mqtt_publish_state(&hs);
}

static void on_motor_state(const motor_state_t *st, void *ctx) {
	update_ble(st);
	update_ha(st);
}

esp_err_t app_door_init(void) {
	const motor_config_t cfg = {
		.uart_port = CONFIG_GDC_MOTOR_UART_PORT,
		.tx_gpio = CONFIG_GDC_MOTOR_TX_GPIO,
		.rx_gpio = CONFIG_GDC_MOTOR_RX_GPIO,
		.baud_rate = CONFIG_GDC_MOTOR_BAUD,
		.tx_gap_ms = CONFIG_GDC_MOTOR_TX_GAP_MS,
		.tx_retries = CONFIG_GDC_MOTOR_TX_RETRIES,
		.echo_timeout_ms = CONFIG_GDC_MOTOR_ECHO_TIMEOUT_MS,
	};
	esp_err_t err = motor_init(&cfg);
	if (err != ESP_OK) {
		return err;
	}
	err = motor_add_listener(on_motor_state, NULL);
	app_door_refresh_status();
	return err;
}

esp_err_t app_door_action(door_action_t action, const char *source) {
	motor_cmd_t cmd;
	if (action == DOOR_ACT_TOGGLE) {
		/* Wall-button semantics: moving -> stop, closed -> open, otherwise close. */
		motor_state_t st;
		motor_get_state(&st);
		if (st.valid && (st.op == MOTOR_OP_OPENING || st.op == MOTOR_OP_CLOSING)) {
			cmd = MOTOR_CMD_STOP;
		} else if (!st.valid || st.position == 0) {
			cmd = MOTOR_CMD_OPEN;
		} else {
			cmd = MOTOR_CMD_CLOSE;
		}
	} else if (action < DOOR_ACT_MAX) {
		cmd = s_action_cmd[action];
	} else {
		return ESP_ERR_INVALID_ARG;
	}
	ESP_LOGI(TAG, "%s requested %s.", source, motor_cmd_str(cmd));
	return motor_send(cmd);
}

void app_door_refresh_status(void) {
	motor_state_t st;
	motor_get_state(&st);
	update_ble(&st);
}
