#ifndef	_CI_HAL_EPLD_H
#define	_CI_HAL_EPLD_H

#include "ci_internal.h"
ca_error_code_t  ci_slot_status_epld(
		struct ci_dvb_ca_en50221 *ca, uint16_t *pin_value_p);
ca_error_code_t  ci_slot_reset_epld(
		struct ci_dvb_ca_en50221 *ca);

ca_error_code_t  ci_slot_data_irq_disable_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_data_irq_enable_epld(
		struct	ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_data_irq_uninstall_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_data_irq_install_epld(
		struct ci_dvb_ca_en50221 *ca);

ca_error_code_t  ci_slot_detect_irq_disable_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_detect_irq_enable_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_detect_irq_uninstall_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_detect_irq_install_epld(
		struct ci_dvb_ca_en50221 *ca);

ca_error_code_t  ci_slot_power_enable_epld(
		struct ci_dvb_ca_en50221 *ca, ci_voltage_t voltage);
ca_error_code_t  ci_slot_power_disable_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_switch_access_mode_epld(
		struct ci_dvb_ca_en50221 *ca, ci_access_mode_t  access_mode);

ca_error_code_t  ci_slot_init_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_shutdown_epld(
		struct ci_dvb_ca_en50221 *ca);
ca_error_code_t  ci_slot_enable_epld(
		struct ci_dvb_ca_en50221 *ca);

#endif

