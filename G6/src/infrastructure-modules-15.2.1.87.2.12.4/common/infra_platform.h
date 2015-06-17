/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_cec is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_cec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : infra_platform.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#ifndef _INFRA_PLATFORM_H_
#define _INFRA_PLATFORM_H_

struct cec_config_data{
	unsigned int	address;
	unsigned int	mask;
};

struct stm_cec_platform_data {
	struct stm_pad_config	*pad_config;
	unsigned int		num_config_data;
	struct cec_config_data	*config_data;
#ifdef CONFIG_OF
	struct sysconf_field *sysconf;
#endif
	unsigned int clk_err_correction;
	uint8_t auto_ack_for_broadcast_tx;
};
void lx_cpu_configure(void);
void lx_cpu_unconfigure(void);
void cec_configure(void);
void cec_unconfigure(void);
void arch_modprobe_func(void);

#endif
