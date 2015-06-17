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

Source file name : frame_parser_video_vp6.cpp
Author :           Julian

Implementation of the vp6 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-May-08   Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "vp6.h"
#include "frame_parser_video_vp6.h"
#include "ring_generic.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


static BufferDataDescriptor_t     Vp6StreamParametersBuffer     = BUFFER_VP6_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     Vp6FrameParametersBuffer      = BUFFER_VP6_FRAME_PARAMETERS_TYPE;

#define Assert(x, fmt, args...) {                                                               \
        if(!(x))                                                                                \
        {                                                                                       \
            report (severity_error, "FrameParser_VideoVp6_c::%s: " fmt,  __FUNCTION__, ##args); \
            Player->MarkStreamUnPlayable (Stream);                                              \
            return FrameParserError;                                                            \
        }                       }

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif


//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoVp6_c::FrameParser_VideoVp6_c (void)
{

    // Our constructor is called after our subclass so the only change is to rename the frame parser
    Configuration.FrameParserName               = "VideoVp6";


    Configuration.StreamParametersCount         = 4;
    Configuration.StreamParametersDescriptor    = &Vp6StreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &Vp6FrameParametersBuffer;

    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    FrameRate                                   = Rational_t (24000, 1001);
    StreamMetadataValid                         = false;

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoVp6_c::~FrameParser_VideoVp6_c (void)
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
FrameParserStatus_t   FrameParser_VideoVp6_c::Reset (void)
{
    memset (&CopyOfStreamParameters, 0x00, sizeof(Vp6StreamParameters_t));
    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;

    FirstDecodeOfFrame                          = true;
    FrameRate                                   = Rational_t (24000, 1001);
    StreamMetadataValid                         = false;

    return FrameParser_Video_c::Reset();
}
//}}}

//{{{  ReadHeaders
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Scan the start code list reading header specific information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp6_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);

    if (!StreamMetadataValid)
        Status          = ReadStreamMetadata();
    else
    {
        Status          = ReadPictureHeader ();
        if (Status == FrameParserNoError)
            Status      = CommitFrameForDecode();
    }

    return Status;
}
//}}}
//{{{  ReadStreamMetadata
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in an abc metadata structure
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp6_c::ReadStreamMetadata (void)
{

#if 0
    {
        unsigned char*  Buff;
        unsigned int    Bib;
        int             i;

        Bits.GetPosition(&Buff, &Bib);
        Buff -= 4;
        for (i=0; i<40; i+=4)
            report (severity_info, "%02x %02x %02x %02x\n", Buff[i], Buff[i+1], Buff[i+2], Buff[i+3]);
    }
#endif

    if ((StreamParameters != NULL) && (StreamParameters->SequenceHeaderPresent))
    {
        report (severity_error, "%s: Received Sequence Layer MetaData after previous sequence data\n", __FUNCTION__);
        return FrameParserNoError;
    }

    memcpy (&MetaData, BufferData, sizeof (MetaData));


    if (MetaData.FrameRate < 23950000)
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    else if (MetaData.FrameRate < 23990000)
        FrameRate                               = Rational_t (24000, 1001);
    else if (MetaData.FrameRate < 24050000)
        FrameRate                               = Rational_t (24000, 1000);
    else if (MetaData.FrameRate < 24950000)
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    else if (MetaData.FrameRate < 25050000)
        FrameRate                               = Rational_t (25000, 1000);
    else if (MetaData.FrameRate < 29950000)
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);
    else if (MetaData.FrameRate < 29990000)
        FrameRate                               = Rational_t (30000, 1001);
    else if (MetaData.FrameRate < 30050000)
        FrameRate                               = Rational_t (30000, 1000);
    else
        FrameRate                               = Rational_t (MetaData.FrameRate, 1000000);


    StreamMetadataValid                         = (MetaData.Codec == CODEC_ID_VP6) || (MetaData.Codec == CODEC_ID_VP6_ALPHA);

