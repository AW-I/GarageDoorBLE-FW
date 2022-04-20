#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "a_door_task.h"
#include "m_ble_api.h"
#include "m_door_api.h"

#define TASK_NAME "DoorTask"
#define HEAP_SIZE 2048
#define REBOOT_DELAY 100

//Variables
static const char* TAG = "Task Handler";
static const char RESP_OK[] = "OK\r\n";
static const char RESP_ERR[] = "ERR\r\n";
static uint8_t rxBuffer[64] = {0};

//Function definition.
void door_task(void * arg);
esp_err_t handle_open_door(uint8_t * rxBuffer, uint32_t rxSize);
esp_err_t handle_ap_name(uint8_t * rxBuffer, uint32_t rxSize);
esp_err_t handle_ap_pass(uint8_t * rxBuffer, uint32_t rxSize);
esp_err_t handle_reboot(uint8_t * rxBuffer, uint32_t rxSize);

static const cmd_callback_t cmd_callback_list[] = {
		{
				.cmdID = CMD_OPEN,
				.process_func = handle_open_door
		}, {
				.cmdID = CMD_AP_NAME,
				.process_func = handle_ap_name
		}, {
				.cmdID = CMD_AP_PASS,
				.process_func = handle_ap_pass
		}, {
                .cmdID = CMD_REBOOT,
				.process_func = handle_reboot
        }
};

void door_task_init(void) {
    ble_init();
    door_init();
    xTaskCreate(door_task, TASK_NAME, HEAP_SIZE, NULL, 4 , NULL);
}

void door_task(void * arg)
{
    uint8_t cmd_quantity = sizeof(cmd_callback_list)/ sizeof(cmd_callback_t);

    for(;;){
        //Receive message from ble
        uint8_t handled = 0;
        memset(rxBuffer,0,sizeof(rxBuffer));
        uint32_t rxSize = ble_receive(rxBuffer,sizeof(rxBuffer),portMAX_DELAY);
        if(rxSize > 0 ) {
            //Find the command
            for(uint8_t i = 0; i < cmd_quantity; i++){
                cmd_callback_t * ptr = (cmd_callback_t*)&cmd_callback_list[i];
                if (NULL == ptr->process_func) continue;
                if(ptr->cmdID == rxBuffer[0]){
                    handled = pdTRUE;
                    if(ptr->process_func(rxBuffer,sizeof(rxBuffer)) == ESP_OK){
                        ble_send((uint8_t *)RESP_OK,sizeof(RESP_OK));
                    }
                    ble_send((uint8_t *)RESP_ERR,sizeof(RESP_ERR));
                    break;
                }
            }
            //Log the failed command.
            if(handled != pdTRUE) {
                ESP_LOGE(TAG, "Failed to parse command.");
            }
        }
        //Sleep
        vTaskDelay(50);
    }
    vTaskDelete(NULL);
}

esp_err_t handle_open_door(uint8_t * rxBuffer, uint32_t rxSize) {
    ESP_LOGI(TAG, "Got door open command.");
    return door_toggle_open(rxBuffer,rxSize);
}

esp_err_t handle_ap_name(uint8_t * rxBuffer, uint32_t rxSize) {
    ESP_LOGI(TAG, "Got change ap name command.");
    return ESP_OK;
}

esp_err_t handle_ap_pass(uint8_t * rxBuffer, uint32_t rxSize) {
    ESP_LOGI(TAG, "Got change ap pass command.");
    return ESP_OK;
}

esp_err_t handle_reboot(uint8_t * rxBuffer, uint32_t rxSize) {
    ESP_LOGI(TAG, "Got reboot command.");
    vTaskDelay(REBOOT_DELAY);
    esp_restart();
    return ESP_OK;
}