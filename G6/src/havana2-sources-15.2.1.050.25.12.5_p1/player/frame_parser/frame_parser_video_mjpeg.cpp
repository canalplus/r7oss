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
#include "frame_parser_video_mjpeg.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoMjpeg_c"

static BufferDataDescriptor_t   MjpegStreamParametersBuffer       = BUFFER_MJPEG_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t   MjpegFrameParametersBuffer        = BUFFER_MJPEG_FRAME_PARAMETERS_TYPE;

//
FrameParser_VideoMjpeg_c::FrameParser_VideoMjpeg_c(void)
    : FrameParser_Video_c()
    , CopyOfStreamParameters()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , GetDefaultStreamMetadata(true)
{
    Configuration.FrameParserName               = "VideoMjpeg";
    Configuration.StreamParametersCount         = 32;
    Configuration.StreamParametersDescriptor    = &MjpegStreamParametersBuffer;
    Configuration.FrameParametersCount          = 32;
    Configuration.FrameParametersDescriptor     = &MjpegFrameParametersBuffer;
}

//
FrameParser_VideoMjpeg_c::~FrameParser_VideoMjpeg_c(void)
{
    Halt();
}

//{{{  Connect
// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::Connect(Port_c *Port)
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
//{{{  ReadHeaders
// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadHeaders(void)
{
    FrameParserStatus_t         Status          = FrameParserNoError;
    unsigned int                Code;
    bool                        StartOfImageSeen;
    ParsedFrameParameters->DataOffset   = 0;
    StartOfImageSeen                    = false;

    for (unsigned int i = 0; i < StartCodeList->NumberOfStartCodes; i++)
    {
        Code                            = StartCodeList->StartCodes[i];
        Bits.SetPointer(BufferData + ExtractStartCodeOffset(Code));
        Bits.FlushUnseen(16);
        Status                          = FrameParserNoError;

        switch (ExtractStartCodeCode(Code))
        {
        case MJPEG_SOI:
            ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
            StartOfImageSeen                        = true;
            break;

        case  MJPEG_SOF_0:               // Read the start of frame header
        case  MJPEG_SOF_1:
            if (!StartOfImageSeen)
            {
                SE_ERROR("No Start Of Image seen\n");
                return FrameParserError;
            }

            // Get default Time parameters (scale & delta) for ES streams
            if (GetDefaultStreamMetadata)
            {
                Status = ReadStreamMetadata();
                GetDefaultStreamMetadata = false;
            }

            Status                  = ReadStartOfFrame();

            if (Status == FrameParserNoError)
            {
                return CommitFrameForDecode();
            }

            break;

        case MJPEG_APP_0:
            Status                  = ReadAPP0Marker();
            break;

        case MJPEG_APP_15:
            GetDefaultStreamMetadata        = false;
            Status                  = ReadStreamMetadata();
            break;

        default:
            break;
        }

        if (Status != FrameParserNoError)
        {
            IncrementErrorStatistics(Status);
        }
    }

    return FrameParserNoError;
}
//}}}

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadAPP0Marker(void)
{
    FrameParserStatus_t  Status = FrameParserNoError;
    char data[4];
    int ByteCnt, Field_Flag;

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
        memset(&FrameParameters->PictureHeader, 0, sizeof(FrameParameters->PictureHeader));
    }

    MjpegVideoPictureHeader_t  *Header = &FrameParameters->PictureHeader;

    Bits.Get(16); //just the header length; ignore it

    for (ByteCnt = 0; ByteCnt < 4; ByteCnt++)
    {
        data[ByteCnt] = Bits.Get(8);
    }
    if ((data[0] == 'A') && (data[1] == 'V') && (data[2] == 'I') && (data[3] == '1'))
    {
        Field_Flag = Bits.Get(8);
        if (Field_Flag == 0x01)     /* FIRST field */
        {
            Header->field_flag = FIRST_FIELD;
        }
        else if (Field_Flag == 0x02)    /* SECOND field */
        {
            Header->field_flag = SECOND_FIELD;
        }
        else                        /* Frame */
        {
            Header->field_flag = FRAME;
        }
    }
    else
    {
        SE_WARNING("\n AVI1 code Not found! assuming Frame JPEG picture\n");
        Header->field_flag = FRAME;
    }

    SE_VERBOSE(group_frameparser_video, "Header->field_flag = %d\n", Header->field_flag);

    return Status;
}

