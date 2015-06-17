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

#include "ring_generic.h"
#include "frame_parser_video_rmv.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoRmv_c"
//#define DUMP_HEADERS

static BufferDataDescriptor_t     RmvStreamParametersBuffer = BUFFER_RMV_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     RmvFrameParametersBuffer = BUFFER_RMV_FRAME_PARAMETERS_TYPE;

#define CountsIndex( PS, F, TFF, RFF)           ((((PS) != 0) << 3) | (((F) != 0) << 2) | (((TFF) != 0) << 1) | ((RFF) != 0))
#define Legal( PS, F, TFF, RFF)                 Counts[CountsIndex(PS, F, TFF, RFF)].LegalValue
#define PanScanCount( PS, F, TFF, RFF)         Counts[CountsIndex(PS, F, TFF, RFF)].PanScanCountValue
#define DisplayCount0( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount0Value
#define DisplayCount1( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount1Value

static const unsigned int ReferenceFramesRequired[RMV_PICTURE_CODING_TYPE_B + 1]    = {0, 2, 1, 2};
#define REFERENCE_FRAMES_NEEDED(CodingType)             ReferenceFramesRequired[CodingType]
//#define REFERENCE_FRAMES_NEEDED(CodingType)             CodingType

#define MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE         REFERENCE_FRAMES_NEEDED(RMV_PICTURE_CODING_TYPE_B)

static unsigned int PCT_SIZES[] = {0, 1, 1, 2, 2, 3, 3, 3, 3};

//
FrameParser_VideoRmv_c::FrameParser_VideoRmv_c(void)
    : FrameParser_Video_c()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , CopyOfStreamParameters()
    , StreamFormatInfoValid(false)
    , FrameRate()
    , LastTemporalReference(0)
    , TemporalReferenceBase(0)
    , FramePTS(0)
    , FrameNumber(0)
{
    Configuration.FrameParserName               = "VideoRmv";
    Configuration.StreamParametersCount         = 4;
    Configuration.StreamParametersDescriptor    = &RmvStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &RmvFrameParametersBuffer;

#if defined (RMV_RECALCULATE_FRAMERATE)
    InitialTemporalReference                    = 0;
#endif
}

//
FrameParser_VideoRmv_c::~FrameParser_VideoRmv_c(void)
{
    Halt();
}

//}}}
//{{{  Connect
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Connect the output port
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::Connect(Port_c *Port)
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
    return FrameParser_Video_c::Connect(Port);
}
//}}}

