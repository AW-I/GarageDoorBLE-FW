#ifndef __D_BLE_GAP_H_
#define __D_BLE_GAP_H_

#include "esp_gap_ble_api.h"
#include "d_ble_const.h"

/* Advertising Data */
static const uint8_t name[] = {'J', '&', 'W', '_', 'E', 'n', 't', 'e', 'r', 'p', 'r','i', 's', 'e'};
static const uint8_t custom_service[ESP_UUID_LEN_128] = ESP_GATT_UUID_GDP_SERVICE;

static const esp_ble_adv_data_t ble_adv_data = {
    .set_scan_rsp = false,
    .include_name = false,
    .include_txpower = false,
    .min_interval = 0x0028, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0190, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(custom_service),
    .p_service_uuid = (uint8_t *)custom_service,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
//Param
 static const esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x120,
    .adv_int_max        = 0x160,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

#endif