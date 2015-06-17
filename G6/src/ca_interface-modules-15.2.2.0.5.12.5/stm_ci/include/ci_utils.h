#ifndef	_STM_CI_UTILS_H
#define	_STM_CI_UTILS_H
#include "ci_internal.h"

ca_error_code_t ci_validate_init_param(uint32_t dev_id, stm_ci_h *device);
ca_error_code_t ci_validate_del_param(stm_ci_h device);
ca_error_code_t ci_alloc_control_param(uint32_t dev_id, stm_ci_h *device);

ca_error_code_t ci_alloc_global_param(void);

void ci_fill_control_param(uint32_t dev_id , stm_ci_t  *ci_p);

ca_error_code_t ci_dealloc_global_param(void);

ca_error_code_t ci_dealloc_control_param(stm_ci_t *ci_p);

void  ci_enter_critical_section(void);
void ci_exit_critical_section(void);

ca_error_code_t  ci_set_vcc_type(struct stm_ci_platform_data	  *platform_data_p,stm_ci_t  *ci_p);

void ci_register_interface_fptr(stm_ci_t  *ci_p);

#endif

