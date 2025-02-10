#ifndef __M_DOOR_API_H_
#define __M_DOOR_API_H_

#include "esp_err.h"
#include "d_config.h"
esp_err_t door_init(void);
esp_err_t door_deinit(void);
esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen);
esp_err_t door_change_pass(uint8_t * pass, uint32_t passLen);
void door_reset(void);

esp_err_t get_input_state(input_mode_t * state);
esp_err_t get_input_mode(input_mode_t * state);
esp_err_t set_input_mode(input_mode_t * state);
#endif