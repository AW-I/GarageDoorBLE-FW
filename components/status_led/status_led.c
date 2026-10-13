#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "status_led.h"

static const char *TAG = "STATUS_LED";

#define TICK_US (50 * 1000)
#define FLASH_US (120 * 1000)

/* A pattern repeats every `slots` ticks (50 ms each); bit n of `on` lights the LED in slot n. */
typedef struct {
	uint8_t slots;
	uint32_t on;
} pattern_t;

static const pattern_t s_patterns[STATUS_LED_MODE_MAX] = {
	[STATUS_LED_OFF] = {1, 0x0},
	[STATUS_LED_BLINK] = {10, 0x3E0},       /* 250 ms off, 250 ms on */
	[STATUS_LED_DOUBLE_BLINK] = {24, 0x33}, /* 100 on, 100 off, 100 on, 900 off */
	[STATUS_LED_ON] = {1, 0x1},
};

static int s_gpio = -1;
static bool s_active_low;
static volatile status_led_mode_t s_mode = STATUS_LED_OFF;
static volatile int64_t s_flash_until_us;
static esp_timer_handle_t s_timer;

static void tick(void *arg) {
	int64_t now = esp_timer_get_time();
	const pattern_t *p = &s_patterns[s_mode];
	bool on = (p->on >> ((now / TICK_US) % p->slots)) & 1;
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
	if (mode < STATUS_LED_MODE_MAX) {
		s_mode = mode;
	}
}

void status_led_flash(void) {
	s_flash_until_us = esp_timer_get_time() + FLASH_US;
}
