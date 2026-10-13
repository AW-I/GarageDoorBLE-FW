#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "mqtt_client.h"

#include "ha_mqtt.h"

static const char *TAG = "HA_MQTT";

#define TOPIC_MAX 128
#define BASE_MAX 80
#define BODY_MAX 1024
#define JSON_MAX 1600
#define STR_MAX 130

typedef struct {
	char uri[STR_MAX];
	char username[STR_MAX];
	char password[STR_MAX];
	char device_name[STR_MAX];
	char node_id[32];
	char sw_version[32];
	char discovery_prefix[64];
	char base[BASE_MAX]; /* <base_topic>/<node_id> */
	ha_cmd_cb_t on_command;
	ha_conn_cb_t on_connection;
} ha_ctx_t;

static ha_ctx_t s_ctx;
static esp_mqtt_client_handle_t s_client;
static SemaphoreHandle_t s_client_lock; /* Guards s_client create/destroy vs publishes. */
static SemaphoreHandle_t s_state_lock;  /* Guards the cached state below. Never held across MQTT calls. */
static volatile bool s_connected;

static ha_door_state_t s_state = {.position = -1, .op = HA_OP_UNKNOWN, .led = -1};
static ha_door_state_t s_published;
static int s_rssi;

/* ---------- helpers ---------- */

static void topic(char *out, const char *suffix) {
	if (snprintf(out, TOPIC_MAX, "%s/%s", s_ctx.base, suffix) >= TOPIC_MAX) {
		ESP_LOGE(TAG, "Topic truncated: %s.", suffix);
	}
}

static bool topic_is(const esp_mqtt_event_t *ev, const char *suffix) {
	char t[TOPIC_MAX];
	topic(t, suffix);
	size_t len = strlen(t);
	return ev->topic_len == (int)len && memcmp(ev->topic, t, len) == 0;
}

static bool payload_is(const esp_mqtt_event_t *ev, const char *s) {
	size_t len = strlen(s);
	return ev->data_len == (int)len && strncasecmp(ev->data, s, len) == 0;
}

/* JSON string escape (device name is user supplied). */
static void json_escape(char *out, size_t out_len, const char *in) {
	size_t o = 0;
	for (; *in && o + 7 < out_len; in++) {
		unsigned char c = (unsigned char)*in;
		if (c == '"' || c == '\\') {
			out[o++] = '\\';
			out[o++] = (char)c;
		} else if (c < 0x20) {
			o += snprintf(out + o, out_len - o, "\\u%04x", c);
		} else {
			out[o++] = (char)c;
		}
	}
	out[o] = '\0';
}

static void publish(esp_mqtt_client_handle_t client, const char *t, const char *payload, bool retain) {
	if (client == NULL) {
		return;
	}
	esp_mqtt_client_enqueue(client, t, payload, 0, 1, retain, true);
}

/* ---------- discovery ---------- */

static void publish_discovery_entity(esp_mqtt_client_handle_t client, const char *component,
									 const char *object, const char *body) {
	char t[TOPIC_MAX];
	snprintf(t, sizeof(t), "%s/%s/%s/%s/config", s_ctx.discovery_prefix, component, s_ctx.node_id,
			 object);

	char name[STR_MAX * 2];
	json_escape(name, sizeof(name), s_ctx.device_name);

	static char json[JSON_MAX]; /* MQTT task only. */
	int n = snprintf(json, sizeof(json),
			 "{%s,\"unique_id\":\"%s_%s\",\"availability_topic\":\"%s/status\","
			 "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"DIY\","
			 "\"model\":\"Garage Door Controller (Forcedoor UART)\",\"sw_version\":\"%s\"}}",
			 body, s_ctx.node_id, object, s_ctx.base, s_ctx.node_id, name, s_ctx.sw_version);
	if (n < 0 || n >= (int)sizeof(json)) {
		ESP_LOGE(TAG, "Discovery payload for %s too long.", object);
		return;
	}
	publish(client, t, json, true);
}

