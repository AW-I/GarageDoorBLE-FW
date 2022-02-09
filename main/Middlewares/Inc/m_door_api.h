#include "esp_err.h"

esp_err_t door_init(void);
esp_err_t door_deinit(void);
esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen);