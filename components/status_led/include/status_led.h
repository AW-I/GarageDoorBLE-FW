#ifndef __STATUS_LED_H_
#define __STATUS_LED_H_

#include <stdbool.h>
#include "esp_err.h"

/* Patterns are defined in status_led.c - add new ones there. */
typedef enum {
	STATUS_LED_OFF,
	STATUS_LED_BLINK,        /* Even 2 Hz blink, e.g. searching for Wi-Fi. */
	STATUS_LED_DOUBLE_BLINK, /* Two short flashes, then a pause (1.2 s cycle). */
	STATUS_LED_ON,
	STATUS_LED_MODE_MAX,
} status_led_mode_t;

/**
 * @brief Drive a single plain LED (not addressable) on gpio.
 * @param gpio GPIO number, negative disables the LED entirely.
 * @param active_low True when the LED lights with the pin low.
 */
esp_err_t status_led_init(int gpio, bool active_low);

void status_led_set_mode(status_led_mode_t mode);

/** @brief Briefly invert the LED as activity feedback. Safe from any task. */
void status_led_flash(void);

#endif
