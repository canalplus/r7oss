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

#define DUMP_HEADERS

#include "h263.h"
#include "ring_generic.h"
#include "frame_parser_video_h263.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoH263_c"

static BufferDataDescriptor_t     H263StreamParametersBuffer    = BUFFER_H263_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     H263FrameParametersBuffer     = BUFFER_H263_FRAME_PARAMETERS_TYPE;

struct DisplaySize_s
{
    unsigned int        Width;
    unsigned int        Height;
};
static struct DisplaySize_s H263DisplaySize[] =
{
    {    0,    0 },                     // H263_FORMAT_FORBIDDEN,
    {  128,   96 },                     // H263_FORMAT_SUB_QCIF,
    {  176,  144 },                     // H263_FORMAT_QCIF,
    {  352,  288 },                     // H263_FORMAT_CIF,
    {  704,  576 },                     // H263_FORMAT_4CIF,
    { 1408, 1152 },                     // H263_FORMAT_16CIF,
    {    0,    0 },                     // H263_FORMAT_RESERVED1,
    {    0,    0 },                     // H263_FORMAT_RESERVED2,
};

#if defined (DUMP_HEADERS)
static unsigned int PictureNo;
#endif

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoH263_c::FrameParser_VideoH263_c(void)
    : FrameParser_Video_c()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , CopyOfStreamParameters()
    , TemporalReference(H263_TEMPORAL_REFERENCE_INVALID)
{
    Configuration.FrameParserName               = "VideoH263";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &H263StreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &H263FrameParametersBuffer;

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoH263_c::~FrameParser_VideoH263_c(void)
{
    Halt();
}

//{{{  Connect
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Connect the output port
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoH263_c::Connect(Port_c *Port)
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

FrameParserStatus_t   FrameParser_VideoH263_c::ResetReferenceFrameList(void)
{
    FrameParserStatus_t         Status  = FrameParser_Video_c::ResetReferenceFrameList();
    TemporalReference                   = H263_TEMPORAL_REFERENCE_INVALID;
    return Status;
}
//}}}
//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoH263_c::PrepareReferenceFrameList(void)
{
    unsigned int               i;
    unsigned int               ReferenceFramesNeeded;
    unsigned int               PictureCodingType;
    H263VideoPicture_t        *PictureHeader;
    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    //
    PictureHeader               = &(((H263FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->ptype;
    ReferenceFramesNeeded       = (PictureCodingType == H263_PICTURE_CODING_TYPE_P) ? 1 : 0;

    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
    {
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    for (i = 0; i < ReferenceFramesNeeded; i++)
    {
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];
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
FrameParserStatus_t   FrameParser_VideoH263_c::ForPlayUpdateReferenceFrameList(void)
{
    if (ReferenceFrameList.EntryCount == 1)
    {
        Stream->ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);
    }

    ReferenceFrameList.EntryCount       = 1;
    ReferenceFrameList.EntryIndicies[0] = ParsedFrameParameters->DecodeFrameIndex;
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

FrameParserStatus_t   FrameParser_VideoH263_c::RevPlayProcessDecodeStacks(void)
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
FrameParserStatus_t   FrameParser_VideoH263_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    unsigned int                StartCode;
    Bits.SetPointer(BufferData);
    StartCode                   = Bits.Get(17);

    if (StartCode == H263_PICTURE_START_CODE)
    {
        Status                  = H263ReadPictureHeader();
    }
    else
    {
        SE_ERROR("Not at the start of a picture - lost sync (%x)\n", StartCode);
        return FrameParserError;
    }

    if (Status == FrameParserNoError)
    {
        Status      = CommitFrameForDecode();
    }

    return Status;
}
//}}}
//{{{  H263ReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoH263_c::H263ReadPictureHeader(void)
{
    FrameParserStatus_t         Status;
    H263VideoPicture_t         *Header;
    H263VideoSequence_t        *SequenceHeader;
    unsigned int                Marker;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(H263VideoPicture_t));
    Header->version                             = Bits.Get(5);

    if (Header->version != 0x00)
    {
        SE_ERROR("Version %02x incorrect - should be 0x00\n", Header->version);
        return FrameParserError;
    }

    Header->tref                                = Bits.Get(8);                         // temporal reference 5.1.2
    Marker                                      = Bits.Get(2);

    if (Marker != 0x02)
    {
        SE_ERROR("Flags bytes %02x incorrect - should be 0x02\n", Marker);
        return FrameParserError;
    }

    // see 5.1.3
    Header->ssi                                 = Bits.Get(1);                         // split screen indicator
    Header->dci                                 = Bits.Get(1);                         // document camera indicator
    Header->fpr                                 = Bits.Get(1);                         // freeze picture release
    Header->format                              = Bits.Get(3);                         // source format
    Header->ptype                               = Bits.Get(1);
    Header->optional_bits                       = Bits.Get(4);

    // For H263 there is no sequence header so we create one whenever the picture size changes
    if ((StreamParameters == NULL) || (!StreamParameters->SequenceHeaderPresent) ||
        (Header->format != StreamParameters->SequenceHeader.format))
    {
        Status                                  = GetNewStreamParameters((void **)&StreamParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }

        StreamParameters->UpdatedSinceLastFrame = true;
        SequenceHeader                          = &StreamParameters->SequenceHeader;
        memset(SequenceHeader, 0, sizeof(H263VideoSequence_t));
        StreamParameters->SequenceHeaderPresent = true;
        Header->ptype                           = H263_PICTURE_CODING_TYPE_I;           // New sequence - set picture type to I (5.1.3)
    }
    else
    {
        SequenceHeader                          = &StreamParameters->SequenceHeader;
    }

    SequenceHeader->format                      = Header->format;
    SequenceHeader->width                       = H263DisplaySize[Header->format].Width;
    SequenceHeader->height                      = H263DisplaySize[Header->format].Height;
    Header->pquant                              = Bits.Get(5);                          // quantizer 5.1.4
    Header->cpm                                 = Bits.Get(1);                          // continuous presence multipoint 5.1.5
    Header->psbi                                = Bits.Get(2);                          // picture sub bitstream 5.1.6
    Header->trb                                 = Bits.Get(3);                          // temporal ref for b pictures 5.1.7
    Header->dbquant                             = Bits.Get(2);                          // quantization for b pictures 5.1.8

    while (Bits.Get(1))                                                                 // extra insertion info (pei) 5.1.9
    {
        Bits.Get(8);    // spare info (pspare) 5.1.10
    }

    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS
    {
        SE_INFO(group_frameparser_video,  "Picture header (%d):-\n", PictureNo++);
        SE_INFO(group_frameparser_video,  "    tref          : %6d\n", Header->tref);
        SE_INFO(group_frameparser_video,  "    ssi           : %6d\n", Header->ssi);
        SE_INFO(group_frameparser_video,  "    dci           : %6d\n", Header->dci);
        SE_INFO(group_frameparser_video,  "    fpr           : %6d\n", Header->fpr);
        SE_INFO(group_frameparser_video,  "    format        : %6d\n", Header->format);
        SE_INFO(group_frameparser_video,  "    width         : %6d\n", SequenceHeader->width);
        SE_INFO(group_frameparser_video,  "    height        : %6d\n", SequenceHeader->height);
        SE_INFO(group_frameparser_video,  "    ptype         : %6d\n", Header->ptype);
        SE_INFO(group_frameparser_video,  "    pquant        : %6d\n", Header->pquant);
        SE_INFO(group_frameparser_video,  "    cpm           : %6d\n", Header->cpm);
        SE_INFO(group_frameparser_video,  "    psbi          : %6d\n", Header->psbi);
        SE_INFO(group_frameparser_video,  "    trb           : %6d\n", Header->trb);
        SE_INFO(group_frameparser_video,  "    dbquant       : %6d\n", Header->dbquant);
    }
#endif
    return FrameParserNoError;
}
//}}}

//{{{  CommitFrameForDecode
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Send frame for decode
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoH263_c::CommitFrameForDecode(void)
{
    FrameParserStatus_t                 Status = FrameParserNoError;
    H263VideoPicture_t                 *PictureHeader;
    H263VideoSequence_t                *SequenceHeader;

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

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;
    // Record the stream and frame parameters into the appropriate structure
    // Parsed frame parameters
    ParsedFrameParameters->FirstParsedParametersForOutputFrame  = true;  //FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump  = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                  = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                = ContinuousReverseJump;
    ParsedFrameParameters->KeyFrame                             = PictureHeader->ptype == H263_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = true;
    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NumberOfReferenceFrameLists          = 1;
    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(H263StreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(H263FrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;
    // Parsed video parameters
    ParsedVideoParameters->Content.Width                        = SequenceHeader->width;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->height;
    ParsedVideoParameters->Content.DisplayWidth                 = SequenceHeader->width;
    ParsedVideoParameters->Content.DisplayHeight                = SequenceHeader->height;
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n",  ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;
    ParsedVideoParameters->Content.VideoFullRange               = false;                                // H263 conforms to ITU_R_BT601 for source
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;
    // Frame rate is always derived from 29.976
    StreamEncodedFrameRate                                      = Rational_t (30000, 1001);
    ParsedVideoParameters->Content.FrameRate                    = ResolveFrameRate();
    // Picture header cycles 0-255 with gaps - see 5.1.2
    ParsedVideoParameters->DisplayCount[0]                      = (TemporalReference == H263_TEMPORAL_REFERENCE_INVALID) ? 1 :
                                                                  (TemporalReference > PictureHeader->tref)              ? PictureHeader->tref + 256 - TemporalReference :
                                                                  PictureHeader->tref - TemporalReference;
    ParsedVideoParameters->DisplayCount[1]                      = 0;
    TemporalReference                                           = PictureHeader->tref;
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
bool   FrameParser_VideoH263_c::NewStreamParametersCheck(void)
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
    Different   = memcmp(&CopyOfStreamParameters, StreamParameters, sizeof(H263StreamParameters_t)) != 0;

    if (Different)
    {
        memcpy(&CopyOfStreamParameters, StreamParameters, sizeof(H263StreamParameters_t));
        return true;
    }

//
    return false;
}
//}}}