//{{{  ReadStreamMetadata
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Read in stream generic information
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadStreamMetadata(void)
{
    MjpegVideoSequence_t       *Header;
    char                        VendorId[64];
    FrameParserStatus_t         Status          = FrameParserNoError;

    if (!GetDefaultStreamMetadata)
    {
        for (unsigned int i = 0; i < sizeof(VendorId); i++)
        {
            VendorId[i]             = Bits.Get(8);

            if (VendorId[i] == 0)
            {
                break;
            }
        }

        if (strcmp(VendorId, "STMicroelectronics") != 0)
        {
            SE_INFO(group_frameparser_video, "    VendorId          : %s\n", VendorId);
            return FrameParserNoError;
        }
    }

    Status                      = GetNewStreamParameters((void **)&StreamParameters);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    StreamParameters->UpdatedSinceLastFrame     = true;
    Header                      = &StreamParameters->SequenceHeader;
    memset(Header, 0, sizeof(MjpegVideoSequence_t));

    if (!GetDefaultStreamMetadata)
    {
        Header->time_scale          = Bits.Get(32);
        Header->time_delta          = Bits.Get(32);
        SE_INFO(group_frameparser_video, "1    Header->time_scale = %d, Header->time_delta  = %d\n", Header->time_scale, Header->time_delta);
    }
    else
    {
        Header->time_scale          = 25000;
        Header->time_delta          = 1000;
        SE_INFO(group_frameparser_video, " 2   Header->time_scale = %d, Header->time_delta  = %d\n", Header->time_scale, Header->time_delta);
    }

    StreamParameters->SequenceHeaderPresent             = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "StreamMetadata :-\n");
    SE_INFO(group_frameparser_video, "    VendorId          : %s\n", VendorId);
    SE_INFO(group_frameparser_video, "    time_scale        : %6d\n", Header->time_scale);
    SE_INFO(group_frameparser_video, "    time_delta        : %6d\n", Header->time_delta);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadStartOfFrame
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read the start of frame header
//

