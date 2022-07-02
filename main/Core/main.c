#include "a_task_handler.h"
#include "nvs_flash.h"
#include "esp_log.h"

/* Private functions */
esp_err_t app_setup(void);
void log_init(void);

/* Entry Point */
void app_main(void)
{
    log_init();
    app_setup();
    task_init();
    return;
}

/* Base Drivers */
void log_init(void){
    esp_log_level_t logLevel = ESP_LOG_INFO;
    //Task
    esp_log_level_set("DOOR_TASK",logLevel);
    esp_log_level_set("WTG_TASK",logLevel);

    //API
    esp_log_level_set("BLE_API",logLevel);
    esp_log_level_set("DOOR_API",logLevel);

    //Driver
    esp_log_level_set("BLE_DRV",logLevel);
    esp_log_level_set("BLE_GAP",logLevel);
    esp_log_level_set("BLE_GATT",logLevel);
    esp_log_level_set("CFG_DRV",logLevel);
}

esp_err_t app_setup(void) {
    esp_err_t ret;
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    return ret;
}