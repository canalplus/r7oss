#include "ca_os_wrapper.h"
#include "stm_common.h"

#include "stm_scr.h"
#include "scr_data_parsing.h"

#include "scr_internal.h"
#include "scr_utils.h"
#include "scr_io.h"

#define TAX 0x10
#define TBX 0x20
#define TCX 0x40
#define TDX 0x80

uint32_t  scr_process_atr(scr_atr_response_t *atr_data_p)
{
	uint32_t count = 0, yi, index = 1, mode = 0;
	bool first_ta_for_t15 = true;
	bool first_tb_for_t15 = true;
	scr_atr_parsed_data_t *parsed_data_p = &atr_data_p->parsed_data;
	uint8_t * raw_data_p = atr_data_p->raw_data;

	if (raw_data_p[count] == 0x3B)
		parsed_data_p->bit_convention = SCR_DIRECT;
	else if (raw_data_p[count] == 0x3F)
		parsed_data_p->bit_convention = SCR_INVERSE;
	else
		goto error;

	count++;
	parsed_data_p->supported_protocol = 0;
	parsed_data_p->minimum_guard_time = false;
	parsed_data_p->wi = SCR_DEFAULT_W;
	parsed_data_p->N = SCR_DEFAULT_N;
	parsed_data_p->f_int = SCR_DEFAULT_F;
	parsed_data_p->d_int = SCR_DEFAULT_D;
	parsed_data_p->specific_mode = false;
	parsed_data_p->specific_protocol_type = 0;
	parsed_data_p->negotiable_mode = 0;
	parsed_data_p->parameters_to_use = 0;
	parsed_data_p->first_protocol = 0;
	parsed_data_p->ifsc = 32;
	parsed_data_p->size_of_history_bytes = raw_data_p[count] & 0x0F;

	parsed_data_p->history_bytes_p = &raw_data_p[atr_data_p->atr_size -
					parsed_data_p->size_of_history_bytes];

	yi = count;
	count++;
	while (count + parsed_data_p->size_of_history_bytes
					!= atr_data_p->atr_size) {
		scr_debug_print("count %d  index %d  ,"\
				"atr size %d history size %d\n",
				count,
				index,
				atr_data_p->atr_size,
				parsed_data_p->size_of_history_bytes);
/* index tell the X */
/* Interface byte TAX parsing*/
		if ((raw_data_p[yi] & 0xF0) & TAX) {
			if (1 == index) {
				/* Max Fi and Di*/
				parsed_data_p->f_int =
					(raw_data_p[count] & 0xF0) >> 4;
				parsed_data_p->d_int =
					raw_data_p[count] & 0x0F;
			} else if (2 == index) {
				parsed_data_p->specific_mode = true;
				parsed_data_p->specific_protocol_type =
					raw_data_p[count] & 0x0F;
				parsed_data_p->negotiable_mode =
					raw_data_p[count] & 0x80;
				parsed_data_p->parameters_to_use =
					raw_data_p[count] & 0x10;
			} else if ((2 < index && 14 == mode) ||
					(3 == index && 1 == mode)) {
				parsed_data_p->ifsc = raw_data_p[count];
			}
			if (15 == mode && first_ta_for_t15) {
				parsed_data_p->clock_stop_indicator =
					raw_data_p[count] & 0xC0;
				parsed_data_p->class_supported =
					raw_data_p[count] & 0x3F;
				first_ta_for_t15 = false;
			}
			count++;
		}
/* Interface byte TBX parsing*/
		if ((raw_data_p[yi] & 0xF0) & TBX) {
			if (15 == mode && first_tb_for_t15) {
				parsed_data_p->spu = raw_data_p[count];
				first_tb_for_t15 = false;
			}
			if ((2 < index && 14 == mode) ||
					(3 == index && 1 == mode)) {
				parsed_data_p->character_waiting_index =
						raw_data_p[count] & 0x0F;
				parsed_data_p->block_waiting_index =
						(raw_data_p[count] & 0xF0) >> 4;
			}
			/* This is depreciated interface */
			count++;
		}
/* Interface byte TCX parsing*/
		if ((raw_data_p[yi] & 0xF0) & TCX) {
			if (1 == index) {
				if (raw_data_p[count] == 0xFF) {
					parsed_data_p->minimum_guard_time =
						true;
					parsed_data_p->N = SCR_DEFAULT_N;
				} else {
					parsed_data_p->N =
						raw_data_p[count] +
						SCR_DEFAULT_N;

					scr_debug_print("count %d %d N %d\n",
						count, raw_data_p[count],
						parsed_data_p->N);
				}
				if (parsed_data_p->N < SCR_DEFAULT_N)
					parsed_data_p->N = SCR_DEFAULT_N;
			} else if (2 == index)
				parsed_data_p->wi = raw_data_p[count];
			else if ((2 < index && 14 == mode) ||
					(3 == index && 1 == mode)) {
				/* b1 = 1 use CRC, = 0 use LRC */
				parsed_data_p->rc = raw_data_p[count] & 0x01;
			}
			count++;
		}
/* Interface byte TDX parsing */
		if ((raw_data_p[yi] & 0xF0) & TDX) {
			mode = raw_data_p[count] & 0x0F;
			if (1 == index) {
				parsed_data_p->first_protocol =
					raw_data_p[count] & 0x0F;
				if (parsed_data_p->first_protocol == 14)
					parsed_data_p->first_protocol =
						STM_SCR_PROTOCOL_T14;
			}
			yi = count;
			count++;
		} else {
			if (1 == index) {
				parsed_data_p->first_protocol =
					STM_SCR_PROTOCOL_T0;
				parsed_data_p->supported_protocol =
					STM_SCR_PROTOCOL_T0;
			}
			goto done;
		}

		if (14 == mode)
			parsed_data_p->supported_protocol |=
				0x1 << STM_SCR_PROTOCOL_T14;
		else
			parsed_data_p->supported_protocol |=
				0x1 << mode;

		index++;
	}

	done :
	if (count + parsed_data_p->size_of_history_bytes !=
		atr_data_p->atr_size) {

		scr_error_print("<%s>:<%d> Some thing gone wrong with parsing "\
			"present count %d history byte %d  ATR size %d\n",
			__func__, __LINE__,
			count,
			parsed_data_p->size_of_history_bytes,
			atr_data_p->atr_size);

	}
	return 0;

	error:
		return -EINVAL;
}

