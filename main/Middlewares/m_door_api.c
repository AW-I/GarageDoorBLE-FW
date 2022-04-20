#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "m_door_api.h"
#include "d_config.h"

#define DOOR_GPIO 33
#define DOOR_DELAY 100

static const char* TAG = "Door Api";

void door_idle();
void door_open();

esp_err_t door_init(void){
    esp_err_t err;
    //Config init
    err = config_init();
    if(err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to init config.");
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
    ESP_LOGI(TAG, "Door de-initialized.");
    return ESP_OK;
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
    if(memcmp(passBuff,pass,sizeof(passBuffLen))) return ESP_FAIL;
    //Open door here.
    ESP_LOGI(TAG, "Opening door.");
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