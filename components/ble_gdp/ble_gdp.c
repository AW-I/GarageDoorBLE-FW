#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_bt.h"
#include "sdkconfig.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_gdp.h"

static const char *TAG = "BLE_GDP";

#define RX_QUEUE_LEN 4
#define RESP_WAIT_MS 2000
#define ADV_INTERVAL 0x40 /* 40 ms, same as V1 */
#define APPEARANCE_GENERIC_REMOTE 0x0180

void ble_store_config_init(void);

/* Bluedroid stored 128-bit UUIDs LSB first - BLE_UUID128_INIT uses the same order. */
#define GDP_UUID(b12) \
	BLE_UUID128_INIT(0x1E, 0x94, 0x8D, 0xF1, 0x48, 0x31, 0x94, 0xBA, 0x75, 0x4C, 0x3E, 0x50, b12, 0x00, 0x3D, 0x71)

static const ble_uuid128_t s_svc_uuid = GDP_UUID(0x00);
static const ble_uuid128_t s_state_uuid = GDP_UUID(0x02);
static const ble_uuid128_t s_rx_uuid = GDP_UUID(0x03);
static const ble_uuid128_t s_ext_state_uuid = GDP_UUID(0x04);

/* Service data carried in both adv and scan response by V1 ("AW" uuid16 + 'I'). */
static const uint8_t s_svc_data[] = {'A', 'W', 'I'};

typedef struct {
	uint16_t len;
	uint8_t data[BLE_GDP_RX_MAX];
} rx_msg_t;

static ble_gdp_config_t s_cfg;
static char s_name[CONFIG_BT_NIMBLE_GAP_DEVICE_NAME_MAX_LEN + 1];
static uint8_t s_own_addr_type;
static volatile bool s_synced;

static volatile uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static volatile int64_t s_last_activity_us;

static uint16_t s_rx_handle, s_state_handle, s_ext_state_handle;
static QueueHandle_t s_rx_queue;

static SemaphoreHandle_t s_resp_lock;
static SemaphoreHandle_t s_resp_ready;
static volatile bool s_resp_pending;
static uint8_t s_resp[BLE_GDP_RESP_MAX];
static size_t s_resp_len;

static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_state;
static uint8_t s_ext_state[BLE_GDP_EXT_STATE_LEN] = {0xFF, 0xFF, 0xFF, 0x00};

static int gap_event(struct ble_gap_event *event, void *arg);

/* ---------- GATT ---------- */

static int rx_access(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt) {
	if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
		static rx_msg_t msg; /* Host task only - keep it off the stack. */
		uint16_t len = 0;
		if (OS_MBUF_PKTLEN(ctxt->om) > BLE_GDP_RX_MAX) {
			return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
		}
		if (ble_hs_mbuf_to_flat(ctxt->om, msg.data, sizeof(msg.data), &len) != 0) {
			return BLE_ATT_ERR_UNLIKELY;
		}
		msg.len = len;
		s_last_activity_us = esp_timer_get_time();

		xSemaphoreTake(s_resp_lock, portMAX_DELAY);
		s_resp_len = 0;
		s_resp_pending = true;
		xSemaphoreTake(s_resp_ready, 0);
		xSemaphoreGive(s_resp_lock);

		if (xQueueSend(s_rx_queue, &msg, 0) != pdTRUE) {
			ESP_LOGE(TAG, "RX queue full, command dropped.");
			s_resp_pending = false;
			return BLE_ATT_ERR_INSUFFICIENT_RES;
		}
		ESP_LOGD(TAG, "RX %u bytes.", len);
		return 0;
	}

	if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
		/* The phone writes a command then reads the reply - give the command task time. */
		if (s_resp_pending) {
			xSemaphoreTake(s_resp_ready, pdMS_TO_TICKS(RESP_WAIT_MS));
		}
		xSemaphoreTake(s_resp_lock, portMAX_DELAY);
		int rc = os_mbuf_append(ctxt->om, s_resp, s_resp_len);
		xSemaphoreGive(s_resp_lock);
		if (s_resp_len == 0) {
			ESP_LOGW(TAG, "No response for this request.");
		}
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}
	return BLE_ATT_ERR_UNLIKELY;
}

static int chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
					  void *arg) {
	if (attr_handle == s_rx_handle) {
		return rx_access(conn_handle, ctxt);
	}
	if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
		return BLE_ATT_ERR_UNLIKELY;
	}
	uint8_t buf[BLE_GDP_EXT_STATE_LEN];
	size_t len;
	taskENTER_CRITICAL(&s_state_mux);
	if (attr_handle == s_state_handle) {
		buf[0] = s_state;
		len = 1;
	} else {
		memcpy(buf, s_ext_state, sizeof(buf));
		len = sizeof(buf);
	}
	taskEXIT_CRITICAL(&s_state_mux);
	return os_mbuf_append(ctxt->om, buf, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
	{
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = &s_svc_uuid.u,
		.characteristics =
			(struct ble_gatt_chr_def[]){
				{
					.uuid = &s_rx_uuid.u,
					.access_cb = chr_access,
					.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
					.val_handle = &s_rx_handle,
				},
				{
					.uuid = &s_state_uuid.u,
					.access_cb = chr_access,
					.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
					.val_handle = &s_state_handle,
				},
				{
					.uuid = &s_ext_state_uuid.u,
					.access_cb = chr_access,
					.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
					.val_handle = &s_ext_state_handle,
				},
				{0},
			},
	},
	{0},
};

