/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ce_hal.h

Declares stm_ce HAL APIs and HAL management functions
************************************************************************/
#ifndef __CE_DT_H
#define __CE_DT_H

#include <linux/clk.h>

extern struct of_device_id ce_dt_sc_match[];

/* Describes a clock used by SC */
struct ce_hal_clk {
	struct clk *clk;
	const char *name;
	/* Set to a non-zero if we need to force the clock frequency.
	 * If zero we leave the clock frequency as configure by the system
	 */
	uint32_t freq;

	struct clk *parent_clk;
	const char *parent_name;
};

struct ce_hal_dt_config {
	/* For now not too many things except clocks */
	struct ce_hal_clk *clk;
	uint32_t nb_clk;
	const char *default_session_name;
};

int ce_hal_dt_check(void);
int ce_hal_dt_get_pdata(struct platform_device *pdev);
int ce_hal_dt_put_pdata(struct platform_device *pdev);
int ce_hal_clk_enable(struct platform_device *pdev);
int ce_hal_clk_disable(struct platform_device *pdev);
const char *get_default_session_name(struct platform_device *pdev);

#endif
