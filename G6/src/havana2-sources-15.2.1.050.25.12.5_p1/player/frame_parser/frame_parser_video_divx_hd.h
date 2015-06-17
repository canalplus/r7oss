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

#ifndef H_FRAME_PARSER_VIDEO_DIVX_HD
#define H_FRAME_PARSER_VIDEO_DIVX_HD

#include "frame_parser_video_divx.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoDivxHd_c"

#define QUANT_MATRIX_SIZE 64

class FrameParser_VideoDivxHd_c : public FrameParser_VideoDivx_c
{
public:
    FrameParser_VideoDivxHd_c(void);

private:
    struct MPEG4MetaData_s        mMetaData;
    bool                          mIsMetaDataConfigurationSet;

    FrameParserStatus_t  ReadVopHeader(Mpeg4VopHeader_t         *Vop);
    FrameParserStatus_t  ReadHeaders(void);
    FrameParserStatus_t  ReadPictureHeader(void);
    void                 ReadStreamMetadata(void);
    FrameParserStatus_t  ConstructDefaultVolHeader(void);
    FrameParserStatus_t  AllocateStreamParameterSet(void);
};

#endif // H_FRAME_PARSER_VIDEO_DIVX_HD
