#ifndef __M_DOOR_API_H_
#define __M_DOOR_API_H_

#include "esp_err.h"
#include "d_config.h"

/* Configuration for IO */
#define GPIO_ON             0x01  //Off state
#define GPIO_OFF            0x00  //On state

#define GPIO_DOOR_INPUT     4       //Door status pin
#define GPIO_DOOR_OUTPUT    21      //Door control pin
#define INPUT_DEBOUNCE      50      //Time to debounce input
#define OUTPUT_DEBOUNCE     250     //Time to debounce output (for door)
#define PASS_TIMEOUT        100     //Time between requests to change password.

typedef enum door_state_type {
    DOOR_STATE_OPEN = 0x00,
    DOOR_STATE_CLOSED = 0x01
}door_state_t ;

esp_err_t door_init(void);
esp_err_t door_deinit(void);
esp_err_t door_toggle_open(uint8_t * pass, uint32_t passLen);
esp_err_t door_change_pass(uint8_t * pass, uint32_t passLen);

//Door state
door_state_t door_get_state(void);
esp_err_t door_set_mode(input_mode_t mode);
input_mode_t door_get_mode(void);

void door_trigger_open();
void door_reset(void);

#endif