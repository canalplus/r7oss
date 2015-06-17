/************************************************************************
Copyright (C) 2004 STMicroelectronics. All Rights Reserved.

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

Source file name : h264_video.h
Author :           Nick

Definition of the types and constants that are used by several components
dealing with h264 video decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
21-Jun-04   Created                                         Nick

************************************************************************/

#ifndef H_H264
#define H_H264

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

//
// Constants constraining our specific implementation
//

#define H264_CODED_FRAME_COUNT                          1024
#define H264_MAXIMUM_FRAME_SIZE                         0x100000

#define H264_FRAME_MEMORY_SIZE                          PLAYER2_H264_FRAME_MEMORY_SIZE /* 0x800000 */
#define H264_STREAM_PARAMETERS_COUNT			PLAYER2_H264_STREAM_PARAMETERS_COUNT /* 64 */
#define H264_FRAME_PARAMETERS_COUNT			PLAYER2_H264_FRAME_PARAMETERS_COUNT /* 64 */
#define H264_MACROBLOCK_STRUCTURE_MEMORY                PLAYER2_H264_MACROBLOCK_STRUCTURE_MEMORY        /* 0.5mb per 1920x1088 frame */

#define H264_STANDARD_MAX_SEQUENCE_PARAMETER_SETS       32
#define H264_MAX_SEQUENCE_PARAMETER_SETS                32              /* Standard says 32 */
#define H264_MAX_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE      255             /* used in SPS standard says 255 */
#define H264_HRD_MAX_CPB_CNT                            8               /* Used in HRD data, in VUI, in SPS, standard says 32 */

#define H264_STANDARD_MAX_PICTURE_PARAMETER_SETS        256
#define H264_MAX_PICTURE_PARAMETER_SETS                 256             /* Standard says 255 */
#define H264_MAX_SLICE_GROUPS                           1               /* Standard varies for different levels - the firmware supports only 1 (NO FMO) */
#define H264_MAX_PIC_SIZE_IN_MAP_UNITS                  1               /* Standard waffles a bit about this, I expect 1 is way out */

#define H264_MAX_REF_L0_IDX_ACTIVE                      32              /* Standard says 32 */
#define H264_MAX_REF_L1_IDX_ACTIVE                      32              /* Standard says 32 */
#define H264_MAX_REF_PIC_REORDER_OPERATIONS             64              /* I dont know what the standard says */
#define H264_MAX_MEMORY_MANAGEMENT_OPERATIONS           32              /* I dont know what the standard says */

#define H264_MAX_REFERENCE_FRAMES                       16              /* Standard says 32 I believe - firmware constrains us*/
#define H264_MAX_REFERENCE_FIELDS                       32              /* Twice the max frames */


#define H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS               3               /* Forced to 3 by standard */
#define H264_SEI_MAX_PAN_SCAN_VALUES                    3               /* Forced to 3 by standard */

//
// Constants defining the NAL unit types
//

#define NALU_TYPE_SLICE                                 1               // Slice non-idr picture
#define NALU_TYPE_DPA                                   2               // Slice data partition A
#define NALU_TYPE_DPB                                   3               // Slice data partition B
#define NALU_TYPE_DPC                                   4               // Slice data partition C
#define NALU_TYPE_IDR                                   5               // Slice (instantaneous decoding refresh), nothing after this depends on anything before.
#define NALU_TYPE_SEI                                   6               // Supplemental Enhancement Information
#define NALU_TYPE_SPS                                   7               // Sequence Parameter Set
#define NALU_TYPE_PPS                                   8               // Picture Parameter Set
#define NALU_TYPE_PD                                    9               // Access unit delimiter
#define NALU_TYPE_EOSEQ                                 10              // End Of SEQuence
#define NALU_TYPE_EOSTREAM                              11              // End Of STREAM
#define NALU_TYPE_FILL                                  12              // Filler data
#define NALU_TYPE_SPS_EXT                               13              // Sequence Parameter Set Extension
#define NALU_TYPE_AUX_SLICE                             19              // Slice Coded Auxillary pictue

