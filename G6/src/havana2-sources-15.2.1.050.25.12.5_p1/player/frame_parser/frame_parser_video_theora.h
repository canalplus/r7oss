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

#ifndef H_FRAME_PARSER_VIDEO_THEORA
#define H_FRAME_PARSER_VIDEO_THEORA

#include "theora.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoTheora_c"

#define THEORA_REFERENCE_FRAMES_FOR_FORWARD_DECODE     1
#define THEORA_MIN_FRAME_WIDTH                         16
#define THEORA_MAX_FRAME_WIDTH                         720
#define THEORA_MIN_FRAME_HEIGHT                        16
#define THEORA_MAX_FRAME_HEIGHT                        576

typedef enum
{
    TheoraNoHeaders                     = 0x00,
    TheoraIdentificationHeader          = 0x01,
    TheoraCommentHeader                 = 0x02,
    TheoraSetupHeader                   = 0x04,
    TheoraAllHeaders                    = 0x07,
} TheoraHeader_t;

/// Frame parser for Theora
class FrameParser_VideoTheora_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoTheora_c(void);
    ~FrameParser_VideoTheora_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t         Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    FrameParserStatus_t         ReadHeaders(void);

    FrameParserStatus_t         PrepareReferenceFrameList(void);

    FrameParserStatus_t         ForPlayUpdateReferenceFrameList(void);

    FrameParserStatus_t         RevPlayProcessDecodeStacks(void);

private:
    TheoraStreamParameters_t   *StreamParameters;
    TheoraFrameParameters_t    *FrameParameters;
    unsigned int                StreamHeadersRead;

    Rational_t                  FrameRate;
    Rational_t                  PixelAspectRatio;

    FrameParserStatus_t         ReadStreamHeaders(void);
    FrameParserStatus_t         ReadPictureHeader(void);
    int                         ReadHuffmanTree(TheoraVideoSequence_t  *SequenceHeader);

    FrameParserStatus_t         CommitFrameForDecode(void);
    bool                        NewStreamParametersCheck(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoTheora_c);
};

#endif
