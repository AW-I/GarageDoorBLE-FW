#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "m_ble_api.h"
#include "d_ble_queue.h"
#include "d_ble.h"
#include "d_config.h"

#define PASS_TIMEOUT 200

static const char* TAG = "BLE_API";
static volatile bool drvInitialized = false;
static volatile uint8_t apUnlocked = pdFALSE;
static volatile uint32_t apUnlockedTime = 0;

esp_err_t ble_init(void) {
    esp_err_t ret = ESP_OK;
    if (drvInitialized == false) {
        ret = ble_drv_init();
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "BLE failed to start.");
            return ret;
        } 
        ret = ble_queue_init();
            if(ret != ESP_OK) {
            ESP_LOGE(TAG, "BLE queue failed to init: %s.",esp_err_to_name(ret));
            return ret;
        } 
        drvInitialized = true;
        ESP_LOGI(TAG, "BLE initialized.");
    } else {
        ESP_LOGD(TAG, "BLE already initialized.");
    }
    return ret;
}

esp_err_t ble_deinit(void) {
    ESP_LOGI(TAG, "BLE de-initialized.");
    return ble_drv_deinit();
}

uint32_t ble_receive(uint8_t * rxBuffer, uint32_t maxDataSize, uint32_t timeout) {
    return ble_queue_rcv(rxBuffer,maxDataSize,timeout);
}

esp_err_t ble_send(uint8_t * txBuffer, uint32_t txDataSize) {
    return ble_queue_send_push(txBuffer,txDataSize);
}

esp_err_t ble_set_ap_name(uint8_t * data, uint32_t rxSize) {
    esp_err_t ret = ESP_OK;
    uint8_t * passBuff = NULL;
    uint8_t passBuffLen = 0; 
    uint32_t currTime = xTaskGetTickCount();
    if(!rxSize ) {
        ESP_LOGE(TAG, "Rx Buffer was empty.");
        return ESP_ERR_INVALID_ARG;
    }
    //Unlock password.
    if(apUnlocked == pdFALSE || (currTime - apUnlockedTime) > PASS_TIMEOUT) {
        //Get the password 
        passBuffLen = config_get_pass(&passBuff);
        if(passBuffLen <= 0 || passBuff == NULL) {
            return ESP_FAIL;
        }
        //Compare length & data
        if(rxSize != passBuffLen || memcmp(passBuff,data,passBuffLen)){
            return ESP_FAIL;
        } 
        //Unlock and timestamp.
        apUnlocked = pdTRUE;
        apUnlockedTime = currTime;
        ret = ESP_ERR_NOT_FINISHED;
    } else{
        //Set the password.
        uint32_t apLen = strlen((char *)data);
        if(!apLen || apLen > MAX_NAME_LEN) {
            ESP_LOGE(TAG, "Calculated name size invalid: %d.",apLen);
            return ESP_ERR_INVALID_ARG;
        }
        ret = config_set_name(data,apLen);
        //Lock the password.
        if(ret == ESP_OK) {
            apUnlocked = pdFALSE;
        }
    }
    return ret;
}

esp_err_t ble_disconnect(void) {
    return ble_drv_disconnect();
}

uint32_t ble_get_connected_duration() {
    if(ble_drv_is_connected()) {
        return ble_drv_get_connection_duration();
    }
    return 0;
}