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

//#define DUMP_HEADERS

#include "ring_generic.h"
#include "frame_parser_video_mpeg2.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoMpeg2_c"

static BufferDataDescriptor_t     Mpeg2StreamParametersBuffer = BUFFER_MPEG2_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     Mpeg2FrameParametersBuffer = BUFFER_MPEG2_FRAME_PARAMETERS_TYPE;

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
    { false, 0, 0, 0 },         //    0       0      0     1    // Not legal not seen
    { true,  1, 1, 0 },         //    0       0      1     0
    { false, 0, 0, 0 },         //    0       0      1     1    // Not legal not seen
    { true,  2, 1, 1 },         //    0       1      0     0
    { true,  3, 2, 1 },         //    0       1      0     1
    { true,  2, 1, 1 },         //    0       1      1     0
    { true,  3, 2, 1 },         //    0       1      1     1
    { false, 0, 0, 0 },         //    1       0      0     0    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      0     1    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      1     0    // Not legal not seen
    { false, 0, 0, 0 },         //    1       0      1     1    // Not legal not seen
    { true,  1, 1, 0 },         //    1       1      0     0
    { true,  2, 2, 0 },         //    1       1      0     1
    { true,  1, 1, 0 },         //    1       1      1     0    // Not legal but seen in actual use so we allow it
    { true,  3, 3, 0 }          //    1       1      1     1
};

#define CountsIndex( PS, F, TFF, RFF )          ((((PS) != 0) << 3) | (((F) != 0) << 2) | (((TFF) != 0) << 1) | ((RFF) != 0))
#define Legal( PS, F, TFF, RFF )                Counts[CountsIndex( PS, F, TFF, RFF )].LegalValue
#define PanScanCount( PS, F, TFF, RFF )         Counts[CountsIndex( PS, F, TFF, RFF )].PanScanCountValue
#define DisplayCount0( PS, F, TFF, RFF )        Counts[CountsIndex( PS, F, TFF, RFF )].DisplayCount0Value
#define DisplayCount1( PS, F, TFF, RFF )        Counts[CountsIndex( PS, F, TFF, RFF )].DisplayCount1Value

#define MIN_LEGAL_MPEG2_ASPECT_RATIO_CODE       1
#define MAX_LEGAL_MPEG2_ASPECT_RATIO_CODE       4

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     Mpeg2AspectRatioValues[][2]     =
{
    {   1,   0 },       // not a valid ratio: MIN_LEGAL_MPEG1_ASPECT_RATIO_CODE is 1
    {   1,   1 },
    {   4,   3 },
    {  16,   9 },
    { 221, 100 }
};

#define Mpeg2AspectRatios(N) Rational_t(Mpeg2AspectRatioValues[N][0],Mpeg2AspectRatioValues[N][1])

#define MIN_LEGAL_MPEG1_ASPECT_RATIO_CODE       1
#define MAX_LEGAL_MPEG1_ASPECT_RATIO_CODE       14

// BEWARE !!!! you cannot declare static initializers of a constructed type such as Rational_t
//             the compiler will silently ignore them..........
static unsigned int     Mpeg1AspectRatioValues[][2]     =
{
    {0, 1},
    {10000, 10000},
    { 6735, 10000},
    { 7031, 10000},
    { 7615, 10000},
    { 8055, 10000},
    { 8437, 10000},
    { 8935, 10000},
    { 9157, 10000},
    { 9815, 10000},
    {10255, 10000},
    {10695, 10000},
    {10950, 10000},
    {11575, 10000},
    {12015, 10000}
};

#define Mpeg1AspectRatios(N) Rational_t(Mpeg1AspectRatioValues[N][0],Mpeg1AspectRatioValues[N][1])

#define MIN_LEGAL_FRAME_RATE_CODE               1
#define MAX_LEGAL_FRAME_RATE_CODE               8

static unsigned int     FrameRateValues[][2]    =
{
    {0, 1},
    { 24000, 1001 },
    { 24, 1 },
    { 25, 1 },
    { 30000, 1001 },
    { 30, 1 },
    { 50, 1 },
    { 60000, 1001 },
    { 60, 1 }
};

#define FrameRates(N) Rational_t(FrameRateValues[N][0],FrameRateValues[N][1])

static PictureStructure_t       PictureStructures[]     =
{
    StructureFrame,                 // Strictly illegal
    StructureTopField,
    StructureBottomField,
    StructureFrame
};

