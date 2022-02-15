#ifndef __D_BLE_CONST_H_
#define __D_BLE_CONST_h_

#include "esp_gap_ble_api.h"

#define DEVICE_NAME          "J&W_Enterpise"    
#define SPP_SVC_INST_ID	            0

/// Service UUID
static const uint16_t spp_service_uuid = 0xABF0;
/// Characteristic UUID
#define ESP_GATT_UUID_SPP_DATA_RECEIVE      0xABF1
#define ESP_GATT_UUID_SPP_DATA_NOTIFY       0xABF2

/* Advertising Data */
//Raw Data
static const uint8_t spp_adv_data[23] = {
    /* Flags */
    0x02,0x01,0x06,
    /* Complete List of 16-bit Service Class UUIDs */
    0x03,0x03,0xF0,0xAB,
    /* Complete Local Name in advertising */
    0x0F,0x09, 'J', '&', 'W', '_', 'E', 'n', 't', 'e', 'r', 'p', 'r','i', 's', 'e'
};
//Param
static esp_ble_adv_params_t spp_adv_params = {
    .adv_int_min        = 0x120,
    .adv_int_max        = 0x160,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#endif