#ifndef __D_BLE_GATT_H_
#define __D_BLE_GATT_H_

#include "esp_gatts_api.h"
#include "d_ble_const.h"

/** Defines **/
#define gdp_sprintf(s,...)         sprintf((char*)(s), ##__VA_ARGS__)
#define GDP_DATA_MAX_LEN           (512)
#define GDP_DATA_BUFF_MAX_LEN      (2*1024)

/* BLE profile */
typedef enum{
    GDP_IDX_SVC,

    GDP_IDX_DATA_RECV_CHAR,
    GDP_IDX_DATA_RECV_VAL,

    GDP_IDX_NB
} ble_db_enum;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

/// Service UUID 
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;

//Custom Service (GDP)
static const uint8_t gdp_service_uuid[ESP_UUID_LEN_128] = ESP_GATT_UUID_GDP_SERVICE;

///GDP Service - data receive characteristic, write without response
static const uint8_t  gdp_data_receive_uuid[ESP_UUID_LEN_128]  = ESP_GATT_UUID_GDP_DATA_RECEIVE;

///GDP Service - data receive characteristic, write without response
static const uint8_t  gdp_data_send_uuid[ESP_UUID_LEN_128]  = ESP_GATT_UUID_GDP_DATA_SEND;

//Properties
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_READ;

///Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t gdp_gatt_db[GDP_IDX_NB] =
{
    //GDP -  Service Declaration
    [GDP_IDX_SVC]                      	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    sizeof(gdp_service_uuid), sizeof(gdp_service_uuid), (uint8_t *)&gdp_service_uuid}},

    //GDP -  Data receive characteristic Declaration
    [GDP_IDX_DATA_RECV_CHAR]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    //GDP -  Data receive characteristic Value
    [GDP_IDX_DATA_RECV_VAL]                 =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_128, (uint8_t *)&gdp_data_receive_uuid, ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
    GDP_DATA_MAX_LEN,0, NULL}}
};

/** Functions **/
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
esp_err_t gatts_close(bool shutdown);
#endif