#define NALU_TYPE_PLAYER2_CONTAINER_PARAMETERS          24

//
// Dont know yet
//

#define NALU_PRIORITY_HIGHEST                           3
#define NALU_PRIORITY_HIGH                              2
#define NALU_PRIRITY_LOW                                1
#define NALU_PRIORITY_DISPOSABLE                        0

//
// Constants defining the supplemental
//

#define SEI_BUFFERING_PERIOD                            0
#define SEI_PIC_TIMING                                  1
#define SEI_PAN_SCAN_RECT                               2
#define SEI_FILLER_PAYLOAD                              3
#define SEI_USER_DATA_REGISTERED_ITU_T_T35              4
#define SEI_USER_DATA_UNREGISTERED                      5
#define SEI_RECOVERY_POINT                              6
#define SEI_DEC_REF_PIC_MARKING_REPETITION              7
#define SEI_SPARE_PIC                                   8
#define SEI_SCENE_INFO                                  9
#define SEI_SUB_SEQ_INFO                                10
#define SEI_SUB_SEQ_LAYER_CHARACTERISTICS               11
#define SEI_SUB_SEQ_CHARACTERISTICS                     12
#define SEI_FULL_FRAME_FREEZE                           13
#define SEI_FULL_FRAME_FREEZE_RELEASE                   14
#define SEI_FULL_FRAME_SNAPSHOT                         15
#define SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START        16
#define SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END          17
#define SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET          18
#define SEI_FILM_GRAIN_CHARACTERISTICS                  19
#define SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE        20
#define SEI_STEREO_VIDEO_INFO                           21
#define SEI_RESERVED_SEI_MESSAGE                        22

//
// Some of the SEI message constants
//

#define SEI_PICTURE_TIMING_PICSTRUCT_FRAME              0
#define SEI_PICTURE_TIMING_PICSTRUCT_TOP_FIELD          1
#define SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_FIELD       2
#define SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM         3
#define SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP         4
#define SEI_PICTURE_TIMING_PICSTRUCT_TOP_BOTTOM_TOP     5
#define SEI_PICTURE_TIMING_PICSTRUCT_BOTTOM_TOP_BOTTOM  6
#define SEI_PICTURE_TIMING_PICSTRUCT_FRAME_DOUBLING     7
#define SEI_PICTURE_TIMING_PICSTRUCT_FRAME_TRIPLING     8

//
// Profile constants
//

#define H264_PROFILE_IDC_BASELINE                       66
#define H264_PROFILE_IDC_MAIN                           77
#define H264_PROFILE_IDC_EXTENDED                       88
#define H264_PROFILE_IDC_HIGH                           100
#define H264_PROFILE_IDC_HIGH10                         110
#define H264_PROFILE_IDC_HIGH422                        122
#define H264_PROFILE_IDC_HIGH444                        144

//
// VUI aspect ration constants
//

#define H264_ASPECT_RATIO_UNSPECIFIED                   0
#define H264_ASPECT_RATIO_1_TO_1                        1
#define H264_ASPECT_RATIO_12_TO_11                      2
#define H264_ASPECT_RATIO_10_TO_11                      3
#define H264_ASPECT_RATIO_16_TO_11                      4
#define H264_ASPECT_RATIO_40_TO_33                      5
#define H264_ASPECT_RATIO_24_TO_11                      6
#define H264_ASPECT_RATIO_20_TO_11                      7
#define H264_ASPECT_RATIO_32_TO_11                      8
#define H264_ASPECT_RATIO_80_TO_33                      9
#define H264_ASPECT_RATIO_18_TO_11                      10
#define H264_ASPECT_RATIO_15_TO_11                      11
#define H264_ASPECT_RATIO_64_TO_33                      12
#define H264_ASPECT_RATIO_160_TO_99                     13
#define H264_ASPECT_RATIO_EXTENDED_SAR                  255

//
// Picture Parameter Set slice group map constants
//

