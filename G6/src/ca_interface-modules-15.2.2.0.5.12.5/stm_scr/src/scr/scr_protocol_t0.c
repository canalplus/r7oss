#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"
#include "scr_utils.h"
#include "scr_protocol_t0.h"
#include "stm_asc_scr.h"

#define SCR_T0_CMD_LENGTH           5

#define SCR_T0_CLA_OFFSET           0
#define SCR_T0_INS_OFFSET           1
#define SCR_T0_P1_OFFSET            2
#define SCR_T0_P2_OFFSET            3
#define SCR_T0_P3_OFFSET            4
#define SCR_C5_INDEX                SCR_T0_P3_OFFSET
#define SCR_C6_INDEX                5
#define SCR_C7_INDEX                6

/* to be enabled when using DALTS */
#define ENABLE_DALTS_PATCH 1

typedef enum {
	SCR_CASE1 = 0,
	SCR_CASE2S,
	SCR_CASE3S,
	SCR_CASE4S,
	SCR_CASE2E,
	SCR_CASE3E,
	SCR_CASE4E
} scr_command_type_t;


typedef enum {
	SCR_INVALID_BYTE =0,
	SCR_ACK_XOR_BYTE,
	SCR_ACK_BYTE,
	SCR_SW1_BYTE,
	SCR_NULL_BYTE
} scr_procedure_byte_t;

static uint32_t scr_process_procedure_byte(scr_ctrl_t *scr_p,
			const uint8_t *buffer_to_write_p,
			uint32_t size_to_write,
			scr_command_response_t *response_p,
			uint32_t size_to_read,
			uint32_t timeout);

static uint32_t scr_procedure_byte(uint8_t procedure_byte,
			uint8_t ins_byte);

static uint32_t scr_process_response_of_4E(scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t data_to_read,
			uint32_t timeout);

static uint32_t scr_t0_case_1(scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint8_t write_buffer[SCR_T0_CMD_LENGTH];
	uint32_t tx_timeout = 0;
	uint32_t error_code = 0;

	scr_debug_print(" <%s>:: <%d> [API in]\n", __func__, __LINE__);

	memcpy(write_buffer, command_p, SCR_T0_CMD_LENGTH-1);

	write_buffer[SCR_T0_P3_OFFSET] = 0x00;

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
		scr_p->N,
		scr_p->scr_caps.baud_rate,
		timeout);

	scr_debug_print(" <%s>:: <%d> timeout %d\n",
			__func__,
			__LINE__,
			tx_timeout);
	error_code=scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	scr_debug_print(" <%s>:: <%d>\n", __func__, __LINE__);
	error_code = scr_asc_write(
		scr_p,
		write_buffer,
		SCR_T0_CMD_LENGTH,
		SCR_T0_CMD_LENGTH,
		tx_timeout);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_process_procedure_byte(scr_p,
		command_p,
		write_buffer[SCR_T0_P3_OFFSET],/*0x00*/  /* Noting to write */
		response_p,
		write_buffer[SCR_T0_P3_OFFSET],/*0x00*/
		timeout);/* Nothing to read*/

	error:
		scr_debug_print(" <%s>:: <%d> [API out] %d\n",
			__func__,
			__LINE__,
			error_code);
		return error_code;
}

static uint32_t scr_t0_case_2s(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;
	uint8_t write_buffer[SCR_T0_CMD_LENGTH];
	uint32_t data_to_read;
	scr_debug_print(" <%s>:: <%d> [API in]\n",
			__func__,
			__LINE__);
	data_to_read = command_p[SCR_C5_INDEX];
	if (!data_to_read)/* if C5 is not zero */
		data_to_read = 256;

	memcpy(write_buffer, command_p, SCR_T0_CMD_LENGTH);

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
		scr_p->N,
		scr_p->scr_caps.baud_rate,
		timeout);

	scr_debug_print("<%s>:: <%d> timeout %d\n",
			__func__,
			__LINE__,
			tx_timeout);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_asc_write(scr_p,
		write_buffer,
		SCR_T0_CMD_LENGTH,
		SCR_T0_CMD_LENGTH,
		tx_timeout);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_process_procedure_byte(scr_p,
		command_p,
		0x00                 /*nothing to write*/,
		response_p,
		data_to_read, /* Read byte describe by C5_INDEX above */
		timeout);

	if (error_code)
		goto error;

	scr_debug_print("<%s>:: <%d> SW1 %x SW2 %x\n", __func__, __LINE__,
			response_p->response_data[SCR_SW1_INDEX],
			response_p->response_data[SCR_SW2_INDEX]);
