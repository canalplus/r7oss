#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"
#include "scr_utils.h"

#define SCR_CA_INITIAL_BAUD_RATE           9677
#define SCR_CA_INITIAL_ETU            620
#define SCR_CA_MAX_RESET_FREQUENCY    8000000
#define SCR_CA_MIN_RESET_FREQUENCY    6000000

/* T14 Defines for CA1 Acess Card*/
#define T14_CMD_LENGTH                      7
#define T14_SOH_OFFSET                      0
#define T14_CLA_OFFSET                      1
#define T14_INS_OFFSET                      2
#define T14_P1_OFFSET                       3
#define T14_P2_OFFSET                       4
#define T14_P3_OFFSET                       5
#define T14_LC_OFFSET                       6

#define T14_SW1_OFFSET                      2
#define T14_SW2_OFFSET                      3
#define T14_RES_INS_OFFSET                  4

/* T14 CA1 Response Length */
#define CA1_T14_FIXEDBYTESTOREAD 8

typedef struct {
	uint8_t SOH;
	uint8_t Ins;
	uint8_t Size;
	uint8_t SW[2];
} scr_t14_data_t;

uint32_t scr_ca1_read(scr_ctrl_t *scr_p,
		uint8_t *buffer_to_read_p,
		uint32_t size_of_buffer,
		uint32_t size_to_read,
		uint32_t  timeout,
		bool change_ctrl_enable);

static int __set_ca1_default_param(scr_ctrl_t *scr_p,
		struct scr_ca_param_s *ca_param_p);
static int __manage_ca1_auto_parity(stm_asc_h  asc,
		stm_scr_protocol_t protocol);

stm_asc_scr_stopbits_t scr_ca1_set_stopbit(scr_ctrl_t *scr_handle_p)
{
	return STM_ASC_SCR_STOP_0_5;
}

uint32_t scr_ca1_populate_init_parameters(scr_ctrl_t * scr_handle_p)
{
	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA) {
		/* In terms of ETU */
		scr_handle_p->scr_caps.baud_rate = SCR_CA_INITIAL_BAUD_RATE;
		/* In terms of clock cycles */
		scr_handle_p->scr_caps.work_etu = SCR_CA_INITIAL_ETU;
		/* flow control  same as ISO */
		scr_handle_p->nack = false;
		scr_handle_p->reset_retry = true;
	}
	return 0;
}

#ifdef CONFIG_ARCH_STI
void scr_ca1_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = GPIO_BIDIR;
}
#else
void scr_ca1_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = stm_pad_gpio_direction_in;
}
#endif

uint32_t scr_ca1_set_parity(stm_asc_scr_databits_t *databits)
{
	*databits = STM_ASC_SCR_8BITS_NO_PARITY;
	return 0;
}

uint32_t scr_ca1_recalculate_parameters(scr_ctrl_t *scr_handle_p)
{
	UNUSED_PARAMETER(scr_handle_p);
	/* BWT same as ISO*/
	/* CWT same as ISO*/
	return 0;
}

int __set_ca1_default_param(scr_ctrl_t *scr_p,
			struct scr_ca_param_s *ca_param_p)
{
	uint32_t delay;

	/* 40000 = Number of clock cycles :Delay between the last byte of
	 * the ATR received from the smart card and the first command
	 * byte sent to the smart card*/
	delay = ((40000 * (CA_OS_GET_TICKS_PER_SEC * (1000 /
				HZ)))/SCR_CA_MIN_RESET_FREQUENCY) + 1;

	ca_param_p->reset_to_write_delay = delay;
	ca_param_p->rst_low_to_clk_low_delay = 0; /* in us */
	return 0;
}

int __manage_ca1_auto_parity(stm_asc_h  asc,
		stm_scr_protocol_t protocol)
{
	int ret = 0;
	/* T14 protocol frame lack parity thus no need to enable
	 * auto parity rejection feature of ASC */
	if (protocol == STM_SCR_PROTOCOL_T14)
		ret = stm_asc_scr_set_control(
				asc,
				STM_ASC_SCR_AUTO_PARITY_REJECTION,
				false);
	return ret;
}


