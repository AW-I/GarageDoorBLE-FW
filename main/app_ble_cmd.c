#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_ble_cmd.h"
#include "app_door.h"
#include "app_net.h"
#include "app_ota.h"
#include "app_supervisor.h"
#include "ble_gdp.h"
#include "ha_mqtt.h"
#include "motor.h"
#include "net_wifi.h"
#include "settings.h"
#include "version.h"

static const char *TAG = "BLE_CMD";

#define TASK_NAME "BleCmd"
#define TASK_STACK 4096
#define TASK_PRIO 4
#define RX_TIMEOUT_MS 2500
#define AUTH_FAIL_DELAY_MS 1000

typedef enum {
	POST_NONE,
	POST_RESTART,
	POST_APPLY,
	POST_FACTORY_RESET,
} post_action_t;

typedef struct {
	const uint8_t *data; /* Payload after the command byte, NUL terminated. */
	size_t len;
	char *resp;          /* Reply data after the status byte. */
	size_t resp_max;
	size_t resp_len;
	post_action_t post;
} cmd_ctx_t;

typedef struct {
	uint8_t cmd_id;
	esp_err_t (*process)(cmd_ctx_t *ctx);
} cmd_callback_t;

static volatile bool s_session;

static struct {
	uint8_t key;
	size_t len;
	char buf[SETTINGS_MQTT_URI_MAX + 1];
} s_set;

static uint8_t s_rx[BLE_GDP_RX_MAX + 1];
static uint8_t s_resp[BLE_GDP_RESP_MAX];

/* ---------- commands ---------- */

static esp_err_t handle_auth(cmd_ctx_t *ctx) {
	s_session = settings_check_password(ctx->data, ctx->len);
	if (!s_session) {
		return ESP_ERR_INVALID_CRC; /* Mapped to RESP_AUTH. */
	}
	ESP_LOGI(TAG, "Session authenticated.");
	/* A phone got in, so BLE works on this image - keep it after an update. */
	app_ota_confirm();
	return ESP_OK;
}

static esp_err_t handle_door(cmd_ctx_t *ctx) {
	if (ctx->len != 1 || ctx->data[0] >= DOOR_ACT_MAX) {
		return ESP_ERR_INVALID_ARG;
	}
	return app_door_action((door_action_t)ctx->data[0], "BLE");
}

static esp_err_t handle_set(cmd_ctx_t *ctx) {
	if (ctx->len < 2) {
		return ESP_ERR_INVALID_ARG;
	}
	uint8_t key = ctx->data[0];
	uint8_t flags = ctx->data[1];
	const uint8_t *chunk = ctx->data + 2;
	size_t chunk_len = ctx->len - 2;

	if (flags & SET_FLAG_FIRST) {
		s_set.key = key;
		s_set.len = 0;
	} else if (s_set.key != key) {
		ESP_LOGE(TAG, "SET chunk for key %d without a first chunk.", key);
		return ESP_ERR_INVALID_STATE;
	}
	if (s_set.len + chunk_len > sizeof(s_set.buf) - 1) {
		s_set.key = 0;
		return ESP_ERR_INVALID_SIZE;
	}
	memcpy(s_set.buf + s_set.len, chunk, chunk_len);
	s_set.len += chunk_len;
	if (!(flags & SET_FLAG_LAST)) {
		return ESP_OK;
	}

	s_set.buf[s_set.len] = '\0';
	esp_err_t err = settings_set_str((setting_id_t)key, s_set.buf, s_set.len);
	if (err == ESP_OK && key == SETTING_DEVICE_NAME) {
		ble_gdp_set_name(s_set.buf);
	}
	memset(s_set.buf, 0, sizeof(s_set.buf)); /* Don't keep secrets around. */
	s_set.key = 0;
	s_set.len = 0;
	return err;
}

static esp_err_t handle_get(cmd_ctx_t *ctx) {
	if (ctx->len != 1 || ctx->data[0] == 0 || ctx->data[0] >= SETTING_MAX) {
		return ESP_ERR_INVALID_ARG;
	}
	setting_id_t id = (setting_id_t)ctx->data[0];
	char *out = ctx->resp;
	esp_err_t err = settings_get_str(id, out, ctx->resp_max);
	if (err != ESP_OK) {
		return err;
	}
	if (settings_is_secret(id) && out[0]) {
		strlcpy(out, "********", ctx->resp_max);
	}
	ctx->resp_len = strlen(out);
	return ESP_OK;
}

static esp_err_t handle_apply(cmd_ctx_t *ctx) {
	ctx->post = POST_APPLY;
	return ESP_OK;
}

static esp_err_t handle_info(cmd_ctx_t *ctx) {
	static const char *const wifi_txt[] = {"Off", "Connecting", "Connected"};
	char name[SETTINGS_NAME_MAX + 1];
	char ssid[SETTINGS_WIFI_SSID_MAX + 1];
	char ip[16];
	motor_state_t st;
	settings_get_str(SETTING_DEVICE_NAME, name, sizeof(name));
	settings_get_str(SETTING_WIFI_SSID, ssid, sizeof(ssid));
	net_wifi_ip_str(ip, sizeof(ip));
	motor_get_state(&st);

	int n = snprintf(ctx->resp, ctx->resp_max,
					 "fw=%s;name=%s;wifi=%s;ssid=%s;ip=%s;rssi=%d;mqtt=%d;pos=%d;op=%s;led=%d", VERSION,
					 name, wifi_txt[net_wifi_get_state()], ssid, ip, net_wifi_rssi(),
					 ha_mqtt_is_connected(), st.valid ? st.position : -1, motor_op_str(st.op), st.led);
	ctx->resp_len = (n < 0) ? 0 : ((size_t)n >= ctx->resp_max ? ctx->resp_max - 1 : (size_t)n);
	return ESP_OK;
}

