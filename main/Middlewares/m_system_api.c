#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "spi_flash_mmap.h"
#include "rom/crc.h"
#include "string.h"

#include "m_system_api.h"
#include "m_ble_api.h"
#include "m_door_api.h"
#include "d_config.h"

static const char* TAG = "SYS_API";
#define FLASH_ADDRESS_START 0x00010000
#define FW_SIZE 0xAFFF4
#define REBOOT_DELAY 100
#define MAGIC 38269420

static const char * name = DEFAULT_NAME;
static const char * pass = DEFAULT_PASS;    

static volatile char * newName = DEFAULT_NAME;
static volatile char * newPass = DEFAULT_PASS;

uint64_t pwnd = 0;

bool verify_crc(void) {
    esp_err_t err;
    uint8_t * dataPtr = NULL;
    spi_flash_mmap_handle_t handle = 0;
    //Map the flash
    err =  spi_flash_mmap(FLASH_ADDRESS_START, FW_SIZE, SPI_FLASH_MMAP_DATA,(const void **) &dataPtr, &handle);
    if(err != ESP_OK) {
        ESP_LOGD(TAG, "Failed map memory: %s.",esp_err_to_name(err));
        goto exit;
    }
    //Calculate CRC
    uint32_t crc = 0;
    crc = crc32_le(crc, dataPtr, FW_SIZE);
    crc = crc % MAGIC;
    //Compare.
    dataPtr += FW_SIZE;
    uint32_t expected = (uint32_t)*((uint32_t *)dataPtr);
    ESP_LOGD(TAG, "CRC: %08x, Exp: %08x,Addr: %08x",(unsigned int)crc,(unsigned int)expected, (unsigned int)dataPtr);
    if(!memcmp(&expected,&crc,sizeof(uint32_t))) {
        spi_flash_munmap(handle);
        return true;
    }
    //Don't modify my firmware...
    exit:
        ESP_LOGD(TAG, "CRC Failed!");
        spi_flash_munmap(handle);
        pwnd = 0xdeadbeef;
        return false;
}

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

void system_reboot(void){
    door_deinit();
    ble_deinit();
    vTaskDelay(REBOOT_DELAY);
    esp_restart();
    while(1);
}