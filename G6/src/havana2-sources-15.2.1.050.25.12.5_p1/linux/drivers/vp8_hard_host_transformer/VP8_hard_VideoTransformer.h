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

/************************************************************************
Portions (C) COPYRIGHT 2006 HANTRO PRODUCTS OY
************************************************************************/

#ifndef __VP8_HWDEC_VIDEOTRANSFORMER_H
#define __VP8_HWDEC_VIDEOTRANSFORMER_H

#include <mme.h>

MME_ERROR VP8DEC_ProcessCommand(void *handle, MME_Command_t *commandInfo);
MME_ERROR VP8DEC_InitTransformer(MME_UINT size, MME_GenericParams_t params, void  **handle);
MME_ERROR VP8DEC_TermTransformer(void *handle);
MME_ERROR VP8DEC_Abort(void *handle, MME_CommandId_t CmdId);
MME_ERROR VP8DEC_GetTransformerCapability(MME_TransformerCapability_t *capability);

#endif /* #ifndef __VP8_HWDEC_VIDEOTRANSFORMER_H */
