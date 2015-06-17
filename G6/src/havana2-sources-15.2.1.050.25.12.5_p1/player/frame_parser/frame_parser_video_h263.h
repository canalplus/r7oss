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

#ifndef H_FRAME_PARSER_VIDEO_H263
#define H_FRAME_PARSER_VIDEO_H263

#include "h263.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoH263_c"

#define H263_REFERENCE_FRAMES_FOR_FORWARD_DECODE                1
#define H263_TEMPORAL_REFERENCE_INVALID                         0x1000

/// Frame parser for H263
class FrameParser_VideoH263_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoH263_c(void);
    ~FrameParser_VideoH263_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(void);

    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);

    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);

protected:

    H263StreamParameters_t     *StreamParameters;
    H263FrameParameters_t      *FrameParameters;
    H263StreamParameters_t      CopyOfStreamParameters;

    unsigned int                TemporalReference;

    FrameParserStatus_t         CommitFrameForDecode(void);

private:
    FrameParserStatus_t         H263ReadPictureHeader(void);

    bool                        NewStreamParametersCheck(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoH263_c);
};

#endif

