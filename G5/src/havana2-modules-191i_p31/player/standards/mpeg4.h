/************************************************************************
Copyright (C) 2002 STMicroelectronics. All Rights Reserved.

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

Source file name : mpeg4_video.h
Author :           Nick

Definition of the types and constants that are used by several components
dealing with mpeg4 video decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
11-Nov-02   Created                                         Nick

************************************************************************/

#ifndef H_MPEG4_VIDEO
#define H_MPEG4_VIDEO

#include "mpeg2.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

#define VO_START_CODE_MASK              0xe0
#define VO_ID_MASK                      0x1f
#define VO_START_CODE                   0x00

#define VOL_START_CODE_MASK             0xf0
#define VOL_ID_MASK                     0x0f
#define VOL_START_CODE                  0x20

#define USER_DATA_START_CODE_MASK       0xff
#define USER_DATA_START_CODE            MPEG2_USER_DATA_START_CODE

#define VOP_START_CODE_MASK             0xff
#define VOP_START_CODE                  0xb6

#define VSOS_START_CODE                 0xb0                    /* ViSual Object Sequence Start Code */
#define VSOS_END_CODE                   0xb1                    /* ViSual Object Sequence End Code */
#define GOV_START_CODE                  0xb3                    /* ViSual Object Sequence End Code */
#define VSO_START_CODE                  0xb5                    /* ViSual Object Start Code */

#define INVALID_START_CODE              0xf0
#define GREATEST_SYSTEM_START_CODE      MPEG2_GREATEST_SYSTEM_START_CODE

// An end of list code that we append
#define END_OF_START_CODE_LIST          GREATEST_SYSTEM_START_CODE


/* Definition of prediction_type values */

#define PREDICTION_TYPE_I               0
#define PREDICTION_TYPE_P               1
#define PREDICTION_TYPE_B               2

/* definition of shape values */

#define SHAPE_RECTANGULAR               0
#define SHAPE_BINARY                    1
#define SHAPE_BINARY_ONLY               2
#define SHAPE_GREY_SCALE                3

/* definition of sprite_usage values */

#define SPRITE_NOT_USED                 0
#define SPRITE_STATIC                   1
#define GMC_SPRITE                      2

/* definition of aspect_ratio_info values */

#define PAR_SQUARE                      0x01                    /* 1:1 */
#define PAR_4_3_PAL                     0x02                    /* 12:11 */
#define PAR_4_3_NTSC                    0x03                    /* 10:11 */
#define PAR_16_9_PAL                    0x04                    /* 16:11 */
#define PAR_16_9_NTSC                   0x05                    /* 40:33 */
#define PAR_EXTENDED                    0x0f

/* Definition of macro function to calculate bit field size for time increments */

#define INCREMENT_BITS(N)               ((N >  0x100) ?                                 \
					    ((N > 0x1000) ?                             \
						((N > 0x4000) ?                         \
						     (( N > 0x8000) ? 16 : 15)          \
							      :                         \
						     (( N > 0x2000) ? 14 : 13)          \
						)                                       \
							  :                             \
						((N >  0x400) ?                         \
						     (( N >  0x800) ? 12 : 11)          \
							      :                         \
						     (( N >  0x200) ? 10 :  9)          \
						)                                       \
					    )                                           \
						      :                                 \
					    ((N >   0x10) ?                             \
						((N >   0x40) ?                         \
						     (( N >   0x80) ?  8 :  7)          \
							      :                         \
						     (( N >   0x20) ?  6 :  5)          \
						)                                       \
							  :                             \
						((N >    0x4) ?                         \
						     (( N >    0x8) ?  4 :  3)          \
							      :                         \
						     (( N >    0x2) ?  2 :  1)          \
						)                                       \
					    )                                           \
					)


// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching mpeg4 headers
//

