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

Source file name : frame_parser_video_vc1.cpp
Author :           Mark C

Implementation of the VC-1 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Mar-07   Created                                         Mark C

************************************************************************/


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_vc1.h"
#include "ring_generic.h"

//#define DUMP_HEADERS
#define REMOVE_ANTI_EMULATION_BYTES

//{{{  Locally defined constants and macros
static BufferDataDescriptor_t     Vc1StreamParametersBuffer = BUFFER_VC1_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     Vc1FrameParametersBuffer = BUFFER_VC1_FRAME_PARAMETERS_TYPE;

//

static struct Count_s
{
    bool                LegalValue;
    unsigned int        PanScanCountValue;
    unsigned int        DisplayCount0Value;
    unsigned int        DisplayCount1Value;
} Counts[] = 
{
                                // ProgSeq  Frame   TFF   RFF
    { true,  1, 1, 0 },         //    0       0      0     0
    { false, 0, 0, 0 },         //    0       0      0     1
    { true,  1, 1, 0 },         //    0       0      1     0
    { false, 0, 0, 0 },         //    0       0      1     1
    { true,  2, 1, 1 },         //    0       1      0     0
    { true,  3, 2, 1 },         //    0       1      0     1
    { true,  2, 1, 1 },         //    0       1      1     0
    { true,  3, 2, 1 },         //    0       1      1     1
    { false, 0, 0, 0 },         //    1       0      0     0
    { false, 0, 0, 0 },         //    1       0      0     1
    { false, 0, 0, 0 },         //    1       0      1     0
    { false, 0, 0, 0 },         //    1       0      1     1
    { true,  1, 1, 0 },         //    1       1      0     0
    { true,  2, 2, 0 },         //    1       1      0     1
    { false, 0, 0, 0 },         //    1       1      1     0
    { true,  3, 3, 0 }          //    1       1      1     1
};

#define CountsIndex( PS, F, TFF, RFF)           ((((PS) != 0) << 3) | (((F) != 0) << 2) | (((TFF) != 0) << 1) | ((RFF) != 0))
#define Legal( PS, F, TFF, RFF)                 Counts[CountsIndex(PS, F, TFF, RFF)].LegalValue
#define PanScanCount( PS, F, TFF, RFF)         Counts[CountsIndex(PS, F, TFF, RFF)].PanScanCountValue
#define DisplayCount0( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount0Value
#define DisplayCount1( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount1Value

//

#define MIN_LEGAL_ASPECT_RATIO_CODE     1
#define MAX_LEGAL_ASPECT_RATIO_CODE     4

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     AspectRatioValues[][2]  =
{ 
        {  0,   1 },   // 0 - Unspecified
        {  1,   1 },   // 1 - Square
        { 11,  12 },   // 2
        { 11,  10 },   // 3
        { 11,  16 },   // 4
        { 33,  40 },   // 5
        { 11,  24 },   // 6
        { 11,  20 },   // 7
        { 11,  32 },   // 8
        { 33,  80 },   // 9
        { 11,  18 },   // 10
        { 11,  15 },   // 11
        { 33,  64 },   // 12
        { 99, 160 },   // 13
        {  0,   1 }    // 14 - Reserved
};
#define AspectRatios(N) Rational_t(AspectRatioValues[N][0],AspectRatioValues[N][1])


static unsigned int     FieldPicture1[] =
{
        VC1_PICTURE_CODING_TYPE_I,
        VC1_PICTURE_CODING_TYPE_I,
        VC1_PICTURE_CODING_TYPE_P,
        VC1_PICTURE_CODING_TYPE_P,
        VC1_PICTURE_CODING_TYPE_B,
        VC1_PICTURE_CODING_TYPE_B,
        VC1_PICTURE_CODING_TYPE_BI,
        VC1_PICTURE_CODING_TYPE_BI
};

static unsigned int     FieldPicture2[] =
{
        VC1_PICTURE_CODING_TYPE_I,
        VC1_PICTURE_CODING_TYPE_P,
        VC1_PICTURE_CODING_TYPE_I,
        VC1_PICTURE_CODING_TYPE_P,
        VC1_PICTURE_CODING_TYPE_B,
        VC1_PICTURE_CODING_TYPE_BI,
        VC1_PICTURE_CODING_TYPE_B,
        VC1_PICTURE_CODING_TYPE_BI
};

//

#define MIN_LEGAL_FRAME_RATE_CODE               1
#define MAX_LEGAL_FRAME_RATE_CODE               8

static unsigned int     FrameRateValues[][2]    =
{
        { 0,        1 },        // Forbidden
        { 24,       1 },
        { 25,       1 },
        { 30,       1 },
        { 50,       1 },
        { 60,       1 },
        { 48,       1 },
        { 72,       1 },
        { 0,        1 },        // Forbidden
        { 24000, 1001 },
        { 25000, 1001 },
        { 30000, 1001 },
        { 50000, 1001 },
        { 60000, 1001 },
        { 48000, 1001 },
        { 72000, 1001 }
};
#define FrameRates(N) Rational_t(FrameRateValues[N][0],FrameRateValues[N][1])

static PictureStructure_t       PictureStructures[]     =
{
        StructureTopField,
        StructureBottomField,
        StructureBottomField,
        StructureTopField,
};

#define DEFAULT_DECODE_TIME_OFFSET                      3600            // 40ms in 90Khz ticks
#define MAXIMUM_DECODE_TIME_OFFSET                      (4 * 90000)     // 4s in 90Khz ticks

static const unsigned int ReferenceFramesRequired[VC1_PICTURE_CODING_TYPE_SKIPPED+1]    = {0, 0, 1, 2, 0, 1};

static SliceType_t SliceTypeTranslation[]  = { INVALID_INDEX, SliceTypeI, SliceTypeP, SliceTypeB, SliceTypeI, INVALID_INDEX };


//#define REFERENCE_FRAMES_NEEDED( CodingType)           (CodingType - 1)
#define REFERENCE_FRAMES_NEEDED(CodingType)             ReferenceFramesRequired[CodingType]
#define MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE         REFERENCE_FRAMES_NEEDED(VC1_PICTURE_CODING_TYPE_B)

#define Assert(L)               if( !(L) )                                                                      \
                                {                                                                               \
                                    report ( severity_error, "Assertion fail %s %d\n", __FUNCTION__, __LINE__ );\
                                    Player->MarkStreamUnPlayable( Stream );                                     \
                                    return FrameParserError;                                                    \
                                }

#define AssertAntiEmulationOk(Text)                                                                             \
                                {                                                                               \
                                FrameParserStatus_t     Status;                                                 \
                                    Status  = TestAntiEmulationBuffer();                                        \
                                    if( Status != FrameParserNoError )                                          \
                                    {                                                                           \
                                        report( severity_error, "FrameParser_VideoVc1_c::%s - Anti Emulation Test fail.\n", Text );    \
                                        Player->MarkStreamUnPlayable( Stream );                                 \
                                        return FrameParserError;                                                \
                                    }                                                                           \
                                }

struct FrameRate_s
{
    unsigned int        AverageTimePerFrame;
    unsigned int        FrameRateNumerator;
    unsigned int        FrameRateDenominator;
};

#define RECOGNISED_FRAME_RATES          12
static const struct FrameRate_s         FrameRateList[RECOGNISED_FRAME_RATES] =
{   {416666, 1, 1},
    {417083, 1, 2},
    {400000, 2, 1},
    {333333, 3, 1},
    {333666, 3, 2},
    {200000, 4, 1},
    {166666, 5, 1},
    {166833, 5, 2},
    {208333, 6, 1},
    {208541, 6, 2},
    {138888, 7, 1},
    {139027, 7, 2},
};
//}}}

//{{{  class constants and macros

const unsigned int      FrameParser_VideoVc1_c::BFractionNumerator[23]          =
    {1, 1, 2, 1, 3, 1, 2, 3, 4, 1, 5, 1, 2, 3, 4, 5, 6, 1, 3, 5, 7, 0, 0};

const unsigned int      FrameParser_VideoVc1_c::BFractionDenominator[23]        =
    {2, 3, 3, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 0, 0};

// See table 36 7.1.1.6
const unsigned char     FrameParser_VideoVc1_c::Pquant[32]   =
    {0,1,2,3,4,5,6,7,8,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,27,29,31};

// See tables 46, 47, 48, 49 7.1.1.32 and 7.1.1.33
const Vc1MvMode_t       FrameParser_VideoVc1_c::MvModeLowRate[5]   =
{
    VC1_MV_MODE_MV_HALF_PEL_BI,
    VC1_MV_MODE_MV,
    VC1_MV_MODE_MV_HALF_PEL,
    VC1_MV_MODE_INTENSITY_COMP,
    VC1_MV_MODE_MV_MIXED
};

const Vc1MvMode_t       FrameParser_VideoVc1_c::MvModeHighRate[5]  =
{
    VC1_MV_MODE_MV,
    VC1_MV_MODE_MV_MIXED,
    VC1_MV_MODE_MV_HALF_PEL,
    VC1_MV_MODE_INTENSITY_COMP,
    VC1_MV_MODE_MV_HALF_PEL_BI
};

const Vc1MvMode_t       FrameParser_VideoVc1_c::MvMode2LowRate[4]  =
{
    VC1_MV_MODE_MV_HALF_PEL_BI,
    VC1_MV_MODE_MV,
    VC1_MV_MODE_MV_HALF_PEL,
    VC1_MV_MODE_MV_MIXED
};

const Vc1MvMode_t       FrameParser_VideoVc1_c::MvMode2HighRate[4] =
{
    VC1_MV_MODE_MV,
    VC1_MV_MODE_MV_MIXED,
    VC1_MV_MODE_MV_HALF_PEL,
    VC1_MV_MODE_MV_HALF_PEL_BI
};


//}}}

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoVc1_c::FrameParser_VideoVc1_c (void)
{
    Configuration.FrameParserName               = "VideoVc1";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &Vc1StreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &Vc1FrameParametersBuffer;

//

    SequenceLayerMetaDataValid                  = false;
    FrameRateValid                              = false;

    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    Reset();

}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoVc1_c::~FrameParser_VideoVc1_c (void)
{
    Halt();
    Reset();
}