//{{{  ResetReferenceFrameList
// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoRmv_c::ResetReferenceFrameList(void)
{
    FrameParserStatus_t         Status  = FrameParser_Video_c::ResetReferenceFrameList();
    LastTemporalReference               = 0;
    TemporalReferenceBase               = 0;
    FrameNumber                         = 0;
#if defined (RMV_RECALCULATE_FRAMERATE)
    InitialTemporalReference            = 0;
#endif
    return Status;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::PrepareReferenceFrameList(void)
{
    unsigned int        ReferenceFramesNeeded;
    unsigned int        ReferenceFramesAvailable;
    unsigned int        PictureCodingType;
    RmvVideoPicture_t  *PictureHeader;
    unsigned int        i;
    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    //
    PictureHeader               = &(((RmvFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->PictureCodingType;
    ReferenceFramesNeeded       = REFERENCE_FRAMES_NEEDED(PictureCodingType);
    ReferenceFramesAvailable    = ReferenceFrameList.EntryCount;

    // Check for sufficient reference frames.  We cannot decode otherwise
    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
    {
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = 0;

    // For RMV we always fill in all of the reference frames.  This is done by copying previous ones
    // where necessary.  The most recent reference frame is inserted in slot 0 and the one before
    // that in slot 1. They are kept in the ReferenceFrameList array in the reverse order, however.
    if (ReferenceFramesAvailable > 0)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE;

        for (i = 0; i < MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE; i++)
        {
            if (ReferenceFramesAvailable > i)
            {
                ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFramesAvailable - i - 1];
            }
            else
            {
                ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i - 1];
            }
        }
    }

    //SE_INFO(group_frameparser_video,  "Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
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
FrameParserStatus_t   FrameParser_VideoRmv_c::ForPlayUpdateReferenceFrameList(void)
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
                Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                                          CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);
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
            Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE,
                                                      CodecFnReleaseReferenceFrame, ParsedFrameParameters->DecodeFrameIndex);
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

FrameParserStatus_t   FrameParser_VideoRmv_c::RevPlayProcessDecodeStacks(void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();
    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status;
    Bits.SetPointer(BufferData);

    if (!StreamFormatInfoValid)
    {
        Status          = ReadStreamFormatInfo();
    }
    else
    {
        Status          = ReadPictureHeader();

        if (Status == FrameParserNoError)
        {
            Status      = CommitFrameForDecode();
        }
    }

    return Status;
}
//}}}
//{{{  ReadStreamFormatInfo
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a FormatInfo structure
///
/// /////////////////////////////////////////////////////////////////////////

FrameParserStatus_t   FrameParser_VideoRmv_c::ReadStreamFormatInfo(void)
{
    FrameParserStatus_t         Status;
    RmvVideoSequence_t         *SequenceHeader;
    unsigned int                EncodedSize     = 0;
    unsigned int                i;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        SE_ERROR("Received Stream FormatInfo after previous sequence data\n");
        return FrameParserNoError;
    }

    Status      = GetNewStreamParameters((void **)&StreamParameters);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    StreamParameters->SequenceHeaderPresent     = true;
    StreamParameters->UpdatedSinceLastFrame     = true;
    SequenceHeader                              = &StreamParameters->SequenceHeader;
    memset(SequenceHeader, 0, sizeof(RmvVideoSequence_t));
    SequenceHeader->Length                      = Bits.Get(32);
    SequenceHeader->MOFTag                      = Bits.Get(32);
    SequenceHeader->SubMOFTag                   = Bits.Get(32);
    SequenceHeader->Width                       = Bits.Get(16);
    SequenceHeader->Height                      = Bits.Get(16);
    SequenceHeader->BitCount                    = Bits.Get(16);
    SequenceHeader->PadWidth                    = Bits.Get(16);
    SequenceHeader->PadHeight                   = Bits.Get(16);
    SequenceHeader->FramesPerSecond             = Bits.Get(32);
    SequenceHeader->OpaqueDataSize              = SequenceHeader->Length - RMV_FORMAT_INFO_HEADER_SIZE;

    if ((SequenceHeader->Width > RMV_MAX_SUPPORTED_DECODE_WIDTH) || (SequenceHeader->Height > RMV_MAX_SUPPORTED_DECODE_HEIGHT))
    {
        SE_ERROR("Content dimensions (%dx%d) greater than max supported (%dx%d)\n",
                 SequenceHeader->Width, SequenceHeader->Height,
                 RMV_MAX_SUPPORTED_DECODE_WIDTH, RMV_MAX_SUPPORTED_DECODE_HEIGHT);
        Stream->MarkUnPlayable();
        return FrameParserError;
    }

    FrameRate                                   = Rational_t (SequenceHeader->FramesPerSecond, 0x10000);
    SequenceHeader->RPRSize[0]                  = SequenceHeader->Width;
    SequenceHeader->RPRSize[1]                  = SequenceHeader->Height;
    SequenceHeader->NumRPRSizes                 = 0;

    if (SequenceHeader->OpaqueDataSize > 0)
    {
        unsigned int    BitstreamVersionData;
        SequenceHeader->SPOFlags                = Bits.Get(32);
        BitstreamVersionData                    = Bits.Get(32);
        SequenceHeader->BitstreamVersion        = (BitstreamVersionData & RV89_BITSTREAM_VERSION_BITS) >>
                                                  RV89_BITSTREAM_VERSION_SHIFT;
        SequenceHeader->BitstreamMinorVersion   = (BitstreamVersionData & RV89_BITSTREAM_MINOR_BITS) >>
                                                  RV89_BITSTREAM_MINOR_SHIFT;

        if ((SequenceHeader->BitstreamVersion == RV9_BITSTREAM_VERSION) &&
            ((SequenceHeader->BitstreamMinorVersion == RV9_BITSTREAM_MINOR_VERSION) ||
             (SequenceHeader->BitstreamMinorVersion == RV89_RAW_BITSTREAM_MINOR_VERSION)))
        {}
        else if ((SequenceHeader->BitstreamVersion == RV8_BITSTREAM_VERSION) &&
                 ((SequenceHeader->BitstreamMinorVersion == RV8_BITSTREAM_MINOR_VERSION) ||
                  (SequenceHeader->BitstreamMinorVersion == RV89_RAW_BITSTREAM_MINOR_VERSION)))
        {}
        else
        {
            SE_ERROR("Unsupported bitstream versions (%d, %d)\n",
                     SequenceHeader->BitstreamVersion, SequenceHeader->BitstreamMinorVersion);
            Stream->MarkUnPlayable();
            return FrameParserError;
        }

        if (SequenceHeader->BitstreamMinorVersion != RV89_RAW_BITSTREAM_MINOR_VERSION)
        {
            if ((SequenceHeader->BitstreamVersion == RV20_BITSTREAM_VERSION) ||
                (SequenceHeader->BitstreamVersion == RV30_BITSTREAM_VERSION))
            {
                SequenceHeader->NumRPRSizes     = (SequenceHeader->SPOFlags & RV20_SPO_NUMRESAMPLE_IMAGES_BITS) >>
                                                  RV20_SPO_NUMRESAMPLE_IMAGES_SHIFT;

                for (i = 2; i <= (SequenceHeader->NumRPRSizes * 2); i += 2)
                {
                    SequenceHeader->RPRSize[i]          = Bits.Get(8) << 2;
                    SequenceHeader->RPRSize[i + 1]        = Bits.Get(8) << 2;
                }
            }
            else if (SequenceHeader->BitstreamVersion == RV40_BITSTREAM_VERSION)
            {
                if (SequenceHeader->OpaqueDataSize >= 12)
                {
                    EncodedSize                 = Bits.Get(32);
                }
            }
        }
    }
    else
    {
        SE_ERROR("No opaque data present in format info\n");
        return FrameParserError;
    }

    SequenceHeader->NumRPRSizes++;
    SequenceHeader->MaxWidth                    = (EncodedSize & RV40_ENCODED_SIZE_WIDTH_BITS)  >> RV40_ENCODED_SIZE_WIDTH_SHIFT;
    SequenceHeader->MaxWidth                    = (EncodedSize & RV40_ENCODED_SIZE_HEIGHT_BITS) << RV40_ENCODED_SIZE_HEIGHT_SHIFT;

    if (SequenceHeader->MaxWidth == 0)
    {
        SequenceHeader->MaxWidth                = SequenceHeader->Width;
    }

    if (SequenceHeader->MaxHeight == 0)
    {
        SequenceHeader->MaxHeight               = SequenceHeader->Height;
    }

    // Max width and height must be multiples of 16 to be a whole number of macroblocks
    SequenceHeader->MaxWidth                    = (SequenceHeader->MaxWidth + 0x0f) & 0xfffffff0;
    SequenceHeader->MaxHeight                   = (SequenceHeader->MaxHeight + 0x0f) & 0xfffffff0;
    //if (SequenceHeader->MajorBitstreamVersion
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "StreamFormatInfo :-\n");
    SE_INFO(group_frameparser_video,  "Length                      %6u\n", SequenceHeader->Length);
    SE_INFO(group_frameparser_video,  "MOFTag                        %.4s\n", (unsigned char *)&SequenceHeader->MOFTag);
    SE_INFO(group_frameparser_video,  "SubMOFTag                     %.4s\n", (unsigned char *)&SequenceHeader->SubMOFTag);
    SE_INFO(group_frameparser_video,  "Width                       %6u\n", SequenceHeader->Width);
    SE_INFO(group_frameparser_video,  "Height                      %6u\n", SequenceHeader->Height);
    SE_INFO(group_frameparser_video,  "BitCount                    %6u\n", SequenceHeader->BitCount);
    SE_INFO(group_frameparser_video,  "PadWidth                    %6u\n", SequenceHeader->PadWidth);
    SE_INFO(group_frameparser_video,  "PadHeight                   %6u\n", SequenceHeader->PadHeight);
    SE_INFO(group_frameparser_video,  "FramesPerSecond           %8x\n", SequenceHeader->FramesPerSecond);
    SE_INFO(group_frameparser_video,  "OpaqueDataSize              %6u\n", SequenceHeader->OpaqueDataSize);
    SE_INFO(group_frameparser_video,  "SPOFlags                  %8x\n", SequenceHeader->SPOFlags);
    SE_INFO(group_frameparser_video,  "Bitstream Version           %6u\n", SequenceHeader->BitstreamVersion);
    SE_INFO(group_frameparser_video,  "Bitstream Minor Version     %6u\n", SequenceHeader->BitstreamMinorVersion);
    SE_INFO(group_frameparser_video,  "NumRPRSizes                 %6u\n", SequenceHeader->NumRPRSizes);
    SE_INFO(group_frameparser_video,  "Max Width                   %6u\n", SequenceHeader->MaxWidth);
    SE_INFO(group_frameparser_video,  "Max Height                  %6u\n", SequenceHeader->MaxHeight);
    SE_INFO(group_frameparser_video,  "FrameRate                 %d.%04d\n", FrameRate.IntegerPart(), FrameRate.RemainderDecimal());

    for (i = 0; i <= (SequenceHeader->NumRPRSizes * 2); i += 2)
    {
        SE_INFO(group_frameparser_video,  "RPRSize[%d]                 %6u\n", i, SequenceHeader->RPRSize[i]);
        SE_INFO(group_frameparser_video,  "RPRSize[%d]                 %6u\n", i + 1, SequenceHeader->RPRSize[i + 1]);
    }