/* ---------- GAP ---------- */

static void advertise(void) {
	if (!s_synced || s_conn_handle != BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active()) {
		return;
	}

	struct ble_hs_adv_fields fields = {0};
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
	fields.appearance = APPEARANCE_GENERIC_REMOTE;
	fields.appearance_is_present = 1;
	fields.uuids128 = &s_svc_uuid;
	fields.num_uuids128 = 1;
	fields.uuids128_is_complete = 1;
	fields.svc_data_uuid16 = s_svc_data;
	fields.svc_data_uuid16_len = sizeof(s_svc_data);
	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGE(TAG, "Set adv data failed: %d.", rc);
		return;
	}

	struct ble_hs_adv_fields rsp = {0};
	const char *name = ble_svc_gap_device_name();
	rsp.name = (const uint8_t *)name;
	rsp.name_len = strlen(name);
	rsp.name_is_complete = 1;
	rsp.svc_data_uuid16 = s_svc_data;
	rsp.svc_data_uuid16_len = sizeof(s_svc_data);
	rc = ble_gap_adv_rsp_set_fields(&rsp);
	if (rc != 0) {
		ESP_LOGE(TAG, "Set scan response failed: %d.", rc);
		return;
	}

	struct ble_gap_adv_params params = {0};
	params.conn_mode = BLE_GAP_CONN_MODE_UND;
	params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	params.itvl_min = ADV_INTERVAL;
	params.itvl_max = ADV_INTERVAL;
	rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
	if (rc != 0 && rc != BLE_HS_EALREADY) {
		ESP_LOGE(TAG, "Advertising start failed: %d.", rc);
		return;
	}
	ESP_LOGI(TAG, "Advertising as \"%s\".", name);
}

static int gap_event(struct ble_gap_event *event, void *arg) {
	struct ble_gap_conn_desc desc;

	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status != 0) {
			ESP_LOGW(TAG, "Connect failed: %d.", event->connect.status);
			advertise();
			return 0;
		}
		s_conn_handle = event->connect.conn_handle;
		s_last_activity_us = esp_timer_get_time();
		if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
			const uint8_t *a = desc.peer_id_addr.val;
			ESP_LOGI(TAG, "Device connected: %02x:%02x:%02x:%02x:%02x:%02x.", a[5], a[4], a[3],
					 a[2], a[1], a[0]);
		}
		/* V1 requested encryption straight away - keep that. */
		ble_gap_security_initiate(event->connect.conn_handle);
		if (s_cfg.on_connection) {
			s_cfg.on_connection(true);
		}
		return 0;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Device disconnected (reason 0x%x).", event->disconnect.reason);
		s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
		if (s_cfg.on_connection) {
			s_cfg.on_connection(false);
		}
		advertise();
		return 0;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		advertise();
		return 0;

	case BLE_GAP_EVENT_ENC_CHANGE:
		if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
			ESP_LOGI(TAG, "Encryption %s (bonded=%d).",
					 event->enc_change.status == 0 ? "enabled" : "failed", desc.sec_state.bonded);
		}
		return 0;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
		/* Phone lost its bond (app reinstall / forget device) - drop ours and re-pair. */
		if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
			ble_store_util_delete_peer(&desc.peer_id_addr);
		}
		return BLE_GAP_REPEAT_PAIRING_RETRY;

	case BLE_GAP_EVENT_SUBSCRIBE:
		ESP_LOGD(TAG, "Subscribe attr %d notify=%d.", event->subscribe.attr_handle,
				 event->subscribe.cur_notify);
		return 0;

	case BLE_GAP_EVENT_MTU:
		ESP_LOGD(TAG, "MTU %d.", event->mtu.value);
		return 0;

	default:
		return 0;
	}
}

static void on_sync(void) {
	int rc = ble_hs_util_ensure_addr(0);
	if (rc == 0) {
		rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
	}
	if (rc != 0) {
		ESP_LOGE(TAG, "Address setup failed: %d.", rc);
		return;
	}
	s_synced = true;
	advertise();
}

static void on_reset(int reason) {
	s_synced = false;
	ESP_LOGE(TAG, "Host reset, reason %d.", reason);
}

static void host_task(void *param) {
	nimble_port_run();
	nimble_port_freertos_deinit();
}