static esp_err_t handle_reboot(cmd_ctx_t *ctx) {
	ctx->post = POST_RESTART;
	return ESP_OK;
}

static esp_err_t handle_factory_reset(cmd_ctx_t *ctx) {
	ctx->post = POST_FACTORY_RESET;
	return ESP_OK;
}

static uint32_t read_u32_le(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static esp_err_t handle_ota_begin(cmd_ctx_t *ctx) {
	if (ctx->len != 4) {
		return ESP_ERR_INVALID_ARG;
	}
	return app_ota_begin(read_u32_le(ctx->data));
}

static esp_err_t handle_ota_status(cmd_ctx_t *ctx) {
	uint32_t received;
	esp_err_t err = app_ota_status(&received);
	if (err != ESP_OK) {
		return err;
	}
	for (int i = 0; i < 4; i++) {
		ctx->resp[i] = (char)(received >> (8 * i));
	}
	ctx->resp_len = 4;
	return ESP_OK;
}

static esp_err_t handle_ota_end(cmd_ctx_t *ctx) {
	esp_err_t err = app_ota_finish();
	if (err == ESP_OK) {
		ctx->post = POST_RESTART;
	}
	return err;
}

static esp_err_t handle_ota_abort(cmd_ctx_t *ctx) {
	app_ota_abort();
	return ESP_OK;
}

static const cmd_callback_t cmd_callback_list[] = {
	{CMD_AUTH, handle_auth},
	{CMD_DOOR, handle_door},
	{CMD_SET, handle_set},
	{CMD_GET, handle_get},
	{CMD_APPLY, handle_apply},
	{CMD_INFO, handle_info},
	{CMD_REBOOT, handle_reboot},
	{CMD_FACTORY_RESET, handle_factory_reset},
	{CMD_OTA_BEGIN, handle_ota_begin},
	{CMD_OTA_STATUS, handle_ota_status},
	{CMD_OTA_END, handle_ota_end},
	{CMD_OTA_ABORT, handle_ota_abort},
};

/* ---------- task ---------- */

static resp_status_t status_from_err(esp_err_t err) {
	switch (err) {
	case ESP_OK:
		return RESP_OK;
	case ESP_ERR_INVALID_CRC:
		return RESP_AUTH;
	case ESP_ERR_NOT_SUPPORTED:
		return RESP_UNKNOWN;
	case ESP_ERR_INVALID_ARG:
	case ESP_ERR_INVALID_SIZE:
	case ESP_ERR_INVALID_STATE:
		return RESP_INVALID;
	default:
		return RESP_FAILED;
	}
}

static void run_post(post_action_t post) {
	switch (post) {
	case POST_RESTART:
		app_supervisor_request_restart();
		break;
	case POST_APPLY:
		app_net_apply();
		break;
	case POST_FACTORY_RESET:
		settings_factory_reset();
		ble_gdp_clear_bonds();
		app_supervisor_request_restart();
		break;
	default:
		break;
	}
}

static void ble_cmd_task(void *arg) {
	const size_t count = sizeof(cmd_callback_list) / sizeof(cmd_callback_list[0]);
	for (;;) {
		size_t rx_len = ble_gdp_receive(s_rx, sizeof(s_rx), RX_TIMEOUT_MS);
		if (rx_len == 0) {
			continue;
		}

		const uint8_t cmd_id = s_rx[0];
		const cmd_callback_t *handler = NULL;
		for (size_t i = 0; i < count; i++) {
			if (cmd_callback_list[i].cmd_id == cmd_id) {
				handler = &cmd_callback_list[i];
				break;
			}
		}

		cmd_ctx_t ctx = {
			.data = s_rx + 1,
			.len = rx_len - 1,
			.resp = (char *)s_resp + 1,
			.resp_max = sizeof(s_resp) - 1,
		};
		esp_err_t err;
		if (handler == NULL) {
			err = ESP_ERR_NOT_SUPPORTED;
		} else if (cmd_id != CMD_AUTH && !s_session) {
			err = ESP_ERR_INVALID_CRC;
		} else {
			err = handler->process(&ctx);
		}
		memset(s_rx, 0, sizeof(s_rx));

		resp_status_t status = status_from_err(err);
		s_resp[0] = (uint8_t)status;
		ble_gdp_set_response(s_resp, status == RESP_OK ? ctx.resp_len + 1 : 1);

		if (status == RESP_OK) {
			run_post(ctx.post);
		} else {
			ESP_LOGE(TAG, "Command 0x%02x failed: %s.", cmd_id, esp_err_to_name(err));
			if (status == RESP_AUTH) {
				/* Let the phone read the reply, then make password guessing slow. */
				vTaskDelay(pdMS_TO_TICKS(AUTH_FAIL_DELAY_MS));
				ble_gdp_disconnect();
			}
		}
	}
}

esp_err_t app_ble_cmd_init(void) {
	if (xTaskCreate(ble_cmd_task, TASK_NAME, TASK_STACK, NULL, TASK_PRIO, NULL) != pdPASS) {
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

void app_ble_cmd_on_connection(bool connected) {
	s_session = false;
	app_ota_abort(); /* An update never outlives the connection that started it. */
	s_set.key = 0;
	s_set.len = 0;
}
