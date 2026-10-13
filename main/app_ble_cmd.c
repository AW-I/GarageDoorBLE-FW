#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_ble_cmd.h"
#include "app_door.h"
#include "app_net.h"
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
#define DISCONNECT_DELAY_MS 1000
#define NAME_UNLOCK_MS 2000
#define PASS_UNLOCK_MS 1000

/* V1 replied with sizeof() of these strings, i.e. including the NUL - keep byte-identical. */
static const char RESP_OK[] = "OK\r\n";
static const char RESP_ERR[] = "ERROR\r\n";

typedef enum {
	POST_NONE,
	POST_RESTART,
	POST_APPLY,
	POST_FACTORY_RESET,
} post_action_t;

typedef struct {
	const uint8_t *data; /* Payload after the command byte, NUL terminated. */
	size_t len;
	char *resp;          /* Optional data reply; leave resp_len 0 for plain OK. */
	size_t resp_max;
	size_t resp_len;
	post_action_t post;
} cmd_ctx_t;

typedef struct {
	uint8_t cmd_id;
	bool needs_session;
	esp_err_t (*process)(cmd_ctx_t *ctx);
} cmd_callback_t;

typedef struct {
	bool unlocked;
	TickType_t time;
} unlock_t;

static volatile bool s_session;
static unlock_t s_name_unlock;
static unlock_t s_pass_unlock;

static struct {
	uint8_t key;
	size_t len;
	char buf[SETTINGS_MQTT_URI_MAX + 1];
} s_set;

static uint8_t s_rx[BLE_GDP_RX_MAX + 1];
static char s_resp[BLE_GDP_RESP_MAX];

/* ---------- V1 commands ---------- */

/* Two step change used by V1: password unlocks, next write within the window is the value. */
static esp_err_t two_step_change(unlock_t *lock, uint32_t window_ms, setting_id_t id,
								 cmd_ctx_t *ctx) {
	if (ctx->len == 0) {
		return ESP_ERR_INVALID_ARG;
	}
	TickType_t now = xTaskGetTickCount();
	if (!lock->unlocked || (now - lock->time) > pdMS_TO_TICKS(window_ms)) {
		if (!settings_check_password(ctx->data, ctx->len)) {
			lock->unlocked = false;
			return ESP_FAIL;
		}
		lock->unlocked = true;
		lock->time = now;
		ESP_LOGI(TAG, "Setting %d unlocked.", id);
		return ESP_OK;
	}
	lock->unlocked = false;
	size_t len = strnlen((const char *)ctx->data, ctx->len);
	esp_err_t err = settings_set_str(id, (const char *)ctx->data, len);
	if (err == ESP_OK && id == SETTING_DEVICE_NAME) {
		ble_gdp_set_name((const char *)ctx->data);
	}
	return err;
}

static esp_err_t handle_open_door(cmd_ctx_t *ctx) {
	if (!settings_check_password(ctx->data, ctx->len)) {
		ESP_LOGE(TAG, "Door open request failed.");
		return ESP_FAIL;
	}
	return app_door_action(DOOR_ACT_TOGGLE, "BLE");
}

static esp_err_t handle_ap_name(cmd_ctx_t *ctx) {
	return two_step_change(&s_name_unlock, NAME_UNLOCK_MS, SETTING_DEVICE_NAME, ctx);
}

static esp_err_t handle_ap_pass(cmd_ctx_t *ctx) {
	return two_step_change(&s_pass_unlock, PASS_UNLOCK_MS, SETTING_DOOR_PASS, ctx);
}

static esp_err_t handle_reboot(cmd_ctx_t *ctx) {
	ESP_LOGI(TAG, "Got reboot command.");
	ctx->post = POST_RESTART;
	return ESP_OK;
}

