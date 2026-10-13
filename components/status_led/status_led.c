#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "status_led.h"

static const char *TAG = "STATUS_LED";

#define TICK_US (50 * 1000)
#define BLINK_HALF_PERIOD_US (250 * 1000) /* 2 Hz blink */
#define FLASH_US (120 * 1000)

static int s_gpio = -1;
static bool s_active_low;
static volatile status_led_mode_t s_mode = STATUS_LED_OFF;
static volatile int64_t s_flash_until_us;
static esp_timer_handle_t s_timer;

static void tick(void *arg) {
	int64_t now = esp_timer_get_time();
	bool on;
	switch (s_mode) {
	case STATUS_LED_ON:
		on = true;
		break;
	case STATUS_LED_BLINK:
		on = (now / BLINK_HALF_PERIOD_US) & 1;
		break;
	default:
		on = false;
		break;
	}
	/* Activity flash shows as the opposite of the background pattern. */
	if (now < s_flash_until_us) {
		on = !on;
	}
	gpio_set_level(s_gpio, on ^ s_active_low);
}

esp_err_t status_led_init(int gpio, bool active_low) {
	if (gpio < 0) {
		ESP_LOGI(TAG, "Status LED disabled.");
		return ESP_OK;
	}
	s_gpio = gpio;
	s_active_low = active_low;

	gpio_reset_pin(gpio);
	esp_err_t err = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
	if (err != ESP_OK) {
		return err;
	}
	gpio_set_level(gpio, active_low);

	const esp_timer_create_args_t args = {.callback = tick, .name = "status_led"};
	err = esp_timer_create(&args, &s_timer);
	if (err == ESP_OK) {
		err = esp_timer_start_periodic(s_timer, TICK_US);
	}
	ESP_LOGI(TAG, "Status LED on GPIO%d (active %s).", gpio, active_low ? "low" : "high");
	return err;
}

void status_led_set_mode(status_led_mode_t mode) {
	s_mode = mode;
}

void status_led_flash(void) {
	s_flash_until_us = esp_timer_get_time() + FLASH_US;
}
