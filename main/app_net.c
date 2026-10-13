#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"

#include "app_net.h"
#include "app_door.h"
#include "app_supervisor.h"
#include "net_wifi.h"
#include "ha_mqtt.h"
#include "settings.h"
#include "status_led.h"
#include "version.h"

static const char *TAG = "NET";

/* Wi-Fi and a broker are both configured, so Home Assistant is expected to be up. */
static volatile bool s_mqtt_enabled;

void app_net_node_id(char *out, size_t out_len) {
	uint8_t mac[6] = {0};
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	snprintf(out, out_len, "gdc_%02x%02x%02x", mac[3], mac[4], mac[5]);
}

/* DHCP hostname from the device name: lowercase alnum and '-'. */
static void make_hostname(const char *name, char *out, size_t out_len) {
	size_t o = 0;
	for (; *name && o + 1 < out_len; name++) {
		char c = (char)tolower((unsigned char)*name);
		if (isalnum((unsigned char)c)) {
			out[o++] = c;
		} else if (o > 0 && out[o - 1] != '-') {
			out[o++] = '-';
		}
	}
	while (o > 0 && out[o - 1] == '-') {
		o--;
	}
	out[o] = '\0';
	if (o == 0) {
		strlcpy(out, "garage-door", out_len);
	}
}

/* LED: off = no Wi-Fi configured, blinking = searching / reconnecting, double blink = Wi-Fi up
 * but the MQTT broker is not, solid = everything configured is connected.
 * Called from the Wi-Fi and MQTT tasks; it recomputes from current state, so order does not matter. */
static void update_led(void) {
	switch (net_wifi_get_state()) {
	case NET_WIFI_CONNECTED:
		status_led_set_mode(s_mqtt_enabled && !ha_mqtt_is_connected() ? STATUS_LED_DOUBLE_BLINK
																	   : STATUS_LED_ON);
		break;
	case NET_WIFI_CONNECTING:
		status_led_set_mode(STATUS_LED_BLINK);
		break;
	default:
		status_led_set_mode(STATUS_LED_OFF);
		break;
	}
}

static void on_wifi_state(net_wifi_state_t state) {
	update_led();
	app_door_refresh_status();
}

static void on_mqtt_connection(bool connected) {
	update_led();
	app_door_refresh_status();
}

static void on_ha_command(ha_cmd_t cmd) {
	switch (cmd) {
	case HA_CMD_OPEN:
		app_door_action(DOOR_ACT_OPEN, "HA");
		break;
	case HA_CMD_CLOSE:
		app_door_action(DOOR_ACT_CLOSE, "HA");
		break;
	case HA_CMD_STOP:
		app_door_action(DOOR_ACT_STOP, "HA");
		break;
	case HA_CMD_LED_ON:
		app_door_action(DOOR_ACT_LED_ON, "HA");
		break;
	case HA_CMD_LED_OFF:
		app_door_action(DOOR_ACT_LED_OFF, "HA");
		break;
	case HA_CMD_RESTART:
		/* Runs in the MQTT task - must not stop the client from here. */
		app_supervisor_request_restart();
		break;
	}
}

esp_err_t app_net_apply(void) {
	char ssid[SETTINGS_WIFI_SSID_MAX + 1];
	char pass[SETTINGS_WIFI_PASS_MAX + 1];
	char name[SETTINGS_NAME_MAX + 1];
	char host[SETTINGS_NAME_MAX + 1];
	settings_get_str(SETTING_WIFI_SSID, ssid, sizeof(ssid));
	settings_get_str(SETTING_WIFI_PASS, pass, sizeof(pass));
	settings_get_str(SETTING_DEVICE_NAME, name, sizeof(name));
	make_hostname(name, host, sizeof(host));

	esp_err_t err = net_wifi_apply(ssid, pass, host);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Wi-Fi apply failed: %s.", esp_err_to_name(err));
	}

	char uri[SETTINGS_MQTT_URI_MAX + 1];
	char user[SETTINGS_MQTT_USER_MAX + 1];
	char mpass[SETTINGS_MQTT_PASS_MAX + 1];
	char node[16];
	settings_get_str(SETTING_MQTT_URI, uri, sizeof(uri));
	settings_get_str(SETTING_MQTT_USER, user, sizeof(user));
	settings_get_str(SETTING_MQTT_PASS, mpass, sizeof(mpass));
	app_net_node_id(node, sizeof(node));
	s_mqtt_enabled = ssid[0] && uri[0];

	const ha_mqtt_config_t hcfg = {
		.uri = ssid[0] ? uri : "", /* No Wi-Fi, no broker. */
		.username = user,
		.password = mpass,
		.device_name = name,
		.node_id = node,
		.sw_version = VERSION,
		.discovery_prefix = CONFIG_GDC_HA_DISCOVERY_PREFIX,
		.base_topic = CONFIG_GDC_HA_BASE_TOPIC,
		.on_command = on_ha_command,
		.on_connection = on_mqtt_connection,
	};
	esp_err_t merr = ha_mqtt_start(&hcfg);
	if (merr != ESP_OK) {
		ESP_LOGE(TAG, "MQTT start failed: %s.", esp_err_to_name(merr));
	}
	update_led();
	app_door_refresh_status();
	return err != ESP_OK ? err : merr;
}

esp_err_t app_net_init(void) {
	esp_err_t err = net_wifi_init(on_wifi_state);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Wi-Fi init failed: %s.", esp_err_to_name(err));
		return err;
	}
	return app_net_apply();
}