#ifndef ENABLE_DALTS_PATCH

	if (0x6C == response_p->response_data[SCR_SW1_INDEX]) {

		write_buffer[SCR_T0_P3_OFFSET] =
			response_p->response_data[SCR_SW2_INDEX];
		data_to_read = response_p->response_data[SCR_SW2_INDEX];
		if (!data_to_read)
			data_to_read = 256;

/* if Ne > Na then read only the Na bytes
	but command will be send for Ne
	remaining data will be flushed
	from the uart FIFO is the next flush*/

		error_code = scr_asc_write(scr_p,
			write_buffer,
			SCR_T0_CMD_LENGTH,
			SCR_T0_CMD_LENGTH,
			tx_timeout);
		if ((signed int)error_code < 0)
			goto error;

		error_code = scr_process_procedure_byte(scr_p,
			command_p,
			0x00,                       /* Nothing to write*/
			response_p,
			data_to_read,              /* Read the data described by SW2_INDEX above */
			timeout);
	}
#endif
	error:
		scr_debug_print("<%s>::<%d>[API out] error %d data size %d\n",
			__func__,
			__LINE__,
			error_code,
			response_p->size_of_data);
		return error_code;
}
static uint32_t scr_t0_case_3s(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
		scr_p->N,
		scr_p->scr_caps.baud_rate,
		timeout);

	scr_debug_print("<%s>:: <%d> [API in]\n", __func__, __LINE__);
	scr_debug_print("<%s>:: <%d> timeout %d\n",
			__func__,
			__LINE__,
			tx_timeout);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_asc_write(scr_p,
		command_p,
		SCR_T0_CMD_LENGTH,
		SCR_T0_CMD_LENGTH,
		tx_timeout);

	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_process_procedure_byte(scr_p,
		command_p,
		command_p[SCR_T0_P3_OFFSET],   /* Write data described by C5*/
		response_p,
		0x00,                         /* Nothing to read */
		timeout);

	error:
		scr_debug_print("<%s>::<%d> [API out] error %d data size %d\n",
				__func__,
				__LINE__,
				error_code,
				response_p->size_of_data);
		return error_code;
}

static uint32_t scr_t0_case_4s(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;
	uint8_t get_response_buffer[SCR_T0_CMD_LENGTH] = {
		0x00, 0xC0, 0x00, 0x00, 0x00};

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
		scr_p->N,
		scr_p->scr_caps.baud_rate,
		timeout);

	scr_debug_print("<%s>:: <%d> [API in]\n", __func__, __LINE__);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_asc_write(
		scr_p,
		command_p,
		SCR_T0_CMD_LENGTH,
		SCR_T0_CMD_LENGTH,
		tx_timeout);
	if ((signed int)error_code < 0)
		goto error;

	error_code = scr_process_procedure_byte(scr_p,
		command_p,
		command_p[SCR_T0_P3_OFFSET],
		response_p,
		0x00,
		timeout);
	if (error_code)
		goto error;

	if (0x61 == response_p->response_data[SCR_SW1_INDEX]) {
		/* Noting to be done for the ISO certification */
		get_response_buffer[SCR_T0_P3_OFFSET] =
		(command_p[command_p[SCR_C5_INDEX] + SCR_T0_CMD_LENGTH] <
		response_p->response_data[SCR_SW2_INDEX]) ?
		command_p[command_p[SCR_C5_INDEX] + SCR_T0_CMD_LENGTH] :
		response_p->response_data[SCR_SW2_INDEX];
		/* C(n) < Nx ? C(n): Nx*/

		/*some delay needed between previous receive and next write*/
		ca_os_sleep_milli_sec(1);

		error_code = scr_asc_write(
			scr_p,
			command_p,
			SCR_T0_CMD_LENGTH,
			SCR_T0_CMD_LENGTH,
			tx_timeout);
		if ((signed int)error_code < 0)
			goto error;

		error_code = scr_process_procedure_byte(scr_p,
			command_p,
			0x00,                                  /* Nothing to write */
			response_p,
			get_response_buffer[SCR_T0_P3_OFFSET],
			timeout); /* Read the bytes defined by the P3 index above*/
		if (error_code)
			goto error;

	} else if (0x62 == response_p->response_data[SCR_SW1_INDEX] ||
			0x63 == response_p->response_data[SCR_SW1_INDEX]) {
			/* Nothing to be done */
	} else if (0x9000 == ((response_p->response_data[SCR_SW1_INDEX] << 8) |
				response_p->response_data[SCR_SW2_INDEX])) {

		/*some delay needed between previous receive and next write*/
		ca_os_sleep_milli_sec(1);

		error_code = scr_asc_flush(scr_p);
		if ((signed int)error_code < 0)
			goto error;

		get_response_buffer[SCR_T0_P3_OFFSET] =
				command_p[command_p[SCR_C5_INDEX] +
				SCR_T0_CMD_LENGTH];/* C(n)*/

		scr_t0_case_2s(scr_p,
			get_response_buffer,
			response_p,
			status,
			timeout);
	} else if (0x60 == (response_p->response_data[SCR_SW1_INDEX] & 0xF0)) {
		/* Nothing to be done */
	} else if (0x90 == (response_p->response_data[SCR_SW1_INDEX] & 0xF0)) {
		/* Nothing to be done */
	}

	error:
		scr_debug_print("<%s>::<%d>[API out] error %d data size %d\n",
			__func__,
			__LINE__,
			error_code,
			response_p->size_of_data);

		return error_code;
}

