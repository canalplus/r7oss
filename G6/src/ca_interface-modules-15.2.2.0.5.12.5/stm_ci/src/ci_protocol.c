#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include "stm_ci.h"
#include "ci_utils.h"
#include "ci_hal.h"
#include "ci_protocol.h"

static stm_ci_cam_type_t ci_check_cam_type(ci_tupple_t *tuple);
static uint8_t	ciplus_check_addition_feature(ci_tupple_t *tuple);

static ca_error_code_t ci_read_tuple(stm_ci_t *ci_p, uint16_t tuple_offset,
		ci_tupple_t *tuple_p);
static int get_product_info_from_tuple(
				ci_tupple_t *tuple,
				uint8_t *product_info_p);
static void go_till_string_end(const char *str_p,
				uint8_t *index_p);
static bool is_num_in_hex_format(const char *str_p,
		char **cur_read_p);
static void get_decimal_val_from_string(const char *str_p,
		uint32_t *value);
static int get_hex_val_from_string(const char *str_p,
		uint32_t *value);
static bool is_cam_ciplus(const char *buf_p,
		uint8_t tuple_length,
		char **cur_p);
static bool is_ciprof_present(const char *buf_p, uint8_t tuple_length,
		char **cur_p);

static ca_error_code_t ci_clear_cmdreg_bit(stm_ci_t	*ci_p,
			stm_ci_command_t command)
{
	ca_error_code_t error = CA_NO_ERROR;
	u_long flags;
	uint32_t io_base_addr = (uint32_t)ci_p->hw_addr.io_addr_v;
	volatile uint8_t *io_reg_addr = (volatile uint8_t *)io_base_addr;

	switch (command) {
	case STM_CI_COMMAND_DA_INT_ENABLE:
		*(io_reg_addr + CI_CMDSTATUS) = CI_RESET_COMMAND_REG;
		if (ci_p->ca.slot_data_irq_disable != NULL)
			error = ci_p->ca.slot_data_irq_disable(&ci_p->ca);
		spin_lock_irqsave(&ci_p->lock, flags);
		ci_p->device.daie_set = false;
		ci_p->device.frie_set = false;
		spin_unlock_irqrestore(&ci_p->lock, flags);
		break;
	default:
		error =	-EINVAL;
		ci_error_trace(CI_PROTOCOL_RUNTIME,
				"error %d\n",
				error);
		break;
	}
	return error;
}

static ca_error_code_t ci_set_cmdreg_bit(stm_ci_t *ci_p,
			stm_ci_command_t command)
{
	ca_error_code_t error = CA_NO_ERROR;
	u_long flags;

	/* Checking for	the Command bit	*/
	switch (command) {
	case STM_CI_COMMAND_DA_INT_ENABLE:
		ci_write_command_reg(ci_p, CI_SETDAIE);
		spin_lock_irqsave(&ci_p->lock, flags);
		ci_p->device.daie_set =	true;
		spin_unlock_irqrestore(&ci_p->lock, flags);
		if (ci_p->ca.slot_data_irq_enable != NULL)
			error =	ci_p->ca.slot_data_irq_enable(&ci_p->ca);
		break;
	default:
		error =	-EINVAL;
		ci_error_trace(CI_PROTOCOL_RUNTIME,
				"error %d\n",
				error);
		break;
	}
	return error;
}

/* Exported functions */
void ci_read_status_register(stm_ci_t	*ci_p, uint8_t *status_value_p)
{
	uint8_t *io_reg_addr = (uint8_t *)ci_p->hw_addr.io_addr_v;
	*status_value_p = *(io_reg_addr + CI_CMDSTATUS);
}

void ci_write_command_reg(stm_ci_t *ci_p, uint8_t	value)
{
	ca_error_code_t error = CA_NO_ERROR;
	u_long flags;
	uint8_t command_to_write;
	uint32_t io_base_addr = (uint32_t)ci_p->hw_addr.io_addr_v;
	uint8_t	*io_reg_addr = (uint8_t *)io_base_addr;

	command_to_write = value;

	/* Reset command or clear register command disables all	flags */
	if ((value == CI_SETRS) || (value == CI_RESET_COMMAND_REG)) {
		/* if interrupt	enabled	disable*/
		if (ci_p->ca.slot_data_irq_disable != NULL)
			ci_p->ca.slot_data_irq_disable(&ci_p->ca);
		spin_lock_irqsave(&ci_p->lock, flags);
		ci_p->device.daie_set =	false;
		ci_p->device.frie_set = false;
		spin_unlock_irqrestore(&ci_p->lock, flags);
	}
	/* ensuring that DAIE and FRIE does not	get reset if
	* another command is	issued */
	if (ci_p->device.frie_set == true)
		command_to_write |= CI_SETFRIE;
	if (ci_p->device.daie_set == true)
		command_to_write |= CI_SETDAIE;

	ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"cmd to write = 0x%x io_reg_addr = %p 0x%x %p\n",
			command_to_write,
			io_reg_addr,
			(uint32_t)ci_p->hw_addr.io_addr_v,
			(io_reg_addr + CI_CMDSTATUS));

	if (value & CI_SETFRIE) {
		spin_lock_irqsave(&ci_p->lock, flags);
		ci_p->device.frie_set =	true;
		spin_unlock_irqrestore(&ci_p->lock, flags);
		if (ci_p->ca.slot_data_irq_enable != NULL)
			error =	ci_p->ca.slot_data_irq_enable(&ci_p->ca);
	}
	if (value & CI_SETDAIE) {
		spin_lock_irqsave(&ci_p->lock, flags);
		ci_p->device.daie_set =	true;
		spin_unlock_irqrestore(&ci_p->lock, flags);
		if (ci_p->ca.slot_data_irq_enable != NULL)
			error =	ci_p->ca.slot_data_irq_enable(&ci_p->ca);
	}

	*(io_reg_addr + CI_CMDSTATUS) = command_to_write;
}

