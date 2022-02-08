#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"

void vTaskInit(void)
{

#if MAIN_TASK

#endif

#if BLE_TASK

#endif

    vTaskDelete(NULL);
}