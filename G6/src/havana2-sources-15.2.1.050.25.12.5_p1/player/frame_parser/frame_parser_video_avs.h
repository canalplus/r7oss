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

#ifndef H_FRAME_PARSER_VIDEO_AVS
#define H_FRAME_PARSER_VIDEO_AVS

#include "avs.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoAvs_c"

#define AVS_PICTURE_DISTANCE_RANGE              256
#define AVS_PICTURE_DISTANCE_HALF_RANGE         (AVS_PICTURE_DISTANCE_RANGE >> 1)

class FrameParser_VideoAvs_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoAvs_c(void);
    ~FrameParser_VideoAvs_c(void);

    // FrameParser class functions
    FrameParserStatus_t   Connect(Port_c *Port);

    // Stream specific functions
    FrameParserStatus_t   ReadHeaders(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);
    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);
    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);

private:
    AvsStreamParameters_t   CopyOfStreamParameters;
    AvsStreamParameters_t  *StreamParameters;
    AvsFrameParameters_t   *FrameParameters;

    int                     LastPanScanHorizontalOffset;
    int                     LastPanScanVerticalOffset;

    bool                    EverSeenRepeatFirstField;
    bool                    LastFirstFieldWasAnI; // Support for self referencing IP field pairs

    int                     PictureDistanceBase;
    int                     LastPictureDistanceBase;
    unsigned int            LastPictureDistance;

    unsigned int            LastReferenceFramePictureCodingType;

    int                     ImgtrNextP;
    int                     ImgtrLastP;
    int                     ImgtrLastPrevP;

    FrameParserStatus_t     ReadSequenceHeader(void);
    FrameParserStatus_t     ReadSequenceDisplayExtensionHeader(void);
    FrameParserStatus_t     ReadCopyrightExtensionHeader(void);
    FrameParserStatus_t     ReadCameraParametersExtensionHeader(void);

    FrameParserStatus_t     ReadPictureHeader(unsigned int            PictureStartCode);
    FrameParserStatus_t     ReadPictureDisplayExtensionHeader(void);
    FrameParserStatus_t     ReadSliceHeader(unsigned int            StartCodeIndex);

    FrameParserStatus_t     CommitFrameForDecode(void);
    bool                    NewStreamParametersCheck(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoAvs_c);
};

#endif
