/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name ; avs.h
Author ;           Andy

Definition of the constants/macros that define useful things associated with 
AVS streams.


Date        Modification                                    Name
----        ------------                                    --------
19-Jan-09   Created                                         Andy

************************************************************************/

#ifndef H_AVS
#define H_AVS

// Definition of Pes codes
#define AVS_PES_START_CODE_MASK                         0xf0
#define AVS_VIDEO_PES_START_CODE                        0xe0

// Definition of start code values 
#define AVS_FIRST_SLICE_START_CODE                      0x00
#define AVS_GREATEST_SLICE_START_CODE                   0xAF

#define AVS_START_CODE_PREFIX                           0x000001
#define AVS_VIDEO_SEQUENCE_START_CODE                   0xB0
#define AVS_VIDEO_SEQUENCE_END_CODE                     0xB1
#define AVS_USER_DATA_START_CODE                        0xB2
#define AVS_I_PICTURE_START_CODE                        0xB3
#define AVS_RESERVED_START_CODE_4                       0xB4
#define AVS_EXTENSION_START_CODE                        0xB5
#define AVS_PB_PICTURE_START_CODE                       0xB6
#define AVS_VIDEO_EDIT_CODE                             0xB7
#define AVS_RESERVED_START_CODE_8                       0xB8

#define AVS_SMALLEST_SYSTEM_START_CODE                  0xB9
#define AVS_GREATEST_SYSTEM_START_CODE                  0xFF

#define AVS_SEQUENCE_DISPLAY_EXTENSION_ID               0x02
#define AVS_COPYRIGHT_EXTENSION_ID                      0x04
#define AVS_PICTURE_DISPLAY_EXTENSION_ID                0x07
#define AVS_CAMERA_PARAMETERS_EXTENSION_ID              0x0B

// Definition of picture_coding_type values
#define AVS_PICTURE_CODING_TYPE_I                       0
#define AVS_PICTURE_CODING_TYPE_P                       1
#define AVS_PICTURE_CODING_TYPE_B                       2

// Colour primary values
#define AVS_MATRIX_COEFFICIENTS_FORBIDDEN               0
#define AVS_MATRIX_COEFFICIENTS_BT709                   1
#define AVS_MATRIX_COEFFICIENTS_UNSPECIFIED             2
#define AVS_MATRIX_COEFFICIENTS_RESERVED                3
#define AVS_MATRIX_COEFFICIENTS_FCC                     4
#define AVS_MATRIX_COEFFICIENTS_BT470_BGI               5
#define AVS_MATRIX_COEFFICIENTS_SMPTE_170M              6
#define AVS_MATRIX_COEFFICIENTS_SMPTE_240M              7

// ////////////////////////////////////////////////////////////////////////////////////
//
// 
//

#define PROFILE_FORBIDDEN           0x00
#define PROFILE_JIZHUN              0x20

#define LEVEL_FORBIDDEN             0x00
#define LEVEL_2_0                   0x10
#define LEVEL_4_0                   0x20
#define LEVEL_4_2                   0x22
#define LEVEL_6_0                   0x40
#define LEVEL_6_2                   0x42



// ////////////////////////////////////////////////////////////////////////////////////
//
// The header definitions
//


typedef struct AvsVideoSequence_s
{
    unsigned int        profile_id;                             /* u(8) */
    unsigned int        level_id;                               /* u(8) */
    unsigned int        progressive_sequence;                   /* u(1) */
    unsigned int        horizontal_size;                        /* u(14) */
    unsigned int        vertical_size;                          /* u(14) */

    unsigned int        chroma_format;                          /* u(2) */
    unsigned int        sample_precision;                       /* u(3) */
    unsigned int        aspect_ratio;                           /* u(4) */
    unsigned int        frame_rate_code;                        /* u(4) */
    unsigned int        bit_rate_lower;                         /* u(18) */
    //unsigned int      marker_bit;                             /* f(1) */
    unsigned int        bit_rate_upper;                         /* u(12) */
    unsigned int        low_delay;                              /* u(1) */
    //unsigned int      marker_bit;                             /* f(1) */
    unsigned int        bbv_buffer_size;                        /* u(18) */
    //unsigned int      reserved_bits;                          /* r(3) */
} AvsVideoSequence_t;

