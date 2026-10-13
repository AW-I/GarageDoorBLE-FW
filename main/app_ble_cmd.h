#ifndef __APP_BLE_CMD_H_
#define __APP_BLE_CMD_H_

#include <stdbool.h>
#include "esp_err.h"

/*
 * Commands are written to the GDP command characteristic as [cmd][payload...].
 * The reply is read back from the same characteristic as [status][data...].
 *
 * Every command except AUTH needs a successful AUTH on the current connection.
 * A wrong password replies RESP_AUTH and drops the link after 1 s (brute-force throttle).
 *
 *  0x01  AUTH           <password>
 *  0x02  DOOR           <action>            see door_action_t
 *  0x03  SET            <key><flags><data>  flags: bit0 first chunk, bit1 last chunk
 *  0x04  GET            <key>               data: value (secrets masked)
 *  0x05  APPLY                              reconnect Wi-Fi / MQTT with saved settings
 *  0x06  INFO                               data: key=value;... status text
 *  0x07  REBOOT
 *  0x08  FACTORY RESET                      erase settings and bonds, reboot
 *  0x09  OTA_BEGIN      <size u32 LE>       start a firmware update (see app_ota.h)
 *  0x0A  OTA_STATUS                         data: next expected offset u32 LE
 *  0x0B  OTA_END                            verify the image, then reboot into it
 *  0x0C  OTA_ABORT
 */
typedef enum {
	CMD_AUTH = 0x01,
	CMD_DOOR = 0x02,
	CMD_SET = 0x03,
	CMD_GET = 0x04,
	CMD_APPLY = 0x05,
	CMD_INFO = 0x06,
	CMD_REBOOT = 0x07,
	CMD_FACTORY_RESET = 0x08,
	CMD_OTA_BEGIN = 0x09,
	CMD_OTA_STATUS = 0x0A,
	CMD_OTA_END = 0x0B,
	CMD_OTA_ABORT = 0x0C,
} cmd_t;

typedef enum {
	RESP_OK = 0x00,
	RESP_AUTH = 0x01,    /* Wrong password, or command sent before AUTH. */
	RESP_INVALID = 0x02, /* Bad payload / length / key. */
	RESP_FAILED = 0x03,  /* Valid request that could not be completed. */
	RESP_UNKNOWN = 0x04, /* Unknown command byte. */
} resp_status_t;

#define SET_FLAG_FIRST 0x01
#define SET_FLAG_LAST 0x02

esp_err_t app_ble_cmd_init(void);

/** @brief Clear per-connection state (session auth, partial SET). */
void app_ble_cmd_on_connection(bool connected);

#endif