#ifdef DUMP_HEADERS
    report (severity_info, "StreamMetadata :- \n");
    report (severity_info, "    Codec             : %6d\n", MetaData.Codec);
    report (severity_info, "    Width             : %6d\n", MetaData.Width);
    report (severity_info, "    Height            : %6d\n", MetaData.Height);
    report (severity_info, "    Duration          : %6d\n", MetaData.Duration);
    report (severity_info, "    FrameRate         : %6d\n", MetaData.FrameRate);
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp6_c::ReadPictureHeader (void)
{
    FrameParserStatus_t         Status;
    Vp6VideoPicture_t*          Header;
    Vp6VideoSequence_t*         SequenceHeader;
    unsigned int                Marker;
    unsigned char*              BitsPointer;
    unsigned int                BitsInByte;
    int                         VrtShift        = 0;
    bool                        ParseFilterInfo = false;
    unsigned int                Version;
    unsigned int                Profile;
    unsigned int                EncodedWidth;
    unsigned int                EncodedHeight;
    unsigned int                DisplayWidth;
    unsigned int                DisplayHeight;
    unsigned int                HorizontalAdjustment;
    unsigned int                VerticalAdjustment;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        return Status;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(Vp6VideoPicture_t));

    HorizontalAdjustment                        = Bits.Get(4);
    VerticalAdjustment                          = Bits.Get(4);
    if ((StreamMetadataValid) && (MetaData.Codec == CODEC_ID_VP6_ALPHA))
    {
        unsigned int  OffsetToAlpha             = Bits.Get(24);
        FRAME_DEBUG ("Offset to alpha = %d\n", OffsetToAlpha);
    }
    Header->ptype                               = Bits.Get(1);
    Header->pquant                              = Bits.Get(6);

    Marker                                      = Bits.Get(1);
    //Assert (Marker == 1, "VP60 (Simple profile) not supported\n");

    if (Header->ptype == VP6_PICTURE_CODING_TYPE_I)
    {
        // VersionNo. The values 6,7, and 8 represent VP6.0, VP6.1, and VP6.2 bitsreams, respectively.
        Version                                 = Bits.Get(5);
        Assert (Version <= 8, "Version %d not supported\n", Version);

        Profile                                 = Bits.Get(2);
        Assert (Profile == VP6_PROFILE_ADVANCED, "Profile %d not supported\n", Profile);

        Assert (Bits.Get(1) == 0, "Interlaced not supported\n")

        EncodedHeight                           = Bits.Get(8) * 16;
        EncodedWidth                            = Bits.Get(8) * 16;             // Values are given in macro blocks
        DisplayHeight                           = Bits.Get(8) * 16;
        DisplayWidth                            = Bits.Get(8) * 16;

        // For VP6 there is no sequence header so we create one whenever the picture size changes
        if ((StreamParameters == NULL) || (!StreamParameters->SequenceHeaderPresent) ||
            (EncodedWidth  != StreamParameters->SequenceHeader.encoded_width)  ||
            (EncodedHeight != StreamParameters->SequenceHeader.encoded_height) ||
            (DisplayWidth  != StreamParameters->SequenceHeader.display_width)  ||
            (DisplayHeight != StreamParameters->SequenceHeader.display_height))
        {
            Status                                      = GetNewStreamParameters ((void **)&StreamParameters);
            if (Status != FrameParserNoError)
                return Status;
            StreamParameters->UpdatedSinceLastFrame     = true;

            SequenceHeader                              = &StreamParameters->SequenceHeader;
            memset (SequenceHeader, 0x00, sizeof(Vp6VideoSequence_t));
            StreamParameters->SequenceHeaderPresent     = true;

            SequenceHeader->version                     = Version;
            SequenceHeader->profile                     = Profile;
            SequenceHeader->encoded_width               = EncodedWidth;
            SequenceHeader->encoded_height              = EncodedHeight;
            SequenceHeader->display_width               = DisplayWidth - HorizontalAdjustment;
            SequenceHeader->display_height              = DisplayHeight - VerticalAdjustment;
        }
        else
            SequenceHeader                              = &StreamParameters->SequenceHeader;

        Bits.GetPosition (&BitsPointer, &BitsInByte);
        RangeDecoder.Init (BitsPointer);

        RangeDecoder.GetBits (2);                                               // marker bits - ignored

        ParseFilterInfo                         = 1;

        if (SequenceHeader->version < 8)
            VrtShift                            = 5;
    }
    else if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR ("Sequence header not found.\n");
        return FrameParserNoStreamParameters;
    }
    else
    {
        SequenceHeader                          = &StreamParameters->SequenceHeader;

        if (SequenceHeader->version == 0)
        {
            FRAME_ERROR ("Cannot decode p frame as unknown version\n");
            return FrameParserError;
        }

        Bits.GetPosition (&BitsPointer, &BitsInByte);
        RangeDecoder.Init (BitsPointer);

        Header->golden_frame                    = RangeDecoder.GetBit();
        Header->deblock_filtering               = RangeDecoder.GetBit();        // UseLoopFilter,present in Advanced Profile only
        if (Header->deblock_filtering)
            RangeDecoder.GetBit ();             // Parse LoopFilterSelector
                                                // The de-ringing version of the loop-filter is NOT currently defined in the VP6
                                                // decoder specification. Therefore, at the current time it is mandated that where
                                                // the loop-filter is used the field LoopFilterSelector must be set to the value 0

        if (SequenceHeader->version > 7)
            ParseFilterInfo                     = RangeDecoder.GetBit();        // Present only in VP6.2 bitstreams
    }

    if (ParseFilterInfo)
    {
        if (RangeDecoder.GetBit())
        {
            SequenceHeader->filter_mode         = 2;

            // PredictionFilterVarThresh. Variance threshold at or above which the bi-cubic
            // motioncompensated interpolation filter will be used, otherwise bi-linear filter is used.
            SequenceHeader->variance_threshold  = RangeDecoder.GetBits(5) << VrtShift;

            // PredictionFilterMvSizeThresh. Used to set largest MV magnitude at which the
            // bi-cubic filter is used, otherwise bi-linear filter is used
            SequenceHeader->max_vector_length   = 2 << RangeDecoder.GetBits(3);
        }
        else
            SequenceHeader->filter_mode         = RangeDecoder.GetBit();

        if (SequenceHeader->version > 7)
        {
            SequenceHeader->filter_selection    = RangeDecoder.GetBits(4);
        }
        else
            SequenceHeader->filter_selection    = 16;
    }

    RangeDecoder.GetBit();

    Header->high                                = RangeDecoder.HighVal();
    Header->bits                                = RangeDecoder.BitsVal();
    Header->code_word                           = RangeDecoder.DataVal();
    Header->offset                              = RangeDecoder.BufferVal() - BufferData;

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    if (Header->ptype == VP6_PICTURE_CODING_TYPE_I)
    {
        report (severity_note, "Picture header (%d)\n", PictureNo++);
        report (severity_info, "    version             : %6d\n", SequenceHeader->version);
        report (severity_info, "    profile             : %6d\n", SequenceHeader->profile);
        report (severity_info, "    HorizontalAdjustment: %6d\n", HorizontalAdjustment);
        report (severity_info, "    VerticalAdjustment  : %6d\n", VerticalAdjustment);
        report (severity_info, "    encoded_width       : %6d\n", SequenceHeader->encoded_width);
        report (severity_info, "    encoded_height      : %6d\n", SequenceHeader->encoded_height);
        report (severity_info, "    display_width       : %6d\n", SequenceHeader->display_width);
        report (severity_info, "    display_height      : %6d\n", SequenceHeader->display_height);
        report (severity_info, "    filter_mode         : %6d\n", SequenceHeader->filter_mode);
        report (severity_info, "    variance_threshold  : %6d\n", SequenceHeader->variance_threshold);
        report (severity_info, "    max_vector_length   : %6d\n", SequenceHeader->max_vector_length);
        report (severity_info, "    filter_selection    : %6d\n", SequenceHeader->filter_selection);
    }
    else
        PictureNo++;