typedef struct Mpeg4VolHeader_s
{
								// Bits         Needed for decode
    unsigned int        random_accessible_vol;                  //  1                   No
    unsigned int        type_indication;                        //  8                   No
    unsigned int        is_object_layer_identifier;             //  1                   No
    unsigned int        visual_object_layer_verid;              //  4                   Yes
    unsigned int        visual_object_layer_priority;           //  3                   Yes
    unsigned int        aspect_ratio_info;                      //  4                   No
    unsigned int        par_width;                              //  8                   No
    unsigned int        par_height;                             //  8                   No

    unsigned int        version;

    unsigned int        vol_control_parameters;                 //  1                   No
    unsigned int        chroma_format;                          //  2                   No
    unsigned int        low_delay;                              //  1                   No
    unsigned int        vbv_parameters;                         //  1                   No
    unsigned int        first_half_bit_rate;                    //  15                  No
    unsigned int        latter_half_bit_rate;                   //  15                  No
    unsigned int        first_half_vbv_buffer_size;             //  15                  No
    unsigned int        latter_half_vbv_buffer_size;            //  3                   No
    unsigned int        first_half_vbv_occupancy;               //  11                  No
    unsigned int        latter_half_vbv_occupancy;              //  15                  No
    unsigned int        shape;                                  //  2                   Yes
    unsigned int        sprite_warping_accuracy;                
    unsigned int        no_of_sprite_warping_points;            //  6
    unsigned int        sprite_brightness_change;               //  2
   
    unsigned int        time_increment_resolution;              //  16                  No
    unsigned int        fixed_vop_rate;                         //  1                   No
    unsigned int        fixed_vop_time_increment;               // Variable             No
								// Only of fixed_vop_rate, N bits where
								// N is the number of bits needed to represent
								// a number modulo time_increment_resolution
    unsigned int        width;                                  //  13                  Yes
    unsigned int        height;                                 //  13                  Yes
    unsigned int        interlaced;                             //  1                   Yes
    unsigned int        obmc_disable;                           //  1                   No
    unsigned int        sprite_usage;                           //  1 or 2              Yes
    unsigned int        not_8_bit;                              //  1                   No
    unsigned int        quant_precision;                        //  4                   Yes
    unsigned int        bits_per_pixel;                         //  4                   No
    unsigned int        quant_type;                             //  1                   Yes
    unsigned int        load_intra_quant_matrix;                //  1                   No
    QuantiserMatrix_t   intra_quant_matrix;                     //  64*8                Yes
    unsigned int        load_non_intra_quant_matrix;            //  1                   No
    QuantiserMatrix_t   non_intra_quant_matrix;                 //  64*8                Yes
    unsigned int        quarter_pixel;                          //  1                   No
    unsigned int        complexity_estimation_disable;          //  1                   No
    unsigned int        resync_marker_disable;                  //  1                   Yes
    unsigned int        data_partitioning;                      //  1                   No
    unsigned int        reversible_vlc;                         //  1                   Yes
    unsigned int        intra_acdc_pred_disable;                //  1                   No
    unsigned int        request_upstream_message_type;          //  1                   No
    unsigned int        newpred_segment_type;                   //  1                   No
    unsigned int        reduced_resolution_vop_enable;          //  1                   No
    unsigned int        scalability;                            //  1                   Yes
   
} Mpeg4VolHeader_t;

/*
        //Short Header
    unsigned int short_video_header;
    unsigned int num_mb_in_gob; // shv
    unsigned int num_gobs_in_vop; // shv
    unsigned int prediction_type;
    unsigned int quantizer;
    unsigned int rounding_type;
    unsigned int fcode_for;
    unsigned int vop_coded;
    unsigned int use_intra_dc_vlc;
                   
                           
*/   



// --------------------------------------------------

typedef struct Mpeg4VopHeader_s
{
								// Bits         Needed for decode
    unsigned int        prediction_type;                        //  2                   Yes
    unsigned int        time_base;                              //  Variable            No
    unsigned int        time_inc;                               //  Variable            No
    unsigned int        vop_coded;                              //  1                   Yes
    unsigned int        rounding_type;                          //  0..1                Yes
    unsigned int        intra_dc_vlc_thr;                       //  3                   Yes
    unsigned int        top_field_first;                        //  1                   Yes
    unsigned int        alternate_vertical_scan_flag;           //  0                   No
    unsigned int        quantizer;                              // Variable             Yes
    unsigned int        fcode_forward;                          //  3                   Yes
    unsigned int        fcode_backward;                         //  3                   Yes
    unsigned int        divx_nvop;                              //  0                   No (calculated in frame parser)
	
	
    int trbi;
    int trdi;
    int trb_trd;
    int trb_trd_trd;
    int trd; // temporal diff. between current B-VOP and previous reference VOP
    int trb; // temporal diff. between next and previous reference VOP
    unsigned int bit_skip_no;
    
    unsigned int display_time_prev;
    unsigned int display_time_next;
    unsigned int time_increment_resolution;
             int tframe;
	
} Mpeg4VopHeader_t;

