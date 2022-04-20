#ifndef __D_NVM_H_
#define __D_NVM_H_
#include "esp_err.h"

#define DEFAULT_PASS "default"
#define DEFAULT_NAME "BLEDoor"

#define MAX_PASS_LEN 20
#define MAX_NAME_LEN 15
#define MIN_PASS_LEN 1
#define MIN_NAME_LEN 1

esp_err_t config_init(void);
esp_err_t config_set_pass(uint8_t * pass, uint8_t passLen);
esp_err_t config_set_name(uint8_t * name, uint8_t nameLen);
uint8_t config_get_name(uint8_t ** name);
uint8_t config_get_pass(uint8_t ** pass);

#endif