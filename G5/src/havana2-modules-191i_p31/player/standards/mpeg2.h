/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name ; mpeg2.h
Author ;           Nick

Definition of the constants/macros that define useful things associated with 
Mpeg2 streams.


Date        Modification                                    Name
----        ------------                                    --------
05-Sep-05   Created                                         Nick

************************************************************************/

#ifndef H_MPEG2
#define H_MPEG2

// Definition of Pes codes
#define MPEG2_PES_START_CODE_MASK                       0xf0
#define MPEG2_VIDEO_PES_START_CODE                      0xe0

// Definition of start code values 
#define MPEG2_PICTURE_START_CODE                        0x00
#define MPEG2_FIRST_SLICE_START_CODE                    0x01
#define MPEG2_GREATEST_SLICE_START_CODE                 0xAF
#define MPEG2_RESERVED_START_CODE_0                     0xB0
#define MPEG2_RESERVED_START_CODE_1                     0xB1
#define MPEG2_USER_DATA_START_CODE                      0xB2
#define MPEG2_SEQUENCE_HEADER_CODE                      0xB3
#define MPEG2_SEQUENCE_ERROR_CODE                       0xB4
#define MPEG2_EXTENSION_START_CODE                      0xB5
#define MPEG2_RESERVED_START_CODE_6                     0xB6
#define MPEG2_SEQUENCE_END_CODE                         0xB7
#define MPEG2_GROUP_START_CODE                          0xB8
#define MPEG2_SMALLEST_SYSTEM_START_CODE                0xB9
#define MPEG2_GREATEST_SYSTEM_START_CODE                0xFF

// Definition of extension_start_code_identifier values
#define MPEG2_SEQUENCE_EXTENSION_ID                     0x01
#define MPEG2_SEQUENCE_DISPLAY_EXTENSION_ID             0x02
#define MPEG2_QUANT_MATRIX_EXTENSION_ID                 0x03
#define MPEG2_COPYRIGHT_EXTENSION_ID                    0x04
#define MPEG2_SEQUENCE_SCALABLE_EXTENSION_ID            0x05
#define MPEG2_PICTURE_DISPLAY_EXTENSION_ID              0x07
#define MPEG2_PICTURE_CODING_EXTENSION_ID               0x08
#define MPEG2_PICTURE_SPATIAL_SCALABLE_EXTENSION_ID     0x09
#define MPEG2_PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID    0x0A

// Definition of chroma formats found in sequence extension
#define MPEG2_SEQUENCE_CHROMA_RESERED                   0       /* unsupported chroma type */
#define MPEG2_SEQUENCE_CHROMA_4_2_0                     1       /* chroma type 4:2:0       */
#define MPEG2_SEQUENCE_CHROMA_4_2_2                     2       /* chroma type 4:2:2       */
#define MPEG2_SEQUENCE_CHROMA_4_4_4                     3       /* chroma type 4:4:4       */

// Definition of picture_coding_type values
#define MPEG2_PICTURE_CODING_TYPE_I                     1
#define MPEG2_PICTURE_CODING_TYPE_P                     2
#define MPEG2_PICTURE_CODING_TYPE_B                     3
#define MPEG2_PICTURE_CODING_TYPE_D_IN_MPEG1            4

// Definition of picture_structure values
#define MPEG2_PICTURE_STRUCTURE_TOP_FIELD               1
#define MPEG2_PICTURE_STRUCTURE_BOTTOM_FIELD            2
#define MPEG2_PICTURE_STRUCTURE_FRAME                   3

// Definition of scaleable modes

#define MPEG2_SCALABLE_MODE_DATA_PARTITIONING           0
#define MPEG2_SCALABLE_MODE_SPATIAL_SCALABILITY         1
#define MPEG2_SCALABLE_MODE_SNR_SCALABILITY             2
#define MPEG2_SCALABLE_MODE_TEMPORAL_SCALABILITY        3

// Definition of matrix coefficient values

