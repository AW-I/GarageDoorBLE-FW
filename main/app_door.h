#ifndef __APP_DOOR_H_
#define __APP_DOOR_H_

#include "esp_err.h"

/* Values are the action byte of the BLE DOOR command - do not renumber. */
typedef enum {
	DOOR_ACT_OPEN = 0,
	DOOR_ACT_STOP = 1,
	DOOR_ACT_CLOSE = 2,
	/* 3 was partial open - removed, rejected as invalid. */
	DOOR_ACT_LED_ON = 4,
	DOOR_ACT_LED_OFF = 5,
	DOOR_ACT_TOGGLE = 6, /* Wall-button behaviour: stop if moving, else open/close. */
	DOOR_ACT_MAX
} door_action_t;

/** @brief Start the motor link and route its state to BLE + Home Assistant. */
esp_err_t app_door_init(void);

esp_err_t app_door_action(door_action_t action, const char *source);

/** @brief Recompute the BLE state characteristic (call after network changes). */
void app_door_refresh_status(void);

#endif