#define H264_SLICE_GROUP_MAP_INTERLEAVED                0
#define H264_SLICE_GROUP_MAP_DISPERSED                  1
#define H264_SLICE_GROUP_MAP_FOREGROUND_AND_LEFTOVER    2
#define H264_SLICE_GROUP_MAP_CHANGING0                  3
#define H264_SLICE_GROUP_MAP_CHANGING1                  4
#define H264_SLICE_GROUP_MAP_CHANGING2                  5
#define H264_SLICE_GROUP_MAP_EXPLICIT                   6

//
// Slice header slice type constants
//

#define H264_SLICE_TYPE_P                               0
#define H264_SLICE_TYPE_B                               1
#define H264_SLICE_TYPE_I                               2
#define H264_SLICE_TYPE_SP                              3
#define H264_SLICE_TYPE_SI                              4
#define H264_SLICE_TYPE_P_ALL                           5
#define H264_SLICE_TYPE_B_ALL                           6
#define H264_SLICE_TYPE_I_ALL                           7
#define H264_SLICE_TYPE_SP_ALL                          8
#define H264_SLICE_TYPE_SI_ALL                          9

#define SLICE_TYPE_IS( v, slice_type )                  (((v) == (slice_type)) || ((v) == ((slice_type) + H264_SLICE_TYPE_P_ALL)))

//
// Memory management control operations
//

#define H264_MMC_END                                                    0
#define H264_MMC_MARK_SHORT_TERM_UNUSED_FOR_REFERENCE                   1
#define H264_MMC_MARK_LONG_TERM_UNUSED_FOR_REFERENCE                    2
#define H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_SHORT_TERM_PICTURE           3
#define H264_MMC_SPECIFY_MAX_LONG_TERM_INDEX                            4
#define H264_MMC_CLEAR                                                  5
#define H264_MMC_ASSIGN_LONG_TERM_INDEX_TO_CURRENT_DECODED_PICTURE      6

#define NO_LONG_TERM_FRAME_INDICES                                      0xffffffff

//
// Colour matrix coeffient codes
//

#define  H264_MATRIX_COEFFICIENTS_IDENTITY		0		// already in RGB
#define  H264_MATRIX_COEFFICIENTS_BT709			1
#define  H264_MATRIX_COEFFICIENTS_UNSPECIFIED		2
#define  H264_MATRIX_COEFFICIENTS_RESERVED		3
#define  H264_MATRIX_COEFFICIENTS_FCC			4
#define  H264_MATRIX_COEFFICIENTS_BT470_BGI		5
#define  H264_MATRIX_COEFFICIENTS_SMPTE_170M		6
#define  H264_MATRIX_COEFFICIENTS_SMPTE_240M		7
#define  H264_MATRIX_COEFFICIENTS_YCGCO			8

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching H264 headers
//

//
// Hrd parameters
//

typedef struct H264HrdParameters_s
{
    unsigned int                cpb_cnt_minus1;                                 // ue(v)
    unsigned int                bit_rate_scale;                                 // u(4)
    unsigned int                cpb_size_scale;                                 // u(4)
    unsigned int                bit_rate_value_minus1[H264_HRD_MAX_CPB_CNT];    // ue(v)
    unsigned int                cpb_size_value_minus1[H264_HRD_MAX_CPB_CNT];    // ue(v)
    unsigned int                cbr_flag[H264_HRD_MAX_CPB_CNT];                 // u(1)
    unsigned int                initial_cpb_removal_delay_length_minus1;        // u(5)
    unsigned int                cpb_removal_delay_length_minus1;                // u(5)
    unsigned int                cpb_output_delay_length_minus1;                 // u(5)
    unsigned int                time_offset_length;                             // u(5)
} H264HrdParameters_t;

//
// Video Usability Information
//