//{{{  Reset
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      The Reset function release any resources, and reset all variable
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::Reset (void)
{

    Configuration.SupportSmoothReversePlay      = true;

    memset (&CopyOfStreamParameters, 0x00, sizeof(Vc1StreamParameters_t));
    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));
    memset (&FirstFieldPictureHeader, 0x00, sizeof(Vc1VideoPicture_t));

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    FirstDecodeOfFrame                          = true;

    RangeMapValid                               = false;
    RangeMapYFlag                               = false;
    RangeMapY                                   = 0;
    RangeMapUVFlag                              = false;
    RangeMapUV                                  = 0;

    FrameRateDefaultDen                         = 1001;
    FrameRateDefaultNum                         = 24000;

    return FrameParser_Video_c::Reset();
}
//}}}
//{{{  RegisterOutputBufferRing
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Register the output ring
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::RegisterOutputBufferRing (Ring_t          Ring)
{
    //
    // Clear our parameter pointers
    //

    StreamParameters                    = NULL;
    FrameParameters                     = NULL;
    DeferredParsedFrameParameters       = NULL;
    DeferredParsedVideoParameters       = NULL;

    //
    // Pass the call on down (we need the frame parameters count obtained by the lower level function).
    //

    return FrameParser_Video_c::RegisterOutputBufferRing (Ring);
}
//}}}

//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::PrepareReferenceFrameList (void)
{
    unsigned int        i;
    bool                SelfReference;
    unsigned int        ReferenceFramesNeeded;
    unsigned int        PictureCodingType;
    Vc1VideoPicture_t*  PictureHeader;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.
    //

    PictureHeader               = &(((Vc1FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->ptype;
    ReferenceFramesNeeded       = REFERENCE_FRAMES_NEEDED(PictureCodingType);

    //
    // Detect the special case of a second field referencing the first
    // this is when we have the field startup condition
    //          I P P P 
    // where the first P actually references its own decode buffer.
    //

    SelfReference               = (!PictureHeader->first_field) && (ReferenceFramesNeeded == 1) && (ReferenceFrameList.EntryCount == 0);

    //
    // Check for sufficient reference frames.  We cannot decode otherwise
    //

    if ((!SelfReference) && (ReferenceFrameList.EntryCount < ReferenceFramesNeeded))
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    if (SelfReference)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0]   = NextDecodeFrameIndex;
    }
    else
    {
        for(i=0; i<ReferenceFramesNeeded; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
    }

    //report (severity_info, "Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
    //        ReferenceFrameList.EntryIndicies[0], ReferenceFrameList.EntryIndicies[1],
    //        ReferenceFramesNeeded, ReferenceFrameList.EntryCount, ReferenceFrameList.EntryCount - ReferenceFramesNeeded );

    return FrameParserNoError;
}
//}}}

//{{{  ForPlayUpdateReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///             A reference frame is only recorded as such on the last field, in order to
///             ensure the correct management of reference frames in the codec, the
///             codec is informed immediately of a release on the first field of a field picture.
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ForPlayUpdateReferenceFrameList (void)
{
    unsigned int        i;
    bool                LastField;

    if (ParsedFrameParameters->ReferenceFrame)
    {
        LastField       = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                          !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

        if (LastField)
        {
            if (ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE)
            {
                Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);

                ReferenceFrameList.EntryCount--;
                for (i=0; i<ReferenceFrameList.EntryCount; i++)
                    ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i+1];
            }

            ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
        }
        else
        {
            Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);
        }
    }

    return FrameParserNoError;
}
//}}}

//{{{  RevPlayProcessDecodeStacks
// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for processing decode stacks, this 
//      initializes the post decode ring before passing itno the real 
//      implementation of this function.
//

FrameParserStatus_t   FrameParser_VideoVc1_c::RevPlayProcessDecodeStacks (void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();

    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}
//{{{  RevPlayGeneratePostDecodeParameterSettings
// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter 
//      settings for reverse play, these consist of the display frame index,
//      and presentation time, both of which may be deferred if the information
//      is unavailable.
//
//      For reverse play, this function will use a simple numbering system,
//      Imaging a sequence  I B B P B B this should be numbered (in reverse) as
//                          3 5 4 0 2 1
//      These will be presented to this function in reverse order ( B B P B B I)
//      so for non ref frames we ring them, and for ref frames we use the next number
//      and then process what is on the ring.
//

FrameParserStatus_t   FrameParser_VideoVc1_c::RevPlayGeneratePostDecodeParameterSettings (void)
{
    //
    // If this is the first decode of a frame then we need display frame indices and presentation timnes
    //

    if (ParsedFrameParameters->FirstParsedParametersForOutputFrame)
    {
        //
        // If this is not a reference frame then place it on the ring for calculation later
        //

        if (!ParsedFrameParameters->ReferenceFrame)
        {
            ReverseQueuedPostDecodeSettingsRing->Insert ((unsigned int)ParsedFrameParameters);
            ReverseQueuedPostDecodeSettingsRing->Insert ((unsigned int)ParsedVideoParameters);
        }
        else

        //
        // If this is a reference frame then first process it, then process the frames on the ring
        //

        {
            CalculateFrameIndexAndPts (ParsedFrameParameters, ParsedVideoParameters);

            while (ReverseQueuedPostDecodeSettingsRing->NonEmpty())
            {
                ReverseQueuedPostDecodeSettingsRing->Extract ((unsigned int *)&DeferredParsedFrameParameters);
                ReverseQueuedPostDecodeSettingsRing->Extract ((unsigned int *)&DeferredParsedVideoParameters);
                CalculateFrameIndexAndPts (DeferredParsedFrameParameters, DeferredParsedVideoParameters);
            }
        }
    }

//

    return FrameParserNoError;
}
//}}}
//{{{  RevPlayRemoveReferenceFrameFromList
// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to remove a frame from the reference
//      frame list in reverse play.
//
//      Note, we only inserted the reference frame in the list on the last
//      field but we need to inform the codec we are finished with it on both
//      fields (for field pictures).
//

FrameParserStatus_t   FrameParser_VideoVc1_c::RevPlayRemoveReferenceFrameFromList (void)
{
    bool        LastField;

    LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                  !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if ((ReferenceFrameList.EntryCount != 0) && LastField)
    {
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);

        if (LastField)
            ReferenceFrameList.EntryCount--;
    }

    return FrameParserNoError;
}
//}}}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadHeaders (void)
{
    unsigned int                i;
    unsigned int                Code;
    FrameParserStatus_t         Status;
    bool                        FrameReadyForDecode     = false;

#if 0
    report (severity_info, "Start codes (%d):", StartCodeList->NumberOfStartCodes);
    for (i=0; i<StartCodeList->NumberOfStartCodes; i++)
        report (severity_info, " %02x(%d)", ExtractStartCodeCode(StartCodeList->StartCodes[i]), ExtractStartCodeOffset(StartCodeList->StartCodes[i]));
    report (severity_info, "\n");
#endif

    for (i=0; i<StartCodeList->NumberOfStartCodes; i++)
    {
        Code    = StartCodeList->StartCodes[i];

#if defined (REMOVE_ANTI_EMULATION_BYTES)
        LoadAntiEmulationBuffer (BufferData + ExtractStartCodeOffset(Code));
        CheckAntiEmulationBuffer (METADATA_ANTI_EMULATION_REQUEST);
#else
        Bits.SetPointer (BufferData + ExtractStartCodeOffset(Code));
#endif

        Bits.FlushUnseen(32);

        Status  = FrameParserNoError;
        if (FrameReadyForDecode && (ExtractStartCodeCode(Code) != VC1_SLICE_START_CODE))
        {
            Status              = CommitFrameForDecode();
            FrameReadyForDecode = false;
        }

        switch (ExtractStartCodeCode(Code))
        {
            case   VC1_FRAME_START_CODE:
                //report (severity_info, "VC1_FRAME_START_CODE\n");
                // Record the first Picture Header as the start of the data to be decoded.
                // In interlaced field streams, Frame Start Code denotes first field.
                ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
                if (ReadPictureHeaderAdvancedProfile (1) == FrameParserNoError)
                    FrameReadyForDecode = true;
                break;

            case   VC1_FIELD_START_CODE:
                //report (severity_info, "VC1_FIELD_START_CODE\n");
                // Record the first Picture Header as the start of the data to be decoded.
                // In interlaced field streams, Field Start Code denotes second field.
                // Commit the first field for decode?
                // Status       = CommitFrameForDecode();
                ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
                if (ReadPictureHeaderAdvancedProfile (0) == FrameParserNoError)
                    FrameReadyForDecode = true;
                break;

            case   VC1_SEQUENCE_HEADER_CODE:
                //report (severity_info, "VC1_SEQUENCE_HEADER_CODE\n");
                Status  = ReadSequenceHeader();
                break;

            case   VC1_SEQUENCE_LAYER_METADATA_START_CODE:
                //report (severity_info, "VC1_SEQUENCE_LAYER_METADATA_START_CODE\n");
                if (!SequenceLayerMetaDataValid)
                    Status  = ReadSequenceLayerMetadata();
                break;


            case   VC1_SLICE_START_CODE:
                //report (severity_info, "VC1_SLICE_START_CODE\n");
                // Build a list of the slices in this frame, recording an entry for each
                // SLICE_START_CODE as needed by the VC1 decoder.
                Status  = ReadSliceHeader(ExtractStartCodeOffset(Code));
                break;

            case   VC1_ENTRY_POINT_HEADER_CODE:
                //report (severity_info, "VC1_ENTRY_POINT_HEADER_CODE\n");
                Status  = ReadEntryPointHeader();
                break;

            case   VC1_END_OF_SEQUENCE:
                //report (severity_info, "VC1_END_OF_SEQUENCE\n");
                break;

	    case 8:
	        FrameRateDefaultDen = Bits.Get(32);
		FrameRateDefaultNum = Bits.Get(32);
		report (severity_error, "Received framerate %d / %d\n", FrameRateDefaultDen, FrameRateDefaultNum);
		break;

            default:
                #if 0
                {
                    unsigned char* Buff = &BufferData[ExtractStartCodeOffset(Code)-12];
                    report (severity_info, "::::%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                            Buff[0], Buff[1], Buff[2], Buff[3], Buff[4], Buff[5], Buff[6], Buff[7],
                            Buff[8], Buff[9], Buff[10], Buff[11], Buff[12], Buff[13], Buff[14], Buff[15],
                            Buff[16], Buff[17], Buff[18], Buff[19], Buff[20], Buff[21], Buff[22], Buff[23],
                            Buff[24], Buff[25], Buff[26], Buff[27], Buff[28], Buff[29], Buff[30], Buff[31]);
                }
                report (severity_error, "Unknown start code %x\n", ExtractStartCodeCode(Code));
                #endif
                Status  = FrameParserUnhandledHeader;
                break;
        }

        if ((Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader))
            return Status;

#if defined (REMOVE_ANTI_EMULATION_BYTES)
        // Check that we didn't lose track and overun the anti-emulation buffer
        AssertAntiEmulationOk (__FUNCTION__);
#endif
    }

    // Finished processing all the start codes, send the frame to be decoded.
    if (FrameReadyForDecode)
    {
        Status              = CommitFrameForDecode();
        FrameReadyForDecode = false;
    }

    return FrameParserNoError;
}
//}}}
//{{{  ReadSequenceHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a sequence header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadSequenceHeader (void)
{
    FrameParserStatus_t             Status;
    Vc1VideoSequence_t              *Header;

    //

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent) &&
        (StreamParameters->SequenceHeader.profile != VC1_ADVANCED_PROFILE))
    {
        report (severity_error, "%s: Sequence header only valid for advanced profile\n", __FUNCTION__);
        //return FrameParserError;
    }

    Status      = GetNewStreamParameters ((void **)&StreamParameters);
    if (Status != FrameParserNoError)
        return Status;

    StreamParameters->UpdatedSinceLastFrame = true;
    Header = &StreamParameters->SequenceHeader;
    memset (Header, 0x00, sizeof(Vc1VideoSequence_t));

    // clear out entrypoint header - it can only occur after a sequence header
    memset (&StreamParameters->EntryPointHeader, 0x00, sizeof(Vc1VideoEntryPoint_t));

    //