/* ---------- Public API ---------- */

esp_err_t ble_gdp_init(const ble_gdp_config_t *cfg) {
	if (cfg == NULL || s_rx_queue != NULL) {
		return ESP_ERR_INVALID_STATE;
	}
	s_cfg = *cfg;
	s_rx_queue = xQueueCreate(RX_QUEUE_LEN, sizeof(rx_msg_t));
	s_resp_lock = xSemaphoreCreateMutex();
	s_resp_ready = xSemaphoreCreateBinary();
	if (!s_rx_queue || !s_resp_lock || !s_resp_ready) {
		return ESP_ERR_NO_MEM;
	}

	esp_err_t err = nimble_port_init();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "NimBLE init failed: %s.", esp_err_to_name(err));
		return err;
	}
	esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

	ble_hs_cfg.reset_cb = on_reset;
	ble_hs_cfg.sync_cb = on_sync;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	/* Same security as V1: bonding + LE secure connections, no IO (Just Works). */
	ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_sc = 1;
	ble_hs_cfg.sm_mitm = 0;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

	ble_svc_gap_init();
	ble_svc_gatt_init();
	int rc = ble_gatts_count_cfg(s_gatt_svcs);
	if (rc == 0) {
		rc = ble_gatts_add_svcs(s_gatt_svcs);
	}
	if (rc != 0) {
		ESP_LOGE(TAG, "GATT registration failed: %d.", rc);
		return ESP_FAIL;
	}
	ble_svc_gap_device_appearance_set(APPEARANCE_GENERIC_REMOTE);
	strlcpy(s_name, cfg->device_name ? cfg->device_name : "BLEDoor", sizeof(s_name));
	ble_svc_gap_device_name_set(s_name);
	s_cfg.device_name = s_name;

	ble_store_config_init();
	nimble_port_freertos_init(host_task);
	ESP_LOGI(TAG, "BLE initialized.");
	return ESP_OK;
}

size_t ble_gdp_receive(uint8_t *buf, size_t max_len, uint32_t timeout_ms) {
	static rx_msg_t msg;
	if (s_rx_queue == NULL || max_len == 0) {
		return 0;
	}
	if (xQueueReceive(s_rx_queue, &msg, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
		return 0;
	}
	size_t len = msg.len < max_len - 1 ? msg.len : max_len - 1;
	memcpy(buf, msg.data, len);
	buf[len] = '\0';
	return len;
}

esp_err_t ble_gdp_set_response(const uint8_t *data, size_t len) {
	if (len > BLE_GDP_RESP_MAX) {
		return ESP_ERR_INVALID_SIZE;
	}
	xSemaphoreTake(s_resp_lock, portMAX_DELAY);
	memcpy(s_resp, data, len);
	s_resp_len = len;
	s_resp_pending = false;
	xSemaphoreGive(s_resp_lock);
	xSemaphoreGive(s_resp_ready);
	return ESP_OK;
}

void ble_gdp_set_state(uint8_t state) {
	bool changed;
	taskENTER_CRITICAL(&s_state_mux);
	changed = s_state != state;
	s_state = state;
	taskEXIT_CRITICAL(&s_state_mux);
	if (changed && s_state_handle) {
		ble_gatts_chr_updated(s_state_handle);
	}
}

void ble_gdp_set_ext_state(const uint8_t state[BLE_GDP_EXT_STATE_LEN]) {
	bool changed;
	taskENTER_CRITICAL(&s_state_mux);
	changed = memcmp(s_ext_state, state, BLE_GDP_EXT_STATE_LEN) != 0;
	memcpy(s_ext_state, state, BLE_GDP_EXT_STATE_LEN);
	taskEXIT_CRITICAL(&s_state_mux);
	if (changed && s_ext_state_handle) {
		ble_gatts_chr_updated(s_ext_state_handle);
	}
}

esp_err_t ble_gdp_set_name(const char *name) {
	if (name == NULL || name[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}
	if (ble_svc_gap_device_name_set(name) != 0) {
		return ESP_FAIL;
	}
	/* Restart advertising so the scan response carries the new name. */
	if (ble_gap_adv_active()) {
		ble_gap_adv_stop();
		advertise();
	}
	return ESP_OK;
}

esp_err_t ble_gdp_disconnect(void) {
	uint16_t handle = s_conn_handle;
	if (handle == BLE_HS_CONN_HANDLE_NONE) {
		return ESP_OK;
	}
	return ble_gap_terminate(handle, BLE_ERR_REM_USER_CONN_TERM) == 0 ? ESP_OK : ESP_FAIL;
}

bool ble_gdp_is_connected(void) {
	return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

uint32_t ble_gdp_idle_ms(void) {
	if (!ble_gdp_is_connected()) {
		return 0;
	}
	return (uint32_t)((esp_timer_get_time() - s_last_activity_us) / 1000);
}