uint8_t scr_validate_tck(scr_atr_response_t *atr_data_p)
{
	uint8_t cs = 0;
	uint32_t i = 0;

	for (i = 1; i < atr_data_p->atr_size; i++)
		cs ^= atr_data_p->raw_data[i];

	return cs;
}

uint32_t scr_process_pps_response(scr_pps_data_t *pps_data)
{
	uint32_t count = SCR_PPS0_INDEX + 1;
	scr_pps_info_t *pps_info = &pps_data->pps_info;

	if ((pps_data->pps_request[SCR_PPS0_INDEX]&0x0F) !=
		(pps_data->pps_response[SCR_PPS0_INDEX] & 0x0F)) {
		goto invalid;
	} else {
		/*check for the T14 == 0xE*/
		if (0xE == (pps_data->pps_response[SCR_PPS0_INDEX] & 0x0F))
			pps_info->protocol_set = STM_SCR_PROTOCOL_T14;
		else
			pps_info->protocol_set =
				pps_data->pps_response[SCR_PPS0_INDEX] & 0x0F;
	}

/* PPS1 parasing*/
	if (((pps_data->pps_request[SCR_PPS0_INDEX] & SCR_PPS1_MASK) ==
		(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS1_MASK)) ||
		!(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS1_MASK)) {
		if ((pps_data->pps_response[SCR_PPS0_INDEX]) & SCR_PPS1_MASK) {
			if (pps_data->pps_response[count] ==
				pps_data->pps_request[count]) {
				pps_info->d_int =
					pps_data->pps_response[count] & 0x0F;
				pps_info->f_int =
					(pps_data->pps_response[count] & 0xF0)
					>> 4;
			} else
				goto invalid;
			count++;
		} else {
			pps_info->d_int = SCR_DEFAULT_D;
			pps_info->f_int = SCR_DEFAULT_F;
		}
	} else
		goto invalid;

/* PPS2 parsing*/
	if (((pps_data->pps_request[SCR_PPS0_INDEX] & SCR_PPS2_MASK) ==
		(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS2_MASK)) ||
		!(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS2_MASK)) {

		if (pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS2_MASK) {
			if (pps_data->pps_response[count] ==
				pps_data->pps_request[count]) {
				pps_info->spu = pps_data->pps_response[count];
			} else
				goto invalid;
			count++;
		} else
			pps_info->spu = 0x0;
	} else
		goto invalid;

/* PPS3 parsing*/
	if (((pps_data->pps_request[SCR_PPS0_INDEX] & SCR_PPS3_MASK) ==
		(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS3_MASK)) ||
		!(pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS3_MASK)) {

		if (pps_data->pps_response[SCR_PPS0_INDEX] & SCR_PPS3_MASK) {
			if (pps_data->pps_response[count] ==
				pps_data->pps_request[count]) {
				/* Reserved for future used */
			} else
				goto invalid;
			count++;
		} else {
			/* Reserved for future used */
		}
	} else
		goto invalid;

if (scr_calculate_xor(pps_data->pps_response, pps_data->pps_size))
		goto invalid;

	return 0;

	invalid :
		return -EINVAL;
}
