#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_protocol_t1.h"

#define SCR_CA_INITIAL_BAUD_RATE            9600
#define SCR_CA_INITIAL_ETU             625
#define SCR_CA_MAX_RESET_FREQUENCY     6000000
#define SCR_CA_MIN_RESET_FREQUENCY     1000000

/* same as ISO */
static uint32_t iso_fi_table[16] = {
		372, 372, 558, 744, 1116, 1488, 1860, FiRFU,
		FiRFU, 512, 768, 1024, 1536, 2048, FiRFU, FiRFU};

static uint32_t scr_getdi(scr_ctrl_t *scr_handle_p,
						uint8_t f_int,
						uint8_t d_int)
{
	uint32_t ta1 = ((f_int & 0x0F) << 4) | d_int;
	uint32_t di = SCR_INITIAL_Di;

	switch (ta1) {
	case 0x11:
		di = SCR_INITIAL_Di;
		break;
	case 0xB6:
	case 0xA6:
	case 0x96:
		di = 32;
		break;
	case 0x95:
		di = 16;
		break;
	case 0x18:
		di = 12;
		break;
	default:
		break;
	}

	if (STM_SCR_PROTOCOL_T14 ==
			scr_handle_p->current_protocol) {
		switch (ta1) {
		case 0x93:
			di = 4;
			break;
		case 0x21:
			di = SCR_INITIAL_Di;
			break;
		default:
			break;
		}
	}
	return di;
}

stm_asc_scr_stopbits_t scr_ca2_set_stopbit(scr_ctrl_t *scr_handle_p)
{
	return STM_ASC_SCR_STOP_0_5;
}

uint32_t scr_ca2_populate_init_parameters(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->atr_response.extra_bytes_in_atr = 2;

	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA) {
		/* In terms of ETU */
		scr_handle_p->scr_caps.baud_rate = SCR_CA_INITIAL_BAUD_RATE;
		/* In terms of clock cycles */
		scr_handle_p->scr_caps.work_etu = SCR_CA_INITIAL_ETU;
		/* flow control  same as ISO */
		/* NACK control   Disable */
		scr_handle_p->nack = false;
		scr_handle_p->reset_retry = true;
	}
	return 0;
}
#ifdef CONFIG_ARCH_STI
void scr_ca2_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = GPIO_BIDIR;
}
#else
void scr_ca2_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = stm_pad_gpio_direction_in;
}
#endif
uint32_t scr_ca2_recalculate_parameters(scr_ctrl_t *scr_handle_p)
{
	scr_atr_parsed_data_t *atr_data_p =
			&scr_handle_p->atr_response.parsed_data;
	stm_scr_capabilities_t *scr_caps_p = &scr_handle_p->scr_caps;

        /* CWT */
	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA)
		scr_handle_p->scr_caps.character_waiting_time =
			(atr_data_p->character_waiting_index * 300);

	/*To avoid dispersion problems, Nagravision fixes the value FF (255)
	* for TC1 to 12 ETU.*/
	if (((STM_SCR_PROTOCOL_T1 == scr_handle_p->current_protocol) ||
	(STM_SCR_PROTOCOL_T14 == scr_handle_p->current_protocol))
	&& (atr_data_p->minimum_guard_time == true))
		atr_data_p->N = SCR_DEFAULT_N;

	/* modify the stored values */
	if (((STM_SCR_PROTOCOL_T1 == scr_handle_p->current_protocol) ||
	(STM_SCR_PROTOCOL_T14 == scr_handle_p->current_protocol))
	&& (atr_data_p->N == 1))
		scr_caps_p->guard_delay = 0;
	else
		scr_caps_p->guard_delay = atr_data_p->N;

	scr_handle_p->N = atr_data_p->N;

	return 0;
}

uint32_t scr_ca2_set_parity(stm_asc_scr_databits_t *databits)
{
	*databits = STM_ASC_SCR_8BITS_NO_PARITY;
	return 0;
}

uint32_t scr_ca2_recalculate_etu(scr_ctrl_t *scr_handle_p,
				uint8_t f_int, uint8_t d_int)
{
	uint32_t	work_etu;
	stm_scr_capabilities_t *scr_caps_p = &scr_handle_p->scr_caps;
	uint32_t ta1 = ((f_int & 0x0F) << 4) | d_int;

	/* T14 and T1 CA2 */
	switch (ta1) {
	case 0x11:
	case 0x18:
		work_etu = 372;
		break;
	case 0xB6:
		work_etu = 1024;
		break;
	case 0x95:
		work_etu = 512;
		break;
	case 0xA6:
		work_etu = 768;
		break;
	case 0x96:
		work_etu = 512;
		break;
	default:
		work_etu = SCR_INITIAL_ETU;
		break;
	}

	if (scr_handle_p->current_protocol == STM_SCR_PROTOCOL_T14) {
		switch (ta1) {
		case 0x93:
			work_etu = 512;
			break;
		case 0x21:
			work_etu = 558;
			break;
		default:
			work_etu = SCR_CA_INITIAL_ETU;
			break;
		}
	}
	scr_handle_p->scr_caps.work_etu = work_etu;
	scr_caps_p->max_clock_frequency = SCR_CA_MAX_RESET_FREQUENCY;
	return 0;
}

uint32_t scr_ca2_transfer_t14(scr_ctrl_t *scr_handle_p,
		stm_scr_transfer_params_t *trans_params_p)
{
	return scr_transfer_t1(scr_handle_p, trans_params_p);
}

uint32_t scr_ca2_read(scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable)
{
	return scr_asc_read(scr_p, buffer_to_read_p, size_of_buffer,
		size_to_read, timeout, change_ctrl_enable);
}
uint32_t scr_ca2_update_fi_di(scr_ctrl_t *scr_p,
				uint8_t f_int,
				uint8_t d_int)
{
	scr_p->di = scr_getdi(scr_p, f_int, d_int);
	scr_p->fi = iso_fi_table[f_int];
	return 0;
}

void scr_ca_init(void)
{
	scr_ca_transfer_t14 = scr_ca2_transfer_t14;
	scr_ca_populate_init_parameters = scr_ca2_populate_init_parameters;
	scr_ca_recalculate_parameters = scr_ca2_recalculate_parameters;
	scr_ca_read = scr_ca2_read;
	scr_ca_set_parity = scr_ca2_set_parity;
	scr_ca_recalculate_etu = scr_ca2_recalculate_etu;
	scr_ca_set_stopbits = scr_ca2_set_stopbit;
	scr_ca_gpio_direction = scr_ca2_gpio_direction;
	scr_ca_update_fi_di = scr_ca2_update_fi_di;
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