typedef struct H264VUISequenceParameters_s
{
    unsigned int                aspect_ratio_info_present_flag;                 // u(1)
    unsigned int                aspect_ratio_idc;                               // u(8)
    unsigned int                sar_width;                                      // u(16)
    unsigned int                sar_height;                                     // u(16)
    unsigned int                overscan_info_present_flag;                     // u(1)
    unsigned int                overscan_appropriate_flag;                      // u(1)
    unsigned int                video_signal_type_present_flag;                 // u(1)
    unsigned int                video_format;                                   // u(3)
    unsigned int                video_full_range_flag;                          // u(1)
    unsigned int                colour_description_present_flag;                // u(1)
    unsigned int                colour_primaries;                               // u(8)
    unsigned int                transfer_characteristics;                       // u(8)
    unsigned int                matrix_coefficients;                            // u(8)
    unsigned int                chroma_loc_info_present_flag;                   // u(1)
    unsigned int                chroma_sample_loc_type_top_field;               // ue(v)
    unsigned int                chroma_sample_loc_type_bottom_field;            // ue(v)
    unsigned int                timing_info_present_flag;                       // u(1)
    unsigned int                num_units_in_tick;                              // u(32)
    unsigned int                time_scale;                                     // u(32)
    unsigned int                fixed_frame_rate_flag;                          // u(1)
    unsigned int                nal_hrd_parameters_present_flag;                // u(1)
    H264HrdParameters_t         nal_hrd_parameters;                             // H264HrdParameters_t
    unsigned int                vcl_hrd_parameters_present_flag;                // u(1)
    H264HrdParameters_t         vcl_hrd_parameters;                             // H264HrdParameters_t
    unsigned int                low_delay_hrd_flag;                             // u(1)
    unsigned int                pict_struct_present_flag;                       // u(1)
    unsigned int                bitstream_restriction_flag;                     // u(1)
    unsigned int                motion_vectors_over_pic_boundaries_flag;        // u(1)
    unsigned int                max_bytes_per_pic_denom;                        // ue(v)
    unsigned int                max_bits_per_mb_denom;                          // ue(v)
    unsigned int                log2_max_mv_length_horizontal;                  // ue(v)
    unsigned int                log2_max_mv_length_vertical;                    // ue(v)
    unsigned int                num_reorder_frames;                             // ue(v)
    unsigned int                max_dec_frame_buffering;                        // ue(v)
} H264VUISequenceParameters_t;

//
// The Sequence parameter set structure
//

typedef struct H264SequenceParameterSetHeader_s
{
    unsigned int                profile_idc;                                    // u(8)
    unsigned int                constrained_set0_flag;                          // u(1)
    unsigned int                constrained_set1_flag;                          // u(1)
    unsigned int                constrained_set2_flag;                          // u(1)
    unsigned int                constrained_set3_flag;                          // u(1)
    // reserved zero bits                                                       // u(4)
    unsigned int                level_idc;                                      // u(8)
    unsigned int                seq_parameter_set_id;                           // ue(v)
    unsigned int                chroma_format_idc;                              // ue(v)
    unsigned int                residual_colour_transform_flag;                 // u(1)
    unsigned int                bit_depth_luma_minus8;                          // ue(v)
    unsigned int                bit_depth_chroma_minus8;                        // ue(v)
    unsigned int                qpprime_y_zero_transform_bypass_flag;           // u(1)
    unsigned int                seq_scaling_matrix_present_flag;                // u(1)
    unsigned int                seq_scaling_list_present_flag[8];               // u(1)
    unsigned int                ScalingList4x4[6][16];                          // Derived
    unsigned int                UseDefaultScalingMatrix4x4Flag[6];              // Derived
    unsigned int                ScalingList8x8[2][64];                          // Derived
    unsigned int                UseDefaultScalingMatrix8x8Flag[2];              // Derived
    unsigned int                log2_max_frame_num_minus4;                      // ue(v)
    unsigned int                pic_order_cnt_type;                             // ue(v)
    unsigned int                log2_max_pic_order_cnt_lsb_minus4;              // ue(v)
    unsigned int                delta_pic_order_always_zero_flag;               // u(1)
    int                         offset_for_non_ref_pic;                         // se(v)
    int                         offset_for_top_to_bottom_field;                 // se(v)
    unsigned int                num_ref_frames_in_pic_order_cnt_cycle;          // ue(v)
    int                         offset_for_ref_frame[H264_MAX_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE];       // se(v)
    unsigned int                num_ref_frames;                                 // ue(v)
    unsigned int                gaps_in_frame_num_value_allowed_flag;           // u(1)
    unsigned int                pic_width_in_mbs_minus1;                        // ue(v)
    unsigned int                pic_height_in_map_units_minus1;                 // ue(v)
    unsigned int                frame_mbs_only_flag;                            // u(1)
    unsigned int                mb_adaptive_frame_field_flag;                   // u(1)
    unsigned int                direct_8x8_inference_flag;                      // u(1)
    unsigned int                frame_cropping_flag;                            // u(1)
    unsigned int                frame_cropping_rect_left_offset;                // ue(v)
    unsigned int                frame_cropping_rect_right_offset;               // ue(v)
    unsigned int                frame_cropping_rect_top_offset;                 // ue(v)
    unsigned int                frame_cropping_rect_bottom_offset;              // ue(v)
    unsigned int                vui_parameters_present_flag;                    // u(1)
    H264VUISequenceParameters_t vui_seq_parameters;                             // H264VUISequenceParameters_t
} H264SequenceParameterSetHeader_t;

