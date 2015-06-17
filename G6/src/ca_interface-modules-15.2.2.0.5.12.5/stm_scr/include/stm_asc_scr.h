/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   stm_asc_scr.h
 @brief
*/

#ifndef _ST_ASC_SCR_H
#define _ST_ASC_SCR_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct stm_asc_s *stm_asc_h;

typedef enum {
	STM_ASC_SCR_TRANSFER_CPU = 0,
	STM_ASC_SCR_TRANSFER_FDMA
} stm_asc_scr_transfer_type_t;

typedef enum stm_asc_scr_databits_e {
	STM_ASC_SCR_8BITS_NO_PARITY = 0,
	STM_ASC_SCR_8BITS_ODD_PARITY,
	STM_ASC_SCR_8BITS_EVEN_PARITY,
	STM_ASC_SCR_9BITS_UNKNOWN_PARITY,
	STM_ASC_SCR_9BITS_NO_PARITY,
	STM_ASC_SCR_9BITS_CONFIRMED_PARITY
} stm_asc_scr_databits_t;


typedef enum stm_asc_scr_stopbits_e {
	STM_ASC_SCR_STOP_0_5 = 0,
	STM_ASC_SCR_STOP_1_0,
	STM_ASC_SCR_STOP_1_5,
	STM_ASC_SCR_STOP_2_0
} stm_asc_scr_stopbits_t;


typedef struct {
	stm_asc_scr_transfer_type_t  transfer_type;
	uint32_t   asc_fifo_size;
	void  *device_data;
} stm_asc_scr_new_params_t;

typedef struct {
	uint32_t  baud;
	stm_asc_scr_databits_t databits;
	stm_asc_scr_stopbits_t  stopbits;
} stm_asc_scr_protocol_t;

typedef struct {
	uint8_t    *buffer_p;
	uint32_t    size;
	uint32_t    actual_size;
	int         timeout_ms;
} stm_asc_scr_data_params_t;

enum {
	STM_ASC_GPIO_C4_BIT,
	STM_ASC_GPIO_C7_BIT,
	STM_ASC_GPIO_C8_BIT
};

typedef enum {
	STM_ASC_SCR_FLOW_CONTROL = 0,
	STM_ASC_SCR_AUTO_PARITY_REJECTION,
	STM_ASC_SCR_NACK,
	STM_ASC_SCR_TX_RETRIES,
	STM_ASC_SCR_PROTOCOL,
	STM_ASC_SCR_GUARD_TIME,
	STM_ASC_SCR_GPIO_C4,
	STM_ASC_SCR_GPIO_C7,
	STM_ASC_SCR_GPIO_C8,
	STM_ASC_SCR_MAX
} stm_asc_scr_ctrl_flags_t;


int stm_asc_scr_new(stm_asc_scr_new_params_t *newparams_p, stm_asc_h  *asc_p);
int stm_asc_scr_delete(stm_asc_h  asc);
int stm_asc_scr_pad_config(stm_asc_h  asc, bool release);
int stm_asc_scr_read(stm_asc_h  asc, stm_asc_scr_data_params_t *params_p);
int stm_asc_scr_write(stm_asc_h  asc, stm_asc_scr_data_params_t *params_p);
int stm_asc_scr_abort(stm_asc_h  asc);
int stm_asc_scr_flush(stm_asc_h  asc);
int stm_asc_scr_set_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag,
		uint32_t value);
int stm_asc_scr_get_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag,
		uint32_t *value_p);
int stm_asc_scr_set_compound_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag,
		void *value_p);
int stm_asc_scr_get_compound_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag,
		void *value_p);

int stm_asc_scr_freeze(stm_asc_h  asc);
int stm_asc_scr_restore(stm_asc_h  asc);
#ifdef __cplusplus
}
#endif

#endif /* */
