/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/cecplatform.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef CECPLATFORM_H_
#define CECPLATFORM_H_

struct cec_config_data{
    unsigned int	address;
    unsigned int	mask;
};
#ifdef CONFIG_ARCH_STI
struct stm_cec_platform_data {
    struct platform_device  *pdev;
    struct pinctrl  *pinctrl;
    unsigned int            num_config_data;
    struct cec_config_data  *config_data;
    struct reset_control    *reset;
    unsigned int clk_err_correction;
    uint8_t auto_ack_for_broadcast_tx;
};
#else
struct stm_cec_platform_data {
    struct stm_pad_config	*pad_config;
    unsigned int		num_config_data;
    struct cec_config_data	*config_data;
    struct sysconf_field *sysconf;
    unsigned int clk_err_correction;
    uint8_t auto_ack_for_broadcast_tx;
};
#endif

#endif /* CECPLATFORM_H_ */
