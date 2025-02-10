#ifndef __M_CAPS_DOOR_API_H_
#define __M_CAPS_DOOR_API_H_

#include "caps/iot_caps_helper_doorControl.h"

typedef struct caps_garage_data {
    IOT_CAP_HANDLE* handle;
    void *usr_data;
    void *cmd_data;

    char *garage_state;
    const char *(*get_garage_state)(struct caps_garage_data *caps_data);
    void (*set_garage_state)(struct caps_garage_data *caps_data, const char *value);
    int (*attr_garage_str2idx)(const char *value);
    void (*attr_garage_send)(struct caps_garage_data *caps_data);

    void (*init_usr_cb)(struct caps_garage_data *caps_data);
    void (*cmd_toggle_usr_cb)(struct caps_garage_data *caps_data);
} caps_garage_data_t;

caps_garage_data_t *caps_garage_initialize(IOT_CTX *ctx, const char *component, void *init_usr_cb, void *usr_data);

#endif