#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"
#include "scr_protocol_t1.h"
#include "scr_utils.h"
#include "stm_asc_scr.h"

#define SCR_HEADER_SIZE 3
#define SCR_DATA_SIZE 254
#define SCR_EPILOGUE_MAX_SIZE 2
#define SCR_BLOCK_SIZE SCR_HEADER_SIZE + SCR_DATA_SIZE + SCR_EPILOGUE_MAX_SIZE
#define SCR_NAD_INDEX 0
#define SCR_PCB_INDEX 1
#define SCR_LEN_INDEX 2
#define SCR_DATA_INDEX 3
#define SCR_EPILOGUE_CRC_SIZE 2
#define SCR_EPILOGUE_LRC_SIZE 1

void scr_initialise_t1_block(scr_ctrl_t *scr_handle_p,
		stm_scr_transfer_params_t  *trans_params_p,
		scr_t1_t *t1_block);

void scr_send_next_block(scr_t1_t *t1_block)
{
	t1_block->our_sequence = !t1_block->our_sequence;
}
void scr_get_next_block(scr_t1_t *t1_block)
{
	t1_block->their_sequence = !t1_block->their_sequence;
}
uint32_t scr_get_block_type(uint8_t pcb)
{
	return (pcb & SCR_BLOCK_TYPE_BIT);
}
void scr_print_t1_parameters(scr_t1_t t1_block)
{
	scr_debug_print("T1 protocol parameters\n");
	if (t1_block.trans_params_p) {
		scr_debug_print(" bytes to read  %d\n",
			t1_block.trans_params_p->response_buffer_size);
		scr_debug_print(" bytes to write %d\n",
			t1_block.trans_params_p->buffer_size);
	}
	scr_debug_print(" bytes read till now    %d\n",
			t1_block.bytes_read);
	scr_debug_print(" bytes writen till now  %d\n",
			t1_block.bytes_written);
	scr_debug_print(" bytes last read        %d\n",
			t1_block.bytes_last_read);
	scr_debug_print(" bytes last written     %d\n",
			t1_block.bytes_last_written);
	scr_debug_print(" RC error code          %d\n",
			t1_block.rc);
	scr_debug_print(" NAD                    %d\n",
			t1_block.nad);
	scr_debug_print(" ifsc                   %d\n",
			t1_block.ifsc);
	scr_debug_print(" our sequence           %d\n",
			t1_block.our_sequence);
	scr_debug_print(" their sequence         %d\n",
			t1_block.their_sequence);
	scr_debug_print(" next transfer time     %d\n",
			t1_block.next_transfer_time);
	scr_debug_print(" block_waiting_timeout  %d\n",
			t1_block.timeout_t1.block_waiting_timeout);
	scr_debug_print(" char_waiting_timeout   %d\n",
			t1_block.timeout_t1.character_waiting_timeout);
	scr_debug_print(" block_guard_timeout    %d\n",
			t1_block.timeout_t1.block_guard_timeout);
	scr_debug_print(" block_multiplier       %d\n",
			t1_block.timeout_t1.block_multiplier);

}