uint32_t scr_ca1_recalculate_etu(scr_ctrl_t *scr_handle_p,
				uint8_t f_int,
				uint8_t d_int)
{
	uint32_t  fd, clock_rate_divider;

	fd = ((f_int & 0x0F) << 4) | d_int;

	switch (fd) {
	case 0x21:
		clock_rate_divider = 620;
		scr_handle_p->scr_caps.work_etu = SCR_CA_INITIAL_ETU;
	break;
	case 0x11:
		clock_rate_divider = 416;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
	break;
	case 0x22:
		clock_rate_divider = 310;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
		scr_handle_p->scr_caps.work_waiting_time =
			scr_handle_p->scr_caps.work_waiting_time * 2;
	break;
	case 0x23:
		clock_rate_divider = 155;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
		scr_handle_p->scr_caps.work_waiting_time =
			scr_handle_p->scr_caps.work_waiting_time * 4;
	break;
	case 0xb8:
		clock_rate_divider = 96;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
		scr_handle_p->scr_caps.work_waiting_time =
			scr_handle_p->scr_caps.work_waiting_time * 12;
	break;
	case 0x29:
		clock_rate_divider = 31;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
		scr_handle_p->scr_caps.work_waiting_time =
			scr_handle_p->scr_caps.work_waiting_time * 20;
	break;
	case 0xFF:
		clock_rate_divider = 64;
		scr_handle_p->scr_caps.work_etu = clock_rate_divider;
		scr_handle_p->scr_caps.work_waiting_time =
		scr_handle_p->scr_caps.work_waiting_time / 64;
	break;
	default:
		scr_handle_p->scr_caps.work_etu = SCR_CA_INITIAL_ETU;
		break;
	}
	return 0;
}

uint32_t scr_ca1_transfer_t14(scr_ctrl_t *scr_handle_p,
	stm_scr_transfer_params_t *trans_params_p)
{
	uint32_t error_code = 0;
	scr_t14_data_t procedure_data;
	uint32_t rx_timeout = 0, tx_timeout = 0;
	scr_debug_print(" <%s> :: <%d> [API In]\n", __func__, __LINE__);
	error_code = scr_asc_flush(scr_handle_p);
	if (error_code)
		goto error;
		tx_timeout =
			SCR_TX_TIMEOUT(scr_handle_p->scr_caps.work_waiting_time,
			scr_handle_p->N,
			scr_handle_p->scr_caps.baud_rate,
			trans_params_p->timeout_ms);

	rx_timeout = SCR_RX_TIMEOUT(scr_handle_p->scr_caps.work_waiting_time,
				scr_handle_p->scr_caps.baud_rate,
				trans_params_p->timeout_ms,
				scr_handle_p->wwt_tolerance);
	scr_debug_print("<%s> :: <%d> Tx time %d Rx time %d\n",
		__func__, __LINE__, tx_timeout, rx_timeout);

	error_code = scr_asc_write(scr_handle_p,
			trans_params_p->buffer_p,
			trans_params_p->buffer_size,
			trans_params_p->buffer_size,
			tx_timeout);
	if ((signed int)error_code < 0)
		goto error;
	else
		error_code = 0;
	scr_debug_print(" <%s> :: <%d>\n", __func__, __LINE__);
	trans_params_p->actual_response_size = scr_ca1_read(scr_handle_p,
			trans_params_p->response_p,
			trans_params_p->response_buffer_size,
			trans_params_p->response_buffer_size,
			rx_timeout,
			true);
	if ((signed int)trans_params_p->actual_response_size < 0) {
		error_code = trans_params_p->actual_response_size;
		goto error;
	}
	scr_debug_print(" <%s> :: <%d>\n", __func__, __LINE__);
	if ((trans_params_p->buffer_p[T14_SOH_OFFSET] ==
		trans_params_p->response_p[T14_SOH_OFFSET]) &&
		(trans_params_p->buffer_p[T14_RES_INS_OFFSET] ==
		trans_params_p->response_p[T14_RES_INS_OFFSET])) {

		procedure_data.SOH =
			trans_params_p->response_p[T14_SOH_OFFSET];
		procedure_data.Ins =
			trans_params_p->response_p[T14_RES_INS_OFFSET];
		procedure_data.SW[0] =
			trans_params_p->response_p[T14_SW1_OFFSET];
		procedure_data.SW[1] =
			trans_params_p->response_p[T14_SW2_OFFSET];
		procedure_data.Size =
			trans_params_p->actual_response_size;
	} else {
		scr_debug_print(" <%s> :: <%d> data comparison Failed !!!!\n",
				__func__, __LINE__);
		 scr_debug_print(" <SOH> Command %x -- Response %x\n",
			trans_params_p->buffer_p[T14_SOH_OFFSET],
			trans_params_p->response_p[T14_SOH_OFFSET]);
		scr_debug_print(" <INS> Command %x -- Response %x\n",
			trans_params_p->buffer_p[T14_RES_INS_OFFSET],
			trans_params_p->response_p[T14_RES_INS_OFFSET]);
		error_code = -EINVAL;
	}

	error:
		scr_debug_print(" <%s> :: <%d> [API Out] %d\n",
				__func__, __LINE__ , error_code);
		return error_code;
}

