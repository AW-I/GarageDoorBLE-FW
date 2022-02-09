#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"

#include "m_door_api.h"


static const char* TAG = "Door Api";
static const uint8_t DEF_PASS[] = {'B','i','g','c','h','0','n','k'};

esp_err_t door_init(void){
    ESP_LOGI(TAG, "Door initialized.");
    return ESP_OK;
}

esp_err_t door_deinit(void){
    ESP_LOGI(TAG, "Door de-initialized.");
    return ESP_OK;
}

esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen){
    if(passLen == sizeof(DEF_PASS)) return ESP_FAIL;
    if(memcmp(DEF_PASS,pass,sizeof(DEF_PASS))) return ESP_FAIL;
    //Open door here.
    ESP_LOGI(TAG, "Opening door.");
    return ESP_OK;
}