//
// The Sequence parameter set extension structure
//

typedef struct H264SequenceParameterSetExtensionHeader_s
{
    unsigned int                seq_parameter_set_id;                           // ue(v)
    unsigned int                aux_format_idc;                                 // ue(v)
    unsigned int                bit_depth_aux_minus8;                           // ue(v)
    unsigned int                alpha_incr_flag;                                // ue(1)
    unsigned int                alpha_opaque_value;                             // u(v)
    unsigned int                alpha_transparent_value;                        // u(v)
    unsigned int                additional_extension_flag;                      // ue(1)
} H264SequenceParameterSetExtensionHeader_t;

//
// The Picture parameter set structure
//

typedef struct H264PictureParameterSetHeader_s
{
    unsigned int                pic_parameter_set_id;                           // ue(v)
    unsigned int                seq_parameter_set_id;                           // ue(v)
    unsigned int                entropy_coding_mode_flag;                       // u(1)
    unsigned int                pic_order_present_flag;                         // u(1)
    unsigned int                num_slice_groups_minus1;                        // ue(v)
    unsigned int                slice_group_map_type;                           // ue(v)
    unsigned int                run_length_minus1[H264_MAX_SLICE_GROUPS];       // ue(v)
    unsigned int                top_left[H264_MAX_SLICE_GROUPS];                // ue(v)
    unsigned int                bottom_right[H264_MAX_SLICE_GROUPS];            // ue(v)
    unsigned int                slice_group_change_direction_flag;              // u(1)
    unsigned int                slice_group_change_rate_minus1;                 // ue(v)
    unsigned int                pic_size_in_map_units_minus1;                   // ue(v)
    unsigned int                slice_group_id[H264_MAX_PIC_SIZE_IN_MAP_UNITS]; // u(Ceil(log2(num_slice_groups_minus1+1)))
    unsigned int                num_ref_idx_l0_active_minus1;                   // ue(v)
    unsigned int                num_ref_idx_l1_active_minus1;                   // ue(v)
    unsigned int                weighted_pred_flag;                             // u(1)
    unsigned int                weighted_bipred_idc;                            // u(2)
    int                         pic_init_qp_minus26;                            // se(v)
    int                         pic_init_qs_minus26;                            // se(v)
    int                         chroma_qp_index_offset;                         // se(v)
    unsigned int                deblocking_filter_control_present_flag;         // u(1)
    unsigned int                constrained_intra_pred_flag;                    // u(1)
    unsigned int                redundant_pic_cnt_present_flag;                 // u(1)

    unsigned int                transform_8x8_mode_flag;                        // u(1)
    unsigned int                pic_scaling_matrix_present_flag;                // u(1)
    unsigned int                pic_scaling_list_present_flag[8];               // u(1)
    unsigned int                ScalingList4x4[6][16];                          // Derived
    unsigned int                UseDefaultScalingMatrix4x4Flag[6];              // Derived
    unsigned int                ScalingList8x8[2][64];                          // Derived
    unsigned int                UseDefaultScalingMatrix8x8Flag[2];              // Derived
    int                         second_chroma_qp_index_offset;                  // se(v)

    //
    // Supplementary data
    //

    H264SequenceParameterSetHeader_t            *SequenceParameterSet;          // Pointer to the appropriate sequence parameter set.
    H264SequenceParameterSetExtensionHeader_t   *SequenceParameterSetExtension; // Pointer to the appropriate sequence parameter set extension.

} H264PictureParameterSetHeader_t;

