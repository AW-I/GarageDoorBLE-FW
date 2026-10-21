#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "net_wifi.h"

static const char *TAG = "NET_WIFI";

#define RETRY_MS 60000
/* C3 Super Mini antenna distorts at full power; 8.5 dBm links far better. Units of 0.25 dBm. */
#define WIFI_MAX_TX_POWER 34

static esp_netif_t *s_netif;
static net_wifi_state_cb_t s_cb;
static SemaphoreHandle_t s_lock;
static esp_timer_handle_t s_retry_timer;
static volatile net_wifi_state_t s_state = NET_WIFI_DISABLED;
static volatile bool s_enabled;
static bool s_started;
static int64_t s_down_since_us; /* 0 while connected or disabled. */

static void set_state(net_wifi_state_t state) {
	if (s_state == state) {
		return;
	}
	s_state = state;
	if (state != NET_WIFI_CONNECTING) {
		s_down_since_us = 0;
	} else if (s_down_since_us == 0) {
		s_down_since_us = esp_timer_get_time();
	}
	if (s_cb) {
		s_cb(state);
	}
}

static void retry_cb(void *arg) {
	if (s_enabled) {
		esp_wifi_connect();
	}
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
	if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
		/* Only valid after esp_wifi_start(), and reset on each restart. */
		esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER);
		if (s_enabled) {
			esp_wifi_connect();
		}
	} else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
		if (!s_enabled) {
			return;
		}
		wifi_event_sta_disconnected_t *ev = data;
		ESP_LOGW(TAG, "Disconnected (reason %d), retry in %d s.", ev->reason, RETRY_MS / 1000);
		set_state(NET_WIFI_CONNECTING);
		esp_timer_stop(s_retry_timer);
		esp_timer_start_once(s_retry_timer, (uint64_t)RETRY_MS * 1000);
	} else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *ev = data;
		ESP_LOGI(TAG, "Connected, IP " IPSTR ".", IP2STR(&ev->ip_info.ip));
		set_state(NET_WIFI_CONNECTED);
	}
}

esp_err_t net_wifi_init(net_wifi_state_cb_t cb) {
	s_cb = cb;
	s_lock = xSemaphoreCreateMutex();
	if (s_lock == NULL) {
		return ESP_ERR_NO_MEM;
	}
	ESP_ERROR_CHECK(esp_netif_init());
	esp_err_t err = esp_event_loop_create_default();
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		return err;
	}
	s_netif = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	err = esp_wifi_init(&cfg);
	if (err != ESP_OK) {
		return err;
	}
	/* Credentials live in our own settings store, not the driver's NVS copy. */
	esp_wifi_set_storage(WIFI_STORAGE_RAM);
	esp_wifi_set_mode(WIFI_MODE_STA);

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));

	const esp_timer_create_args_t targs = {.callback = retry_cb, .name = "wifi_retry"};
	return esp_timer_create(&targs, &s_retry_timer);
}

esp_err_t net_wifi_apply(const char *ssid, const char *pass, const char *hostname) {
	if (s_netif == NULL) {
		return ESP_ERR_INVALID_STATE;
	}
	xSemaphoreTake(s_lock, portMAX_DELAY);

	s_enabled = false;
	esp_timer_stop(s_retry_timer);
	if (s_started) {
		esp_wifi_stop();
		s_started = false;
	}

	if (ssid == NULL || ssid[0] == '\0') {
		ESP_LOGI(TAG, "No SSID configured, Wi-Fi off.");
		set_state(NET_WIFI_DISABLED);
		xSemaphoreGive(s_lock);
		return ESP_OK;
	}

	wifi_config_t wcfg = {0};
	strlcpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid));
	if (pass) {
		/* 64 char PSK fills the field exactly, so copy rather than strlcpy. */
		strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password));
	}
	wcfg.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
	wcfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
	wcfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
	wcfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
	wcfg.sta.pmf_cfg.capable = true;

	if (hostname && hostname[0]) {
		esp_netif_set_hostname(s_netif, hostname);
	}
	esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wcfg);
	if (err == ESP_OK) {
		s_enabled = true;
		set_state(NET_WIFI_CONNECTING);
		err = esp_wifi_start();
		s_started = err == ESP_OK;
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Wi-Fi start failed: %s.", esp_err_to_name(err));
		s_enabled = false;
		set_state(NET_WIFI_DISABLED);
	} else {
		ESP_LOGI(TAG, "Connecting to \"%s\".", ssid);
	}
	xSemaphoreGive(s_lock);
	return err;
}

net_wifi_state_t net_wifi_get_state(void) {
	return s_state;
}

uint32_t net_wifi_down_s(void) {
	int64_t since = s_down_since_us;
	return since ? (uint32_t)((esp_timer_get_time() - since) / 1000000) : 0;
}

int8_t net_wifi_rssi(void) {
	wifi_ap_record_t ap;
	if (s_state != NET_WIFI_CONNECTED || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
		return 0;
	}
	return ap.rssi;
}

void net_wifi_ip_str(char *out, size_t out_len) {
	esp_netif_ip_info_t info;
	out[0] = '\0';
	if (s_state == NET_WIFI_CONNECTED && esp_netif_get_ip_info(s_netif, &info) == ESP_OK) {
		snprintf(out, out_len, IPSTR, IP2STR(&info.ip));
	}
}