static uint32_t scr_t0_case_2e(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;
	uint8_t write_buffer[SCR_T0_CMD_LENGTH];
	uint32_t data_to_read;
	uint8_t get_response_buffer[SCR_T0_CMD_LENGTH] = {
		0x00, 0xC0, 0x00, 0x00, 0x00};
	data_to_read = command_p[SCR_C6_INDEX] << 8 | command_p[SCR_C7_INDEX];

	scr_debug_print("<%s>::<%d> [API in] data to read %d\n",
		__func__,
		__LINE__,
		data_to_read);

	memcpy(write_buffer, command_p, SCR_T0_CMD_LENGTH-1);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	if ((data_to_read >= 0x0001) && (data_to_read <= 0x0100)) {
		write_buffer[SCR_T0_P3_OFFSET] = command_p[SCR_C7_INDEX];
		scr_t0_case_2s(scr_p,
		write_buffer,
		response_p,
		status,
		timeout);
	} else {
		write_buffer[SCR_T0_P3_OFFSET] = 0x00;
		tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
			scr_p->N,
			scr_p->scr_caps.baud_rate,
			timeout);

		scr_debug_print("<%s>:: <%d> timeout %d\n",
			__func__,
			__LINE__,
			tx_timeout);

		error_code = scr_asc_write(scr_p,
			write_buffer,
			SCR_T0_CMD_LENGTH,
			SCR_T0_CMD_LENGTH,
			tx_timeout);
		if ((signed int)error_code < 0)
			goto error;

		error_code = scr_process_procedure_byte(scr_p,
			command_p,
			write_buffer[SCR_T0_P3_OFFSET],
			response_p,
			256,
			timeout);
		if (error_code)
			goto error;

		scr_debug_print(" <%s>:: <%d> SW1--> %x\n",
			__func__,
			__LINE__,
			response_p->response_data[SCR_SW1_INDEX]);

		switch (response_p->response_data[SCR_SW1_INDEX]) {
			case 0x67:
				/* nothing to be done*/
			break;
#ifndef ENABLE_DALTS_PATCH
			case 0x6C:

				write_buffer[SCR_T0_P3_OFFSET] =
				response_p->response_data[SCR_SW2_INDEX];

				data_to_read =
				response_p->response_data[SCR_SW2_INDEX];

				if (!data_to_read)
					data_to_read = 256;

				if (data_to_read > command_p[SCR_C5_INDEX])
					data_to_read = command_p[SCR_C5_INDEX];
			/* if Ne > Na then read only the Na bytes
				but command will be send for Ne
				remaining data will be flushed
				from the uart FIFO is the next flush*/

				error_code = scr_asc_write(scr_p,
					write_buffer,
					SCR_T0_CMD_LENGTH,
					SCR_T0_CMD_LENGTH,
					tx_timeout);
				if ((signed int)error_code < 0)
					goto error;

				error_code = scr_process_procedure_byte(scr_p,
					command_p + SCR_T0_CMD_LENGTH,
					0x00,
					response_p,
					data_to_read,
					timeout);
				if (error_code)
					goto error;
			break;
#endif
			case 0x90:
				/* Nothing to done*/
			break;
			case 0x61:
			do {
				get_response_buffer[SCR_T0_P3_OFFSET] =
				response_p->response_data[SCR_SW2_INDEX] >
				data_to_read - response_p->size_of_data ?
				response_p->response_data[SCR_SW2_INDEX] :
				data_to_read - response_p->size_of_data;

				error_code = scr_asc_write(scr_p,
					get_response_buffer,
					SCR_T0_CMD_LENGTH,
					SCR_T0_CMD_LENGTH,
					tx_timeout);
				if ((signed int)error_code < 0)
					goto error;

				error_code = scr_process_procedure_byte(scr_p,
					command_p + SCR_T0_CMD_LENGTH,
					0x00,
					response_p,
					write_buffer[SCR_T0_P3_OFFSET],
					timeout);
				if (error_code)
					goto error;

			} while ((data_to_read - response_p->size_of_data) &&
			(response_p->response_data[SCR_SW2_INDEX] != 0x90));

			break;
			default:
			break;
		}

	}
	error:
		scr_debug_print("<%s>::<%d>[API out] error %d data size %d\n",
					__func__,
					__LINE__,
					error_code,
					response_p->size_of_data);
		return error_code;
}