ca_error_code_t ci_read_data(stm_ci_t *ci_p, stm_ci_msg_params_t *params_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint32_t number_read = 0, buffer_count = 0, bytes_available = 0,
		number_to_read = params_p->length;
	uint32_t io_base_addr = (uint32_t)ci_p->hw_addr.io_addr_v;
	volatile uint8_t *io_reg_addr = (volatile uint8_t *)io_base_addr;

	params_p->response_length = 0;
	number_read = 0;
	bytes_available	= 0;

	/* data	not available with CAM */
	if (!(io_reg_addr[CI_CMDSTATUS] & CI_DA)) {
		params_p->response_length = 0;
		return -EIO;
	}
	bytes_available	= io_reg_addr[CI_SIZE_LS];
	bytes_available	|= (io_reg_addr[CI_SIZE_MS] << SHIFT_EIGHT_BIT);

	/* Control size	acceptable */
	if ((number_to_read <=	0) || (bytes_available > number_to_read)) {
		number_read = 0;
		error = -EIO;
		ci_debug_trace(CI_PROTOCOL_RUNTIME,
		"State= %d, Bytes available= %d more than \
				requested length = %d \n",
				ci_p->currstate,
				bytes_available,
				number_to_read);
	} else {
		for (buffer_count = 0; buffer_count < bytes_available;
				buffer_count++) {
			/* state is CP when called from	Init */
			if ((ci_p->currstate != CI_STATE_PROCESS) &&
				(ci_p->device.link_initialized == true)) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"state is no more read state %d\n",
					ci_p->currstate);
				error = -EPERM;
				break;
			}
			params_p->msg_p[buffer_count] =	io_reg_addr[CI_DATA];
			number_read++;
		}
	}
	params_p->response_length = number_read;
	return error;
}

ca_error_code_t ci_write_data(stm_ci_t *ci_p, stm_ci_msg_params_t *params_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	uint32_t		number_sent, retry_count;
	uint16_t		buffer_count;

	uint32_t io_base_addr = (uint32_t)ci_p->hw_addr.io_addr_v;
	volatile uint8_t *io_reg_addr = (volatile uint8_t *)io_base_addr;

	uint32_t		number_to_write = params_p->length;
	params_p->response_length = number_sent	= 0;

	/* The host writes the number of bytes he wants	to send	to the module */
	io_reg_addr[CI_SIZE_LS]	= (uint8_t)(number_to_write & LOWER_BYTE);
	io_reg_addr[CI_SIZE_MS]	= (uint8_t)(number_to_write >> SHIFT_EIGHT_BIT);

	/* few modules send first byte in WE error while writing :
	* need to retry	*/
#ifndef	CI_WE_CHECK_ENABLED /* MG check	this flag config */
	retry_count = 0;
	number_sent = 0;

	/* writing the first byte unless only one byte to read */
	while ((number_to_write > 1) && (retry_count <	CI_MAX_WRITE_RETRY)) {
		/* state is CP	when called from Init */
		if ((ci_p->currstate != CI_STATE_PROCESS) &&
			ci_p->device.link_initialized == true) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"state is no more write\n");
			return -EPERM;
		}
		io_reg_addr[CI_DATA] = params_p->msg_p[0];
		if (io_reg_addr[CI_CMDSTATUS] & CI_WE) {
			number_sent++;
			break;
		}
		retry_count++;
	}
	if (retry_count	== CI_MAX_WRITE_RETRY) {
		error = -EIO;
		ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"Write failed:tried for retry count\n");
		goto out;
	}
	for (buffer_count = 1 ;	buffer_count < (number_to_write	- 1);
			buffer_count++){
		if ((ci_p->currstate != CI_STATE_PROCESS) &&
			(ci_p->device.link_initialized == true)) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"state is no more write\n");
			error =	-EPERM;
			goto out;
		}
		io_reg_addr[CI_DATA] = params_p->msg_p[buffer_count];

		/* If there is some writing errors, we stop the	transfer */
		if ((io_reg_addr[CI_CMDSTATUS] & CI_WE) == 0) {
			error = -EIO;
			goto out;
		}
		number_sent++;
	}
	retry_count = 0;

	/* Write the last byte till WE bit is low */
	do {
		io_reg_addr[CI_DATA] =	params_p->msg_p[number_to_write-1];
	} while	(((io_reg_addr[CI_CMDSTATUS] &	CI_WE) == CI_WE) &&
		(++retry_count < CI_MAX_WRITE_RETRY));

	if (retry_count	== CI_MAX_WRITE_RETRY) {
		error = -EIO;
		goto out;
	}
	number_sent++;
