#ifndef __M_SYSTEM_API_H_
#define __M_SYSTEM_API_H_

#include "esp_err.h"
extern uint64_t pwnd;
typedef void (*restart_callback_t)(void);

void system_register_restart_callback(restart_callback_t func);
void system_restart(void);
bool verify_crc(void);
bool verify_config(void);

#endif