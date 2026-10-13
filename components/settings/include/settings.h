#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Maximum string lengths (excluding NUL terminator). */
#define SETTINGS_NAME_MAX      20
#define SETTINGS_PASS_MAX      20
#define SETTINGS_WIFI_SSID_MAX 32
#define SETTINGS_WIFI_PASS_MAX 64
#define SETTINGS_MQTT_URI_MAX  128
#define SETTINGS_MQTT_USER_MAX 64
#define SETTINGS_MQTT_PASS_MAX 64

/* Ids double as the key byte used by the BLE SET/GET commands - do not renumber. */
typedef enum {
	SETTING_WIFI_SSID = 1,
	SETTING_WIFI_PASS = 2,
	SETTING_MQTT_URI = 3,
	SETTING_MQTT_USER = 4,
	SETTING_MQTT_PASS = 5,
	SETTING_DEVICE_NAME = 6,
	SETTING_DOOR_PASS = 7,
	SETTING_MAX
} setting_id_t;

typedef enum {
	SWITCH_NORMALLY_OPEN = 0,
	SWITCH_NORMALLY_CLOSED = 1
} input_mode_t;

typedef struct {
	const char *device_name; /* Used when NVS has no value. */
	const char *door_pass;
} settings_defaults_t;

/**
 * @brief Initialise NVS (erasing on version/space errors) and load all settings into RAM.
 */
esp_err_t settings_init(const settings_defaults_t *defaults);

/**
 * @brief Copy a setting into out (always NUL terminated).
 */
esp_err_t settings_get_str(setting_id_t id, char *out, size_t out_len);

/**
 * @brief Validate and persist a setting. len excludes any NUL terminator.
 */
esp_err_t settings_set_str(setting_id_t id, const char *value, size_t len);

/** @brief True for secrets that must never be read back over BLE. */
bool settings_is_secret(setting_id_t id);

/** @brief Constant-time compare against the stored door password. */
bool settings_check_password(const uint8_t *pass, size_t len);

input_mode_t settings_get_input_mode(void);
esp_err_t settings_set_input_mode(input_mode_t mode);

/** @brief Erase every stored setting (defaults apply after reboot). */
esp_err_t settings_factory_reset(void);

#endif