#if 0
    {
        unsigned char* Buff;
        unsigned int   Bib;

        Bits.GetPosition(&Buff, &Bib);
        Buff -= 8;
        report (severity_info, "::::%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                Buff[0], Buff[1], Buff[2], Buff[3], Buff[4], Buff[5], Buff[6], Buff[7],
                Buff[8], Buff[9], Buff[10], Buff[11], Buff[12], Buff[13], Buff[14], Buff[15],
                Buff[16], Buff[17], Buff[18], Buff[19], Buff[20], Buff[21], Buff[22], Buff[23],
                Buff[24], Buff[25], Buff[26], Buff[27], Buff[28], Buff[29], Buff[30], Buff[31]);
    }
#endif
    if (Bits.Show(2) != VC1_ADVANCED_PROFILE)
    {
        report (severity_error, "Invalid sequence header\n");
        return FrameParserError;
    }

    Header->profile                                     = Bits.Get(2);
    Header->level                                       = Bits.Get(3);
    Header->colordiff_format                            = Bits.Get(2);
    Header->frmrtq_postproc                             = Bits.Get(3);
    Header->bitrtq_postproc                             = Bits.Get(5);
    Header->postprocflag                                = Bits.Get(1);
    Header->max_coded_width                             = (Bits.Get(12)) * 2 + 2;
    Header->max_coded_height                            = (Bits.Get(12)) * 2 + 2;
    Header->pulldown                                    = Bits.Get(1);
    Header->interlace                                   = Bits.Get(1);
    Header->tfcntrflag                                  = Bits.Get(1);
    Header->finterpflag                                 = Bits.Get(1);
    Header->reserved                                    = Bits.Get(1);
    Header->psf                                         = Bits.Get(1);

    Header->display_ext                                 = Bits.Get(1);
    if (Header->display_ext == 1)
    {
        Header->disp_horiz_size                         = Bits.Get(14) + 1;
        Header->disp_vert_size                          = Bits.Get(14) + 1;

        Header->aspect_ratio_flag                       = Bits.Get(1);
        if (Header->aspect_ratio_flag == 1)
        {
            Header->aspect_ratio                        = Bits.Get(4);

            if (Header->aspect_ratio == 15)
            {
                Header->aspect_horiz_size               = Bits.Get(8);
                Header->aspect_vert_size                = Bits.Get(8);
            }
        }

        Header->frame_rate_flag                         = Bits.Get(1);
        if (Header->frame_rate_flag == 1)
        {
            Header->frame_rate_ind                      = Bits.Get(1);

            if (Header->frame_rate_ind == 0)
            {
                Header->frameratenr                     = Bits.Get(8);
                Header->frameratedr                     = Bits.Get(4);
                if (Header->frameratenr > 7)
                    Header->frameratenr = 0;
                if (Header->frameratedr > 2)
                    Header->frameratedr = 0;
            }
            else
            {
                Header->framerateexp                    = Bits.Get(16);
            }
        }

        Header->color_format_flag                       = Bits.Get(1);
        if (Header->color_format_flag == 1)
        {
            Header->color_prim                          = Bits.Get(8);
            Header->transfer_char                       = Bits.Get(8);
            Header->matrix_coef                         = Bits.Get(8);
        }

        Header->hrd_param_flag                          = Bits.Get(1);
        if (Header->hrd_param_flag == 1)
        {
            // Hypothetical Reference Decoder Indicator Flag is set, so read the HRD parameters
            // from the stream. We are only interested in the number of parameter sets as it affects
            // the interpretation of the entry point header.
            Header->hrd_num_leaky_buckets               = Bits.Get(5);
        }
    }

    StreamParameters->SequenceHeaderPresent     = true;

#ifdef DUMP_HEADERS
    report (severity_info, "Sequence header :- \n");
    report (severity_info, "    profile           : %6d\n", Header->profile);
    report (severity_info, "    level             : %6d\n", Header->level);
    report (severity_info, "    colordiff_format  : %6d\n", Header->colordiff_format);
    report (severity_info, "    max_coded_width   : %6d\n", Header->max_coded_width);
    report (severity_info, "    max_coded_height  : %6d\n", Header->max_coded_height);
    report (severity_info, "    pulldown          : %6d\n", Header->pulldown);
    report (severity_info, "    interlace         : %6d\n", Header->interlace);
    report (severity_info, "    tfcntrflag        : %6d\n", Header->tfcntrflag);
    report (severity_info, "    finterpflag       : %6d\n", Header->finterpflag);
    report (severity_info, "    psf               : %6d\n", Header->psf);
    report (severity_info, "    display_ext       : %6d\n", Header->display_ext);
    report (severity_info, "    disp_horiz_size   : %6d\n", Header->disp_horiz_size);
    report (severity_info, "    disp_vert_size    : %6d\n", Header->disp_vert_size);
    report (severity_info, "    aspect_ratio_flag : %6d\n", Header->aspect_ratio_flag);
    report (severity_info, "    frame_rate_flag   : %6d\n", Header->frame_rate_flag);
    report (severity_info, "    frameratenr       : %6d\n", Header->frameratenr);
    report (severity_info, "    frameratedr       : %6d\n", Header->frameratedr);
    report (severity_info, "    frmrtq_postproc   : %6d\n", Header->frmrtq_postproc);
    report (severity_info, "    bitrtq_postproc   : %6d\n", Header->bitrtq_postproc);
    report (severity_info, "    postprocflag      : %6d\n", Header->postprocflag);
    report (severity_info, "    color_format_flag : %6d\n", Header->color_format_flag);
    report (severity_info, "    hrd_param_flag    : %6d\n", Header->hrd_param_flag);
    report (severity_info, "    hrd_num_leaky_buckets : %6d\n", Header->hrd_num_leaky_buckets);
#endif

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    AssertAntiEmulationOk (__FUNCTION__);
#endif

    Assert (Header->level <= VC1_HIGHEST_LEVEL_SUPPORTED);
    Assert (Header->max_coded_width  <= VC1_MAX_CODED_WIDTH);
    Assert (Header->max_coded_height <= VC1_MAX_CODED_HEIGHT);

    return FrameParserNoError;
}
//}}}
//{{{  ReadEntryPointHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in the Entry Point Layer
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadEntryPointHeader (void)
{
    FrameParserStatus_t     Status;
    Vc1VideoEntryPoint_t    *Header;
    Vc1VideoSequence_t      *SequenceHeader;

    //

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent) &&
        (StreamParameters->SequenceHeader.profile != VC1_ADVANCED_PROFILE))
    {
        report (severity_error, "%s: Entrypoint header only valid for advanced profile\n", __FUNCTION__);
        return FrameParserError;
    }

    if (StreamParameters == NULL)
    {
        Status  = GetNewStreamParameters ((void **)&StreamParameters);
        if (Status != FrameParserNoError)
            return Status;
    }
    SequenceHeader = &StreamParameters->SequenceHeader;

#if 0
    {
        unsigned char* Buff;
        unsigned int   Bib;

        Bits.GetPosition(&Buff, &Bib);
        Buff -= 8;
        report (severity_info, "EPH::::%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                Buff[0], Buff[1], Buff[2], Buff[3], Buff[4], Buff[5], Buff[6], Buff[7],
                Buff[8], Buff[9], Buff[10], Buff[11], Buff[12], Buff[13], Buff[14], Buff[15],
                Buff[16], Buff[17], Buff[18], Buff[19], Buff[20], Buff[21], Buff[22], Buff[23],
                Buff[24], Buff[25], Buff[26], Buff[27], Buff[28], Buff[29], Buff[30], Buff[31]);
    }
