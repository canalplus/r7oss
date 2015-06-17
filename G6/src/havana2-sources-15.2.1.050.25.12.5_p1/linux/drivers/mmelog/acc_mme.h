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
// This file abstracts Multicom/mme.h for debugging of ACC_Multicom Transformers

#ifndef _ACC_MME_H_
#define _ACC_MME_H_

// Abstract multicom calls if value > 0 (EXPORT_SYMBOLs done in that case in _ACC_MME_WRAPPER_C_ file)
#define	_ACC_WRAPP_MME_	0

#ifndef _ACC_MME_WRAPPER_C_
#if _ACC_WRAPP_MME_ > 0
#define MME_InitTransformer			 acc_MME_InitTransformer
#define MME_SendCommand				 acc_MME_SendCommand
#define MME_AbortCommand			 acc_MME_AbortCommand
#define MME_TermTransformer			 acc_MME_TermTransformer
#define MME_GetTransformerCapability acc_MME_GetTransformerCapability
#endif // _ACC_MME_WRAPPER_C_

#endif // _ACC_WRAPP_MME_

#include <mme.h>

#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else
#define _EXTERN_C_ extern
#endif

_EXTERN_C_ MME_ERROR acc_MME_InitTransformer(const char *Name, MME_TransformerInitParams_t *Params_p,
                                             MME_TransformerHandle_t *Handle_p);
_EXTERN_C_ MME_ERROR acc_MME_GetTransformerCapability(const char *TransformerName,
                                                      MME_TransformerCapability_t *TransformerInfo_p);
_EXTERN_C_ MME_ERROR acc_MME_SendCommand(MME_TransformerHandle_t Handle, MME_Command_t *CmdInfo_p);
_EXTERN_C_ MME_ERROR acc_MME_TermTransformer(MME_TransformerHandle_t handle);

#endif // _ACC_MME_H_