#else
	retry_count = 0;
	/* Few modules does not	set WE after first byte	*/
	ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"CI_WE_CHECK_ENABLED enabled\n");
	/* Without checking WE bit */
	for (buffer_count = 0 ;	buffer_count < number_to_write;
		buffer_count++){
		/* state is CP when called from	new */
		if ((ci_p->currstate != CI_STATE_PROCESS) &&
			(ci_p->device.link_initialized == true)) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"state is no more write :State = %d\n",
					ci_p->currstate);
			error =	-EPERM;
			goto out;
		}
		io_reg_addr[CI_DATA] = params_p->msg_p[buffer_count];
		number_sent++;
	}
#endif
out:
	params_p->response_length = number_sent;
	return error;
}

ca_error_code_t ci_read_status(stm_ci_t *ci_p, stm_ci_status_t status,
			uint8_t *value_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint8_t	status_value = 0;

	*value_p = 0;
	ci_read_status_register(ci_p, &status_value);

	switch (status) {	/* Checking for	the status bit */
	case STM_CI_STATUS_DATA_AVAILABLE:
		if (status_value & CI_DA)
			*value_p = BIT_VALUE_HIGH;
		break;

	case STM_CI_STATUS_FREE:
		if (status_value & CI_FR)
			*value_p = BIT_VALUE_HIGH;
		break;

	case STM_CI_STATUS_IIR:
		if (status_value & CI_IIR)
			*value_p = BIT_VALUE_HIGH;
		break;
	default:
		error =	-EINVAL;
		ci_error_trace(CI_PROTOCOL_RUNTIME, "%d\n", error);
		break;
	}
	return error;
}

