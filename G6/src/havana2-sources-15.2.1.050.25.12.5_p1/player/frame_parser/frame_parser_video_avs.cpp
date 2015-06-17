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
#include "frame_parser_video_avs.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_VideoAvs_c"
//{{{

static BufferDataDescriptor_t   AvsStreamParametersBuffer       = BUFFER_AVS_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t   AvsFrameParametersBuffer        = BUFFER_AVS_FRAME_PARAMETERS_TYPE;

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


#define AVS_MIN_LEGAL_CHROMA_FORMAT_CODE                1
#define AVS_MAX_LEGAL_CHROMA_FORMAT_CODE                2
#define AVS_ONLY_LEGAL_SAMPLE_PRECISION_CODE            1

static unsigned int     AvsAspectRatioValues[][2]       =
{
    {   1,   0 },   // not a valid ratio: AVS_MIN_LEGAL_ASPECT_RATIO_CODE is 1
    {   1,   1 },
    {   4,   3 },
    {  16,   9 },
    { 221, 100 }
};
#define AVS_MIN_LEGAL_ASPECT_RATIO_CODE                 1
#define AVS_MAX_LEGAL_ASPECT_RATIO_CODE                 4

#define AvsAspectRatios(N) Rational_t(AvsAspectRatioValues[N][0],AvsAspectRatioValues[N][1])

static unsigned int     AvsFrameRateValues[][2]         =
{
    {     0,    1 },
    { 24000, 1001 },
    {    24,    1 },
    {    25,    1 },
    { 30000, 1001 },
    {    30,    1 },
    {    50,    1 },
    { 60000, 1001 },
    {    60,    1 }
};
#define AVS_MIN_LEGAL_FRAME_RATE_CODE                   1
#define AVS_MAX_LEGAL_FRAME_RATE_CODE                   8

#define AvsFrameRates(N) Rational_t(AvsFrameRateValues[N][0],AvsFrameRateValues[N][1])

static SliceType_t SliceTypeTranslation[]  = { SliceTypeI, SliceTypeP, SliceTypeB };

static const unsigned int ReferenceFramesMinimum[AVS_PICTURE_CODING_TYPE_B + 1]   = {0, 1, 2};
static const unsigned int ReferenceFramesMaximum[AVS_PICTURE_CODING_TYPE_B + 1]   = {0, 2, 2};
//#define REFERENCE_FRAMES_NEEDED( CodingType )           (CodingType)
#define REFERENCE_FRAMES_REQUIRED(CodingType)                                   ReferenceFramesMinimum[CodingType]
#define REFERENCE_FRAMES_DESIRED(CodingType)                                    ReferenceFramesMaximum[CodingType]
//#define REFERENCE_FRAMES_DESIRED(CodingType)                                    ReferenceFramesMinimum[CodingType]
#define MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE                                 REFERENCE_FRAMES_DESIRED(AVS_PICTURE_CODING_TYPE_B)

//}}}

FrameParser_VideoAvs_c::FrameParser_VideoAvs_c(void)
    : FrameParser_Video_c()
    , CopyOfStreamParameters()
    , StreamParameters(NULL)
    , FrameParameters(NULL)
    , LastPanScanHorizontalOffset(0)
    , LastPanScanVerticalOffset(0)
    , EverSeenRepeatFirstField(false)
    , LastFirstFieldWasAnI(false)
    , PictureDistanceBase(0)
    , LastPictureDistanceBase(0)
    , LastPictureDistance(0)
    , LastReferenceFramePictureCodingType(AVS_PICTURE_CODING_TYPE_B)
    , ImgtrNextP(0)
    , ImgtrLastP(0)
    , ImgtrLastPrevP(0)
{
    Configuration.FrameParserName               = "VideoAvs";
    Configuration.StreamParametersCount         = 64;
    Configuration.StreamParametersDescriptor    = &AvsStreamParametersBuffer;
    Configuration.FrameParametersCount          = 64;
    Configuration.FrameParametersDescriptor     = &AvsFrameParametersBuffer;

    // dynamic config
    Configuration.SupportSmoothReversePlay      = true;
}

FrameParser_VideoAvs_c::~FrameParser_VideoAvs_c(void)
{
    Halt();
}

//{{{  Connect
// /////////////////////////////////////////////////////////////////////////
//
//      Method to connect to neighbor
//

