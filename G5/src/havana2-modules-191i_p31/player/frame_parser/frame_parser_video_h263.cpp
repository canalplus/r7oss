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

Source file name : frame_parser_video_h263.cpp
Author :           Julian

Implementation of the h263 -1 video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
19-May-08   Created                                         Julian

************************************************************************/

#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "h263.h"
#include "frame_parser_video_h263.h"
#include "ring_generic.h"


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static BufferDataDescriptor_t     H263StreamParametersBuffer    = BUFFER_H263_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t     H263FrameParametersBuffer     = BUFFER_H263_FRAME_PARAMETERS_TYPE;

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

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

FrameParser_VideoH263_c::FrameParser_VideoH263_c (void)
{

    // Our constructor is called after our subclass so the only change is to rename the frame parser
    Configuration.FrameParserName               = "VideoH263";

    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &H263StreamParametersBuffer;

    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &H263FrameParametersBuffer;

    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    TemporalReference                           = H263_TEMPORAL_REFERENCE_INVALID;

#if defined (DUMP_HEADERS)
    PictureNo                                   = 0;
#endif

    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoH263_c::~FrameParser_VideoH263_c (void)
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
FrameParserStatus_t   FrameParser_VideoH263_c::Reset (void)
{
    memset (&CopyOfStreamParameters, 0x00, sizeof(H263StreamParameters_t));
    memset (&ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t));

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;

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
FrameParserStatus_t   FrameParser_VideoH263_c::RegisterOutputBufferRing (Ring_t          Ring)
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

FrameParserStatus_t   FrameParser_VideoH263_c::ResetReferenceFrameList( void )
{
    FrameParserStatus_t         Status  = FrameParser_Video_c::ResetReferenceFrameList ();

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
FrameParserStatus_t   FrameParser_VideoH263_c::PrepareReferenceFrameList (void)
{
    unsigned int               i;
    unsigned int               ReferenceFramesNeeded;
    unsigned int               PictureCodingType;
    H263VideoPicture_t*        PictureHeader;

    //
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.
    //

    PictureHeader               = &(((H263FrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->ptype;
    ReferenceFramesNeeded       = (PictureCodingType == H263_PICTURE_CODING_TYPE_P) ? 1 : 0;

    if (ReferenceFrameList.EntryCount < ReferenceFramesNeeded)
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesNeeded;

    for (i=0; i<ReferenceFramesNeeded; i++)
        ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesNeeded + i];

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
FrameParserStatus_t   FrameParser_VideoH263_c::ForPlayUpdateReferenceFrameList (void)
{
    if (ReferenceFrameList.EntryCount == 1)
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnReleaseReferenceFrame, ReferenceFrameList.EntryIndicies[0]);

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

FrameParserStatus_t   FrameParser_VideoH263_c::RevPlayProcessDecodeStacks (void)
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
FrameParserStatus_t   FrameParser_VideoH263_c::ReadHeaders (void)
{
    FrameParserStatus_t         Status  = FrameParserNoError;
    unsigned int                StartCode;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif

    Bits.SetPointer (BufferData);

    StartCode                   = Bits.Get(17);
    if (StartCode == H263_PICTURE_START_CODE)
        Status                  = H263ReadPictureHeader ();
    else
    {
        FRAME_ERROR("Not at the start of a picture - lost sync (%x).\n", StartCode);
        return FrameParserError;
    }

    if (Status == FrameParserNoError)
        Status      = CommitFrameForDecode();

    return Status;
}
//}}}
//{{{  H263ReadPictureHeader
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in a picture header
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoH263_c::H263ReadPictureHeader (void)
{
    FrameParserStatus_t         Status;
    H263VideoPicture_t*         Header;
    H263VideoSequence_t*        SequenceHeader;
    unsigned int                Marker;

    if (FrameParameters == NULL)
    {
        Status                                  = GetNewFrameParameters ((void **)&FrameParameters);
        if (Status != FrameParserNoError)
        return Status;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset (Header, 0x00, sizeof(H263VideoPicture_t));

    Header->version                             = Bits.Get(5);
    if (Header->version != 0x00)
    {
        FRAME_ERROR("Version %02x incorrect - should be 0x00.\n", Header->version);
        return FrameParserError;
    }


    Header->tref                                = Bits.Get(8);                         // temporal reference 5.1.2
    Marker                                      = Bits.Get(2);
    if (Marker != 0x02)
    {
        FRAME_ERROR("Flags bytes %02x incorrect - should be 0x02.\n", Marker);
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
        Status                                  = GetNewStreamParameters ((void **)&StreamParameters);
        if (Status != FrameParserNoError)
            return Status;
        StreamParameters->UpdatedSinceLastFrame = true;

        SequenceHeader                          = &StreamParameters->SequenceHeader;
        memset (SequenceHeader, 0x00, sizeof(H263VideoSequence_t));
        StreamParameters->SequenceHeaderPresent = true;

        Header->ptype                           = H263_PICTURE_CODING_TYPE_I;           // New sequence - set picture type to I (5.1.3)
    }
    else
        SequenceHeader                          = &StreamParameters->SequenceHeader;

    SequenceHeader->format                      = Header->format;
    SequenceHeader->width                       = H263DisplaySize[Header->format].Width;
    SequenceHeader->height                      = H263DisplaySize[Header->format].Height;

    Header->pquant                              = Bits.Get(5);                          // quantizer 5.1.4
    Header->cpm                                 = Bits.Get(1);                          // continuous presence multipoint 5.1.5
    Header->psbi                                = Bits.Get(2);                          // picture sub bitstream 5.1.6
    Header->trb                                 = Bits.Get(3);                          // temporal ref for b pictures 5.1.7
    Header->dbquant                             = Bits.Get(2);                          // quantization for b pictures 5.1.8

    while (Bits.Get(1))                                                                 // extra insertion info (pei) 5.1.9
        Bits.Get(8);                                                                    // spare info (pspare) 5.1.10

    FrameParameters->PictureHeaderPresent       = true;

#ifdef DUMP_HEADERS
    {
        report (severity_note, "Picture header (%d):- \n", PictureNo++);
        report (severity_info, "    tref          : %6d\n", Header->tref);
        report (severity_info, "    ssi           : %6d\n", Header->ssi);
        report (severity_info, "    dci           : %6d\n", Header->dci);
        report (severity_info, "    fpr           : %6d\n", Header->fpr);
        report (severity_info, "    format        : %6d\n", Header->format);
        report (severity_info, "    width         : %6d\n", SequenceHeader->width);
        report (severity_info, "    height        : %6d\n", SequenceHeader->height);
        report (severity_info, "    ptype         : %6d\n", Header->ptype);
        report (severity_info, "    pquant        : %6d\n", Header->pquant);
        report (severity_info, "    cpm           : %6d\n", Header->cpm);
        report (severity_info, "    psbi          : %6d\n", Header->psbi);
        report (severity_info, "    trb           : %6d\n", Header->trb);
        report (severity_info, "    dbquant       : %6d\n", Header->dbquant);
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
FrameParserStatus_t   FrameParser_VideoH263_c::CommitFrameForDecode (void)
{
    H263VideoPicture_t*                 PictureHeader;
    H263VideoSequence_t*                SequenceHeader;

    //
    // Check we have the headers we need
    //
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoH263_c::CommitFrameForDecode - Stream parameters unavailable for decode.\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        report (severity_error, "FrameParser_VideoH263_c::CommitFrameForDecode - Frame parameters unavailable for decode (%p).\n", FrameParameters);
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

    ParsedVideoParameters->Content.Progressive                  = true;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;

    ParsedVideoParameters->Content.VideoFullRange               = false;                                // H263 conforms to ITU_R_BT601 for source
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;

    // Frame rate is always derived from 29.976
    ParsedVideoParameters->Content.FrameRate                    = Rational_t (30000, 1001);
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
bool   FrameParser_VideoH263_c::NewStreamParametersCheck (void)
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

    Different   = memcmp (&CopyOfStreamParameters, StreamParameters, sizeof(H263StreamParameters_t)) != 0;
    if (Different)
    {
        memcpy (&CopyOfStreamParameters, StreamParameters, sizeof(H263StreamParameters_t));
        return true;
    }

//

    return false;
}
//}}}