uint32_t scr_t1_block_write(scr_t1_t *t1_block,
			uint8_t *buffer_to_write_p,
			uint32_t size_of_buffer,
			uint32_t size_to_write,
			uint32_t timeout)
{
	int32_t delay;
	uint32_t error_code = 0;
	ca_os_time_t t1 = 0;
	scr_debug_print("<%s>:<%d>[API in ]\n", __func__, __LINE__);

	delay = t1_block->next_transfer_time -
			ca_os_get_time_in_milli_sec();
	if (delay > 0) {
		/* check for timer wrap around */
		if (delay > (t1_block->timeout_t1.block_guard_timeout))
			delay = t1_block->timeout_t1.block_guard_timeout +
				ca_os_get_time_in_milli_sec();

		scr_debug_print("<%s>:<%d> delay %d\n", __func__, __LINE__,
				delay);
		ca_os_sleep_milli_sec(delay);
	}
	/* The wait till CWT time before resending an I Block */
	t1 = ca_os_get_time_in_milli_sec();

	error_code = scr_asc_write(t1_block->scr_handle_p,
				buffer_to_write_p,
				size_of_buffer,
				size_to_write, timeout);
	if ((signed int)error_code < 0) {
		scr_error_print(" <%s>::<%d> Failed %d\n",
				__func__,
				__LINE__,
				error_code);
	}

	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

uint32_t scr_read_block(scr_t1_t *t1_block, scr_t1_block_t *block)
{
	uint32_t error_code = 0;
	uint32_t byte_read = 0, rc_bytes = 0;
	int32_t CumulativeCWT = 0;
	scr_debug_print("<%s>:<%d>[API in ]\n", __func__, __LINE__);
	byte_read = scr_asc_read(t1_block->scr_handle_p,
				(uint8_t *)&block->header.nad,
				1,
				1,
				(t1_block->timeout_t1.block_waiting_timeout *
				t1_block->timeout_t1.block_multiplier) +
				t1_block->timeout_t1.character_waiting_timeout
				+ 1,
				false);

	if ((signed int)byte_read < 0) {
		scr_error_print("<%s>:<%d>reading nad failed\n",
				__func__,
				__LINE__);
		error_code = byte_read;
		goto error;
	}

	scr_debug_print("<%s>:<%d> header read  nad %x\n",
			__func__,
			__LINE__,
			block->header.nad);
	byte_read = scr_asc_read(
		t1_block->scr_handle_p,
		(uint8_t *)&block->header.pcb,
		1,
		1,
		(t1_block->timeout_t1.character_waiting_timeout) + 1,
		false);

	if ((signed int)byte_read < 0) {
		scr_error_print("<%s>:<%d> reading pcb failed\n",
				__func__,
				__LINE__);
		error_code = byte_read;
		goto error;
	}
	scr_debug_print("<%s>:<%d> header read  pcb %x\n",
		__func__,
		__LINE__,
		block->header.pcb);
	byte_read = scr_asc_read(t1_block->scr_handle_p,
			(uint8_t *)&block->header.len,
			1,
			1,
			(t1_block->timeout_t1.character_waiting_timeout) + 1,
			false);


	if ((signed int)byte_read < 0) {
		scr_error_print("<%s>:<%d> reading  len failed\n",
				__func__,
				__LINE__);
		error_code = byte_read;
		goto error;
	}
	scr_debug_print("<%s>:<%d> header read  len %x\n",
			__func__,
			__LINE__,
			block->header.len);

	scr_debug_print("<%s>:<%d> header read  nad %x pcb %x len %x\n",
			__func__,
			__LINE__,
			block->header.nad,
			block->header.pcb,
			block->header.len);

	CumulativeCWT = (1000 *
		t1_block->scr_handle_p->scr_caps.character_waiting_time *
		block->header.len) /
		t1_block->scr_handle_p->scr_caps.baud_rate;

	if (block->header.len) {
		byte_read = scr_asc_read(t1_block->scr_handle_p,
			block->data,
			SCR_DATA_SIZE,
			block->header.len,
			CumulativeCWT + 2 + 1,
			false);
		if ((signed int)byte_read < 0) {
			scr_error_print("<%s>:<%d> reading data failed\n",
				__func__,
				__LINE__);
			error_code = byte_read;
			goto error;
		}
	}
	scr_debug_print("<%s>:<%d> read data size %d\n",
			__func__,
			__LINE__,
			byte_read);

	t1_block->bytes_last_read = byte_read;
	if (t1_block->rc)
		rc_bytes = SCR_EPILOGUE_CRC_SIZE;
	else
		rc_bytes = SCR_EPILOGUE_LRC_SIZE;

	byte_read = scr_asc_read(t1_block->scr_handle_p,
		(uint8_t *)&block->scr_epilogue,
		SCR_EPILOGUE_MAX_SIZE,
		rc_bytes,
		(t1_block->timeout_t1.character_waiting_timeout * rc_bytes) + 1,
		false);
	if ((signed int)byte_read < 0) {
		scr_error_print("<%s>:<%d> reading EPILOGUE failed\n",
				__func__,
				__LINE__);
		error_code = byte_read;
		goto error;
	}

	scr_debug_print("<%s>:<%d> read epilogue\n", __func__, __LINE__);

	t1_block->next_transfer_time =
		ca_os_get_time_in_milli_sec() +
		t1_block->timeout_t1.block_guard_timeout;

	error:
	scr_debug_print("<%s>:<%d> [API out ]\n",
			__func__,
			__LINE__);
		return error_code;
}

static void scr_generate_crc(uint8_t *buffer_p, uint32_t count, uint16_t *crc_p)
{
	uint32_t i;
	*crc_p = 0;
	for (i = 0; i < count; i++) {
		*crc_p ^= buffer_p[i];
		*crc_p ^= (uint8_t)(*crc_p & 0xff) >> 4;
		*crc_p ^= (*crc_p << 8) << 4;
		*crc_p ^= ((*crc_p & 0xff) << 4) << 1;
	}
}

static void scr_generate_lrc(uint8_t *buffer_p, uint32_t count, uint8_t *lrc_p)
{
	uint32_t i;
	*lrc_p = 0;
	for (i = 0; i < count; i++)
	{
		*lrc_p = *lrc_p ^ buffer_p[i];
	}
}

static void scr_generate_epilogue(uint8_t *buffer_p,
			uint32_t count,
			uint32_t rc,
			scr_epilogue_t *scr_epilogue)
{
	if (rc) { /* CRC */
		scr_debug_print("< %s> :: <%d>Genetaring CRC\n",
				__func__,
				__LINE__);
		scr_generate_crc(buffer_p,
				count,
				(uint16_t *)&scr_epilogue->crc);
	} else { /* LRC */
		scr_debug_print("<%s> :: <%d>Genetaring LRC\n",
				__func__,
				__LINE__);
		scr_generate_lrc(buffer_p, count, &scr_epilogue->lrc);
	}
}

bool scr_check_epilogue(scr_t1_t *t1_block, scr_t1_block_t *block)
{
	scr_epilogue_t scr_epilogue_header, scr_epilogue_data, scr_epilogue;
	uint8_t data[4];
	scr_generate_epilogue((uint8_t *)&block->header,
			SCR_HEADER_SIZE,
			t1_block->rc,
			&scr_epilogue_header);

	scr_generate_epilogue((uint8_t *)block->data,
			block->header.len,
			t1_block->rc,
			&scr_epilogue_data);
	if (t1_block->rc) {
		data[0] = scr_epilogue_header.crc[0];
		data[1] = scr_epilogue_header.crc[1];
		data[2] = scr_epilogue_data.crc[0];
		data[3] = scr_epilogue_data.crc[1];
		scr_generate_epilogue(data, 4, t1_block->rc, &scr_epilogue);
		if (scr_epilogue.crc == block->scr_epilogue.crc)
			return true;
	} else {
		data[0] = scr_epilogue_header.lrc;
		data[1] = scr_epilogue_data.lrc;
		scr_generate_epilogue(data, 2, t1_block->rc, &scr_epilogue);
		if (scr_epilogue.lrc == block->scr_epilogue.lrc)
			return true;
	}
	return false;
}

uint32_t scr_send_r_block(scr_t1_t *t1_block,
			uint8_t sequence,
			uint8_t r_block_error)
{
	scr_epilogue_t scr_epilogue;
	unsigned int	timeout_ms = 0;
	uint32_t timeout = 0;
	uint32_t work_waiting_time =
		t1_block->scr_handle_p->scr_caps.work_waiting_time;

	uint32_t error_code = 0;
	uint32_t size = 0;
	uint8_t buffer[SCR_BLOCK_SIZE];
	scr_debug_print("<%s>:<%d>[API in ] seq -> %d\n",
			__func__,
			__LINE__,
			sequence);

	if(t1_block->trans_params_p != NULL)
		timeout_ms = t1_block->trans_params_p->timeout_ms;

	timeout = SCR_TX_TIMEOUT(work_waiting_time,
			t1_block->scr_handle_p->N,
			t1_block->scr_handle_p->scr_caps.baud_rate,
			timeout_ms);

	buffer[SCR_NAD_INDEX] = t1_block->nad;
	buffer[SCR_PCB_INDEX] = SCR_R_BLOCK | r_block_error | (sequence << 4);
	buffer[SCR_LEN_INDEX] = 0;
	scr_generate_epilogue(buffer, 3, t1_block->rc, &scr_epilogue);
	if (t1_block->rc) {
		size = 5;
		buffer[3] = scr_epilogue.crc[1];
		buffer[4] = scr_epilogue.crc[0];
	} else {
		size = 4;
		buffer[3] = scr_epilogue.lrc;
	}
	error_code = scr_t1_block_write(t1_block,
			buffer,
			SCR_BLOCK_SIZE,
			size,
			timeout * size);
	if ((signed int)error_code < 0) {
		scr_debug_print(" <%s> :: <%d>\n", __func__, __LINE__);
		scr_debug_print(" NAD = %x PCB = %x RC = %d\n",
			buffer[SCR_NAD_INDEX],
			buffer[SCR_PCB_INDEX],
			t1_block->rc);
	}

	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

uint32_t scr_send_i_block(scr_t1_t *t1_block, uint8_t *block_buffer)
{
	scr_epilogue_t scr_epilogue;
	uint32_t size = 0, data_written = 0;
	uint32_t work_waiting_time =
		t1_block->scr_handle_p->scr_caps.work_waiting_time;
	uint32_t timeout = 0;
	unsigned int	timeout_ms = 0;

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	if(t1_block->trans_params_p == NULL)
		return -EINVAL;
	timeout_ms = t1_block->trans_params_p->timeout_ms;

	timeout = SCR_TX_TIMEOUT(work_waiting_time,
				t1_block->scr_handle_p->N,
				t1_block->scr_handle_p->scr_caps.baud_rate,
				timeout_ms);

	block_buffer[SCR_NAD_INDEX] = t1_block->nad;
	block_buffer[SCR_PCB_INDEX] = SCR_I_BLOCK_1 |
			(t1_block->our_sequence << 6);

	if (t1_block->ifsc <
	(t1_block->trans_params_p->buffer_size -
	t1_block->bytes_written)) {
		block_buffer[SCR_PCB_INDEX] |= SCR_I_CHAINING_BIT;
		block_buffer[SCR_LEN_INDEX] = t1_block->ifsc;
	} else {
		block_buffer[SCR_PCB_INDEX] &= (~SCR_I_CHAINING_BIT);
		block_buffer[SCR_LEN_INDEX] =
			(t1_block->trans_params_p->buffer_size -
			t1_block->bytes_written);
	}
	memcpy(&block_buffer[SCR_DATA_INDEX],
		t1_block->trans_params_p->buffer_p + t1_block->bytes_written,
		block_buffer[SCR_LEN_INDEX]);

	scr_generate_epilogue(block_buffer,
		block_buffer[SCR_LEN_INDEX] + SCR_HEADER_SIZE,
		t1_block->rc,
		&scr_epilogue);

	size = block_buffer[SCR_LEN_INDEX] + SCR_HEADER_SIZE;
	if (t1_block->rc) {
		memcpy(&block_buffer[size],
			(char *)&scr_epilogue,
			SCR_EPILOGUE_CRC_SIZE);
		size += SCR_EPILOGUE_CRC_SIZE;
	} else {
		memcpy(&block_buffer[size],
			(char *)&scr_epilogue,
			SCR_EPILOGUE_LRC_SIZE);
		size += SCR_EPILOGUE_LRC_SIZE;
	}

	scr_debug_print("<%s>:<%d> sending I block\n"\
			"NAD %d\n"\
			"sequence %d\n"\
			"chaining %d\n"\
			"size %d\n"\
			"timeout %d\n",
			__func__,
			__LINE__,
			t1_block->nad,
			t1_block->our_sequence ,
			block_buffer[SCR_PCB_INDEX] & SCR_I_CHAINING_BIT,
			size,
			timeout);

	data_written = scr_t1_block_write(t1_block,
		block_buffer,
		SCR_BLOCK_SIZE,
		size,
		timeout );

	if ((signed int)data_written < 0) {
		scr_error_print("<%s>:<%d>[API out] I block sending failed\n",
			__func__,
			__LINE__);
		return data_written;
	}
	t1_block->bytes_last_written = block_buffer[SCR_LEN_INDEX];

	scr_debug_print("<%s>:<%d>[API out]\n", __func__, __LINE__);
	return 0;
}
uint32_t scr_resync(scr_t1_t *t1_block)
{
	uint8_t write_block_buffer[SCR_BLOCK_SIZE];
	uint8_t read_block_buffer[SCR_BLOCK_SIZE];
	uint32_t error_code = 0;
	scr_t1_block_t block;
	bool valid = false;
	uint32_t retries = 3;
	uint32_t work_waiting_time =
		t1_block->scr_handle_p->scr_caps.work_waiting_time;
	unsigned int	timeout_ms = 0;
	uint32_t tx_timeout = 0;

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	if(t1_block->trans_params_p != NULL)
		timeout_ms = t1_block->trans_params_p->timeout_ms;

	tx_timeout = SCR_TX_TIMEOUT(work_waiting_time,
		t1_block->scr_handle_p->N,
		t1_block->scr_handle_p->scr_caps.baud_rate,
		timeout_ms);

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	write_block_buffer[SCR_NAD_INDEX] = t1_block->scr_handle_p->nad;
	write_block_buffer[SCR_PCB_INDEX] = SCR_S_BLOCK | SCR_S_RESYNCH_REQUEST;
	write_block_buffer[SCR_LEN_INDEX] = 0;
	scr_generate_epilogue(write_block_buffer,
		3,
		t1_block->scr_handle_p->atr_response.parsed_data.rc,
		(scr_epilogue_t *)&write_block_buffer[SCR_LEN_INDEX + 1]);
	do {
		scr_debug_print("<%s>:<%d> Doing resync for %d try\n",
			__func__,
			__LINE__,
			4-retries);

		error_code = scr_t1_block_write(
			t1_block,
			write_block_buffer,
			SCR_BLOCK_SIZE,
			t1_block->rc ? 5 : 3,
			tx_timeout);
		if ((signed int)error_code < 0) {
			scr_error_print(" < %s> :: <%d> Writing Resync"\
			"failed Failed %d\n",
			__func__,
			__LINE__,
			error_code);
			goto error;
		}
		memset(read_block_buffer, 0, SCR_BLOCK_SIZE);
		error_code = scr_read_block(t1_block, &block);
		if (error_code == 0) {
			if (scr_check_epilogue(t1_block, &block)) {
				valid = true;
				break;
			}
		}
	} while (retries--);
	if (valid == false) {
		scr_error_print(" < %s> :: <%d> Resync failed Failed\n",
				__func__,
				__LINE__);
		error_code = -EINVAL;
	}
	t1_block->our_sequence = 0;
	t1_block->their_sequence = 0;
	t1_block->first_block = true;

error:
		scr_debug_print("<%s>:<%d> [API out ]\n",
				__func__,
				__LINE__);
		return error_code;
}

uint32_t scr_send_s_block(scr_t1_t *t1_block,
		uint8_t s_block_request_type,
		uint8_t data)
{
	uint8_t block_buffer[SCR_BLOCK_SIZE];
	scr_epilogue_t scr_epilogue;
	uint32_t error_code = 0, size = SCR_HEADER_SIZE;
	uint32_t work_waiting_time =
		t1_block->scr_handle_p->scr_caps.work_waiting_time;
	unsigned int	timeout_ms = 0;
	uint32_t timeout = 0;

	if(t1_block->trans_params_p != NULL)
		timeout_ms = t1_block->trans_params_p->timeout_ms;

	timeout = SCR_TX_TIMEOUT(work_waiting_time,
				t1_block->scr_handle_p->N,
				t1_block->scr_handle_p->scr_caps.baud_rate,
				timeout_ms);

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	block_buffer[SCR_NAD_INDEX] = t1_block->nad;
	block_buffer[SCR_PCB_INDEX] = SCR_S_BLOCK | s_block_request_type;

	t1_block->pcb = block_buffer[SCR_PCB_INDEX];

	if ((SCR_S_IFS_REQUEST == s_block_request_type) ||
		(SCR_S_WTX_REQUEST == s_block_request_type)) {
		block_buffer[SCR_LEN_INDEX] = 1;
		memcpy(&block_buffer[SCR_DATA_INDEX], &data, 1);
		size += 1;
	} else
		block_buffer[SCR_LEN_INDEX] = 0;

	scr_generate_epilogue(block_buffer, size, t1_block->rc, &scr_epilogue);

	if (t1_block->rc) {
		memcpy(&block_buffer[size],
			&scr_epilogue,
			SCR_EPILOGUE_CRC_SIZE);
		size += SCR_EPILOGUE_CRC_SIZE;
	} else {
		memcpy(&block_buffer[size],
			&scr_epilogue,
			SCR_EPILOGUE_LRC_SIZE);
		size += SCR_EPILOGUE_LRC_SIZE;
	}
	error_code = scr_t1_block_write(t1_block,
			block_buffer,
			SCR_BLOCK_SIZE,
			size,
			timeout);
	if ((signed int)error_code < 0) {
		scr_error_print(" <%s> : <%d>sending S block failed\n",
			__func__, __LINE__);
	}
	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

uint32_t scr_process_s_block(scr_t1_t *t1_block,
	scr_t1_block_t *block_read,
	bool *transmit_done)
{
	uint32_t error_code = 0;
	uint8_t block_buffer[SCR_BLOCK_SIZE];
	scr_t1_block_t block;
	scr_epilogue_t  scr_epilogue;
	bool abort_done = true;
	uint32_t size = 0;
	uint32_t tx_count = 0;
	uint32_t work_waiting_time =
		t1_block->scr_handle_p->scr_caps.work_waiting_time;

	unsigned int	timeout_ms = 0;
	uint32_t timeout = 0;

	if(t1_block->trans_params_p != NULL)
		timeout_ms = t1_block->trans_params_p->timeout_ms;

	timeout = SCR_TX_TIMEOUT(work_waiting_time,
		t1_block->scr_handle_p->N,
		t1_block->scr_handle_p->scr_caps.baud_rate,
		timeout_ms);

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);
	scr_debug_print(" s block for %d\n", block_read->header.pcb & 0x3F);

	switch (block_read->header.pcb & 0x3F) {
	case SCR_S_RESYNCH_REQUEST:
	break;
	case SCR_S_IFS_REQUEST:
		*transmit_done = false;
		t1_block->ifsc = block_read->data[0];
	break;
	case SCR_S_ABORT_REQUEST:
		*transmit_done = true;
		scr_asc_abort(t1_block->scr_handle_p);
	break;
	case SCR_S_WTX_REQUEST:
		*transmit_done = false;
		t1_block->timeout_t1.block_multiplier = block_read->data[0];
	break;
	case SCR_S_RESYNCH_RESPONSE:
	break;
	case SCR_S_IFS_RESPONSE:
	break;
	case SCR_S_ABORT_RESPONSE:
	break;
	case SCR_S_WTX_RESPONSE:
	break;
	default:
		error_code = -EINVAL;
	break;
	}
	block_read->header.pcb = block_read->header.pcb | SCR_S_RESPONSE;
	memcpy(block_buffer, (uint8_t *)&block_read->header, 3);
	scr_generate_epilogue(block_buffer, 3, t1_block->rc, &scr_epilogue);
	if (t1_block->rc) {
		size = 5;
		memcpy(&block_buffer[3], scr_epilogue.crc, 2);
	} else {
		size = 4;
		memcpy(&block_buffer[3], scr_epilogue.crc, 1);
	}
	error_code = scr_t1_block_write(t1_block,
			block_buffer,
			SCR_BLOCK_SIZE,
			size,
			timeout);
	if (error_code)
		return error_code;

	if ((block_read->header.pcb & 0x3F) == SCR_S_ABORT_RESPONSE) {
		scr_debug_print("<%s>:<%d> handle abort response "\
			"SCR_S_ABORT_RESPONSE\n",
			__func__,
			__LINE__);
		do {
			bool s_block = true ;
			error_code = scr_read_block(t1_block, &block);
			if (error_code)
				scr_debug_print("<%s>:<%d> reading s response"\
				"failed for SCR_S_ABORT_RESPONSE\n",
				__func__,
				__LINE__);
			else
				s_block = (block.header.pcb & 0x0c) ?
					true : false;
			if ((s_block == false) || ((s_block == true)
				&& (block.header.pcb & SCR_S_ABORT_RESPONSE)))
				abort_done = false;
			else {
				tx_count++;
				if (tx_count >= 3) {
					error_code = scr_resync(t1_block);
					if (error_code)
						return error_code;
					abort_done = false;
				} else {
					scr_debug_print("<%s>:<%d> "\
					"sending S block for "\
					"SCR_S_ABORT_RESPONSE for %d try\n",
					__func__,
					__LINE__,
					tx_count);

					error_code =
						scr_t1_block_write(t1_block,
								block_buffer,
								SCR_BLOCK_SIZE,
								size,
								timeout);
					if (error_code)
						return error_code;
				}
			}
		} while (abort_done);
	}
	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

bool scr_process_r_block(scr_t1_t *t1_block, scr_t1_block_t *block_read)
{
	bool error_code;
	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	if ((block_read->header.pcb & (SCR_R_EDC_ERROR | SCR_R_OTHER_ERROR)) &&
		((block_read->header.pcb & 0x10) >> 4 !=
		t1_block->our_sequence)) {
		scr_debug_print("<%s>::<%d> get next block\n",
				__func__,
				__LINE__);
		scr_send_next_block(t1_block);
		error_code = true;
	} else {
		scr_debug_print("<%s>::<%d> get same block\n",
				__func__,
				__LINE__);
		error_code = false;
	}
	return error_code;
}
uint32_t scr_process_i_block(scr_t1_t *t1_block, scr_t1_block_t *block)
{
	uint32_t error_code = 0;
	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);
	if (((block->header.pcb & SCR_I_SEQUENCE_BIT) >> 0x6) ==
		t1_block->their_sequence){
		memcpy(t1_block->trans_params_p->response_p +
			t1_block->bytes_read,
			block->data,
			t1_block->bytes_last_read);
		t1_block->bytes_read += t1_block->bytes_last_read;

		if ((block->header.pcb & SCR_I_CHAINING_BIT) != 0) {

			scr_debug_print(" scr_get_next_block seq -> %d\n",
				t1_block->their_sequence);

			scr_get_next_block(t1_block);

			scr_send_r_block(t1_block, t1_block->their_sequence,
			SCR_R_NO_ERROR);
		} else {
			scr_debug_print("scr_get_next_block else seq -> %d\n",
				t1_block->their_sequence);

			scr_get_next_block(t1_block);
			error_code = ENODATA;
		}
	} else {
		scr_debug_print("sequence check failed seq -> %d\n",
				t1_block->their_sequence);
		scr_send_r_block(t1_block,
			t1_block->their_sequence,
			SCR_R_NO_ERROR);
	}
	scr_debug_print("<%s>:<%d> [API out]\n",
		__func__,
		__LINE__);
	return error_code;
}

uint32_t scr_recover_edc_error(scr_t1_t *t1_block, bool *first_block)
{
	uint32_t error_code = 0;
	int retries_count = 2;
	uint32_t block_type = 0xaa;
	scr_t1_block_t block;
	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	while (retries_count) {
		retries_count--;
		if (*first_block)
			error_code = scr_send_r_block(t1_block,
					0,
					SCR_R_EDC_ERROR);
		else
			error_code = scr_send_r_block(t1_block,
					t1_block->their_sequence,
					SCR_R_EDC_ERROR);
		if (error_code)
			return error_code;

		error_code = scr_read_block(t1_block, &block);
		if (error_code)
			continue;

		block_type = scr_get_block_type(block.header.pcb);
		if (block_type != SCR_R_BLOCK)
			continue;

		error_code = scr_check_epilogue(t1_block, &block);
		if (!error_code)
			continue;

		scr_process_r_block(t1_block, &block);
		return error_code;
	}
	if (!*first_block) {
		error_code = scr_resync(t1_block);
		if (error_code == 0)
			*first_block = true;

	}

	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

uint32_t scr_recover_timeout(scr_t1_t *t1_block, bool *first_block)
{
	uint32_t error_code = 0;
	int retries_count = 2;
	uint32_t block_type = 0xaa;
	scr_t1_block_t block;
	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	while (retries_count) {
		retries_count--;
		if (*first_block) {
			error_code = scr_send_r_block(t1_block,
					0,
					SCR_R_OTHER_ERROR);
		} else {
			error_code = scr_send_r_block(t1_block,
					t1_block->their_sequence,
					SCR_R_OTHER_ERROR);
		}

		if ((signed int)error_code < 0)
			return error_code;

		error_code = scr_read_block(t1_block, &block);
		if (error_code)
			continue;

		block_type = scr_get_block_type(block.header.pcb);

		error_code = scr_check_epilogue(t1_block, &block);
		if (!error_code)
			continue;

		switch (block_type) {
		case SCR_R_BLOCK:
			scr_process_r_block(t1_block, &block);
			break;
		case SCR_I_BLOCK_1 :
		case SCR_I_BLOCK_2 :
			error_code = scr_process_i_block(t1_block, &block);
			break;
		default:
			/* This should not occur */
			continue;
		}
		return error_code;
	}
	if (!*first_block) {
		error_code = scr_resync(t1_block);
		if (error_code == 0)
			*first_block = true;
	}
	scr_debug_print("<%s>:<%d> [API out ]\n", __func__, __LINE__);
	return error_code;
}

void scr_initialise_t1_block(scr_ctrl_t *scr_handle_p,
		stm_scr_transfer_params_t  *trans_params_p,
		scr_t1_t *t1_block)
{
	t1_block->trans_params_p = trans_params_p;
	t1_block->scr_handle_p = scr_handle_p;

	/* T=1 defaults */
	t1_block->ifsc = scr_handle_p->atr_response.parsed_data.ifsc;
	t1_block->rc = scr_handle_p->atr_response.parsed_data.rc;
	t1_block->nad = scr_handle_p->nad;
}


uint32_t scr_calculate_timeouts(scr_ctrl_t *scr_handle_p,
		scr_protocol_t1_timeout_t *timeout_t1)
{
	timeout_t1->block_guard_timeout =
			(((CA_OS_GET_TICKS_PER_SEC * (1000 / HZ)) *
			scr_handle_p->scr_caps.block_guard_time) /
			scr_handle_p->scr_caps.baud_rate) + 1; /* in ms */

	timeout_t1->character_waiting_timeout =
			(((CA_OS_GET_TICKS_PER_SEC * (1000 / HZ)) *
			scr_handle_p->scr_caps.character_waiting_time) /
			scr_handle_p->scr_caps.baud_rate) + 1; /* in ms */

	timeout_t1->block_waiting_timeout =
				scr_handle_p->scr_caps.block_waiting_time +
			(11*(CA_OS_GET_TICKS_PER_SEC * (1000 / HZ)))/
			scr_handle_p->scr_caps.baud_rate;

	timeout_t1->block_multiplier = 1;

	scr_debug_print("<%s>:<%d> BGT = %d   CWT=%d BWT=%d BaudRate = %d\n",
		__func__,
		__LINE__,
		timeout_t1->block_guard_timeout,
		timeout_t1->character_waiting_timeout,
		timeout_t1->block_waiting_timeout,
		scr_handle_p->scr_caps.baud_rate);

	if (!(timeout_t1->block_guard_timeout &&
		timeout_t1->block_waiting_timeout &&
			timeout_t1->character_waiting_timeout))
		return -EINVAL;

	return 0;
}

uint32_t scr_t1_read(scr_t1_t *t1_block, uint8_t *buffer)
{
	uint32_t error_code = 0;
	scr_t1_block_t block;
	bool  transmit_done;
	uint32_t block_type = 0xaa;
	bool last_tx_acked = false;

	t1_block->bytes_last_read = 0;
	t1_block->bytes_read = 0;


	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);
	do {
		error_code = scr_read_block(t1_block, &block);
		if ((signed int)error_code == -EIO) {
			scr_debug_print("<%s>:<%d>Timeout going for recovery\n",
				__func__, __LINE__);
			error_code = scr_recover_timeout(t1_block,
				&t1_block->first_block);
			if (error_code == ENODATA) {
				if (last_tx_acked == false) {
					scr_debug_print("<%s>:<%d> receiver I "\
					"block for last block send\n",
					__func__, __LINE__);

					last_tx_acked = true;
					scr_send_next_block(t1_block);
					t1_block->first_block = false;
				}
				return 0;
			} else if (error_code) {
				scr_error_print("<%s>:<%d> "\
				"Timeout recvry failed\n", __func__, __LINE__);
				return error_code;
			}
			continue;
		}
		if (!scr_check_epilogue(t1_block, &block)) {
			scr_debug_print("<%s>:<%d> Epilogue check failed going"\
			"for recovery\n", __func__, __LINE__);

			error_code = scr_recover_edc_error(t1_block,
					&t1_block->first_block);
			if (error_code) {
				scr_error_print("<%s>:<%d> Epilogue recovery"\
				"failed\n", __func__, __LINE__);

				return error_code;
			}
			continue;
		}
		block_type = scr_get_block_type(block.header.pcb);
		scr_debug_print("<%s>:<%d> correct block "\
			"recvd blk Type %d\n", __func__, __LINE__, block_type);
		switch (block_type) {
		case SCR_S_BLOCK:
			scr_process_s_block(t1_block, &block, &transmit_done);
			if (transmit_done == true)
				return -EINTR;
			continue;
		case SCR_R_BLOCK:
			scr_process_r_block(t1_block, &block);
			if (last_tx_acked == false)
				scr_send_i_block(t1_block, buffer);
			else {
				error_code = scr_send_r_block(t1_block,
						t1_block->their_sequence,
						SCR_R_OTHER_ERROR);
				if (error_code) {
					scr_error_print("<%s>:<%d>"\
					"SENDING R BLOCK failed\n",
					__func__, __LINE__);

					return error_code;
				}
			}
			continue;
		case SCR_I_BLOCK_1:
		case SCR_I_BLOCK_2:
			error_code = scr_process_i_block(t1_block, &block);
			if (last_tx_acked == false) {
				scr_debug_print("<%s>:<%d> receiver I block "\
				"for last block send\n", __func__, __LINE__);

				last_tx_acked = true;
				scr_send_next_block(t1_block);
			}
			if(error_code == ENODATA)
				return 0;
			break;
		}
		t1_block->first_block = false;
	} while (t1_block->bytes_read <
		t1_block->trans_params_p->response_buffer_size);

	scr_debug_print("<%s>:<%d> [API out]\n", __func__, __LINE__);
	return error_code;
}

uint32_t scr_t1_write(scr_t1_t *t1_block, uint8_t *block_buffer)
{
	uint32_t error_code = 0;
	uint32_t block_type = 0xaa;
	scr_t1_block_t block_read;
	bool transmit_done;
	scr_debug_print("<%s>:<%d> [API in]\n", __func__, __LINE__);
	t1_block->bytes_last_written = 0;
	t1_block->bytes_written = 0;

	error_code = scr_send_i_block(t1_block, block_buffer);
	if (error_code) {
		scr_error_print("<%s>:<%d> scr_send_i_block failed\n",
		__func__, __LINE__);
		goto error;
	}

	if ((t1_block->trans_params_p->buffer_size - t1_block->bytes_written)
		< t1_block->ifsc)
		return 0;

	while (t1_block->bytes_written <
			t1_block->trans_params_p->buffer_size) {
		error_code = scr_read_block(t1_block, &block_read);
		if ((signed int)error_code == -EIO) {
			scr_debug_print("<%s>:<%d>Timeout going for recovery\n",
				__func__, __LINE__);

			error_code = scr_recover_timeout(t1_block,
						&t1_block->first_block);
			if (error_code) {
				scr_error_print("<%s>:<%d>Timeout recovery"\
				"failed\n", __func__, __LINE__);

				goto error;
			}
			continue;
		}
		if (!scr_check_epilogue(t1_block, &block_read)) {
			scr_debug_print("<%s>:<%d> Epilogue check failed "\
			"going for recovery\n", __func__, __LINE__);

			error_code = scr_recover_edc_error(t1_block,
					&t1_block->first_block);
			if (error_code) {
				scr_error_print("<%s>:<%d> "\
				"Epilogue rcvry failed\n", __func__, __LINE__);
				goto error;
			}
			continue;
		}
		block_type = scr_get_block_type(block_read.header.pcb);

		scr_debug_print("<%s>:<%d> correct block"\
			"received block Type %d\n",
			__func__, __LINE__, block_type);

		switch (block_type) {
		case SCR_S_BLOCK:
			scr_process_s_block(t1_block,
				&block_read,
				&transmit_done);
			if (transmit_done == false)
				continue;
			break;
		case SCR_R_BLOCK:
			error_code = scr_process_r_block(t1_block, &block_read);
			if (!error_code)
				t1_block->bytes_written +=
					t1_block->bytes_last_written;

			break;
		case SCR_I_BLOCK_1:
		case SCR_I_BLOCK_2:
			scr_error_print("<%s>:<%d> some thing is "\
			"wrong, should not be here %d\n", __func__, __LINE__,
				block_type);
			break;
		}
		t1_block->first_block = false;
		error_code = scr_send_i_block(t1_block, block_buffer);
		if (error_code)
			goto error;
		if (t1_block->trans_params_p->buffer_size -
			t1_block->bytes_written < t1_block->ifsc)
			break;
	}
	error:
		scr_debug_print("<%s>:<%d> [API out] %d\n",
				__func__, __LINE__, error_code);
		return error_code;
}

uint32_t scr_transfer_t1(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p)
{
	uint8_t block_buffer[SCR_BLOCK_SIZE];
	uint32_t error_code = 0;
	scr_t1_t *t1_block;
	bool nack = false;

	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	scr_debug_print("Transfer parameters\n");
	scr_debug_print(" bytes to read %d\n",
			trans_params_p->response_buffer_size);
	scr_debug_print(" bytes to write %d\n",
			trans_params_p->buffer_size);

	/* Variable setup */
        t1_block = &scr_handle_p->t1_block_details;

	scr_initialise_t1_block(scr_handle_p, trans_params_p, t1_block);

	error_code = scr_calculate_timeouts(scr_handle_p,
			&t1_block->timeout_t1);
	if (error_code) {
		scr_error_print("<%s> : <%d> time out calculation failed\n",
				__func__, __LINE__);

		goto error;
	}

	scr_print_t1_parameters(*t1_block);

	error_code =  stm_asc_scr_set_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_NACK,
			nack);
	if (error_code) {
		scr_error_print("<%s>:<%d> stm_asc_scr_set_control failed\n",
				__func__, __LINE__);

		goto error;
	}
	if ((signed int)scr_asc_flush(scr_handle_p)) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		error_code = -EIO;
		goto error;
	}

	scr_debug_print("<%s>:<%d> Start the T1 writing\n", __func__, __LINE__);
	error_code = scr_t1_write(t1_block, block_buffer);
	if (error_code) {
		scr_debug_print("<%s>:<%d> write failed\n", __func__, __LINE__);
		goto error;
	}
	scr_debug_print("<%s>:<%d> T1 writing done\n", __func__, __LINE__);
	scr_debug_print("<%s>:<%d> T1 reading started\n", __func__, __LINE__);

	error_code = scr_t1_read(t1_block, block_buffer);
	if (error_code) {
		scr_error_print("<%s>:<%d>scr_t1_read failed\n",
				__func__, __LINE__);
		goto error;
	}
	trans_params_p->actual_response_size = t1_block->bytes_read;

	scr_debug_print("<%s>:<%d> T1 reading done\n",
				__func__, __LINE__);
	error:

	scr_debug_print("<%s>:<%d> [API out] %d size %d\n",
				__func__, __LINE__, error_code,
				trans_params_p->actual_response_size);
	return error_code;
}

