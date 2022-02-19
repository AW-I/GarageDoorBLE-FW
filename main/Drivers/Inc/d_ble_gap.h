#ifndef __D_BLE_GAP_H_
#define __D_BLE_GAP_H_

#include "esp_gap_ble_api.h"

/* Advertising Data */
//Raw Data
static const uint8_t ble_adv_data[23] = {
    /* Flags */
    0x02,0x01,0x06,
    /* Complete List of 16-bit Service Class UUIDs */
    0x03,0x03,0xF0,0xAB,
    /* Complete Local Name in advertising */
    0x0F,0x09, 'J', '&', 'W', '_', 'E', 'n', 't', 'e', 'r', 'p', 'r','i', 's', 'e'
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