static uint32_t scr_t0_case_3e(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;
	uint16_t trailer_bytes = 0;
	uint8_t write_buffer[265];
	uint8_t envelope_header[SCR_T0_CMD_LENGTH] = {
			0x00, 0xC2, 0x00, 0x00, 0x00};
	bool done = true;
	uint32_t write_count = 0;
	uint32_t data_to_write = command_p[SCR_C5_INDEX] << 16 |
			command_p[SCR_C6_INDEX] << 8 |
			command_p[SCR_C7_INDEX];

	scr_debug_print("<%s>:: <%d> [API in]\n", __func__, __LINE__);

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
				scr_p->N,
				scr_p->scr_caps.baud_rate,
				timeout);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	if (data_to_write <= 0xFF) {
		memcpy(write_buffer, command_p, SCR_T0_CMD_LENGTH - 1);

		write_buffer[SCR_T0_P3_OFFSET] = data_to_write;

		memcpy(&write_buffer[SCR_C6_INDEX],
			&command_p[SCR_C7_INDEX + 1],
			data_to_write);

		scr_debug_print("<%s>:: <%d>\n", __func__, __LINE__);

		error_code = scr_asc_write(scr_p,
			write_buffer,
			265,
			SCR_T0_CMD_LENGTH + data_to_write,
			timeout ? tx_timeout : tx_timeout * (SCR_T0_CMD_LENGTH + data_to_write));

		if ((signed int)error_code < 0)
			goto error;

		error_code = scr_process_procedure_byte(scr_p,
			command_p ,
			data_to_write, /* Data to write as described by the C7 above*/
			response_p,
			0x00,
			timeout);         /* Nothing to read */
		if (error_code)
			goto error;
	} else {
			memcpy(write_buffer,
				envelope_header,
				SCR_T0_CMD_LENGTH - 1);

			write_buffer[SCR_T0_P3_OFFSET] = 0xFF;

			scr_debug_print(" <%s>:: <%d>\n", __func__, __LINE__);

			do {
				error_code = scr_asc_write(scr_p,
					write_buffer,
					SCR_T0_CMD_LENGTH,
					SCR_T0_CMD_LENGTH,
					tx_timeout);
				if ((signed int)error_code < 0)
					goto error;

				error_code = scr_process_procedure_byte(scr_p,
					command_p + (write_count * 0xFF),
					write_buffer[SCR_T0_P3_OFFSET],
					response_p,
					0x00,
					timeout);
				if (error_code)
					goto error;

				trailer_bytes =
				response_p->response_data[SCR_SW1_INDEX] << 8 |
				response_p->response_data[SCR_SW2_INDEX];

				if (0x9000 == trailer_bytes) {
					write_count++;
	/*writing second last envelop command*/
					if ((write_count * 0xFF) >
						data_to_write)
						write_buffer[SCR_T0_P3_OFFSET] =
						(data_to_write -
						(write_count * 0xFF));
	/*writing envelop command with zero data i.e. last envelop command*/
					else if ((write_count * 0xFF) >
					(data_to_write + 0xff))
						write_buffer[SCR_T0_P3_OFFSET] =
								0x00;

					else
						write_buffer[SCR_T0_P3_OFFSET] =
								0xFF;
				} else
					done = false;
			} while (done);
	}
	error:
		scr_debug_print(" <%s>:: <%d> [API out] error %d\n",
			__func__,
			__LINE__,
			error_code);
		return error_code;
}