#endif

    // StreamParameters->UpdatedSinceLastFrame      = true; // Dont set this as it'll invoke MME global params
    Header = &StreamParameters->EntryPointHeader;
    memset (Header, 0x00, sizeof(Vc1VideoEntryPoint_t));


    Header->broken_link                 = Bits.Get(1);
    Header->closed_entry                = Bits.Get(1);
    Header->panscan_flag                = Bits.Get(1);
    Header->refdist_flag                = Bits.Get(1);
    Header->loopfilter                  = Bits.Get(1);
    Header->fastuvmc                    = Bits.Get(1);
    Header->extended_mv                 = Bits.Get(1);
    Header->dquant                      = Bits.Get(2);
    Header->vstransform                 = Bits.Get(1);
    Header->overlap                     = Bits.Get(1);
    Header->quantizer                   = Bits.Get(2);

    if (SequenceHeader->hrd_param_flag == 1)
    {
        // Skip HRD_FULL[n] for each leaky bucket
        unsigned int    n;
        for (n = 0; n < SequenceHeader->hrd_num_leaky_buckets; n++)
            Bits.Get(8);
    }

    Header->coded_size_flag         = Bits.Get(1);
    if (Header->coded_size_flag == 1)
    {
        unsigned int        Width    = Bits.Get(12);
        unsigned int        Height   = Bits.Get(12);

        Header->coded_width         = (Width * 2) + 2;
        if (Header->coded_width > SequenceHeader->max_coded_width)
        {
            report (severity_info, "FrameParser_VideoVc1_c::ReadEntryPointHeader: Warning Coded Width %d greater than MaxCodedWidth %d - using %d\n",
                                    Header->coded_width, SequenceHeader->max_coded_width,  SequenceHeader->max_coded_width);
            Header->coded_width     = SequenceHeader->max_coded_width;

        }
        Header->coded_height        = (Height * 2) + 2;
        if (Header->coded_height > SequenceHeader->max_coded_height)
        {
            report (severity_info, "FrameParser_VideoVc1_c::ReadEntryPointHeader: Warning Coded Height %d greater than MaxCodedHeight %d - using %d\n",
                                    Header->coded_height, SequenceHeader->max_coded_height,  SequenceHeader->max_coded_height);
            Header->coded_height    = SequenceHeader->max_coded_height;

        }
    }

    if (Header->extended_mv == 1)
    {
        Header->extended_dmv        = Bits.Get(1);
    }

    Header->range_mapy_flag         = Bits.Get(1);
    if (Header->range_mapy_flag == 1)
    {
        Header->range_mapy          = Bits.Get(3);
    report (severity_info, "%s: range_mapy : %6d\n", __FUNCTION__, Header->range_mapy);
    }

    Header->range_mapuv_flag        = Bits.Get(1);
    if (Header->range_mapuv_flag == 1)
    {
        Header->range_mapuv         = Bits.Get(3);
    report (severity_info, "%s: range_mapuv : %6d\n", __FUNCTION__, Header->range_mapuv);
    }

    if ((Header->closed_entry == 0) && RangeMapValid)
    {
        Header->range_mapy_flag     = RangeMapYFlag;
        Header->range_mapy          = RangeMapY;
        Header->range_mapuv_flag    = RangeMapUVFlag;
        Header->range_mapuv         = RangeMapUV;
    }
    else
    {
        RangeMapYFlag               =  Header->range_mapy_flag;
        RangeMapY                   =  Header->range_mapy;
        RangeMapUVFlag              =  Header->range_mapuv_flag;
        RangeMapUV                  =  Header->range_mapuv;
        RangeMapValid               =  true;
    }

    StreamParameters->EntryPointHeaderPresent   = true;


#ifdef DUMP_HEADERS
    report (severity_note, "EntryPoint header :- \n");
    report (severity_info, "    broken_link       : %6d\n", Header->broken_link);
    report (severity_info, "    closed_entry      : %6d\n", Header->closed_entry);
    report (severity_info, "    panscan_flag      : %6d\n", Header->panscan_flag);
    report (severity_info, "    refdist_flag      : %6d\n", Header->refdist_flag);
    report (severity_info, "    loopfilter        : %6d\n", Header->loopfilter);
    report (severity_info, "    coded_size_flag   : %6d\n", Header->coded_size_flag);
    report (severity_info, "    coded_width       : %6d\n", Header->coded_width);
    report (severity_info, "    coded_height      : %6d\n", Header->coded_height);
    report (severity_info, "    extended_mv       : %6d\n", Header->extended_mv);
    report (severity_info, "    dquant            : %6d\n", Header->dquant);
    report (severity_info, "    vstransform       : %6d\n", Header->vstransform);
    report (severity_info, "    overlap           : %6d\n", Header->overlap);
    report (severity_info, "    quantizer         : %6d\n", Header->quantizer);
    report (severity_info, "    extended_dmv      : %6d\n", Header->extended_dmv);
    report (severity_info, "    range_mapy_flag   : %6d\n", Header->range_mapy_flag);
    report (severity_info, "    range_mapy        : %6d\n", Header->range_mapy);
    report (severity_info, "    range_mapuv_flag  : %6d\n", Header->range_mapuv_flag);
    report (severity_info, "    range_mapuv       : %6d\n", Header->range_mapuv);
#endif

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    AssertAntiEmulationOk (__FUNCTION__);
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureHeaderAdvancedProfile
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadPictureHeaderAdvancedProfile (unsigned int first_field)
{
    int                         Val;
    FrameParserStatus_t         Status;
    Vc1VideoPicture_t*          Header;
    Vc1VideoSequence_t*         SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;
    unsigned char*              StartPointer;
    unsigned int                StartBitsInByte;

    //

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoVc1_c::ReadPictureHeader - Sequence header not found.\n");
        return FrameParserNoStreamParameters;
    }

    //

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        {
            report (severity_error, "FrameParser_VideoVc1_c::ReadPictureHeader - Failed to get new frame parameters.\n");
            return Status;
        }
    }

    Header                              = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(Vc1VideoPicture_t));

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    EntryPointHeader                    = &StreamParameters->EntryPointHeader;

    //
#if 0
    {
        unsigned char* Buff;
        unsigned int   Bib;
        static unsigned int PictureNo;

        Bits.GetPosition(&Buff, &Bib);
        Buff -= 8;
        report (severity_info, "Picture %d::::%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                PictureNo++, Buff[0], Buff[1], Buff[2], Buff[3], Buff[4], Buff[5], Buff[6], Buff[7],
                Buff[8], Buff[9], Buff[10], Buff[11], Buff[12], Buff[13], Buff[14], Buff[15],
                Buff[16], Buff[17], Buff[18], Buff[19], Buff[20], Buff[21], Buff[22], Buff[23],
                Buff[24], Buff[25], Buff[26], Buff[27], Buff[28], Buff[29], Buff[30], Buff[31]);
    }
#endif

    Bits.GetPosition (&StartPointer, &StartBitsInByte);
    Header->first_field = first_field;                                  // Determined from FIELD or FRAME start code

    if (Header->first_field != 0)
    {
        if (SequenceHeader->interlace == 1)                             // 7.1.1.15
        {
            Val = BitsDotGetVc1VLC(2, VC1_VLC_LEAF_ZERO);
            switch (Val)
            {
                case VC1_VLC_CODE(2, 1, 0x00):  // 0b
                    Header->fcm             = VC1_FCM_PROGRESSIVE;
                    break;
                case VC1_VLC_CODE(2, 2, 0x02):  // 10b
                    Header->fcm             = VC1_FCM_FRAME_INTERLACED;
                    break;
                case VC1_VLC_CODE(2, 2, 0x03):  // 11b
                    // Use this with either FRAME_SC or FIELD_SC to determine if first
                    // or second field in a field interlaced picture. Use TFF to then
                    // identify if it is the top or bottom field.
                    Header->fcm             = VC1_FCM_FIELD_INTERLACED;
                    break;
                default:
                    report (severity_error, "FrameParser_VideoVc1_c::ReadPictureHeader - Unknown field information.\n");
                    return FrameParserHeaderSyntaxError;
            }
        }

        if ((SequenceHeader->interlace == 1) && (Header->fcm == VC1_FCM_FIELD_INTERLACED))      // 7.1.1.4
        {
            Header->fptype      = Bits.Get(3);
            Header->ptype       = FieldPicture1[Header->fptype];
        }
        else
        {
            Val = BitsDotGetVc1VLC(4, VC1_VLC_LEAF_ZERO);               // Picture Type
            switch (Val)
            {
                case VC1_VLC_CODE(4, 1, 0x00):  // 0b
                    Header->ptype                   = VC1_PICTURE_CODING_TYPE_P;
                    break;
                case VC1_VLC_CODE(4, 2, 0x02):  // 10b
                    Header->ptype                   = VC1_PICTURE_CODING_TYPE_B;
                    break;
                case VC1_VLC_CODE(4, 3, 0x06):  // 110b
                    Header->ptype                   = VC1_PICTURE_CODING_TYPE_I;
                    break;
                case VC1_VLC_CODE(4, 4, 0x0E):  // 1110b
                    Header->ptype                   = VC1_PICTURE_CODING_TYPE_BI;
                    break;
                case VC1_VLC_CODE(4, 4, 0x0F):  // 1111b
                    Header->ptype                   = VC1_PICTURE_CODING_TYPE_SKIPPED;
                    break;
                default:
                    report (severity_error, "FrameParser_VideoVc1_c::ReadPictureHeader - Unknown picture type.\n");
                    return FrameParserHeaderSyntaxError;
            }
        }

        if ((SequenceHeader->tfcntrflag == 1) && (Header->ptype != VC1_PICTURE_CODING_TYPE_SKIPPED))    // 7.1.1.16
            Header->tfcntr                      = Bits.Get(8);          // Temporal Ref Frame Counter

        if (SequenceHeader->pulldown == 1)                              // 7.1.1.17 etc
        {
            if ((SequenceHeader->interlace == 0) || (SequenceHeader->psf == 1))
            {
                Header->rptfrm                  = Bits.Get(2);          // Repeat Frame Count
            }
            else
            {
                Header->tff                     = Bits.Get(1);          // Top Field First
                Header->rff                     = Bits.Get(1);          // Repeat First Field
            }
        }
        else
        {
            Header->tff                         = 1;                    // Defaults to true
        }

        if (EntryPointHeader->panscan_flag == 1)                        // 7.1.1.20
        {
            unsigned int i;

            Header->ps_present                  = Bits.Get(1);          // Pan & Scan Flags Present

            if (Header->ps_present == 1)
            {

                // Calculate the number of Pan & Scan Windows - 8.9.1
                if ((SequenceHeader->interlace == 1) && (SequenceHeader->psf == 0))
                {
                    if (SequenceHeader->pulldown == 1)
                        Header->ps_window_count     = 2 + Header->rff;
                    else
                        Header->ps_window_count     = 2;
                }
                else
                {
                    if (SequenceHeader->pulldown == 1)
                        Header->ps_window_count     = 1 + Header->rptfrm;
                    else
                        Header->ps_window_count     = 1;
                }

                for (i=0; i<Header->ps_window_count; i++)
                {
                    Header->ps_hoffset[i]       = Bits.Get(18);
                    Header->ps_voffset[i]       = Bits.Get(18);
                    Header->ps_width[i]         = Bits.Get(14);
                    Header->ps_height[i]        = Bits.Get(14);
                }
            }
        }

        if (Header->ptype != VC1_PICTURE_CODING_TYPE_SKIPPED)
        {
            Header->rndctrl                     = Bits.Get(1);  // Rounding Control Bit

            if (SequenceHeader->interlace == 1)
                Header->uvsamp                                  = Bits.Get(1);                  // 7.1.1.26

            if ((SequenceHeader->interlace == 0) || (Header->fcm == VC1_FCM_PROGRESSIVE))
                ReadPictureHeaderProgressive ();
            else
            {
                if (Header->fcm == VC1_FCM_FRAME_INTERLACED)
                    ReadPictureHeaderInterlacedFrame ();
                else
                {
                    unsigned char*  EndPointer;
                    unsigned int    EndBitsInByte;

                    // read remaining common fields for both first and second fields
                    if ((EntryPointHeader->refdist_flag == 1) && (Header->fptype <= 0x3))       // see 9.1.1.43
                    {
                        Header->refdist                         = Bits.Get(2);
                        if (Header->refdist == 0x3)
                        {
                            unsigned int N;

                            Val                                 = BitsDotGetVc1VLC(13, VC1_VLC_LEAF_ZERO);
                            N                                   = VC1_VLC_BITSREAD(13, Val);
                            Header->refdist                     = N+2;
                        }
                        BackwardRefDist                         = Header->refdist;              // remember ref dist for future b frames
                    }
                    if (Header->fptype > 0x3)                                                   // B/B, B/BI, BI/B field pairs
                    {
                        unsigned int    BFraction               = Bits.Get(3);                  // see 7.1.1.14
                        if (BFraction == 0x07)                                                  // 111b
                            BFraction                          += Bits.Get(4);
                        Header->bfraction_numerator             = BFractionNumerator[BFraction];
                        Header->bfraction_denominator           = BFractionDenominator[BFraction];

                        Header->backward_refdist                = BackwardRefDist;
                    }

                    Bits.GetPosition (&EndPointer, &EndBitsInByte);
                    Header->picture_layer_size                  = (((unsigned int)EndPointer - (unsigned int)StartPointer) * 8) + (StartBitsInByte - EndBitsInByte);

                    // Remember first field picture header for second field then read field specific data
                    // for first field.
                    memcpy (&FirstFieldPictureHeader, Header, sizeof (Vc1VideoPicture_t));
                    ReadPictureHeaderInterlacedField ();
                }
            }
        }
    }
    else
    {
        // second field
        // Set the picture type of the second field using the fptype we read in the first field
        // Copy first field data from saved structure then read field specific data for second field
        memcpy (Header, &FirstFieldPictureHeader, sizeof (Vc1VideoPicture_t));
        memset (&FirstFieldPictureHeader, 0x00, sizeof (Vc1VideoPicture_t));
        Header->ptype                   = FieldPicture2[Header->fptype];
        Header->first_field             = 0;    // restore after being overwritten by memcpy
        Header->ps_window_count         = 0;    // Pan scan info only with first field

        ReadPictureHeaderInterlacedField ();
    }