#if 0
    report (severity_info, "    ptype               : %6d\n", Header->ptype);
    report (severity_info, "    pquant              : %6d\n", Header->pquant);
    report (severity_info, "    golden_frame        : %6d\n", Header->golden_frame);
    report (severity_info, "    deblock_filtering   : %6d\n", Header->deblock_filtering);
#endif

#endif

     return FrameParserNoError;
}
//}}}

//{{{  RegisterOutputBufferRing
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Register the output ring
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp6_c::RegisterOutputBufferRing (Ring_t          Ring)
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
FrameParserStatus_t   FrameParser_VideoVp6_c::PrepareReferenceFrameList (void)
{
    unsigned int               i;
    unsigned int               ReferenceFramesNeeded;
    unsigned int               PictureCodingType;
    Vp6VideoPicture_t*         PictureHeader;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.
    //

    // For VP6, every frame is a reference frame so we always fill in the reference frame list
    // even though I frames do not actually need them.
    // Element 0 is for the reference frame and element 1 is for the golden frame.

    PictureHeader               = &(((Vp6FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->ptype;
    ReferenceFramesNeeded       = (PictureCodingType == VP6_PICTURE_CODING_TYPE_P) ? 1 : 0;

    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;

    if ((ReferenceFrameList.EntryCount == 0) && (PictureCodingType == VP6_PICTURE_CODING_TYPE_I))       // First frame reference self
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = 2;
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0]   = NextDecodeFrameIndex;
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1]   = NextDecodeFrameIndex;
    }
    else
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryCount         = ReferenceFrameList.EntryCount;
        for (i=0; i<ReferenceFrameList.EntryCount; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[i];
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
FrameParserStatus_t   FrameParser_VideoVp6_c::ForPlayUpdateReferenceFrameList (void)
{
    Vp6FrameParameters_t*       FrameParameters = (Vp6FrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;

    // For VP6 every frame is a reference frame so we always free the current reference frame if it isn't a
    // golden frame.  The reference frame is kept in slot 0. If the current frame is also a golden frame we
    // free and replace the golden frame in slot 1 as well.

    if (ReferenceFrameList.EntryCount == 0)
        ReferenceFrameList.EntryCount           = 1;
    else if (ReferenceFrameList.EntryIndicies[0] != ReferenceFrameList.EntryIndicies[1])
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);

    ReferenceFrameList.EntryIndicies[0]         = ParsedFrameParameters->DecodeFrameIndex;      // put us into slot 0 as a reference frame

    if ((FrameParameters->PictureHeader.golden_frame) || (FrameParameters->PictureHeader.ptype == VP6_PICTURE_CODING_TYPE_I))
    {
        if (ReferenceFrameList.EntryCount == 1)
            ReferenceFrameList.EntryCount       = 2;
        else
            Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[1]);
        ReferenceFrameList.EntryIndicies[1]     = ParsedFrameParameters->DecodeFrameIndex;      // insert into slot 1 as a golden frame
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

FrameParserStatus_t   FrameParser_VideoVp6_c::RevPlayProcessDecodeStacks (void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();

    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}

//{{{  CommitFrameForDecode
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Send frame for decode
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoVp6_c::CommitFrameForDecode (void)
{
    Vp6VideoPicture_t*                  PictureHeader;
    Vp6VideoSequence_t*                 SequenceHeader;

    //
    // Check we have the headers we need
    //
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR ("Stream parameters unavailable for decode.\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR ("Frame parameters unavailable for decode (%p).\n", FrameParameters);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;

    // Record the stream and frame parameters into the appropriate structure

    // Parsed frame parameters
    ParsedFrameParameters->FirstParsedParametersForOutputFrame  = true;  //FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump  = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                  = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                = ContinuousReverseJump;

    ParsedFrameParameters->KeyFrame                             = PictureHeader->ptype == VP6_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = true;

    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NumberOfReferenceFrameLists          = 1;

    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(Vp6StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;

    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(Vp6FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;

    // Parsed video parameters
    ParsedVideoParameters->Content.Width                        = SequenceHeader->encoded_width;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->encoded_height;
    ParsedVideoParameters->Content.DisplayWidth                 = SequenceHeader->display_width;
    ParsedVideoParameters->Content.DisplayHeight                = SequenceHeader->display_height;

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;

    ParsedVideoParameters->Content.VideoFullRange               = false;                                // VP6 conforms to ITU_R_BT601 for source
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;

    // Frame rate defaults to 23.976 if metadata not valid
    ParsedVideoParameters->Content.FrameRate                    = FrameRate;
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
    FrameToDecode                                               = true;

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
bool   FrameParser_VideoVp6_c::NewStreamParametersCheck (void)
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

    Different   = memcmp (&CopyOfStreamParameters, StreamParameters, sizeof(Vp6StreamParameters_t)) != 0;
    if (Different)
    {
        memcpy (&CopyOfStreamParameters, StreamParameters, sizeof(Vp6StreamParameters_t));
        return true;
    }

//

    return false;
}
//}}}
