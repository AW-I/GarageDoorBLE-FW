#include <string.h>
#include "esp_log.h"
#include "m_ble_api.h"
#include "d_ble_queue.h"
#include "d_ble.h"
#include "d_config.h"

static const char* TAG = "BLE Api";
static volatile bool drvInitialized = false;

esp_err_t ble_init(void) {
    esp_err_t ret = ESP_OK;
    if (drvInitialized == false) {
        ret = ble_drv_init();
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "BLE failed to start with code: %d.",ret);
            return ret;
        } 
        ret = ble_queue_init();
            if(ret != ESP_OK) {
            ESP_LOGE(TAG, "BLE queue failed to init with code: %d.",ret);
            return ret;
        } 
        drvInitialized = true;
        ESP_LOGI(TAG, "BLE initialized.");
    } else {
        ESP_LOGI(TAG, "BLE already initialized.");

    }
    return ret;
}

esp_err_t ble_deinit(void) {
    ESP_LOGI(TAG, "BLE de-initialized.");
    return ESP_OK;
}

uint32_t ble_receive(uint8_t * rxBuffer, uint32_t maxDataSize, uint32_t timeout) {
    return ble_queue_rcv(rxBuffer,maxDataSize,timeout);
}

esp_err_t ble_send(uint8_t * txBuffer, uint32_t txDataSize) {
    return ble_queue_send_push(txBuffer,txDataSize);
}

esp_err_t ble_set_ap_name(uint8_t * rxBuffer, uint32_t rxSize) {
    //Size checks
    if(!rxSize ) {
        ESP_LOGE(TAG, "RX buffer was empty");
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t namesize = strlen((char *)rxBuffer);
    if(!namesize || namesize > MAX_NAME_LEN) {
        ESP_LOGE(TAG, "Calculated namesize invalid: %d.",rxSize);
        return ESP_ERR_INVALID_ARG;
    }
    return config_set_name(rxBuffer,namesize);
}

uint32_t ble_get_connected_duration() {
    if(ble_drv_is_connected()) {
        return ble_drv_get_connection_duration();
    }
    return 0;
}