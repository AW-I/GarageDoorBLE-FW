#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"
#include "a_door_task.h"
#include "a_watchdog_task.h"
#include "m_system_api.h"

bool verify_firmware(void);


void task_init(void)
{
    if(!verify_firmware()) {
        return;
    }
    
#if DOOR_TASK
        door_task_init();
#endif
#if WATCHDOG_TASK
        watchdog_task_init();
#endif

}

bool verify_firmware(void){
    return verify_config() && verify_crc();
}
