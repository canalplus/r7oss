#include "ca_os_wrapper.h"
#include "stm_scr.h"
#include "scr_plugin.h"

static uint8_t ca_op_set;
static struct scr_ca_ops_s g_ca_ops;

void scr_register_ca_ops(struct scr_ca_ops_s *ca_ops_p)
{
	g_ca_ops.ca_transfer_t14 = ca_ops_p->ca_transfer_t14;
	g_ca_ops.ca_fill_init_param = ca_ops_p->ca_fill_init_param;
	g_ca_ops.ca_set_param = ca_ops_p->ca_set_param;
	g_ca_ops.ca_read =  ca_ops_p->ca_read;
	g_ca_ops.ca_set_parity = ca_ops_p->ca_set_parity;
	g_ca_ops.ca_reeval_etu = ca_ops_p->ca_reeval_etu;
	g_ca_ops.ca_set_stopbits = ca_ops_p->ca_set_stopbits;
	g_ca_ops.manage_auto_parity = ca_ops_p->manage_auto_parity;
	ca_op_set=1;
}
EXPORT_SYMBOL(scr_register_ca_ops);

int scr_get_ca_ops(struct scr_ca_ops_s *ca_ops_p)
{
	if (ca_op_set) {
		ca_ops_p->ca_transfer_t14 = g_ca_ops.ca_transfer_t14;
		ca_ops_p->ca_fill_init_param = g_ca_ops.ca_fill_init_param;
		ca_ops_p->ca_set_param = g_ca_ops.ca_set_param;
		ca_ops_p->ca_read =  g_ca_ops.ca_read;
		ca_ops_p->ca_set_parity = g_ca_ops.ca_set_parity;
		ca_ops_p->ca_reeval_etu = g_ca_ops.ca_reeval_etu;
		ca_ops_p->ca_set_stopbits = g_ca_ops.ca_set_stopbits;
		ca_ops_p->manage_auto_parity = g_ca_ops.manage_auto_parity;
		return 0;
	}
	return -1;
}


void scr_get_io_ops(struct scr_io_ops_s *io_ops_p)
{
	/*TBD*/
	UNUSED_PARAMETER(io_ops_p);
}

EXPORT_SYMBOL(scr_get_io_ops);