//

typedef struct AvsVideoSequenceDisplayExtension_s
{
    unsigned int        video_format;                           /* u(3) */
    unsigned int        sample_range;                           /* u(1) */
    unsigned int        color_description;                      /* u(1) */
    unsigned int        color_primaries;                        /* u(8) */
    unsigned int        transfer_characteristics;               /* u(8) */
    unsigned int        matrix_coefficients;                    /* u(8) */
    unsigned int        display_horizontal_size;                /* u(14) */
    unsigned int        display_vertical_size;                  /* u(14) */
} AvsVideoSequenceDisplayExtension_t;

//

typedef struct AvsVideoCopyrightExtension_s
{
    unsigned int        copyright_flag;                         /* u(1) */
    unsigned int        copyright_id;                           /* u(8) */
    unsigned int        original_or_copy;                       /* u(1) */
    unsigned int        copyright_number_1;                     /* u(20) */
    unsigned int        copyright_number_2;                     /* u(22) */
    unsigned int        copyright_number_3;                     /* 22) */
} AvsVideoCopyrightExtension_t;

//

typedef struct AvsVideoCameraParametersExtension_s
{
    unsigned int        camera_id;                              /* u(7) */
    unsigned int        height_of_image_device;                 /* u(22) */
    unsigned int        focal_length;                           /* u(22) */
    unsigned int        f_number;                               /* u(22) */
    unsigned int        vertical_angle_of_view;                 /* u(22) */
    signed int          camera_position_x_upper;                /* i(16) */
    signed int          camera_position_x_lower;                /* i(16) */
    signed int          camera_position_y_upper;                /* i(16) */
    signed int          camera_position_y_lower;                /* i(16) */
    signed int          camera_position_z_upper;                /* i(16) */
    signed int          camera_position_z_lower;                /* i(16) */
    signed int          camera_direction_x;                     /* i(22) */
    signed int          camera_direction_y;                     /* i(22) */
    signed int          camera_direction_z;                     /* i(22) */
    signed int          image_plane_vertical_x;                 /* i(22) */
    signed int          image_plane_vertical_y;                 /* i(22) */
    signed int          image_plane_vertical_z;                 /* i(22) */
} AvsVideoCameraParametersExtension_t;

//

typedef struct AvsVideoPictureHeader_s
{
    unsigned int        bbv_delay;                              /* u(16) */
    unsigned int        picture_coding_type;                    /* u(2) */
    unsigned int        time_code_flag;                         /* u(1) */
    unsigned int        time_code;                              /* u(24) */
    unsigned int        picture_distance;                       /* u(8) */
    unsigned int        bbv_check_times;                        /* ue(v) */
    unsigned int        progressive_frame;                      /* u(1) */
    unsigned int        picture_structure;                      /* u(1) */
    unsigned int        picture_structure_bwd;                  /* u(1) */
    unsigned int        advanced_pred_mode_disable;             /* u(1) */
    unsigned int        top_field_first;                        /* u(1) */
    unsigned int        repeat_first_field;                     /* u(1) */
    unsigned int        fixed_picture_qp;                       /* u(1) */
    unsigned int        picture_qp;                             /* u(6) */
    unsigned int        picture_reference_flag;                 /* u(1) */
    unsigned int        no_forward_reference_flag;              /* u(1) */
    unsigned int        skip_mode_flag;                         /* u(1) */
    unsigned int        loop_filter_disable;                    /* u(1) */
    unsigned int        loop_filter_parameter_flag;             /* u(1) */
    signed int          alpha_c_offset;                         /* se(v) */
    signed int          beta_offset;                            /* se(v) */

    // calculated values
    signed int          top_field_offset;
    signed int          bottom_field_offset;
    signed int          picture_order_count;
    signed int          tr;                       //<! temporal reference, 8 bit, wraps at 255
    signed int          imgtr_next_P;
    signed int          imgtr_last_P;
    signed int          imgtr_last_prev_P;

    bool                ReversePlay;
} AvsVideoPictureHeader_t;


