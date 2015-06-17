/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
  This header file defines the scr ioctls */
/* *************************************************************************/

#ifndef _SCR_USER_INTERFACE_H
#define _SCR_USER_INTERFACE_H

#include "stm_scr.h"

#ifndef SCR_MAGIC_NO
#define SCR_MAGIC_NO   'l'
#endif

#define SCR_DEVICE_TYPE(x) ((strstr(x, "ca") || strstr(x, "CA")) ? 1 : 0)
#define MAX_ATR_SIZE 33
#define MAX_PPS_SIZE 6
#define SCR_DEVICE_NAME "stm_scr"

enum {
	IOCTL_RESET = 0x1,
	IOCTL_GET_CTRL,
	IOCTL_SET_CTRL,
	IOCTL_GET_CAPABILITIES,
	IOCTL_GET_CARD_STATUS,
	IOCTL_DEACTIVATE,
	IOCTL_ABORT,
	IOCTL_PROTOCOL_NEGOTIATION,
	IOCTL_TRANSFER,
	IOCTL_GET_LAST_ERROR
};

#define STM_SCR_RESET		_IOWR(SCR_MAGIC_NO, IOCTL_RESET, unsigned int)
#define STM_SCR_GET_CTRL	_IOWR(SCR_MAGIC_NO, \
					IOCTL_GET_CTRL, unsigned int)
#define STM_SCR_SET_CTRL	_IOWR(SCR_MAGIC_NO, \
						IOCTL_SET_CTRL, unsigned int)
#define STM_SCR_GET_CAPABILITIES _IOWR(SCR_MAGIC_NO, \
					IOCTL_GET_CAPABILITIES, \
					unsigned int)
#define STM_SCR_GET_CARD_STATUS	_IOWR(SCR_MAGIC_NO, \
					IOCTL_GET_CARD_STATUS, unsigned int)
#define STM_SCR_DEACTIVATE	_IOWR(SCR_MAGIC_NO, \
					IOCTL_DEACTIVATE, unsigned int)
#define STM_SCR_ABORT		_IOWR(SCR_MAGIC_NO, \
						IOCTL_ABORT, unsigned int)
#define STM_SCR_PROTOCOL_NEGOTIATION	_IOWR(SCR_MAGIC_NO, \
						IOCTL_PROTOCOL_NEGOTIATION, \
						unsigned int)
#define STM_SCR_TRANSFER	_IOWR(SCR_MAGIC_NO, \
					IOCTL_TRANSFER, \
					unsigned int)
#define STM_SCR_GET_LAST_ERROR	_IOWR(SCR_MAGIC_NO, \
					IOCTL_GET_LAST_ERROR, unsigned int)

struct stm_ctrl_command_s {
	stm_scr_ctrl_flags_t ctrl_flag;
	unsigned int        value;
};

struct stm_reset_command_s {
	unsigned int reset_type;
	unsigned char atr[MAX_ATR_SIZE];
	unsigned int  atr_length;
};

struct stm_transfer_command_s {
	const unsigned char *write_buffer;
	unsigned int write_size;
	unsigned char *read_buffer;
	unsigned int  read_size;
	unsigned int timeout_ms;
	unsigned int actual_read_size;
};


struct stm_pps_command_s {
	unsigned char pps_data[MAX_PPS_SIZE];
	unsigned char pps_response[MAX_PPS_SIZE];
};

#endif
