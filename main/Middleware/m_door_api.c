#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "m_door_api.h"

static const char* TAG = "DOOR_API";
static volatile uint8_t passUnlocked = pdFALSE;
static volatile uint32_t passUnlockedTime = 0;

void door_open();

esp_err_t door_init(void){
    esp_err_t err;
    //Config init
    err = config_init();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init config.");
        return err;
    }
    //Config Output
    gpio_reset_pin(GPIO_DOOR_OUTPUT);
    gpio_set_direction(GPIO_DOOR_OUTPUT, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_DOOR_OUTPUT,GPIO_OFF);
    //Config Input
    gpio_reset_pin(GPIO_DOOR_INPUT);
    gpio_set_direction(GPIO_DOOR_INPUT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_DOOR_INPUT,GPIO_PULLUP_ONLY);
    door_reset();
    //OK
    ESP_LOGI(TAG, "Door initialized.");
    return ESP_OK;
}

esp_err_t door_deinit(void){
    door_reset();
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
        //Compare length & data
        if(passLen != passBuffLen || memcmp(passBuff,pass,passBuffLen)){
            return ESP_FAIL;
        } 
        //Unlock and timestamp.
        passUnlocked = pdTRUE;
        passUnlockedTime = currTime;
        ret = ESP_ERR_NOT_FINISHED;
    } else{
        //Set the password.
        uint32_t passSize = strlen((char *)pass);
        if(!passSize || passSize > MAX_PASS_LEN) {
            ESP_LOGE(TAG, "Calculated pass size invalid: %d.",(int)passSize);
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
    //Check password and size
    if(passLen != passBuffLen || memcmp(passBuff,pass,passBuffLen)){
        return ESP_FAIL;
    } 
    //Open
    door_open();
    return ESP_OK;
}

door_state_t door_get_state(void) {
    input_mode_t inputMode;
    int level = gpio_get_level(GPIO_DOOR_INPUT);
    do {
        vTaskDelay(INPUT_DEBOUNCE);
        level = gpio_get_level(GPIO_DOOR_INPUT);
    } while(level != gpio_get_level(GPIO_DOOR_INPUT));
    inputMode = config_get_input_mode();
    return level ^ inputMode;
}

esp_err_t door_set_mode(input_mode_t mode) {
    return config_set_input_mode(mode);
}
input_mode_t door_get_mode(void) {
    return config_get_input_mode();
}

void door_trigger_open(){
    door_open();
    door_reset();
}

void door_reset(void){
    vTaskDelay(OUTPUT_DEBOUNCE);
    gpio_set_level(GPIO_DOOR_OUTPUT,GPIO_OFF);
}

void door_open(void) {
    gpio_set_level(GPIO_DOOR_OUTPUT,GPIO_ON);
}