#endif
    StreamFormatInfoValid         = true;
    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::ReadPictureHeader(void)
{
    FrameParserStatus_t         Status;
    RmvVideoPicture_t          *Header;
    RmvVideoSequence_t         *SequenceHeader;
    unsigned int                NumMacroBlocks;
    unsigned int                MbaBits;
    unsigned int                i;
    RmvVideoSegmentList_t      *SegmentList;

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to get new frame parameters\n");
            return Status;
        }
    }

    Header                              = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(RmvVideoPicture_t));

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    // Copy Segment list into Frame parameters for later use by codec
    SegmentList                         = &FrameParameters->SegmentList;
    //memset (&SegmentList->Segment[0], 0, sizeof (RmvVideoSegment_t)*RMV_MAX_SEGMENTS);
    SegmentList->NumSegments            = StartCodeList->NumberOfStartCodes;

    for (i = 0; i < StartCodeList->NumberOfStartCodes; i++)
    {
        SegmentList->Segment[i].Valid   =  1;
        SegmentList->Segment[i].Offset  =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
    }

    FrameParameters->SegmentListPresent = true;

    switch (SequenceHeader->BitstreamVersion)
    {
    case RV20_BITSTREAM_VERSION:
    case RV30_BITSTREAM_VERSION:
        Header->BitstreamVersion    = Bits.Get(3);
        Header->PictureCodingType   = Bits.Get(2);
        Header->ECC                 = Bits.Get(1);
        Header->Quant               = Bits.Get(5);
        Header->Deblock             = Bits.Get(1);
        Header->RvTr                = Bits.Get(13);
        Header->PctSize             = Bits.Get(PCT_SIZES[SequenceHeader->NumRPRSizes]);
        NumMacroBlocks              = (((SequenceHeader->Width + 15) >> 4) * ((SequenceHeader->Height + 15) >> 4)) - 1;
        MbaBits                     = (NumMacroBlocks & 0x2000) ? 14 :
                                      (NumMacroBlocks & 0x1000) ? 13 :
                                      (NumMacroBlocks & 0x800) ? 12 :
                                      (NumMacroBlocks & 0x400) ? 11 :
                                      (NumMacroBlocks & 0x200) ? 10 :
                                      (NumMacroBlocks & 0x100) ? 9 :
                                      (NumMacroBlocks & 0x80) ? 8 :
                                      (NumMacroBlocks & 0x40) ? 7 :
                                      (NumMacroBlocks & 0x20) ? 6 :
                                      (NumMacroBlocks & 0x10) ? 5 :
                                      (NumMacroBlocks & 0x08) ? 4 :
                                      (NumMacroBlocks & 0x04) ? 3 :
                                      (NumMacroBlocks & 0x02) ? 2 : 1;
        Header->Mba                 = Bits.Get(MbaBits);
        Header->RType               = Bits.Get(1);
        break;

    case RV40_BITSTREAM_VERSION:
        Header->ECC                 = Bits.Get(1);
        Header->PictureCodingType   = Bits.Get(2);
        Header->Quant               = Bits.Get(5);
        Header->BitstreamVersion    = Bits.Get(1);
        Bits.Get(4);
        Header->RvTr                = Bits.Get(13);
        //Header->OsvQuant            = Bits.Get(5);
        //Header->Deblock             = 1 - Bits.Get(1);
        //MbaBits                     = (NumMacroBlocks & 0x2000) ? 14 :
        //                                  (NumMacroBlocks & 0x1000) ? 13 :
        //                                  (NumMacroBlocks & 0x800) ? 12 :
        //                                  (NumMacroBlocks & 0x400) ? 11 :
        //                                  (NumMacroBlocks & 0x200) ? 10 :
        //                                  (NumMacroBlocks & 0x100) ? 9 :
        //                                  (NumMacroBlocks & 0x80) ? 8 :
        //                                  (NumMacroBlocks & 0x40) ? 7 :
        //                                  (NumMacroBlocks & 0x20) ? 6 :
        //                                  (NumMacroBlocks & 0x10) ? 5 :
        //                                  (NumMacroBlocks & 0x08) ? 4 :
        //                                  (NumMacroBlocks & 0x04) ? 3 :
        //                                  (NumMacroBlocks & 0x02) ? 2 : 1;
        //Header->Mba                 = Bits.Get(MbaBits);
        break;
    }

    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "PictureHeader :-\n");
    SE_INFO(group_frameparser_video,  "Picture Ptype %d\n", Header->PictureCodingType);
    SE_INFO(group_frameparser_video,  "BitstreamVersion            %6u\n", Header->BitstreamVersion);
    SE_INFO(group_frameparser_video,  "PictureCodingType           %6u\n", Header->PictureCodingType);
    SE_INFO(group_frameparser_video,  "ECC                         %6u\n", Header->ECC);
    SE_INFO(group_frameparser_video,  "Quant                       %6u\n", Header->Quant);
    SE_INFO(group_frameparser_video,  "Deblock                     %6u\n", Header->Deblock);
    SE_INFO(group_frameparser_video,  "RvTr                        %6u\n", Header->RvTr);
    SE_INFO(group_frameparser_video,  "PctSize                     %6u\n", Header->PctSize);
    SE_INFO(group_frameparser_video,  "Mba                         %8x\n", Header->Mba);
    SE_INFO(group_frameparser_video,  "RType                       %6u\n", Header->RType);
    SE_INFO(group_frameparser_video,  "NumMacroBlocks              %6u\n", NumMacroBlocks);
    SE_INFO(group_frameparser_video,  "MbaBits                     %6u\n", MbaBits);