ca_error_code_t ci_write_command(stm_ci_t *ci_p, stm_ci_command_t command,
				uint8_t	value)
{
	ca_error_code_t error = CA_NO_ERROR;

	if (value == BIT_VALUE_HIGH) {
		error =	ci_set_cmdreg_bit(ci_p, command);
	} else {
		if (value == BIT_VALUE_LOW)
			/*If the Command bit is to be set low */
			error =	ci_clear_cmdreg_bit(ci_p, command);
		else
			error =	-EINVAL;
	}
	return error;
}
/* To be checked */
static ca_error_code_t ci_parse_cis(stm_ci_t *ci_p,
			stm_ci_capabilities_t *cap_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	static ci_tupple_t current_tuple;
	uint16_t offset_current_tuple;
	uint8_t	*SubtTuppleValue_p, *TPCE_SUB_TUPPLE_p, *TPCE_TD_p, *TPCE_PD_p;
	uint32_t	StatusCheckCIS = 0, retry_count, TPCC_RADR = 0,
			TPCE_IO_BLOCK_START, TPCE_IO_BLOCK_SIZE = 0,
			number_read = 0;

	uint8_t	SubtTuppleTag, SubtTuppleLen, FunctionEntry, TpceIf = 0,
		Tpcefs = 0, PowerDescriptionBits, CORValue = 0;

	/* Variable related to the PC Card Specification Vol 2 */
	uint8_t	TPCC_SZ, TPCC_LAST, TPCC_RMSK, TPCC_RFSZ, TPCC_RMSZ, TPCC_RASZ,
		TPCE_INDX, TPCE_INTFACE, TPCE_DEFAULT, TPCE_IO_RANGE,
		TPCE_IR_LEVEL;
	uint8_t	TPCE_PD_SPSB;	/* Structure parameter selection byte */
	uint8_t	TPCE_PD_SPD;	/* Structure parameter definition */
	uint8_t	TPCE_PD_SPDX;	/* Extension structure parameter definition */
	uint8_t	TPCE_IO_BM;	/* I/O bus mapping */
	uint8_t	TPCE_IO_AL;		/* I/O address line */
	uint8_t	TPCE_IO_RDB;	/* I/O Range descriptor	byte */
	uint8_t	TPCE_IO_SOL;	/* Size	of Length I/O address range*/
	uint8_t	TPCE_IO_SOA;	/* Size	of Address I/O address range*/
	uint8_t	TPCE_IO_NAR;	/* Number of I:O address range */
	uint8_t	*TPCE_IO_RDFFCI_p, *TPCE_IO_p, *TPCE_IR_p, *TPCE_MS_p;

	uint8_t	*temp_cis_p = ci_p->device.cis_buf_p;

	ci_debug_trace(CI_PROTOCOL_RUNTIME, "\n");

	TPCE_SUB_TUPPLE_p = NULL;
	offset_current_tuple = 0;
	for (retry_count = 0; retry_count < CI_MAX_CHECK_CIS_RETRY;
			retry_count++) {
		if (ci_p->currstate == CI_STATE_CARD_NOT_PRESENT) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"CAM is not present\n");
			return -EPERM;
		}
		ca_os_sleep_milli_sec(1);

		error =	ci_read_tuple(ci_p, offset_current_tuple,
				&current_tuple);
		if (error == CA_NO_ERROR) {
			ca_os_copy((uint8_t *)temp_cis_p, &current_tuple,
					(2 + current_tuple.tuple_link));
			temp_cis_p += (2 + current_tuple.tuple_link);
			break;
		}
	}
	/* Looping till	we get the last	Tupple or any error */
	while ((current_tuple.tuple_tag != CISTPL_END)
			&& (current_tuple.tuple_link != 0)
			&& (StatusCheckCIS != STATUS_CIS_CHECKED)) {
		/* check for the tuple tag */
		switch (current_tuple.tuple_tag) {
		case CISTPL_DEVICE_OA:
			break;
		case CISTPL_VERS_1:
		{
			if ((current_tuple.tuple_data[IND_TPLLV1_MINOR]
				!= VAL_TPLLV1_MINOR) ||
			(current_tuple.tuple_data[IND_TPLLV1_MAJOR] \
				!= VAL_TPLLV1_MAJOR)) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Invalid tupple	CISTPL_VERS_1(%02X) \
				LINK=%d\n",
				current_tuple.tuple_tag,
				current_tuple.tuple_link);
				break;
			}
			cap_p->cam_type	= ci_check_cam_type(&current_tuple);
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"CAM type = %d\n",
					cap_p->cam_type);
			if (cap_p->cam_type == STM_CI_CAM_DVBCI_PLUS)
				cap_p->features.operator_profile_resource =
				ciplus_check_addition_feature(&current_tuple);
			StatusCheckCIS |= STATUS_CIS_VERS_1_CHECKED;
			current_tuple.tuple_data[current_tuple.tuple_link] = 0;
			break;
		} /* end of case CISTPL_VERS_1 */

		case CISTPL_DEVICE_OC:
		if ((current_tuple.tuple_data[0] & 0x6)	== 0) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"Card operates at 5V 0x%x\n",
					current_tuple.tuple_data[0]);
			ci_p->device.cam_voltage = CI_VOLT_5000;
		} else if ((current_tuple.tuple_data[0] & 0x6) == 0x2) {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Card operates at 3.3V 0x%x\n",
				current_tuple.tuple_data[0]);
			ci_p->device.cam_voltage = CI_VOLT_3300;
		} else {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Reserved for future use\n");
		}
		break;
		case CISTPL_MANFID:
		{
			if ((current_tuple.tuple_link != SZ_TPLLMANFID)) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"Invalid tupple CISTPL_MANFID(%02X) \
					LINK=%d\n",
					current_tuple.tuple_tag,
					current_tuple.tuple_link);
				break;
			}
			StatusCheckCIS |= STATUS_CIS_MANFID_CHECKED;
			break;
		}
		case CISTPL_CONFIG:
		{
			if ((current_tuple.tuple_link < 5)) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"Invalid tupple	CISTPL_CONFIG(%02X) \
					LINK=%d\n",
					current_tuple.tuple_tag,
					current_tuple.tuple_link);
				break;
			}
			TPCC_SZ		= current_tuple.tuple_data[0];
			TPCC_LAST	= current_tuple.tuple_data[1];
			TPCC_RFSZ	= TPCC_SZ & MASK_TPCC_RFSZ;
			TPCC_RMSZ	= TPCC_SZ & MASK_TPCC_RMSZ;
			TPCC_RASZ	= TPCC_SZ & MASK_TPCC_RASZ;

			switch (TPCC_RASZ) {
			case TPCC_RADR_1_BYTE:
			{
				TPCC_RADR = current_tuple.tuple_data[2];
				break;
			}
			case TPCC_RADR_2_BYTE:
			{
				TPCC_RADR = current_tuple.tuple_data[3];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[2];
				break;
			}
			case TPCC_RADR_3_BYTE:
			{
				TPCC_RADR = current_tuple.tuple_data[4];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[3];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[2];
				break;
			}
			case TPCC_RADR_4_BYTE:
			{
				TPCC_RADR = current_tuple.tuple_data[5];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[4];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[3];
				TPCC_RADR <<= SHIFT_EIGHT_BIT;
				TPCC_RADR |= current_tuple.tuple_data[2];
				break;
			}
			} /* end switch	TPCC_RASZ */
			if (TPCC_RADR > MAX_COR_BASE_ADDRESS) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"Invalid tupple CISTPL_CONFIG(%02X) \
					LINK=%d\n TPCC_RADR[%4X] > 0x0FFE\n",
					current_tuple.tuple_tag,
					current_tuple.tuple_link,
					TPCC_RADR);
				break;
			} else {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"Valid tupple CISTPL_CONFIG TPCC_RADR \
					%x\n",
					TPCC_RADR);

			}
			if (TPCC_RMSZ != 0) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Invalid tupple	CISTPL_CONFIG(%02X) \
				LINK=%d\n more than 1 Configuration register \
				[%d]\n",
				current_tuple.tuple_tag,
				current_tuple.tuple_link,
				TPCC_RMSZ+1);

				break;
			}

			TPCC_RMSK = current_tuple.tuple_data[TPCC_RASZ + 3];
			SubtTuppleTag = current_tuple.tuple_data[TPCC_RASZ+4];
			SubtTuppleLen = current_tuple.tuple_data[TPCC_RASZ+5];
			SubtTuppleValue_p = current_tuple.tuple_data +
						TPCC_RASZ + 6;

			FunctionEntry =	SubtTuppleTag;

			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"FunctionEntry = %02X\n",
					FunctionEntry);

			if (((strcmp(STCF_LIB, (char *)SubtTuppleValue_p + 2))
						== 0) &&
				(SubtTuppleValue_p[1] == STCI_IFN_HIGH) &&\
				(SubtTuppleValue_p[0] == STCI_IFN_LOW)) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"TPCC_LAST = %02X STCI_IFN=%02X%02X\n%s\n",
						TPCC_LAST,
						SubtTuppleValue_p[1],
						SubtTuppleValue_p[0],
						SubtTuppleValue_p + 2);

				StatusCheckCIS |= STATUS_CIS_CONFIG_CHECKED;

			}
			break;
		}
		case CISTPL_CFTABLE_ENTRY:
		{
			TPCE_INDX = current_tuple.tuple_data[0];
			TPCE_INTFACE = TPCE_INDX & MASK_7_BIT;
			TPCE_DEFAULT = TPCE_INDX & MASK_6_BIT;
			TPCE_INDX = TPCE_INDX & MASK_TPCE_INDX;

			if (TPCE_INTFACE) {
				TpceIf = current_tuple.tuple_data[1];
				Tpcefs = current_tuple.tuple_data[2];
				TPCE_PD_p = current_tuple.tuple_data + 3;
			} else	{
				Tpcefs = current_tuple.tuple_data[1];
				TPCE_PD_p = current_tuple.tuple_data + 2;
			}

			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"\tField Selection %x\n",
					Tpcefs);
			PowerDescriptionBits =	Tpcefs & \
						(MASK_1_BIT | MASK_0_BIT);

			/* Check power description */
			while (PowerDescriptionBits > 0)	{
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"\tpower description %02X, nb=%d\n",
					*TPCE_PD_p,
					PowerDescriptionBits);
			for (TPCE_PD_SPSB = *TPCE_PD_p++;\
				TPCE_PD_SPSB > 0; TPCE_PD_SPSB >>= 1) {
				if (TPCE_PD_SPSB & MASK_0_BIT) {
					TPCE_PD_SPD = *TPCE_PD_p++;
					ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"\t\tpower description field %02X\n",
						TPCE_PD_SPD);
					while (*TPCE_PD_p & MASK_7_BIT)
						TPCE_PD_SPDX = *TPCE_PD_p++;
				}
			}
			PowerDescriptionBits--;
		}
		TPCE_TD_p = TPCE_PD_p;
		/* on n est plus sur un power descriptor
		* * mais sur un time descriptor */
		TPCE_PD_p = NULL;
		/* timing */
		if (Tpcefs & MASK_2_BIT) {
			switch ((*TPCE_TD_p) &	MASK_RW_SCALE) {
			case NOT_READY_WAIT:
			/* neither ready scale nor wait scale definition */
				TPCE_IO_p = TPCE_TD_p +	1;
				break;
			case WAIT_SCALE_0:
			case WAIT_SCALE_1:
			case WAIT_SCALE_2: /* only wait scale definition */
				TPCE_IO_p = TPCE_TD_p +	2;
				break;

			case READY_SCALE_0:
			case READY_SCALE_1:
			case READY_SCALE_2:
			case READY_SCALE_3:
			case READY_SCALE_4:
			case READY_SCALE_5:
			case READY_SCALE_6: /* only ready scale	definition */
				TPCE_IO_p = TPCE_TD_p +	2;
				break;
			default:
				TPCE_IO_p = TPCE_TD_p +	3;
				break;
			}
		} else {
			TPCE_IO_p = TPCE_TD_p;
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"\t Timing Descriptor:Not There\n");
		}
		/* I/O space */
		if (Tpcefs & MASK_3_BIT) {
			TPCE_IR_p = TPCE_IO_p +	1;
			TPCE_IO_RANGE =	(*TPCE_IO_p) & MASK_7_BIT;

			TPCE_IO_BM = ((*TPCE_IO_p) &\
				(MASK_5_BIT | MASK_6_BIT)) >> 5;
			/* bus mapping */
			TPCE_IO_AL = (*TPCE_IO_p) & MASK_IO_ADDLINES;

			if (TPCE_IO_RANGE) {
				int TpceLoop;
				/* I/O Range descriptor byte */
				TPCE_IO_RDB = TPCE_IO_p[1];
				/* Size of Length I/O address range*/
				TPCE_IO_SOL = (TPCE_IO_RDB &\
						(MASK_7_BIT | MASK_6_BIT)) >> 6;
				/* Size of Address I/O address range*/
				TPCE_IO_SOA = (TPCE_IO_RDB &\
						(MASK_5_BIT | MASK_4_BIT)) >> 4;
				/* Number of I:O address range */
				TPCE_IO_NAR = (TPCE_IO_RDB &\
						(MASK_3_BIT | MASK_2_BIT |\
						MASK_1_BIT | MASK_0_BIT)) + 1;

				TPCE_IO_RDFFCI_p = TPCE_IO_p + 2;
				for (TpceLoop = 0; TpceLoop < TPCE_IO_NAR;
						TpceLoop++) {
					TPCE_IO_BLOCK_START =
					TPCE_IO_RDFFCI_p[(TPCE_IO_SOL +
							TPCE_IO_SOA)*TpceLoop];
					if (TPCE_IO_SOA > 1) {
						TPCE_IO_BLOCK_START |=
						((uint32_t)TPCE_IO_RDFFCI_p[(\
							TPCE_IO_SOL +\
						TPCE_IO_SOA)*TpceLoop+1]) << \
						SHIFT_EIGHT_BIT;
				}
				TPCE_IO_BLOCK_SIZE =
				TPCE_IO_RDFFCI_p[(TPCE_IO_SOL + TPCE_IO_SOA) * \
						TpceLoop + TPCE_IO_SOA] + 1;
				}
				TPCE_IR_p = TPCE_IO_RDFFCI_p + \
					(TPCE_IO_SOL+TPCE_IO_SOA)*TPCE_IO_NAR;
			} else {
				TPCE_IR_p = TPCE_IO_p+1;
			}

		} else {
			TPCE_IR_p = TPCE_IO_p;
			*TPCE_IO_p = 0;
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"\tNot I/O space descriptor, Tpcefs =%02X\n",
				Tpcefs);
		}
		/* IRQ */
		if (Tpcefs & MASK_4_BIT) {
			TPCE_IR_LEVEL =	(*TPCE_IR_p) & MASK_5_BIT;
			if (TPCE_IR_LEVEL == MASK_5_BIT) {
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"\tLevel triggered interrupt supported \
				TPCE_IR_p = %02X\n", *TPCE_IR_p);
			/* check here if DVBCI and if not then what */
			cap_p->interrupt_supported = true;
		}
		TPCE_MS_p = TPCE_IR_p + 1;
		ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"IRQ description is present\n");
		} else {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"IRQ description is not	present\n");
			TPCE_MS_p = TPCE_IR_p;
		}
		/* Memory space	*/
		switch ((Tpcefs & (MASK_6_BIT | MASK_6_BIT)) >> 5) {
		case 0:
			TPCE_SUB_TUPPLE_p = TPCE_MS_p;
			TPCE_MS_p = NULL;
			break;
		case 1:
			TPCE_SUB_TUPPLE_p = TPCE_MS_p +	1;
			break;
		case 2:
			TPCE_SUB_TUPPLE_p = TPCE_MS_p +	2;
			break;
		case 3:
			TPCE_SUB_TUPPLE_p = TPCE_MS_p +	3;
			break;
		}
		if ((TpceIf == TPCE_IF) && (*TPCE_IO_p == TPCE_IO)) {
			CORValue = TPCE_INDX;
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"Function COR(%02X) at(0x%x) for entry %s\n",
					CORValue,
					TPCC_RADR,
					TPCE_SUB_TUPPLE_p+2);
			StatusCheckCIS |= STATUS_CIS_CFTENTRY_CHECKED;
		} else {
			ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"Function COR(%02X) at(0x%x) for entry %s\n",
					CORValue,
					TPCC_RADR,
					TPCE_SUB_TUPPLE_p + 2);
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Other Function	COR(%02X) for entry %s\n",
					TPCE_INDX,
					TPCE_SUB_TUPPLE_p + 2);
		}
		break;
		}
		default:
		break;
		}	/* end switch */

		offset_current_tuple +=	2 + current_tuple.tuple_link;
		/* number of cis bytes */
		number_read += (5 + current_tuple.tuple_link);
		error =	ci_read_tuple(ci_p, offset_current_tuple,
				&current_tuple);
		if (error != CA_NO_ERROR)
			return error;

		ca_os_copy((uint8_t *)temp_cis_p, &current_tuple,
				(2 + current_tuple.tuple_link));
		temp_cis_p += (2 + current_tuple.tuple_link);
	} /* end while */

	/* for last tuple */
	number_read += (5 + current_tuple.tuple_link);
	if (StatusCheckCIS != STATUS_CIS_CHECKED) {
		ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Parse CIS Not Completed 0x%x\n",
				StatusCheckCIS);
		ci_p->device.cor_address = 0;
		ci_p->device.configuration_index = 0;
		error =	-EIO;
	} else {
		ci_p->device.cor_address = TPCC_RADR;
		ci_p->device.configuration_index = CORValue;
		cap_p->cis_size	= number_read;
		error = CA_NO_ERROR;
	}
	if (error != CA_NO_ERROR)
		ci_error_trace(CI_PROTOCOL_RUNTIME, "\n");
	else
		ci_debug_trace(CI_PROTOCOL_RUNTIME, "\n");
	return error;

}