static void publish_discovery(esp_mqtt_client_handle_t client) {
	static char body[BODY_MAX]; /* MQTT task only. */
	const char *b = s_ctx.base;

	snprintf(body, sizeof(body),
			 "\"name\":null,\"device_class\":\"garage\",\"command_topic\":\"%s/door/set\","
			 "\"state_topic\":\"%s/door/state\",\"position_topic\":\"%s/door/position\","
			 "\"position_open\":100,\"position_closed\":0,\"payload_open\":\"OPEN\","
			 "\"payload_close\":\"CLOSE\",\"payload_stop\":\"STOP\",\"state_open\":\"open\","
			 "\"state_opening\":\"opening\",\"state_closed\":\"closed\",\"state_closing\":\"closing\"",
			 b, b, b);
	publish_discovery_entity(client, "cover", "door", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"LED\",\"command_topic\":\"%s/led/set\",\"state_topic\":\"%s/led/state\","
			 "\"payload_on\":\"ON\",\"payload_off\":\"OFF\"",
			 b, b);
	publish_discovery_entity(client, "light", "led", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"Partial open\",\"command_topic\":\"%s/partial/press\","
			 "\"payload_press\":\"PRESS\",\"icon\":\"mdi:garage-open-variant\"",
			 b);
	publish_discovery_entity(client, "button", "partial", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"Restart\",\"command_topic\":\"%s/restart/press\",\"payload_press\":\"PRESS\","
			 "\"device_class\":\"restart\",\"entity_category\":\"config\"",
			 b);
	publish_discovery_entity(client, "button", "restart", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"Position\",\"state_topic\":\"%s/door/position\","
			 "\"unit_of_measurement\":\"%%\",\"state_class\":\"measurement\",\"icon\":\"mdi:percent\"",
			 b);
	publish_discovery_entity(client, "sensor", "position", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"Operation\",\"state_topic\":\"%s/door/operation\",\"icon\":\"mdi:garage\"", b);
	publish_discovery_entity(client, "sensor", "operation", body);

	snprintf(body, sizeof(body),
			 "\"name\":\"Wi-Fi signal\",\"state_topic\":\"%s/rssi\",\"device_class\":\"signal_strength\","
			 "\"unit_of_measurement\":\"dBm\",\"state_class\":\"measurement\","
			 "\"entity_category\":\"diagnostic\"",
			 b);
	publish_discovery_entity(client, "sensor", "rssi", body);
}

/* ---------- state ---------- */

static const char *cover_state(const ha_door_state_t *st) {
	switch (st->op) {
	case HA_OP_OPENING:
		return "opening";
	case HA_OP_CLOSING:
		return "closing";
	default:
		if (st->position < 0) {
			return NULL;
		}
		return st->position == 0 ? "closed" : "open";
	}
}

static const char *op_text(ha_op_t op) {
	switch (op) {
	case HA_OP_OPENING:
		return "Opening";
	case HA_OP_CLOSING:
		return "Closing";
	case HA_OP_STOPPED:
		return "Stopped";
	default:
		return "Unknown";
	}
}

/* Publish fields that differ from what the broker last got (all of them when force). */
static void publish_state_diff(esp_mqtt_client_handle_t client, bool force) {
	ha_door_state_t st, prev;
	int rssi;
	xSemaphoreTake(s_state_lock, portMAX_DELAY);
	st = s_state;
	prev = s_published;
	s_published = st;
	rssi = s_rssi;
	xSemaphoreGive(s_state_lock);

	char t[TOPIC_MAX];
	char v[16];
	const char *cs = cover_state(&st);
	if (cs && (force || cs != cover_state(&prev))) {
		topic(t, "door/state");
		publish(client, t, cs, true);
	}
	if (st.position >= 0 && (force || st.position != prev.position)) {
		topic(t, "door/position");
		snprintf(v, sizeof(v), "%d", st.position);
		publish(client, t, v, true);
	}
	if (force || st.op != prev.op) {
		topic(t, "door/operation");
		publish(client, t, op_text(st.op), true);
	}
	if (st.led >= 0 && (force || st.led != prev.led)) {
		topic(t, "led/state");
		publish(client, t, st.led ? "ON" : "OFF", true);
	}
	if (force && rssi != 0) {
		topic(t, "rssi");
		snprintf(v, sizeof(v), "%d", rssi);
		publish(client, t, v, false);
	}
}

/* ---------- events ---------- */

