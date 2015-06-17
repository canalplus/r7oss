#ifndef SCR_PROTOCOL_T14_H_
#define SCR_PROTOCOL_T14_H_
uint32_t (*scr_ca_transfer_t14)(scr_ctrl_t *scr_handle_p,
			stm_scr_transfer_params_t  *trans_params_p);

uint32_t (*scr_ca_populate_init_parameters)(scr_ctrl_t *scr_handle_p);

uint32_t (*scr_ca_recalculate_parameters)(scr_ctrl_t *scr_handle_p);

uint32_t (*scr_ca_read) (scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable);

uint32_t (*scr_ca_set_parity)(stm_asc_scr_databits_t *data_bit_p);

uint32_t (*scr_ca_recalculate_etu)(scr_ctrl_t *scr_handle_p,
					uint8_t f_int,
					uint8_t d_int);

void (*scr_ca_gpio_direction)(scr_ctrl_t *scr_handle_p);

stm_asc_scr_stopbits_t (*scr_ca_set_stopbits)(scr_ctrl_t *scr_handle_p);

uint32_t (*scr_ca_update_fi_di)(scr_ctrl_t *scr_handle_p,
				uint8_t f_int,
				uint8_t d_int);

uint32_t (*scr_ca_enable_clock)(scr_ctrl_t *scr_handle_p);

#endif