ca_error_code_t ci_configure_card(stm_ci_t *ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	stm_ci_capabilities_t	ci_caps;
	u_long			flags;

	if (ci_p->device.access_mode != CI_ATTRIB_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
					CI_ATTRIB_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_ATTRIB_ACCESS;
	}
	memset(&ci_caps, 0x00, sizeof(stm_ci_capabilities_t));
	error =	ci_parse_cis(ci_p, &ci_caps);
	if (error != CA_NO_ERROR) {
		ci_debug_trace(CI_PROTOCOL_RUNTIME,
				"Parse CIS attribute failed = %d\n",
				error);
		return error;
	}

	/* Update the capabilities */
	spin_lock_irqsave(&ci_p->lock, flags);
	ca_os_copy(&ci_p->device.ci_caps, &ci_caps,
				sizeof(stm_ci_capabilities_t));
	spin_unlock_irqrestore(&ci_p->lock, flags);

	/* Card	Voltage	change from 5V to 3.3V */
	if (ci_p->device.cam_voltage == CI_VOLT_3300) {
		error =	ci_hal_power_disable(ci_p);
		if (error == CA_NO_ERROR)
			error =	ci_hal_power_enable(ci_p);
	}
	ci_write_cor(ci_p);
	return error;
}

void ci_write_cor(stm_ci_t *ci_p)
{
	uint8_t	*cor_mem_addr_p;
	uint32_t mem_base_addr = (uint32_t)ci_p->hw_addr.attr_addr_v +\
					ci_p->device.cor_address;
	cor_mem_addr_p = (uint8_t *)mem_base_addr;
	ci_debug_trace(CI_PROTOCOL_RUNTIME,
			"COR value=0x%x	@COR address %p\n",
			(ci_p->device.configuration_index),
			cor_mem_addr_p);
	/* Writing the Config Index vlaue to the COR */
	*cor_mem_addr_p	= ci_p->device.configuration_index;
}

