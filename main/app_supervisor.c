#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_supervisor.h"
#include "ble_gdp.h"
#include "ha_mqtt.h"
#include "motor.h"
#include "net_wifi.h"

static const char *TAG = "SUPERVISOR";

#define TASK_NAME "Supervisor"
#define TASK_STACK 3072
#define POLL_MS 1000
#define RSSI_PERIOD_S 60
#define REBOOT_DELAY_MS 300

static volatile bool s_restart_requested;

static void restart_now(void) {
	ESP_LOGW(TAG, "Restarting.");
	ha_mqtt_stop();
	ble_gdp_disconnect();
	vTaskDelay(pdMS_TO_TICKS(REBOOT_DELAY_MS));
	esp_restart();
}

static void supervisor_task(void *arg) {
	if (esp_task_wdt_add(NULL) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add watchdog to task.");
	}
	uint32_t ticks = 0;
	const int64_t reboot_us = (int64_t)CONFIG_GDC_PERIODIC_REBOOT_H * 3600LL * 1000000LL;

	for (;;) {
		esp_task_wdt_reset();

		if (s_restart_requested) {
			restart_now();
		}

		/* Free the single BLE slot if a phone sits connected without sending commands. */
		if (ble_gdp_idle_ms() > CONFIG_GDC_BLE_IDLE_TIMEOUT_S * 1000U) {
			ESP_LOGI(TAG, "BLE client idle, disconnecting.");
			ble_gdp_disconnect();
		}

		if (reboot_us > 0 && esp_timer_get_time() > reboot_us && !ble_gdp_is_connected() &&
			!motor_is_moving()) {
			ESP_LOGI(TAG, "Periodic reboot.");
			restart_now();
		}

		if (++ticks % RSSI_PERIOD_S == 0) {
			ha_mqtt_publish_rssi(net_wifi_rssi());
		}

		vTaskDelay(pdMS_TO_TICKS(POLL_MS));
	}
}

esp_err_t app_supervisor_init(void) {
	if (xTaskCreate(supervisor_task, TASK_NAME, TASK_STACK, NULL, 4, NULL) != pdPASS) {
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

void app_supervisor_request_restart(void) {
	s_restart_requested = true;
}
