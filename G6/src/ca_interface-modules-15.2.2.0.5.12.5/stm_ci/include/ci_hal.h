#ifndef	_STM_CI_HAL_H
#define	_STM_CI_HAL_H
#include "ci_internal.h"
#include "ca_platform.h"
#include "ci_hal_pad.h"
#include "ci_protocol.h"
#include "ci_hal_epld.h"
#include "ci_handler.h"

ca_error_code_t ci_hal_slot_init(stm_ci_t *ci_p);
ca_error_code_t ci_hal_slot_shutdown(stm_ci_t *ci_p);
ca_error_code_t ci_hal_slot_status(stm_ci_t *ci_p);
ca_error_code_t ci_hal_power_enable(stm_ci_t	*ci_p);
ca_error_code_t ci_hal_power_disable(stm_ci_t *ci_p);
ca_error_code_t ci_hal_slot_reset(stm_ci_t *ci_p);

#define	CI_DETECT_GPIO	0
#define	CI_IRQ_GPIO	0

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#define	CI_SET_IRQ_TYPE(irq, type)	irq_set_irq_type(irq, type)
#else
#define CI_SET_IRQ_TYPE(irq, type)	set_irq_type(irq, type)
#endif
#endif

