#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_utils.h"
#include "scr_io.h"
#include "scr_plugin.h"

static uint32_t scr_ca3_di_table[16] = {
		DiRFU, 1, 2, 4, 8, 16, 32, 64, 12, 20, DiRFU,
		DiRFU, DiRFU, DiRFU, 80, 93};

/* same as ISO */
static uint32_t iso_fi_table[16] = {
		372, 372, 558, 744, 1116, 1488, 1860, FiRFU,
		FiRFU, 512, 768, 1024, 1536, 2048, FiRFU, FiRFU};


static int __set_ca3_default_param(scr_ctrl_t *scr_p,
			struct scr_ca_param_s *ca_param_p);


stm_asc_scr_stopbits_t scr_ca3_set_stopbit(scr_ctrl_t *scr_handle_p)
{
	return STM_ASC_SCR_STOP_1_5;
}

uint32_t scr_ca3_populate_init_parameters(scr_ctrl_t *scr_handle_p)
{
	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA) {
		/* Initital ETU same as ISO*/
		/* Initial Baud rate same as ISO*/
		/* flow control Enable */
		scr_handle_p->flow_control = true;
		/* NACK control   same as ISO*/
		/* Parity control same as ISO*/
		/* Set the working frequency for CA3 to 4.5 MHz for reset */
		scr_handle_p->scr_caps.working_frequency =
				scr_handle_p->new_inuse_freq =
				4500000;
		scr_handle_p->scr_caps.baud_rate =
			scr_handle_p->scr_caps.working_frequency /
			scr_handle_p->scr_caps.work_etu;
		scr_handle_p->reset_retry = false;
		scr_handle_p->wwt_tolerance = 0;
	}
	return 0;
}
#ifdef CONFIG_ARCH_STI
void scr_ca3_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = GPIO_BIDIR;
}
#else
void scr_ca3_gpio_direction(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->gpio_direction = stm_pad_gpio_direction_bidir;
}
#endif
uint32_t scr_ca3_recalculate_etu(scr_ctrl_t *scr_handle_p,
				uint8_t f_int,
				uint8_t d_int)
{
	UNUSED_PARAMETER(scr_handle_p);
	UNUSED_PARAMETER(f_int);
	UNUSED_PARAMETER(d_int);
	return 0;
}

uint32_t scr_ca3_set_parity(stm_asc_scr_databits_t *databits)
{
	return 0;
}

uint32_t scr_ca3_recalculate_parameters(scr_ctrl_t *scr_handle_p)
{
	UNUSED_PARAMETER(scr_handle_p);
	/*Work ETU*/
	/* BWT same as ISO*/
	/* CWT same as ISO*/
	return 0;
}

uint32_t scr_ca3_transfer_t14(scr_ctrl_t *scr_handle_p,
		stm_scr_transfer_params_t  *trans_params_p)
{
	return -EPROTONOSUPPORT;
}

uint32_t scr_ca3_read(scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable)
{
	return scr_asc_read(
		scr_p,
		buffer_to_read_p,
		size_of_buffer,
		size_to_read,
		timeout,
		change_ctrl_enable);
}

int __set_ca3_default_param(scr_ctrl_t *scr_p,
			struct scr_ca_param_s *ca_param_p)
{
	/* no delay mentioned in specs */
	UNUSED_PARAMETER(scr_p);
	ca_param_p->reset_to_write_delay = 0;
	/*2.1.11.2 - Deactivation Sequence
	RST low (t1) and CLK low (t2) : (t2-t1) > 7.5us */
	ca_param_p->rst_low_to_clk_low_delay = 8;
	return 0;
}

uint32_t scr_ca3_update_fi_di(scr_ctrl_t *scr_p,
				uint8_t f_int,
				uint8_t d_int)
{
	scr_p->di = scr_ca3_di_table[d_int];
	scr_p->fi = iso_fi_table[f_int];
	return 0;
}

uint32_t scr_ca3_enable_clock(scr_ctrl_t *scr_p)
{
	scr_set_reset_clock_frequency(scr_p);
	return 0;
}

void scr_ca_init(void)
{
	struct scr_ca_ops_s ca_ops;

	memset(&ca_ops, 0, sizeof(struct scr_ca_ops_s));
	ca_ops.ca_set_param = __set_ca3_default_param;
	scr_register_ca_ops(&ca_ops);

	scr_ca_transfer_t14 = scr_ca3_transfer_t14;
	scr_ca_populate_init_parameters = scr_ca3_populate_init_parameters;
	scr_ca_recalculate_parameters = scr_ca3_recalculate_parameters;
	scr_ca_read = scr_ca3_read;
	scr_ca_set_parity = scr_ca3_set_parity;
	scr_ca_recalculate_etu = scr_ca3_recalculate_etu;
	scr_ca_set_stopbits = scr_ca3_set_stopbit;
	scr_ca_gpio_direction = scr_ca3_gpio_direction;
	scr_ca_update_fi_di = scr_ca3_update_fi_di;
	scr_ca_enable_clock = scr_ca3_enable_clock;
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