static ca_error_code_t ci_read_tuple(stm_ci_t *ci_p, uint16_t tuple_offset,
							ci_tupple_t *tuple_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	volatile uint8_t *tuple_addr_p;
	uint32_t tupple_base_addr = 0;
	uint16_t byteOffset = 0;
	uint8_t	tuple_byte_count;

	tupple_base_addr = (uint32_t)ci_p->hw_addr.attr_addr_v;
	tupple_base_addr += (tuple_offset << EVEN_ADDRESS_SHIFT);
	tuple_addr_p = (volatile uint8_t *)tupple_base_addr;
	tuple_p->tuple_tag = tuple_addr_p[0];

	switch (tuple_p->tuple_tag) {
	case CISTPL_DEVICE:
	case CISTPL_DEVICE_OA:
	case CISTPL_DEVICE_OC:
	case CISTPL_VERS_1:
	case CISTPL_MANFID:
	case CISTPL_CONFIG:
	case CISTPL_CFTABLE_ENTRY:
	case CISTPL_NO_LINK:
	case CISTPL_17:
	case CISTPL_FUNCID:
	case CISTPL_END:
		break;
	default:
		return -EIO;
	} /* End switch	*/

	byteOffset = (1	<< EVEN_ADDRESS_SHIFT);
	tuple_p->tuple_link = tuple_addr_p[byteOffset];

	if ((tuple_p->tuple_link == 0) && \
		(tuple_p->tuple_tag != CISTPL_NO_LINK))
		return -EIO;

	if (tuple_p->tuple_tag == CISTPL_END)
		tuple_p->tuple_link = 0;

	/* Reading the tupple data */
	for (tuple_byte_count =	0; tuple_byte_count < tuple_p->tuple_link;
		tuple_byte_count++) {
		tuple_p->tuple_data[tuple_byte_count] =
		tuple_addr_p[(tuple_byte_count + 2) << EVEN_ADDRESS_SHIFT];
	}
	tuple_p->tuple_data[tuple_byte_count++]	= '\0';
	tuple_p->tuple_data[tuple_byte_count++]	= '\0';
	tuple_p->tuple_data[tuple_byte_count++]	= '\0';
	tuple_p->tuple_data[tuple_byte_count] = '\0';

	return error;
}

