#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include "settings.h"

static const char *TAG = "SETTINGS";

#define STORAGE_NAMESPACE "config"

typedef struct {
	const char *key;
	size_t min_len;
	size_t max_len;
	bool secret;
} setting_desc_t;

static const setting_desc_t s_desc[SETTING_MAX] = {
	[SETTING_WIFI_SSID] = {"wifi_ssid", 0, SETTINGS_WIFI_SSID_MAX, false},
	[SETTING_WIFI_PASS] = {"wifi_pass", 0, SETTINGS_WIFI_PASS_MAX, true},
	[SETTING_MQTT_URI] = {"mqtt_uri", 0, SETTINGS_MQTT_URI_MAX, false},
	[SETTING_MQTT_USER] = {"mqtt_user", 0, SETTINGS_MQTT_USER_MAX, false},
	[SETTING_MQTT_PASS] = {"mqtt_pass", 0, SETTINGS_MQTT_PASS_MAX, true},
	[SETTING_DEVICE_NAME] = {"name", 1, SETTINGS_NAME_MAX, false},
	[SETTING_DOOR_PASS] = {"pass", 1, SETTINGS_PASS_MAX, true},
};

/* RAM cache, one slot per id sized for the largest value. */
static char s_values[SETTING_MAX][SETTINGS_MQTT_URI_MAX + 1];
static SemaphoreHandle_t s_lock;

static bool id_valid(setting_id_t id) {
	return id > 0 && id < SETTING_MAX && s_desc[id].key != NULL;
}

static void load_str(nvs_handle_t handle, setting_id_t id, const char *fallback) {
	size_t len = sizeof(s_values[id]);
	esp_err_t err = nvs_get_str(handle, s_desc[id].key, s_values[id], &len);
	if (err != ESP_OK) {
		if (err != ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to read %s: %s.", s_desc[id].key, esp_err_to_name(err));
		}
		strlcpy(s_values[id], fallback ? fallback : "", sizeof(s_values[id]));
	}
}

esp_err_t settings_init(const settings_defaults_t *defaults) {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	if (err != ESP_OK) {
		return err;
	}

	if (s_lock == NULL) {
		s_lock = xSemaphoreCreateMutex();
		if (s_lock == NULL) {
			return ESP_ERR_NO_MEM;
		}
	}

	nvs_handle_t handle;
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open NVS: %s.", esp_err_to_name(err));
		return err;
	}
	for (setting_id_t id = 1; id < SETTING_MAX; id++) {
		const char *fallback = NULL;
		if (id == SETTING_DEVICE_NAME) {
			fallback = defaults->device_name;
		} else if (id == SETTING_DOOR_PASS) {
			fallback = defaults->door_pass;
		}
		load_str(handle, id, fallback);
	}
	nvs_close(handle);

	ESP_LOGI(TAG, "Loaded. Name: %s, Wi-Fi: %s, MQTT: %s.", s_values[SETTING_DEVICE_NAME],
			 s_values[SETTING_WIFI_SSID][0] ? s_values[SETTING_WIFI_SSID] : "<none>",
			 s_values[SETTING_MQTT_URI][0] ? s_values[SETTING_MQTT_URI] : "<none>");
	if (defaults->door_pass && strcmp(s_values[SETTING_DOOR_PASS], defaults->door_pass) == 0) {
		ESP_LOGW(TAG, "Door password is still the factory default - change it.");
	}
	return ESP_OK;
}

esp_err_t settings_get_str(setting_id_t id, char *out, size_t out_len) {
	if (!id_valid(id) || out == NULL || out_len == 0) {
		return ESP_ERR_INVALID_ARG;
	}
	xSemaphoreTake(s_lock, portMAX_DELAY);
	size_t len = strlcpy(out, s_values[id], out_len);
	xSemaphoreGive(s_lock);
	return len < out_len ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t settings_set_str(setting_id_t id, const char *value, size_t len) {
	if (!id_valid(id) || (value == NULL && len > 0)) {
		return ESP_ERR_INVALID_ARG;
	}
	const setting_desc_t *desc = &s_desc[id];
	if (len < desc->min_len || len > desc->max_len || memchr(value, '\0', len) != NULL) {
		ESP_LOGE(TAG, "Invalid length/content for %s: %d.", desc->key, (int)len);
		return ESP_ERR_INVALID_ARG;
	}
	if (id == SETTING_WIFI_PASS && len > 0 && len < 8) {
		ESP_LOGE(TAG, "Wi-Fi password must be empty (open) or 8..64 chars.");
		return ESP_ERR_INVALID_ARG;
	}

	char buf[SETTINGS_MQTT_URI_MAX + 1];
	memcpy(buf, value, len);
	buf[len] = '\0';

	xSemaphoreTake(s_lock, portMAX_DELAY);
	nvs_handle_t handle;
	esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
	if (err == ESP_OK) {
		err = nvs_set_str(handle, desc->key, buf);
		if (err == ESP_OK) {
			err = nvs_commit(handle);
		}
		nvs_close(handle);
	}
	if (err == ESP_OK) {
		strlcpy(s_values[id], buf, sizeof(s_values[id]));
		ESP_LOGI(TAG, "Saved %s.", desc->key);
	} else {
		ESP_LOGE(TAG, "Failed to save %s: %s.", desc->key, esp_err_to_name(err));
	}
	xSemaphoreGive(s_lock);
	return err;
}

bool settings_is_secret(setting_id_t id) {
	return !id_valid(id) || s_desc[id].secret;
}

bool settings_check_password(const uint8_t *pass, size_t len) {
	if (pass == NULL || len == 0 || len > SETTINGS_PASS_MAX) {
		return false;
	}
	xSemaphoreTake(s_lock, portMAX_DELAY);
	const char *stored = s_values[SETTING_DOOR_PASS];
	size_t stored_len = strlen(stored);
	/* Always walk the full max length so timing does not leak the match position. */
	uint8_t diff = (uint8_t)(stored_len != len);
	for (size_t i = 0; i < SETTINGS_PASS_MAX; i++) {
		uint8_t a = i < stored_len ? (uint8_t)stored[i] : 0;
		uint8_t b = i < len ? pass[i] : 0;
		diff |= a ^ b;
	}
	xSemaphoreGive(s_lock);
	return stored_len > 0 && diff == 0;
}

esp_err_t settings_factory_reset(void) {
	nvs_handle_t handle;
	esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		return err;
	}
	err = nvs_erase_all(handle);
	if (err == ESP_OK) {
		err = nvs_commit(handle);
	}
	nvs_close(handle);
	ESP_LOGW(TAG, "Factory reset: %s.", esp_err_to_name(err));
	return err;
}