FrameParserStatus_t   FrameParser_VideoAvs_c::Connect(Port_c *Port)
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

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadHeaders(void)
{
    unsigned int                Code;
    unsigned int                ExtensionCode;
    FrameParserStatus_t         Status = FrameParserNoError;
    bool                        FrameReadyForDecode     = false;
    bool                        StreamDataValid         = false;
    ParsedFrameParameters->DataOffset                   = 0;

    for (unsigned int i = 0; i < StartCodeList->NumberOfStartCodes; i++)
    {
        Code                    = StartCodeList->StartCodes[i];
        Bits.SetPointer(BufferData + ExtractStartCodeOffset(Code));
        Bits.FlushUnseen(32);
        Status                  = FrameParserNoError;

        switch (ExtractStartCodeCode(Code))
        {
        case   AVS_I_PICTURE_START_CODE:
            //ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
            Status                                  = ReadPictureHeader(AVS_I_PICTURE_START_CODE);
            FrameReadyForDecode                     = (Status == FrameParserNoError);
            break;

        case   AVS_PB_PICTURE_START_CODE:
            //ParsedFrameParameters->DataOffset       = ExtractStartCodeOffset(Code);
            Status                                  = ReadPictureHeader(AVS_PB_PICTURE_START_CODE);
            FrameReadyForDecode                     = (Status == FrameParserNoError);
            break;

        case  AVS_VIDEO_SEQUENCE_START_CODE:
            //SE_INFO(group_frameparser_video, "VC1_SEQUENCE_START_CODE\n");
            Status                                  = ReadSequenceHeader();
            StreamDataValid                         = (Status == FrameParserNoError);
            break;

        case  AVS_EXTENSION_START_CODE:
            ExtensionCode                           = Bits.Get(4);

            switch (ExtensionCode)
            {
            case  AVS_SEQUENCE_DISPLAY_EXTENSION_ID:
                if (!StreamDataValid)
                {
                    break;
                }

                Status                          = ReadSequenceDisplayExtensionHeader();
                break;

            case  AVS_COPYRIGHT_EXTENSION_ID:
                if (!StreamDataValid)
                {
                    break;
                }

                Status                          = ReadCopyrightExtensionHeader();
                break;

            case  AVS_CAMERA_PARAMETERS_EXTENSION_ID:
                if (!StreamDataValid)
                {
                    break;
                }

                Status                          = ReadCameraParametersExtensionHeader();
                break;

            case  AVS_PICTURE_DISPLAY_EXTENSION_ID:
                if (!FrameReadyForDecode)
                {
                    break;
                }

                Status                          = ReadPictureDisplayExtensionHeader();
                break;

            default:
                break;
            }

        case  AVS_USER_DATA_START_CODE:
            break;

        case AVS_VIDEO_EDIT_CODE:
        case AVS_VIDEO_SEQUENCE_END_CODE:
            break;

        default:
            if (ExtractStartCodeCode(Code) <= AVS_GREATEST_SLICE_START_CODE)
            {
                // coverity fix: removed check (ExtractStartCodeCode(Code) >= AVS_FIRST_SLICE_START_CODE)
                SE_ASSERT(AVS_FIRST_SLICE_START_CODE == 0);

                if (!FrameReadyForDecode)
                {
                    break;
                }

                // Build a list of the slices in this frame, recording an entry for each
                // SLICE_START_CODE as needed by the AVS decoder.
                Status                              = ReadSliceHeader(i);
                break;
            }
            else
            {
                SE_ERROR("Unknown/Unsupported header %02x\n", ExtractStartCodeCode(Code));
                Status                              = FrameParserUnhandledHeader;
                break;
            }
        }

        if ((Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader))
        {
            StreamDataValid             = false;
            FrameReadyForDecode         = false;
        }
    }

    // Finished processing all the start codes, send the frame to be decoded.
    if (FrameReadyForDecode)
    {
        Status          = CommitFrameForDecode();
    }

    return Status;
}
//}}}

