#include "a_task_handler.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char* TAG = "Start";

//Private functions
esp_err_t app_setup(void);


//Entry point.
void app_main(void)
{
    //Setup universal drivers.
    app_setup();

    //Run app tasks.
    vTaskInit();
    ESP_LOGD(TAG, "Application tasks started.");
    return;
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