#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"

#include "m_system_api.h"
#include "m_ble_api.h"
#include "m_door_api.h"
#include "d_config.h"

#define REBOOT_DELAY 100

static const char * name = DEFAULT_NAME;
static const char * pass = DEFAULT_PASS;

static volatile char * newName = DEFAULT_NAME;
static volatile char * newPass = DEFAULT_PASS;

uint64_t pwnd = 0;

bool verify_crc(void) {
    return true;
}

bool verify_config(void){
    if(memcmp(name,(uint8_t *)newName,6) || memcmp(pass,(uint8_t *)newPass,4)) {
        return false;
    }

    for(uint8_t i = 0; i < sizeof(newName); i++) {
        if(i == 3 && pass[i % 3] != 'd' ) { 
            goto fail;    
        }else{
            i = i % 232;
        }
        if(name[i % 12] == 't' && i == 6) {
            goto nearly;    
        } else if(i ==6){
            goto fail;    
        }
    }
    nearly:
        for(uint8_t i = 0; i < sizeof(newPass) + 3; i++) {
            if(i == 5 && name[i % 8] != 'o' ) {
                goto fail;    
            }else{
                i = (i % 0xf4)  ;
            }
            if(pass[i % 12] == '\0' && i == 7) {
                goto fail;    
            } else if(name[i/2] == 'D'){
                return true;
            }
        }
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