//

typedef struct AvsVideoPictureDisplayExtension_s
{
    unsigned int        number_of_frame_centre_offsets;
    struct
    {
        int             horizontal_offset;                      /* i(16) */
        int             vertical_offset;                        /* i(16) */
    } frame_centre[3];
} AvsVideoPictureDisplayExtension_t;


#if 0
typedef struct AvsVideoSlice_s
{
    unsigned char       slice_start_code;                       /* f(32) */
    unsigned char       slice_vertical_position_extension;      /* u(3) */
    unsigned char       fixed_slice_qp;                         /* u(1) */
    unsigned char       slice_qp;                               /* u(6) */
    unsigned char       slice_weighting_flag;                   /* u(1) */

    struct
    {
        unsigned char   luma_scale;                             /* u(8) */
        signed char     luma_shift;                             /* i(8) */
        unsigned char   chroma_scale;                           /* u(8) */
        signed char     chroma_shift;                           /* i(8) */
    } slice;
} AvsVideoSlice_t;
#else
#define AVS_MAX_SLICE_COUNT                                     68
typedef struct AvsVideoSlice_s
{
        unsigned int    slice_start_code;
        unsigned int    slice_offset;
} AvsVideoSlice_t;

typedef struct AvsVideoSliceList_s
{
        unsigned int    no_slice_headers;
        AvsVideoSlice_t slice_array[AVS_MAX_SLICE_COUNT];
} AvsVideoSliceList_t;
#endif
// macroblock

typedef struct AvsVideoMacroblock_s
{
    unsigned int        mb_type;                                /* ue(v) */
    unsigned int        mb_part_type;                           /* u(2) */
    unsigned int        pred_mode_flag;                         /* u(1) */
    unsigned int        intra_luma_pred_mode;                   /* u(2) */
    unsigned int        intra_chroma_pred_mode;                 /* ue(v) */
    unsigned int        intra_chroma_pred_mode_422;             /* ue(v) */
} AvsVideoMacroblock_t;

// block

typedef struct AvsVideoBlock_s
{
    unsigned int        trans_coefficient;                      /* ce(v) */
    unsigned int        escape_level_diff;                      /* ce(v) */
} AvsVideoBlock_t;

// ////////////////////////////////////////////////////////////////////////////////////
//
// The conglomerate structures used by the player
//

//

typedef struct AvsStreamParameters_s
{
    bool                                    UpdatedSinceLastFrame;

    bool                                    SequenceHeaderPresent;
    bool                                    SequenceDisplayExtensionHeaderPresent;
    bool                                    CopyrightExtensionHeaderPresent;
    bool                                    CameraParametersExtensionHeaderPresent;

    AvsVideoSequence_t                      SequenceHeader;
    AvsVideoSequenceDisplayExtension_t      SequenceDisplayExtensionHeader;
    AvsVideoCopyrightExtension_t            CopyrightExtensionHeader;
    AvsVideoCameraParametersExtension_t     CameraParametersExtensionHeader;
} AvsStreamParameters_t;

#define BUFFER_AVS_STREAM_PARAMETERS        "AvsStreamParameters"
#define BUFFER_AVS_STREAM_PARAMETERS_TYPE   {BUFFER_AVS_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AvsStreamParameters_t)}

//

typedef struct AvsFrameParameters_s
{
    bool                                    PictureHeaderPresent;
    bool                                    PictureDisplayExtensionHeaderPresent;
    bool                                    SliceHeaderPresent;

    AvsVideoPictureHeader_t                 PictureHeader;
    AvsVideoPictureDisplayExtension_t       PictureDisplayExtensionHeader;
    AvsVideoSliceList_t                     SliceHeaderList;
} AvsFrameParameters_t;

#define BUFFER_AVS_FRAME_PARAMETERS        "AvsFrameParameters"
#define BUFFER_AVS_FRAME_PARAMETERS_TYPE   {BUFFER_AVS_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AvsFrameParameters_t)}

//

#endif

