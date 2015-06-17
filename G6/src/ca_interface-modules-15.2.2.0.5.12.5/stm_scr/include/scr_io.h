#ifndef _STM_SCR_IO_H
#define _STM_SCR_IO_H

void scr_bit_convertion(uint8_t *data_p, uint32_t size);
int32_t  scr_asc_pad_config(scr_ctrl_t *scr_p, stm_scr_gpio_t asc_gpio);
int32_t scr_asc_get_pad_config(scr_ctrl_t *scr_p, int *value_p);
uint32_t  scr_asc_new(scr_ctrl_t *scr_p);
uint32_t scr_asc_delete(scr_ctrl_t *scr_p);

uint32_t scr_set_asc_ctrl(scr_ctrl_t *scr_p);
uint32_t  scr_asc_read(scr_ctrl_t *scr_p,
				uint8_t *buffer_to_read_p,
				uint32_t size_of_buffer,
				uint32_t size_to_read,
				uint32_t  timeout,
				bool change_ctrl_enable);

uint32_t  scr_asc_write(scr_ctrl_t *scr_p,
				const uint8_t *buffer_to_write_p,
				uint32_t size_of_buffer,
				uint32_t  size_to_write,
				uint32_t  timeout);

uint32_t scr_asc_abort(scr_ctrl_t *scr_p);

uint32_t scr_asc_flush(scr_ctrl_t *scr_p);

uint32_t scr_asc_freeze(scr_ctrl_t *scr_p);
uint32_t scr_asc_restore(scr_ctrl_t *scr_p);
#endif
