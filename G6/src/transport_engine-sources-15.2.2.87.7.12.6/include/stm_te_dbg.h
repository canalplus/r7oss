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

Source file name : stm_te_dbg.h

Declares pr_<level> format
******************************************************************************/
#ifndef __STM_TE_DBG
#define __STM_TE_DBG

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) KBUILD_MODNAME ".%s() - " fmt, __func__
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/ratelimit.h>

/* Define pr_warn for older kernels */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
#define pr_warn pr_warning
#define pr_warn_ratelimited pr_warning_ratelimited
#endif

#define stm_te_trace_in() pr_debug("[STM_TE API IN]\n")
#define stm_te_trace_out() pr_debug("[STM_TE API OUT]\n")
#define stm_te_trace_out_result(res) pr_debug("[STM_TE API OUT] - result %d\n",\
		res)
#endif

