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

Source file name : frame_parser_video_rmv.cpp
Author :           Julian

Implementation of the Real Media video video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                   --------
24-Oct-2008 Created                                         Julian

************************************************************************/


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_rmv.h"
#include "ring_generic.h"

//#define DUMP_HEADERS

//{{{  Locally defined constants and macros
static BufferDataDescriptor_t     RmvStreamParametersBuffer = BUFFER_RMV_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     RmvFrameParametersBuffer = BUFFER_RMV_FRAME_PARAMETERS_TYPE;

//

#define CountsIndex( PS, F, TFF, RFF)           ((((PS) != 0) << 3) | (((F) != 0) << 2) | (((TFF) != 0) << 1) | ((RFF) != 0))
#define Legal( PS, F, TFF, RFF)                 Counts[CountsIndex(PS, F, TFF, RFF)].LegalValue
#define PanScanCount( PS, F, TFF, RFF)         Counts[CountsIndex(PS, F, TFF, RFF)].PanScanCountValue
#define DisplayCount0( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount0Value
#define DisplayCount1( PS, F, TFF, RFF)        Counts[CountsIndex(PS, F, TFF, RFF)].DisplayCount1Value

//

static const unsigned int ReferenceFramesRequired[RMV_PICTURE_CODING_TYPE_B+1]    = {0, 2, 1, 2};
#define REFERENCE_FRAMES_NEEDED(CodingType)             ReferenceFramesRequired[CodingType]
//#define REFERENCE_FRAMES_NEEDED(CodingType)             CodingType

#define MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE         REFERENCE_FRAMES_NEEDED(RMV_PICTURE_CODING_TYPE_B)

#define Assert(L)               if( !(L) )                                                                      \
                                {                                                                               \
                                    report ( severity_error, "Assertion fail %s %d\n", __FUNCTION__, __LINE__ );\
                                    Player->MarkStreamUnPlayable( Stream );                                     \
                                    return FrameParserError;                                                    \
                                }

static unsigned int PCT_SIZES[] = {0, 1, 1, 2, 2, 3, 3, 3, 3};

//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoRmv_c::FrameParser_VideoRmv_c (void)
{
    Configuration.FrameParserName               = "VideoRmv";

    Configuration.StreamParametersCount         = 4;
    Configuration.StreamParametersDescriptor    = &RmvStreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &RmvFrameParametersBuffer;

//

    StreamFormatInfoValid                       = false;

    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    Reset();

    LastTemporalReference                       = 0;
    TemporalReferenceBase                       = 0;
    FrameNumber                                 = 0;
#if defined (RECALCULATE_FRAMERATE)
    InitialTemporalReference                    = 0;
#endif

}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoRmv_c::~FrameParser_VideoRmv_c (void)
{
    Halt();
    Reset();
}
//}}}


//{{{  Reset
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      The Reset function release any resources, and reset all variable
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::Reset (void)
{
    memset (&CopyOfStreamParameters, 0x00, sizeof(RmvStreamParameters_t));
    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    FirstDecodeOfFrame                          = true;

    return FrameParser_Video_c::Reset();
}
//}}}
//{{{  RegisterOutputBufferRing
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Register the output ring
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoRmv_c::RegisterOutputBufferRing (Ring_t          Ring)
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

//{{{  ResetReferenceFrameList
// /////////////////////////////////////////////////////////////////////////
//
//      The reset reference frame list function
//

