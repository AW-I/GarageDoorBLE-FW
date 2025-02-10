#ifndef __D_BLE_GAP_H_
#define __D_BLE_GAP_H_

#include "esp_gap_ble_api.h"
#include "d_ble_const.h"

extern bool is_connected;
extern uint32_t connection_time;

/* Advertising Data */
static const uint8_t custom_service[ESP_UUID_LEN_128] = ESP_GATT_UUID_GDP_SERVICE;
static const esp_ble_adv_data_t ble_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0xffff, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0xffff, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = ESP_BLE_APPEARANCE_GENERIC_REMOTE,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 3,
    .p_service_data = (uint8_t *)"AWI",
    .service_uuid_len = sizeof(custom_service),
    .p_service_uuid = (uint8_t *)custom_service,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
};
// config scan response data
static const esp_ble_adv_data_t ble_scan_rsp_config = {
    .set_scan_rsp = true,
    .include_name = true,
    .service_data_len = 3,
    .p_service_data = (uint8_t *)"AWI",
};

//Param
 static const esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x040,
    .adv_int_max        = 0x040,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_RANDOM,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

#endif