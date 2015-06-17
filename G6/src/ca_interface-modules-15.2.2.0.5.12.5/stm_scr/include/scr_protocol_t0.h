#ifndef _STM_SCR_PROTOCOL_T0_H
#define _STM_SCR_PROTOCOL_T0_H

typedef  enum {
	SCR_PROCEDURE_BYTE_INDEX = 0,
	SCR_SW1_INDEX,
	SCR_SW2_INDEX,
	SCR_RESPONSE_MAX
} scr_command_index_t;

typedef  enum {
	COMMAND_OK = 0,
	CLA_NOK,
	CLA_OK_INS_NOK,
	CLA_INS_OK_P1P2_NOK,
	CLA_INS_P1_P2_OK_P3_NOK,
	COMMAND_NOK
} scr_command_status_t;

typedef  struct {
	uint8_t response_data[SCR_RESPONSE_MAX];
	uint8_t *data;
	uint32_t size_of_data;
} scr_command_response_t;


uint32_t scr_transfer_t0(
				scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p);

#endif
