#ifndef __M_SYSTEM_API_H_
#define __M_SYSTEM_API_H_

#include "esp_err.h"
void system_reboot(void);
bool verify_crc(void);
bool verify_config(void);

extern uint64_t pwnd;

#endif