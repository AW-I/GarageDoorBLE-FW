#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"
#include "a_door_task.h"
#include "a_watchdog_task.h"
#include "m_system_api.h"   

void task_init(void){    
        door_task_init();
        watchdog_task_init();

        //Register the deinit functions
        system_register_restart_callback(door_task_deinit);
        system_register_restart_callback(watchdog_task_deinit);
}
