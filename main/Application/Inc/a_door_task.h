#ifndef __A_DOOR_TASK_H_
#define __A_DOOR_TASK_H_

typedef enum {
    CMD_OPEN,
    CMD_AP_NAME,
    CMD_AP_PASS,
    CMD_REBOOT
} cmd_t;

typedef struct cmd_callback_struct {
	uint8_t cmdID;
	esp_err_t (*process_func)(uint8_t * rxBuffer, uint32_t rxSize);
    void (*post_func)(void);

}cmd_callback_t;

void door_task_init();

#endif