//{{{  ReadSequenceHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence header 7.1.2.1
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSequenceHeader(void)
{
    FrameParserStatus_t                 Status  = FrameParserNoError;
    AvsVideoSequence_t                 *Header;
    SE_DEBUG(group_frameparser_video, "\n");
    Status      = GetNewStreamParameters((void **)&StreamParameters);

    if (Status != FrameParserNoError)
    {
        return Status;
    }

    StreamParameters->UpdatedSinceLastFrame = true;
    Header = &StreamParameters->SequenceHeader;
    memset(Header, 0, sizeof(AvsVideoSequence_t));
    Header->profile_id                  = Bits.Get(8);
    Header->level_id                    = Bits.Get(8);
    Header->progressive_sequence        = Bits.Get(1);    // 1=progressive 0=may contain both
    Header->horizontal_size             = Bits.Get(14);
    Header->vertical_size               = Bits.Get(14);
    Header->chroma_format               = Bits.Get(2);
    Header->sample_precision            = Bits.Get(3);
    Header->aspect_ratio                = Bits.Get(4);
    Header->frame_rate_code             = Bits.Get(4);
    Header->bit_rate_lower              = Bits.Get(18);
    MarkerBit(1);
    Header->bit_rate_upper              = Bits.Get(12);
    Header->low_delay                   = Bits.Get(1);
    MarkerBit(1);
    Header->bbv_buffer_size             = Bits.Get(18);
    Status              =  FrameParserHeaderSyntaxError;

    if (!inrange(Header->chroma_format, AVS_MIN_LEGAL_CHROMA_FORMAT_CODE, AVS_MAX_LEGAL_CHROMA_FORMAT_CODE))
    {
        SE_ERROR("Illegal chroma format code (%02x)\n", Header->chroma_format);
    }
    else if (!inrange(Header->aspect_ratio, AVS_MIN_LEGAL_ASPECT_RATIO_CODE, AVS_MAX_LEGAL_ASPECT_RATIO_CODE))
    {
        SE_ERROR("Illegal aspect ratio code (%02x)\n", Header->aspect_ratio);
    }
    else if (!inrange(Header->frame_rate_code, AVS_MIN_LEGAL_FRAME_RATE_CODE, AVS_MAX_LEGAL_FRAME_RATE_CODE))
    {
        SE_ERROR("Illegal frame rate code (%02x)\n", Header->frame_rate_code);
    }
    else
    {
        Status                                  =  FrameParserNoError;
        StreamParameters->SequenceHeaderPresent = true;
    }

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Sequence header :-\n");
    SE_INFO(group_frameparser_video, "    profile_id                      : %6d\n", Header->profile_id);
    SE_INFO(group_frameparser_video, "    level_id                        : %6d\n", Header->level_id);
    SE_INFO(group_frameparser_video, "    progressive_sequence            : %6d\n", Header->progressive_sequence);
    SE_INFO(group_frameparser_video, "    horizontal_size                 : %6d\n", Header->horizontal_size);
    SE_INFO(group_frameparser_video, "    vertical_size                   : %6d\n", Header->vertical_size);
    SE_INFO(group_frameparser_video, "    chroma_format                   : %6d\n", Header->chroma_format);
    SE_INFO(group_frameparser_video, "    sample_precision                : %6d\n", Header->sample_precision);
    SE_INFO(group_frameparser_video, "    aspect_ratio                    : %6d\n", Header->aspect_ratio);
    SE_INFO(group_frameparser_video, "    frame_rate_code                 : %6d\n", Header->frame_rate_code);
    SE_INFO(group_frameparser_video, "    bit_rate_lower                  : %6d\n", Header->bit_rate_lower);
    SE_INFO(group_frameparser_video, "    bit_rate_upper                  : %6d\n", Header->bit_rate_upper);
    SE_INFO(group_frameparser_video, "    low_delay                       : %6d\n", Header->low_delay);
    SE_INFO(group_frameparser_video, "    bbv_buffer_size                 : %6d\n", Header->bbv_buffer_size);
#endif
    return Status;
}
//}}}
//{{{  ReadSequenceDisplayExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence display extension header 7.1.2.3
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSequenceDisplayExtensionHeader(void)
{
    AvsVideoSequenceDisplayExtension_t    *Header;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceDisplayExtensionHeader;
    memset(Header, 0, sizeof(AvsVideoSequenceDisplayExtension_t));
    Header->video_format                        = Bits.Get(3);
    Header->sample_range                        = Bits.Get(1);
    Header->color_description                   = Bits.Get(1);

    if (Header->color_description != 0)
    {
        Header->color_primaries                 = Bits.Get(8);
        Header->transfer_characteristics        = Bits.Get(8);
        Header->matrix_coefficients             = Bits.Get(8);
    }

    Header->display_horizontal_size             = Bits.Get(14);
    MarkerBit(1);
    Header->display_vertical_size               = Bits.Get(14);
    StreamParameters->SequenceDisplayExtensionHeaderPresent     = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Sequence Display Extension header :-\n");
    SE_INFO(group_frameparser_video, "    video_format                 : %6d\n", Header->video_format);

    if (Header->color_description != 0)
    {
        SE_INFO(group_frameparser_video, "    color_primaries              : %6d\n", Header->color_primaries);
        SE_INFO(group_frameparser_video, "    transfer_characteristics     : %6d\n", Header->transfer_characteristics);
        SE_INFO(group_frameparser_video, "    matrix_coefficients          : %6d\n", Header->matrix_coefficients);
    }

    SE_INFO(group_frameparser_video, "    display_horizontal_size      : %6d\n", Header->display_horizontal_size);
    SE_INFO(group_frameparser_video, "    display_vertical_size        : %6d\n", Header->display_vertical_size);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadCopyrightExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in extension data 7.1.2.4
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadCopyrightExtensionHeader(void)
{
    AvsVideoCopyrightExtension_t   *Header;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->CopyrightExtensionHeader;
    memset(Header, 0, sizeof(AvsVideoCopyrightExtension_t));
    Header->copyright_flag                      = Bits.Get(1);
    Header->copyright_id                        = Bits.Get(8);
    Header->original_or_copy                    = Bits.Get(1);
    Bits.Get(7);                                 // reserved bits
    MarkerBit(1);
    Header->copyright_number_1                  = Bits.Get(20);
    MarkerBit(1);
    Header->copyright_number_2                  = Bits.Get(22);
    MarkerBit(1);
    Header->copyright_number_3                  = Bits.Get(22);
    StreamParameters->CopyrightExtensionHeaderPresent   = true;
#ifdef DUMP_HEADER
    SE_INFO(group_frameparser_video, "Copyright extension header\n");
    SE_INFO(group_frameparser_video, "    copyright_flag                             : %6d\n", Header->copyright_flag);
    SE_INFO(group_frameparser_video, "    copyright_id                               : %6d\n", Header->copyright_id);
    SE_INFO(group_frameparser_video, "    original_or_copy                           : %6d\n", Header->original_or_copy);
    SE_INFO(group_frameparser_video, "    copyright_number_1                         : %6d\n", Header->copyright_number_1);
    SE_INFO(group_frameparser_video, "    copyright_number_2                         : %6d\n", Header->copyright_number_2);
    SE_INFO(group_frameparser_video, "    copyright_number_3                         : %6d\n", Header->copyright_number_3);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadCameraParametersExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in extension data
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadCameraParametersExtensionHeader(void)
{
    AvsVideoCameraParametersExtension_t        *Header;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->CameraParametersExtensionHeader;
    memset(Header, 0, sizeof(AvsVideoCameraParametersExtension_t));
    Bits.Get(1);                        // reserved bits
    Header->camera_id                           = Bits.Get(7);
    MarkerBit(1);
    Header->height_of_image_device              = Bits.Get(22);
    MarkerBit(1);
    Header->focal_length                        = Bits.Get(22);
    MarkerBit(1);
    Header->f_number                            = Bits.Get(22);
    MarkerBit(1);
    Header->vertical_angle_of_view              = Bits.Get(22);
    MarkerBit(1);
    Header->camera_position_x_upper             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_position_x_lower             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_position_y_upper             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_position_y_lower             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_position_z_upper             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_position_z_lower             = Bits.Get(16);
    MarkerBit(1);
    Header->camera_direction_x                  = Bits.Get(22);
    MarkerBit(1);
    Header->camera_direction_y                  = Bits.Get(22);
    MarkerBit(1);
    Header->camera_direction_z                  = Bits.Get(22);
    MarkerBit(1);
    Header->image_plane_vertical_x              = Bits.Get(22);
    MarkerBit(1);
    Header->image_plane_vertical_y              = Bits.Get(22);
    MarkerBit(1);
    Header->image_plane_vertical_z              = Bits.Get(22);
    MarkerBit(1);
    StreamParameters->CameraParametersExtensionHeaderPresent    = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Camera parameters extension headern");
    SE_INFO(group_frameparser_video, "    camera_id                          : %6d\n", Header->camera_id);
    SE_INFO(group_frameparser_video, "    height_of_image_device             : %6d\n", Header->height_of_image_device);
    SE_INFO(group_frameparser_video, "    focal_length                       : %6d\n", Header->focal_length);
    SE_INFO(group_frameparser_video, "    f_number                           : %6d\n", Header->f_number);
    SE_INFO(group_frameparser_video, "    vertical_angle_of_view             : %6d\n", Header->vertical_angle_of_view);
    SE_INFO(group_frameparser_video, "    camera_position_x_upper            : %6d\n", Header->camera_position_x_upper);
    SE_INFO(group_frameparser_video, "    camera_position_x_lower            : %6d\n", Header->camera_position_x_lower);
    SE_INFO(group_frameparser_video, "    camera_position_y_upper            : %6d\n", Header->camera_position_y_upper);
    SE_INFO(group_frameparser_video, "    camera_position_y_lower            : %6d\n", Header->camera_position_y_lower);
    SE_INFO(group_frameparser_video, "    camera_position_z_upper            : %6d\n", Header->camera_position_z_upper);
    SE_INFO(group_frameparser_video, "    camera_position_z_lower            : %6d\n", Header->camera_position_z_lower);
    SE_INFO(group_frameparser_video, "    camera_direction_x                 : %6d\n", Header->camera_direction_x);
    SE_INFO(group_frameparser_video, "    camera_direction_y                 : %6d\n", Header->camera_direction_y);
    SE_INFO(group_frameparser_video, "    camera_direction_z                 : %6d\n", Header->camera_direction_z);
    SE_INFO(group_frameparser_video, "    image_plane_vertical_x             : %6d\n", Header->image_plane_vertical_x);
    SE_INFO(group_frameparser_video, "    image_plane_vertical_y             : %6d\n", Header->image_plane_vertical_y);
    SE_INFO(group_frameparser_video, "    image_plane_vertical_z             : %6d\n", Header->image_plane_vertical_z);
#endif
    return FrameParserNoError;
}
//}}}

//{{{  ReadPictureHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture header
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadPictureHeader(unsigned int PictureStartCode)
{
    FrameParserStatus_t         Status;
    AvsVideoPictureHeader_t    *Header;
    AvsVideoSequence_t         *SequenceHeader;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    SequenceHeader                              = &StreamParameters->SequenceHeader;

    if (FrameParameters == NULL)
    {
        Status  = GetNewFrameParameters((void **)&FrameParameters);

        if (Status != FrameParserNoError)
        {
            return Status;
        }
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset(Header, 0, sizeof(AvsVideoPictureHeader_t));
    Header->bbv_delay                           = Bits.Get(16);

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
    {
        Header->picture_coding_type             = AVS_PICTURE_CODING_TYPE_I;

        if (Bits.Get(1) == 1)
        {
            Header->time_code                   = Bits.Get(24);
        }

        if (Bits.Get(1) != 1)
        {
            SE_DEBUG(group_frameparser_video, "Marker bit not available for Codec_Version_no < 521/rm52h\n");
        }
    }
    else
    {
        Header->picture_coding_type             = Bits.Get(2);
    }

    Header->picture_distance                    = Bits.Get(8);

    if (SequenceHeader->low_delay == 1)
    {
        Header->bbv_check_times                 = Bits.GetUe();
    }

    Header->progressive_frame                   = Bits.Get(1);

    if (Header->progressive_frame == 0)
    {
        Header->picture_structure               = Bits.Get(1);

        if ((PictureStartCode != AVS_I_PICTURE_START_CODE) && (Header->picture_structure == 0))
        {
            Header->advanced_pred_mode_disable  = Bits.Get(1);
        }

        //if (SequenceHeader->progressive_sequence == 1)
        //    Header->picture_structure           = 1;
    }
    else
    {
        Header->picture_structure               = 1;
    }

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
    {
        Header->picture_structure_bwd           = Header->picture_structure;
    }

    Header->top_field_first                     = Bits.Get(1);
    Header->repeat_first_field                  = Bits.Get(1);
    Header->fixed_picture_qp                    = Bits.Get(1);
    Header->picture_qp                          = Bits.Get(6);

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
    {
        if ((Header->progressive_frame == 0) && (Header->picture_structure == 0))
        {
            Header->skip_mode_flag              = Bits.Get(1);
        }

        Bits.Get(4);
    }
    else
    {
        if ((Header->picture_coding_type == AVS_PICTURE_CODING_TYPE_P) && (Header->picture_structure == 1))
        {
            Header->picture_reference_flag      = Bits.Get(1);
        }

        Header->no_forward_reference_flag       = Bits.Get(1);
        Bits.Get(3);
        Header->skip_mode_flag                  = Bits.Get(1);
    }

    Header->loop_filter_disable                 = Bits.Get(1);

    if (!Header->loop_filter_disable)
    {
        Header->loop_filter_parameter_flag      = Bits.Get(1);

        if (Header->loop_filter_parameter_flag)
        {
            Header->alpha_c_offset              = Bits.GetSe();
            Header->beta_offset                 = Bits.GetSe();
        }
    }

    //Calculate the POC, tr, PictureDistanceBase
    if (Header->picture_distance < LastPictureDistance && (LastPictureDistance - Header->picture_distance) >= (AVS_PICTURE_DISTANCE_HALF_RANGE))
    {
        PictureDistanceBase = LastPictureDistanceBase + AVS_PICTURE_DISTANCE_RANGE;
    }
    else if (Header->picture_distance > LastPictureDistance && (Header->picture_distance - LastPictureDistance) > (AVS_PICTURE_DISTANCE_HALF_RANGE))
    {
        PictureDistanceBase = LastPictureDistanceBase - AVS_PICTURE_DISTANCE_RANGE;
    }
    else
    {
        PictureDistanceBase = LastPictureDistanceBase;
    }

    Header->picture_order_count = PictureDistanceBase + Header->picture_distance;
    Header->tr                  = Header->picture_order_count;

    if (Header->picture_coding_type != AVS_PICTURE_CODING_TYPE_B)
    {
        Header->imgtr_last_prev_P               = ImgtrLastP;
        Header->imgtr_last_P                    = ImgtrNextP;
        Header->imgtr_next_P                    = Header->tr;
        ImgtrLastPrevP                          = ImgtrLastP;
        ImgtrLastP                              = ImgtrNextP;
        ImgtrNextP                              = Header->tr;
        LastPictureDistance                     = Header->picture_distance;
        LastPictureDistanceBase                 = PictureDistanceBase;
    }
    else
    {
        Header->imgtr_last_prev_P               = ImgtrLastPrevP;
        Header->imgtr_last_P                    = ImgtrLastP;
        Header->imgtr_next_P                    = ImgtrNextP;
    }

    Header->ReversePlay                         = PlaybackDirection == PlayBackward;
    FrameParameters->PictureHeaderPresent       = true;
#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Picture header :-\n");
    SE_INFO(group_frameparser_video, "    bbv_delay                       : %6d\n", Header->bbv_delay);
    SE_INFO(group_frameparser_video, "    picture_coding_type             : %6d\n", Header->picture_coding_type);
    SE_INFO(group_frameparser_video, "    time_code_flag                  : %6d\n", Header->time_code_flag);
    SE_INFO(group_frameparser_video, "    time_code                       : %6d\n", Header->time_code);
    SE_INFO(group_frameparser_video, "    picture_distance                : %6d\n", Header->picture_distance);
    SE_INFO(group_frameparser_video, "    bbv_check_times                 : %6d\n", Header->bbv_check_times);
    SE_INFO(group_frameparser_video, "    progressive_frame               : %6d\n", Header->progressive_frame);
    SE_INFO(group_frameparser_video, "    picture_structure               : %6d\n", Header->picture_structure);
    SE_INFO(group_frameparser_video, "    advanced_pred_mode_disable      : %6d\n", Header->advanced_pred_mode_disable);
    SE_INFO(group_frameparser_video, "    top_field_first                 : %6d\n", Header->top_field_first);
    SE_INFO(group_frameparser_video, "    repeat_first_field              : %6d\n", Header->repeat_first_field);
    SE_INFO(group_frameparser_video, "    fixed_picture_qp                : %6d\n", Header->fixed_picture_qp);
    SE_INFO(group_frameparser_video, "    picture_qp                      : %6d\n", Header->picture_qp);
    SE_INFO(group_frameparser_video, "    picture_reference_flag          : %6d\n", Header->picture_reference_flag);
    SE_INFO(group_frameparser_video, "    no_forward_reference_flag       : %6d\n", Header->no_forward_reference_flag);
    SE_INFO(group_frameparser_video, "    skip_mode_flag                  : %6d\n", Header->skip_mode_flag);
    SE_INFO(group_frameparser_video, "    loop_filter_disable             : %6d\n", Header->loop_filter_disable);
    SE_INFO(group_frameparser_video, "    loop_filter_parameter_flag      : %6d\n", Header->loop_filter_parameter_flag);
    SE_INFO(group_frameparser_video, "    alpha_c_offset                  : %6d\n", Header->alpha_c_offset);
    SE_INFO(group_frameparser_video, "    beta_offset                     : %6d\n", Header->beta_offset);
    SE_INFO(group_frameparser_video, "    picture_order_count             : %6d\n", Header->picture_order_count);
    SE_INFO(group_frameparser_video, "    tr                              : %6d\n", Header->tr);
    SE_INFO(group_frameparser_video, "    imgtr_next_P                    : %6d\n", Header->imgtr_next_P);
    SE_INFO(group_frameparser_video, "    imgtr_last_P                    : %6d\n", Header->imgtr_last_P);
    SE_INFO(group_frameparser_video, "    imgtr_last_prev_P               : %6d\n", Header->imgtr_last_prev_P);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureDisplayExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture display extension
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadPictureDisplayExtensionHeader(void)
{
    AvsVideoPictureDisplayExtension_t  *Header;
    AvsVideoSequence_t                 *SequenceHeader;
    AvsVideoPictureHeader_t            *PictureHeader;
    SE_DEBUG(group_frameparser_video, "\n");

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        SE_ERROR("Appropriate sequence header not found\n");
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        SE_ERROR("Appropriate picture header not found\n");
        return FrameParserNoStreamParameters;
    }

    Header                                      = &FrameParameters->PictureDisplayExtensionHeader;
    memset(Header, 0, sizeof(AvsVideoPictureDisplayExtension_t));
    SequenceHeader                              = &StreamParameters->SequenceHeader;
    PictureHeader                               = &FrameParameters->PictureHeader;

    //{{{  Work out number of frame centre offsets
    if (SequenceHeader->progressive_sequence == 1)
    {
        if (PictureHeader->repeat_first_field == 1)
        {
            if (PictureHeader->top_field_first == 1)
            {
                Header->number_of_frame_centre_offsets          = 3;
            }
            else
            {
                Header->number_of_frame_centre_offsets          = 2;
            }
        }
        else
        {
            Header->number_of_frame_centre_offsets              = 1;
        }
    }
    else
    {
        if (PictureHeader->picture_structure == 1)
        {
            Header->number_of_frame_centre_offsets              = 1;
        }
        else
        {
            if (PictureHeader->repeat_first_field == 1)
            {
                Header->number_of_frame_centre_offsets          = 3;
            }
            else
            {
                Header->number_of_frame_centre_offsets          = 2;
            }
        }
    }

    //}}}

    for (unsigned int i = 0; i < Header->number_of_frame_centre_offsets; i++)
    {
        Header->frame_centre[i].horizontal_offset               = Bits.Get(16);
        MarkerBit(1);
        Header->frame_centre[i].vertical_offset                 = Bits.Get(16);
        MarkerBit(1);
    }

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Picture display extension header :\n");
    SE_INFO(group_frameparser_video, "    number_of_frame_centre_offsets                     : %6d\n", Header->number_of_frame_centre_offsets);

    for (unsigned int i = 0; i < Header->number_of_frame_centre_offsets; i++)
    {
        SE_INFO(group_frameparser_video, "    frame_centre[%d].horizontal_offset                  : %6d\n", i, Header->frame_centre[i].horizontal_offset);
        SE_INFO(group_frameparser_video, "    frame_centre[%d].vertical_offset                    : %6d\n", i, Header->frame_centre[i].vertical_offset);
    }

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
FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSliceHeader(unsigned int StartCodeIndex)
{
    AvsVideoPictureHeader_t    *PictureHeader;
    AvsVideoSequence_t         *SequenceHeader;
    int                         SliceCount;
    AvsVideoSlice_t            *Slice;
    unsigned int                Code;
    unsigned int                SliceCode;
    unsigned int                SliceOffset;
    unsigned int                MacroblockRows;
    SE_DEBUG(group_frameparser_video, "\n");

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

    SliceCount                          = FrameParameters->SliceHeaderList.no_slice_headers;
    Slice                               = &FrameParameters->SliceHeaderList.slice_array[SliceCount];
    Code                                = StartCodeList->StartCodes[StartCodeIndex];
    SliceCode                           = ExtractStartCodeCode(Code);
    SliceOffset                         = ExtractStartCodeOffset(Code);
    Slice->slice_start_code             = SliceCode;
    Slice->slice_offset                 = SliceOffset;
    FrameParameters->SliceHeaderList.no_slice_headers++;
    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;

    if (SliceCount == 0)
    {
        PictureHeader->top_field_offset         = SliceOffset;// - ParsedFrameParameters->DataOffset;      // As an offset from the picture start
        PictureHeader->bottom_field_offset      = SliceOffset;//PictureHeader->top_field_offset;
    }

    MacroblockRows                      = ((SequenceHeader->vertical_size + 15) / 16) >> 1;

    if ((SliceCode >= MacroblockRows) && (PictureHeader->bottom_field_offset == 0))
    {
        PictureHeader->bottom_field_offset      = SliceOffset;    // - ParsedFrameParameters->DataOffset;
    }

#ifdef DUMP_HEADERS
    SE_INFO(group_frameparser_video, "Slice header :-\n");
    SE_INFO(group_frameparser_video, "Slice start code               %6d\n", SliceCode);
    SE_INFO(group_frameparser_video, "Slice offset                   %6d\n", SliceOffset);
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
FrameParserStatus_t   FrameParser_VideoAvs_c::CommitFrameForDecode(void)
{
    AvsVideoPictureHeader_t            *PictureHeader;
    AvsVideoSequence_t                 *SequenceHeader;
    PictureStructure_t                  PictureStructure;
    bool                                ProgressiveSequence;
    bool                                RepeatFirstField;
    bool                                TopFieldFirst;
    bool                                Frame;
    //bool                                FieldSequenceError;
    unsigned int                        PanAndScanCount;
    MatrixCoefficientsType_t            MatrixCoefficients;
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
    // Obtain and check the progressive etc values.
    ProgressiveSequence         = SequenceHeader->progressive_sequence;
    // All pictures are frame pictures picture
#if 0

    if ((SequenceHeader->progressive_sequence == 1) || (PictureHeader->progressive_frame == 1))
    {
        PictureStructure        = StructureFrame;
    }
    else if (PictureHeader->picture_structure == 0)
    {
        PictureStructure        = StructureTopField;
    }
    else
    {
        PictureStructure        = StructureBottomField;
    }

#else
    PictureStructure            = StructureFrame;
#endif
    Frame                       = PictureStructure == StructureFrame;
    RepeatFirstField            = PictureHeader->repeat_first_field;
    TopFieldFirst               = PictureHeader->top_field_first;

    if (FrameParameters->PictureDisplayExtensionHeaderPresent)
    {
        PanAndScanCount         = FrameParameters->PictureDisplayExtensionHeader.number_of_frame_centre_offsets;
    }
    else
    {
        PanAndScanCount         = 0;
    }

    if (!Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField))
    {
        SE_ERROR("Illegal combination (%c %c %c %c)\n",
                 (ProgressiveSequence    ? 'T' : 'F'),
                 (Frame                  ? 'T' : 'F'),
                 (RepeatFirstField       ? 'T' : 'F'),
                 (TopFieldFirst          ? 'T' : 'F'));
        return FrameParserHeaderSyntaxError;
    }

    // Deduce the matrix coefficients for colour conversions.
    if ((StreamParameters->SequenceDisplayExtensionHeaderPresent) && (StreamParameters->SequenceDisplayExtensionHeader.color_description == 1))
    {
        switch (StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients)
        {
        case AVS_MATRIX_COEFFICIENTS_BT709:         MatrixCoefficients      = MatrixCoefficients_ITU_R_BT709;       break;

        case AVS_MATRIX_COEFFICIENTS_FCC:           MatrixCoefficients      = MatrixCoefficients_FCC;               break;

        case AVS_MATRIX_COEFFICIENTS_BT470_BGI:     MatrixCoefficients      = MatrixCoefficients_ITU_R_BT470_2_BG;  break;

        case AVS_MATRIX_COEFFICIENTS_SMPTE_170M:    MatrixCoefficients      = MatrixCoefficients_SMPTE_170M;        break;

        case AVS_MATRIX_COEFFICIENTS_SMPTE_240M:    MatrixCoefficients      = MatrixCoefficients_SMPTE_240M;        break;

        default:
        case AVS_MATRIX_COEFFICIENTS_FORBIDDEN:
        case AVS_MATRIX_COEFFICIENTS_RESERVED:
            SE_ERROR("Forbidden or reserved matrix coefficient code specified (%02x)\n", StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients);

        // fallthrough (TBC) or return error ?

        case AVS_MATRIX_COEFFICIENTS_UNSPECIFIED:   MatrixCoefficients      = MatrixCoefficients_ITU_R_BT601;               break;
        }
    }
    else
    {
        MatrixCoefficients = MatrixCoefficients_ITU_R_BT601;
    }

    ParsedFrameParameters->FirstParsedParametersForOutputFrame          = FirstDecodeOfFrame;
    ParsedFrameParameters->FirstParsedParametersAfterInputJump          = FirstDecodeAfterInputJump;
    ParsedFrameParameters->SurplusDataInjected                          = SurplusDataInjected;
    ParsedFrameParameters->ContinuousReverseJump                        = ContinuousReverseJump;
    //
    // Record the stream and frame parameters into the appropriate structure
    //
    ParsedFrameParameters->KeyFrame                             = PictureHeader->picture_coding_type == AVS_PICTURE_CODING_TYPE_I;
    ParsedFrameParameters->ReferenceFrame                       = PictureHeader->picture_coding_type != AVS_PICTURE_CODING_TYPE_B;
    ParsedFrameParameters->IndependentFrame                     = ParsedFrameParameters->KeyFrame;
    ParsedFrameParameters->NewStreamParameters                  = NewStreamParametersCheck();
    ParsedFrameParameters->SizeofStreamParameterStructure       = sizeof(AvsStreamParameters_t);
    ParsedFrameParameters->StreamParameterStructure             = StreamParameters;
    ParsedFrameParameters->NewFrameParameters                   = true;
    ParsedFrameParameters->SizeofFrameParameterStructure        = sizeof(AvsFrameParameters_t);
    ParsedFrameParameters->FrameParameterStructure              = FrameParameters;
    ParsedVideoParameters->Content.Width                        = SequenceHeader->horizontal_size;
    ParsedVideoParameters->Content.Height                       = SequenceHeader->vertical_size;

    if (StreamParameters->SequenceDisplayExtensionHeaderPresent)
    {
        ParsedVideoParameters->Content.DisplayWidth             = StreamParameters->SequenceDisplayExtensionHeader.display_horizontal_size;
        ParsedVideoParameters->Content.DisplayHeight            = StreamParameters->SequenceDisplayExtensionHeader.display_vertical_size;
    }
    else
    {
        ParsedVideoParameters->Content.DisplayWidth             = ParsedVideoParameters->Content.Width;
        ParsedVideoParameters->Content.DisplayHeight            = ParsedVideoParameters->Content.Height;
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

    Status = CheckForResolutionConstraints(ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);

    if (Status != FrameParserNoError)
    {
        SE_ERROR("Unsupported resolution %d x %d\n", ParsedVideoParameters->Content.Width, ParsedVideoParameters->Content.Height);
        return Status;
    }

    StreamEncodedFrameRate                  = AvsFrameRates(StreamParameters->SequenceHeader.frame_rate_code);
    ParsedVideoParameters->Content.FrameRate                    = ResolveFrameRate();
    ParsedVideoParameters->Content.VideoFullRange               = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients;
    ParsedVideoParameters->Content.Progressive                  = ProgressiveSequence;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;
    ParsedVideoParameters->Content.PixelAspectRatio             = AvsAspectRatios(SequenceHeader->aspect_ratio);

    if (ParsedVideoParameters->Content.PixelAspectRatio != Rational_t(1, 1))
    {
        // Aspect ratio convertion is managed by VIBE.
        // Convert to height/width similar to mpeg2
        ParsedVideoParameters->Content.PixelAspectRatio = (ParsedVideoParameters->Content.PixelAspectRatio * ParsedVideoParameters->Content.Height) / ParsedVideoParameters->Content.Width;
    }

    ParsedVideoParameters->SliceType                            = SliceTypeTranslation[PictureHeader->picture_coding_type];
    ParsedVideoParameters->PictureStructure                     = PictureStructure;
    ParsedVideoParameters->InterlacedFrame                      = !PictureHeader->progressive_frame;
    ParsedVideoParameters->TopFieldFirst                        = TopFieldFirst;
    ParsedVideoParameters->DisplayCount[0]                      = DisplayCount0(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    ParsedVideoParameters->DisplayCount[1]                      = DisplayCount1(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

    if (!PictureHeader->progressive_frame && !PictureHeader->picture_structure)
    {
        SE_ERROR("AVS Interlaced field picture stream - Not currently supported\n");
        Stream->MarkUnPlayable();
    }

    ParsedVideoParameters->PanScanCount                         = PanAndScanCount;

    for (unsigned int i = 0; i < PanAndScanCount; i++)
    {
        // The dimensions of the pan-scan display rectangle are extracted from the Sequence Display Extension header
        ParsedVideoParameters->PanScan[i].Width                 = ParsedVideoParameters->Content.DisplayWidth;
        ParsedVideoParameters->PanScan[i].Height                = ParsedVideoParameters->Content.DisplayHeight;

        // The frame_centre offsets are 16-bit signed integers, giving the position of the centre of the display rectangle (in units of 1/16th sample),
        // compared to the centre of the reconstructed frame. A positive value indicates that the centre of the reconstructed frame lies to the right
        // of the centre of the display rectangle.
        ParsedVideoParameters->PanScan[i].HorizontalOffset      = 16 * (ParsedVideoParameters->Content.Width - ParsedVideoParameters->Content.DisplayWidth) / 2 -
                                                                  FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].horizontal_offset;
        ParsedVideoParameters->PanScan[i].VerticalOffset        = 16 * (ParsedVideoParameters->Content.Height - ParsedVideoParameters->Content.DisplayHeight) / 2 -
                                                                  FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].vertical_offset;

        // DisplayCount is always 1 since there are as much pan-scan data as fields to be rendered
        ParsedVideoParameters->PanScan[i].DisplayCount          = 1;
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
    FrameToDecode               = true;
    //FrameToDecode               = PictureHeader->picture_coding_type != AVS_PICTURE_CODING_TYPE_B;
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
bool   FrameParser_VideoAvs_c::NewStreamParametersCheck(void)
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
    Different   = memcmp(&CopyOfStreamParameters, StreamParameters, sizeof(AvsStreamParameters_t)) != 0;

    if (Different)
    {
        memcpy(&CopyOfStreamParameters, StreamParameters, sizeof(AvsStreamParameters_t));
        return true;
    }

//
    return false;
}
//}}}

//{{{  PrepareReferenceFrameList
/// /////////////////////////////////////////////////////////////////////////
///
/// \brief      Stream specific function to prepare a reference frame list
///
/// /////////////////////////////////////////////////////////////////////////
FrameParserStatus_t   FrameParser_VideoAvs_c::PrepareReferenceFrameList(void)
{
    unsigned int                        i;
    unsigned int                        ReferenceFramesRequired;
    unsigned int                        ReferenceFramesDesired;
    unsigned int                        PictureCodingType;
    AvsVideoPictureHeader_t            *PictureHeader;
    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequence header or group of pictures
    // header which belong to the next frame.
    PictureHeader               = &(((AvsFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->picture_coding_type;
    ReferenceFramesRequired     = REFERENCE_FRAMES_REQUIRED(PictureCodingType);
    ReferenceFramesDesired      = REFERENCE_FRAMES_DESIRED(PictureCodingType);
    SE_DEBUG(group_frameparser_video, "ReferenceFrameList.EntryCount %d, Picture type = %d\n", ReferenceFrameList.EntryCount, PictureCodingType);

    // Check for sufficient reference frames.  We cannot decode otherwise
    if (ReferenceFrameList.EntryCount < ReferenceFramesRequired)
    {
        return FrameParserInsufficientReferenceFrames;
    }

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesDesired;

    if (ReferenceFrameList.EntryCount < ReferenceFramesDesired)
    {
        //Rare underflow case: when (ReferenceFrameList.EntryCount < ReferenceFramesDesired); Helpful in ERC.
        for (i = 0; i < ReferenceFrameList.EntryCount; i++)
        {
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[i];
        }

        for (i = ReferenceFrameList.EntryCount; i < ReferenceFramesDesired; i++)
        {
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - 1];
        }
    }
    else
    {
        //Generic case: when (ReferenceFrameList.EntryCount == ReferenceFramesDesired)
        //Rare overflow case: when (ReferenceFrameList.EntryCount > ReferenceFramesDesired)
        for (i = 0; i < ReferenceFramesDesired; i++)
        {
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesDesired + i];
        }
    }

    //SE_INFO(group_frameparser_video, "Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
    //        ReferenceFrameList.EntryIndicies[0], ReferenceFrameList.EntryIndicies[1],
    //        ReferenceFramesDesired, ReferenceFrameList.EntryCount, ReferenceFrameList.EntryCount - ReferenceFramesDesired);

    if (ParsedFrameParameters->ReferenceFrame)
    {
        LastReferenceFramePictureCodingType     = PictureCodingType;
    }

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
FrameParserStatus_t   FrameParser_VideoAvs_c::ForPlayUpdateReferenceFrameList(void)
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
//}}}

//{{{  RevPlayProcessDecodeStacks
// /////////////////////////////////////////////////////////////////////////
//
//      Stream specific override function for processing decode stacks, this
//      initializes the post decode ring before passing itno the real
//      implementation of this function.
//

FrameParserStatus_t   FrameParser_VideoAvs_c::RevPlayProcessDecodeStacks(void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();
    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}