#define  MPEG2_MATRIX_COEFFICIENTS_FORBIDDEN		0
#define  MPEG2_MATRIX_COEFFICIENTS_BT709		1
#define  MPEG2_MATRIX_COEFFICIENTS_UNSPECIFIED		2
#define  MPEG2_MATRIX_COEFFICIENTS_RESERVED		3
#define  MPEG2_MATRIX_COEFFICIENTS_FCC			4
#define  MPEG2_MATRIX_COEFFICIENTS_BT470_BGI		5
#define  MPEG2_MATRIX_COEFFICIENTS_SMPTE_170M		6
#define  MPEG2_MATRIX_COEFFICIENTS_SMPTE_240M		7

// ////////////////////////////////////////////////////////////////////////////////////
//
// The header definitions
//

#define QUANTISER_MATRIX_SIZE           64
typedef unsigned char   QuantiserMatrix_t[QUANTISER_MATRIX_SIZE];

//

typedef struct Mpeg2VideoSequence_s
{
    unsigned int        horizontal_size_value;                           /*   12 bits */
    unsigned int        vertical_size_value;                             /*   12 bits */
    unsigned int        aspect_ratio_information;                        /*    4 bits */
    unsigned int        frame_rate_code;                                 /*    4 bits */
    unsigned int        bit_rate_value;                                  /*   18 bits */
    //unsigned int      marker_bit;                                      /*    1 bit  */
    unsigned int        vbv_buffer_size_value;                           /*   10 bits */
    unsigned int        constrained_parameters_flag;  /* 0 in MPEG2 */   /*    1 bit  */
    unsigned int        load_intra_quantizer_matrix;                     /*    1 bit  */
    QuantiserMatrix_t   intra_quantizer_matrix;                          /* 64*8 bits */
    unsigned int        load_non_intra_quantizer_matrix;                 /*    1 bit  */
    QuantiserMatrix_t   non_intra_quantizer_matrix;                      /* 64*8 bits */
} Mpeg2VideoSequence_t;

//

typedef struct Mpeg2VideoSequenceExtension_s
{
    unsigned int        profile_and_level_indication;                   /*  8 bits */
    unsigned int        progressive_sequence;                           /*  1 bit  */
    unsigned int        chroma_format;                                  /*  2 bits */
    unsigned int        horizontal_size_extension;                      /*  2 bits */
    unsigned int        vertical_size_extension;                        /*  2 bits */
    unsigned int        bit_rate_extension;                             /* 12 bits */
    //unsigned int      marker_bit;                                     /*  1 bit  */
    unsigned int        vbv_buffer_size_extension;                      /*  8 bits */
    unsigned int        low_delay;                                      /*  1 bit  */
    unsigned int        frame_rate_extension_n;                         /*  2 bits */
    unsigned int        frame_rate_extension_d;                         /*  5 bits */
} Mpeg2VideoSequenceExtension_t;

//

typedef struct Mpeg2VideoSequenceDisplayExtension_s
{
    unsigned int        video_format;                                   /* 3 bits */
    unsigned int        color_description;                              /* 1 bit */
    unsigned int        color_primaries;                                /* 8 bits */
    unsigned int        transfer_characteristics;                       /* 8 bits */
    unsigned int        matrix_coefficients;                            /* 8 bits */
    unsigned int        display_horizontal_size;                        /* 14 bits */
    //unsigned int      marker_bit;                                     /*  1 bit  */
    unsigned int        display_vertical_size;                          /* 14 bits */
} Mpeg2VideoSequenceDisplayExtension_t;

//

typedef struct Mpeg2VideoSequenceScalableExtension_s
{
    unsigned            scalable_mode;                                  /*  2 bits */
    unsigned            layer_id;                                       /*  4 bits */

    unsigned            lower_layer_prediction_horizontal_size;         /* 14 bits Optional if scalable_mode == 01 */
    //unsigned          marker_bit;                                     /*  1 bit  Optional if scalable_mode == 01 */
    unsigned            lower_layer_prediction_vertical_size;           /* 14 bits Optional if scalable_mode == 01 */
    unsigned            horizontal_subsampling_factor_m;                /*  5 bits Optional if scalable_mode == 01 */
    unsigned            horizontal_subsampling_factor_n;                /*  5 bits Optional if scalable_mode == 01 */
    unsigned            vertical_subsampling_factor_m;                  /*  5 bits Optional if scalable_mode == 01 */
    unsigned            vertical_subsampling_factor_n;                  /*  5 bits Optional if scalable_mode == 01 */

    unsigned            picture_mux_enable;                             /*  1 bit  Optional if scalable_mode == 11 */
    unsigned            mux_to_progressive_sequence;                    /*  1 bit  Optional if scalable_mode == 11 */
    unsigned            picture_mux_order;                              /*  3 bits Optional if scalable_mode == 11 */
    unsigned            picture_mux_factor;                             /*  3 bits Optional if scalable_mode == 11 */
} Mpeg2VideoSequenceScalableExtension_t;

