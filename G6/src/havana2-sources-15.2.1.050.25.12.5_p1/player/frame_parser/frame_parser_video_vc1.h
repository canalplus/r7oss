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

#ifndef H_FRAME_PARSER_VIDEO_VC1
#define H_FRAME_PARSER_VIDEO_VC1

#include "vc1.h"
#include "frame_parser_video.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoVc1_c"

#define VC1_VLC_LEAF_ZERO                               0
#define VC1_VLC_LEAF_ONE                                1
#define VC1_VLC_LEAF_NONE                               2

#define VC1_VLC_CODE(MaxBits, BitsRead, Result)         (Result | (BitsRead << MaxBits))
#define VC1_VLC_BITSREAD(MaxBits, Result)               (Result >> MaxBits)
#define VC1_VLC_RESULT(MaxBits, Result)                 (Result & ((1 << MaxBits) - 1))

#define VC1_HIGHEST_LEVEL_SUPPORTED                     3
#define VC1_MAX_CODED_WIDTH                             1920
#define VC1_MAX_CODED_HEIGHT                            1088

#define DEFAULT_ANTI_EMULATION_REQUEST                  32
#define SLICE_ANTI_EMULATION_REQUEST                    8
#define METADATA_ANTI_EMULATION_REQUEST                 48

/// Frame parser for VC1 video.
class FrameParser_VideoVc1_c : public FrameParser_Video_c
{
public:
    FrameParser_VideoVc1_c(void);
    ~FrameParser_VideoVc1_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);
    FrameParserStatus_t   PresentCollatedHeader(unsigned char StartCode,
                                                unsigned char *HeaderBytes, FrameParserHeaderFlag_t *Flags);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(void);

    FrameParserStatus_t   PrepareReferenceFrameList(void);

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(void);

    FrameParserStatus_t   RevPlayProcessDecodeStacks(void);
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(void);

protected:
    Vc1StreamParameters_t      *StreamParameters;
    Vc1FrameParameters_t       *FrameParameters;

    Vc1StreamParameters_t       CopyOfStreamParameters;

    bool                        SequenceLayerMetaDataValid;

    static const unsigned int   BFractionNumerator[23];
    static const unsigned int   BFractionDenominator[23];
    static const unsigned char  Pquant[32];
    static const Vc1MvMode_t    MvModeLowRate[5];
    static const Vc1MvMode_t    MvModeHighRate[5];
    static const Vc1MvMode_t    MvMode2LowRate[4];
    static const Vc1MvMode_t    MvMode2HighRate[4];

    FrameParserStatus_t         ReadSequenceLayerMetadata(void);
    FrameParserStatus_t         ReadSequenceHeaderStruct_C(void);
    bool                        NewStreamParametersCheck(void);
    FrameParserStatus_t         CommitFrameForDecode(void);

    unsigned long               BitsDotGetVc1VLC(unsigned long               MaxBits,
                                                 unsigned long               LeafNode);
    void                        SetBFraction(unsigned int                    Fraction,
                                             unsigned int                   *Numerator,
                                             unsigned int                   *Denominator);

private:
    Vc1VideoPicture_t           FirstFieldPictureHeader;
    unsigned int                BackwardRefDist;

    bool                        RangeMapValid;
    bool                        RangeMapYFlag;
    unsigned int                RangeMapY;
    bool                        RangeMapUVFlag;
    unsigned int                RangeMapUV;

    //  Frame rate details
    bool                        FrameRateValid;
    Rational_t                  FrameRate;

    FrameParserStatus_t         ReadSequenceHeader(void);
    FrameParserStatus_t         ReadEntryPointHeader(void);
    FrameParserStatus_t         ReadSliceHeader(unsigned int                    pSlice);
    FrameParserStatus_t         ReadPictureHeader(unsigned int                  first_field);
    FrameParserStatus_t         ReadPictureHeaderAdvancedProfile(unsigned int   first_field);
    void                        ReadPictureHeaderProgressive(void);
    void                        ReadPictureHeaderInterlacedFrame(void);
    void                        ReadPictureHeaderInterlacedField(void);

    DISALLOW_COPY_AND_ASSIGN(FrameParser_VideoVc1_c);
};

#endif
