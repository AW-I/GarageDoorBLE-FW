#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_task_handler.h"
#include "esp_log.h"

static const char* TAG = "vTaskInit";


void vTaskInit(void)
{

#if MAIN_TASK

#endif

#if BLE_TASK

#endif
    ESP_LOGI(TAG, "User Applications Started.");
}