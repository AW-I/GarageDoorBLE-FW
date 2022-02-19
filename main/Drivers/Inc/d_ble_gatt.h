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

    GDP_IDX_DATA_NOTIFY_CHAR,
    GDP_IDX_DATA_NTY_VAL,
    GDP_IDX_DATA_NTF_CFG,
    GDP_IDX_NB,
} ble_db_enum;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;

///GDP Service - data receive characteristic, read&write without response
static const uint16_t gdp_data_receive_uuid = ESP_GATT_UUID_GDP_DATA_RECEIVE;
static const uint8_t  gdp_data_receive_val[20] = {0x00};

///GDP Service - data notify characteristic, notify&read
static const uint16_t gdp_data_notify_uuid = ESP_GATT_UUID_GDP_DATA_NOTIFY;
static const uint8_t  gdp_data_notify_val[20] = {0x00};
static const uint8_t  gdp_data_notify_ccc[2] = {0x00, 0x00};

///Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t gdp_gatt_db[GDP_IDX_NB] =
{
    //GDP -  Service Declaration
    [GDP_IDX_SVC]                      	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    sizeof(gdp_service_uuid), sizeof(gdp_service_uuid), (uint8_t *)&gdp_service_uuid}},

    //GDP -  data receive characteristic Declaration
    [GDP_IDX_DATA_RECV_CHAR]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    //GDP -  data receive characteristic Value
    [GDP_IDX_DATA_RECV_VAL]             	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&gdp_data_receive_uuid, ESP_GATT_PERM_WRITE,
    GDP_DATA_MAX_LEN,sizeof(gdp_data_notify_val), (uint8_t *)gdp_data_notify_val}},

    //GDP -  data notify characteristic Declaration
    [GDP_IDX_DATA_NOTIFY_CHAR]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

    //GDP -  data notify characteristic Value
    [GDP_IDX_DATA_NTY_VAL]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&gdp_data_notify_uuid, ESP_GATT_PERM_READ,
    GDP_DATA_MAX_LEN, sizeof(gdp_data_notify_val), (uint8_t *)gdp_data_notify_val}},

    //GDP -  data notify characteristic - Client Characteristic Configuration Descriptor
    [GDP_IDX_DATA_NTF_CFG]         =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(uint16_t),sizeof(gdp_data_notify_ccc), (uint8_t *)gdp_data_notify_ccc}},
};

/** Functions **/
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#endif