#if 0
    switch (Header->ptype)
    {
        case VC1_PICTURE_CODING_TYPE_P:             report (severity_info, "P frame\n");            break;
        case VC1_PICTURE_CODING_TYPE_B:             report (severity_info, "B frame\n");            break;
        case VC1_PICTURE_CODING_TYPE_I:             report (severity_info, "I frame\n");            break;
        case VC1_PICTURE_CODING_TYPE_SKIPPED:       report (severity_info, "Skipped frame\n");      break;
        case VC1_PICTURE_CODING_TYPE_BI:            report (severity_info, "BI frame\n");           break;
    }
#endif

    // Field_Picture_Layer starts here OR Frame_Picture continues here.

    FrameParameters->PictureHeaderPresent   = true;

#ifdef DUMP_HEADERS
        report (severity_note, "Picture header :- \n");
        report (severity_note, "    fcm                   : %6d\n", Header->fcm);
        report (severity_info, "    fptype                : %6d\n", Header->fptype);
        report (severity_note, "    ptype                 : %6d\n", Header->ptype);
        report (severity_note, "    rptfrm                : %6d\n", Header->rptfrm);
        report (severity_note, "    tfcntr                : %6d\n", Header->tfcntr);
        report (severity_note, "    tff                   : %6d\n", Header->tff);
        report (severity_note, "    rff                   : %6d\n", Header->rff);
        report (severity_note, "    ps_present            : %6d\n", Header->ps_present);
        report (severity_info, "    bfraction_num         : %6d\n", Header->bfraction_numerator);
        report (severity_info, "    bfraction_den         : %6d\n", Header->bfraction_denominator);
        report (severity_note, "    rndctrl               : %6d\n", Header->rndctrl);
        report (severity_info, "    pqindex               : %6d\n", Header->pqindex);
        report (severity_info, "    halfqp                : %6d\n", Header->halfqp);
        report (severity_info, "    pquant                : %6d\n", Header->pquant);
        report (severity_info, "    pquantizer            : %6d\n", Header->pquantizer);
        report (severity_info, "    refdist               : %6d\n", Header->refdist);
        report (severity_info, "    backward_refdist      : %6d\n", Header->backward_refdist);
        report (severity_info, "    numref                : %6d\n", Header->numref);
        report (severity_info, "    reffield              : %6d\n", Header->reffield);
        report (severity_info, "    postproc              : %6d\n", Header->postproc);
        report (severity_info, "    mvrange               : %6d\n", Header->mvrange);
        report (severity_info, "    respic                : %6d\n", Header->respic);
        report (severity_info, "    mvmode                : %6d\n", Header->mvmode);
        report (severity_info, "    mvmode2               : %6d\n", Header->mvmode2);
        report (severity_info, "    intensity_comp_field  : %6d\n", Header->intensity_comp_field);
        report (severity_info, "    intensity_comp_top    : %6d\n", Header->intensity_comp_top);
        report (severity_info, "    intensity_comp_bottom : %6d\n", Header->intensity_comp_bottom);
        report (severity_info, "    first_field           : %6d\n", Header->first_field);
#endif

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    AssertAntiEmulationOk (__FUNCTION__);
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadSliceHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a slice header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadSliceHeader (unsigned int pSlice)
{
    FrameParserStatus_t         Status;
    Vc1VideoSlice_t            *Header;
    int                         i;

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    CheckAntiEmulationBuffer (SLICE_ANTI_EMULATION_REQUEST);
#endif

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoVc1_c::ReadSliceHeader - Appropriate sequence header not found.\n");
        return FrameParserNoStreamParameters;
    }

//

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
            return Status;
    }

    Header      = &FrameParameters->SliceHeaderList;
    //memset (Header, 0x00, sizeof(Vc1VideoSlice_t));

    i = Header->no_slice_headers;

//

    Header->slice_start_code[i]         = pSlice;
    Header->slice_addr[i]               = Bits.Get(9);

//

    Header->no_slice_headers++;
    FrameParameters->SliceHeaderPresent = true;

//

#ifdef DUMP_HEADERS
    report (severity_note, "Slice header[%d] :- \n", i);
    report (severity_note, "    pSlice               : %08x\n", pSlice);
    report (severity_note, "    slice_addr           : %08x\n", Header->slice_addr[i]);
#endif

#if defined (REMOVE_ANTI_EMULATION_BYTES)
    AssertAntiEmulationOk (__FUNCTION__);
#endif

    return FrameParserNoError;
}
//}}}