//

typedef struct Mpeg2VideoGOP_s
{
    struct {
	unsigned        drop_frame_flag;                                /*  1 bit  */
	unsigned        hours;                                          /*  5 bits */
	unsigned        minutes;                                        /*  6 bits */
	//unsigned      marker_bit;                                     /*  1 bit  */
	unsigned        seconds;                                        /*  6 bits */
	unsigned        pictures;                                       /*  6 bits */
    }  time_code;

    unsigned            closed_gop;                                     /*  1 bit  */
    unsigned            broken_link;                                    /*  1 bit  */
} Mpeg2VideoGOP_t;

//

typedef struct Mpeg2VideoPicture_s
{
    unsigned            temporal_reference;                             /*  10 bits */
    unsigned            picture_coding_type;                            /*   3 bits */ /* D pictures only allowed in MPEG1 */ 
    unsigned            vbv_delay;                                      /*  16 bits */
    unsigned            full_pel_forward_vector;                        /*   1 bit  */ /* 0   in MPEG2 */
    unsigned            forward_f_code;                                 /*   3 bits */ /* 0x7 in MPEG2 */
    unsigned            full_pel_backward_vector;                       /*   1 bit  */ /* 0   in MPEG2 */
    unsigned            backward_f_code;                                /*   3 bits */ /* 0x7 in MPEG2 */
} Mpeg2VideoPicture_t;

//

typedef struct Mpeg2VideoPictureCodingExtension_s
{
    unsigned            f_code[2][2];                                   /* 4*4 bits */
    unsigned            intra_dc_precision;                             /*  2 bits */
    unsigned            picture_structure;                              /*  2 bits */
    unsigned            top_field_first;                                /*  1 bit  */
    unsigned            frame_pred_frame_dct;                           /*  1 bit  */
    unsigned            concealment_motion_vectors;                     /*  1 bit  */
    unsigned            q_scale_type;                                   /*  1 bit  */
    unsigned            intra_vlc_format;                               /*  1 bit  */
    unsigned            alternate_scan;                                 /*  1 bit  */
    unsigned            repeat_first_field;                             /*  1 bit  */
    unsigned            chroma_420_type;                                /*  1 bit  */
    unsigned            progressive_frame;                              /*  1 bit  */
    unsigned            composite_display_flag;                         /*  1 bit  */
    unsigned            v_axis;                                         /*  1 bit  */
    unsigned            field_sequence;                                 /*  3 bits */
    unsigned            sub_carrier;                                    /*  1 bit  */
    unsigned            burst_amplitude;                                /*  7 bits */
    unsigned            sub_carrier_phase;                              /*  8 bits */
} Mpeg2VideoPictureCodingExtension_t;

//

typedef struct Mpeg2VideoQuantMatrixExtension_s
{
    unsigned            load_intra_quantizer_matrix;                    /*    1 bit  */
    QuantiserMatrix_t   intra_quantizer_matrix;                         /* 64*8 bits */
    unsigned            load_non_intra_quantizer_matrix;                /*    1 bit  */
    QuantiserMatrix_t   non_intra_quantizer_matrix;                     /* 64*8 bits */
    unsigned            load_chroma_intra_quantizer_matrix;             /*    1 bit  */
    QuantiserMatrix_t   chroma_intra_quantizer_matrix;                  /* 64*8 bits */
    unsigned            load_chroma_non_intra_quantizer_matrix;         /*    1 bit  */
    QuantiserMatrix_t   chroma_non_intra_quantizer_matrix;              /* 64*8 bits */
} Mpeg2VideoQuantMatrixExtension_t;

//

typedef struct Mpeg2VideoPictureDisplayExtension_s
{
    struct
    {
	int             horizontal_offset;                              /* 16 bits */
	//unsigned      marker_bit;                                     /*  1 bit  */
	int             vertical_offset;                                /* 16 bits */
	//unsigned      marker_bit;                                     /*  1 bit  */
    } frame_centre[3];
} Mpeg2VideoPictureDisplayExtension_t;