static int QuantizationMatrixNaturalOrder[QUANTISER_MATRIX_SIZE] =
{
    0,   1,  8, 16,  9,  2,  3, 10,                              // This translates zigzag matrix indices,
    17, 24, 32, 25, 18, 11,  4,  5,                              // used in coefficient transmission,
    12, 19, 26, 33, 40, 48, 41, 34,                              // to natural order indices.
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

static SliceType_t SliceTypeTranslation[]  = { INVALID_INDEX, SliceTypeI, SliceTypeP, SliceTypeB, SliceTypeB };

#define REFERENCE_FRAMES_NEEDED( CodingType )           (CodingType - 1)
#define MAX_REFERENCE_FRAMES_FORWARD_PLAY               REFERENCE_FRAMES_NEEDED(MPEG2_PICTURE_CODING_TYPE_B)

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoMpeg2_c::FrameParser_VideoMpeg2_c(void)
    : FrameParser_Video_c()
    , CopyOfStreamParameters()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , LastPanScanHorizontalOffset(0)
    , LastPanScanVerticalOffset(0)
    , EverSeenRepeatFirstField(false)
    , LastFirstFieldWasAnI(false)
    , LastRecordedTemporalReference(0)
{
    Configuration.FrameParserName               = "Unspecified";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &Mpeg2StreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &Mpeg2FrameParametersBuffer;
    Configuration.MaxUserDataBlocks             = 3;

    FirstDecodeOfFrame                          = false;  // specific..
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoMpeg2_c::~FrameParser_VideoMpeg2_c(void)
{
    Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::Connect(Port_c *Port)
{
    PlayerStatus_t  Status;
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
    Status      = FrameParser_Video_c::Connect(Port);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    //
    // Pass the call down the line
    //
    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The present collated header function
//
FrameParserStatus_t FrameParser_VideoMpeg2_c::PresentCollatedHeader(
    unsigned char StartCode,
    unsigned char *HeaderBytes,
    FrameParserHeaderFlag_t *Flags)
{
    bool StartOfFrame;
    *Flags = 0;
    StartOfFrame = (StartCode == MPEG2_PICTURE_START_CODE);

    if (StartOfFrame)
    {
        *Flags |= FrameParserHeaderFlagPartitionPoint;
    }

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadHeaders(void)
{
    unsigned int            i;
    unsigned int            Code;
    FrameParserStatus_t     Status;
    unsigned int            ExtensionCode;
    bool                    IgnoreStreamComponents;
    bool                    IgnoreFrameComponents;

    IgnoreFrameComponents               = true;
    IgnoreStreamComponents              = true;

    for (i = 0; i < StartCodeList->NumberOfStartCodes; i++)
    {
        Code    = StartCodeList->StartCodes[i];
        Bits.SetPointer(BufferData + ExtractStartCodeOffset(Code));
        Bits.FlushUnseen(32);
        Status  = FrameParserNoError;

        switch (ExtractStartCodeCode(Code))
        {
        case   MPEG2_PICTURE_START_CODE:
            // Update the user data number with saved user data index as we can lose it if GOP (with user data)
            // and the frame aren't in the same buffer (ParsedFrameParameters->UserDataNumber become zero)
            ParsedFrameParameters->UserDataNumber = UserDataIndex;
            Status  = ReadPictureHeader();
            IgnoreFrameComponents   = (Status != FrameParserNoError);
            break;

        case   MPEG2_FIRST_SLICE_START_CODE:
            // Initialise UserDataIndex as no more user data for this frame.
            UserDataIndex = 0;

            if (IgnoreFrameComponents)
            {
                break;
            }

            ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
            Status  = CommitFrameForDecode();
            IgnoreFrameComponents                   = true;
            break;

        case   MPEG2_USER_DATA_START_CODE:
            unsigned int UserDataLength;
            unsigned char *UserDataBufferStart;
            // Calculate the user data length for 2 cases :
            // we are at latest start code or not
            UserDataLength = ((i == (StartCodeList->NumberOfStartCodes - 1)) ?
                              (BufferLength - ExtractStartCodeOffset(Code)) :
                              (ExtractStartCodeOffset(StartCodeList->StartCodes[i + 1]) - ExtractStartCodeOffset(Code)));
            // Remove the user data start code length from the total user data length
            UserDataLength -= USER_DATA_START_CODE_LENGTH;
            // Calculate the start of user
            UserDataBufferStart  = BufferData + ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
            // Shift the start of user data to after the user data start code
            UserDataBufferStart += USER_DATA_START_CODE_LENGTH;
            // Now read user datas
            Status   = ReadUserData(UserDataLength, UserDataBufferStart);
            break;

        case   MPEG2_SEQUENCE_HEADER_CODE:
            // Initialise UserDataIndex for next frame
            UserDataIndex = 0;
            Status   = ReadSequenceHeader();
            IgnoreStreamComponents  = (Status != FrameParserNoError);
            break;

        case   MPEG2_SEQUENCE_ERROR_CODE:
            break;

        case   MPEG2_EXTENSION_START_CODE:
            ExtensionCode = Bits.Get(4);

            switch (ExtensionCode)
            {
            case   MPEG2_SEQUENCE_EXTENSION_ID:
                if (IgnoreStreamComponents)
                {
                    break;
                }

                Status   = ReadSequenceExtensionHeader();
                break;

            case   MPEG2_SEQUENCE_DISPLAY_EXTENSION_ID:
                if (IgnoreStreamComponents)
                {
                    break;
                }

                Status   = ReadSequenceDisplayExtensionHeader();
                break;

            case   MPEG2_QUANT_MATRIX_EXTENSION_ID:
                Status   = ReadQuantMatrixExtensionHeader();
                break;

            case   MPEG2_SEQUENCE_SCALABLE_EXTENSION_ID:
                if (IgnoreStreamComponents)
                {
                    break;
                }

                Status   = ReadSequenceScalableExtensionHeader();
                break;

            case   MPEG2_PICTURE_DISPLAY_EXTENSION_ID:
                if (IgnoreFrameComponents)
                {
                    break;
                }

                Status   = ReadPictureDisplayExtensionHeader();

                if (Status != FrameParserNoError)
                {
                    IgnoreFrameComponents       = true;
                }

                break;

            case   MPEG2_PICTURE_CODING_EXTENSION_ID:
                if (IgnoreFrameComponents)
                {
                    break;
                }

                Status   = ReadPictureCodingExtensionHeader();
                break;

            case   MPEG2_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
                if (IgnoreFrameComponents)
                {
                    break;
                }

                Status   = ReadPictureSpatialScalableExtensionHeader();
                break;

            case   MPEG2_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
                if (IgnoreFrameComponents)
                {
                    break;
                }

                Status   = ReadPictureTemporalScalableExtensionHeader();
                break;

            default:
                SE_ERROR("Unknown/Unsupported extension header %02x\n", ExtensionCode);
                Status  = FrameParserUnhandledHeader;
                break;
            }

            if ((Status == FrameParserNoError) && (StreamParameters != NULL))
            {
                StreamParameters->StreamType        = MpegStreamTypeMpeg2;
            }

            break;

        case   MPEG2_SEQUENCE_END_CODE:
            if (PlaybackDirection == PlayForward)
            {
                GotSequenceEndCode = true;
            }

            break;

        case   MPEG2_GROUP_START_CODE:
            if (IgnoreStreamComponents)
            {
                break;
            }
            Status  = ReadGroupOfPicturesHeader();
            LastRecordedTemporalReference = 0;
            break;

        case   MPEG2_VIDEO_PES_START_CODE:
            SE_ERROR("Pes headers should be removed\n");
            Status  = FrameParserUnhandledHeader;
            break;

        default:
            SE_ERROR("Unknown/Unsupported header %02x\n", ExtractStartCodeCode(Code));
            Status  = FrameParserUnhandledHeader;
            break;
        }

        if (Status != FrameParserNoError)
        {
            IncrementErrorStatistics(Status);
        }

        if ((Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader))
        {
            IgnoreStreamComponents      = true;
            IgnoreFrameComponents       = true;
        }
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ResetReferenceFrameList(void)
{
    Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, CODEC_RELEASE_ALL);
    ReferenceFrameList.EntryCount       = 0;

    // Reset counters involved in field reference management to avoid issues in case of ad insertion
    OldAccumulatedPictureStructure = AccumulatedPictureStructure = StructureEmpty;
    EverSeenRepeatFirstField = false;
    LastFirstFieldWasAnI = false;
    FirstDecodeOfFrame = false;
    SE_DEBUG(GetGroupTrace(), "Resetting field variables on discontinuity\n");

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//      Stream specific override function for processing decode stacks, this
//      initializes the post decode ring before passing into the real
//      implementation of this function.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayProcessDecodeStacks(void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();
    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to generate the post decode parameter
//      settings for reverse play, these consist of the display frame index,
//      and presentation time, both of which may be deferred if the information
//      is unavailable.
//
//      For mpeg2 reverse play, this function will use a simple numbering system,
//      Imaging a sequence  I B B P B B this should be numbered (in reverse) as
//                          3 5 4 0 2 1
//      These will be presented to this function in reverse order ( B B P B B I )
//      so for non ref frames we ring them, and for ref frames we use the next number
//      and then process what is on the ring.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayGeneratePostDecodeParameterSettings(void)
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
            ReverseQueuedPostDecodeSettingsRing->Insert((uintptr_t)ParsedFrameParameters);
            ReverseQueuedPostDecodeSettingsRing->Insert((uintptr_t)ParsedVideoParameters);
        }
        else
            //
            // If this is a reference frame then first process it, then process the frames on the ring
            //
        {
            CalculateFrameIndexAndPts(ParsedFrameParameters, ParsedVideoParameters);

            while (ReverseQueuedPostDecodeSettingsRing->NonEmpty())
            {
                ReverseQueuedPostDecodeSettingsRing->Extract((uintptr_t *)&DeferredParsedFrameParameters);
                ReverseQueuedPostDecodeSettingsRing->Extract((uintptr_t *)&DeferredParsedVideoParameters);
                CalculateFrameIndexAndPts(DeferredParsedFrameParameters, DeferredParsedVideoParameters);
            }
        }
    }

//
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to prepare a reference frame list
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::PrepareReferenceFrameList(void)
{
    unsigned int    i;
    bool            Field;
    bool            SecondField;
    bool            FirstField;
    bool            SelfReference;
    unsigned int    ReferenceFramesNeeded;
    unsigned int    PictureCodingType;
    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    //
    PictureCodingType           = ((Mpeg2FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader.picture_coding_type;
    ReferenceFramesNeeded       = REFERENCE_FRAMES_NEEDED(PictureCodingType);
    //
    // Detect the special case of a second field referencing the first
    // this is when we have the field startup condition
    //          I P P P
    // where the first P actually references its own decode buffer.
    //
    Field                       = (ParsedVideoParameters->PictureStructure != StructureFrame);
    SecondField                 = Field && !ParsedFrameParameters->FirstParsedParametersForOutputFrame;
    FirstField                  = Field && ParsedFrameParameters->FirstParsedParametersForOutputFrame;
    SelfReference               = SecondField && (ReferenceFramesNeeded == 1) && LastFirstFieldWasAnI;

    if (FirstField)
    {
        LastFirstFieldWasAnI    = (PictureCodingType == MPEG2_PICTURE_CODING_TYPE_I);
    }

    //
    // Now we cannot decode if we do not have enbough reference frames,
    // and this is not the most heinous of special cases.
    //

    if (!SelfReference && (ReferenceFrameList.EntryCount < ReferenceFramesNeeded))
    {
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    if (SelfReference)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0]   = NextDecodeFrameIndex;
    }
    else
    {
        for (i = 0; i < ReferenceFramesNeeded; i++)
        {
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
        }
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to manage a reference frame list in forward play
//      we only record a reference frame as such on the last field, in order to
//      ensure the correct management of reference frames in the codec, we immediately
//      inform the codec of a release on the first field of a field picture.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ForPlayUpdateReferenceFrameList(void)
{
    unsigned int    i;
    bool            LastField;

    if (ParsedFrameParameters->ReferenceFrame)
    {
        LastField       = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                          !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

        if (LastField)
        {
            if (ReferenceFrameList.EntryCount >= MAX_REFERENCE_FRAMES_FORWARD_PLAY)
            {
                Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);
                ReferenceFrameList.EntryCount--;

                for (i = 0; i < ReferenceFrameList.EntryCount; i++)
                {
                    ReferenceFrameList.EntryIndicies[i] = ReferenceFrameList.EntryIndicies[i + 1];
                }
            }

            ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
        }
        else
        {
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);
        }
    }

    return FrameParserNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to check if there is a deferred picture
//      that could be pushed to display. This check is mpeg2 specific and uses
//  temporal reference information.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ForPlayCheckForReferenceReadyForManifestation()
{
    Mpeg2FrameParameters_t    *Mpeg2FrameParameters;


    if (ParsedFrameParameters != NULL)
    {
        Mpeg2FrameParameters           = (Mpeg2FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
        // displayframeindex for reference frame could also be calculated if displayindex of its immediate previous frame
        // in display order is already computed. As temporal_reference indicates display order, so this is used to
        // determine immediate previous frame in displayorder
        if ((Mpeg2FrameParameters->PictureHeader.temporal_reference == (LastRecordedTemporalReference + 1)) &&
            (FirstDecodeOfFrame))
        {
            // In this function, the display index is computed, allowing the picture to be sent to manifest.
            ForPlayPurgeQueuedPostDecodeParameterSettings();

        }
    }

    return FrameParserNoError;
}

void FrameParser_VideoMpeg2_c::StoreTemporalReferenceForLastRecordedFrame(ParsedFrameParameters_t *ParsedFrame)
{
    LastRecordedTemporalReference = ((Mpeg2FrameParameters_t *)ParsedFrame->FrameParameterStructure)->PictureHeader.temporal_reference;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to add a frame to the reference
//      frame list in reverse play.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayAppendToReferenceFrameList(void)
{
    bool LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                       !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if (ParsedFrameParameters->ReferenceFrame && LastField)
    {
        if (ReferenceFrameList.EntryCount >= MAX_ENTRIES_IN_REFERENCE_FRAME_LIST)
        {
            SE_ERROR("List full - Implementation error\n");
            return PlayerImplementationError;
        }

        ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount++] = ParsedFrameParameters->DecodeFrameIndex;
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to remove a frame from the reference
//      frame list in reverse play.
//
//      Note, we only inserted the reference frame in the list on the last
//      field but we need to inform the codec we are finished with it on both
//      fields (for field pictures).
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayRemoveReferenceFrameFromList(void)
{
    bool LastField   = (ParsedVideoParameters->PictureStructure == StructureFrame) ||
                       !ParsedFrameParameters->FirstParsedParametersForOutputFrame;

    if ((ReferenceFrameList.EntryCount != 0) && LastField)
    {
        Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);

        if (LastField)
        {
            ReferenceFrameList.EntryCount--;
        }
    }

    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific function to junk the reference frame list
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::RevPlayJunkReferenceFrameList(void)
{
    ReferenceFrameList.EntryCount       = 0;
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceHeader(void)
{
    int                                      i;
    Mpeg2VideoSequence_t                    *Header;
    Mpeg2VideoQuantMatrixExtension_t        *CumulativeHeader;

    FrameParserStatus_t Status = GetNewStreamParameters((void **)&StreamParameters);
    if (Status != FrameParserNoError)
    {
        return Status;
    }

    StreamParameters->UpdatedSinceLastFrame     = true;
    Header                                      = &StreamParameters->SequenceHeader;
    memset(Header, 0, sizeof(Mpeg2VideoSequence_t));
    Header->horizontal_size_value                       = Bits.Get(12);
    Header->vertical_size_value                         = Bits.Get(12);
    Header->aspect_ratio_information                    = Bits.Get(4);
    Header->frame_rate_code                             = Bits.Get(4);
    Header->bit_rate_value                              = Bits.Get(18);
    MarkerBit(1);
    Header->vbv_buffer_size_value                       = Bits.Get(10);
    Header->constrained_parameters_flag                 = Bits.Get(1);
    Header->load_intra_quantizer_matrix                 = Bits.Get(1);

    if (Header->load_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]   = Bits.Get(8);
        }

    Header->load_non_intra_quantizer_matrix             = Bits.Get(1);

    if (Header->load_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]] = Bits.Get(8);
        }

    CumulativeHeader    = &StreamParameters->CumulativeQuantMatrices;

    if (Header->load_intra_quantizer_matrix)
    {
        CumulativeHeader->load_intra_quantizer_matrix           = Header->load_intra_quantizer_matrix;
        memcpy(CumulativeHeader->intra_quantizer_matrix, Header->intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    if (Header->load_non_intra_quantizer_matrix)
    {
        CumulativeHeader->load_non_intra_quantizer_matrix       = Header->load_non_intra_quantizer_matrix;
        memcpy(CumulativeHeader->non_intra_quantizer_matrix, Header->non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    if (!inrange(Header->frame_rate_code, MIN_LEGAL_FRAME_RATE_CODE, MAX_LEGAL_FRAME_RATE_CODE))
    {
        SE_ERROR("frame_rate_code has illegal value (%02x)\n", Header->frame_rate_code);
        return FrameParserHeaderSyntaxError;
    }

    StreamParameters->SequenceHeaderPresent             = true;

    // Reset the last pan scan offsets
    LastPanScanHorizontalOffset         = 0;
    LastPanScanVerticalOffset           = 0;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Sequence header :-\n");
    SE_INFO(group_frameparser_video,  "    horizontal_size_value        : %6d\n", Header->horizontal_size_value);
    SE_INFO(group_frameparser_video,  "    vertical_size_value          : %6d\n", Header->vertical_size_value);
    SE_INFO(group_frameparser_video,  "    aspect_ratio_information     : %6d\n", Header->aspect_ratio_information);
    SE_INFO(group_frameparser_video,  "    frame_rate_code              : %6d\n", Header->frame_rate_code);
    SE_INFO(group_frameparser_video,  "    bit_rate_value               : %6d\n", Header->bit_rate_value);
    SE_INFO(group_frameparser_video,  "    vbv_buffer_size_value        : %6d\n", Header->vbv_buffer_size_value);
    SE_INFO(group_frameparser_video,  "    load_intra_quantizer_matrix  : %6d\n", Header->load_intra_quantizer_matrix);

    if (Header->load_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

    SE_INFO(group_frameparser_video,  "    load_non_intra_quantizer_matrix  : %6d\n", Header->load_non_intra_quantizer_matrix);

    if (Header->load_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->non_intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceExtensionHeader(void)
{
    Mpeg2VideoSequenceExtension_t   *Header;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header = &StreamParameters->SequenceExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoSequenceExtension_t));
    Header->profile_and_level_indication        = Bits.Get(8);
    Header->progressive_sequence                = Bits.Get(1);
    Header->chroma_format                       = Bits.Get(2);
    Header->horizontal_size_extension           = Bits.Get(2);
    Header->vertical_size_extension             = Bits.Get(2);
    Header->bit_rate_extension                  = Bits.Get(12);
    MarkerBit(1);
    Header->vbv_buffer_size_extension           = Bits.Get(8);
    Header->low_delay                           = Bits.Get(1);
    Header->frame_rate_extension_n              = Bits.Get(2);
    Header->frame_rate_extension_d              = Bits.Get(5);

    if (Header->chroma_format != MPEG2_SEQUENCE_CHROMA_4_2_0)
    {
        SE_ERROR("Unsupported chroma format %d\n", Header->chroma_format);
        Stream->MarkUnPlayable();
        return FrameParserHeaderUnplayable;
    }

    StreamParameters->SequenceExtensionHeaderPresent    = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Sequence Extension header :-\n");
    SE_INFO(group_frameparser_video,  "    profile_and_level_indication : %6d\n", Header->profile_and_level_indication);
    SE_INFO(group_frameparser_video,  "    progressive_sequence         : %6d\n", Header->progressive_sequence);
    SE_INFO(group_frameparser_video,  "    chroma_format                : %6d\n", Header->chroma_format);
    SE_INFO(group_frameparser_video,  "    horizontal_size_extension    : %6d\n", Header->horizontal_size_extension);
    SE_INFO(group_frameparser_video,  "    vertical_size_extension      : %6d\n", Header->vertical_size_extension);
    SE_INFO(group_frameparser_video,  "    bit_rate_extension           : %6d\n", Header->bit_rate_extension);
    SE_INFO(group_frameparser_video,  "    vbv_buffer_size_extension    : %6d\n", Header->vbv_buffer_size_extension);
    SE_INFO(group_frameparser_video,  "    low_delay                    : %6d\n", Header->low_delay);
    SE_INFO(group_frameparser_video,  "    frame_rate_extension_n       : %6d\n", Header->frame_rate_extension_n);
    SE_INFO(group_frameparser_video,  "    frame_rate_extension_d       : %6d\n", Header->frame_rate_extension_d);
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence display extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceDisplayExtensionHeader(void)
{
    Mpeg2VideoSequenceDisplayExtension_t    *Header;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceDisplayExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoSequenceDisplayExtension_t));
    Header->video_format                      = Bits.Get(3);
    Header->color_description                 = Bits.Get(1);

    if (Header->color_description != 0)
    {
        Header->color_primaries               = Bits.Get(8);
        Header->transfer_characteristics      = Bits.Get(8);
        Header->matrix_coefficients           = Bits.Get(8);
    }

    Header->display_horizontal_size           = Bits.Get(14);
    MarkerBit(1);
    Header->display_vertical_size             = Bits.Get(14);

    StreamParameters->SequenceDisplayExtensionHeaderPresent     = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Sequence Display Extension header :-\n");
    SE_INFO(group_frameparser_video,  "    video_format                 : %6d\n", Header->video_format);

    if (Header->color_description != 0)
    {
        SE_INFO(group_frameparser_video,  "    color_primaries              : %6d\n", Header->color_primaries);
        SE_INFO(group_frameparser_video,  "    transfer_characteristics     : %6d\n", Header->transfer_characteristics);
        SE_INFO(group_frameparser_video,  "    matrix_coefficients          : %6d\n", Header->matrix_coefficients);
    }

    SE_INFO(group_frameparser_video,  "    display_horizontal_size      : %6d\n", Header->display_horizontal_size);
    SE_INFO(group_frameparser_video,  "    display_vertical_size        : %6d\n", Header->display_vertical_size);
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadSequenceScalableExtensionHeader(void)
{
    Mpeg2VideoSequenceScalableExtension_t   *Header;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceScalableExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoSequenceScalableExtension_t));

    Header->scalable_mode                               = Bits.Get(2);
    Header->layer_id                                    = Bits.Get(4);

    if (Header->scalable_mode == MPEG2_SCALABLE_MODE_SPATIAL_SCALABILITY)
    {
        Header->lower_layer_prediction_horizontal_size  = Bits.Get(14);
        MarkerBit(1);
        Header->lower_layer_prediction_vertical_size    = Bits.Get(14);
        Header->horizontal_subsampling_factor_m         = Bits.Get(5);
        Header->horizontal_subsampling_factor_n         = Bits.Get(5);
        Header->vertical_subsampling_factor_m           = Bits.Get(5);
        Header->vertical_subsampling_factor_n           = Bits.Get(5);
    }
    else if (Header->scalable_mode == MPEG2_SCALABLE_MODE_TEMPORAL_SCALABILITY)
    {
        Header->picture_mux_enable                      = Bits.Get(1);

        if (Header->picture_mux_enable)
        {
            Header->mux_to_progressive_sequence         = Bits.Get(1);
            Header->picture_mux_order                   = Bits.Get(3);
            Header->picture_mux_factor                  = Bits.Get(3);
        }
    }

    StreamParameters->SequenceScalableExtensionHeaderPresent    = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Sequence Scalable Extension header :-\n");
    SE_INFO(group_frameparser_video,  "    scalable_mode                : %6d\n", Header->scalable_mode);
    SE_INFO(group_frameparser_video,  "    layer_id                     : %6d\n", Header->layer_id);

    if (Header->scalable_mode == MPEG2_SCALABLE_MODE_SPATIAL_SCALABILITY)
    {
        SE_INFO(group_frameparser_video,  "    lower_layer_prediction_horizontal_size : %6d\n", Header->lower_layer_prediction_horizontal_size);
        SE_INFO(group_frameparser_video,  "    lower_layer_prediction_vertical_size   : %6d\n", Header->lower_layer_prediction_vertical_size);
        SE_INFO(group_frameparser_video,  "    horizontal_subsampling_factor_m        : %6d\n", Header->horizontal_subsampling_factor_m);
        SE_INFO(group_frameparser_video,  "    horizontal_subsampling_factor_n        : %6d\n", Header->horizontal_subsampling_factor_n);
        SE_INFO(group_frameparser_video,  "    vertical_subsampling_factor_m          : %6d\n", Header->vertical_subsampling_factor_m);
        SE_INFO(group_frameparser_video,  "    vertical_subsampling_factor_n          : %6d\n", Header->vertical_subsampling_factor_n);
    }
    else if (Header->scalable_mode == MPEG2_SCALABLE_MODE_TEMPORAL_SCALABILITY)
    {
        SE_INFO(group_frameparser_video,  "    picture_mux_enable           : %6d\n", Header->picture_mux_enable);

        if (Header->picture_mux_enable)
        {
            SE_INFO(group_frameparser_video,  "    mux_to_progressive_sequence  : %6d\n", Header->mux_to_progressive_sequence);
            SE_INFO(group_frameparser_video,  "    picture_mux_order            : %6d\n", Header->picture_mux_order);
            SE_INFO(group_frameparser_video,  "    picture_mux_factor           : %6d\n", Header->picture_mux_factor);
        }
    }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a group of pictures header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadGroupOfPicturesHeader(void)
{
    Mpeg2VideoGOP_t         *Header;

    if ((StreamParameters == NULL) || (StreamParameters->SequenceHeaderPresent == false))
    {
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->GroupOfPicturesHeader;
    memset(Header, 0, sizeof(Mpeg2VideoGOP_t));

    Header->time_code.drop_frame_flag   = Bits.Get(1);
    Header->time_code.hours             = Bits.Get(5);
    Header->time_code.minutes           = Bits.Get(6);
    MarkerBit(1);
    Header->time_code.seconds           = Bits.Get(6);
    Header->time_code.pictures          = Bits.Get(6);
    Header->closed_gop                  = Bits.Get(1);
    Header->broken_link                 = Bits.Get(1);

    StreamParameters->GroupOfPicturesHeaderPresent       = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Gop header :-\n");
    SE_INFO(group_frameparser_video,  "    time_code.drop_frame_flag    : %6d\n", Header->time_code.drop_frame_flag);
    SE_INFO(group_frameparser_video,  "    time_code.hours              : %6d\n", Header->time_code.hours);
    SE_INFO(group_frameparser_video,  "    time_code.minutes            : %6d\n", Header->time_code.minutes);
    SE_INFO(group_frameparser_video,  "    time_code.seconds            : %6d\n", Header->time_code.seconds);
    SE_INFO(group_frameparser_video,  "    time_code.pictures           : %6d\n", Header->time_code.pictures);
    SE_INFO(group_frameparser_video,  "    closed_gop                   : %6d\n", Header->closed_gop);
    SE_INFO(group_frameparser_video,  "    broken_link                  : %6d\n", Header->broken_link);
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureHeader(void)
{
    FrameParserStatus_t      Status;
    Mpeg2VideoPicture_t     *Header;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header      = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(Mpeg2VideoPicture_t));

    Header->temporal_reference                  = Bits.Get(10);
    Header->picture_coding_type                 = Bits.Get(3);
    Header->vbv_delay                           = Bits.Get(16);

    if (Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_P)
    {
        Header->full_pel_forward_vector         = Bits.Get(1);
        Header->forward_f_code                  = Bits.Get(3);
    }
    else if (Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B)
    {
        Header->full_pel_forward_vector         = Bits.Get(1);
        Header->forward_f_code                  = Bits.Get(3);
        Header->full_pel_backward_vector        = Bits.Get(1);
        Header->backward_f_code                 = Bits.Get(3);
    }

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Picture header :-\n");
    SE_INFO(group_frameparser_video,  "    temporal_reference           : %6d\n", Header->temporal_reference);
    SE_INFO(group_frameparser_video,  "    picture_coding_type          : %6d\n", Header->picture_coding_type);
    SE_INFO(group_frameparser_video,  "    vbv_delay                    : %6d\n", Header->vbv_delay);

    if ((Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_P) ||
        (Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B))
    {
        SE_INFO(group_frameparser_video,  "    full_pel_forward_vector      : %6d\n", Header->full_pel_forward_vector);
        SE_INFO(group_frameparser_video,  "    forward_f_code               : %6d\n", Header->forward_f_code);
    }

    if (Header->picture_coding_type == MPEG2_PICTURE_CODING_TYPE_B)
    {
        SE_INFO(group_frameparser_video,  "    full_pel_backward_vector     : %6d\n", Header->full_pel_backward_vector);
        SE_INFO(group_frameparser_video,  "    backward_f_code              : %6d\n", Header->backward_f_code);
    }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture coding extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureCodingExtensionHeader(void)
{
    Mpeg2VideoPictureCodingExtension_t      *Header;

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Appropriate picture header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureCodingExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoPictureCodingExtension_t));

    Header->f_code[0][0]                = Bits.Get(4);
    Header->f_code[0][1]                = Bits.Get(4);
    Header->f_code[1][0]                = Bits.Get(4);
    Header->f_code[1][1]                = Bits.Get(4);
    Header->intra_dc_precision          = Bits.Get(2);
    Header->picture_structure           = Bits.Get(2);
    Header->top_field_first             = Bits.Get(1);
    Header->frame_pred_frame_dct        = Bits.Get(1);
    Header->concealment_motion_vectors  = Bits.Get(1);
    Header->q_scale_type                = Bits.Get(1);
    Header->intra_vlc_format            = Bits.Get(1);
    Header->alternate_scan              = Bits.Get(1);
    Header->repeat_first_field          = Bits.Get(1);
    Header->chroma_420_type             = Bits.Get(1);
    Header->progressive_frame           = Bits.Get(1);
    Header->composite_display_flag      = Bits.Get(1);

    if (Header->composite_display_flag)
    {
        Header->v_axis                      = Bits.Get(1);
        Header->field_sequence              = Bits.Get(3);
        Header->sub_carrier                 = Bits.Get(1);
        Header->burst_amplitude             = Bits.Get(7);
        Header->sub_carrier_phase           = Bits.Get(8);
    }

    FrameParameters->PictureCodingExtensionHeaderPresent        = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Picture Coding Extension header :-\n");
    SE_INFO(group_frameparser_video,  "    f_code                       : %6d %6d %6d %6d\n", Header->f_code[0][0],
            Header->f_code[0][1],
            Header->f_code[1][0],
            Header->f_code[1][1]);
    SE_INFO(group_frameparser_video,  "    intra_dc_precision           : %6d\n", Header->intra_dc_precision);
    SE_INFO(group_frameparser_video,  "    picture_structure            : %6d\n", Header->picture_structure);
    SE_INFO(group_frameparser_video,  "    top_field_first              : %6d\n", Header->top_field_first);
    SE_INFO(group_frameparser_video,  "    frame_pred_frame_dct         : %6d\n", Header->frame_pred_frame_dct);
    SE_INFO(group_frameparser_video,  "    concealment_motion_vectors   : %6d\n", Header->concealment_motion_vectors);
    SE_INFO(group_frameparser_video,  "    q_scale_type                 : %6d\n", Header->q_scale_type);
    SE_INFO(group_frameparser_video,  "    intra_vlc_format             : %6d\n", Header->intra_vlc_format);
    SE_INFO(group_frameparser_video,  "    alternate_scan               : %6d\n", Header->alternate_scan);
    SE_INFO(group_frameparser_video,  "    repeat_first_field           : %6d\n", Header->repeat_first_field);
    SE_INFO(group_frameparser_video,  "    chroma_420_type              : %6d\n", Header->chroma_420_type);
    SE_INFO(group_frameparser_video,  "    progressive_frame            : %6d\n", Header->progressive_frame);
    SE_INFO(group_frameparser_video,  "    composite_display_flag       : %6d\n", Header->composite_display_flag);

    if (Header->composite_display_flag)
    {
        SE_INFO(group_frameparser_video,  "    v_axis                       : %6d\n", Header->v_axis);
        SE_INFO(group_frameparser_video,  "    field_sequence               : %6d\n", Header->field_sequence);
        SE_INFO(group_frameparser_video,  "    sub_carrier                  : %6d\n", Header->sub_carrier);
        SE_INFO(group_frameparser_video,  "    burst_amplitude              : %6d\n", Header->burst_amplitude);
        SE_INFO(group_frameparser_video,  "    sub_carrier_phase            : %6d\n", Header->sub_carrier_phase);
    }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a Quantization matrix extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadQuantMatrixExtensionHeader(void)
{
    unsigned int                             i;
    Mpeg2VideoQuantMatrixExtension_t        *Header;
    Mpeg2VideoQuantMatrixExtension_t        *CumulativeHeader;

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    StreamParameters->UpdatedSinceLastFrame     = true;
    Header      = &StreamParameters->QuantMatrixExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoQuantMatrixExtension_t));

    Header->load_intra_quantizer_matrix                                                 = Bits.Get(1);

    if (Header->load_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]           = Bits.Get(8);
        }

    Header->load_non_intra_quantizer_matrix                                             = Bits.Get(1);

    if (Header->load_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]       = Bits.Get(8);
        }

    Header->load_chroma_intra_quantizer_matrix                                          = Bits.Get(1);

    if (Header->load_chroma_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->chroma_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]]    = Bits.Get(8);
        }

    Header->load_chroma_non_intra_quantizer_matrix                                      = Bits.Get(1);

    if (Header->load_chroma_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i++)
        {
            Header->chroma_non_intra_quantizer_matrix[QuantizationMatrixNaturalOrder[i]] = Bits.Get(8);
        }

    CumulativeHeader    = &StreamParameters->CumulativeQuantMatrices;

    if (Header->load_intra_quantizer_matrix)
    {
        CumulativeHeader->load_intra_quantizer_matrix           = Header->load_intra_quantizer_matrix;
        memcpy(CumulativeHeader->intra_quantizer_matrix, Header->intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    if (Header->load_non_intra_quantizer_matrix)
    {
        CumulativeHeader->load_non_intra_quantizer_matrix       = Header->load_non_intra_quantizer_matrix;
        memcpy(CumulativeHeader->non_intra_quantizer_matrix, Header->non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    if (Header->load_chroma_intra_quantizer_matrix)
    {
        CumulativeHeader->load_chroma_intra_quantizer_matrix    = Header->load_chroma_intra_quantizer_matrix;
        memcpy(CumulativeHeader->chroma_intra_quantizer_matrix, Header->chroma_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    if (Header->load_chroma_non_intra_quantizer_matrix)
    {
        CumulativeHeader->load_chroma_non_intra_quantizer_matrix = Header->load_chroma_non_intra_quantizer_matrix;
        memcpy(CumulativeHeader->chroma_non_intra_quantizer_matrix, Header->chroma_non_intra_quantizer_matrix, sizeof(QuantiserMatrix_t));
    }

    StreamParameters->QuantMatrixExtensionHeaderPresent = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Quant Matrix Extension header :-\n");
    SE_INFO(group_frameparser_video,  "    load_intra_quantizer_matrix  : %6d\n", Header->load_intra_quantizer_matrix);

    if (Header->load_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

    SE_INFO(group_frameparser_video,  "    load_non_intra_quantizer_matrix  : %6d\n", Header->load_non_intra_quantizer_matrix);

    if (Header->load_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->non_intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

    SE_INFO(group_frameparser_video,  "    load_chroma_intra_quantizer_matrix  : %6d\n", Header->load_chroma_intra_quantizer_matrix);

    if (Header->load_chroma_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->chroma_intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

    SE_INFO(group_frameparser_video,  "    load_chroma_non_intra_quantizer_matrix  : %6d\n", Header->load_chroma_non_intra_quantizer_matrix);

    if (Header->load_chroma_non_intra_quantizer_matrix)
        for (i = 0; i < 64; i += 16)
        {
            int j;
            SE_INFO(group_frameparser_video,  "            ");

            for (j = 0; j < 16; j++)
            {
                SE_INFO(group_frameparser_video,  "%02x ", Header->chroma_non_intra_quantizer_matrix[i + j]);
            }

            SE_INFO(group_frameparser_video,  "\n");
        }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture display extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureDisplayExtensionHeader(void)
{
    unsigned int                             i;
    unsigned int                             NumberOfOffsets;
    Mpeg2VideoPictureDisplayExtension_t     *Header;
    bool                                     ProgressiveSequence;
    bool                                     Frame;
    bool                                     RepeatFirstField;
    bool                                     TopFieldFirst;

    if ((StreamParameters == NULL) || (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Appropriate picture header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureDisplayExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoPictureDisplayExtension_t));

    ProgressiveSequence = !StreamParameters->SequenceExtensionHeaderPresent ||
                          StreamParameters->SequenceExtensionHeader.progressive_sequence;
    Frame               = !FrameParameters->PictureCodingExtensionHeaderPresent ||
                          (FrameParameters->PictureCodingExtensionHeader.picture_structure == MPEG2_PICTURE_STRUCTURE_FRAME);
    RepeatFirstField    = FrameParameters->PictureCodingExtensionHeaderPresent &&
                          FrameParameters->PictureCodingExtensionHeader.repeat_first_field;
    TopFieldFirst       = FrameParameters->PictureCodingExtensionHeaderPresent &&
                          FrameParameters->PictureCodingExtensionHeader.top_field_first;

    if (!Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField))
    {
        SE_ERROR("Illegal combination of progressive_sequence, progressive_frame,  top_field_first and repeat_first_field(%c %c %c %c)\n",
                 (ProgressiveSequence    ? 'T' : 'F'),
                 (Frame                  ? 'T' : 'F'),
                 (TopFieldFirst          ? 'T' : 'F'),
                 (RepeatFirstField       ? 'T' : 'F'));
        return FrameParserHeaderSyntaxError;
    }

    NumberOfOffsets     = PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

    for (i = 0; i < NumberOfOffsets; i++)
    {
        Header->frame_centre[i].horizontal_offset       = Bits.SignedGet(16);
        MarkerBit(1);
        Header->frame_centre[i].vertical_offset         = Bits.SignedGet(16);
        MarkerBit(1)
    }

    FrameParameters->PictureDisplayExtensionHeaderPresent       = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Picture Display Extension header   :-\n");

    for (i = 0; i < NumberOfOffsets; i++)
    {
        SE_INFO(group_frameparser_video,  "    frame_centre[%d].horizontal_offset       : %6d\n", i, Header->frame_centre[i].horizontal_offset);
        SE_INFO(group_frameparser_video,  "    frame_centre[%d].vertical_offset         : %6d\n", i, Header->frame_centre[i].vertical_offset);
    }

#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture temporal scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureTemporalScalableExtensionHeader(void)
{
    Mpeg2VideoPictureTemporalScalableExtension_t    *Header;

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Appropriate picture header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureTemporalScalableExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoPictureTemporalScalableExtension_t));

    Header->reference_select_code               = Bits.Get(2);
    Header->forward_temporal_reference          = Bits.Get(10);
    MarkerBit(1);
    Header->backward_temporal_reference         = Bits.Get(10);

    FrameParameters->PictureTemporalScalableExtensionHeaderPresent      = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Picture Temporal Scalable Extension header   :-\n");
    SE_INFO(group_frameparser_video,  "    reference_select_code                    : %6d\n", Header->reference_select_code);
    SE_INFO(group_frameparser_video,  "    forward_temporal_reference               : %6d\n", Header->forward_temporal_reference);
    SE_INFO(group_frameparser_video,  "    backward_temporal_reference              : %6d\n", Header->backward_temporal_reference);
#endif
    return FrameParserNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture spatial scalable extension header
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::ReadPictureSpatialScalableExtensionHeader(void)
{
    Mpeg2VideoPictureSpatialScalableExtension_t     *Header;

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Appropriate picture header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &FrameParameters->PictureSpatialScalableExtensionHeader;
    memset(Header, 0, sizeof(Mpeg2VideoPictureSpatialScalableExtension_t));

    Header->lower_layer_temporal_reference              = Bits.Get(10);
    MarkerBit(1);
    Header->lower_layer_horizontal_offset               = Bits.Get(15);
    MarkerBit(1);
    Header->lower_layer_vertical_offset                 = Bits.Get(15);
    Header->spatial_temporal_weight_code_table_index    = Bits.Get(2);
    Header->lower_layer_progressive_frame               = Bits.Get(1);
    Header->lower_layer_deinterlaced_field_select       = Bits.Get(1);

    FrameParameters->PictureSpatialScalableExtensionHeaderPresent       = true;

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Picture Spatial Scalable Extension header   :-\n");
    SE_INFO(group_frameparser_video,  "    lower_layer_temporal_reference          : %6d\n", Header->lower_layer_temporal_reference);
    SE_INFO(group_frameparser_video,  "    lower_layer_horizontal_offset           : %6d\n", Header->lower_layer_horizontal_offset);
    SE_INFO(group_frameparser_video,  "    lower_layer_vertical_offset             : %6d\n", Header->lower_layer_vertical_offset);
    SE_INFO(group_frameparser_video,  "    spatial_temporal_weight_code_table_index: %6d\n", Header->spatial_temporal_weight_code_table_index);
    SE_INFO(group_frameparser_video,  "    lower_layer_progressive_frame           : %6d\n", Header->lower_layer_progressive_frame);
    SE_INFO(group_frameparser_video,  "    lower_layer_deinterlaced_field_select   : %6d\n", Header->lower_layer_deinterlaced_field_select);
#endif
    return FrameParserNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      On a first slice code, we should have garnered all the data
//      we require we for a frame decode, this function records that fact.
//

FrameParserStatus_t   FrameParser_VideoMpeg2_c::CommitFrameForDecode(void)
{
    unsigned int             i;
    bool                     ProgressiveSequence;
    bool                     FieldSequenceError;
    PictureStructure_t       PictureStructure;
    bool                     Frame;
    bool                     RepeatFirstField;
    bool                     TopFieldFirst;
    unsigned int             PanAndScanCount;
    unsigned int             Height, Width;
    SliceType_t              SliceType;
    MatrixCoefficientsType_t MatrixCoefficients;
    FrameParserStatus_t      Status = FrameParserNoError;

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

    //
    // Check we have the headers we need
    //

    if ((StreamParameters == NULL) ||
        !StreamParameters->SequenceHeaderPresent ||
        ((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !StreamParameters->SequenceExtensionHeaderPresent))
    {
        SE_ERROR("Stream parameters unavailable for decode\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) ||
        !FrameParameters->PictureHeaderPresent ||
        ((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !FrameParameters->PictureCodingExtensionHeaderPresent))
    {
        SE_ERROR("Frame parameters unavailable for decode. %p\n",
                 FrameParameters);

        if (FrameParameters != NULL)
        {
            SE_INFO(group_frameparser_video,  "        %d %d\n",
                    !FrameParameters->PictureHeaderPresent,
                    ((StreamParameters->StreamType == MpegStreamTypeMpeg2) && !FrameParameters->PictureCodingExtensionHeaderPresent));
        }

        Stream->MarkUnPlayable();
        return FrameParserPartialFrameParameters;
    }

    //
    // Check that the aspect ratio code is valid for the specific standard
    //

    if ((StreamParameters->StreamType == MpegStreamTypeMpeg2) ?
        !inrange(StreamParameters->SequenceHeader.aspect_ratio_information, MIN_LEGAL_MPEG2_ASPECT_RATIO_CODE, MAX_LEGAL_MPEG2_ASPECT_RATIO_CODE) :
        !inrange(StreamParameters->SequenceHeader.aspect_ratio_information, MIN_LEGAL_MPEG1_ASPECT_RATIO_CODE, MAX_LEGAL_MPEG1_ASPECT_RATIO_CODE))
    {
        SE_ERROR("aspect_ratio_information has illegal value (%02x) for mpeg standard\n", StreamParameters->SequenceHeader.aspect_ratio_information);
        return FrameParserHeaderSyntaxError;
    }

    //
    // Check that the constrained parameters flag is valid for the specific standard
    //

    if ((StreamParameters->StreamType == MpegStreamTypeMpeg2) &&
        (StreamParameters->SequenceHeader.constrained_parameters_flag != 0))
    {
        SE_ERROR("constrained_parameters_flag has illegal value (%02x) for mpeg2 standard\n", StreamParameters->SequenceHeader.constrained_parameters_flag);
        return FrameParserHeaderSyntaxError;
    }

    //
    // Obtain and check the progressive etc values.
    //
    ProgressiveSequence = !StreamParameters->SequenceExtensionHeaderPresent ||
                          StreamParameters->SequenceExtensionHeader.progressive_sequence;

    //
    // Specialist code for broadcast streams height 720 without ProgressiveSequence flag set (usually 720 means 720P)
    //
    if (StreamParameters->SequenceHeader.vertical_size_value == 720)
    {
        ProgressiveSequence = true;
    }

    PictureStructure    = FrameParameters->PictureCodingExtensionHeaderPresent ?
                          PictureStructures[FrameParameters->PictureCodingExtensionHeader.picture_structure] :
                          StructureFrame;

    SliceType           = SliceTypeTranslation[FrameParameters->PictureHeader.picture_coding_type];
    Frame               = PictureStructure == StructureFrame;
    RepeatFirstField    = FrameParameters->PictureCodingExtensionHeaderPresent &&
                          FrameParameters->PictureCodingExtensionHeader.repeat_first_field;
    TopFieldFirst       = FrameParameters->PictureCodingExtensionHeaderPresent &&
                          FrameParameters->PictureCodingExtensionHeader.top_field_first;
    PanAndScanCount     = FrameParameters->PictureDisplayExtensionHeaderPresent ?
                          PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField) :
                          0;

    // case if we have user data : set top field first flag
    if (ParsedFrameParameters->UserDataNumber > 0)
    {
        for (unsigned int i = 0; i < ParsedFrameParameters->UserDataNumber ; i++)
        {
            AccumulatedUserData[i].UserDataAdditionalParameters.CodecUserDataParameters.MPEG2UserDataParameters.TopFieldFirst = TopFieldFirst;
        }
    }

    if (!Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField))
    {
        SE_ERROR("Illegal combination of progressive_sequence, progressive_frame, repeat_first_field and top_field_first (%c %c %c %c)\n",
                 (ProgressiveSequence    ? 'T' : 'F'),
                 (Frame                  ? 'T' : 'F'),
                 (RepeatFirstField       ? 'T' : 'F'),
                 (TopFieldFirst          ? 'T' : 'F'));
        return FrameParserHeaderSyntaxError;
    }

    // Reset of FirstDecodeOfFrame = 0. AccumulatedPictureStructure stores value of previous PictureStructure
    // For Frame: FDF=1, FSE=0, APS=3
    // For Field: FDF=1(for first field only), FSE=0 (unless APS == PS), APS = previous PictureStructure
    if (Frame)
    {
        FieldSequenceError = false;
        FirstDecodeOfFrame = true;
    }
    else
    {
        FieldSequenceError = (AccumulatedPictureStructure == PictureStructure);
        FirstDecodeOfFrame = (AccumulatedPictureStructure == StructureFrame) ? false : FirstDecodeOfFrame;
        FirstDecodeOfFrame = (FirstDecodeOfFrame == FieldSequenceError);
    }
    AccumulatedPictureStructure = PictureStructure;

    SE_DEBUG(group_decoder_video, "FDF=%d, PS=%d, APS=%d, Field=%d, FSE=%d\n",
             FirstDecodeOfFrame, PictureStructure, AccumulatedPictureStructure, !Frame, FieldSequenceError);

    if (FieldSequenceError)
    {
        if (!FirstDecodeOfFrame)
        {
            SE_WARNING("Field sequence error detected, First Field Missing\n");
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
        }
        else
        {
            SE_WARNING("Field sequence error : Second Field Missing\n");
        }
    }

    //
    // For a field decode we need to recalculate top field first,
    // as this is always set false for a field picture.
    //

    if (!Frame)
    {
        TopFieldFirst   = (FirstDecodeOfFrame == (PictureStructure == StructureTopField));
    }

    //
    // Deduce the matrix coefficients for colour conversions
    //

    if (StreamParameters->StreamType == MpegStreamTypeMpeg1)
    {
        MatrixCoefficients      = MatrixCoefficients_ITU_R_BT601;
    }
    else if (StreamParameters->SequenceDisplayExtensionHeaderPresent &&
             StreamParameters->SequenceDisplayExtensionHeader.color_description)
    {
        switch (StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients)
        {
        case MPEG2_MATRIX_COEFFICIENTS_BT709:       MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;       break;

        case MPEG2_MATRIX_COEFFICIENTS_FCC:         MatrixCoefficients      = MatrixCoefficients_FCC;               break;

        case MPEG2_MATRIX_COEFFICIENTS_BT470_BGI:   MatrixCoefficients      = MatrixCoefficients_ITU_R_BT470_2_BG;  break;

        case MPEG2_MATRIX_COEFFICIENTS_SMPTE_170M:  MatrixCoefficients      = MatrixCoefficients_SMPTE_170M;        break;

        case MPEG2_MATRIX_COEFFICIENTS_SMPTE_240M:  MatrixCoefficients      = MatrixCoefficients_SMPTE_240M;        break;

        default:
        case MPEG2_MATRIX_COEFFICIENTS_FORBIDDEN:
        case MPEG2_MATRIX_COEFFICIENTS_RESERVED:
            SE_ERROR("Forbidden or reserved matrix coefficient code specified (%02x)\n", StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients);

        // fall through

        case MPEG2_MATRIX_COEFFICIENTS_UNSPECIFIED: MatrixCoefficients      = MatrixCoefficients_Undefined;         break;
        }
    }
    else
    {
        int Policy = Player->PolicyValue(Playback, Stream, PolicyMPEG2ApplicationType);

        switch (Policy)
        {
        case PolicyValueMPEG2ApplicationMpeg2:
        case PolicyValueMPEG2ApplicationAtsc:       MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;
            break;

        default:
        case PolicyValueMPEG2ApplicationDvb:        if (StreamParameters->SequenceHeader.horizontal_size_value > 720)
            {
                MatrixCoefficients  = MatrixCoefficients_ITU_R_BT709;
            }
            else if (FrameRates(StreamParameters->SequenceHeader.frame_rate_code) < 28)
            {
                MatrixCoefficients  = MatrixCoefficients_ITU_R_BT470_2_BG;
            }
            else
            {
                MatrixCoefficients  = MatrixCoefficients_SMPTE_170M;
            }

            break;
        }
    }

    //
    // Record the stream and frame parameters into the appropriate structure
    //
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                                     = SliceType == SliceTypeI;
    ParsedFrameParameters->ReferenceFrame                               = SliceType != SliceTypeB;
    ParsedFrameParameters->IndependentFrame                             = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->StillPicture                                 = (ParsedFrameParameters->IndependentFrame &&
                                                                           (FrameParameters->PictureHeader.temporal_reference == 0));
    ParsedFrameParameters->NewStreamParameters                          = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure               = sizeof(Mpeg2StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure                     = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                           = true;
    ParsedFrameParameters->SizeofFrameParameterStructure                = sizeof(Mpeg2FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure                      = FrameParameters;

    ParsedVideoParameters->Content.DecodeWidth                          = StreamParameters->SequenceHeader.horizontal_size_value;
    ParsedVideoParameters->Content.DecodeHeight                         = StreamParameters->SequenceHeader.vertical_size_value;
    ParsedVideoParameters->Content.Width                                = StreamParameters->SequenceHeader.horizontal_size_value;
    ParsedVideoParameters->Content.Height                               = StreamParameters->SequenceHeader.vertical_size_value;

    //Deal with 1088 lines encoded streams which must be displayed in 1080 lines
    if (ParsedVideoParameters->Content.Height > 1080)
    {
        ParsedVideoParameters->Content.Height = 1080;
    }

    ParsedVideoParameters->Content.DisplayWidth                         = ParsedVideoParameters->Content.Width;
    ParsedVideoParameters->Content.DisplayHeight                        = ParsedVideoParameters->Content.Height;

    if (StreamParameters->SequenceDisplayExtensionHeaderPresent)
    {
        ParsedVideoParameters->Content.DisplayWidth                     = StreamParameters->SequenceDisplayExtensionHeader.display_horizontal_size;
        ParsedVideoParameters->Content.DisplayHeight                    = StreamParameters->SequenceDisplayExtensionHeader.display_vertical_size;
    }

    // Center the display window
    if (ParsedVideoParameters->Content.DisplayWidth > ParsedVideoParameters->Content.Width)
    {
        ParsedVideoParameters->Content.DisplayWidth                     = ParsedVideoParameters->Content.Width;
    }

    if (ParsedVideoParameters->Content.DisplayHeight > ParsedVideoParameters->Content.Height)
    {
        ParsedVideoParameters->Content.DisplayHeight                    = ParsedVideoParameters->Content.Height;
    }

    ParsedVideoParameters->Content.DisplayX                         = (ParsedVideoParameters->Content.Width - ParsedVideoParameters->Content.DisplayWidth) / 2;
    ParsedVideoParameters->Content.DisplayY                         = (ParsedVideoParameters->Content.Height - ParsedVideoParameters->Content.DisplayHeight) / 2;


    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.DecodeWidth, ParsedVideoParameters->Content.DecodeHeight);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.DecodeWidth, ParsedVideoParameters->Content.DecodeHeight);
        return Status;
    }

    ParsedVideoParameters->Content.Progressive                          = ProgressiveSequence;
    ParsedVideoParameters->Content.OverscanAppropriate                  = false;
    ParsedVideoParameters->Content.PixelAspectRatio                     = (StreamParameters->StreamType == MpegStreamTypeMpeg2) ?
                                                                          Mpeg2AspectRatios(StreamParameters->SequenceHeader.aspect_ratio_information) :
                                                                          Mpeg1AspectRatios(StreamParameters->SequenceHeader.aspect_ratio_information);
    StreamEncodedFrameRate                      = FrameRates(StreamParameters->SequenceHeader.frame_rate_code);
    ParsedVideoParameters->Content.FrameRate                            = ResolveFrameRate();
    ParsedVideoParameters->Content.VideoFullRange                       = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients             = MatrixCoefficients;
    ParsedVideoParameters->SliceType                                    = SliceType;
    ParsedVideoParameters->PictureStructure                             = PictureStructure;

    if (FrameParameters->PictureCodingExtensionHeaderPresent)
    {
        ParsedVideoParameters->InterlacedFrame                              = (!ProgressiveSequence) ? (FrameParameters->PictureCodingExtensionHeader.progressive_frame == 0) : false;
    }
    else
    {
        ParsedVideoParameters->InterlacedFrame                              = !ProgressiveSequence;
    }

    ParsedVideoParameters->TopFieldFirst                                = !FrameParameters->PictureCodingExtensionHeaderPresent || TopFieldFirst;
    ParsedVideoParameters->DisplayCount[0]                              = DisplayCount0(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    ParsedVideoParameters->DisplayCount[1]                              = DisplayCount1(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    //
    // Specialist code for broadcast streams that do not get progressive_frame right
    //
    // What we do is treat every frame as interlaced, except those which we have
    // good reason not to. If for example we have seen a repeat first field in the
    // stream then we suspect that some sort of pulldown (possibly 3:2) may be in effect.
    //
    int Policy   = Player->PolicyValue(Playback, Stream, PolicyMPEG2DoNotHonourProgressiveFrameFlag);

    if ((Policy == PolicyValueApply) && (!ProgressiveSequence))
    {
        if (RepeatFirstField)
        {
            EverSeenRepeatFirstField                                    = true;
        }

        if (!EverSeenRepeatFirstField)
        {
            ParsedVideoParameters->InterlacedFrame                      = true;
        }
        else
        {
            ParsedVideoParameters->InterlacedFrame                      = false;
        }
    }

    //
    // Now insert the pan and scan counts if we have them,
    // alternatively repeat the last value for the appropriate period
    //

    if (PanAndScanCount != 0)
    {
        ParsedVideoParameters->PanScanCount                     = PanAndScanCount;

        for (i = 0; i < PanAndScanCount; i++)
        {
            // The dimensions of the pan-scan display rectangle are extracted from the Sequence Display Extension header
            ParsedVideoParameters->PanScan[i].Width             = ParsedVideoParameters->Content.DisplayWidth;
            ParsedVideoParameters->PanScan[i].Height            = ParsedVideoParameters->Content.DisplayHeight;

            // The frame_centre offsets are 16-bit signed integers, giving the position of the centre of the display rectangle (in units of 1/16th sample),
            // compared to the centre of the reconstructed frame. A positive value indicates that the centre of the reconstructed frame lies to the right
            // of the centre of the display rectangle.
            ParsedVideoParameters->PanScan[i].HorizontalOffset  = 16 * (ParsedVideoParameters->Content.Width - ParsedVideoParameters->Content.DisplayWidth) / 2 -
                                                                  FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].horizontal_offset;
            ParsedVideoParameters->PanScan[i].VerticalOffset    = 16 * (ParsedVideoParameters->Content.Height - ParsedVideoParameters->Content.DisplayHeight) / 2 -
                                                                  FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].vertical_offset;

            // DisplayCount is always 1 since there are as much pan-scan data as fields to be rendered
            ParsedVideoParameters->PanScan[i].DisplayCount      = 1;
        }

        LastPanScanHorizontalOffset = ParsedVideoParameters->PanScan[PanAndScanCount - 1].HorizontalOffset;
        LastPanScanVerticalOffset   = ParsedVideoParameters->PanScan[PanAndScanCount - 1].VerticalOffset;
    }
    else
    {
        // Repeat the last parsed pan-scan data
        ParsedVideoParameters->PanScanCount                 = 1;
        ParsedVideoParameters->PanScan[0].DisplayCount      = ParsedVideoParameters->DisplayCount[0] + ParsedVideoParameters->DisplayCount[1];
        ParsedVideoParameters->PanScan[0].Width             = ParsedVideoParameters->Content.DisplayWidth;
        ParsedVideoParameters->PanScan[0].Height            = ParsedVideoParameters->Content.DisplayHeight;
        ParsedVideoParameters->PanScan[0].HorizontalOffset  = LastPanScanHorizontalOffset;
        ParsedVideoParameters->PanScan[0].VerticalOffset    = LastPanScanVerticalOffset;
    }

    //
    // If we have a Display aspect ratio, convert to a sample aspect ratio
    //

    if ((ParsedVideoParameters->Content.PixelAspectRatio != Rational_t(1, 1)) &&
        (StreamParameters->StreamType == MpegStreamTypeMpeg2) && (ParsedVideoParameters->Content.Width > 0))
    {
        // Aspect ratio convertion is managed by VIBE.
        Width                                   = ParsedVideoParameters->Content.Width;
        Height                                  = ParsedVideoParameters->Content.Height;
        // Convert to height/width similar to h264, vc1 etc
        ParsedVideoParameters->Content.PixelAspectRatio = (ParsedVideoParameters->Content.PixelAspectRatio * Height) / Width;
    }

    //
    // Record our claim on both the frame and stream parameters
    //
    Buffer->AttachBuffer(StreamParametersBuffer);
    Buffer->AttachBuffer(FrameParametersBuffer);
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


// /////////////////////////////////////////////////////////////////////////
//
//      Boolean function to evaluate whether or not the stream
//      parameters are new.
//

bool   FrameParser_VideoMpeg2_c::NewStreamParametersCheck(void)
{
    bool            Different;

    //
    // The parameters cannot be new if they have been used before.
    //

    if (!StreamParameters->UpdatedSinceLastFrame)
    {
        return false;
    }

    StreamParameters->UpdatedSinceLastFrame     = false;
    //
    // Check for difference using a straightforward comparison to see if the
    // stream parameters have changed. (since we zero on allocation simple
    // memcmp should be sufficient).
    //
    Different   = memcmp(&CopyOfStreamParameters, StreamParameters, sizeof(Mpeg2StreamParameters_t)) != 0;

    if (Different)
    {
        memcpy(&CopyOfStreamParameters, StreamParameters, sizeof(Mpeg2StreamParameters_t));
        return true;
    }

    return false;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialise the user data specific to MPEG2
//

bool FrameParser_VideoMpeg2_c::ReadAdditionalUserDataParameters(void)
{
    AccumulatedUserData[ParsedFrameParameters->UserDataNumber].UserDataAdditionalParameters.Available = true;
    AccumulatedUserData[ParsedFrameParameters->UserDataNumber].UserDataAdditionalParameters.Length = sizeof(MPEG2UserDataParameters_t);
    // set the codec ID
    AccumulatedUserData[ParsedFrameParameters->UserDataNumber].UserDataGenericParameters.CodecId = USER_DATA_MPEG2_CODEC_ID;
    return true;
}

FrameParserStatus_t FrameParser_VideoMpeg2_c::GetMpeg2TimeCode(stm_se_ctrl_mpeg2_time_code_t *TimeCode)
{
    Mpeg2VideoGOP_t *Header;

    if ((StreamParameters == NULL) || (StreamParameters->GroupOfPicturesHeaderPresent == false))
    {
        SE_ERROR("MPEG2 Time Code information not available\n");
        return FrameParserError;
    }

    Header = &StreamParameters->GroupOfPicturesHeader;
    TimeCode->time_code_hours      = Header->time_code.hours;
    TimeCode->time_code_minutes    = Header->time_code.minutes;
    TimeCode->time_code_seconds    = Header->time_code.seconds;
    TimeCode->time_code_pictures   = Header->time_code.pictures;
    return FrameParserNoError;
}

