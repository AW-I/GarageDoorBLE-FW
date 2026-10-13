#ifndef __APP_BLE_CMD_H_
#define __APP_BLE_CMD_H_

#include <stdbool.h>
#include "esp_err.h"

/*
 * Commands written to the GDP data characteristic: [cmd][payload...]
 * Reply is read back from the same characteristic ("OK\r\n\0" / "ERROR\r\n\0" or data).
 * Any failed command replies ERROR and the link is dropped after 1 s (V1 behaviour).
 *
 * V1 (unchanged):
 *  0x00 / '0'  OPEN          <password>          toggle door (open / stop / close)
 *  0x01        DEVICE NAME   <password>, then within 2 s <new name>
 *  0x02        PASSWORD      <password>, then within 1 s <new password>
 *  0x03        REBOOT
 *  0x04        INPUT MODE    '0' | '1'           (kept for compatibility only)
 *
 * V3 (session based - AUTH once per connection):
 *  0x10        AUTH          <password>
 *  0x11        DOOR          <action>            see door_action_t
 *  0x12        SET           <key><flags><data>  flags: bit0 first chunk, bit1 last chunk
 *  0x13        GET           <key>               reply: value (secrets masked)
 *  0x14        APPLY                             reconnect Wi-Fi / MQTT with saved settings
 *  0x15        INFO                              reply: key=value;... status text
 *  0x16        FACTORY RESET                     erase settings and reboot
 */
typedef enum {
	CMD_OPEN = 0x00,
	CMD_OPEN_ALT = '0',
	CMD_AP_NAME = 0x01,
	CMD_AP_PASS = 0x02,
	CMD_REBOOT = 0x03,
	CMD_SET_INPUT_MODE = 0x04,

	CMD_AUTH = 0x10,
	CMD_DOOR = 0x11,
	CMD_SET = 0x12,
	CMD_GET = 0x13,
	CMD_APPLY = 0x14,
	CMD_INFO = 0x15,
	CMD_FACTORY_RESET = 0x16,
} cmd_t;

#define SET_FLAG_FIRST 0x01
#define SET_FLAG_LAST 0x02

esp_err_t app_ble_cmd_init(void);

/** @brief Clear per-connection state (session auth, unlock windows). */
void app_ble_cmd_on_connection(bool connected);

#endif
