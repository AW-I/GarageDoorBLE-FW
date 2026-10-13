#ifndef __MOTOR_H_
#define __MOTOR_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "motor_proto.h"

typedef struct {
	int uart_port;
	int tx_gpio;
	int rx_gpio;
	int baud_rate;
	uint32_t tx_gap_ms;   /* Minimum spacing between frames sent to the motor. */
	uint8_t tx_retries;   /* Resends when no echo arrives (0 = send once). */
	uint32_t echo_timeout_ms;
} motor_config_t;

typedef struct {
	bool valid;           /* False until the first status frame is received. */
	uint8_t position;     /* 0 = closed .. 100 = fully open. */
	motor_op_t op;
	int8_t led;           /* 1 on, 0 off, -1 unknown. */
	int64_t updated_us;   /* esp_timer time of last status frame. */
} motor_state_t;

/* Called from the motor task whenever the decoded state changes. Keep it short. */
typedef void (*motor_state_cb_t)(const motor_state_t *state, void *ctx);

esp_err_t motor_init(const motor_config_t *cfg);

/** @brief Queue a command for the motor (non-blocking). */
esp_err_t motor_send(motor_cmd_t cmd);

void motor_get_state(motor_state_t *out);

esp_err_t motor_add_listener(motor_state_cb_t cb, void *ctx);

/** @brief True while the door is reported opening or closing. */
bool motor_is_moving(void);

#endif
