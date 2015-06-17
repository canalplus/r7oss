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

#ifndef H_FRAME_PARSER_VIDEO_MJPEG
#define H_FRAME_PARSER_VIDEO_MJPEG

#include "mjpeg.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoMjpeg_c"

class FrameParser_VideoMjpeg_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoMjpeg_c(void);
    ~FrameParser_VideoMjpeg_c(void);

    // FrameParser class functions
    FrameParserStatus_t         Connect(Port_c *Port);

    // Stream specific functions
    FrameParserStatus_t         ReadHeaders(void);
    //FrameParserStatus_t         RevPlayProcessDecodeStacks(                     void );
    //FrameParserStatus_t         RevPlayGeneratePostDecodeParameterSettings(     void );

private:
    MjpegStreamParameters_t     CopyOfStreamParameters;
    MjpegStreamParameters_t    *StreamParameters;
    MjpegFrameParameters_t     *FrameParameters;
    bool                        GetDefaultStreamMetadata;

    FrameParserStatus_t         ReadStreamMetadata(void);
    FrameParserStatus_t         ReadStartOfFrame(void);

    FrameParserStatus_t         CommitFrameForDecode(void);
    FrameParserStatus_t         ReadAPP0Marker(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoMjpeg_c);
};

#endif
