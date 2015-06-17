/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/module.h>

#include "osdev_mem.h"

MODULE_DESCRIPTION("OS device abstraction functions");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

#if defined(SE_DEBUG_MEMORY) &&  (SE_DEBUG_MEMORY > 0)
/* --- memory activity tracking --- */

int dmc_prints[DMC_LAST];
EXPORT_SYMBOL(dmc_prints);

module_param_array(dmc_prints, int, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dmc_prints, "individual dump memory check masks");

static int dmc_print_mccs = 1;
module_param(dmc_print_mccs, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dmc_print_mccs, "dump memory check counters");

struct semaphore dumpmemchk_mutex = __SEMAPHORE_INITIALIZER(dumpmemchk_mutex, 1);
EXPORT_SYMBOL(dumpmemchk_mutex);

int dmc_counts[DMC_LAST];
EXPORT_SYMBOL(dmc_counts);

void OSDEV_Dump_MemCheckCounters(const char *str)
{
	_LOCKM();
	if (dmc_print_mccs) {
		pr_info("SeMemCheckCounts: %s newk:%d newv:%d kmem:%d bigphys:%d bpa2:%d thd:%d tune:%d\n",
		        str,
		        dmc_counts[DMC_NEWK],
		        dmc_counts[DMC_NEWV],
		        dmc_counts[DMC_KMEM],
		        dmc_counts[DMC_BIGPHYS],
		        dmc_counts[DMC_BPA2],
		        dmc_counts[DMC_THREAD],
		        dmc_counts[DMC_TUNE]);
	}
	_UNLOCKM();
}

#else
void OSDEV_Dump_MemCheckCounters(const char *str) {}

#endif // SE_DEBUG_MEMORY

EXPORT_SYMBOL(OSDEV_Dump_MemCheckCounters);
