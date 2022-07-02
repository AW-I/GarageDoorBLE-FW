#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"
#include "a_door_task.h"
#include "a_watchdog_task.h"

void task_init(void)
{
#if DOOR_TASK
    door_task_init();
#endif
#if WATCHDOG_TASK
    watchdog_task_init();
#endif
}