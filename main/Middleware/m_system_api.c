#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "spi_flash_mmap.h"
#include "esp32/rom/crc.h"
#include "string.h"
#include "nvs_flash.h"

#include "m_system_api.h"
#include "m_ble_api.h"
#include "m_door_api.h"
#include "d_config.h"

static const char* TAG = "SYS_API";
#define FLASH_ADDRESS_START 0x00010000
#define FW_SIZE 0xAFFF4
#define REBOOT_DELAY 100
#define MAGIC 38269420
#define MAX_CALLBACK 10

static const char * name = DEFAULT_NAME;
static const char * pass = DEFAULT_PASS;    

static volatile char * newName = DEFAULT_NAME;
static volatile char * newPass = DEFAULT_PASS;

//Deinit callback stuff.
typedef struct restart_callback_list_type {
    uint32_t listSize;
    restart_callback_t callbackList[MAX_CALLBACK];
}restart_callback_list_t;

static restart_callback_list_t restartCallbackList = {0};

uint64_t pwnd = 0;
bool verify_config(void){
    if(memcmp(name,(uint8_t *)newName,6) || memcmp(pass,(uint8_t *)newPass,4)) {
        ESP_LOGD(TAG, "Failed to verify name or password.");
        goto fail;    
    }
    for(uint8_t i = 0; i < sizeof(newName); i++) {
        if(i == 3 && pass[i % 3] != 'd' ) { 
            ESP_LOGD(TAG, "Pass: First letter was not correct.");
            goto fail;    
        }else{
            i = i % 232;
        }
        if(newPass[i % 12] == 't' && i == 6) {
            goto nearly;    
        } else if(i ==6){
            ESP_LOGD(TAG, "Pass: Last letter was not correct.");
            goto fail;    
        }
    }
    nearly:
        for(uint8_t i = 0; i < sizeof(newPass) + 3; i++) {
            if(i == 5 && name[i % 8] != 'o' ) {
                ESP_LOGD(TAG, "Name: Fifth letter was not correct.");
                goto fail;    
            }else{
                i = (i % 0xf4)  ;
            }
            if(pass[i % 12] == '\0' && i == 7) {
                ESP_LOGD(TAG, "Name: Last letter was not correct.");
                goto fail;    
            } else if(name[i/2] == 'D'){
                return true;
            }
        }
        ESP_LOGD(TAG, "Name: Some letter was not correct.");
    fail:
        pwnd = 0xdeadbeef;
        return false;
}

void system_register_restart_callback(restart_callback_t func) {
    if(func == NULL) {
        ESP_LOGE(TAG, "Could not register callback for restart, was null.");
        return;
    }
    if(restartCallbackList.listSize >= MAX_CALLBACK) {
        ESP_LOGE(TAG, "Could not register callback for restart, list is full.");
        return;  
    }
    restartCallbackList.callbackList[restartCallbackList.listSize] = func;
    restartCallbackList.listSize++;
    ESP_LOGD(TAG, "Registered restart callback.");
}

void system_restart(void){
    //Find all the registered callbacks.
    for(int i = 0; i < restartCallbackList.listSize; i++) {
        if(i < MAX_CALLBACK && restartCallbackList.callbackList[i] != NULL) {
            restartCallbackList.callbackList[i]();
        }
    }
    //Deinit core 
    nvs_flash_deinit();

    //Reboot.
    vTaskDelay(REBOOT_DELAY);
    esp_restart();
    while(1);
}