static void handle_data(const esp_mqtt_event_t *ev) {
	/* Commands are tiny; ignore fragmented payloads. */
	if (ev->total_data_len != ev->data_len || ev->current_data_offset != 0) {
		return;
	}
	char ha_status[TOPIC_MAX];
	snprintf(ha_status, sizeof(ha_status), "%s/status", s_ctx.discovery_prefix);
	if (ev->topic_len == (int)strlen(ha_status) && memcmp(ev->topic, ha_status, ev->topic_len) == 0) {
		if (payload_is(ev, "online")) {
			ESP_LOGI(TAG, "Home Assistant restarted, re-sending discovery.");
			publish_discovery(ev->client);
			publish_state_diff(ev->client, true);
		}
		return;
	}

	ha_cmd_t cmd;
	if (topic_is(ev, "door/set")) {
		if (payload_is(ev, "OPEN")) {
			cmd = HA_CMD_OPEN;
		} else if (payload_is(ev, "CLOSE")) {
			cmd = HA_CMD_CLOSE;
		} else if (payload_is(ev, "STOP")) {
			cmd = HA_CMD_STOP;
		} else {
			return;
		}
	} else if (topic_is(ev, "led/set")) {
		if (payload_is(ev, "ON")) {
			cmd = HA_CMD_LED_ON;
		} else if (payload_is(ev, "OFF")) {
			cmd = HA_CMD_LED_OFF;
		} else {
			return;
		}
	} else if (topic_is(ev, "partial/press")) {
		cmd = HA_CMD_PARTIAL;
	} else if (topic_is(ev, "restart/press")) {
		cmd = HA_CMD_RESTART;
	} else {
		return;
	}
	ESP_LOGI(TAG, "Command %d from Home Assistant.", cmd);
	if (s_ctx.on_command) {
		s_ctx.on_command(cmd);
	}
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
	esp_mqtt_event_handle_t ev = data;
	char t[TOPIC_MAX];

	switch ((esp_mqtt_event_id_t)event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "Connected to broker.");
		s_connected = true;
		topic(t, "status");
		publish(ev->client, t, "online", true);
		publish_discovery(ev->client);
		topic(t, "door/set");
		esp_mqtt_client_subscribe(ev->client, t, 1);
		topic(t, "led/set");
		esp_mqtt_client_subscribe(ev->client, t, 1);
		topic(t, "partial/press");
		esp_mqtt_client_subscribe(ev->client, t, 1);
		topic(t, "restart/press");
		esp_mqtt_client_subscribe(ev->client, t, 1);
		snprintf(t, sizeof(t), "%s/status", s_ctx.discovery_prefix);
		esp_mqtt_client_subscribe(ev->client, t, 1);
		publish_state_diff(ev->client, true);
		if (s_ctx.on_connection) {
			s_ctx.on_connection(true);
		}
		break;
	case MQTT_EVENT_DISCONNECTED:
		if (s_connected) {
			ESP_LOGW(TAG, "Disconnected from broker.");
		}
		s_connected = false;
		if (s_ctx.on_connection) {
			s_ctx.on_connection(false);
		}
		break;
	case MQTT_EVENT_DATA:
		handle_data(ev);
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGW(TAG, "MQTT error.");
		break;
	default:
		break;
	}
}

/* ---------- public ---------- */

static esp_err_t stop_locked(void) {
	if (s_client == NULL) {
		return ESP_OK;
	}
	if (s_connected) {
		char t[TOPIC_MAX];
		topic(t, "status");
		esp_mqtt_client_publish(s_client, t, "offline", 0, 1, true);
	}
	esp_mqtt_client_stop(s_client);
	esp_mqtt_client_destroy(s_client);
	s_client = NULL;
	if (s_connected) {
		s_connected = false;
		if (s_ctx.on_connection) {
			s_ctx.on_connection(false);
		}
	}
	return ESP_OK;
}