//

typedef struct Mpeg2VideoPictureTemporalScalableExtension_s
{
    unsigned            reference_select_code;                          /*  2 bits */
    unsigned            forward_temporal_reference;                     /* 10 bits */
    //unsigned          marker_bit;                                     /*  1 bit  */
    unsigned            backward_temporal_reference;                    /* 10 bits */
} Mpeg2VideoPictureTemporalScalableExtension_t;

//

typedef struct Mpeg2VideoPictureSpatialScalableExtension_s
{
    unsigned            lower_layer_temporal_reference;                 /* 10 bits */
    //unsigned          marker_bit;                                     /*  1 bit  */
    unsigned            lower_layer_horizontal_offset;                  /* 15 bits */
    //unsigned          marker_bit;                                     /*  1 bit  */
    unsigned            lower_layer_vertical_offset;                    /* 15 bits */
    unsigned            spatial_temporal_weight_code_table_index;       /*  2 bits */
    unsigned            lower_layer_progressive_frame;                  /*  1 bit  */
    unsigned            lower_layer_deinterlaced_field_select;          /*  1 bit  */
} Mpeg2VideoPictureSpatialScalableExtension_t;


// ////////////////////////////////////////////////////////////////////////////////////
//
// The conglomerate structures used by the player
//

typedef enum MpegStreamType_e
{
    MpegStreamTypeMpeg1 = 0,
    MpegStreamTypeMpeg2 = 1
} MpegStreamType_t;

//

typedef struct Mpeg2StreamParameters_s
{
    bool                                        UpdatedSinceLastFrame;

    bool                                        SequenceHeaderPresent;
    bool                                        SequenceExtensionHeaderPresent;
    bool                                        SequenceDisplayExtensionHeaderPresent;
    bool                                        SequenceScalableExtensionHeaderPresent;
    bool                                        QuantMatrixExtensionHeaderPresent;

    MpegStreamType_t                            StreamType;

    Mpeg2VideoSequence_t                        SequenceHeader;
    Mpeg2VideoSequenceExtension_t               SequenceExtensionHeader;
    Mpeg2VideoSequenceDisplayExtension_t        SequenceDisplayExtensionHeader;
    Mpeg2VideoSequenceScalableExtension_t       SequenceScalableExtensionHeader;
    Mpeg2VideoQuantMatrixExtension_t            QuantMatrixExtensionHeader;
    Mpeg2VideoQuantMatrixExtension_t            CumulativeQuantMatrices;
} Mpeg2StreamParameters_t;

#define BUFFER_MPEG2_STREAM_PARAMETERS        "Mpeg2StreamParameters"
#define BUFFER_MPEG2_STREAM_PARAMETERS_TYPE   {BUFFER_MPEG2_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Mpeg2StreamParameters_t)}

//

typedef struct Mpeg2FrameParameters_s
{
    bool                                        GroupOfPicturesHeaderPresent;
    bool                                        PictureHeaderPresent;
    bool                                        PictureCodingExtensionHeaderPresent;
    bool                                        PictureDisplayExtensionHeaderPresent;
    bool                                        PictureTemporalScalableExtensionHeaderPresent;
    bool                                        PictureSpatialScalableExtensionHeaderPresent;

    Mpeg2VideoGOP_t                             GroupOfPicturesHeader;
    Mpeg2VideoPicture_t                         PictureHeader;
    Mpeg2VideoPictureCodingExtension_t          PictureCodingExtensionHeader;
    Mpeg2VideoPictureDisplayExtension_t         PictureDisplayExtensionHeader;
    Mpeg2VideoPictureTemporalScalableExtension_t PictureTemporalScalableExtensionHeader;
    Mpeg2VideoPictureSpatialScalableExtension_t PictureSpatialScalableExtensionHeader;
} Mpeg2FrameParameters_t;

#define BUFFER_MPEG2_FRAME_PARAMETERS        "Mpeg2FrameParameters"
#define BUFFER_MPEG2_FRAME_PARAMETERS_TYPE   {BUFFER_MPEG2_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Mpeg2FrameParameters_t)}

//

#endif