//{{{  ReadSequenceLayerMetadata
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in an abc metadata structure
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::ReadSequenceLayerMetadata (void)
{
    FrameParserStatus_t         Status;
    Vc1VideoSequence_t*         SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;
    unsigned int                FlagByte        = 0;
    unsigned int                FlagWord        = 0;
    unsigned int                NumFrames       = 0;
    unsigned int                HrdBuffer;
    unsigned int                HrdRate;
    unsigned int                FrameRate;
    unsigned int                Temp;


    //
    // Conforms to Sequence Layer Data Structure - see table 265 in Annex L
    // Must be only one in a stream
    //

#if 0
    int i;
    report (severity_info, "data %d :\n", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        report (severity_error, "%s: Received Sequence Layer MetaData after previous sequence data\n", __FUNCTION__);
        return FrameParserNoError;
    }

    Status      = GetNewStreamParameters ((void **)&StreamParameters);
    if (Status != FrameParserNoError)
        return Status;

    StreamParameters->UpdatedSinceLastFrame = true;
    SequenceHeader      = &StreamParameters->SequenceHeader;
    memset (SequenceHeader, 0x00, sizeof(Vc1VideoSequence_t));

    // some values go into the entry point header
    EntryPointHeader    = &StreamParameters->EntryPointHeader;
    memset (EntryPointHeader, 0x00, sizeof(Vc1VideoEntryPoint_t));

    NumFrames                                           = Bits.Get (8);        // Num frames
    NumFrames                                          |= (Bits.Get (8) << 8);
    NumFrames                                          |= (Bits.Get (8) << 16);
    FlagByte                                            = Bits.Get (8);         // 8 bit flag must be 0xc5
    if (FlagByte != 0xc5)
    {
        report (severity_error, "%s: Invalid flag word expected 0xc5 received 0x%02x\n", __FUNCTION__, FlagByte);
        return FrameParserError;
    }

    FlagWord                                            = Bits.Get (8);         // must be 0x04
    FlagWord                                           |= (Bits.Get (8) << 8);
    FlagWord                                           |= (Bits.Get (8) << 16);
    FlagWord                                           |= (Bits.Get (8) << 24);
    if (FlagWord != 0x04)
    {
        report (severity_error, "%s: Invalid flag word expected 0x00000004 received 0x%08x\n", __FUNCTION__, FlagWord);
        return FrameParserError;
    }

    // Sequence Header Struct C - see table 263, 264 in Annex J
    SequenceHeader->profile                             = Bits.Get (2);         // PROFILE bits 0,1
    Temp                                                = Bits.Get(2);          // PROFILE bits 2,3
    if (SequenceHeader->profile == VC1_ADVANCED_PROFILE)
        Temp                                            = Bits.Get(28);         // skip the rest
    else
    {
        unsigned int    Reserved3;
        unsigned int    Reserved4;
        unsigned int    Reserved5;
        unsigned int    Reserved6;
        SequenceHeader->frmrtq_postproc                 = Bits.Get(3);          // FRMRTQ_POSTPROC
        SequenceHeader->bitrtq_postproc                 = Bits.Get(5);          // BITRTQ_POSTPROC
        EntryPointHeader->loopfilter                    = Bits.Get(1);          // LOOPFILTER
        Reserved3                                       = Bits.Get(1);          // Reserved3
        SequenceHeader->multires                        = Bits.Get(1);          // MULTIRES
        Reserved4                                       = Bits.Get(1);          // Reserved4
        EntryPointHeader->fastuvmc                      = Bits.Get(1);          // FASTUVMC
        EntryPointHeader->extended_mv                   = Bits.Get(1);          // EXTENDED_MV
        EntryPointHeader->dquant                        = Bits.Get(2);          // DQUANT
        EntryPointHeader->vstransform                   = Bits.Get(1);          // VSTRANSFORM
        Reserved5                                       = Bits.Get(1);          // Reserved5
        EntryPointHeader->overlap                       = Bits.Get(1);          // OVERLAP
        SequenceHeader->syncmarker                      = Bits.Get(1);          // SYNCMARKER
        SequenceHeader->rangered                        = Bits.Get(1);          // RANGERED
        SequenceHeader->maxbframes                      = Bits.Get(3);          // MAXBFRAMES
        EntryPointHeader->quantizer                     = Bits.Get(2);          // QUANTIZER
        SequenceHeader->finterpflag                     = Bits.Get(1);          // FINTERPFLAG
        Reserved6                                       = Bits.Get(1);          // Reserved6
        if ((Reserved3 != 0) | (Reserved4 != 1) || (Reserved5 != 0) || (Reserved6 != 1))
        {
            report (severity_error, "%s: Reserved values incorrect\n", __FUNCTION__);
            return FrameParserError;
        }
    }

    // Sequence Header Struct A - see table 260 in Annex J
    SequenceHeader->max_coded_height                    = Bits.Get(8);          // VERT_SIZE
    SequenceHeader->max_coded_height                   |= (Bits.Get(8) << 8);
    SequenceHeader->max_coded_height                   |= (Bits.Get(8) << 16);
    SequenceHeader->max_coded_height                   |= (Bits.Get(8) << 24);
    SequenceHeader->max_coded_width                     = Bits.Get(8);          // VERT_SIZE
    SequenceHeader->max_coded_width                    |= (Bits.Get(8) << 8);
    SequenceHeader->max_coded_width                    |= (Bits.Get(8) << 16);
    SequenceHeader->max_coded_width                    |= (Bits.Get(8) << 24);

    FlagWord                                            = Bits.Get(8);         // must be 0x0c
    FlagWord                                           |= (Bits.Get (8) << 8);
    FlagWord                                           |= (Bits.Get (8) << 16);
    FlagWord                                           |= (Bits.Get (8) << 24);
    if (FlagWord != 0x0c)
    {
        report (severity_error, "%s: Invalid flag word expected 0x0000000c received 0x%08x\n", __FUNCTION__, FlagWord);
        return FrameParserError;
    }

    // Sequence Header Struct B
    SequenceHeader->level                               = Bits.Get(3);          // LEVEL
    SequenceHeader->cbr                                 = Bits.Get(1);          // CBR
    Temp                                                = Bits.Get(4);          // RES1
    HrdBuffer                                           = Bits.Get(8);          // HRD_BUFFER
    HrdBuffer                                          |= (Bits.Get(8) << 8);
    HrdBuffer                                          |= (Bits.Get(8) << 16);
    HrdRate                                             = Bits.Get(8);         // HRD_RATE
    HrdRate                                            |= (Bits.Get(8) << 8);
    HrdRate                                            |= (Bits.Get(8) << 16);
    HrdRate                                            |= (Bits.Get(8) << 24);
    FrameRate                                           = Bits.Get(8);         // FRAMERATE
    FrameRate                                          |= (Bits.Get(8) << 8);
    FrameRate                                          |= (Bits.Get(8) << 16);
    FrameRate                                          |= (Bits.Get(8) << 24);

    if ((FrameRate != 0) && (FrameRate != 0xffffffff))
    {
        int     i;
        // Value arrives as frame duration in units of 100 nanoseconds (from wmv header)
        //SequenceHeader->frame_rate_ind                  = 1;                    // Special flag for wmv to indicate
        //SequenceHeader->frameratenr                     = 10000000;             // that we should use the frame rate
        //SequenceHeader->frameratedr                     = FrameRate;            // values directly
        SequenceHeader->frame_rate_flag                 = 1;                    // Frame rate present

        for (i=0; i < RECOGNISED_FRAME_RATES; i++)
        {
            int Diff    = FrameRate - FrameRateList[i].AverageTimePerFrame;
            if ((Diff >= -10) && (Diff <= 10))  // Arbitrarily select range of 1 microsecond
                break;
        }
        if (i < RECOGNISED_FRAME_RATES)
        {
            SequenceHeader->frameratenr         = FrameRateList[i].FrameRateNumerator;
            SequenceHeader->frameratedr         = FrameRateList[i].FrameRateDenominator;
            this->FrameRate                     = FrameRates(SequenceHeader->frameratenr + ((SequenceHeader->frameratedr-1) << 3));
        }
        else if (FrameRate < 1000)
        {
            SequenceHeader->frame_rate_ind      = 1;
            SequenceHeader->framerateexp        = (FrameRate*32)-1;             // 6.1.14.4.4
            this->FrameRate                     = Rational_t(SequenceHeader->framerateexp+1, 32);
        }
        else
        {
            SequenceHeader->frame_rate_ind      = 1;
            SequenceHeader->framerateexp        = (320000000/FrameRate)-1;      // 6.1.14.4.4
            this->FrameRate                     = Rational_t(SequenceHeader->framerateexp+1, 32);
        }
        FrameRateValid                          = true;

    }

    StreamParameters->SequenceHeaderPresent             = true;
    StreamParameters->EntryPointHeaderPresent           = true;

#ifdef DUMP_HEADERS
    report (severity_info, "SequenceLayerMetadata :- \n");
    report (severity_info, "    profile           : %6d\n", SequenceHeader->profile);
    report (severity_info, "    level             : %6d\n", SequenceHeader->level);
    report (severity_info, "    max_coded_width   : %6d\n", SequenceHeader->max_coded_width);
    report (severity_info, "    max_coded_height  : %6d\n", SequenceHeader->max_coded_height);
    report (severity_info, "    interlace         : %6d\n", SequenceHeader->interlace);
    report (severity_info, "    framerate         : %6d\n", FrameRate);
    report (severity_info, "    frameratenr       : %6d\n", SequenceHeader->frameratenr);
    report (severity_info, "    frameratedr       : %6d\n", SequenceHeader->frameratedr);
    report (severity_info, "    frame_rate_flag   : %6d\n", SequenceHeader->frame_rate_flag);
    report (severity_info, "    framerateexp      : %6d\n", SequenceHeader->framerateexp);
    report (severity_info, "    frmrtq_postproc   : %6d\n", SequenceHeader->frmrtq_postproc);
    report (severity_info, "    bitrtq_postproc   : %6d\n", SequenceHeader->bitrtq_postproc);
    report (severity_info, "    loopfilter        : %6d\n", EntryPointHeader->loopfilter);
    report (severity_info, "    multires          : %6d\n", SequenceHeader->multires);
    report (severity_info, "    fastuvmc          : %6d\n", EntryPointHeader->fastuvmc);
    report (severity_info, "    extended_ mv      : %6d\n", EntryPointHeader->extended_mv);
    report (severity_info, "    dquant            : %6d\n", EntryPointHeader->dquant);
    report (severity_info, "    vstransform       : %6d\n", EntryPointHeader->vstransform);
    report (severity_info, "    overlap           : %6d\n", EntryPointHeader->overlap);
    report (severity_info, "    syncmarker        : %6d\n", SequenceHeader->syncmarker);
    report (severity_info, "    rangered          : %6d\n", SequenceHeader->rangered);
    report (severity_info, "    maxbframes        : %6d\n", SequenceHeader->maxbframes);
    report (severity_info, "    quantizer         : %6d\n", EntryPointHeader->quantizer);
    report (severity_info, "    finterpflag       : %6d\n", SequenceHeader->finterpflag);
#endif

    SequenceLayerMetaDataValid          = true;

    return FrameParserNoError;
}
//}}}

