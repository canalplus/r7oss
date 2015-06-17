/************************************************************************
 * Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : module_init.c

Main entry point for the STLinuxTV module

Date        Modification                                    Name
----        ------------                                    --------

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>

#include "stat.h"

#ifdef CONFIG_STLINUXTV_SOFTWARE_DEMUX
int __init StmLoadModule(void);
void __exit StmUnloadModule(void);
#endif

#ifdef CONFIG_STLINUXTV_FRONTEND_DEMUX
int32_t __init stm_dvb_init(void);
void __exit stm_dvb_exit(void);
#endif

void stm_v4l2_capture_init(void);
void stm_v4l2_capture_exit(void);

static int32_t __init stlinuxtv_init(void)
{
	int32_t ret;

	ret = stlinuxtv_stat_register_class();
	if (ret)
		return ret;

#ifdef CONFIG_STLINUXTV_FRONTEND_DEMUX
	ret = stm_dvb_init();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
#ifdef CONFIG_STLINUXTV_SOFTWARE_DEMUX
	ret = StmLoadModule();
	if (ret)
		return ret;
#endif

	stm_v4l2_capture_init();
#endif

	return 0;
}

module_init(stlinuxtv_init);

static void __exit stlinuxtv_exit(void)
{
#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	stm_v4l2_capture_exit();

#ifdef CONFIG_STLINUXTV_SOFTWARE_DEMUX
	StmUnloadModule();
#endif
#endif

#ifdef CONFIG_STLINUXTV_FRONTEND_DEMUX
	stm_dvb_exit();
#endif
	stlinuxtv_stat_unregister_class();
}

module_exit(stlinuxtv_exit);

MODULE_DESCRIPTION("STLinuxTV adaptation driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STLINUXTV_VERSION);
MODULE_LICENSE("GPL");
