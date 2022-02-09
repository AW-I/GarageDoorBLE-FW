#include "esp_err.h"

esp_err_t ble_init(void);
esp_err_t ble_deinit(void);
esp_err_t ble_receive(uint8_t * rxBuffer, uint32_t maxDataSize, uint32_t timeout);
esp_err_t ble_send(uint8_t * txBuffer, uint32_t txDataSize, uint32_t timeout);