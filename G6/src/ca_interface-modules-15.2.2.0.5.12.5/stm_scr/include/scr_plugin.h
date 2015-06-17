#ifndef _SCR_PLUGIN_H_
#define _SCR_PLUGIN_H_

#include "stm_scr.h"
#include "stm_asc_scr.h"

struct scr_ca_param_s {
	uint32_t reset_to_write_delay;
	uint32_t rst_low_to_clk_low_delay;
};


struct scr_ca_ops_s {
	int32_t (*ca_transfer_t14)(stm_scr_h scr,
		stm_scr_transfer_params_t  *trans_params_p);
	int32_t (*ca_fill_init_param)(stm_scr_h scr);
	int32_t (*ca_set_param)(stm_scr_h scr,
				struct scr_ca_param_s *param_p);
	int32_t (*ca_read) (stm_scr_h scr,
				stm_scr_transfer_params_t  *params_p);
	int32_t (*ca_set_parity)(stm_asc_scr_databits_t *data_bit_p);
	int32_t (*ca_reeval_etu)(stm_scr_h *scr_p);
	stm_asc_scr_stopbits_t (*ca_set_stopbits)(stm_scr_h *scr_p);
	int32_t (*manage_auto_parity)(stm_asc_h  asc,
				stm_scr_protocol_t protocol);
};

struct scr_io_ops_s {
	int32_t (*scr_asc_read)(stm_scr_h scr,
				uint8_t *buf_p,
				uint32_t buf_size,
				uint32_t size_to_read,
				uint32_t  timeout,
				bool change_ctrl_enable);
	int32_t (*scr_asc_write)(stm_scr_h scr,
				const uint8_t *buf_p,
				uint32_t buf_size,
				uint32_t  size_to_write,
				uint32_t  timeout);

	int32_t (*scr_asc_flush)(stm_scr_h scr);
	int32_t (*scr_transfer_t1)(stm_scr_h scr,
				stm_scr_transfer_params_t  *trans_params_p);
};

struct scr_extended_ops_s {
	int32_t (*scr_deactivate)(stm_scr_h scr);
	int32_t (*scr_change_class)(stm_scr_h scr);
	int32_t (*scr_reset)(stm_scr_h *scr_p,uint32_t reset_type);
};

int scr_get_ca_ops(struct scr_ca_ops_s *ca_ops_p);
void scr_register_ca_ops(struct scr_ca_ops_s *ca_ops_p);
void scr_get_io_ops(struct scr_io_ops_s *io_ops_p);
void scr_get_extended_ops(struct scr_extended_ops_s *extended_ops_p);

#endif
