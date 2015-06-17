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

#ifndef H_FRAME_PARSER_VIDEO_UNCOMPRESSED
#define H_FRAME_PARSER_VIDEO_UNCOMPRESSED

#include "uncompressed.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoUncompressed_c"

typedef struct UncompressedStreamParameters_s
{
    int dummy;
} UncompressedStreamParameters_t;

typedef struct UncompressedFrameParameters_s
{
    int dummy;
} UncompressedFrameParameters_t;

class FrameParser_VideoUncompressed_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoUncompressed_c(void);
    ~FrameParser_VideoUncompressed_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);


    //
    // Extensions to the class to be fulfilled by my inheritors,
    // these are required to support the process buffer override
    //

    FrameParserStatus_t   ReadHeaders(void);

    FrameParserStatus_t   PrepareReferenceFrameList(void);

private:
    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoUncompressed_c);
};

#endif // H_FRAME_PARSER_VIDEO_UNCOMPRESSED
