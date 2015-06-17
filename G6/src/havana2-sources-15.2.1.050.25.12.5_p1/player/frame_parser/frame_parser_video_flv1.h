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

#ifndef H_FRAME_PARSER_VIDEO_FLV1
#define H_FRAME_PARSER_VIDEO_FLV1

#include "h263.h"
#include "frame_parser_video_h263.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoFlv1_c"

#define FLV1_MIN_FRAME_WIDTH        16
#define FLV1_MAX_FRAME_WIDTH        720
#define FLV1_MIN_FRAME_HEIGHT       16
#define FLV1_MAX_FRAME_HEIGHT       576

/// Frame parser for Flv1
class FrameParser_VideoFlv1_c : public FrameParser_VideoH263_c
{
public:

    FrameParser_VideoFlv1_c(void);

    FrameParserStatus_t         ReadHeaders(void);

private:

    FrameParserStatus_t         FlvReadPictureHeader(void);
};

#endif