// ////////////////////////////////////////////////////////////////////////////
//
//  Specific defines and constants for the codec layer
//

#define MPEG4_VIDEO_MAX_REFERENCE_FRAMES_IN_DECODE      2

typedef struct Mpeg4VideoStreamParameters_s
{
    unsigned int                          MicroSecondsPerFrame;                 // propegated from the AVI
    Mpeg4VolHeader_t                      VolHeader;
} Mpeg4VideoStreamParameters_t;

#define BUFFER_MPEG4_STREAM_PARAMETERS        "Mpeg4StreamParameters"
#define BUFFER_MPEG4_STREAM_PARAMETERS_TYPE   {BUFFER_MPEG4_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Mpeg4VideoStreamParameters_t)}

//

typedef struct Mpeg4VideoFrameParameters_s
{
//    unsigned long long                    Pts;
//    unsigned int                          PtsValid;
//    unsigned int                          BackwardReferenceIndex;               // Reference indices must be first 2 parameters to allow
//    unsigned int                          ForwardReferenceIndex;                // generic codec to translate them to buffer indices
//    unsigned int                          top_field_first;                      // used in the manifester
//    unsigned int                          prediction_type;
    unsigned int                          bit_skip_no;
    Mpeg4VopHeader_t                      VopHeader;
} Mpeg4VideoFrameParameters_t;

#define BUFFER_MPEG4_FRAME_PARAMETERS        "Mpeg4FrameParameters"
#define BUFFER_MPEG4_FRAME_PARAMETERS_TYPE   {BUFFER_MPEG4_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(Mpeg4VideoFrameParameters_t)}



// offsets for transfer of parameters

#define PACKED_MPEG4_STREAM_OFFSET_VOL_VERID                    0
#define PACKED_MPEG4_STREAM_OFFSET_VOL_PRIORITY                 1
#define PACKED_MPEG4_STREAM_OFFSET_SHAPE                        2
#define PACKED_MPEG4_STREAM_OFFSET_WIDTH                        3
#define PACKED_MPEG4_STREAM_OFFSET_HEIGHT                       4
#define PACKED_MPEG4_STREAM_OFFSET_INTERLACED                   5
#define PACKED_MPEG4_STREAM_OFFSET_SPRITE_USAGE                 6
#define PACKED_MPEG4_STREAM_OFFSET_QUANT_PRECISION              7
#define PACKED_MPEG4_STREAM_OFFSET_QUANT_TYPE                   8
#define PACKED_MPEG4_STREAM_OFFSET_RESYNC_MARKER_DISABLE        9
#define PACKED_MPEG4_STREAM_OFFSET_SCALABILITY                  10
#define PACKED_MPEG4_STREAM_OFFSET_INTRA_QUANTIZER              11
#define PACKED_MPEG4_STREAM_OFFSET_NON_INTRA_QUANTIZER          (11 + (QUANTISER_MATRIX_SIZE/sizeof(unsigned int)))
#define PACKED_MPEG4_STREAM_SIZE                                (11 + 2*(QUANTISER_MATRIX_SIZE/sizeof(unsigned int)))

#define PACKED_MPEG4_FRAME_OFFSET_COMPRESSED_DATA_BIT_OFFSET    0
#define PACKED_MPEG4_FRAME_OFFSET_PREDICTION_TYPE               1
#define PACKED_MPEG4_FRAME_OFFSET_VOP_CODED                     2
#define PACKED_MPEG4_FRAME_OFFSET_ROUNDING_TYPE                 3
#define PACKED_MPEG4_FRAME_OFFSET_INTRA_DC_VLC_THR              4
#define PACKED_MPEG4_FRAME_OFFSET_QUANTIZER                     5
#define PACKED_MPEG4_FRAME_OFFSET_FCODE_FORWARD                 6
#define PACKED_MPEG4_FRAME_OFFSET_FCODE_BACKWARD                7
#define PACKED_MPEG4_FRAME_SIZE                                 8

#define INVALID_FRAME_INDEX     0xffffffff

#define VALID_PTS_MASK                          0x1ffffffffull
#define INVALID_PTS_VALUE                       0x200000000ull

#endif
