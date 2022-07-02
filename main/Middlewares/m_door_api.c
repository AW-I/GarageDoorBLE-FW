#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "m_door_api.h"
#include "d_config.h"

#define DOOR_GPIO 33
#define DOOR_DELAY 100
#define PASS_TIMEOUT 200

static const char* TAG = "DOOR_API";
static volatile uint8_t passUnlocked = pdFALSE;
static volatile uint32_t passUnlockedTime = 0;

void door_idle();
void door_open();

esp_err_t door_init(void){
    esp_err_t err;
    //Config init
    err = config_init();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init config.");
        return err;
    }
    //Config gpio
    gpio_reset_pin(DOOR_GPIO);
    gpio_set_direction(DOOR_GPIO, GPIO_MODE_OUTPUT);
    door_idle();
    //OK
    ESP_LOGI(TAG, "Door initialized.");
    return ESP_OK;
}

esp_err_t door_deinit(void){
    door_idle();
    ESP_LOGI(TAG, "Door de-initialized.");
    return ESP_OK;
}

esp_err_t door_change_pass(uint8_t * pass, uint32_t passLen) {
    esp_err_t ret = ESP_OK;
    uint8_t * passBuff = NULL;
    uint8_t passBuffLen = 0; 
    uint32_t currTime = xTaskGetTickCount();
    if(!passLen ) {
        ESP_LOGE(TAG, "Password buffer was empty.");
        return ESP_ERR_INVALID_ARG;
    }
    //Unlock password.
    if(passUnlocked == pdFALSE || (currTime - passUnlockedTime) > PASS_TIMEOUT) {
        //Get the password 
        passBuffLen = config_get_pass(&passBuff);
        if(passBuffLen <= 0 || passBuff == NULL) {
            return ESP_FAIL;
        }
        //Compare
        if(memcmp(passBuff,pass,sizeof(passBuffLen))){
            return ESP_FAIL;
        } 
        //Unlock and timestamp.
        passUnlocked = pdTRUE;
        passUnlockedTime = currTime;
        ret = ESP_ERR_NOT_FINISHED;
    } else{
        //Set the password.
        uint32_t passSize = strlen((char *)pass);
        if(!passSize || passSize > MAX_NAME_LEN) {
            ESP_LOGE(TAG, "Calculated pass size invalid: %d.",passSize);
            return ESP_ERR_INVALID_ARG;
        }
        ret = config_set_pass(pass,passSize);
        //Lock the password.
        if(ret == ESP_OK) {
            passUnlocked = pdFALSE;
        }
    }
    return ret;
}

esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen){
    uint8_t * passBuff = NULL;
    uint8_t passBuffLen = 0; 
    //Get the password 
    passBuffLen = config_get_pass(&passBuff);
    if(passBuffLen <= 0 || passBuff == NULL) {
        return ESP_FAIL;
    }
    //Check password
    if(memcmp(passBuff,pass,sizeof(passBuffLen))){
        return ESP_FAIL;
    } 
    //Close connection and then open.
    door_open();
    vTaskDelay(DOOR_DELAY);
    door_idle();
    return ESP_OK;
}

void door_open() {
    gpio_set_level(DOOR_GPIO,1);
}

void door_idle(){
    gpio_set_level(DOOR_GPIO,0);
}