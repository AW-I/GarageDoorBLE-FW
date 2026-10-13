#include "motor_proto.h"

uint8_t motor_proto_checksum(const uint8_t *data, size_t len) {
	uint32_t sum = 0;
	for (size_t i = 0; i < len; i++) {
		sum += data[i];
	}
	return (uint8_t)(sum & 0xFF);
}

size_t motor_proto_build_cmd(motor_cmd_t cmd, uint8_t *out, size_t out_len) {
	if (out == NULL || out_len < MOTOR_CMD_FRAME_LEN) {
		return 0;
	}
	out[0] = MOTOR_HDR_CMD;
	out[1] = MOTOR_CMD_FRAME_LEN;
	out[2] = MOTOR_TYPE_CMD;
	out[3] = (uint8_t)cmd;
	out[4] = 0x00;
	out[5] = motor_proto_checksum(out, 5);
	out[6] = MOTOR_FOOTER;
	return MOTOR_CMD_FRAME_LEN;
}

static void decode(const uint8_t *f, uint8_t len, motor_frame_t *out) {
	out->type = MOTOR_FRAME_OTHER;
	out->op = MOTOR_OP_UNKNOWN;
	out->led = -1;
	out->position = 0;
	out->echo_cmd = 0;

	if (len == MOTOR_STATUS_LEN && f[2] == MOTOR_TYPE_STATUS) {
		out->type = MOTOR_FRAME_STATUS;
		uint8_t op = f[MOTOR_STATUS_OFS_OP];
		out->op = op <= MOTOR_OP_CLOSING ? (motor_op_t)op : MOTOR_OP_UNKNOWN;
		uint8_t led = f[MOTOR_STATUS_OFS_LED];
		out->led = led == MOTOR_LED_ON_BYTE ? 1 : (led == MOTOR_LED_OFF_BYTE ? 0 : -1);
		uint8_t pos = f[MOTOR_STATUS_OFS_POS];
		out->position = pos > 100 ? 100 : pos;
	} else if (len >= MOTOR_CMD_FRAME_LEN && f[2] == MOTOR_TYPE_CMD) {
		out->type = MOTOR_FRAME_ECHO;
		out->echo_cmd = f[3];
	}
}

bool motor_proto_parse(const uint8_t *buf, size_t len, motor_frame_t *out, size_t *consumed) {
	size_t i = 0;
	while (i < len) {
		if (buf[i] != MOTOR_HDR_STATUS) {
			i++;
			continue;
		}
		if (len - i < 2) {
			break; /* Need the length byte. */
		}
		uint8_t flen = buf[i + 1];
		if (flen < MOTOR_FRAME_MIN || flen > MOTOR_FRAME_MAX) {
			i++;
			continue;
		}
		if (len - i < flen) {
			break; /* Incomplete, wait for more bytes. */
		}
		const uint8_t *f = &buf[i];
		if (f[flen - 1] != MOTOR_FOOTER || motor_proto_checksum(f, flen - 2) != f[flen - 2]) {
			i++; /* False header inside garbage - resync on the next byte. */
			continue;
		}
		decode(f, flen, out);
		*consumed = i + flen;
		return true;
	}
	*consumed = i;
	return false;
}

const char *motor_cmd_str(motor_cmd_t cmd) {
	switch (cmd) {
	case MOTOR_CMD_OPEN:
		return "OPEN";
	case MOTOR_CMD_STOP:
		return "STOP";
	case MOTOR_CMD_CLOSE:
		return "CLOSE";
	case MOTOR_CMD_PARTIAL:
		return "PARTIAL OPEN";
	case MOTOR_CMD_LED_ON:
		return "LED ON";
	case MOTOR_CMD_LED_OFF:
		return "LED OFF";
	}
	return "?";
}

const char *motor_op_str(motor_op_t op) {
	switch (op) {
	case MOTOR_OP_OPENING:
		return "Opening";
	case MOTOR_OP_STOPPED:
		return "Stopped";
	case MOTOR_OP_CLOSING:
		return "Closing";
	default:
		return "Unknown";
	}
}