//{{{  ReadPictureHeaderProgressive
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in the remainder of a progressive picture header
///
/// /////////////////////////////////////////////////////////////////////////
void FrameParser_VideoVc1_c::ReadPictureHeaderProgressive (void)
{
    Vc1VideoPicture_t*          Header;
    Vc1VideoSequence_t*         SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;

    Header                      = &FrameParameters->PictureHeader;
    SequenceHeader              = &StreamParameters->SequenceHeader;
    EntryPointHeader            = &StreamParameters->EntryPointHeader;

    if (SequenceHeader->finterpflag == 1)
        Header->interpfrm                       = Bits.Get(1);

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_B)
    {
        unsigned int    BFraction               = Bits.Get(3);                  // 7.1.1.14
        if (BFraction == 0x07)                                                  // 111b
            BFraction                          += Bits.Get(4);
        Header->bfraction_numerator             = BFractionNumerator[BFraction];
        Header->bfraction_denominator           = BFractionDenominator[BFraction];
    }

    Header->pqindex                             = Bits.Get(5);                  // 7.1.1.6
    if (Header->pqindex <= 8)
        Header->halfqp                          = Bits.Get(1);                  // 7.1.1.7

    if (EntryPointHeader->quantizer == 0x00)                                    // 7.1.1.8
    {
        Header->pquant                          = Pquant[Header->pqindex];
        Header->pquantizer                      = (Header->pqindex <= 8) ? 1 : 0;
    }
    else
    {
        Header->pquant                          = Header->pqindex;
        if (EntryPointHeader->quantizer == 0x01)                                    // 7.1.1.8
            Header->pquantizer                  = Bits.Get(1);
    }

    if (SequenceHeader->postprocflag == 1)                                      // 7.1.1.27
        Header->postproc                       = Bits.Get(2);

    // We are not interested in any of the remaining I/BI header information so return
    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) ||  (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
        return;

    // Leaving just P and B pictures below.
    if (EntryPointHeader->extended_mv == 0x01)                                  // 7.1.1.9
    {
        Header->mvrange                         = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->mvrange                         = VC1_VLC_RESULT(3, Header->mvrange);
    }
    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        for (Header->mvmode = 0; Header->mvmode < 4; Header->mvmode++)          // 7.1.1.32
            if (Bits.Get(1) == 0x01)
                break;
        Header->mvmode                          = (Header->pquant > 12) ? MvModeLowRate[Header->mvmode] : MvModeHighRate[Header->mvmode];

        if (Header->mvmode == VC1_MV_MODE_INTENSITY_COMP)
        {
            unsigned int LumaScale;
            unsigned int LumaShift;

            for (Header->mvmode2 = 0; Header->mvmode2 < 3; Header->mvmode2++)   // 7.1.1.33
                if (Bits.Get(1) == 0x01)
                    break;
            Header->mvmode2                     = (Header->pquant > 12) ? MvMode2LowRate[Header->mvmode2] : MvMode2HighRate[Header->mvmode2];

            LumaScale                           = Bits.Get(6);                  // 7.1.1.34
            LumaShift                           = Bits.Get(6);                  // 7.1.1.35
            Header->intensity_comp_top          = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_bottom       = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_field        = VC1_INTENSITY_COMP_BOTH;
        }
    }
    else
        Header->mvmode                          = Bits.Get(1);

}
//}}}
//{{{  ReadPictureHeaderInterlacedFrame
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in the remainder of an interlacedFrame picture header
///
/// /////////////////////////////////////////////////////////////////////////
void FrameParser_VideoVc1_c::ReadPictureHeaderInterlacedFrame (void)
{
    Vc1VideoPicture_t*          Header;
    Vc1VideoSequence_t*         SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;

    Header                      = &FrameParameters->PictureHeader;
    SequenceHeader              = &StreamParameters->SequenceHeader;
    EntryPointHeader            = &StreamParameters->EntryPointHeader;

    Header->pqindex                             = Bits.Get(5);                          // 7.1.1.6
    if (Header->pqindex <= 8)
        Header->halfqp                          = Bits.Get(1);                          // 7.1.1.7

    if (EntryPointHeader->quantizer == 0x00)                                            // 7.1.1.8
    {
        Header->pquant                          = Pquant[Header->pqindex];
        Header->pquantizer                      = (Header->pqindex <= 8) ? 1 : 0;
    }
    else
    {
        Header->pquant                          = Header->pqindex;
        if (EntryPointHeader->quantizer == 0x01)                                    // 7.1.1.8
            Header->pquantizer                  = Bits.Get(1);
    }

    if (SequenceHeader->postprocflag == 1)
        Header->postproc                       = Bits.Get(2);                           // 7.1.1.27

    // We are not interested in any of the remaining I/BI header information so return
    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) ||  (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
        return;

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_B)
    {
        unsigned int    BFraction               = Bits.Get(3);                          // see 7.1.1.14
        if (BFraction == 0x07)                                                          // 111b
            BFraction                          += Bits.Get(4);
        Header->bfraction_numerator             = BFractionNumerator[BFraction];
        Header->bfraction_denominator           = BFractionDenominator[BFraction];
    }

    if (EntryPointHeader->extended_mv == 0x01)                                          // 9.1.1.26
    {
        Header->mvrange                         = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->mvrange                         = VC1_VLC_RESULT(3, Header->mvrange);
    }
    if (EntryPointHeader->extended_dmv == 0x01)                                         // 9.1.1.27
    {
        Header->dmvrange                        = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->dmvrange                        = VC1_VLC_RESULT(3, Header->dmvrange);
    }

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        Bits.Get(1);                                                                    // 4mvswitch 9.1.1.28
        if (Bits.Get(1) == 1)                                                           // intensity compensation
        {
            unsigned int LumaScale;
            unsigned int LumaShift;

            LumaScale                           = Bits.Get(6);                          // 9.1.1.30
            LumaShift                           = Bits.Get(6);                          // 9.1.1.31
            Header->intensity_comp_top          = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_bottom       = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            Header->intensity_comp_field        = VC1_INTENSITY_COMP_BOTH;
        }
    }

}
//}}}
//{{{  ReadPictureHeaderInterlacedField
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in the remainder of an interlaced field picture header
///
/// /////////////////////////////////////////////////////////////////////////
void FrameParser_VideoVc1_c::ReadPictureHeaderInterlacedField (void)
{
    Vc1VideoPicture_t*          Header;
    Vc1VideoSequence_t*         SequenceHeader;
    Vc1VideoEntryPoint_t*       EntryPointHeader;
    unsigned int                MvModeBits;

    Header                      = &FrameParameters->PictureHeader;
    SequenceHeader              = &StreamParameters->SequenceHeader;
    EntryPointHeader            = &StreamParameters->EntryPointHeader;

    Header->pqindex                             = Bits.Get(5);                          // 7.1.1.6
    if (Header->pqindex <= 8)
        Header->halfqp                          = Bits.Get(1);                          // 7.1.1.7

    if (EntryPointHeader->quantizer == 0x00)                                    // 7.1.1.8
    {
        Header->pquant                          = Pquant[Header->pqindex];
        Header->pquantizer                      = (Header->pqindex <= 8) ? 1 : 0;
    }
    else
    {
        Header->pquant                          = Header->pqindex;
        if (EntryPointHeader->quantizer == 0x01)                                    // 7.1.1.8
            Header->pquantizer                  = Bits.Get(1);
    }

    if (SequenceHeader->postprocflag == 0x01)
        Header->postproc                       = Bits.Get(2);                           // 7.1.1.27

    // We are not interested in any of the remaining I/BI header information so return
    if ((Header->ptype == VC1_PICTURE_CODING_TYPE_I) ||  (Header->ptype == VC1_PICTURE_CODING_TYPE_BI))
        return;

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        Header->numref                          = Bits.Get(1);                          // 9.1.1.44
        if (Header->numref == 0)
            Header->reffield                    = Bits.Get(1);                          // 9.1.1.45
    }

    if (EntryPointHeader->extended_mv == 0x01)                                          // 9.1.1.26
    {
        Header->mvrange                         = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->mvrange                         = VC1_VLC_RESULT(3, Header->mvrange);
    }

    if (EntryPointHeader->extended_dmv == 0x01)                                         // 9.1.1.27
    {
        Header->dmvrange                        = BitsDotGetVc1VLC(3, VC1_VLC_LEAF_ZERO);
        Header->dmvrange                        = VC1_VLC_RESULT(3, Header->dmvrange);
    }

    // B fields do not have intensity compensation values - 9.1.1.46
    MvModeBits                                  = (Header->ptype == VC1_PICTURE_CODING_TYPE_P) ? 4 : 3;
    for (Header->mvmode = 0; Header->mvmode < MvModeBits; Header->mvmode++)             // 9.1.1.46
        if (Bits.Get(1) == 0x01)
            break;

    if (Header->ptype == VC1_PICTURE_CODING_TYPE_P)
    {
        Header->mvmode                          = (Header->pquant > 12) ? MvModeLowRate[Header->mvmode] : MvModeHighRate[Header->mvmode];
        if (Header->mvmode == VC1_MV_MODE_INTENSITY_COMP)
        {
            unsigned int LumaScale;
            unsigned int LumaShift;

            for (Header->mvmode2 = 0; Header->mvmode2 < 3; Header->mvmode2++)               // 9.1.1.47
                if (Bits.Get(1) == 0x01)
                    break;
            Header->mvmode2                     = (Header->pquant > 12) ? MvMode2LowRate[Header->mvmode2] : MvMode2HighRate[Header->mvmode2];

            if (Bits.Get(1) == 0x01)                                                        // 9.1.1.48
                Header->intensity_comp_field    = VC1_INTENSITY_COMP_BOTH;
            else if (Bits.Get(1) == 0x01)
                Header->intensity_comp_field    = VC1_INTENSITY_COMP_BOTTOM;
            else
                Header->intensity_comp_field    = VC1_INTENSITY_COMP_TOP;

            LumaScale                           = Bits.Get(6);                          // 9.1.1.49
            LumaShift                           = Bits.Get(6);                          // 9.1.1.50
            if (Header->intensity_comp_field == VC1_INTENSITY_COMP_BOTTOM)
                Header->intensity_comp_bottom   = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            else
                Header->intensity_comp_top      = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);

            if (Header->intensity_comp_field == VC1_INTENSITY_COMP_BOTH)
            {
                LumaScale                       = Bits.Get(6);                          // 9.1.1.51
                LumaShift                       = Bits.Get(6);                          // 9.1.1.52
                Header->intensity_comp_bottom   = (LumaScale << VC1_LUMASCALE_SHIFT) | (LumaShift << VC1_LUMASHIFT_SHIFT);
            }
        }
    }
    else
        Header->mvmode                          = (Header->pquant > 12) ? MvMode2LowRate[Header->mvmode] : MvMode2HighRate[Header->mvmode];

}
//}}}

//{{{  CommitFrameForDecode
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Send frame for decode
///             On a first slice code, we should have garnered all the data
///             we require we for a frame decode, this function records that fact.
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVc1_c::CommitFrameForDecode (void)
{
    unsigned int                        i;
    bool                                ProgressiveSequence;
    bool                                FieldSequenceError;
    PictureStructure_t                  PictureStructure;
    bool                                Frame;
    bool                                RepeatFirstField;
    bool                                TopFieldFirst;
    unsigned int                        PanAndScanCount;
    Vc1VideoPicture_t                  *PictureHeader;
    Vc1VideoSequence_t                 *SequenceHeader;
    Vc1VideoEntryPoint_t               *EntryPointHeader;
    SliceType_t                         SliceType;
    MatrixCoefficientsType_t            MatrixCoefficients;

    //
    // Check we have the headers we need
    //

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - Stream parameters unavailable for decode.\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - Frame parameters unavailable for decode (%p).\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader          = &StreamParameters->SequenceHeader;
    EntryPointHeader        = &StreamParameters->EntryPointHeader;
    PictureHeader           = &FrameParameters->PictureHeader;

    //
    // Obtain and check the progressive etc values.
    //

    SliceType               = SliceTypeTranslation[PictureHeader->ptype];
    ProgressiveSequence     = SequenceHeader->interlace == 0 ? true : false;

    if (PictureHeader->fcm == VC1_FCM_FIELD_INTERLACED)
        PictureStructure        = PictureStructures[(PictureHeader->tff << 1) | PictureHeader->first_field];
    else
        PictureStructure        = StructureFrame;
    Frame               = PictureStructure == StructureFrame;

    RepeatFirstField    = PictureHeader->rff;
    TopFieldFirst       = PictureHeader->tff;
    PanAndScanCount     = PictureHeader->ps_window_count;


    if (!Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField))
    {
        report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - Illegal combination (%c %c %c %c).\n",
                (ProgressiveSequence    ? 'T' : 'F'),
                (Frame                  ? 'T' : 'F'),
                (RepeatFirstField       ? 'T' : 'F'),
                (TopFieldFirst          ? 'T' : 'F'));
        //return FrameParserHeaderSyntaxError;
    }

    //
    // If we are doing field decode check for sequence error, and set appropriate flags
    //

    FieldSequenceError                  = (AccumulatedPictureStructure == PictureStructure);
    FirstDecodeOfFrame                  = FieldSequenceError || (AccumulatedPictureStructure == StructureEmpty);
    AccumulatedPictureStructure         = (FirstDecodeOfFrame && !Frame) ? PictureStructure : StructureEmpty;

    if (FieldSequenceError)
    {
        report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - Field sequence error detected.\n");
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
    }


    //
    // Deduce the matrix coefficients for colour conversions.
    //

    if( SequenceHeader->display_ext && SequenceHeader->color_format_flag )
    {
        switch( SequenceHeader->matrix_coef )
        {
            case VC1_MATRIX_COEFFICIENTS_BT709:         MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;       break;
            case VC1_MATRIX_COEFFICIENTS_FCC:           MatrixCoefficients      = MatrixCoefficients_FCC;               break;
            case VC1_MATRIX_COEFFICIENTS_BT470_BGI:     MatrixCoefficients      = MatrixCoefficients_ITU_R_BT470_2_BG;  break;
            case VC1_MATRIX_COEFFICIENTS_SMPTE_170M:    MatrixCoefficients      = MatrixCoefficients_SMPTE_170M;        break;
            case VC1_MATRIX_COEFFICIENTS_SMPTE_240M:    MatrixCoefficients      = MatrixCoefficients_SMPTE_240M;        break;

            default:
            case VC1_MATRIX_COEFFICIENTS_FORBIDDEN:
            case VC1_MATRIX_COEFFICIENTS_RESERVED:
                                                        report( severity_error, "FrameParser_VideoVC1_c::CommitFrameForDecode - Forbidden or reserved matrix coefficient code specified (%02x)\n", SequenceHeader->matrix_coef );
                                                        // fall through

            case VC1_MATRIX_COEFFICIENTS_UNSPECIFIED:   MatrixCoefficients      = MatrixCoefficients_ITU_R_BT601;               break;
        }
    }
    else
    {
        MatrixCoefficients = MatrixCoefficients_ITU_R_BT601;
    }

    //
    // Nick added this to make vc1 struggle through
    //

    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;

    //
    // Record the stream and frame parameters into the appropriate structure
    //

    ParsedFrameParameters->KeyFrame                             = SliceType == SliceTypeI;
