#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"

uint32_t scr_transfer_t14(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  *trans_params_p)
{
	uint32_t error_code = 0;

	scr_debug_print("<%s>::<%d> [API In]\n", __func__, __LINE__);
	if (scr_ca_transfer_t14)
		error_code = scr_ca_transfer_t14(scr_handle_p, trans_params_p);
	else {
		scr_error_print(" <%s>::<%d> Failed Protocol not supported\n",
			__func__, __LINE__);
	/*Will fail only when the T14 module not loaded*/
		error_code = -EPROTONOSUPPORT;
	}
	scr_debug_print("<%s>::<%d> [API Out]\n", __func__, __LINE__);
	return error_code;
}
