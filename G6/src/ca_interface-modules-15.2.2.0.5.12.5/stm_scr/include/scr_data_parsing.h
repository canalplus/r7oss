#ifndef _STM_SCR_DATA_PARSING_H
#define _STM_SCR_DATA_PARSING_H
#include "stm_scr.h"

#define MAX_ATR_SIZE 33
#define SCR_PPS_SIZE 6

#define SCR_PPSS_INDEX 0
#define SCR_PPS0_INDEX 1
#define SCR_PCK_INDEX  5

#define SCR_PPS1_MASK 0x10
#define SCR_PPS2_MASK 0x20
#define SCR_PPS3_MASK 0x40

/* They are capital to directly map to the doc ISO/IEC 7816-3 2006*/
/* Capabilites to be sanity checked from here */

#define SCR_DISCOVERY  STM_SCR_CONVENTION_UNKNOWN
#define	SCR_DIRECT     STM_SCR_CONVENTION_DIRECT
#define SCR_INVERSE    STM_SCR_CONVENTION_INVERSE

typedef enum {
	SCR_CLOCK_STOP_NOT_SUPPORTED = 0,
	SCR_CLOCK_STOP_STATE_L,
	SCR_CLOCK_STOP_STATE_H,
	SCR_CLOCK_STOP_STATE_NO_PREFERNCE
} scr_clock_stop_t;

typedef enum {
	SCR_CLASS_A = 0x1,
	SCR_CLASS_B = 0x2,
	SCR_CLASS_C = 0x4,
	SCR_CLASS_A_B = SCR_CLASS_A|SCR_CLASS_B,
	SCR_CLASS_B_C = SCR_CLASS_B|SCR_CLASS_C,
	SCR_CLASS_A_B_C = SCR_CLASS_A|SCR_CLASS_B|SCR_CLASS_C,
	SCR_CLASS_RFU
} scr_class_t;

/*ATR PARSED DATA*/
typedef struct {
	uint32_t bit_convention;
	uint32_t size_of_history_bytes;
	uint8_t f_int;
	uint8_t d_int;
	uint8_t wi;
	uint8_t N;      /* Extra guard time */
	bool specific_mode;
	bool minimum_guard_time;
	uint8_t specific_protocol_type;
	bool negotiable_mode;
	bool parameters_to_use;
	stm_scr_protocol_t first_protocol;
	uint8_t * history_bytes_p;
	uint32_t ifsc;
	scr_clock_stop_t clock_stop_indicator;
	scr_class_t class_supported;
	char spu;
	/* T1 data*/
	uint32_t rc;
	uint32_t character_waiting_index;
	uint32_t block_waiting_index;
	uint32_t supported_protocol;
} scr_atr_parsed_data_t;

typedef struct {
	uint32_t atr_size;
	scr_atr_parsed_data_t parsed_data;
	uint32_t extra_bytes_in_atr;
	uint8_t raw_data[MAX_ATR_SIZE];
} scr_atr_response_t;

typedef struct {
	stm_scr_protocol_t protocol_set;
	uint8_t f_int;
	uint8_t d_int;
	uint8_t spu;
} scr_pps_info_t;

typedef struct {
	uint32_t pps_size;
	uint8_t  pps_request[SCR_PPS_SIZE];
	uint8_t  pps_response[SCR_PPS_SIZE];
	scr_pps_info_t pps_info;
} scr_pps_data_t;

uint32_t  scr_process_atr(scr_atr_response_t *atr_data_p);
uint8_t scr_validate_tck(scr_atr_response_t *atr_data_p);
uint32_t scr_process_pps_response(scr_pps_data_t* pps_data);

#endif
