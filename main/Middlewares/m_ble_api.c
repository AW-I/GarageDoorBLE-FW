#include "esp_log.h"
#include "m_ble_api.h"
#include "d_ble.h"

static const char* TAG = "BLE Api";

esp_err_t ble_init(void) {
    esp_err_t ret = ble_drv_init();
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "BLE failed to start with: %d.",ret);
        return ret;
    } 
    //Free RTOS queues for receive and transmit.

    ESP_LOGI(TAG, "BLE initialized.");
    return ret;
}

esp_err_t ble_deinit(void) {
    ESP_LOGI(TAG, "BLE de-initialized.");
    return ESP_OK;
}

esp_err_t ble_receive(uint8_t * rxBuffer, uint32_t maxDataSize, uint32_t timeout) {
    ESP_LOGI(TAG, "BLE rx size: %d ", 0);
    return ESP_OK;
}

esp_err_t ble_send(uint8_t * txBuffer, uint32_t txDataSize, uint32_t timeout) {
    ESP_LOGI(TAG, "BLE tx size: %d ", txDataSize);
    return ESP_OK;
}