FrameParserStatus_t   FrameParser_VideoRmv_c::ResetReferenceFrameList( void )
{
    FrameParserStatus_t         Status  = FrameParser_Video_c::ResetReferenceFrameList ();

    LastTemporalReference               = 0;
    TemporalReferenceBase               = 0;
    FrameNumber                         = 0;
#if defined (RECALCULATE_FRAMERATE)
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
FrameParserStatus_t   FrameParser_VideoRmv_c::PrepareReferenceFrameList (void)
{
    unsigned int        ReferenceFramesNeeded;
    unsigned int        ReferenceFramesAvailable;
    unsigned int        PictureCodingType;
    RmvVideoPicture_t*  PictureHeader;
    unsigned int        i;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.
    //

    PictureHeader               = &(((RmvFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->PictureCodingType;
    ReferenceFramesNeeded       = REFERENCE_FRAMES_NEEDED(PictureCodingType);
    ReferenceFramesAvailable    = ReferenceFrameList.EntryCount;

    // Check for sufficient reference frames.  We cannot decode otherwise
    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = 0;

    // For RMV we always fill in all of the reference frames.  This is done by copying previous ones
    // where necessary.  The most recent reference frame is inserted in slot 0 and the one before
    // that in slot 1. They are kept in the ReferenceFrameList array in the reverse order, however.
    if (ReferenceFramesAvailable > 0)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE;
        for(i=0; i<MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE; i++)
        {
            if (ReferenceFramesAvailable > i)
                ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFramesAvailable - i - 1];
            else
                ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i-1];
        }
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
FrameParserStatus_t   FrameParser_VideoRmv_c::ForPlayUpdateReferenceFrameList (void)
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

FrameParserStatus_t   FrameParser_VideoRmv_c::RevPlayProcessDecodeStacks (void)
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
FrameParserStatus_t   FrameParser_VideoRmv_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);

    if (!StreamFormatInfoValid)
        Status          = ReadStreamFormatInfo();
    else
    {
        Status          = ReadPictureHeader ();
        if (Status == FrameParserNoError)
            Status      = CommitFrameForDecode();
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

FrameParserStatus_t   FrameParser_VideoRmv_c::ReadStreamFormatInfo (void)
{

    FrameParserStatus_t         Status;
    RmvVideoSequence_t*         SequenceHeader;
    unsigned int                EncodedSize     = 0;
    unsigned int                i;

    FRAME_DEBUG("\n");

#if 0
    unsigned int        Checksum = 0;
    report (severity_info, "data %d :\n", BufferLength);
    for (i=0; i<BufferLength; i++)
    {
        if ((i&0x0f)==0)
            report (severity_info, "\n%06x", i);
        report (severity_info, " %02x", BufferData[i]);
        Checksum       += BufferData[i];
    }
    report (severity_info, "\nChecksum %08x\n", Checksum);
#endif

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        report (severity_error, "%s: Received Stream FormatInfo after previous sequence data\n", __FUNCTION__);
        return FrameParserNoError;
    }

    Status      = GetNewStreamParameters ((void **)&StreamParameters);
    if (Status != FrameParserNoError)
        return Status;

    StreamParameters->SequenceHeaderPresent     = true;
    StreamParameters->UpdatedSinceLastFrame     = true;
    SequenceHeader                              = &StreamParameters->SequenceHeader;
    memset (SequenceHeader, 0x00, sizeof(RmvVideoSequence_t));

    SequenceHeader->Length                      = Bits.Get (32);
    SequenceHeader->MOFTag                      = Bits.Get (32);
    SequenceHeader->SubMOFTag                   = Bits.Get (32);
    SequenceHeader->Width                       = Bits.Get (16);
    SequenceHeader->Height                      = Bits.Get (16);
    SequenceHeader->BitCount                    = Bits.Get (16);
    SequenceHeader->PadWidth                    = Bits.Get (16);
    SequenceHeader->PadHeight                   = Bits.Get (16);
    SequenceHeader->FramesPerSecond             = Bits.Get (32);
    SequenceHeader->OpaqueDataSize              = SequenceHeader->Length - RMV_FORMAT_INFO_HEADER_SIZE;

    if ((SequenceHeader->Width > RMV_MAX_SUPPORTED_DECODE_WIDTH) || (SequenceHeader->Height > RMV_MAX_SUPPORTED_DECODE_HEIGHT))
    {
        FRAME_ERROR ("Content dimensions (%dx%d) greater than max supported (%dx%d)\n",
                        SequenceHeader->Width, SequenceHeader->Height,
                        RMV_MAX_SUPPORTED_DECODE_WIDTH, RMV_MAX_SUPPORTED_DECODE_HEIGHT);
        Player->MarkStreamUnPlayable (Stream);
        return FrameParserError;
    }

    FrameRate                                   = Rational_t (SequenceHeader->FramesPerSecond, 0x10000);

    SequenceHeader->RPRSize[0]                  = SequenceHeader->Width;
    SequenceHeader->RPRSize[1]                  = SequenceHeader->Height;
    SequenceHeader->NumRPRSizes                 = 0;
    if (SequenceHeader->OpaqueDataSize > 0)
    {
        unsigned int    BitstreamVersionData;

        SequenceHeader->SPOFlags                = Bits.Get (32);

        BitstreamVersionData                    = Bits.Get (32);
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
            FRAME_ERROR ("Unsupported bitstream versions (%d, %d)\n",
                          SequenceHeader->BitstreamVersion, SequenceHeader->BitstreamMinorVersion);
            Player->MarkStreamUnPlayable (Stream);
            return FrameParserError;
        }

        if (SequenceHeader->BitstreamMinorVersion != RV89_RAW_BITSTREAM_MINOR_VERSION)
        {
            if ((SequenceHeader->BitstreamVersion == RV20_BITSTREAM_VERSION) ||
                (SequenceHeader->BitstreamVersion == RV30_BITSTREAM_VERSION))
            {
                SequenceHeader->NumRPRSizes     = (SequenceHeader->SPOFlags & RV20_SPO_NUMRESAMPLE_IMAGES_BITS) >>
                                                                              RV20_SPO_NUMRESAMPLE_IMAGES_SHIFT;
                for (i = 2; i <= (SequenceHeader->NumRPRSizes*2); i+=2)
                {
                    SequenceHeader->RPRSize[i]          = Bits.Get (8)<<2;
                    SequenceHeader->RPRSize[i+1]        = Bits.Get (8)<<2;
                }
            }
            else if (SequenceHeader->BitstreamVersion == RV40_BITSTREAM_VERSION)
            {
                if (SequenceHeader->OpaqueDataSize >= 12)
                    EncodedSize                 = Bits.Get (32);
            }
        }

    }
    else
    {
        FRAME_ERROR ("No opaque data present in format info\n");
        return FrameParserError;
    }
    SequenceHeader->NumRPRSizes++;

    SequenceHeader->MaxWidth                    = (EncodedSize & RV40_ENCODED_SIZE_WIDTH_BITS)  >> RV40_ENCODED_SIZE_WIDTH_SHIFT;
    SequenceHeader->MaxWidth                    = (EncodedSize & RV40_ENCODED_SIZE_HEIGHT_BITS) << RV40_ENCODED_SIZE_HEIGHT_SHIFT;
    if (SequenceHeader->MaxWidth == 0)
        SequenceHeader->MaxWidth                = SequenceHeader->Width;
    if (SequenceHeader->MaxHeight == 0)
        SequenceHeader->MaxHeight               = SequenceHeader->Height;


    // Max width and height must be multiples of 16 to be a whole number of macroblocks
    SequenceHeader->MaxWidth                    = (SequenceHeader->MaxWidth +0x0f) & 0xfffffff0;
    SequenceHeader->MaxHeight                   = (SequenceHeader->MaxHeight +0x0f) & 0xfffffff0;

    //if (SequenceHeader->MajorBitstreamVersion

#ifdef DUMP_HEADERS
    report (severity_info, "StreamFormatInfo :- \n");
    report (severity_info, "Length                      %6u\n", SequenceHeader->Length);
    report (severity_info, "MOFTag                        %.4s\n", (unsigned char*)&SequenceHeader->MOFTag);
    report (severity_info, "SubMOFTag                     %.4s\n", (unsigned char*)&SequenceHeader->SubMOFTag);
    report (severity_info, "Width                       %6u\n", SequenceHeader->Width);
    report (severity_info, "Height                      %6u\n", SequenceHeader->Height);
    report (severity_info, "BitCount                    %6u\n", SequenceHeader->BitCount);
    report (severity_info, "PadWidth                    %6u\n", SequenceHeader->PadWidth);
    report (severity_info, "PadHeight                   %6u\n", SequenceHeader->PadHeight);
    report (severity_info, "FramesPerSecond           %8x\n", SequenceHeader->FramesPerSecond);
    report (severity_info, "OpaqueDataSize              %6u\n", SequenceHeader->OpaqueDataSize);
    report (severity_info, "SPOFlags                  %8x\n", SequenceHeader->SPOFlags);
    report (severity_info, "Bitstream Version           %6u\n", SequenceHeader->BitstreamVersion);
    report (severity_info, "Bitstream Minor Version     %6u\n", SequenceHeader->BitstreamMinorVersion);
    report (severity_info, "NumRPRSizes                 %6u\n", SequenceHeader->NumRPRSizes);
    report (severity_info, "Max Width                   %6u\n", SequenceHeader->MaxWidth);
    report (severity_info, "Max Height                  %6u\n", SequenceHeader->MaxHeight);
    report (severity_info, "FrameRate                 %d.%04d\n", FrameRate.IntegerPart(), FrameRate.RemainderDecimal());
    for (i=0; i<=(SequenceHeader->NumRPRSizes*2); i+=2)
    {
        report (severity_info, "RPRSize[%d]                 %6u\n", i, SequenceHeader->RPRSize[i]);
        report (severity_info, "RPRSize[%d]                 %6u\n", i+1, SequenceHeader->RPRSize[i+1]);
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
FrameParserStatus_t   FrameParser_VideoRmv_c::ReadPictureHeader (void)
{
    FrameParserStatus_t         Status;
    RmvVideoPicture_t*          Header;
    RmvVideoSequence_t*         SequenceHeader;
    unsigned int                NumMacroBlocks;
    unsigned int                MbaBits;
    unsigned int                i;
    RmvVideoSegmentList_t*      SegmentList;

#if 0
    unsigned int        Checksum = 0;
    report (severity_info, "data %d :\n", BufferLength);
    for (i=0; i<BufferLength; i++)
    {
        if ((i&0x0f)==0)
            report (severity_info, "\n%06x", i);
        report (severity_info, " %02x", BufferData[i]);
        Checksum       += BufferData[i];
    }
    report (severity_info, "\nChecksum %08x\n", Checksum);
#endif

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        {
            report (severity_error, "FrameParser_VideoRmv_c::ReadPictureHeader - Failed to get new frame parameters.\n");
            return Status;
        }
    }

    Header                              = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(RmvVideoPicture_t));

    SequenceHeader                      = &StreamParameters->SequenceHeader;

    // Copy Segment list into Frame parameters for later use by codec
    SegmentList                         = &FrameParameters->SegmentList;
    //memset (&SegmentList->Segment[0], 0, sizeof (RmvVideoSegment_t)*RMV_MAX_SEGMENTS);
    SegmentList->NumSegments            = StartCodeList->NumberOfStartCodes;
    for (i=0; i<StartCodeList->NumberOfStartCodes; i++)
    {
        SegmentList->Segment[i].Valid   =  1;
        SegmentList->Segment[i].Offset  =  ExtractStartCodeOffset(StartCodeList->StartCodes[i]);
    }
    FrameParameters->SegmentListPresent = true;

    //{{{  DEBUG
    #if 0
    {
        static unsigned int         Segment = 0;
        if (Segment++ < 4)
        {
            report (severity_info, "Segment list (%d):\n", StartCodeList->NumberOfStartCodes);
            for (i=0; i<StartCodeList->NumberOfStartCodes; i++)
            {
                report (severity_info, "    %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    SegmentList->Segment[i].Valid & 0xff, (SegmentList->Segment[i].Valid >> 8) & 0xff,
                    (SegmentList->Segment[i].Valid >> 16) & 0xff, (SegmentList->Segment[i].Valid >> 24) & 0xff,
                    SegmentList->Segment[i].Offset & 0xff, (SegmentList->Segment[i].Offset >> 8) & 0xff,
                    (SegmentList->Segment[i].Offset >> 16) & 0xff, (SegmentList->Segment[i].Offset >> 24) & 0xff);
            }
        }
    }
    #endif
    //}}}

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
            NumMacroBlocks              = (((SequenceHeader->Width+15)>>4)*((SequenceHeader->Height+15)>>4))-1;
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
    report (severity_info, "PictureHeader :- \n");
    report (severity_info, "Picture Ptype %d\n", Header->PictureCodingType);
    report (severity_info, "BitstreamVersion            %6u\n", Header->BitstreamVersion);
    report (severity_info, "PictureCodingType           %6u\n", Header->PictureCodingType);
    report (severity_info, "ECC                         %6u\n", Header->ECC);
    report (severity_info, "Quant                       %6u\n", Header->Quant);
    report (severity_info, "Deblock                     %6u\n", Header->Deblock);
    report (severity_info, "RvTr                        %6u\n", Header->RvTr);
    report (severity_info, "PctSize                     %6u\n", Header->PctSize);
    report (severity_info, "Mba                         %8x\n", Header->Mba);
    report (severity_info, "RType                       %6u\n", Header->RType);
    report (severity_info, "NumMacroBlocks              %6u\n", NumMacroBlocks);
    report (severity_info, "MbaBits                     %6u\n", MbaBits);
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
FrameParserStatus_t   FrameParser_VideoRmv_c::CommitFrameForDecode (void)
{
    RmvVideoPicture_t*                  PictureHeader;
    RmvVideoSequence_t*                 SequenceHeader;

    //
    // Check we have the headers we need
    //

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoRmv_c::CommitFrameForDecode - Stream parameters unavailable for decode.\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoRmv_c::CommitFrameForDecode - Frame parameters unavailable for decode (%p).\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader          = &StreamParameters->SequenceHeader;
    PictureHeader           = &FrameParameters->PictureHeader;

    //
    // Nick added this to make rmv struggle through
    //

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

#if defined (OVERRIDE_PTS)
    if ((int)PictureHeader->RvTr < (int)(LastTemporalReference - RMV_TEMPORAL_REF_HALF_RANGE))
        TemporalReferenceBase                                  += RMV_TEMPORAL_REF_RANGE;
    else if (PictureHeader->RvTr > (LastTemporalReference + RMV_TEMPORAL_REF_HALF_RANGE))
        TemporalReferenceBase                                  -= RMV_TEMPORAL_REF_RANGE;
    LastTemporalReference                                       = PictureHeader->RvTr;
    FramePTS                                                    = (LastTemporalReference + TemporalReferenceBase) * 90;

    FRAME_DEBUG("Frame %d, Pic type %d, TRef %d(%d), PTS from TRef %llu, PTS from PES %llu(%d)\n",
                 FrameNumber, PictureHeader->PictureCodingType, LastTemporalReference, TemporalReferenceBase, FramePTS,
                 CodedFrameParameters->PlaybackTime,  CodedFrameParameters->PlaybackTimeValid);
#endif

#if defined (RECALCULATE_FRAMERATE)
    if (FrameNumber == 0)
    {
        CalculatedFrameRate             = FrameRate;
        InitialTemporalReference        = LastTemporalReference;
    }
    else if (PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_I)
    {
        CalculatedFrameRate             = Rational_t (FrameNumber*1000, (LastTemporalReference+TemporalReferenceBase)-InitialTemporalReference);
        PCount                          = 0;
    }
    else if (PictureHeader->PictureCodingType == RMV_PICTURE_CODING_TYPE_B)
    {
        CalculatedFrameRate             = Rational_t ((FrameNumber-PCount)*1000, (LastTemporalReference+TemporalReferenceBase)-InitialTemporalReference);
    }
    else
    {
        PCount                          = 1;
    }
    FRAME_DEBUG("CalculatedFrameRate(%d) %d.%04d\n", PictureHeader->PictureCodingType, CalculatedFrameRate.IntegerPart(), CalculatedFrameRate.RemainderDecimal());
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

    ParsedVideoParameters->Content.FrameRate                    = FrameRate;
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

    ParsedVideoParameters->PanScan.Count                        = 0;

    // Record our claim on both the frame and stream parameters
    Buffer->AttachBuffer (StreamParametersBuffer);
    Buffer->AttachBuffer (FrameParametersBuffer);

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
bool   FrameParser_VideoRmv_c::NewStreamParametersCheck (void)
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

    Different   = memcmp (&CopyOfStreamParameters, StreamParameters, sizeof(RmvStreamParameters_t)) != 0;
    if (Different)
    {
        memcpy (&CopyOfStreamParameters, StreamParameters, sizeof(RmvStreamParameters_t));
        return true;
    }

    return false;
}
//}}}

