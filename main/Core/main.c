#include "a_task_handler.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* Private functions */
esp_err_t app_setup(void);

/* Entry Point */
void app_main(void)
{
    app_setup();
    task_init();
    return;
}

/* Base Drivers */
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