//     if (SliceType != SliceTypeI)
//       CodedFramePlaybackTimeValid = false;
    //report (severity_error, "Slite type %d.\n", PictureHeader->ptype);
    if (PictureHeader->ptype != 1) { //SliceType == SliceTypeB || PictureHeader->ptype == VC1_PICTURE_CODING_TYPE_BI) {
//       ParsedFrameParameters->NormalizedPlaybackTime = INVALID_TIME;
//       CodedFramePlaybackTimeValid = false;
      //report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - B or BI frame, ignoring PTS.\n");
    }
    ParsedFrameParameters->ReferenceFrame                       = (SliceType != SliceTypeB) && (PictureHeader->ptype != VC1_PICTURE_CODING_TYPE_BI);
    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;

    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(Vc1StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;

    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(Vc1FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;

//
    if (EntryPointHeader->coded_size_flag == 1)
    {
        ParsedVideoParameters->Content.Width                    = EntryPointHeader->coded_width;
        ParsedVideoParameters->Content.Height                   = EntryPointHeader->coded_height;
    }
    else
    {
        ParsedVideoParameters->Content.Width                    = SequenceHeader->max_coded_width;
        ParsedVideoParameters->Content.Height                   = SequenceHeader->max_coded_height;
    }

    if (SequenceHeader->display_ext == 1)
    {
        ParsedVideoParameters->Content.DisplayWidth             = SequenceHeader->disp_horiz_size;
        ParsedVideoParameters->Content.DisplayHeight            = SequenceHeader->disp_vert_size;
    }
#if 0
    else
    {
        ParsedVideoParameters->Content.DisplayWidth             = ParsedVideoParameters->Content.Width;
        ParsedVideoParameters->Content.DisplayHeight            = ParsedVideoParameters->Content.Height;
    }
#endif

    if (SequenceHeader->frame_rate_flag == 1)
    {
        if (SequenceHeader->frame_rate_ind == 0)        //      (0 - 7)                 +    (1 (1000), or 2 (1001)) = (0 - 15)
            ParsedVideoParameters->Content.FrameRate            = FrameRates(SequenceHeader->frameratenr + ((SequenceHeader->frameratedr-1) << 3));
        else
            ParsedVideoParameters->Content.FrameRate            = Rational_t((SequenceHeader->framerateexp+1), 32);
    }
    //else if (SequenceHeader->frame_rate_ind == 1)               // this is a special case used by WMV
    //    ParsedVideoParameters->Content.FrameRate                = Rational_t(SequenceHeader->frameratenr, SequenceHeader->frameratedr);
    else if (FrameRateValid)
    {
        ParsedVideoParameters->Content.FrameRate                = this->FrameRate;
    }
    else
    {
#warning "Need to calculate frame rates from PTS values"
#if 0
        if (PtsPresent)
        {
            unsigned int        PtsDifference   = (unsigned int)(Pts - LastPts);

            FrameRate   = Rational_t (90000, Pts - LastPts);
            report (severity_info, "FrameParser_VideoVc1_c::CommitFrameForDecode - No framerate, use PTS (Not Implemented).\n");
	    FrameRate.IntegerPart(), FrameRate.RemainderDecimal());
        }
#endif
        //report (severity_error, "FrameParser_VideoVc1_c::CommitFrameForDecode - No framerate in ES.\n");
        //ParsedVideoParameters->Content.FrameRate                = Rational_t (24000, 1001);
        ParsedVideoParameters->Content.FrameRate                = Rational_t (FrameRateDefaultNum, FrameRateDefaultDen);
    }

    ParsedVideoParameters->Content.Progressive                  = ProgressiveSequence;
    ParsedVideoParameters->Content.OverscanAppropriate  = false;

    if (SequenceHeader->aspect_ratio_flag == 1)
    {
        if (SequenceHeader->aspect_ratio == 15)
            ParsedVideoParameters->Content.PixelAspectRatio     = Rational_t (SequenceHeader->aspect_horiz_size, SequenceHeader->aspect_vert_size);
        else
            ParsedVideoParameters->Content.PixelAspectRatio     = AspectRatios(SequenceHeader->aspect_ratio);
    }
    else if (SequenceHeader->display_ext == 1)
    {
        // This calculation assumes that the display aspect ratio is 1:1.  Therefore to convert to the pixel aspect ratio we
        // must multiply by the horizontal pixel count and divide by the vertical pixel count
        ParsedVideoParameters->Content.PixelAspectRatio         = Rational_t ((ParsedVideoParameters->Content.DisplayHeight * ParsedVideoParameters->Content.Width),
                                                                              (ParsedVideoParameters->Content.DisplayWidth * ParsedVideoParameters->Content.Height));
    }
    else
        ParsedVideoParameters->Content.PixelAspectRatio         = 1;

    ParsedVideoParameters->SliceType                            = SliceType;
    ParsedVideoParameters->PictureStructure                     = PictureStructure;
    ParsedVideoParameters->InterlacedFrame                      = PictureHeader->fcm == VC1_FCM_PROGRESSIVE ? false : true;
    ParsedVideoParameters->TopFieldFirst                        = TopFieldFirst;

    ParsedVideoParameters->DisplayCount[0]                      = DisplayCount0(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    ParsedVideoParameters->DisplayCount[1]                      = DisplayCount1(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

#if 0
    static unsigned int FrameNo      = 0;
    report (severity_info, "%s: (%d, %d, %d, %d) - CountsIndex %d Legal %d PanScanCount %d \n", __FUNCTION__,
            ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField,
            CountsIndex(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField),
            Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField),
            PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField));

    report (severity_info, "%s: - DisplayCount0 %d DisplayCount1 %d, FrameNo %d\n", __FUNCTION__,
            ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1], FrameNo++);
#endif

    ParsedVideoParameters->PanScan.Count                        = PanAndScanCount;

    for (i=0; i<PanAndScanCount; i++)
    {
        ParsedVideoParameters->PanScan.DisplayCount[i]          = 1;
        ParsedVideoParameters->PanScan.HorizontalOffset[i]      = PictureHeader->ps_hoffset[i];
        ParsedVideoParameters->PanScan.VerticalOffset[i]        = PictureHeader->ps_voffset[i];
    }

    //
    // Record our claim on both the frame and stream parameters
    //

    Buffer->AttachBuffer (StreamParametersBuffer);
    Buffer->AttachBuffer (FrameParametersBuffer);

    //
    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This 
    // will generate an error should I accidentally write code that 
    // accesses this data when it should not.
    //

    FrameParameters             = NULL;

    //
    // Finally set the appropriate flag and return
    //

    FrameToDecode       = true;

    return FrameParserNoError;
}
//}}}
//{{{  NewStreamParametersCheck
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Boolean function to evaluate whether or not the stream
///             parameters are new.
///
/// /////////////////////////////////////////////////////////////////////////
bool   FrameParser_VideoVc1_c::NewStreamParametersCheck (void)
{
    bool            Different;

    //
    // The parameters cannot be new if they have been used before.
    //

    if (!StreamParameters->UpdatedSinceLastFrame)
        return false;

    StreamParameters->UpdatedSinceLastFrame     = false;

    //
    // Check for difference using a straightforward comparison to see if the
    // stream parameters have changed. (since we zero on allocation simple
    // memcmp should be sufficient).
    //

    Different   = memcmp (&CopyOfStreamParameters, StreamParameters, sizeof(Vc1StreamParameters_t)) != 0;
    if (Different)
    {
        memcpy (&CopyOfStreamParameters, StreamParameters, sizeof(Vc1StreamParameters_t));
        return true;
    }

//

    return false;
}
//}}}
//{{{  BitsDotGetVc1VLC
unsigned long   FrameParser_VideoVc1_c::BitsDotGetVc1VLC (unsigned long MaxBits, unsigned long LeafNode)
{
    unsigned long BitsRead=0;
    unsigned long NextBit=0;
    unsigned long Result=0;

    while (BitsRead < MaxBits)
    {
        NextBit = Bits.Get(1);

        Result = (Result << 1) | NextBit;

        BitsRead++;

        if (NextBit == LeafNode)
            break;
    }

    Result |= (BitsRead << MaxBits);

    //report (severity_info, "%s: Result = %x, BitsRead %d\n", __FUNCTION__, Result, BitsRead);

    return (Result);
}

//}}}

