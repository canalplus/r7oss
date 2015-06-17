/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 * Implementation of Smart Card user space API
 *************************************************************************/

#ifndef _STMSCR_H_
#define _STMSCR_H_

#include <stdio.h>

#ifndef SCR_MAGIC_NO
#define SCR_MAGIC_NO   'l'
#endif

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
#define STM_SCR_GET_LAST_ERROR  _IOWR(SCR_MAGIC_NO, \
				IOCTL_GET_LAST_ERROR, unsigned int)

#define MAX_ATR_SIZE	33
#define MAX_PPS_SIZE	6
#define SCR_DEVICE_NAME	"stm_scr"

enum {
	SCR_ERROR_NO_ANSWER = 0xEEE1,
	SCR_ERROR_NOT_RESET,
	SCR_ERROR_READ_TIMEOUT,
	SCR_ERROR_WRITE_TIMEOUT,
	SCR_ERROR_PPS_FAILED,
	SCR_ERROR_OTHER = 0xEEEE
};


enum stm_scr_ctrl_flags_e {
	STM_SCR_CTRL_BAUD_RATE = 1,
	STM_SCR_CTRL_WORKING_CLOCK_FREQUENCY,
	STM_SCR_CTRL_CONTACT_VCC,
	STM_SCR_CTRL_SET_IFSC,
	STM_SCR_CTRL_NAD,
	STM_SCR_CTRL_PARITY_RETRIES,
	STM_SCR_CTRL_NAK_RETRIES,
	STM_SCR_CTRL_WWT,
	STM_SCR_CTRL_GUARD_TIME,
	STM_SCR_CTRL_CONVENTION,
	STM_SCR_CTRL_PROTOCOL,
	STM_SCR_CTRL_EXTCLK,
	STM_SCR_CTRL_STOP_BITS,
	STM_SCR_CTRL_DATA_PARITY,
	STM_SCR_CTRL_DATA_GPIO,
	STM_SCR_CTRL_CLASS_SELECTION,
};

struct stm_ctrl_command_s {
	enum stm_scr_ctrl_flags_e ctrl_flag;
	unsigned int	value;
};

struct stm_reset_command_s {
	unsigned int	reset_type;
	unsigned char	atr[MAX_ATR_SIZE];
	unsigned int	atr_length;
};

struct stm_transfer_command_s {
	const unsigned char *write_buffer;
	unsigned int	write_size;
	unsigned char	*read_buffer;
	unsigned int	read_size;
	unsigned int	timeout_ms;
	unsigned int	actual_read_size;
};


struct stm_pps_command_s {
	unsigned char	pps_data[MAX_PPS_SIZE];
	unsigned char	pps_response[MAX_PPS_SIZE];
};

enum stm_scr_convention_e {
	STM_SCR_CONVENTION_UNKNOWN = 0,
	STM_SCR_CONVENTION_DIRECT,
	STM_SCR_CONVENTION_INVERSE
};

enum stm_scr_reset_type_e {
	STM_SCR_RESET_COLD = 0,
	STM_SCR_RESET_WARM
};

enum  stm_scr_device_type_e {
	STM_SCR_DEVICE_ISO = 0,
	STM_SCR_DEVICE_CA
};

enum stm_scr_events_e {
	STM_SCR_EVENT_CARD_INSERTED = (1<<0),
	STM_SCR_EVENT_CARD_REMOVED = (1<<1)
};

enum stm_scr_stopbits_e {
	STM_SCR_STOP_0_5 = 0,
	STM_SCR_STOP_1_0,
	STM_SCR_STOP_1_5,
	STM_SCR_STOP_2_0
};

enum stm_scr_databits_e {
	STM_SCR_8BITS_NO_PARITY = 0,
	STM_SCR_8BITS_ODD_PARITY,
	STM_SCR_8BITS_EVEN_PARITY,
	STM_SCR_9BITS_UNKNOWN_PARITY,
};

enum stm_scr_gpio_e {
	STM_SCR_DATA_GPIO_C4_UART = 0x01,
	STM_SCR_DATA_GPIO_C4_LOW  = 0x02,
	STM_SCR_DATA_GPIO_C4_HIGH = 0x04,
	STM_SCR_DATA_GPIO_C7_UART = 0x100,
	STM_SCR_DATA_GPIO_C7_LOW  = 0x200,
	STM_SCR_DATA_GPIO_C7_HIGH = 0x400,
	STM_SCR_DATA_GPIO_C8_UART = 0x10000,
	STM_SCR_DATA_GPIO_C8_LOW  = 0x20000,
	STM_SCR_DATA_GPIO_C8_HIGH = 0x40000
};


/* Please do not change enum, it in inline with standard */
enum stm_scr_protocol_e {
	STM_SCR_PROTOCOL_T0 =	0x0,
	STM_SCR_PROTOCOL_T1 =	0x1,
	STM_SCR_PROTOCOL_T14 =	0xE
};

struct stm_scr_capabilities_s {
	unsigned char	*history;
	unsigned char	*pts_bytes;

	unsigned int	max_baud_rate;
	unsigned int	max_clock_frequency;
	unsigned int	work_etu;

	unsigned int	guard_delay;
	unsigned int	baud_rate;
	unsigned int	clock_frequency;

	/* Current clock frequency being used */
	unsigned int	working_frequency;
	unsigned int	size_of_history_bytes;

	unsigned int	pps_size;

	/* T1 */
	unsigned int	block_guard_time;
	unsigned int	block_waiting_time;
	unsigned int	character_guard_time;
	unsigned int	character_waiting_time;
	unsigned int	work_waiting_time; /* unit = 10ms */
	char		block_retries;
	unsigned int   	ifsc;

	unsigned int	supported_protocol;

	enum stm_scr_device_type_e	device_type;
	enum stm_scr_convention_e	bit_convention;
	unsigned char	class_sel_pad_value;
};

#endif /* _STMSCR_H_ */
