#ifndef __MOTOR_PROTO_H_
#define __MOTOR_PROTO_H_

/*
 * Forcedoor motor unit UART protocol (115200 8N1, 5V TTL on the USB-A socket).
 *
 *  [header][length][payload ...][checksum][0x0D]
 *  - header   0x23 dongle->motor command, 0x40 motor->dongle status/echo
 *  - length   whole frame length including header, length, checksum, footer
 *  - checksum sum of every preceding byte, truncated to 8 bits
 *
 * Command: 23 07 41 <cmd> 00 <crc> 0D
 * Status : 40 11 49 <op> <led> .. .. .. .. .. .. .. <pos%> .. .. <crc> 0D  (17 bytes)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MOTOR_HDR_CMD       0x23
#define MOTOR_HDR_STATUS    0x40
#define MOTOR_FOOTER        0x0D
#define MOTOR_TYPE_CMD      0x41
#define MOTOR_TYPE_STATUS   0x49
#define MOTOR_CMD_FRAME_LEN 7
#define MOTOR_STATUS_LEN    17
#define MOTOR_FRAME_MIN     5
#define MOTOR_FRAME_MAX     32

/* Byte offsets inside a 17 byte status frame. */
#define MOTOR_STATUS_OFS_OP  3
#define MOTOR_STATUS_OFS_LED 4
#define MOTOR_STATUS_OFS_POS 13

#define MOTOR_LED_ON_BYTE  0xF0
#define MOTOR_LED_OFF_BYTE 0xF1

typedef enum {
	MOTOR_CMD_OPEN = 0x00,
	MOTOR_CMD_STOP = 0x01,
	MOTOR_CMD_CLOSE = 0x02,
	MOTOR_CMD_PARTIAL = 0x03,
	MOTOR_CMD_LED_ON = 0xF0,
	MOTOR_CMD_LED_OFF = 0xF1,
} motor_cmd_t;

typedef enum {
	MOTOR_OP_OPENING = 0x00,
	MOTOR_OP_STOPPED = 0x01,
	MOTOR_OP_CLOSING = 0x02,
	MOTOR_OP_UNKNOWN = 0xFF,
} motor_op_t;

typedef enum {
	MOTOR_FRAME_STATUS,
	MOTOR_FRAME_ECHO,
	MOTOR_FRAME_OTHER,
} motor_frame_type_t;

typedef struct {
	motor_frame_type_t type;
	motor_op_t op;     /* STATUS only */
	int8_t led;        /* STATUS only: 1 on, 0 off, -1 unknown byte */
	uint8_t position;  /* STATUS only: 0..100 % open */
	uint8_t echo_cmd;  /* ECHO only */
} motor_frame_t;

uint8_t motor_proto_checksum(const uint8_t *data, size_t len);

/** @brief Build a command frame. Returns frame length or 0 if out is too small. */
size_t motor_proto_build_cmd(motor_cmd_t cmd, uint8_t *out, size_t out_len);

/**
 * @brief Find and decode the first valid frame in buf.
 * @param consumed Bytes the caller may discard (garbage + the decoded frame).
 * @return true when a frame was decoded into out.
 */
bool motor_proto_parse(const uint8_t *buf, size_t len, motor_frame_t *out, size_t *consumed);

const char *motor_cmd_str(motor_cmd_t cmd);
const char *motor_op_str(motor_op_t op);

#endif
