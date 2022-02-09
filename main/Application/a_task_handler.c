#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"
#include "a_door_task.h"
#include "esp_log.h"

static const char* TAG = "Task Handler";

void task_init(void)
{
#if DOOR_TASK
    door_task_init();
#endif
    ESP_LOGI(TAG, "User Applications Started.");
}