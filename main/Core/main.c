/* ***************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
#include "a_task_handler.h"
#include "nvs_flash.h"
#include "esp_log.h"

/* Base Drivers */
static void log_init(void){
    esp_log_level_t logLevel = ESP_LOG_DEBUG;
    //Task
    esp_log_level_set("DOOR_TASK",logLevel);
    esp_log_level_set("WTG_TASK",logLevel);

    //API
    esp_log_level_set("BLE_API",logLevel);
    esp_log_level_set("DOOR_API",logLevel);
    esp_log_level_set("SYS_API",logLevel);

    //Driver
    esp_log_level_set("BLE_DRV",logLevel);
    esp_log_level_set("BLE_GAP",logLevel);
    esp_log_level_set("BLE_GATT",logLevel);
    esp_log_level_set("CFG_DRV",logLevel);
}

static esp_err_t app_setup(void) {
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

/* Entry Point */
void app_main(void){
    log_init();
    app_setup();
    task_init();
    return;
}
