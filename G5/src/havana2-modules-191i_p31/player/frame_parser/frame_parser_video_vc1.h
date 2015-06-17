/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : frame_parser_video_vc1.h
Author :           Mark C

Definition of the frame parser video vc1 class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
02-Mar-07   Created                                         Mark C

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_VC1
#define H_FRAME_PARSER_VIDEO_VC1

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "vc1.h"
#include "frame_parser_video.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

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


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Frame parser for VC1 video.
class FrameParser_VideoVc1_c : public FrameParser_Video_c
{
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
    int32_t                     FrameRateDefaultDen;
    int32_t                     FrameRateDefaultNum;


    // Functions

    FrameParserStatus_t         ReadSequenceHeader(                     void );
    FrameParserStatus_t         ReadEntryPointHeader(                   void );
    FrameParserStatus_t         ReadSliceHeader(                        unsigned int                    pSlice );
    FrameParserStatus_t         ReadPictureHeader(                      unsigned int                    first_field );
    FrameParserStatus_t         ReadPictureHeaderAdvancedProfile(       unsigned int                    first_field );
    void                        ReadPictureHeaderProgressive           (void);
    void                        ReadPictureHeaderInterlacedFrame       (void);
    void                        ReadPictureHeaderInterlacedField       (void);

protected:

    // Data

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

    // Functions

    FrameParserStatus_t         ReadSequenceLayerMetadata(              void );
    bool                        NewStreamParametersCheck(               void );
    FrameParserStatus_t         CommitFrameForDecode(                   void );

    unsigned long               BitsDotGetVc1VLC(                       unsigned long                   MaxBits,
                                                                        unsigned long                   LeafNode);
    void                        SetBFraction                           (unsigned int                    Fraction,
                                                                        unsigned int*                   Numerator,
                                                                        unsigned int*                   Denominator);

public:

    //
    // Constructor function
    //

    FrameParser_VideoVc1_c( void );
    ~FrameParser_VideoVc1_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset(                void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(     Ring_t          Ring );

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(                                          void );

    FrameParserStatus_t   PrepareReferenceFrameList(                            void );

    FrameParserStatus_t   ForPlayUpdateReferenceFrameList(                      void );

    FrameParserStatus_t   RevPlayProcessDecodeStacks(                           void );
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(           void );
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(                  void );
};

#endif