/* MG need to add support of additional	feature	*/
static stm_ci_cam_type_t ci_check_cam_type(ci_tupple_t *tuple)
{
	char *cur_info_p;
	uint8_t	product_info[MAX_TUPLE_BUFFER];
	uint8_t tuple_length;
	bool is_ci_plus = false;

	cur_info_p = tuple->tuple_data + TPLLV1_INFO;
	get_product_info_from_tuple(tuple, product_info);
	tuple_length = tuple->tuple_link - 2;
	is_ci_plus = is_cam_ciplus(product_info,
			tuple_length,
			&cur_info_p);
	if (is_ci_plus == true)
		return STM_CI_CAM_DVBCI_PLUS;

	return STM_CI_CAM_DVBCI;
}

static uint8_t	ciplus_check_addition_feature(ci_tupple_t *tuple)
{
	char	*cur_read_product_info_p;
	uint8_t	product_info[MAX_TUPLE_BUFFER];
	uint8_t tuple_length;
	char *temp_info_p;
	uint32_t value = 0;
	bool is_true;

	get_product_info_from_tuple(tuple, product_info);
	tuple_length	= tuple->tuple_link - 2;
	is_true = is_ciprof_present(product_info,
			tuple_length,
			&cur_read_product_info_p);

	if (is_true == true) {
		is_true = false;
		is_true = is_num_in_hex_format(cur_read_product_info_p,
						&temp_info_p);
		if (is_true == true)
			get_hex_val_from_string(temp_info_p, &value);
		else
			get_decimal_val_from_string(cur_read_product_info_p,
						&value);
		return value & 0x1;
	}
	return 0;
}

