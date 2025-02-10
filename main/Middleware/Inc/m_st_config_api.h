#ifndef __D_DEVICE_CONFIG_H_
#define __D_DEVICE_CONFIG_H_
#include "esp_err.h"
#include "stdbool.h"

esp_err_t device_config_init(void);
bool get_device_info(unsigned char ** infoPtr, unsigned int * configLen );
bool get_onboarding_config(unsigned char ** configPtr, unsigned int * configLen );

#endif //__M_DEVICE_CONFIG_H_