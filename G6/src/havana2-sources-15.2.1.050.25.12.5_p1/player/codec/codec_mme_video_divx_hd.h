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

#ifndef H_CODEC_MME_VIDEO_DIVX_HD
#define H_CODEC_MME_VIDEO_DIVX_HD

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoDivxHd_c"

#include <mme.h>

#include "codec_mme_video.h"
#include "mpeg4.h"

// /////////////////////////////////////////////////////////////////////////
//
// Backwards Compatibility (April 11 2011)
//

// MPEG4P2/DIVX codec was changed from DivXHD to DivX at Version 18
#if (MPEG4P2_MME_VERSION < 18)
//#define Divx_TransformerCapability_t    MME_DivXVideoDecodeCapabilityParams_t
#define Divx_InitTransformerParam_t       MME_DivXVideoDecodeInitParams_t

#define Divx_TransformerCapability_t      MME_DivXHD_Capability_t
#define MME_DivXVideoDecodeInitParams_t   MME_DivXHDVideoDecodeInitParams_t
#define MME_DivXSetGlobalParamSequence_t  MME_DivXHDSetGlobalParamSequence_t
#define MME_DivXVideoDecodeReturnParams_t MME_DivXHDVideoDecodeReturnParams_t
#define MME_DivXVideoDecodeParams_t       MME_DivXHDVideoDecodeParams_t

#define DivX_CompressedData_t             DivXHD_CompressedData_t
#define DivX_LumaAddress_t                DivXHD_LumaAddress_t
#define DivX_ChromaAddress_t              DivXHD_ChromaAddress_t
#define DivX_PictureType_t                unsigned int

#define DIVX_MAINOUT_EN                   DIVXHD_MAINOUT_EN
#define DIVX_HDEC_1                       DIVXHD_HDEC_1
#define DIVX_VDEC_1                       DIVXHD_VDEC_1
#define DIVX_AUX_MAIN_OUT_EN              DIVXHD_AUX_MAIN_OUT_EN
#define DIVX_HDEC_ADVANCED_2              DIVXHD_HDEC_ADVANCED_2
#define DIVX_VDEC_ADVANCED_2_PROG         DIVXHD_VDEC_ADVANCED_2_PROG
#define DIVX_HDEC_ADVANCED_4              DIVXHD_HDEC_ADVANCED_4
#define DIVX_VDEC_ADVANCED_2_PROG         DIVXHD_VDEC_ADVANCED_2_PROG

#endif

class Codec_MmeVideoDivxHd_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoDivxHd_c(void);
    ~Codec_MmeVideoDivxHd_c(void);

protected:
#if (MPEG4P2_MME_VERSION >= 20)
    Divx_TransformerCapability_t      DivxTransformCapability;
#endif
    MME_DivXVideoDecodeInitParams_t   DivxInitializationParameters;

    MME_DivXSetGlobalParamSequence_t  DivXGlobalParamSequence;

    MME_DivXVideoDecodeReturnParams_t ReturnParams;
    MME_DivXVideoDecodeParams_t       DecodeParams;

    unsigned int MaxWidth;
    unsigned int MaxHeight;

    bool IsDivX;
    bool RasterOutput;

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);

#if (MPEG4P2_MME_VERSION >= 13)
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);
#endif

    CodecStatus_t   DumpSetStreamParameters(void *Parameters);
    CodecStatus_t   DumpDecodeParameters(void *Parameters);
    CodecStatus_t   InitializeMMETransformer(void);
    CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request);
};

#endif