static bool is_ciprof_present(const char *product_info_p,
				uint8_t tuple_length,
				char **cur_read_product_info_p)
{
	uint8_t index = 0;
	char *operator_string;
	bool is_ciprof = false;
	char *start_p, *end_p;
	do {
		start_p = (uint8_t *)strstr(
				(const char *)&product_info_p[index],
				(const char *)CIPLUS_PRODUCT_INFO_START);
		end_p = (uint8_t *)strstr(
				(const char *)&product_info_p[index],
				(const char *)CIPLUS_PRODUCT_INFO_END);
		if (start_p != NULL)
			operator_string = strstr(
			(const char *)start_p,
			(const char *)CIPLUS_OPERATOR_STRING_IDENTIFIER);

		if (start_p != NULL &&
			end_p != NULL &&
			operator_string != NULL){
			if (operator_string < end_p) {
				is_ciprof = true;
				/* move to = in string 'ciprof=' */
				*cur_read_product_info_p = operator_string+7;
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
					"CIPROF string present at %s\n",
					operator_string);
				break;
			}
		}
		go_till_string_end(product_info_p, &index);
		/*move to next string location*/
		index++;
	} while ((index < tuple_length) && (tuple_length < MAX_TUPLE_BUFFER));
	return is_ciprof;
}

static bool is_cam_ciplus(const char *product_info_p,
		uint8_t tuple_length,
		char **cur_read_product_info_p)
{
	uint8_t index = 0;
	char *ciplus_string;
	bool is_ci_plus = false;
	char *start_p, *end_p;

	do {
		start_p = (uint8_t *)strstr(
				(const char *)&product_info_p[index],
				(const char *)CIPLUS_PRODUCT_INFO_START);
		end_p = (uint8_t *)strstr(
				(const char *)&product_info_p[index],
				(const char *)CIPLUS_PRODUCT_INFO_END);
		if (start_p != NULL)
			ciplus_string = strstr(
				(const char *)start_p,
				(const char *)CIPLUS_STRING_IDENTIFIER);
		if (start_p != NULL &&
			end_p != NULL &&
			ciplus_string != NULL){
			if (ciplus_string < end_p) {
				is_ci_plus = true;
				ci_debug_trace(CI_PROTOCOL_RUNTIME,
						"ciplus string present at %s\n",
						ciplus_string);
				break;
			}
		}
		go_till_string_end(product_info_p, &index);
		/*move to next string location*/
		index++;
	} while ((index < tuple_length) && (tuple_length < MAX_TUPLE_BUFFER));

/*TODO to be removed later: Special Case. needed for on card*/
#if 1
	index = 0;
	if (strcmp((const char *)&product_info_p[index], "smardtv") == 0)
		return STM_CI_CAM_DVBCI_PLUS;
#endif
	return is_ci_plus;
}

static int get_product_info_from_tuple(
				ci_tupple_t *tuple,
				uint8_t *product_info_p)
{
	uint8_t tuple_length = tuple->tuple_link - 2;
	uint8_t *temp_product_info_p;
	uint32_t index = 0;

	temp_product_info_p = (tuple->tuple_data + TPLLV1_INFO);
	/* convert the string torlower case	*/
	while ((tuple_length > 0) && (tuple_length < MAX_TUPLE_BUFFER)) {
		*temp_product_info_p = tolower(*temp_product_info_p);
		product_info_p[index] = *temp_product_info_p;
		temp_product_info_p++;
		tuple_length--;
		index++;
	}
	return 0;
}

static void go_till_string_end(const char *str_p, uint8_t *index_p)
{
	uint8_t	temp_index = *index_p;
	const char *cur_char = (char *)(&str_p[temp_index]);
	while (*cur_char != '\0') {
		temp_index++;
		cur_char = &(str_p[temp_index]);
	}
	*index_p = temp_index;

	return;
}

static void get_decimal_val_from_string(const char *str_p,
					uint32_t *value)
{
	while (*str_p != ']') {
		*value *= 10;
		*value += (*str_p - '0');
		str_p++;
	}
}

static int get_hex_val_from_string(const char *str_p,
				uint32_t *value)
{
	/*validate if its a hex number*/
	if (!(str_p[0] == '0' &&
		str_p[1] == 'x')) {
		return -1;
	}

	/*move the pointer by 2 places to consume '0x'*/
	str_p += 2;
	while (*str_p != ']') {
		*value <<= 4;
		if (*str_p <= '9')
			*value |= (*str_p - '0');
		else
			*value |= (*str_p + 10 - 'a');

		str_p++;
	}
	return 0;
}

static bool is_num_in_hex_format(const char *str_p,
				char **cur_read)
{
	bool is_hex = false;

	if (NULL == cur_read)
		return false;

	/*search for '0x' pattern in the product info,
	* * if found that means teh number stored is in teh HEX format'*/
	*cur_read = (uint8_t *)strstr(
				(const char *)str_p,
				(const char *)"0x");

	is_hex = (*cur_read != NULL) ? true : false;
	return is_hex;
}
