#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "m_door_api.h"

#define DOOR_GPIO 33
#define DOOR_DELAY 100

static const char* TAG = "Door Api";
static const uint8_t DEF_PASS[] = {'B','i','g','c','h','0','n','k'};

void door_idle();
void door_open();

esp_err_t door_init(void){
    ESP_LOGI(TAG, "Door initialized.");
    gpio_reset_pin(DOOR_GPIO);
    gpio_set_direction(DOOR_GPIO, GPIO_MODE_OUTPUT);
    door_idle();
    return ESP_OK;
}

esp_err_t door_deinit(void){
    ESP_LOGI(TAG, "Door de-initialized.");
    return ESP_OK;
}

esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen){
    if(memcmp(DEF_PASS,pass,sizeof(DEF_PASS))) return ESP_FAIL;
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