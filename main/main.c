/*
 * Garage Door Controller
 *
 *  Phone app --BLE (NimBLE)--> GDP service --> command processor --+
 *  Home Assistant --MQTT/Wi-Fi--> ha_mqtt -------------------------+--> door service --UART--> Forcedoor motor
 *
 * BLE stays available when Wi-Fi / Home Assistant is down and is also how Wi-Fi,
 * MQTT, device name and the door password are configured.
 */
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_ble_cmd.h"
#include "app_door.h"
#include "app_net.h"
#include "app_supervisor.h"
#include "ble_gdp.h"
#include "settings.h"
#include "version.h"

static const char *TAG = "MAIN";

static void log_init(void) {
	esp_log_level_t level = ESP_LOG_INFO;
	esp_log_level_set("MAIN", level);
	esp_log_level_set("DOOR", level);
	esp_log_level_set("NET", level);
	esp_log_level_set("BLE_CMD", level);
	esp_log_level_set("SUPERVISOR", level);
	esp_log_level_set("SETTINGS", level);
	esp_log_level_set("MOTOR", level);
	esp_log_level_set("BLE_GDP", level);
	esp_log_level_set("NET_WIFI", level);
	esp_log_level_set("HA_MQTT", level);
}

void app_main(void) {
	log_init();
	ESP_LOGI(TAG, "Garage Door Controller %s.", VERSION);

	const settings_defaults_t defaults = {
		.device_name = CONFIG_GDC_DEFAULT_DEVICE_NAME,
		.door_pass = CONFIG_GDC_DEFAULT_PASSWORD,
	};
	ESP_ERROR_CHECK(settings_init(&defaults));

	/* BLE first: it is the recovery path if anything below fails. */
	char name[SETTINGS_NAME_MAX + 1];
	settings_get_str(SETTING_DEVICE_NAME, name, sizeof(name));
	const ble_gdp_config_t ble_cfg = {
		.device_name = name,
		.on_connection = app_ble_cmd_on_connection,
	};
	ESP_ERROR_CHECK(ble_gdp_init(&ble_cfg));
	ESP_ERROR_CHECK(app_ble_cmd_init());

	if (app_door_init() != ESP_OK) {
		ESP_LOGE(TAG, "Motor link failed to start.");
	}
	if (app_net_init() != ESP_OK) {
		ESP_LOGE(TAG, "Network failed to start - BLE only.");
	}
	ESP_ERROR_CHECK(app_supervisor_init());
}