//
// Reference picture list re-ordering data (slice header sub-structure)
//

typedef struct H264RefPicListReordering_s
{
    unsigned int                ref_pic_list_reordering_flag_l0;                                        // u(1)
    unsigned int                l0_reordering_of_pic_nums_idc[H264_MAX_REF_PIC_REORDER_OPERATIONS];     // ue(v)
    unsigned int                l0_abs_diff_pic_num_minus1[H264_MAX_REF_PIC_REORDER_OPERATIONS];        // ue(v)
    unsigned int                l0_long_term_pic_num[H264_MAX_REF_PIC_REORDER_OPERATIONS];              // ue(v)
    unsigned int                ref_pic_list_reordering_flag_l1;                                        // u(1)
    unsigned int                l1_reordering_of_pic_nums_idc[H264_MAX_REF_PIC_REORDER_OPERATIONS];     // ue(v)
    unsigned int                l1_abs_diff_pic_num_minus1[H264_MAX_REF_PIC_REORDER_OPERATIONS];        // ue(v)
    unsigned int                l1_long_term_pic_num[H264_MAX_REF_PIC_REORDER_OPERATIONS];              // ue(v)
} H264RefPicListReordering_t;

//
// Prediction weight table (slice header sub-structure)
//

typedef struct H264PredWeightTable_s
{
    unsigned int                luma_log2_weight_denom;                                         // ue(v)
    unsigned int                chroma_log2_weight_denom;                                       // ue(v)
    unsigned int                luma_weight_l0_flag[H264_MAX_REF_L0_IDX_ACTIVE];                // u(1)
    int                         luma_weight_l0[H264_MAX_REF_L0_IDX_ACTIVE];                     // se(v)
    int                         luma_offset_l0[H264_MAX_REF_L0_IDX_ACTIVE];                     // se(v)
    unsigned int                chroma_weight_l0_flag[H264_MAX_REF_L0_IDX_ACTIVE];              // u(1)
    int                         chroma_weight_l0[H264_MAX_REF_L0_IDX_ACTIVE][2];                // se(v)
    int                         chroma_offset_l0[H264_MAX_REF_L0_IDX_ACTIVE][2];                // se(v)
    unsigned int                luma_weight_l1_flag[H264_MAX_REF_L1_IDX_ACTIVE];                // u(1)
    int                         luma_weight_l1[H264_MAX_REF_L1_IDX_ACTIVE];                     // se(v)
    int                         luma_offset_l1[H264_MAX_REF_L1_IDX_ACTIVE];                     // se(v)
    unsigned int                chroma_weight_l1_flag[H264_MAX_REF_L1_IDX_ACTIVE];              // u(1)
    int                         chroma_weight_l1[H264_MAX_REF_L1_IDX_ACTIVE][2];                // se(v)
    int                         chroma_offset_l1[H264_MAX_REF_L1_IDX_ACTIVE][2];                // se(v)
} H264PredWeightTable_t;

//
// Decoded reference picture marking table (slice header sub-structure)
//

typedef struct H264DecRefPicMarking_s
{
    unsigned int                no_ouptput_of_prior_pics_flag;                                                  // u(1)
    unsigned int                long_term_reference_flag;                                                       // u(1)
    unsigned int                adaptive_ref_pic_marking_mode_flag;                                             // u(1)
    unsigned int                memory_management_control_operation[H264_MAX_MEMORY_MANAGEMENT_OPERATIONS];     // ue(v)
    unsigned int                difference_of_pic_nums_minus1[H264_MAX_MEMORY_MANAGEMENT_OPERATIONS];           // ue(v)
    unsigned int                long_term_pic_num[H264_MAX_MEMORY_MANAGEMENT_OPERATIONS];                       // ue(v)
    unsigned int                long_term_frame_idx[H264_MAX_MEMORY_MANAGEMENT_OPERATIONS];                     // ue(v)
    unsigned int                max_long_term_frame_idx_plus1[H264_MAX_MEMORY_MANAGEMENT_OPERATIONS];           // ue(v)
} H264DecRefPicMarking_t;

