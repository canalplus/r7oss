/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_te_main.c

Defines stm_te_api module init and exit functions and module parameters
******************************************************************************/

#include <stm_te_dbg.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/platform.h>
#endif

#include "te_global.h"
#include "te_object.h"
#include "te_demux.h"
#include "te_tsmux.h"
#include "te_internal_cfg.h"
#include "stm_te_version.h"
#include "te_hal_obj.h"
#include "te_sysfs.h"

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("STKPI Transport Engine");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");
MODULE_VERSION(TE_VERSION);

/* Global TE configuration parameters */
struct te_module_config te_cfg = TE_DEFAULT_CONFIG;

/* Module parameters */
module_param_named(max_queued_output_data, te_cfg.max_queued_output_data, uint,
		0644);
module_param_named(ts_buffer_size, te_cfg.ts_buffer_size, uint, 0644);
module_param_named(section_buffer_size, te_cfg.section_buffer_size, uint, 0644);
module_param_named(pes_buffer_size, te_cfg.pes_buffer_size, uint, 0644);
module_param_named(pcr_buffer_size, te_cfg.pcr_buffer_size, uint, 0644);
module_param_named(index_buffer_size, te_cfg.index_buffer_size, uint, 0644);
module_param_named(tsg_index_buffer_size, te_cfg.tsg_index_buffer_size, uint,
		0644);
module_param_named(multiplex_ahead_limit, te_cfg.multiplex_ahead_limit, uint,
		0664);
module_param_named(dts_integrity_threshold, te_cfg.dts_integrity_threshold,
		uint, 0664);

/**
 * \brief Checks that current TE configuration parameters are valid
 */
static int __init pm_te_check_options(void)
{
	if (te_cfg.max_queued_output_data == 0) {
		pr_err("Max queued output data cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.ts_buffer_size == 0) {
		pr_err("TS buffer size cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.section_buffer_size == 0) {
		pr_err("Section buffer size cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.pes_buffer_size == 0) {
		pr_err("PES buffer size cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.pcr_buffer_size == 0) {
		pr_err("PCR buffer size cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.index_buffer_size == 0) {
		pr_err("Index buffer size cannot be zero\n");
		return -EINVAL;
	}

	if (te_cfg.max_queued_output_data < te_cfg.section_buffer_size) {
		pr_err("Max queued output data cannot be less than section buffer size\n");
		return -EINVAL;
	}

	return 0;
}

static int __init stm_te_module_init(void)
{
	int err;

	err = pm_te_check_options();
	if (err)
		return err;

	err = te_global_init();
	if (err)
		return err;

	return 0;
}
module_init(stm_te_module_init);

static void __exit stm_te_module_exit(void)
{
	te_global_term();
}
module_exit(stm_te_module_exit);
