#include "esp_log.h"
#include "m_ble_api.h"
#include "d_ble_queue.h"
#include "d_ble.h"

static const char* TAG = "BLE Api";

esp_err_t ble_init(void) {
    esp_err_t ret = ble_drv_init();
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE failed to start with code: %d.",ret);
        return ret;
    } 
    ret = ble_queue_init();
        if(ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE queue failed to init with code: %d.",ret);
        return ret;
    } 
    ESP_LOGI(TAG, "BLE initialized.");
    return ret;
}

esp_err_t ble_deinit(void) {
    ESP_LOGI(TAG, "BLE de-initialized.");
    return ESP_OK;
}

uint32_t ble_receive(uint8_t * rxBuffer, uint32_t maxDataSize, uint32_t timeout) {
    return ble_queue_rcv(rxBuffer,maxDataSize,timeout);
}

esp_err_t ble_send(uint8_t * txBuffer, uint32_t txDataSize, uint32_t timeout) {
    ESP_LOGI(TAG, "BLE tx size: %d ", txDataSize);
    return ESP_OK;
}