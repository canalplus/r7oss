#ifndef _STM_SCR_H
#define _STM_SCR_H

#ifdef __cplusplus
extern "C" {
#endif
#include "linux/types.h"
	
#define STM_SCR_MAX 2

#define SCR_ERROR_NO_ANSWER     0xEEE1
#define SCR_ERROR_NOT_RESET     0xEEE2
#define SCR_ERROR_READ_TIMEOUT  0xEEE3
#define SCR_ERROR_WRITE_TIMEOUT 0xEEE4
#define SCR_ERROR_PPS_FAILED    0xEEE5
#define SCR_ERROR_OTHER         0xEEEE

typedef struct stm_scr_s *stm_scr_h;

typedef enum {
	STM_SCR_CONVENTION_UNKNOWN = 0,
	STM_SCR_CONVENTION_DIRECT,
	STM_SCR_CONVENTION_INVERSE
}stm_scr_convention_t;

typedef enum {
	STM_SCR_RESET_COLD = 0,
	STM_SCR_RESET_WARM
}stm_scr_reset_type_t;

typedef enum {
	STM_SCR_DEVICE_ISO = 0,
	STM_SCR_DEVICE_CA
} stm_scr_device_type_t;

typedef enum {
	STM_SCR_EVENT_CARD_INSERTED = (1<<0),
	STM_SCR_EVENT_CARD_REMOVED = (1<<1)
} stm_scr_events_t;

typedef enum {
	STM_SCR_STOP_0_5 = 0,
	STM_SCR_STOP_1_0,
	STM_SCR_STOP_1_5,
	STM_SCR_STOP_2_0
} stm_scr_stopbits_t;

typedef enum {
	STM_SCR_8BITS_NO_PARITY = 0,
	STM_SCR_8BITS_ODD_PARITY,
	STM_SCR_8BITS_EVEN_PARITY,
	STM_SCR_9BITS_UNKNOWN_PARITY,
} stm_scr_databits_t;

typedef enum {
	STM_SCR_DATA_GPIO_C4_UART = 0x01,
	STM_SCR_DATA_GPIO_C4_LOW  = 0x02,
	STM_SCR_DATA_GPIO_C4_HIGH = 0x04,
	STM_SCR_DATA_GPIO_C7_UART = 0x100,
	STM_SCR_DATA_GPIO_C7_LOW  = 0x200,
	STM_SCR_DATA_GPIO_C7_HIGH = 0x400,
	STM_SCR_DATA_GPIO_C8_UART = 0x10000,
	STM_SCR_DATA_GPIO_C8_LOW  = 0x20000,
	STM_SCR_DATA_GPIO_C8_HIGH = 0x40000
} stm_scr_gpio_t;

/*Always add in the end of the ctrl structure
 * and also update the user file for the same */
typedef enum {
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
}stm_scr_ctrl_flags_t;

/* Please do not change enum, it in inline with standard */
typedef enum {
	STM_SCR_PROTOCOL_T0 = 0x0,
	STM_SCR_PROTOCOL_T1 = 0x1,
	STM_SCR_PROTOCOL_T14 = 0xE
}stm_scr_protocol_t;

typedef enum {
	STM_SCR_TRANSFER_RAW_WRITE = 1,
	STM_SCR_TRANSFER_RAW_READ,
	STM_SCR_TRANSFER_APDU
} stm_scr_transfer_type_t;

typedef struct {
	uint8_t		*history;
	uint8_t		*pts_bytes;

	uint32_t	max_baud_rate;
	uint32_t	max_clock_frequency;
	uint32_t	work_etu;

	uint32_t	guard_delay;
	uint32_t	baud_rate;
	uint32_t	clock_frequency;
	uint32_t	working_frequency;/*Current clock frequency being used */

	uint32_t	size_of_history_bytes;

	uint32_t	pps_size;

/*T1*/
	uint32_t	block_guard_time;  /* Fix */
	uint32_t	block_waiting_time;
	uint32_t	character_guard_time; /* Fix */
	uint32_t	character_waiting_time;
	uint32_t	work_waiting_time; /* unit = 10ms*/
	char		block_retries;
	uint32_t	ifsc;

	uint32_t	supported_protocol;
	stm_scr_device_type_t	device_type;
	stm_scr_convention_t	bit_convention;
#ifdef CONFIG_ARCH_STI
	uint8_t		class_sel_gpio_value;
#else
	uint8_t		class_sel_pad_value;
#endif
} stm_scr_capabilities_t;


typedef struct {
	const uint8_t 	*buffer_p;
	uint32_t 	buffer_size;
	uint8_t		*response_p;
	uint32_t 	response_buffer_size;
	uint32_t 	actual_response_size;
	uint32_t	timeout_ms;
	stm_scr_transfer_type_t		transfer_mode;
} stm_scr_transfer_params_t;

int __must_check stm_scr_new(uint32_t  scr_dev_id,
				stm_scr_device_type_t	device_type,
				stm_scr_h  *scr_p);

int __must_check stm_scr_delete(stm_scr_h  scr);

int __must_check stm_scr_transfer(stm_scr_h  scr,
				stm_scr_transfer_params_t  *transfer_params_p);

int __must_check stm_scr_reset(stm_scr_h  scr,
			stm_scr_reset_type_t type,
			uint8_t *atr_p,
			uint32_t *atr_length_p);

int __must_check stm_scr_deactivate(stm_scr_h scr);

int __must_check stm_scr_abort(stm_scr_h scr);

int __must_check stm_scr_protocol_negotiation(stm_scr_h scr,
				uint8_t *pps_data_p,
				uint8_t *pps_response_p);

int __must_check stm_scr_get_capabilities(stm_scr_h scr,
				stm_scr_capabilities_t *capabilities_p);

int __must_check stm_scr_get_card_status(stm_scr_h scr, bool *is_present);

int __must_check stm_scr_get_last_error(stm_scr_h scr,
				uint32_t *last_error);

int __must_check stm_scr_get_control(stm_scr_h scr,
				stm_scr_ctrl_flags_t  ctrl_flag,
				uint32_t  *value_p);

int __must_check stm_scr_set_control(stm_scr_h scr,
				stm_scr_ctrl_flags_t ctrl_flag,
				uint32_t  value);

#ifdef __cplusplus
}
#endif

#endif /* _STM_SCR_H*/
