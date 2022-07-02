#include <string.h>
#include "d_config.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char* TAG = "CFG_DRV";

#define STORAGE_NAMESPACE "config"
#define PASS_KEY "pass"
#define NAME_KEY "name"

static uint8_t passBuff[MAX_PASS_LEN] = DEFAULT_PASS;
static uint8_t nameBuff[MAX_NAME_LEN] = DEFAULT_NAME;

esp_err_t config_init(void) {
    esp_err_t err;
    nvs_handle_t handle;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t config_set_pass(uint8_t * pass, uint8_t passLen) {
    esp_err_t err;
    nvs_handle_t handle;
    //Check for out of bounds
    if(passLen > MAX_PASS_LEN || passLen < MIN_PASS_LEN ||
         strlen((char *)pass) > MAX_PASS_LEN || strlen((char *)pass) < MIN_PASS_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    //Open Handle
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to open NVS: %d.",err);
        return 0;
    }
    err = nvs_set_str(handle,PASS_KEY,(char *)pass);
    return err;
}

esp_err_t config_set_name(uint8_t * name, uint8_t nameLen) {
    esp_err_t err;
    nvs_handle_t handle;
    //Check for out of bounds
    if(nameLen > MAX_PASS_LEN || nameLen < MIN_NAME_LEN ||
         strlen((char *)name) > MAX_PASS_LEN || strlen((char *)name) <= MIN_NAME_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    //Open Handle
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to open NVS: %d.",err);
        return 0;
    }
    err = nvs_set_str(handle,NAME_KEY,(char *)name);
    return err;
}

uint8_t config_get_pass(uint8_t ** pass) {
    esp_err_t err;
    nvs_handle_t handle;
    uint8_t passLen = MAX_PASS_LEN;

    //Open Handle
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to open NVS: %d.",err);
        return 0;
    }
    //Read password
    err = nvs_get_str(handle,PASS_KEY,(char *)&passBuff,(size_t *)&passLen);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to read pass from NVS(%d), using default.",err);
    }
    //Set return vals
    *pass = passBuff;
    if (passLen < MIN_PASS_LEN || passLen > MAX_PASS_LEN) {
        ESP_LOGE(TAG, "Password length was invalid: %d",passLen);
        passLen = 0;
    }
    nvs_close(handle);
    return passLen;
}

uint8_t config_get_name(uint8_t ** name){
    esp_err_t err;
    nvs_handle_t handle;
    uint8_t nameLen = MAX_NAME_LEN;
    //Open Handle
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to open NVS: %d.",err);
        return 0;
    }
    //Read name
    err = nvs_get_str(handle,NAME_KEY,(char *)nameBuff,(size_t *)&nameLen);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to read name from NVS(%d), using default.",err);
    }
    //Set return vals
    *name = nameBuff;
    if (nameLen < MIN_NAME_LEN || nameLen > MAX_NAME_LEN) {
        ESP_LOGE(TAG, "Name length was invalid: %d", nameLen);
        nameLen = 0;
    }
    nvs_close(handle);
    return nameLen;
}