#include "d_ble_gap.h"
#include "esp_log.h"

static const char* TAG = "BLE_GAP";

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;
    ESP_LOGD(TAG, "event: %d\n", event);
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    	    esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&ble_adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising start failed: %s\n", esp_err_to_name(err));
            }
            break;
        default:
            break;
    }
}