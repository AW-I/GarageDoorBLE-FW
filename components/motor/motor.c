#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "motor.h"

static const char *TAG = "MOTOR";

#define TASK_NAME "Motor"
#define TASK_STACK 3072
#define TASK_PRIO 5
#define UART_RX_BUF 512
#define CMD_QUEUE_LEN 8
#define RX_POLL_MS 10
#define MAX_LISTENERS 4

typedef struct {
	motor_state_cb_t cb;
	void *ctx;
} listener_t;

static motor_config_t s_cfg;
static QueueHandle_t s_cmd_queue;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static motor_state_t s_state = {.valid = false, .op = MOTOR_OP_UNKNOWN, .led = -1};
static listener_t s_listeners[MAX_LISTENERS];
static uint8_t s_listener_count;

static uint8_t s_rx_buf[128];
static size_t s_rx_len;

/* Pending command awaiting its echo. */
static struct {
	bool active;
	motor_cmd_t cmd;
	uint8_t retries_left;
	int64_t sent_us;
} s_pending;
static int64_t s_last_tx_us;

static void notify_listeners(const motor_state_t *st) {
	for (uint8_t i = 0; i < s_listener_count; i++) {
		s_listeners[i].cb(st, s_listeners[i].ctx);
	}
}

static void handle_frame(const motor_frame_t *frame) {
	if (frame->type == MOTOR_FRAME_ECHO) {
		ESP_LOGD(TAG, "Echo: %s.", motor_cmd_str((motor_cmd_t)frame->echo_cmd));
		if (s_pending.active && frame->echo_cmd == (uint8_t)s_pending.cmd) {
			s_pending.active = false;
		}
		return;
	}
	if (frame->type != MOTOR_FRAME_STATUS) {
		return;
	}

	motor_state_t st;
	bool changed;
	taskENTER_CRITICAL(&s_state_lock);
	changed = !s_state.valid || s_state.position != frame->position || s_state.op != frame->op ||
			  s_state.led != frame->led;
	s_state.valid = true;
	s_state.position = frame->position;
	s_state.op = frame->op;
	s_state.led = frame->led;
	s_state.updated_us = esp_timer_get_time();
	st = s_state;
	taskEXIT_CRITICAL(&s_state_lock);

	if (changed) {
		ESP_LOGI(TAG, "Status: %s, %u%%, LED %s.", motor_op_str(st.op), st.position,
				 st.led < 0 ? "?" : (st.led ? "on" : "off"));
		notify_listeners(&st);
	}
}

static void process_rx(void) {
	int n = uart_read_bytes(s_cfg.uart_port, s_rx_buf + s_rx_len, sizeof(s_rx_buf) - s_rx_len,
							pdMS_TO_TICKS(RX_POLL_MS));
	if (n <= 0) {
		return;
	}
	s_rx_len += n;

	motor_frame_t frame;
	size_t consumed;
	for (;;) {
		bool ok = motor_proto_parse(s_rx_buf, s_rx_len, &frame, &consumed);
		if (consumed > 0) {
			memmove(s_rx_buf, s_rx_buf + consumed, s_rx_len - consumed);
			s_rx_len -= consumed;
		}
		if (!ok) {
			break;
		}
		handle_frame(&frame);
	}
	/* Buffer full of junk without a frame: drop it. */
	if (s_rx_len == sizeof(s_rx_buf)) {
		ESP_LOGW(TAG, "RX overflow, flushing.");
		s_rx_len = 0;
	}
}

static void transmit(motor_cmd_t cmd) {
	int64_t gap_us = (int64_t)s_cfg.tx_gap_ms * 1000;
	int64_t since = esp_timer_get_time() - s_last_tx_us;
	if (since < gap_us) {
		vTaskDelay(pdMS_TO_TICKS((gap_us - since) / 1000 + 1));
	}
	uint8_t frame[MOTOR_CMD_FRAME_LEN];
	size_t len = motor_proto_build_cmd(cmd, frame, sizeof(frame));
	uart_write_bytes(s_cfg.uart_port, frame, len);
	s_last_tx_us = esp_timer_get_time();
	ESP_LOGI(TAG, "Sent %s.", motor_cmd_str(cmd));
}

static void motor_task(void *arg) {
	for (;;) {
		process_rx();

		if (s_pending.active) {
			int64_t waited_ms = (esp_timer_get_time() - s_pending.sent_us) / 1000;
			if (waited_ms > (int64_t)s_cfg.echo_timeout_ms) {
				if (s_pending.retries_left > 0) {
					s_pending.retries_left--;
					ESP_LOGW(TAG, "No echo for %s, retrying.", motor_cmd_str(s_pending.cmd));
					transmit(s_pending.cmd);
					s_pending.sent_us = esp_timer_get_time();
				} else {
					s_pending.active = false;
				}
			}
			continue; /* Hold new commands until this one resolves. */
		}

		motor_cmd_t cmd;
		if (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
			transmit(cmd);
			if (s_cfg.tx_retries > 0) {
				s_pending.active = true;
				s_pending.cmd = cmd;
				s_pending.retries_left = s_cfg.tx_retries;
				s_pending.sent_us = esp_timer_get_time();
			}
		}
	}
}

esp_err_t motor_init(const motor_config_t *cfg) {
	if (cfg == NULL || s_cmd_queue != NULL) {
		return ESP_ERR_INVALID_STATE;
	}
	s_cfg = *cfg;

	const uart_config_t uart_cfg = {
		.baud_rate = cfg->baud_rate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	esp_err_t err = uart_driver_install(cfg->uart_port, UART_RX_BUF, 0, 0, NULL, 0);
	if (err == ESP_OK) {
		err = uart_param_config(cfg->uart_port, &uart_cfg);
	}
	if (err == ESP_OK) {
		err = uart_set_pin(cfg->uart_port, cfg->tx_gpio, cfg->rx_gpio, UART_PIN_NO_CHANGE,
						   UART_PIN_NO_CHANGE);
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "UART init failed: %s.", esp_err_to_name(err));
		return err;
	}

	s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(motor_cmd_t));
	if (s_cmd_queue == NULL) {
		return ESP_ERR_NO_MEM;
	}
	if (xTaskCreate(motor_task, TASK_NAME, TASK_STACK, NULL, TASK_PRIO, NULL) != pdPASS) {
		return ESP_ERR_NO_MEM;
	}
	ESP_LOGI(TAG, "Motor UART%d ready (TX GPIO%d, RX GPIO%d, %d baud).", cfg->uart_port,
			 cfg->tx_gpio, cfg->rx_gpio, cfg->baud_rate);
	return ESP_OK;
}

esp_err_t motor_send(motor_cmd_t cmd) {
	if (s_cmd_queue == NULL) {
		return ESP_ERR_INVALID_STATE;
	}
	return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void motor_get_state(motor_state_t *out) {
	taskENTER_CRITICAL(&s_state_lock);
	*out = s_state;
	taskEXIT_CRITICAL(&s_state_lock);
}

esp_err_t motor_add_listener(motor_state_cb_t cb, void *ctx) {
	if (cb == NULL || s_listener_count >= MAX_LISTENERS) {
		return ESP_ERR_INVALID_ARG;
	}
	s_listeners[s_listener_count].cb = cb;
	s_listeners[s_listener_count].ctx = ctx;
	s_listener_count++;
	return ESP_OK;
}

bool motor_is_moving(void) {
	motor_state_t st;
	motor_get_state(&st);
	return st.valid && (st.op == MOTOR_OP_OPENING || st.op == MOTOR_OP_CLOSING);
}
