/************************************************************************
 * Copyright (C) 2015 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 * Implementation of debugging mechanisms.
 *************************************************************************/

#ifndef __STV_DEBUG_H_
#define __STV_DEBUG_H_

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) KBUILD_MODNAME ".%s():%d - " fmt, __func__ , __LINE__

/* Mapping of STV_* marcos to kernel pr_* macros . */
#define stv_emerg		pr_emerg
#define stv_alert		pr_alert
#define stv_crit		pr_crit
#define stv_err			pr_err
#define stv_warning		pr_warning
#define stv_notice		pr_notice
#define stv_info		pr_info
#define stv_debug		pr_debug
#define stv_cont		pr_cont

#define stv_default		stv_warning

#define stv_info_ratelimited pr_info_ratelimited

#endif