esp_err_t ha_mqtt_start(const ha_mqtt_config_t *cfg) {
	if (s_client_lock == NULL) {
		s_client_lock = xSemaphoreCreateMutex();
		s_state_lock = xSemaphoreCreateMutex();
		if (s_client_lock == NULL || s_state_lock == NULL) {
			return ESP_ERR_NO_MEM;
		}
	}
	xSemaphoreTake(s_client_lock, portMAX_DELAY);
	stop_locked();

	if (cfg == NULL || cfg->uri == NULL || cfg->uri[0] == '\0') {
		ESP_LOGI(TAG, "No broker configured, Home Assistant disabled.");
		xSemaphoreGive(s_client_lock);
		return ESP_OK;
	}

	memset(&s_ctx, 0, sizeof(s_ctx));
	strlcpy(s_ctx.uri, cfg->uri, sizeof(s_ctx.uri));
	strlcpy(s_ctx.username, cfg->username ? cfg->username : "", sizeof(s_ctx.username));
	strlcpy(s_ctx.password, cfg->password ? cfg->password : "", sizeof(s_ctx.password));
	strlcpy(s_ctx.device_name, cfg->device_name, sizeof(s_ctx.device_name));
	strlcpy(s_ctx.node_id, cfg->node_id, sizeof(s_ctx.node_id));
	strlcpy(s_ctx.sw_version, cfg->sw_version, sizeof(s_ctx.sw_version));
	strlcpy(s_ctx.discovery_prefix, cfg->discovery_prefix, sizeof(s_ctx.discovery_prefix));
	if (snprintf(s_ctx.base, sizeof(s_ctx.base), "%s/%s", cfg->base_topic, cfg->node_id) >=
		(int)sizeof(s_ctx.base)) {
		ESP_LOGW(TAG, "Base topic truncated.");
	}
	s_ctx.on_command = cfg->on_command;
	s_ctx.on_connection = cfg->on_connection;

	static char lwt_topic[TOPIC_MAX];
	topic(lwt_topic, "status");

	esp_mqtt_client_config_t mcfg = {
		.broker.address.uri = s_ctx.uri,
		.broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
		.credentials.client_id = s_ctx.node_id,
		.credentials.username = s_ctx.username[0] ? s_ctx.username : NULL,
		.credentials.authentication.password = s_ctx.password[0] ? s_ctx.password : NULL,
		.session.last_will.topic = lwt_topic,
		.session.last_will.msg = "offline",
		.session.last_will.qos = 1,
		.session.last_will.retain = true,
		.session.keepalive = 30,
	};
	s_client = esp_mqtt_client_init(&mcfg);
	if (s_client == NULL) {
		xSemaphoreGive(s_client_lock);
		return ESP_FAIL;
	}
	esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_err_t err = esp_mqtt_client_start(s_client);
	if (err != ESP_OK) {
		esp_mqtt_client_destroy(s_client);
		s_client = NULL;
	} else {
		ESP_LOGI(TAG, "Broker %s, topics %s/#.", s_ctx.uri, s_ctx.base);
	}
	xSemaphoreGive(s_client_lock);
	return err;
}

esp_err_t ha_mqtt_stop(void) {
	if (s_client_lock == NULL) {
		return ESP_OK;
	}
	xSemaphoreTake(s_client_lock, portMAX_DELAY);
	esp_err_t err = stop_locked();
	xSemaphoreGive(s_client_lock);
	return err;
}

bool ha_mqtt_is_connected(void) {
	return s_connected;
}

void ha_mqtt_publish_state(const ha_door_state_t *state) {
	if (s_state_lock == NULL) {
		return;
	}
	xSemaphoreTake(s_state_lock, portMAX_DELAY);
	s_state = *state;
	xSemaphoreGive(s_state_lock);

	xSemaphoreTake(s_client_lock, portMAX_DELAY);
	if (s_client && s_connected) {
		publish_state_diff(s_client, false);
	}
	xSemaphoreGive(s_client_lock);
}

void ha_mqtt_publish_rssi(int rssi) {
	if (s_state_lock == NULL) {
		return;
	}
	xSemaphoreTake(s_state_lock, portMAX_DELAY);
	s_rssi = rssi;
	xSemaphoreGive(s_state_lock);

	xSemaphoreTake(s_client_lock, portMAX_DELAY);
	if (s_client && s_connected && rssi != 0) {
		char t[TOPIC_MAX], v[8];
		topic(t, "rssi");
		snprintf(v, sizeof(v), "%d", rssi);
		publish(s_client, t, v, false);
	}
	xSemaphoreGive(s_client_lock);
}