static uint32_t scr_t0_case_4e(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{

	uint32_t error_code=0,tx_timeout;
	uint8_t write_buffer[SCR_T0_CMD_LENGTH + 255];
	uint32_t data_to_write=0,data_to_read=0;
	uint16_t trailer_bytes=0;
	scr_debug_print(" <%s>:: <%d>  [API in]T0 Case no.\n",
			__func__,
			__LINE__);

	data_to_write = command_p[SCR_C5_INDEX] << 16 |
			command_p[SCR_C6_INDEX] << 8 |
			command_p[SCR_C7_INDEX];

	data_to_read = command_p[SCR_T0_CMD_LENGTH + 2 + data_to_write] << 8 ||
			command_p[SCR_T0_CMD_LENGTH + 2 + data_to_write + 1];

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
				scr_p->N,
				scr_p->scr_caps.baud_rate,
				timeout);

	error_code = scr_asc_flush(scr_p);
	if ((signed int)error_code < 0)
		goto error;

	if (data_to_write <= 0xFF) {
		/*4E.1*/
		memcpy(write_buffer, command_p, SCR_T0_CMD_LENGTH - 1);
		write_buffer[SCR_T0_P3_OFFSET] = data_to_write;
		/* copy data fron the C7 offset to the write buffer*/
		memcpy(write_buffer + SCR_T0_CMD_LENGTH,
			command_p + SCR_T0_CMD_LENGTH + 2,
			data_to_write);

		error_code = scr_asc_write(scr_p,
			write_buffer,
			SCR_T0_CMD_LENGTH,
			SCR_T0_CMD_LENGTH + data_to_write,
			timeout ? tx_timeout : tx_timeout * (SCR_T0_CMD_LENGTH + data_to_write));
		if ((signed int)error_code < 0)
			goto error;
	} else { /* 4E.2*/
		bool done = true;
		uint32_t write_count = 0;
		uint8_t envelope_header[SCR_T0_CMD_LENGTH] = {
			0x00, 0xC2, 0x00, 0x00, 0x00};

		memcpy(write_buffer, envelope_header, SCR_T0_CMD_LENGTH - 1);
		write_buffer[SCR_T0_P3_OFFSET] = 0xFF;

		scr_debug_print("<%s>::<%d>\n",
			__func__,
			__LINE__);
		/* 3E.2*/
		do {
			error_code = scr_asc_write(scr_p,
				write_buffer,
				SCR_T0_CMD_LENGTH,
				SCR_T0_CMD_LENGTH,
				tx_timeout);
			if ((signed int)error_code < 0)
				goto error;

			error_code = scr_process_procedure_byte(scr_p,
				command_p + (write_count * 0xFF),
				write_buffer[SCR_T0_P3_OFFSET],
				response_p,
				0x00,
				timeout);
			if (error_code)
				goto error;

			trailer_bytes =
				response_p->response_data[SCR_SW1_INDEX] << 8 |
				response_p->response_data[SCR_SW2_INDEX];

			if (0x9000 == trailer_bytes) {
				write_count++;
				if((write_count * 0xFF) > data_to_write)
					/*writing second last envelop command*/
					write_buffer[SCR_T0_P3_OFFSET] =
					(data_to_write - (write_count*0xFF));

				else if ((write_count * 0xFF) >
					(data_to_write + 0xff)) {
					/*writing envelop command with zero data
					i.e. last envelop command*/
					write_buffer[SCR_T0_P3_OFFSET] = 0x00;

				} else
					write_buffer[SCR_T0_P3_OFFSET] = 0xFF;
			} else
				done = false;
		} while (done);
	}

	/* Read the response fo the 4E */
	error_code = scr_process_response_of_4E(scr_p,
				command_p,
				response_p,
				status,
				data_to_read,
				timeout);
	if (error_code)
		goto error;

	error:
		scr_debug_print("<%s>:: <%d> [API out] error %d\n",
				__func__,
				__LINE__,
				error_code);
		return error_code;
}
static uint32_t scr_process_response_of_4E(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t data_to_read,
			uint32_t timeout)
{
	uint32_t error_code = 0, tx_timeout;
	uint16_t trailer_bytes = 0;

	uint8_t get_response_buffer[SCR_T0_CMD_LENGTH] = {
		0x00, 0xC0, 0x00, 0x00, 0x00};

	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
				scr_p->N,
				scr_p->scr_caps.baud_rate,
				timeout);

	trailer_bytes = response_p->response_data[SCR_SW1_INDEX] << 8 |
				response_p->response_data[SCR_SW2_INDEX];

	if (trailer_bytes == 0x9000) {
		/*4E.1 B*/
	} else if ((response_p->response_data[SCR_SW1_INDEX] > 0x63 &&
			response_p->response_data[SCR_SW1_INDEX] < 0x70) ||
			response_p->response_data[SCR_SW1_INDEX] == 0x60) {
		/* 4E.1 A */
		/* Nothing to be done*/
		/* Response TPDU is mapped directly to the APDU */
	} else if (response_p->response_data[SCR_SW1_INDEX] == 0x61) {
		/* 4E.1 C */
		do {
			get_response_buffer[SCR_T0_P3_OFFSET] =
				response_p->response_data[SCR_SW2_INDEX] >
				data_to_read - response_p->size_of_data ?
				response_p->response_data[SCR_SW2_INDEX] :
				data_to_read - response_p->size_of_data;

			error_code = scr_asc_write(scr_p,
				get_response_buffer,
				SCR_T0_CMD_LENGTH,
				SCR_T0_CMD_LENGTH,
				tx_timeout);
			if ((signed int)error_code < 0)
				goto error;

			error_code = scr_process_procedure_byte(scr_p,
				command_p+SCR_T0_CMD_LENGTH,
				0x00,
				response_p,
				get_response_buffer[SCR_T0_P3_OFFSET],
				timeout);
			if (error_code)
				goto error;
		} while ((data_to_read - response_p->size_of_data) &&
			(response_p->response_data[SCR_SW2_INDEX] != 0x90));
	} else if (response_p->response_data[SCR_SW1_INDEX] == 0x62 ||
			response_p->response_data[SCR_SW1_INDEX] == 0x63 ||
			(trailer_bytes > 0x9000 && trailer_bytes < 0xA000)) {
		/* 4E.1 D*/
		/* Nothing to be done*/
		/* Response TPDU is mapped directly to the APDU */
	}
	error:
		return error_code;
}

