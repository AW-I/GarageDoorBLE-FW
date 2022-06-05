#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "a_watchdog_task.h"
#include "m_ble_api.h"

#define TASK_NAME "DoorTask"
#define HEAP_SIZE 2048
#define REBOOT_DELAY 100
#define POLL_TIME 1 * 100 // 1 Second

//Static functions
void reboot();
void watchdog_task(void * arg);

//Variables
static const char* TAG = "Watchdog";

void watchdog_task_init(void) {
    ble_init();
    xTaskCreate(watchdog_task, TASK_NAME, HEAP_SIZE, NULL, 4 , NULL);
}

void watchdog_task(void * arg)
{
    ESP_LOGI(TAG, "Watchdog started.");
    for(;;){
        uint32_t upTime = xTaskGetTickCount();
        uint32_t connectionDuration = ble_get_connected_duration();
        //Reboot if connected longer than allowed
        if(connectionDuration > 0) {
            if(connectionDuration > CONNECT_TIMEOUT){
                ESP_LOGI(TAG, "Device timeout reboot.");
                reboot();
            }
        //Reboot if not connected but upTime longer than timeout
        } else if(upTime > REBOOT_FREQUENCY) { 
            ESP_LOGI(TAG, "Periodic reboot.");
            reboot();
        }
        vTaskDelay(POLL_TIME);
    }
    vTaskDelete(NULL);
}

void reboot() {

    while(1);
}