uint32_t  scr_transfer_sblock(scr_ctrl_t *scr_handle_p,
		uint8_t type, uint8_t data)
{
	scr_t1_t t1_block;
	int32_t error_code = 0;
	bool nack = false;
	scr_t1_block_t block;
	memset(&t1_block, 0, sizeof(scr_t1_t));
	scr_debug_print("<%s>:<%d> [API in ]\n", __func__, __LINE__);

	scr_initialise_t1_block(scr_handle_p, NULL, &t1_block);

	error_code = scr_calculate_timeouts(scr_handle_p, &t1_block.timeout_t1);
	if (error_code) {
		scr_error_print("<%s> : <%d> timeout calculation failed\n",
				__func__, __LINE__);
		goto error;
	}

	scr_print_t1_parameters(t1_block);

	error_code =  stm_asc_scr_set_control(scr_handle_p->asc_handle,
				STM_ASC_SCR_NACK, nack);
	if (error_code != CA_NO_ERROR)
		goto error;

	if ((signed int)scr_asc_flush(scr_handle_p)) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		error_code = -EIO;
		goto error;
	}
	error_code = scr_send_s_block(&t1_block, type, data);
	if (error_code < 0) {
		scr_error_print(" <%s> : <%d>  sending S block failed\n",
				__func__, __LINE__);
		goto error;
	}

	error_code = scr_read_block(&t1_block, &block);
	if (error_code < 0) {
		scr_error_print("<%s>:<%d> reading S block response failed\n",
				__func__, __LINE__);
		goto error;
	}

	if (block.header.pcb != (t1_block.pcb | SCR_S_RESPONSE)) {
		error_code = -EINVAL;
		goto error;
	}
	scr_handle_p->scr_caps.ifsc = block.data[0];
	error:
	/* Should we do the recovery for timeout and epilogue error */
	scr_debug_print("<%s>:<%d> [API out] %d\n",
			__func__, __LINE__, error_code);
	return 0;
}
