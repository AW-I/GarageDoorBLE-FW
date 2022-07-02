#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "a_door_task.h"
#include "m_system_api.h"
#include "m_ble_api.h"
#include "m_door_api.h"

#define TASK_NAME "DoorTask"
#define HEAP_SIZE 2048

//Variables
static const char* TAG = "DOOR_TASK";
static const char RESP_OK[] = "OK\r\n";
static const char RESP_ERR[] = "ERROR\r\n";
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
                if(ptr->cmdID == rxBuffer[0] && !pwnd){
                    if(ptr->process_func(rxBuffer + 1 ,rxSize - 1) == ESP_OK){
                        ble_send((uint8_t *)RESP_OK,sizeof(RESP_OK));
                        handled = pdTRUE;
                    } 
                    break;
                }
            }
            //Log the failed command, cya.
            if(handled != pdTRUE) {
                ESP_LOGE(TAG, "Command failed.");
                ble_send((uint8_t *)RESP_ERR,sizeof(RESP_ERR));
                vTaskDelay(150);
                ble_disconnect();
            }
        }
        //Sleep
        vTaskDelay(50);
    }
    vTaskDelete(NULL);
}

/* Callback functions */
esp_err_t handle_open_door(uint8_t * rxBuffer, uint32_t rxSize) {
    esp_err_t ret = ESP_OK;
    ret = door_toggle_open(rxBuffer,rxSize);
    if(ret == ESP_OK) {
        ESP_LOGI(TAG, "Opening door.");
    } else {
        ESP_LOGE(TAG, "Door open request failed.");
    }
    return ret;
}

esp_err_t handle_ap_name(uint8_t * rxBuffer, uint32_t rxSize) {
    esp_err_t ret = ESP_OK;
    ret = ble_set_ap_name(rxBuffer,rxSize);
    if(ret == ESP_OK) {
        ESP_LOGI(TAG, "Changed AP name succesfully.");
    } else {
        ESP_LOGE(TAG, "AP name change request failed.");
    }
    return ret;
}

esp_err_t handle_ap_pass(uint8_t * rxBuffer, uint32_t rxSize) {
    esp_err_t ret = ESP_OK;
    ret = door_change_pass(rxBuffer,rxSize);
    if(ret == ESP_OK) {
        ESP_LOGI(TAG, "Changed door password succesfully.");
    } else if(ret == ESP_ERR_NOT_FINISHED) {
        ESP_LOGI(TAG, "Door password unlocked.");
        ret = ESP_OK;
    } else{
        ESP_LOGE(TAG, "Door password request failed.");
    }
    return ret;
}

esp_err_t handle_reboot(uint8_t * rxBuffer, uint32_t rxSize) {
    ESP_LOGI(TAG, "Got reboot command.");
    system_reboot();
    return ESP_OK;
}