#ifndef _STM_SCR_UTILS_H
#define _STM_SCR_UTILS_H

/* Compute new timeouts:
 *
 * - See ISO7816-3 for more information
 * - We multiply by 1000 to compute the value in
 *   millisecond intervals (used by the IO interface).
 */

/* Default Timeout in msec = (((Default WWT) * 1000 ) / Default Baudrate) +
 * (Halffull/halfempty Interrupt occurance Time for 8 Bytes)
 * = (9600*1000/9600) + ((((1/9600)/8)*10)*8) = 1000 + 10.4 = 1011
 * (Interrupt occurance Time for 8 Bytes) = ((1/Baudrate)/8bits)*10bits*8byte
 */

#define SCR_CONVERT_TO_MS	1000
#define SCR_RX_TIMEOUT(wwt, baudrate, timeout,tolerance)\
		(timeout ? timeout :\
		(((wwt * SCR_CONVERT_TO_MS) / baudrate) + tolerance))
#define SCR_TX_TIMEOUT(wwt, n, baudrate,timeout)\
		(timeout ? timeout :\
		(((wwt + n) * SCR_CONVERT_TO_MS) / baudrate))

#define SCR_THREAD_PRIORITY  85

int scr_send_message_and_wait(scr_ctrl_t *handle_p,
				scr_thread_state_t next_state,
				scr_process_command_t type,
				void *data_p,
				bool at_front);


int scr_send_message(scr_ctrl_t *handle_p,
				scr_thread_state_t next_state,
				scr_process_command_t type,
				void *data_p,
				bool at_front);

uint32_t scr_read_tck(scr_ctrl_t *scr_handle_p);
uint32_t scr_read_extrabytes(scr_ctrl_t *scr_handle_p);

uint32_t scr_initialise_pio(scr_ctrl_t *scr_handle_p);
int scr_claim_class_sel_pad(stm_scr_h scr);
int scr_set_ca_default_param(scr_ctrl_t *, struct scr_ca_param_s *);
uint32_t scr_delete_pio(scr_ctrl_t *scr_handle_p);

bool scr_card_present(scr_ctrl_t *scr_handle_p);
uint32_t scr_check_state(scr_ctrl_t *scr_handle_p , uint32_t next_state);

void scr_change_clock_frequency(scr_ctrl_t *scr_handle_p,
		uint32_t requested_clock);

void scr_set_reset_clock_frequency(scr_ctrl_t *scr_handle_p);
void scr_set_clock_frequency(scr_ctrl_t *scr_handle_p);
uint32_t scr_cold_reset(scr_ctrl_t *scr_handle_p);
uint32_t scr_warm_reset(scr_ctrl_t *scr_handle_p);
uint8_t scr_calculate_xor(uint8_t *data_p, uint32_t count);
uint32_t scr_get_direction(uint32_t direction, uint32_t function);
uint32_t scr_no_of_bits_set(uint8_t byte);

void scr_update_pps_parameters(scr_ctrl_t *scr_handle_p,
		scr_pps_info_t pps_info);

void scr_update_atr_parameters(scr_ctrl_t *scr_handle_p);

void scr_set_asc_params(scr_ctrl_t *scr_handle_p,
		uint32_t value, stm_scr_ctrl_flags_t ctrl_flags);

void scr_get_asc_params(scr_ctrl_t *scr_handle_p,
		uint32_t *value, stm_scr_ctrl_flags_t ctrl_flags);

int32_t scr_set_nack(scr_ctrl_t *scr_handle_p,
		stm_scr_protocol_t current_protocol);

void scr_gpio_direction(scr_ctrl_t *scr_handle_p);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
#define SCR_SET_IRQ_TYPE(irq, type)	irq_set_irq_type(irq, type)
#else
#define SCR_SET_IRQ_TYPE(irq, type)	set_irq_type(irq, type)
#endif

#endif
