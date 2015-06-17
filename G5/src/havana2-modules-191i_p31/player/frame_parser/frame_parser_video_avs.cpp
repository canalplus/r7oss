/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video_avs.cpp
Author :           Julian

Implementation of the avs video frame parser class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-06-09    Created                                         Julian

************************************************************************/

//#define DUMP_HEADERS

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video_avs.h"
#include "ring_generic.h"

//{{{  locally defined constants and macros
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static BufferDataDescriptor_t   AvsStreamParametersBuffer       = BUFFER_AVS_STREAM_PARAMETERS_TYPE;
static BufferDataDescriptor_t   AvsFrameParametersBuffer        = BUFFER_AVS_FRAME_PARAMETERS_TYPE;

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


#define AVS_MIN_LEGAL_CHROMA_FORMAT_CODE                1
#define AVS_MAX_LEGAL_CHROMA_FORMAT_CODE                2
#define AVS_ONLY_LEGAL_SAMPLE_PRECISION_CODE            1

static unsigned int     AvsAspectRatioValues[][2]       =
{
    {   1,   0 },
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

//

static SliceType_t SliceTypeTranslation[]  = { SliceTypeI, SliceTypeP, SliceTypeB };

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//

static const unsigned int ReferenceFramesMinimum[AVS_PICTURE_CODING_TYPE_B+1]   = {0, 1, 2};
static const unsigned int ReferenceFramesMaximum[AVS_PICTURE_CODING_TYPE_B+1]   = {0, 2, 2};
//#define REFERENCE_FRAMES_NEEDED( CodingType )           (CodingType)
#define REFERENCE_FRAMES_REQUIRED(CodingType)                                   ReferenceFramesMinimum[CodingType]
#define REFERENCE_FRAMES_DESIRED(CodingType)                                    ReferenceFramesMaximum[CodingType]
//#define REFERENCE_FRAMES_DESIRED(CodingType)                                    ReferenceFramesMinimum[CodingType]
#define MAX_REFERENCE_FRAMES_FOR_FORWARD_DECODE                                 REFERENCE_FRAMES_DESIRED(AVS_PICTURE_CODING_TYPE_B)

//}}}
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor
//

FrameParser_VideoAvs_c::FrameParser_VideoAvs_c( void )
{
    Configuration.FrameParserName               = "VideoAvs";

    Configuration.StreamParametersCount         = 64;
    Configuration.StreamParametersDescriptor    = &AvsStreamParametersBuffer;

    Configuration.FrameParametersCount          = 64;
    Configuration.FrameParametersDescriptor     = &AvsFrameParametersBuffer;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor
//

FrameParser_VideoAvs_c::~FrameParser_VideoAvs_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

FrameParserStatus_t   FrameParser_VideoAvs_c::Reset(  void )
{

    Configuration.SupportSmoothReversePlay      = true;

    memset( &CopyOfStreamParameters, 0x00, sizeof(AvsStreamParameters_t) );
    memset( &ReferenceFrameList, 0x00, sizeof(ReferenceFrameList_t) );

    StreamParameters                            = NULL;
    FrameParameters                             = NULL;
    DeferredParsedFrameParameters               = NULL;
    DeferredParsedVideoParameters               = NULL;

    FirstDecodeOfFrame                          = true;
    EverSeenRepeatFirstField                    = false;

    LastFirstFieldWasAnI                        = false;
    LastReferenceFramePictureCodingType         = AVS_PICTURE_CODING_TYPE_B;

#if 0
    PicDistanceMsb                              = 0;
    PrevPicDistanceLsb                          = 0;
    CurrPicDistanceMsb                          = 0;
    PicOrderCount                               = 0;
#else
    PictureDistanceBase                         = 0;
    LastPictureDistance                         = 0;
#endif
    ImgtrLastPrevP                              = 0;
    ImgtrLastP                                  = 0;
    ImgtrNextP                                  = 0;

    return FrameParser_Video_c::Reset();
}
//}}}

//{{{  RegisterOutputBufferRing
// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function
//

FrameParserStatus_t   FrameParser_VideoAvs_c::RegisterOutputBufferRing( Ring_t Ring )
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

    return FrameParser_Video_c::RegisterOutputBufferRing( Ring );
}
//}}}
//{{{  ReadHeaders
// /////////////////////////////////////////////////////////////////////////
//
//      The read headers stream specific function
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadHeaders( void )
{
    unsigned int                Code;
    unsigned int                ExtensionCode;
    FrameParserStatus_t         Status = FrameParserNoError;
    bool                        FrameReadyForDecode     = false;
    bool                        StreamDataValid         = false;

#if 0
    unsigned int                i;
    report (severity_info, "First 32 bytes of %d :", BufferLength);
    for (i=0; i<32; i++)
        report (severity_info, " %02x", BufferData[i]);
    report (severity_info, "\n");
#endif
#if 0
    report (severity_info, "Start codes (%d):", StartCodeList->NumberOfStartCodes);
    for (int i=0; i<StartCodeList->NumberOfStartCodes; i++)
        report (severity_info, " %02x(%d)", ExtractStartCodeCode(StartCodeList->StartCodes[i]), ExtractStartCodeOffset(StartCodeList->StartCodes[i]));
    report (severity_info, "\n");
#endif

    ParsedFrameParameters->DataOffset                   = 0;
    for (unsigned int i=0; i<StartCodeList->NumberOfStartCodes; i++)
    {
        Code                    = StartCodeList->StartCodes[i];
        Bits.SetPointer (BufferData + ExtractStartCodeOffset(Code));
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
                //FRAME_TRACE("VC1_SEQUENCE_START_CODE\n");
                Status                                  = ReadSequenceHeader();
                StreamDataValid                         = (Status == FrameParserNoError);
                break;

            case  AVS_EXTENSION_START_CODE:
                ExtensionCode                           = Bits.Get(4);
                switch (ExtensionCode)
                {
                    case  AVS_SEQUENCE_DISPLAY_EXTENSION_ID:
                        if (!StreamDataValid)
                            break;

                        Status                          = ReadSequenceDisplayExtensionHeader();
                        break;

                    case  AVS_COPYRIGHT_EXTENSION_ID:
                        if (!StreamDataValid)
                            break;

                        Status                          = ReadCopyrightExtensionHeader();
                        break;

                    case  AVS_CAMERA_PARAMETERS_EXTENSION_ID:
                        if (!StreamDataValid)
                            break;

                        Status                          = ReadCameraParametersExtensionHeader();
                        break;

                    case  AVS_PICTURE_DISPLAY_EXTENSION_ID:
                        if (!FrameReadyForDecode)
                            break;

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
                if ((ExtractStartCodeCode(Code) >= AVS_FIRST_SLICE_START_CODE) && (ExtractStartCodeCode(Code) <= AVS_GREATEST_SLICE_START_CODE))
                {
                    if (!FrameReadyForDecode)
                        break;

                #if 0
                    Status                              = CalculateFieldOffsets (i);
                    Status                              = CommitFrameForDecode ();
                    FreameReadyForDecode                = false;
                #else
                    // Build a list of the slices in this frame, recording an entry for each
                    // SLICE_START_CODE as needed by the AVS decoder.
                    Status                              = ReadSliceHeader(i);
                #endif

                    break;
                }
                else
                {
                    FRAME_ERROR("%s - Unknown/Unsupported header %02x\n", __FUNCTION__, ExtractStartCodeCode(Code));
                    Status                              = FrameParserUnhandledHeader;
                    break;
                }
        }

        if( (Status != FrameParserNoError) && (Status != FrameParserUnhandledHeader) )
        {
            StreamDataValid             = false;
            FrameReadyForDecode         = false;
        }
    }

    // Finished processing all the start codes, send the frame to be decoded.
    if (FrameReadyForDecode)
        Status          = CommitFrameForDecode();

    return Status;
}
//}}}

//{{{  ReadSequenceHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence header 7.1.2.1
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSequenceHeader( void )
{
    FrameParserStatus_t                 Status  = FrameParserNoError;
    AvsVideoSequence_t*                 Header;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    Status      = GetNewStreamParameters( (void **)&StreamParameters );
    if( Status != FrameParserNoError )
        return Status;

    StreamParameters->UpdatedSinceLastFrame = true;

    Header = &StreamParameters->SequenceHeader;
    memset( Header, 0x00, sizeof(AvsVideoSequence_t) );

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
        FRAME_ERROR("%s - Illegal chroma format code (%02x).\n", __FUNCTION__, Header->chroma_format);
    else if (!inrange(Header->aspect_ratio, AVS_MIN_LEGAL_ASPECT_RATIO_CODE, AVS_MAX_LEGAL_ASPECT_RATIO_CODE))
        FRAME_ERROR("%s - Illegal aspect ratio code (%02x).\n", __FUNCTION__, Header->aspect_ratio);
    else if (!inrange(Header->frame_rate_code, AVS_MIN_LEGAL_FRAME_RATE_CODE, AVS_MAX_LEGAL_FRAME_RATE_CODE))
        FRAME_ERROR("%s - Illegal frame rate code (%02x).\n", __FUNCTION__, Header->frame_rate_code );
    else
    {
        Status                                  =  FrameParserNoError;
        StreamParameters->SequenceHeaderPresent = true;
    }

#ifdef DUMP_HEADERS
    FRAME_TRACE("Sequence header :- \n" );
    FRAME_TRACE("    profile_id                      : %6d\n", Header->profile_id);
    FRAME_TRACE("    level_id                        : %6d\n", Header->level_id);
    FRAME_TRACE("    progressive_sequence            : %6d\n", Header->progressive_sequence);
    FRAME_TRACE("    horizontal_size                 : %6d\n", Header->horizontal_size);
    FRAME_TRACE("    vertical_size                   : %6d\n", Header->vertical_size);

    FRAME_TRACE("    chroma_format                   : %6d\n", Header->chroma_format);
    FRAME_TRACE("    sample_precision                : %6d\n", Header->sample_precision);
    FRAME_TRACE("    aspect_ratio                    : %6d\n", Header->aspect_ratio);
    FRAME_TRACE("    frame_rate_code                 : %6d\n", Header->frame_rate_code);
    FRAME_TRACE("    bit_rate_lower                  : %6d\n", Header->bit_rate_lower);
    FRAME_TRACE("    bit_rate_upper                  : %6d\n", Header->bit_rate_upper);
    FRAME_TRACE("    low_delay                       : %6d\n", Header->low_delay);
    FRAME_TRACE("    bbv_buffer_size                 : %6d\n", Header->bbv_buffer_size);
#endif

    return Status;
}
//}}}
//{{{  ReadSequenceDisplayExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a sequence display extension header 7.1.2.3
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSequenceDisplayExtensionHeader( void )
{
    AvsVideoSequenceDisplayExtension_t    *Header;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate sequence header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->SequenceDisplayExtensionHeader;
    memset( Header, 0x00, sizeof(AvsVideoSequenceDisplayExtension_t) );

    Header->video_format                        = Bits.Get(3);
    Header->sample_range                        = Bits.Get(1);
    Header->color_description                   = Bits.Get(1);
    if( Header->color_description != 0 )
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
    FRAME_TRACE("Sequence Display Extension header :- \n" );
    FRAME_TRACE("    video_format                 : %6d\n", Header->video_format );
    if( Header->color_description != 0 )
    {
        FRAME_TRACE("    color_primaries              : %6d\n", Header->color_primaries );
        FRAME_TRACE("    transfer_characteristics     : %6d\n", Header->transfer_characteristics );
        FRAME_TRACE("    matrix_coefficients          : %6d\n", Header->matrix_coefficients );
    }
    FRAME_TRACE("    display_horizontal_size      : %6d\n", Header->display_horizontal_size );
    FRAME_TRACE("    display_vertical_size        : %6d\n", Header->display_vertical_size );
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadCopyrightExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in extension data 7.1.2.4
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadCopyrightExtensionHeader( void )
{
    AvsVideoCopyrightExtension_t   *Header;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate sequence header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->CopyrightExtensionHeader;
    memset( Header, 0x00, sizeof(AvsVideoCopyrightExtension_t) );

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
    FRAME_TRACE("Copyright extension header\n");
    FRAME_TRACE("    copyright_flag                             : %6d\n", Header->copyright_flag);
    FRAME_TRACE("    copyright_id                               : %6d\n", Header->copyright_id);
    FRAME_TRACE("    original_or_copy                           : %6d\n", Header->original_or_copy);
    FRAME_TRACE("    copyright_number_1                         : %6d\n", Header->copyright_number_1);
    FRAME_TRACE("    copyright_number_2                         : %6d\n", Header->copyright_number_2);
    FRAME_TRACE("    copyright_number_3                         : %6d\n", Header->copyright_number_3);
#endif
    return FrameParserNoError;
}
//}}}
//{{{  ReadCameraParametersExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in extension data
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadCameraParametersExtensionHeader( void )
{
    AvsVideoCameraParametersExtension_t*        Header;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate sequence header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    Header      = &StreamParameters->CameraParametersExtensionHeader;
    memset( Header, 0x00, sizeof(AvsVideoCameraParametersExtension_t) );

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
    FRAME_TRACE("Camera parameters extension headern");
    FRAME_TRACE("    camera_id                          : %6d\n", Header->camera_id);
    FRAME_TRACE("    height_of_image_device             : %6d\n", Header->height_of_image_device);
    FRAME_TRACE("    focal_length                       : %6d\n", Header->focal_length);
    FRAME_TRACE("    f_number                           : %6d\n", Header->f_number);
    FRAME_TRACE("    vertical_angle_of_view             : %6d\n", Header->vertical_angle_of_view);
    FRAME_TRACE("    camera_position_x_upper            : %6d\n", Header->camera_position_x_upper);
    FRAME_TRACE("    camera_position_x_lower            : %6d\n", Header->camera_position_x_lower);
    FRAME_TRACE("    camera_position_y_upper            : %6d\n", Header->camera_position_y_upper);
    FRAME_TRACE("    camera_position_y_lower            : %6d\n", Header->camera_position_y_lower);
    FRAME_TRACE("    camera_position_z_upper            : %6d\n", Header->camera_position_z_upper);
    FRAME_TRACE("    camera_position_z_lower            : %6d\n", Header->camera_position_z_lower);
    FRAME_TRACE("    camera_direction_x                 : %6d\n", Header->camera_direction_x);
    FRAME_TRACE("    camera_direction_y                 : %6d\n", Header->camera_direction_y);
    FRAME_TRACE("    camera_direction_z                 : %6d\n", Header->camera_direction_z);
    FRAME_TRACE("    image_plane_vertical_x             : %6d\n", Header->image_plane_vertical_x);
    FRAME_TRACE("    image_plane_vertical_y             : %6d\n", Header->image_plane_vertical_y);
    FRAME_TRACE("    image_plane_vertical_z             : %6d\n", Header->image_plane_vertical_z);
#endif

    return FrameParserNoError;
}
//}}}

//{{{  ReadPictureHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture header
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadPictureHeader (unsigned int PictureStartCode)
{
    FrameParserStatus_t         Status;
    AvsVideoPictureHeader_t*    Header;
    AvsVideoSequence_t*         SequenceHeader;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate sequence header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }
    SequenceHeader                              = &StreamParameters->SequenceHeader;

    if( FrameParameters == NULL )
    {
        Status  = GetNewFrameParameters( (void **)&FrameParameters );
        if( Status != FrameParserNoError )
            return Status;
    }

    Header                                      = &FrameParameters->PictureHeader;
    memset( Header, 0x00, sizeof(AvsVideoPictureHeader_t) );

    Header->bbv_delay                           = Bits.Get(16);

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
    {
        Header->picture_coding_type             = AVS_PICTURE_CODING_TYPE_I;
        if (Bits.Get(1) == 1)
            Header->time_code                   = Bits.Get(24);

        MarkerBit(1);
    }
    else
        Header->picture_coding_type             = Bits.Get(2);

    Header->picture_distance                    = Bits.Get(8);

    if (SequenceHeader->low_delay == 1)
        Header->bbv_check_times                 = Bits.GetUe();

    Header->progressive_frame                   = Bits.Get(1);
    if (Header->progressive_frame == 0)
    {
        Header->picture_structure               = Bits.Get(1);
        if ((PictureStartCode != AVS_I_PICTURE_START_CODE) && (Header->picture_structure == 0))
            Header->advanced_pred_mode_disable  = Bits.Get(1);
        //if (SequenceHeader->progressive_sequence == 1)
        //    Header->picture_structure           = 1;
    }
    else
        Header->picture_structure               = 1;

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
        Header->picture_structure_bwd           = Header->picture_structure;

    Header->top_field_first                     = Bits.Get(1);
    Header->repeat_first_field                  = Bits.Get(1);
    Header->fixed_picture_qp                    = Bits.Get(1);
    Header->picture_qp                          = Bits.Get(6);

    if (PictureStartCode == AVS_I_PICTURE_START_CODE)
    {
        if ((Header->progressive_frame == 0) && ( Header->picture_structure == 0))
            Header->skip_mode_flag              = Bits.Get(1);

        Bits.Get(4);
    }
    else
    {
        if ((Header->picture_coding_type == AVS_PICTURE_CODING_TYPE_P) && (Header->picture_structure == 1))
            Header->picture_reference_flag      = Bits.Get(1);

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

    if ((int)Header->picture_distance < (int)(LastPictureDistance - AVS_PICTURE_DISTANCE_HALF_RANGE))
        PictureDistanceBase                    += AVS_PICTURE_DISTANCE_RANGE;
    else if (Header->picture_distance > (LastPictureDistance + AVS_PICTURE_DISTANCE_HALF_RANGE))
        PictureDistanceBase                    -= AVS_PICTURE_DISTANCE_RANGE;
    LastPictureDistance                         = Header->picture_distance;

    Header->picture_order_count                 = PictureDistanceBase + Header->picture_distance;
    Header->tr                                  = Header->picture_order_count;

    if (Header->picture_coding_type != AVS_PICTURE_CODING_TYPE_B)
    {
        Header->imgtr_last_prev_P               = ImgtrLastP;
        Header->imgtr_last_P                    = ImgtrNextP;
        Header->imgtr_next_P                    = Header->tr;
        ImgtrLastPrevP                          = ImgtrLastP;
        ImgtrLastP                              = ImgtrNextP;
        ImgtrNextP                              = Header->tr;
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
    FRAME_TRACE("Picture header :- \n" );
    FRAME_TRACE("    bbv_delay                       : %6d\n", Header->bbv_delay);
    FRAME_TRACE("    picture_coding_type             : %6d\n", Header->picture_coding_type);
    FRAME_TRACE("    time_code_flag                  : %6d\n", Header->time_code_flag);
    FRAME_TRACE("    time_code                       : %6d\n", Header->time_code);
    FRAME_TRACE("    picture_distance                : %6d\n", Header->picture_distance);
    FRAME_TRACE("    bbv_check_times                 : %6d\n", Header->bbv_check_times);
    FRAME_TRACE("    progressive_frame               : %6d\n", Header->progressive_frame);
    FRAME_TRACE("    picture_structure               : %6d\n", Header->picture_structure);
    FRAME_TRACE("    advanced_pred_mode_disable      : %6d\n", Header->advanced_pred_mode_disable);
    FRAME_TRACE("    top_field_first                 : %6d\n", Header->top_field_first);
    FRAME_TRACE("    repeat_first_field              : %6d\n", Header->repeat_first_field);
    FRAME_TRACE("    fixed_picture_qp                : %6d\n", Header->fixed_picture_qp);
    FRAME_TRACE("    picture_qp                      : %6d\n", Header->picture_qp);
    FRAME_TRACE("    picture_reference_flag          : %6d\n", Header->picture_reference_flag);
    FRAME_TRACE("    no_forward_reference_flag       : %6d\n", Header->no_forward_reference_flag);
    FRAME_TRACE("    skip_mode_flag                  : %6d\n", Header->skip_mode_flag);
    FRAME_TRACE("    loop_filter_disable             : %6d\n", Header->loop_filter_disable);
    FRAME_TRACE("    loop_filter_parameter_flag      : %6d\n", Header->loop_filter_parameter_flag);
    FRAME_TRACE("    alpha_c_offset                  : %6d\n", Header->alpha_c_offset);
    FRAME_TRACE("    beta_offset                     : %6d\n", Header->beta_offset);
    FRAME_TRACE("    picture_order_count             : %6d\n", Header->picture_order_count);
    FRAME_TRACE("    tr                              : %6d\n", Header->tr);
    FRAME_TRACE("    imgtr_next_P                    : %6d\n", Header->imgtr_next_P);
    FRAME_TRACE("    imgtr_last_P                    : %6d\n", Header->imgtr_last_P);
    FRAME_TRACE("    imgtr_last_prev_P               : %6d\n", Header->imgtr_last_prev_P);
#endif

    return FrameParserNoError;
}
//}}}
//{{{  ReadPictureDisplayExtensionHeader
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Read in a picture display extension
//

FrameParserStatus_t   FrameParser_VideoAvs_c::ReadPictureDisplayExtensionHeader( void )
{
    AvsVideoPictureDisplayExtension_t*  Header;
    AvsVideoSequence_t*                 SequenceHeader;
    AvsVideoPictureHeader_t*            PictureHeader;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    if( (StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate sequence header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    if( (FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent )
    {
        FRAME_ERROR("%s - Appropriate picture header not found.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    Header                                      = &FrameParameters->PictureDisplayExtensionHeader;
    memset( Header, 0x00, sizeof(AvsVideoPictureDisplayExtension_t) );

    SequenceHeader                              = &StreamParameters->SequenceHeader;
    PictureHeader                               = &FrameParameters->PictureHeader;

    //{{{  Work out number of frame centre offsets
    if (SequenceHeader->progressive_sequence == 1)
    {
        if (PictureHeader->repeat_first_field == 1)
        {
            if (PictureHeader->top_field_first == 1)
                Header->number_of_frame_centre_offsets          = 3;
            else
                Header->number_of_frame_centre_offsets          = 2;
        }
        else
            Header->number_of_frame_centre_offsets              = 1;
    }
    else
    {
        if (PictureHeader->picture_structure == 1)
            Header->number_of_frame_centre_offsets              = 1;
        else
        {
            if (PictureHeader->repeat_first_field == 1)
                Header->number_of_frame_centre_offsets          = 3;
            else
                Header->number_of_frame_centre_offsets          = 2;
        }
    }
    //}}}

    for (unsigned int i=0; i<Header->number_of_frame_centre_offsets; i++)
    {
        Header->frame_centre[i].horizontal_offset               = Bits.Get(16);
        MarkerBit(1);
        Header->frame_centre[i].vertical_offset                 = Bits.Get(16);
        MarkerBit(1);
    }

#ifdef DUMP_HEADERS
    FRAME_TRACE("Picture display extension header :\n");
    FRAME_TRACE("    number_of_frame_centre_offsets                     : %6d\n", Header->number_of_frame_centre_offsets);
    for (unsigned int i=0; i<Header->number_of_frame_centre_offsets; i++)
    {
        FRAME_TRACE("    frame_centre[%d].horizontal_offset                  : %6d\n", i, Header->frame_centre[i].horizontal_offset);
        FRAME_TRACE("    frame_centre[%d].vertical_offset                    : %6d\n", i, Header->frame_centre[i].vertical_offset);
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
FrameParserStatus_t   FrameParser_VideoAvs_c::ReadSliceHeader (unsigned int StartCodeIndex)
{
    AvsVideoPictureHeader_t*    PictureHeader;
    AvsVideoSequence_t*         SequenceHeader;
    int                         SliceCount;
    AvsVideoSlice_t*            Slice;
    unsigned int                Code;
    unsigned int                SliceCode;
    unsigned int                SliceOffset;
    unsigned int                MacroblockRows;

    FRAME_DEBUG ("%s\n", __FUNCTION__);

    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR("%s - Stream parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR("%s - Frame parameters unavailable for decode.\n", __FUNCTION__);
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
        PictureHeader->bottom_field_offset      = SliceOffset;// - ParsedFrameParameters->DataOffset;

#ifdef DUMP_HEADERS
    FRAME_TRACE("Slice header :- \n" );
    FRAME_TRACE("Slice start code               %6d\n", SliceCode);
    FRAME_TRACE("Slice offset                   %6d\n", SliceOffset);
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
FrameParserStatus_t   FrameParser_VideoAvs_c::CommitFrameForDecode (void)
{
    AvsVideoPictureHeader_t*            PictureHeader;
    AvsVideoSequence_t*                 SequenceHeader;
    PictureStructure_t                  PictureStructure;
    bool                                ProgressiveSequence;
    bool                                RepeatFirstField;
    bool                                TopFieldFirst;
    bool                                Frame;
    //bool                                FieldSequenceError;
    unsigned int                        PanAndScanCount;
    MatrixCoefficientsType_t            MatrixCoefficients;

    FRAME_DEBUG ("%s\n", __FUNCTION__);
    // Check we have the headers we need
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR("%s - Stream parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }

    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR("%s - Frame parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader              = &StreamParameters->SequenceHeader;
    PictureHeader               = &FrameParameters->PictureHeader;

    // Obtain and check the progressive etc values.
    ProgressiveSequence         = SequenceHeader->progressive_sequence;

    // All pictures are frame pictures picture
#if 0
    if ((SequenceHeader->progressive_sequence == 1) || (PictureHeader->progressive_frame == 1))
        PictureStructure        = StructureFrame;
    else if (PictureHeader->picture_structure == 0)
        PictureStructure        = StructureTopField;
    else
        PictureStructure        = StructureBottomField;
#else
    PictureStructure            = StructureFrame;
#endif
    Frame                       = PictureStructure == StructureFrame;
    RepeatFirstField            = PictureHeader->repeat_first_field;
    TopFieldFirst               = PictureHeader->top_field_first;
    if (FrameParameters->PictureDisplayExtensionHeaderPresent)
        PanAndScanCount         = FrameParameters->PictureDisplayExtensionHeader.number_of_frame_centre_offsets;
     else
        PanAndScanCount         = 0;


    if (!Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField))
    {
        FRAME_ERROR("%s - Illegal combination (%c %c %c %c).\n", __FUNCTION__,
                (ProgressiveSequence    ? 'T' : 'F'),
                (Frame                  ? 'T' : 'F'),
                (RepeatFirstField       ? 'T' : 'F'),
                (TopFieldFirst          ? 'T' : 'F'));
        return FrameParserHeaderSyntaxError;
    }


#if 0
    //
    // If we are doing field decode check for sequence error, and set appropriate flags
    // Update AccumulatedPictureStructure for nex pass, if this is the first decode of a field picture
    // it is what we have otherwise it is empty
    //

    FieldSequenceError          = (AccumulatedPictureStructure == PictureStructure);
    FirstDecodeOfFrame          = FieldSequenceError || (AccumulatedPictureStructure == StructureEmpty);
    AccumulatedPictureStructure = (FirstDecodeOfFrame && !Frame) ? PictureStructure : StructureEmpty;

    if( FieldSequenceError )
    {
        FRAME_ERROR("%s - Field sequence error detected.\n", __FUNCTION__);
        Player->CallInSequence (Stream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
    }
#endif

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
                FRAME_ERROR("%s - Forbidden or reserved matrix coefficient code specified (%02x)\n", __FUNCTION__, StreamParameters->SequenceDisplayExtensionHeader.matrix_coefficients);

            case AVS_MATRIX_COEFFICIENTS_UNSPECIFIED:   MatrixCoefficients      = MatrixCoefficients_ITU_R_BT601;               break;
        }
    }
    else
    {
        MatrixCoefficients = MatrixCoefficients_ITU_R_BT601;
    }


    // Nick added this to make avs struggle through
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
#if 0
    else
    {
        ParsedVideoParameters->Content.DisplayWidth             = ParsedVideoParameters->Content.Width;
        ParsedVideoParameters->Content.DisplayHeight            = ParsedVideoParameters->Content.Height;
    }
#endif

    ParsedVideoParameters->Content.FrameRate                    = AvsFrameRates(StreamParameters->SequenceHeader.frame_rate_code);

    ParsedVideoParameters->Content.VideoFullRange               = false;
    ParsedVideoParameters->Content.ColourMatrixCoefficients     = MatrixCoefficients;

    ParsedVideoParameters->Content.Progressive                  = ProgressiveSequence;
    ParsedVideoParameters->Content.OverscanAppropriate          = false;

    ParsedVideoParameters->Content.PixelAspectRatio             = AvsAspectRatios(SequenceHeader->aspect_ratio);

    ParsedVideoParameters->SliceType                            = SliceTypeTranslation[PictureHeader->picture_coding_type];
    ParsedVideoParameters->PictureStructure                     = PictureStructure;
    ParsedVideoParameters->InterlacedFrame                      = !PictureHeader->progressive_frame;
    ParsedVideoParameters->TopFieldFirst                        = TopFieldFirst;

    ParsedVideoParameters->DisplayCount[0]                      = DisplayCount0(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);
    ParsedVideoParameters->DisplayCount[1]                      = DisplayCount1(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField);

#if 0
    report (severity_info, "%s:%d (%d, %d, %d, %d) - CountsIndex %d Legal %d PanScanCount %d \n", __FUNCTION__, __LINE__,
            ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField,
            CountsIndex(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField),
            Legal(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField),
            PanScanCount(ProgressiveSequence, Frame, TopFieldFirst, RepeatFirstField));

    report (severity_info, "%s:%d - DisplayCount0 %d DisplayCount1 %d\n", __FUNCTION__, __LINE__, ParsedVideoParameters->DisplayCount[0], ParsedVideoParameters->DisplayCount[1]);
#endif

    ParsedVideoParameters->PanScan.Count                        = PanAndScanCount;
    for (unsigned int i=0; i<PanAndScanCount; i++)
    {
        ParsedVideoParameters->PanScan.DisplayCount[i]          = 1;
        ParsedVideoParameters->PanScan.HorizontalOffset[i]      = FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].horizontal_offset;
        ParsedVideoParameters->PanScan.VerticalOffset[i]        = FrameParameters->PictureDisplayExtensionHeader.frame_centre[i].vertical_offset;
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
bool   FrameParser_VideoAvs_c::NewStreamParametersCheck (void)
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

    Different   = memcmp (&CopyOfStreamParameters, StreamParameters, sizeof(AvsStreamParameters_t)) != 0;
    if (Different)
    {
        memcpy (&CopyOfStreamParameters, StreamParameters, sizeof(AvsStreamParameters_t));
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
FrameParserStatus_t   FrameParser_VideoAvs_c::PrepareReferenceFrameList (void)
{
    unsigned int                        i;
    unsigned int                        ReferenceFramesRequired;
    unsigned int                        ReferenceFramesDesired;
    unsigned int                        PictureCodingType;
    AvsVideoPictureHeader_t*            PictureHeader;

    // Note we cannot use StreamParameters or FrameParameters to address data directly,
    // as these may no longer apply to the frame we are dealing with.
    // Particularly if we have seen a sequenece header or group of pictures
    // header which belong to the next frame.

    PictureHeader               = &(((AvsFrameParameters_t *)(ParsedFrameParameters->FrameParameterStructure))->PictureHeader);
    PictureCodingType           = PictureHeader->picture_coding_type;
    ReferenceFramesRequired     = REFERENCE_FRAMES_REQUIRED(PictureCodingType);
    ReferenceFramesDesired      = REFERENCE_FRAMES_DESIRED(PictureCodingType);

    FRAME_DEBUG ("%s: ReferenceFrameList.EntryCount %d, Picture type = %d\n", __FUNCTION__, ReferenceFrameList.EntryCount, PictureCodingType);

    // Check for sufficient reference frames.  We cannot decode otherwise
    if (ReferenceFrameList.EntryCount < ReferenceFramesRequired)
        return FrameParserInsufficientReferenceFrames;

    ParsedFrameParameters->NumberOfReferenceFrameLists                  = 1;
    ParsedFrameParameters->ReferenceFrameList[0].EntryCount             = ReferenceFramesDesired;

    if ((PictureCodingType == AVS_PICTURE_CODING_TYPE_P) && (LastReferenceFramePictureCodingType == AVS_PICTURE_CODING_TYPE_I))
    {   // We must use the previous I frame for both entries in the reference frame list
        for (i=0; i<ReferenceFramesDesired; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount-1];
    }
    else if (ReferenceFrameList.EntryCount < ReferenceFramesDesired)    // this should always be covered by previous case
    {
        for (i=0; i<ReferenceFrameList.EntryCount; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[i];
        for (i=ReferenceFrameList.EntryCount; i<ReferenceFramesDesired; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount-1];
    }
    else
    {
        for (i=0; i<ReferenceFramesDesired; i++)
            ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[i]   = ReferenceFrameList.EntryIndicies[ReferenceFrameList.EntryCount - ReferenceFramesDesired + i];
    }
    //FRAME_TRACE("Prepare Ref list %d %d - %d %d - %d %d %d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
    //        ReferenceFrameList.EntryIndicies[0], ReferenceFrameList.EntryIndicies[1],
    //        ReferenceFramesDesired, ReferenceFrameList.EntryCount, ReferenceFrameList.EntryCount - ReferenceFramesDesired);

    if (ParsedFrameParameters->ReferenceFrame)
        LastReferenceFramePictureCodingType     = PictureCodingType;

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
FrameParserStatus_t   FrameParser_VideoAvs_c::ForPlayUpdateReferenceFrameList (void)
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

FrameParserStatus_t   FrameParser_VideoAvs_c::RevPlayProcessDecodeStacks (void)
{
    ReverseQueuedPostDecodeSettingsRing->Flush();

    return FrameParser_Video_c::RevPlayProcessDecodeStacks();
}
//}}}
#if 0
//{{{  CalculateFieldOffsets
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Calculate picture distance
//

FrameParserStatus_t   FrameParser_VideoAvs_c::CalculateFieldOffsets (unsigned int FirstSliceCodeIndex)
{
    AvsVideoPictureHeader_t*            PictureHeader;
    AvsVideoSequence_t*                 SequenceHeader;
    unsigned int                        Code;
    unsigned int                        SliceCode;
    unsigned int                        FirstSliceCodeOffset;
    unsigned int                        MacroblockRows;
    int                                 SliceCount;
    AvsVideoSlice_t*                    Slice;
    bool                                BottomFieldSlice        = false;

    FRAME_DEBUG ("%s\n", __FUNCTION__);

    // Check we have the headers we need
    if ((StreamParameters == NULL) || !StreamParameters->SequenceHeaderPresent)
    {
        FRAME_ERROR("%s - Stream parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserNoStreamParameters;
    }
    if ((FrameParameters == NULL) || !FrameParameters->PictureHeaderPresent)
    {
        FRAME_ERROR("%s - Frame parameters unavailable for decode.\n", __FUNCTION__);
        return FrameParserPartialFrameParameters;
    }

    SequenceHeader                      = &StreamParameters->SequenceHeader;
    PictureHeader                       = &FrameParameters->PictureHeader;

    Code                                = StartCodeList->StartCodes[FirstSliceCodeIndex];
    FirstSliceCodeOffset                = ExtractStartCodeOffset(Code);

    PictureHeader->top_field_offset     = FirstSliceCodeOffset - ParsedFrameParameters->DataOffset;     // As an offset from the picture start
    PictureHeader->bottom_field_offset  = PictureHeader->top_field_offset;

    if (PictureHeader->picture_structure == 1)
        return FrameParserNoError;

    MacroblockRows                      = ((SequenceHeader->vertical_size + 15) / 16) >> 1;
    FRAME_DEBUG("%s top slice %d at %d\n", __FUNCTION__, ExtractStartCodeCode(Code), ExtractStartCodeOffset(Code));

    SliceCount                          = 0;
    FRAME_TRACE("Slice headers :- \n" );
    for (unsigned int i=FirstSliceCodeIndex; i<StartCodeList->NumberOfStartCodes; i++)
    {
        Slice                           = &FrameParameters->SliceHeaderList.slice_array[SliceCount];
        Code                            = StartCodeList->StartCodes[i];
        SliceCode                       = ExtractStartCodeCode(Code);
        Slice->slice_start_code         = SliceCode;
        Slice->slice_offset             = ExtractStartCodeOffset(Code);

        FRAME_TRACE("Slice start code               %6d\n", Slice->slice_start_code);
        FRAME_TRACE("Slice offset                   %6d\n", Slice->slice_offset);

        if ((SliceCode >= MacroblockRows) && !BottomFieldSlice)
        {
            PictureHeader->bottom_field_offset  = ExtractStartCodeOffset(Code) - ParsedFrameParameters->DataOffset;
            BottomFieldSlice                    = true;
        }
    }
    FrameParameters->SliceHeaderList.no_slice_headers   = SliceCount;
    return FrameParserNoError;
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

FrameParserStatus_t   FrameParser_VideoAvs_c::RevPlayGeneratePostDecodeParameterSettings (void)
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

FrameParserStatus_t   FrameParser_VideoAvs_c::RevPlayRemoveReferenceFrameFromList (void)
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
#endif