static uint32_t scr_process_command_response(
			scr_ctrl_t *scr_p,
			const uint8_t *command_p ,
			scr_command_type_t command_type,
			scr_command_response_t *response_p,
			scr_command_status_t *status,
			uint32_t timeout)
{
	uint32_t error_code = 0;
	scr_debug_print("<%s>:: <%d>[API in]T0 Case no. --> %d\n",
			__func__,
			__LINE__,
			command_type);
	switch (command_type) {
		case SCR_CASE1:
			error_code = scr_t0_case_1(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE2S:
			error_code = scr_t0_case_2s(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE3S:
			error_code = scr_t0_case_3s(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE4S:
			error_code = scr_t0_case_4s(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE2E:
			error_code = scr_t0_case_2e(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE3E:
			error_code = scr_t0_case_3e(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		case SCR_CASE4E:
			error_code = scr_t0_case_4e(scr_p,
						command_p,
						response_p,
						status,
						timeout);
		break;
		default:
		break;
	}
	scr_debug_print("<%s>::<%d> [API out]\n", __func__, __LINE__);
	return error_code;
}

static uint32_t scr_get_command_type(
				const uint8_t  *command_p,
				uint32_t command_size,
				scr_command_type_t *command_type)
{
	uint32_t error_code = 0;
	uint32_t extended_length = 0;
	if (command_size > 7) {
		extended_length =
		(command_p[SCR_C6_INDEX] << 8) + command_p[SCR_C7_INDEX];
	}
	if (command_size == 4) {
		*command_type = SCR_CASE1;
	} else if (command_size == 5) {
		*command_type = SCR_CASE2S;
	} else if ((command_size == (5 + command_p[SCR_C5_INDEX])) &&
			(command_p[SCR_C5_INDEX] != 0x00)) {
		*command_type = SCR_CASE3S;
	} else if ((command_size == (6 + command_p[SCR_C5_INDEX])) &&
			(command_p[SCR_C5_INDEX] != 0x00)) {
		*command_type = SCR_CASE4S;
	} else if ((command_size == 7) &&
			(command_p[SCR_C5_INDEX] == 0x00)) {
		*command_type = SCR_CASE2E;
	} else if ((command_size == (7 + extended_length)) &&
			(extended_length != 0x0000) &&
			(command_p[SCR_C5_INDEX] == 0x00)) {
		*command_type= SCR_CASE3E;
	} else if ((command_size == (9 + extended_length)) &&
			(extended_length != 0x0000) &&
			(command_p[SCR_C5_INDEX] == 0x00)) {
		*command_type = SCR_CASE4E;
	} else {
		scr_error_print("<%s>::<%d> Failed command size %d C5 %d "\
				"Extended length %d\n",
				__func__,
				__LINE__,
				command_size,
				command_p[SCR_C5_INDEX],
				extended_length);
		error_code = -EINVAL;
	}
	return error_code;
}

uint32_t scr_transfer_t0(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  * trans_params_p)
{
	uint32_t error_code = 0;
	uint8_t negative_acknowledge = true;
	scr_command_type_t command_type = 0;
	scr_command_response_t response;
	scr_command_status_t status;
	response.data = trans_params_p->response_p;

	scr_debug_print("<%s>::<%d> [API in] %d\n",
				__func__,
				__LINE__,
				trans_params_p->buffer_p[SCR_T0_P3_OFFSET]);
	error_code = scr_get_command_type(
			trans_params_p->buffer_p,
			trans_params_p->buffer_size,
			&command_type);
	if (error_code)
		goto error;

	if (stm_asc_scr_set_control(scr_handle_p->asc_handle,
		STM_ASC_SCR_NACK,
		negative_acknowledge))
		return -EIO;

	error_code = scr_process_command_response(
		scr_handle_p,
		trans_params_p->buffer_p,
		command_type,
		&response,
		&status,
		trans_params_p->timeout_ms);
	if (error_code)
		goto error;

	trans_params_p->response_p[response.size_of_data] =
			response.response_data[SCR_SW1_INDEX];
	trans_params_p->response_p[response.size_of_data + 1] =
			response.response_data[SCR_SW2_INDEX];
	trans_params_p->actual_response_size =
			response.size_of_data + 2;

	error:
		scr_debug_print("<%s>::<%d> [API out]\n",
				__func__,
				__LINE__);
		return error_code;
}

static uint32_t scr_procedure_byte(uint8_t procedure_byte, uint8_t ins_byte)
{
	uint32_t status = 0;
	if (0x60 == procedure_byte) {
		status = SCR_NULL_BYTE;
	} else if ((0x60 == (procedure_byte & 0xF0)) ||
		(0x90 == (procedure_byte & 0xF0))) {
		status = SCR_SW1_BYTE;
	} else if (ins_byte == procedure_byte) {
		status = SCR_ACK_BYTE;
	} else if ((procedure_byte ^ 0xFF) == ins_byte) {
		status = SCR_ACK_XOR_BYTE;
	} else
		status = SCR_INVALID_BYTE;

	return status;
}

static uint32_t scr_process_procedure_byte(scr_ctrl_t *scr_p,
			const uint8_t *buffer_to_write_p,
			uint32_t size_to_write,
			scr_command_response_t *response_p,
			uint32_t size_to_read,
			uint32_t timeout)
{
	uint32_t rx_timeout = 0, procedure_status, tx_timeout;
	uint32_t error_code = 0;
	bool process_complete = true;
	uint32_t total_read = 0, total_writen = 0;
	uint32_t next_read = 0, next_writen = 0;
	uint32_t current_read = 0, current_writen = 0;
	uint8_t ins_byte = 0;
	uint32_t count =0;
	scr_debug_print("<%s>::<%d> [API in]  with RX timeout %d\n",
			__func__,
			__LINE__,
			timeout);
	tx_timeout = SCR_TX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
		scr_p->N,
		scr_p->scr_caps.baud_rate,
		timeout);

	rx_timeout = SCR_RX_TIMEOUT(scr_p->scr_caps.work_waiting_time,
			scr_p->scr_caps.baud_rate,
			timeout,
			scr_p->wwt_tolerance);

	scr_debug_print(" <%s>:: <%d> tx_timeout %d rx_timeout  %d"\
			"WWT = %d    baud rate = %d\n",
			__func__,
			__LINE__,
			tx_timeout,
			rx_timeout,
			scr_p->scr_caps.work_waiting_time,
			scr_p->scr_caps.baud_rate);
	scr_debug_print("Parameters are write size %d read size %d\n",
			size_to_write,
			size_to_read);


	do {
		error_code = scr_asc_read(scr_p,
			&(response_p->response_data[SCR_PROCEDURE_BYTE_INDEX]),
			1,
			1,
			rx_timeout,
			false);

		if ((signed int)error_code < 0)
			goto error;

		ins_byte = buffer_to_write_p[SCR_T0_INS_OFFSET];

		procedure_status = scr_procedure_byte(
			response_p->response_data[SCR_PROCEDURE_BYTE_INDEX],
			ins_byte);

		scr_debug_print("<%s>::<%d> procedure byte %x  and status  %d "\
			"buffer send INS %x\n",
			__func__,
			__LINE__,
			response_p->response_data[SCR_PROCEDURE_BYTE_INDEX],
			procedure_status,
			buffer_to_write_p[SCR_T0_INS_OFFSET]);

		switch (procedure_status) {
		case SCR_ACK_BYTE:
		case SCR_ACK_XOR_BYTE:
			if (procedure_status == SCR_ACK_XOR_BYTE) {
				next_read = 1;
				next_writen = 1;
			} else {
				next_writen =
					size_to_write - total_writen;
				next_read =
					size_to_read - total_read;
			}

			if (size_to_write && total_writen < size_to_write) {

				scr_debug_print("<%s>:: <%d> scr_asc_write"\
				"for %d total writen till now %d"\
				"size to write %d\n",
				__func__,
				__LINE__,
				next_writen,
				total_writen,
				next_writen);

				current_writen = scr_asc_write(scr_p,
					(buffer_to_write_p +
					SCR_T0_CMD_LENGTH +
					total_writen),
					next_writen,
					next_writen,
					timeout ? tx_timeout : tx_timeout * next_writen);

				if ((signed int)current_writen < 0) {
					scr_error_print("<%s>::<%d> "\
					"scr_asc_write %d\n",
					__func__,
					__LINE__,
					current_writen);
					goto error;
				}

				total_writen += current_writen;
			} else if (size_to_read && total_read < size_to_read) {

				scr_debug_print("<%s>:: <%d>"\
				"scr_asc_read for %d "\
				"total read till now %d "\
				"size to read %d Timeout %d\n",
				__func__,
				__LINE__,
				next_read,
				total_read,
				size_to_read,
				rx_timeout);

				count = next_read;
				while(count) {
					current_read = scr_asc_read(scr_p,
						response_p->data+total_read,
						1,
						1,
						rx_timeout ,
						false);
					if ((signed int)current_read < 0) {
						scr_error_print("<%s>::<%d>"\
						"scr_asc_read %d at count"
						"%d failed \n",
						__func__,
						__LINE__,
						current_read,
						count);
						goto error;
					}
					total_read += current_read;
					count --;

				}
			}
		break;

		case SCR_NULL_BYTE:
		break;

		case SCR_SW1_BYTE:
			response_p->response_data[SCR_SW1_INDEX] =
			response_p->response_data[SCR_PROCEDURE_BYTE_INDEX];

			error_code = scr_asc_read(scr_p,
				&(response_p->response_data[SCR_SW2_INDEX]),
				1,
				1,
				rx_timeout,
				false);
			if ((signed int)error_code < 0) {
				scr_error_print("<%s>::<%d>scr_asc_read %d\n",
				__func__,
				__LINE__,
				error_code);
				goto error;
			}
			scr_debug_print("<%s>::<%d>scr_asc_read the SW2 %x\n",
				__func__,
				__LINE__,
				response_p->response_data[SCR_SW2_INDEX]);

			process_complete = false;
		break;
		case SCR_INVALID_BYTE:
			error_code = -EIO;
			scr_error_print("<%s>::<%d>invalid procedure"
				" byte %d\n",
				__func__,
				__LINE__,
				error_code);
			goto error;
		break;
		}
	} while (process_complete);

	response_p->size_of_data = total_read;
	error_code = 0;
	error:
	scr_debug_print("<%s>::<%d>[API out] error %d\n",
		__func__,
		__LINE__,
		error_code);
	return error_code;
}