static esp_err_t handle_set_input_mode(cmd_ctx_t *ctx) {
	if (ctx->len < 1 || (ctx->data[0] != '0' && ctx->data[0] != '1')) {
		ESP_LOGE(TAG, "Invalid input mode.");
		return ESP_ERR_INVALID_ARG;
	}
	esp_err_t err = settings_set_input_mode(ctx->data[0] == '1' ? SWITCH_NORMALLY_CLOSED
																	 : SWITCH_NORMALLY_OPEN);
	app_door_refresh_status();
	return err;
}

/* ---------- V3 commands ---------- */

static esp_err_t handle_auth(cmd_ctx_t *ctx) {
	if (!settings_check_password(ctx->data, ctx->len)) {
		s_session = false;
		return ESP_FAIL;
	}
	s_session = true;
	ESP_LOGI(TAG, "Session authenticated.");
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
	esp_err_t err = settings_get_str(id, ctx->resp, ctx->resp_max);
	if (err != ESP_OK) {
		return err;
	}
	if (settings_is_secret(id) && ctx->resp[0]) {
		strlcpy(ctx->resp, "********", ctx->resp_max);
	}
	/* Include the NUL so an empty value still produces a non-empty reply. */
	ctx->resp_len = strlen(ctx->resp) + 1;
	return ESP_OK;
}

static esp_err_t handle_apply(cmd_ctx_t *ctx) {
	ctx->post = POST_APPLY;
	return ESP_OK;
}

static esp_err_t handle_info(cmd_ctx_t *ctx) {
	static const char *const wifi_txt[] = {"off", "connecting", "connected"};
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
	ctx->resp_len = (n < 0) ? 0 : ((size_t)n >= ctx->resp_max ? ctx->resp_max : (size_t)n + 1);
	return ESP_OK;
}

static esp_err_t handle_factory_reset(cmd_ctx_t *ctx) {
	ctx->post = POST_FACTORY_RESET;
	return ESP_OK;
}

static const cmd_callback_t cmd_callback_list[] = {
	{CMD_OPEN, false, handle_open_door},
	{CMD_OPEN_ALT, false, handle_open_door},
	{CMD_AP_NAME, false, handle_ap_name},
	{CMD_AP_PASS, false, handle_ap_pass},
	{CMD_REBOOT, false, handle_reboot},
	{CMD_SET_INPUT_MODE, false, handle_set_input_mode},
	{CMD_AUTH, false, handle_auth},
	{CMD_DOOR, true, handle_door},
	{CMD_SET, true, handle_set},
	{CMD_GET, true, handle_get},
	{CMD_APPLY, true, handle_apply},
	{CMD_INFO, true, handle_info},
	{CMD_FACTORY_RESET, true, handle_factory_reset},
};

/* ---------- task ---------- */

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
			.resp = s_resp,
			.resp_max = sizeof(s_resp),
		};
		esp_err_t err = ESP_ERR_NOT_SUPPORTED;
		if (handler != NULL) {
			if (handler->needs_session && !s_session) {
				ESP_LOGE(TAG, "Command 0x%02x needs AUTH first.", cmd_id);
				err = ESP_ERR_INVALID_STATE;
			} else {
				err = handler->process(&ctx);
			}
		}
		memset(s_rx, 0, sizeof(s_rx));

		if (err == ESP_OK) {
			if (ctx.resp_len > 0) {
				ble_gdp_set_response((const uint8_t *)s_resp, ctx.resp_len);
			} else {
				ble_gdp_set_response((const uint8_t *)RESP_OK, sizeof(RESP_OK));
			}
			run_post(ctx.post);
		} else {
			ESP_LOGE(TAG, "Command 0x%02x failed: %s.", cmd_id, esp_err_to_name(err));
			ble_gdp_set_response((const uint8_t *)RESP_ERR, sizeof(RESP_ERR));
			vTaskDelay(pdMS_TO_TICKS(DISCONNECT_DELAY_MS));
			ble_gdp_disconnect();
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
	s_name_unlock.unlocked = false;
	s_pass_unlock.unlocked = false;
	s_set.key = 0;
	s_set.len = 0;
}
