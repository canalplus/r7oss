/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_ce_fuse.h

Declares STKPI Crypto Engine (CE) API constants, types and functions
************************************************************************/

#ifndef __STM_CE_FUSE_H
#define __STM_CE_FUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm_common.h"

#define CE_FUSE_MAX_DATA (4)
#define CE_FUSE_NAME_MAX_SIZE (40)

/** Fuse Read/Write Command */
enum stm_ce_fuse_command {
	FC_READ    =0,
	FC_WRITE,
	FC_WRITE_AND_LOCK,
	FC_LOCK,
	FC_LAST
};

/** Fuse Operation */
struct stm_ce_fuse_op {
	enum stm_ce_fuse_command command;
	char fuse_name[CE_FUSE_NAME_MAX_SIZE];
	uint32_t fuse_value[CE_FUSE_MAX_DATA];
};

#ifdef __cplusplus
}
#endif

#endif

