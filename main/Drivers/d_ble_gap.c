#include <string.h>
#include "esp_log.h"

#include "d_ble_gap.h"

static const char* TAG = "BLE_GAP";
#define ADV_CONFIG_FLAG                           (1 << 0)
#define SCAN_RSP_CONFIG_FLAG                      (1 << 1)

static uint8_t adv_config_done = 0;

static void show_bonded_devices(void);

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0){
            ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT");
            esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&ble_adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0){
            ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT"); 
            esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&ble_adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed, error status = %x", param->adv_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Advertising started.");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        // Send positive security request.
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_KEY_EVT:
        //Shows the ble key info share with peer device to the user.
        ESP_LOGD(TAG, "key type = %d", param->ble_security.ble_key.key_type);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "Device Connected: %02x:%02x:%02x:%02x:%02x:%02x.",
			bd_addr[0],
			bd_addr[1],
			bd_addr[2],
			bd_addr[3],
			bd_addr[4],
			bd_addr[5]);
        ESP_LOGD(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGD(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "Success" : "Fail");
        if(!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(TAG, "Pair failed reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        } else {
            ESP_LOGD(TAG, "Auth mode = %d",param->ble_security.auth_cmpl.auth_mode);
        }
        show_bonded_devices();
        break;
    }
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d", param->remove_bond_dev_cmpl.status);
        ESP_LOGD(TAG, "-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
        ESP_LOG_BUFFER_HEX(TAG, (void *)param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGD(TAG, "------------------------------------");
        break;
    }
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Config local privacy failed, status = %x", param->local_privacy_cmpl.status);
            break;
        }

        esp_err_t ret = esp_ble_gap_config_adv_data((esp_ble_adv_data_t *)&ble_adv_data);
        if (ret){
            ESP_LOGE(TAG, "Config adv data failed, error code = %s", esp_err_to_name(ret));
        }else{
            adv_config_done |= ADV_CONFIG_FLAG;
        }

        ret = esp_ble_gap_config_adv_data((esp_ble_adv_data_t *) &ble_scan_rsp_config);
        if (ret){
            ESP_LOGE(TAG, "Config adv data failed, error code = %s", esp_err_to_name(ret));
        }else{
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
        }

        break;
    default:
        ESP_LOGD(TAG, "Unhandled event: %d", event);
        break;
    }
}

static void show_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();
    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    ESP_LOGD(TAG, "Bonded devices total : %d", dev_num);
    for (int i = 0; i < dev_num; i++) {
        ESP_LOGD(TAG, "Device %d: %08x%04x",i,\
            (dev_list[i].bd_addr[0] << 24) + (dev_list[i].bd_addr[1] << 16)+ (dev_list[i].bd_addr[2] << 8) + dev_list[i].bd_addr[3],
            (dev_list[i].bd_addr[4] << 8) + dev_list[i].bd_addr[5]);
    }
    free(dev_list);
}