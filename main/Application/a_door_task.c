#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "a_door_task.h"
#include "m_ble_api.h"
#include "m_door_api.h"

#define TASK_NAME "DoorTask"
#define HEAP_SIZE 2048

void door_task(void * arg);

uint8_t rxBuffer[64] = {0};

void door_task_init(void) {
    ble_init();
    door_init();
    xTaskCreate(door_task, TASK_NAME, HEAP_SIZE, NULL, 4 , NULL);
}

void door_task(void * arg)
{
    for(;;){
        
        //Receive message from ble
        uint32_t rxSize = ble_receive(rxBuffer,sizeof(rxBuffer),portMAX_DELAY);
        if(rxSize > 0 ) {
            //Send to the door api if its password.
            door_toggle_open(rxBuffer,rxSize);
        }
        //Sleep
        vTaskDelay(50);
    }
    vTaskDelete(NULL);
}