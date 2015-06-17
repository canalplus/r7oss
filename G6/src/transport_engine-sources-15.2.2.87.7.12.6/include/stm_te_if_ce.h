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

Source file name : stm_te_if_ce.h

Declares the private interface for communicating between stm_te pid filters
and stm_ce transforms
******************************************************************************/
#ifndef __STM_TE_IF_CE_H
#define __STM_TE_IF_CE_H

#include "stm_common.h"

#define STM_PID_FILTER_INTERFACE "stm_te-pid-interface"
typedef struct {
	int (*disconnect)(stm_object_h filter, stm_object_h transform);
} stm_te_pid_interface_t;

#define STM_PATH_ID_INTERFACE "stm_ce-path-interface"
typedef struct {
	int (*connect)(stm_object_h transform, stm_object_h filter,
			uint32_t *path_id);
	int (*disconnect)(stm_object_h transform, stm_object_h filter);
} stm_ce_path_interface_t;

#endif
