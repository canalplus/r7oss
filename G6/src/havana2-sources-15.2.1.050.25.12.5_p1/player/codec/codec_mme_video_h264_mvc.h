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

#ifndef H_CODEC_MME_VIDEO_H264_MVC
#define H_CODEC_MME_VIDEO_H264_MVC

#include "codec_mme_video_h264.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoH264_MVC_c"

class Codec_MmeVideoH264_MVC_c : public Codec_MmeVideoH264_c
{
public:
    Codec_MmeVideoH264_MVC_c(void);
    ~Codec_MmeVideoH264_MVC_c(void);

    CodecStatus_t   Halt(void);

    CodecStatus_t   Input(Buffer_t  CodedBuffer);
    CodecStatus_t   Connect(Port_c *Port);

private:
    // specifies whether Codec_MmeVideo_c::Input() is processing the base view or the dependent view
    bool                     isBaseView;

    // Mapping between Decode Frame Indexes and dependent view Buffer Decode Index
    unsigned int             IndexDepBufferMapSize;
    CodecIndexBufferMap_t   *IndexDepBufferMap;

    // Used in MME callback: last decodes
    unsigned int             LastDecodeFrameIndex;
    unsigned int             LastBaseDecodeBufferIndex;
    unsigned int             LastDepDecodeBufferIndex;

    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   TranslateReferenceFrameLists(bool IncrementUseCountForReferenceFrame);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   DumpSetStreamParameters(void *Parameters);
    CodecStatus_t   DumpDecodeParameters(void    *Parameters);
    CodecStatus_t   ReleaseDecodeContext(CodecBaseDecodeContext_t *Context);
    void            UnuseCodedBuffer(Buffer_t CodedBuffer, FramesInPreprocessorChain_t *PPentry);

    // Dependent view management of the mapping "Decode Frame Index <-> Decode Buffer Index"
    CodecStatus_t   MapBufferToDecodeIndex(unsigned int        DecodeIndex,
                                           unsigned int        BufferIndex);
    CodecStatus_t   UnMapBufferIndex(unsigned int              BufferIndex);
    CodecStatus_t   TranslateDecodeIndex(unsigned int          DecodeIndex,
                                         unsigned int         *BufferIndex);

    // Unfortunately, the method we need to overload is prefixed with H264 :-(
    CodecStatus_t   H264ReleaseReferenceFrame(unsigned int       ReferenceFrameDecodeIndex);

    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideoH264_MVC_c);
};
#endif
