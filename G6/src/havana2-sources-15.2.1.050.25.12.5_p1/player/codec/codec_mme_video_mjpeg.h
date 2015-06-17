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

#ifndef H_CODEC_MME_VIDEO_MJPEG
#define H_CODEC_MME_VIDEO_MJPEG

#include "codec_mme_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoMjpeg_c"

#define USE_JPEG_HW_TRANSFORMER

#define MJPEG_DEFAULT_WIDTH                     720
#define MJPEG_DEFAULT_HEIGHT                    576

#if defined (USE_JPEG_HW_TRANSFORMER)
#else
#define MJPEG_NUM_MME_INPUT_BUFFERS             1
#define MJPEG_NUM_MME_OUTPUT_BUFFERS            1
#define MJPEG_NUM_MME_BUFFERS                   (MJPEG_NUM_MME_INPUT_BUFFERS+MJPEG_NUM_MME_OUTPUT_BUFFERS)
#define MJPEG_MME_CODED_DATA_BUFFER             1
#define MJPEG_MME_CURRENT_FRAME_BUFFER          0
#endif

/// The MJPEG video codec proxy.
class Codec_MmeVideoMjpeg_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoMjpeg_c(void);
    ~Codec_MmeVideoMjpeg_c(void);

private:
#if defined (USE_JPEG_HW_TRANSFORMER)
    JPEGDECHW_VideoDecodeCapabilityParams_t     MjpegTransformCapability;
    JPEGDECHW_VideoDecodeInitParams_t           MjpegInitializationParameters;
#else
    JPEGDEC_Capability_t                        MjpegTransformCapability;
#endif

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);

#if !defined (USE_JPEG_HW_TRANSFORMER) && !defined (USE_JPEG_FRAME_TRANSFORMER)
    CodecStatus_t   FillOutTransformCommand(void);
    CodecStatus_t   SendMMEDecodeCommand(void);
#endif
    CodecStatus_t   FillOutDecodeCommand(void);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t      *Context);
    CodecStatus_t   DumpSetStreamParameters(void                        *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);
};

#endif
