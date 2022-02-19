#ifndef __D_BLE_CONST_H_
#define __D_BLE_CONST_H_

#define ESP_BLE_APP_ID  0x69
#define DEVICE_NAME          "J&W_Enterpise"    
#define BLE_SVC_INST_ID	            0

/// Service UUID 
static const uint16_t gdp_service_uuid = 0xABF0;
/// Characteristic UUID
#define ESP_GATT_UUID_GDP_DATA_RECEIVE      0xABF1
#define ESP_GATT_UUID_GDP_DATA_NOTIFY       0xABF2

#endif