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

#ifndef H_FRAME_PARSER_VIDEO_MPEG2
#define H_FRAME_PARSER_VIDEO_MPEG2

#include "mpeg2.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoMpeg2_c"

class FrameParser_VideoMpeg2_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoMpeg2_c(void);
    ~FrameParser_VideoMpeg2_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);
    FrameParserStatus_t PresentCollatedHeader(unsigned char StartCode,
                                              unsigned char *HeaderBytes, FrameParserHeaderFlag_t *Flags);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   GetMpeg2TimeCode(stm_se_ctrl_mpeg2_time_code_t *TimeCode);
    FrameParserStatus_t   ReadHeaders(void);
    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);
    FrameParserStatus_t   ForPlayCheckForReferenceReadyForManifestation(void);

    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(void);
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(void);
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(void);
    bool                  ReadAdditionalUserDataParameters(void);

private:
    Mpeg2StreamParameters_t       CopyOfStreamParameters;

    Mpeg2StreamParameters_t      *StreamParameters;
    Mpeg2FrameParameters_t       *FrameParameters;

    int                   LastPanScanHorizontalOffset;
    int                   LastPanScanVerticalOffset;

    bool                  EverSeenRepeatFirstField;     // Support for PolicyMPEG2DoNotHonourProgressiveFrameFlag

    bool                  LastFirstFieldWasAnI;         // Support for self referencing IP field pairs
    unsigned int          LastRecordedTemporalReference;

    FrameParserStatus_t   ReadSequenceHeader(void);
    FrameParserStatus_t   ReadSequenceExtensionHeader(void);
    FrameParserStatus_t   ReadSequenceDisplayExtensionHeader(void);
    FrameParserStatus_t   ReadSequenceScalableExtensionHeader(void);
    FrameParserStatus_t   ReadGroupOfPicturesHeader(void);
    FrameParserStatus_t   ReadPictureHeader(void);
    FrameParserStatus_t   ReadPictureCodingExtensionHeader(void);
    FrameParserStatus_t   ReadQuantMatrixExtensionHeader(void);
    FrameParserStatus_t   ReadPictureDisplayExtensionHeader(void);
    FrameParserStatus_t   ReadPictureTemporalScalableExtensionHeader(void);
    FrameParserStatus_t   ReadPictureSpatialScalableExtensionHeader(void);

    void                  StoreTemporalReferenceForLastRecordedFrame(ParsedFrameParameters_t *ParsedFrame);

    bool                  NewStreamParametersCheck(void);
    FrameParserStatus_t   CommitFrameForDecode(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoMpeg2_c);
};

#endif