FrameParserStatus_t   FrameParser_VideoMjpeg_c::ReadStartOfFrame(void)
{
    FrameParserStatus_t         Status;

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
        memset(&FrameParameters->PictureHeader, 0, sizeof(FrameParameters->PictureHeader));
    }

    MjpegVideoPictureHeader_t  *Header          = &FrameParameters->PictureHeader;
    Header->length                              = Bits.Get(16);
    Header->sample_precision                    = Bits.Get(8);

    if (Header->field_flag != FRAME)
    {
        // Field stream would require twice the height
        Header->frame_height                        = Bits.Get(16) * 2;
    }
    else
    {
        Header->frame_height                        = Bits.Get(16);
    }

    Header->frame_width                         = Bits.Get(16);
    Header->number_of_components                = Bits.Get(8);

    if (Header->number_of_components >= MJPEG_MAX_COMPONENTS)
    {
        SE_ERROR("Found more than supported number of components (%d)\n", Header->number_of_components);
        return FrameParserError;
    }

    for (unsigned int i = 0; i < Header->number_of_components; i++)
    {
        Header->components[i].id                                = Bits.Get(8);
        Header->components[i].vertical_sampling_factor          = Bits.Get(4);
        Header->components[i].horizontal_sampling_factor        = Bits.Get(4);
        Header->components[i].quantization_table_index          = Bits.Get(8);
    }

    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video,  "Start of frame header:\n");
    SE_INFO(group_frameparser_video,  "        Length              %d\n", Header->length);
    SE_INFO(group_frameparser_video,  "        Precision           %d\n", Header->sample_precision);
    SE_INFO(group_frameparser_video,  "        FrameHeight         %d\n", Header->frame_height);
    SE_INFO(group_frameparser_video,  "        FrameWidth          %d\n", Header->frame_width);
    SE_INFO(group_frameparser_video,  "        NumberOfComponents  %d\n", Header->number_of_components);

    for (unsigned int i = 0; i < Header->number_of_components; i++)
        SE_INFO(group_frameparser_video,  "            Id = %d, HSF = %d, VSF = %d, QTI = %d\n",
                Header->components[i].id,
                Header->components[i].horizontal_sampling_factor,
                Header->components[i].vertical_sampling_factor,
                Header->components[i].quantization_table_index);

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
FrameParserStatus_t   FrameParser_VideoMjpeg_c::CommitFrameForDecode(void)
{
    MjpegVideoPictureHeader_t          *PictureHeader;
    MjpegVideoSequence_t               *SequenceHeader;
    FrameParserStatus_t                 Status = FrameParserNoError;
    SE_DEBUG(group_frameparser_video, "\n");

    // Check we have the headers we need
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Stream parameters unavailable for decode\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Frame parameters unavailable for decode\n");
        return FrameParserPartialFrameParameters;
    }

    if (Buffer == NULL)
    {
        // Basic check: before attach stream/frame param to Buffer
        SE_ERROR("No current buffer to commit to decode\n");
        return FrameParserError;
    }

    SequenceHeader              = &StreamParameters->SequenceHeader;
    PictureHeader               = &FrameParameters->PictureHeader;
    // make mjpeg struggle through
    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    //
    // Record the stream and frame parameters into the appropriate structure
    //
    ParsedFrameParameters->KeyFrame                             = true;
    ParsedFrameParameters->ReferenceFrame                       = false;
    ParsedFrameParameters->IndependentFrame                     = true;
    ParsedFrameParameters->NewStreamParameters                  = false;
    StreamParameters->UpdatedSinceLastFrame                     = false;
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(MjpegStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(MjpegFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;
    ParsedVideoParameters->Content.Width                        = PictureHeader->frame_width;
    ParsedVideoParameters->Content.Height                       = PictureHeader->frame_height;
    ParsedVideoParameters->Content.DisplayWidth                 = PictureHeader->frame_width;
    ParsedVideoParameters->Content.DisplayHeight                = PictureHeader->frame_height;
    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    if (SequenceHeader->time_delta)
    {
        StreamEncodedFrameRate = Rational_t(SequenceHeader->time_scale, SequenceHeader->time_delta);
    }
    else
    {
        StreamEncodedFrameRate = INVALID_FRAMERATE;
    }

    ParsedVideoParameters->Content.FrameRate                    = ResolveFrameRate();
    ParsedVideoParameters->Content.VideoFullRange               = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients_ITU_R_BT601;
    ParsedVideoParameters->Content.Progressive                  = (PictureHeader->field_flag == FRAME);
    ParsedVideoParameters->Content.OverscanAppropriate          = false;
    ParsedVideoParameters->Content.PixelAspectRatio             = 1;

    if (PictureHeader->field_flag == FIRST_FIELD)
    {
        ParsedVideoParameters->PictureStructure = StructureTopField;
    }
    else if (PictureHeader->field_flag == SECOND_FIELD)
    {
        ParsedVideoParameters->PictureStructure = StructureBottomField;
    }
    else
    {
        ParsedVideoParameters->PictureStructure = StructureFrame;
    }

    ParsedVideoParameters->InterlacedFrame                      = !(ParsedVideoParameters->Content.Progressive);
    ParsedVideoParameters->DisplayCount[0]                      = 1;
    ParsedVideoParameters->DisplayCount[1]                      = 0;
    ParsedVideoParameters->PanScanCount                         = 0;
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
    FrameToDecode               = true;
    //FrameToDecode               = PictureHeader->picture_coding_type != MJPEG_PICTURE_CODING_TYPE_B;
    return FrameParserNoError;
}
//}}}

