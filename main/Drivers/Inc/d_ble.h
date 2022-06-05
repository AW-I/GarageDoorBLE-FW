#ifndef __D_BLE_H_
#define __D_BLE_H_

#include "esp_err.h"

esp_err_t ble_drv_init(void);
bool ble_drv_is_connected();
uint32_t ble_drv_get_connection_duration();

#endif