uint32_t scr_ca1_read(scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable)
{
	uint32_t bytes_read = 0, LC, total_read = 0;
	uint32_t error_code = 0;

	bytes_read = scr_asc_read(scr_p,
		buffer_to_read_p,
		size_of_buffer,
		CA1_T14_FIXEDBYTESTOREAD,
		timeout, change_ctrl_enable);
	if ((signed int)bytes_read < 0) {
		error_code = bytes_read;
		goto error;
	}

	total_read = bytes_read;
	LC = buffer_to_read_p[bytes_read - 1];
	scr_debug_print("<%s>::<%d>  LC value %d\n ", __func__, __LINE__, LC);
	{
		int i;
		for (i = 0; i < bytes_read; i++)
			scr_debug_print(" %x\n", buffer_to_read_p[i]);
		scr_debug_print("\n");
	}
	bytes_read = scr_asc_read(scr_p,
			buffer_to_read_p + total_read,
			size_of_buffer - total_read,
			LC,
			timeout,false);
	if ((signed int)bytes_read < 0) {
		error_code = bytes_read;
		goto error;
	}
	total_read += bytes_read;
	scr_debug_print("<%s>::<%d> data still to read %d\n ",
		__func__, __LINE__, size_to_read - total_read);
	if ((total_read < (CA1_T14_FIXEDBYTESTOREAD + LC + 1))
		&& (total_read < size_to_read)) {
		bytes_read = scr_asc_read(scr_p,
			buffer_to_read_p + total_read,
			size_of_buffer - total_read,
			size_to_read - total_read,
			timeout, false);
		if ((signed int)bytes_read < 0) {
			error_code = bytes_read;
			goto error;
		}
	total_read += bytes_read;
	}
	scr_debug_print("<%s>::<%d> Total data read %d\n",
		__func__, __LINE__, total_read);
	error_code =  total_read;

	error :
		return error_code;
}

uint32_t scr_ca1_update_fi_di(scr_ctrl_t *scr_p,
				uint8_t f_int,
				uint8_t d_int)
{
	UNUSED_PARAMETER(scr_p);
	UNUSED_PARAMETER(f_int);
	UNUSED_PARAMETER(d_int);
	return 0;
}
void scr_ca_init(void)
{
	struct scr_ca_ops_s ca_ops;

	memset(&ca_ops, 0, sizeof(struct scr_ca_ops_s));
	ca_ops.ca_set_param = __set_ca1_default_param;
	ca_ops.manage_auto_parity = __manage_ca1_auto_parity;
	scr_register_ca_ops(&ca_ops);

	scr_ca_transfer_t14 = scr_ca1_transfer_t14;
	scr_ca_populate_init_parameters = scr_ca1_populate_init_parameters;
	scr_ca_recalculate_parameters = scr_ca1_recalculate_parameters;
	scr_ca_read = scr_ca1_read;
	scr_ca_set_parity = scr_ca1_set_parity;
	scr_ca_recalculate_etu = scr_ca1_recalculate_etu;
	scr_ca_set_stopbits = scr_ca1_set_stopbit;
	scr_ca_gpio_direction = scr_ca1_gpio_direction;
	scr_ca_update_fi_di = scr_ca1_update_fi_di;
	scr_ca_enable_clock = NULL;
}

void scr_ca_term(void)
{
	scr_ca_transfer_t14 = NULL;
	scr_ca_populate_init_parameters = NULL;
	scr_ca_recalculate_parameters = NULL;
	scr_ca_read = NULL;
	scr_ca_set_parity = NULL;
	scr_ca_recalculate_etu = NULL;
	scr_ca_set_stopbits = NULL;
	scr_ca_gpio_direction = NULL;
	scr_ca_update_fi_di = NULL;
	scr_ca_enable_clock = NULL;
}