#endif
    return FrameParserNoError;
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
FrameParserStatus_t   FrameParser_VideoRmv_c::CommitFrameForDecode(void)
{
    RmvVideoPicture_t                  *PictureHeader;
    RmvVideoSequence_t                 *SequenceHeader;
    FrameParserStatus_t                 Status = FrameParserNoError;

    //
    // Check we have the headers we need
    //

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Stream parameters unavailable for decode\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Frame parameters unavailable for decode (%p)\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

    SequenceHeader          = &StreamParameters->SequenceHeader;
    PictureHeader           = &FrameParameters->PictureHeader;
    // make rmv struggle through
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    //
    // Record the stream and frame parameters into the appropriate structure
    //
    ParsedFrameParameters->KeyFrame                             = PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = (PictureHeader->PictureCodingType != RMV_PICTURE_CODING_TYPE_B);
    //(PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_P);
    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(RmvStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(RmvFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;
#if defined (RMV_OVERRIDE_PTS)
    if ((int)PictureHeader->RvTr < (int)(LastTemporalReference - RMV_TEMPORAL_REF_HALF_RANGE))
    {
        TemporalReferenceBase                                  += RMV_TEMPORAL_REF_RANGE;
    }
    else if (PictureHeader->RvTr > (LastTemporalReference + RMV_TEMPORAL_REF_HALF_RANGE))
    {
        TemporalReferenceBase                                  -= RMV_TEMPORAL_REF_RANGE;
    }

    LastTemporalReference                                       = PictureHeader->RvTr;
    FramePTS                                                    = (LastTemporalReference + TemporalReferenceBase) * 90;
    SE_DEBUG(group_frameparser_video, "Frame %d, Pic type %d, TRef %d(%d), PTS from TRef %llu, PTS from PES %llu(%d)\n",
             FrameNumber, PictureHeader->PictureCodingType, LastTemporalReference, TemporalReferenceBase, FramePTS,
             CodedFrameParameters->PlaybackTime,  CodedFrameParameters->PlaybackTimeValid);
#endif
#if defined (RMV_RECALCULATE_FRAMERATE)

    if (FrameNumber == 0)
    {
        CalculatedFrameRate             = FrameRate;
        InitialTemporalReference        = LastTemporalReference;
    }
    else if (PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_I)
    {
        CalculatedFrameRate             = Rational_t (FrameNumber * 1000,
                                                      (LastTemporalReference + TemporalReferenceBase) - InitialTemporalReference);
        PCount                          = 0;
    }
    else if (PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_B)
    {
        CalculatedFrameRate             = Rational_t ((FrameNumber - PCount) * 1000,
                                                      (LastTemporalReference + TemporalReferenceBase) - InitialTemporalReference);
    }
    else
    {
        PCount                          = 1;
    }

    SE_DEBUG(group_frameparser_video, "CalculatedFrameRate(%d) %d.%04d\n",
             PictureHeader->PictureCodingType, CalculatedFrameRate.IntegerPart(), CalculatedFrameRate.RemainderDecimal());
#endif
    FrameNumber++;
#if 0
    // Use the frame temporal reference to generate the frame playback time.
    CodedFramePlaybackTimeValid                                 = true;
    CodedFramePlaybackTime                                      = FramePTS;
#endif
    CodedFramePlaybackTimeValid                                 = CodedFrameParameters->PlaybackTimeValid;
    CodedFramePlaybackTime                                      = CodedFrameParameters->PlaybackTime;
    ParsedVideoParameters->Content.Width                        = SequenceHeader->MaxWidth;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->MaxHeight;
    ParsedVideoParameters->Content.DisplayWidth                 = SequenceHeader->Width;
    ParsedVideoParameters->Content.DisplayHeight                = SequenceHeader->Height;
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    StreamEncodedFrameRate                  = FrameRate;
    ParsedVideoParameters->Content.FrameRate                    = ResolveFrameRate();
    // The timestamps on each frame seems to provide the only reliable timing information.
    // For this reason we set the fram rate to a high value and allow the output_timer to
    // work out how long to display each frame for.
    // ParsedVideoParameters->Content.FrameRate                    = 60;
    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;
    ParsedVideoParameters->Content.PixelAspectRatio             = 1;
    ParsedVideoParameters->Content.VideoFullRange               = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;
    ParsedVideoParameters->DisplayCount[0]                      = 1;
    ParsedVideoParameters->DisplayCount[1]                      = 0;
    ParsedVideoParameters->Content.PixelAspectRatio             = 1;
    ParsedVideoParameters->PictureStructure                     = StructureFrame;
    ParsedVideoParameters->InterlacedFrame                      = false;
    ParsedVideoParameters->TopFieldFirst                        = true;
    ParsedVideoParameters->PanScanCount                         = 0;
    // Record our claim on both the frame and stream parameters
    Buffer->AttachBuffer(StreamParametersBuffer);
    Buffer->AttachBuffer(FrameParametersBuffer);
    // We clear the FrameParameters pointer, a new one will be obtained
    // before/if we read in headers pertaining to the next frame. This
    // will generate an error should I accidentally write code that
    // accesses this data when it should not.
    FrameParameters                                             = NULL;
    // Finally set the appropriate flag and return
    FrameToDecode
        = true;
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
bool   FrameParser_VideoRmv_c::NewStreamParametersCheck(void)
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
    Different   = memcmp(&CopyOfStreamParameters, StreamParameters, sizeof(RmvStreamParameters_t)) != 0;

    if (Different)
    {
        memcpy(&CopyOfStreamParameters, StreamParameters, sizeof(RmvStreamParameters_t));
        return true;
    }

    return false;
}
//}}}

