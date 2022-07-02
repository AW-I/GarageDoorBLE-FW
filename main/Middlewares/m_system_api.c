#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m_system_api.h"
#include "m_ble_api.h"
#include "m_door_api.h"
#include "esp_system.h"

#define REBOOT_DELAY 100

void system_reboot(void){
    door_deinit();
    ble_deinit();
    vTaskDelay(REBOOT_DELAY);
    esp_restart();
    while(1);
}