//
// Supplemental Enhancement Information - Picture timing record
//

typedef struct H264SEIPictureTiming_s
{
    unsigned int                cpb_removal_delay;                                                              // u()
    unsigned int                dpb_output_delay;                                                               // u()
    unsigned int                pic_struct;                                                                     // u(4)
    unsigned int                clock_timestamp_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                        // u(1)
    unsigned int                ct_type[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                     // u(2)
    unsigned int                nuit_field_based_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                       // u(1)
    unsigned int                counting_type[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                               // u(5)
    unsigned int                full_timestamp_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                         // u(1)
    unsigned int                discontinuity_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                          // u(1)
    unsigned int                cnt_dropped_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                            // u(1)
    unsigned int                n_frames[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                    // u(8)
    unsigned int                seconds_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                // u(1)
    unsigned int                seconds_value[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                               // u(6)
    unsigned int                minutes_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                // u(1)
    unsigned int                minutes_value[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                               // u(6)
    unsigned int                hours_flag[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                  // u(1)
    unsigned int                hours_value[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                 // u(5)
    unsigned int                time_offset[H264_SEI_MAX_NUM_CLOCK_TIMESTAMPS];                                 // i(v)

// derived value

    unsigned int                NumClockTS;
    unsigned int                Valid;                                          // indicates the message structure is valid.
} H264SEIPictureTiming_t;

//

typedef struct H264SEIPanScanRectangle_s
{
    unsigned int                pan_scan_rect_id;                                                               // ue(v)
    unsigned int                pan_scan_rect_cancel_flag;                                                      // u(1)
    unsigned int                pan_scan_cnt_minus1;                                                            // ue(v)
    int                         pan_scan_rect_left_offset[H264_SEI_MAX_PAN_SCAN_VALUES];                        // se(v)
    int                         pan_scan_rect_right_offset[H264_SEI_MAX_PAN_SCAN_VALUES];                       // se(v)
    int                         pan_scan_rect_top_offset[H264_SEI_MAX_PAN_SCAN_VALUES];                         // se(v)
    int                         pan_scan_rect_bottom_offset[H264_SEI_MAX_PAN_SCAN_VALUES];                      // se(v)
    unsigned int                pan_scan_rect_repetition_period;                                                // ue(v)

// derived value

    unsigned int                Valid;                                          // indicates the message structure is valid.
} H264SEIPanScanRectangle_t;

//
// The Slice header
//

typedef struct H264SliceHeader_s
{
    unsigned int                first_mb_in_slice;                              // ue(v)
    unsigned int                slice_type;                                     // ue(v)
    unsigned int                pic_parameter_set_id;                           // ue(v)
    unsigned int                frame_num;                                      // u(SeqParam->log2_max_frame_num_minus4 + 4)
    unsigned int                field_pic_flag;                                 // u(1)
    unsigned int                bottom_field_flag;                              // u(1)
    unsigned int                idr_pic_id;                                     // ue(v)
    unsigned int                pic_order_cnt_lsb;                              // u(SeqParam->log2_max_pic_order_cnt_lsb_minus4 + 4)
    int                         delta_pic_order_cnt_bottom;                     // se(v)
    int                         delta_pic_order_cnt[2];                         // se(v) * 2
    unsigned int                redundant_pic_cnt;                              // ue(v)
    unsigned int                direct_spatial_mv_pred_flag;                    // u(1)
    unsigned int                num_ref_idx_active_override_flag;               // u(1)
    unsigned int                num_ref_idx_l0_active_minus1;                   // ue(v)
    unsigned int                num_ref_idx_l1_active_minus1;                   // ue(v)
    H264RefPicListReordering_t  ref_pic_list_reordering;                        // H264RefPicListReordering_t
    H264PredWeightTable_t       pred_weight_table;                              // H264PredWeightTable_t
    H264DecRefPicMarking_t      dec_ref_pic_marking;                            // H264DecRefPicMarking_t
    unsigned int                cabac_init_idc;                                 // ue(v)
    int                         slice_qp_delta;                                 // se(v)
    unsigned int                sp_for_switch_flag;                             // u(1)
    int                         slice_qs_delta;                                 // se(v)
    unsigned int                disable_deblocking_filter_idc;                  // ue(v)
    int                         slice_alpha_c0_offset_div2;                     // se(v)
    int                         slice_beta_offset_div2;                         // se(v)
    unsigned int                slice_group_change_cycle;                       // u(Ceil(log2( ((PicParam->pic_size_in_map_units_minus1 + 1)/(PicParam->slice_group_change_rate_minus1 + 1)) + 1)))

    //
    // Supplementary data
    //

    unsigned int                nal_unit_type;                                  // Nal header fields, relevent to slice header content
    unsigned int                nal_ref_idc;

    H264SequenceParameterSetHeader_t            *SequenceParameterSet;          // Pointer to the appropriate sequence parameter set.
    H264SequenceParameterSetExtensionHeader_t   *SequenceParameterSetExtension; // Pointer to the appropriate sequence parameter set extension.
    H264PictureParameterSetHeader_t             *PictureParameterSet;           // Pointer to the appropriate picture parameter set.

    //
    // Derived data - generally derived in the standard
    //

    int                         PicOrderCntTop;
    int                         PicOrderCntBot;
    int                         PicOrderCnt;
    unsigned long long          ExtendedPicOrderCnt;                            // Pic order count adjusted to be monotonically increasing 
										// over multiples sequeneces.

    unsigned int                EntryPicOrderCntMsb;                            // Values used in the derivation
    unsigned int                ExitPicOrderCntMsb;
    unsigned int                ExitPicOrderCntMsbForced;

    unsigned int                CropUnitX;
    unsigned int                CropUnitY;

    unsigned int                Valid;                                          // indicates the slice header is valid.

} H264SliceHeader_t;

// ////////////////////////////////////////////////////////////////////////////
//
//  Specific defines and constants for the codec layer
//

typedef struct H264StreamParameters_s
{
    H264SequenceParameterSetHeader_t            *SequenceParameterSet;
    H264SequenceParameterSetExtensionHeader_t   *SequenceParameterSetExtension;
    H264PictureParameterSetHeader_t             *PictureParameterSet;
} H264StreamParameters_t;

#define BUFFER_H264_STREAM_PARAMETERS           "H264StreamParameters"
#define BUFFER_H264_STREAM_PARAMETERS_TYPE      {BUFFER_H264_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(H264StreamParameters_t)}

//

typedef struct H264FrameParameters_s
{
    H264SliceHeader_t                           SliceHeader;

    H264SEIPanScanRectangle_t                   SEIPanScanRectangle;            // Current pan scan rectangle.
} H264FrameParameters_t;

#define BUFFER_H264_FRAME_PARAMETERS        "H264FrameParameters"
#define BUFFER_H264_FRAME_PARAMETERS_TYPE   {BUFFER_H264_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(H264FrameParameters_t)}

//

#define H264_NUM_REF_FRAME_LISTS                3
#define P_REF_PIC_LIST                          0
#define B_REF_PIC_LIST_0                        1
#define B_REF_PIC_LIST_1                        2

//

#define REF_PIC_USE_FRAME                       0
#define REF_PIC_USE_FIELD_TOP                   1
#define REF_PIC_USE_FIELD_BOT                   2

typedef struct H264ReferenceDetails_s
{
    int                 PictureNumber;
    int                 PicOrderCnt;
    int                 PicOrderCntTop;
    int                 PicOrderCntBot;
    unsigned int        UsageCode;
    bool                LongTermReference;
} H264ReferenceDetails_t;


//

#endif
