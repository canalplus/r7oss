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

#ifndef H_HEVC
#define H_HEVC

#include "osinline.h"
#include <hevc_video_transformer_types.h> // MME API

#include "refdetails.h" // existing ReferenceDetails structure reused (partially) for hevc

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

// ////////////////////////////////////////////////////////////////////////////
//
// Definitions and constraints set by HEVC standard
//

#define HEVC_LOG2_MAX_CU_SIZE 6 //!< Log2(Maximum size of CU supported in luma pixels)
#define HEVC_LOG2_MAX_TU_SIZE 5 //!< Log2(Maximum transform size supported)

#define MAX_TLAYER                                      8   //!< max number of temporal layer
#define MAX_CPB_CNT                                     32  //!< Upper bound of (cpb_cnt_minus1 + 1)
#define MAX_VPS_NUM_HRD_PARAMETERS                      1   // Should be 1024 according to standard, but it's a lot of memory, need to create a memory pool
#define MAX_VPS_OP_SETS_PLUS1                           1024
#define MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1        1
#define HEVC_MAX_VIDEO_PARAMETER_SETS                   80  // provision for pictures in the pipeline

#define HEVC_MAX_REFERENCE_INDEX                        15

//! Maximum picture width (in pixels) supported by Hades
#define HEVC_STANDARD_MAX_PIC_WIDTH_IN_LUMA_SAMPLE   4096
//! Maximum picture height (in pixels) supported by Hades
#define HEVC_STANDARD_MAX_PIC_HEIGHT_IN_LUMA_SAMPLE  2176

#define HEVC_LOG2_MAX_CU_SIZE 6 //!< Log2(Maximum size of CU supported in luma pixels)
#define HEVC_LOG2_MAX_TU_SIZE 5 //!< Log2(Maximum transform size supported)

#define MAX_TLAYER                                      8   //!< max number of temporal layer
#define MAX_CPB_CNT                                     32  //!< Upper bound of (cpb_cnt_minus1 + 1)
#define MAX_VPS_NUM_HRD_PARAMETERS                      1
#define MAX_VPS_OP_SETS_PLUS1                           1024
#define MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1        1

#define HEVC_MAX_LONG_TERM_SPS              32

//
// Constants defining the NAL unit types
//

//! NAL unit types
// suffix key: N: sub-layer non-reference picture, R: sub-layer reference picture
enum
{
    NAL_UNIT_CODED_SLICE_TRAIL_N = 0,// 0 Trailing picture (follows IRAP in output order)
    NAL_UNIT_CODED_SLICE_TRAIL_R,    // 1
    NAL_UNIT_CODED_SLICE_TSA_N,      // 2 Temporal Sub-layer Access
    NAL_UNIT_CODED_SLICE_TLA_R,      // 3
    NAL_UNIT_CODED_SLICE_STSA_N,     // 4 Step-wise Temporal Sub-layer Access
    NAL_UNIT_CODED_SLICE_STSA_R,     // 5
    NAL_UNIT_CODED_SLICE_RADL_N,     // 6 Random Access Decodable Leading (precedes IRAP in output order)
    NAL_UNIT_CODED_SLICE_RADL_R,     // 7
    NAL_UNIT_CODED_SLICE_RASL_N,     // 8 Random Access Skipped Leading
    NAL_UNIT_CODED_SLICE_RASL_R,     // 9
    NAL_UNIT_RESERVED_VCL_N10,
    NAL_UNIT_RESERVED_VCL_R11,
    NAL_UNIT_RESERVED_VCL_N12,
    NAL_UNIT_RESERVED_VCL_R13,
    NAL_UNIT_RESERVED_VCL_N14,
    NAL_UNIT_RESERVED_VCL_R15,
    NAL_UNIT_CODED_SLICE_BLA_W_LP,   // 16 Broken Link Access
    NAL_UNIT_CODED_SLICE_BLA_W_RADL, // 17
    NAL_UNIT_CODED_SLICE_BLA_N_LP,   // 18
    NAL_UNIT_CODED_SLICE_IDR_W_RADL, // 19 Instantaneous Decoding Refresh
    NAL_UNIT_CODED_SLICE_IDR_N_LP,   // 20
    NAL_UNIT_CODED_SLICE_CRA,        // 21 Clean Random Access
    NAL_UNIT_RESERVED_IRAP_VCL22,
    NAL_UNIT_RESERVED_IRAP_VCL23,
    NAL_UNIT_RESERVED_VCL24,
    NAL_UNIT_RESERVED_VCL25,
    NAL_UNIT_RESERVED_VCL26,
    NAL_UNIT_RESERVED_VCL27,
    NAL_UNIT_RESERVED_VCL28,
    NAL_UNIT_RESERVED_VCL29,
    NAL_UNIT_RESERVED_VCL30,
    NAL_UNIT_RESERVED_VCL31,
    NAL_UNIT_VPS,                    // 32
    NAL_UNIT_SPS,                    // 33
    NAL_UNIT_PPS,                    // 34
    NAL_UNIT_ACCESS_UNIT_DELIMITER,  // 35
    NAL_UNIT_EOS,                    // 36
    NAL_UNIT_EOB,                    // 37
    NAL_UNIT_FILLER_DATA,            // 38
    NAL_UNIT_PREFIX_SEI,             // 39
    NAL_UNIT_SUFFIX_SEI,             // 40
    NAL_UNIT_RESERVED_NVCL41,
    NAL_UNIT_RESERVED_NVCL42,
    NAL_UNIT_RESERVED_NVCL43,
    NAL_UNIT_RESERVED_NVCL44,
    NAL_UNIT_RESERVED_NVCL45,
    NAL_UNIT_RESERVED_NVCL46,
    NAL_UNIT_RESERVED_NVCL47,
    NAL_UNIT_UNSPECIFIED_48,
    NAL_UNIT_UNSPECIFIED_49,
    NAL_UNIT_UNSPECIFIED_50,
    NAL_UNIT_UNSPECIFIED_51,
    NAL_UNIT_UNSPECIFIED_52,
    NAL_UNIT_UNSPECIFIED_53,
    NAL_UNIT_UNSPECIFIED_54,
    NAL_UNIT_UNSPECIFIED_55,
    NAL_UNIT_UNSPECIFIED_56,
    NAL_UNIT_UNSPECIFIED_57,
    NAL_UNIT_UNSPECIFIED_58,
    NAL_UNIT_UNSPECIFIED_59,
    NAL_UNIT_UNSPECIFIED_60,
    NAL_UNIT_UNSPECIFIED_61,
    NAL_UNIT_UNSPECIFIED_62,
    NAL_UNIT_UNSPECIFIED_63,
    NAL_UNIT_INVALID,
    NALU_TYPE_NUM = NAL_UNIT_INVALID
};

// SEI Payload type
#define SEI_BUFFERING_PERIOD                            0
#define SEI_PIC_TIMING                                  1
#define SEI_PAN_SCAN_RECT                               2
#define SEI_FILLER_PAYLOAD                              3
#define SEI_USER_DATA_REGISTERED_ITU_T_T35              4
#define SEI_USER_DATA_UNREGISTERED                      5
#define SEI_RECOVERY_POINT                              6
#define SEI_SCENE_INFO                                  9
#define SEI_FULL_FRAME_SNAPSHOT                         15
#define SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START        16
#define SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END          17
#define SEI_FILM_GRAIN_CHARACTERISTICS                  19
#define SEI_POST_FILTER_HINT                            22
#define SEI_FRAME_PACKING_ARRANGEMENT                   45
#define SEI_PICTURE_DIGEST                              132

//
// VUI aspect ration constants
//

#define HEVC_ASPECT_RATIO_UNSPECIFIED                   0
#define HEVC_ASPECT_RATIO_1_TO_1                        1
#define HEVC_ASPECT_RATIO_12_TO_11                      2
#define HEVC_ASPECT_RATIO_10_TO_11                      3
#define HEVC_ASPECT_RATIO_16_TO_11                      4
#define HEVC_ASPECT_RATIO_40_TO_33                      5
#define HEVC_ASPECT_RATIO_24_TO_11                      6
#define HEVC_ASPECT_RATIO_20_TO_11                      7
#define HEVC_ASPECT_RATIO_32_TO_11                      8
#define HEVC_ASPECT_RATIO_80_TO_33                      9
#define HEVC_ASPECT_RATIO_18_TO_11                      10
#define HEVC_ASPECT_RATIO_15_TO_11                      11
#define HEVC_ASPECT_RATIO_64_TO_33                      12
#define HEVC_ASPECT_RATIO_160_TO_99                     13
#define HEVC_ASPECT_RATIO_4_TO_3                        14
#define HEVC_ASPECT_RATIO_3_TO_2                        15
#define HEVC_ASPECT_RATIO_2_TO_1                        16
#define HEVC_ASPECT_RATIO_EXTENDED_SAR                  255

#define MIN_LEGAL_HEVC_ASPECT_RATIO_CODE    0
#define MAX_LEGAL_HEVC_ASPECT_RATIO_CODE    16

#define MAX_SCALING_SIZEID      4 //!< Maximum number of SizeID signifying different transformation size for scaling.
#define MAX_SCALING_MATRIXID    6 //!< Maximum number of MatrixID for scaling.
#define MAX_SCALING_COEF        64 //!< Maximum number of coefficients

#define HEVC_STANDARD_MAX_SEQUENCE_PARAMETER_SETS       16
#define HEVC_STANDARD_MAX_PICTURE_PARAMETER_SETS        64
#define HEVC_STANDARD_MAX_VIDEO_PARAMETER_SETS          16


// ////////////////////////////////////////////////////////////////////////////
//
// Definitions and constraints set by Player2 HEVC implementation
//

#define HEVC_CODED_FRAME_COUNT                          1024
#define HEVC_MAXIMUM_FRAME_SIZE                         0x100000 // TODO CL refine that coded frame maximum size
#if defined CONFIG_32BIT || defined CONFIG_ARM
#define HEVC_STREAM_PARAMETERS_COUNT            128
#define HEVC_FRAME_PARAMETERS_COUNT         128
#else
#define HEVC_STREAM_PARAMETERS_COUNT            64
#define HEVC_FRAME_PARAMETERS_COUNT         64
#endif
#define HEVC_MAX_SEQUENCE_PARAMETER_SETS                64  // provision for pictures in the pipeline
#define HEVC_MAX_PICTURE_PARAMETER_SETS                 320 // provision for pictures in the pipeline
#define HEVC_MAX_VIDEO_PARAMETER_SETS                   80  // provision for pictures in the pipeline

//
//  Definitions of types matching HEVC headers
//

// Warning this enum has been imported from STHM' defines.h
//! Slice type identification
enum
{
    B_SLICE,  //!< Intra + Inter prediction (unidirectionnal and bidirectionnal)
    P_SLICE,  //!< Intra + Inter prediction (unidirectionnal L0 only)
    I_SLICE,  //!< Intra prediction
    NUM_SLICE_TYPES
};

//! HRD data
typedef struct hypothetical_reference_decoder
{
    bool     nal_hrd_parameters_present_flag;
    bool     vcl_hrd_parameters_present_flag;
    bool     sub_pic_cpb_params_present_flag;
    uint8_t  tick_divisor_minus2;
    uint8_t  du_cpb_removal_delay_length_minus1;
    bool     sub_pic_cpb_params_in_pic_timing_sei_flag;
    uint8_t  dpb_output_delay_du_length_minus1;
    uint8_t  bit_rate_scale;
    uint8_t  cpb_size_scale;
    uint8_t  cpb_size_du_scale;
    uint8_t  initial_cpb_removal_delay_length_minus1;
    uint8_t  au_cpb_removal_delay_length_minus1;
    uint8_t  dpb_output_delay_length_minus1;
    bool     fixed_pic_rate_general_flag[MAX_TLAYER];
    bool     fixed_pic_rate_within_cvs_flag[MAX_TLAYER];
    uint32_t elemental_duration_in_tc_minus1[MAX_TLAYER];
    bool     low_delay_hrd_flag[MAX_TLAYER];
    uint8_t  cpb_cnt_minus1[MAX_TLAYER];
    uint32_t bit_rate_value_minus1[MAX_TLAYER][2][MAX_CPB_CNT];
    uint32_t cpb_size_value_minus1[MAX_TLAYER][2][MAX_CPB_CNT];
    uint32_t bit_rate_du_value_minus1[MAX_TLAYER][2][MAX_CPB_CNT];
    uint32_t cpb_size_du_value_minus1[MAX_TLAYER][2][MAX_CPB_CNT];
    bool     cbr_flag[MAX_TLAYER][2][MAX_CPB_CNT];
} HevcHrd_t;

//! Scaling list
typedef struct
{
    uint8_t scaling_list_pred_mode_flag;
    uint8_t scaling_list_pred_matrix_id_delta;
    int16_t scaling_list_dc_coef_minus8;
    int8_t  scaling_list_delta_coef[MAX_SCALING_COEF];
} scaling_list_elem_t;

typedef scaling_list_elem_t scaling_list_t[MAX_SCALING_SIZEID][MAX_SCALING_MATRIXID];

//! Element inside a RPS
typedef struct rps_element
{
    int32_t  delta_poc; //!< Difference of between POCs of the referenced and current pictures
    bool     used_by_curr_pic_flag; //!< Flags that the referenced picture is used to decode the current one (?)
} rps_elem_t;

//! Reference picture set (RPS)
typedef struct reference_picture_set
{
    uint8_t  num_pics; //!< Number of elements in the rps
    rps_elem_t elem[HEVC_MAX_REFERENCE_INDEX]; //!< Elements of the rps
} rps_t;

//! Short term RPS
typedef struct short_term_rps
{
    rps_t rps_s0; //!< Store the s0 rps (negative delta_poc)
    rps_t rps_s1; //!< Store the s1 rps (positive delta_poc)
} st_rps_t;

// Warning vps_t, sps_t & pps_t content from hevc_dec_ipm parset.h must be rechecked with current content of HEVC norm

//! Picture parameter set (PPS)
typedef struct pic_parameter_set_rbsp
{
    bool     is_valid;                              //!< Indicates the parameter set is valid
    uint32_t pps_pic_parameter_set_id;              //!< ue(v), 0..255
    uint32_t pps_seq_parameter_set_id;              //!< ue(v), 0..31
    int32_t  init_qp_minus26;                       //!< se(v)
    bool     sign_data_hiding_flag;                 //!< u(1)
    bool     cabac_init_present_flag;               //!< u(1)
    uint8_t  num_ref_idx_l0_default_active;         //!< u(3)
    uint8_t  num_ref_idx_l1_default_active;         //!< u(3)
    bool     constrained_intra_pred_flag;           //!< u(1)
    bool     transform_skip_enabled_flag;           //!< u(1)
    bool     cu_qp_delta_enabled_flag;              //!< u(1)
    uint8_t  diff_cu_qp_delta_depth;                //!< ue(v)
    bool     dependent_slice_segments_enabled_flag; //!< u(1)
    bool     tiles_enabled_flag;                    //!< u(1)
    bool     entropy_coding_sync_enabled_flag;      //!< u(1)
    bool     uniform_spacing_flag;                  //!< u(1)
    uint16_t num_tile_columns;                      //!< ue(v), arbitrary limited to 4
    uint16_t num_tile_rows;                         //!< ue(v), arbitrary limited to 4
    uint8_t  tile_index_size;                       //!< log2(num_tile_columns*num_tile_rows)
    uint16_t tile_width[HEVC_MAX_TILE_COLUMNS];     //!< ue(v) or inferred when last or uniform_spacing_flag is true or copied from SPS
    uint16_t tile_height[HEVC_MAX_TILE_ROWS];       //!< ue(v) or inferred when last or uniform_spacing_flag is true or copied from SPS
    bool     loop_filter_across_tiles_enabled_flag; //!< u(1)
    bool     weighted_pred_flag[NUM_SLICE_TYPES];   //!< u(1)
    bool     pps_loop_filter_across_slices_enabled_flag;//!< u(1)
    bool     deblocking_filter_control_present_flag;//!< u(1)
    bool     deblocking_filter_override_enabled_flag;//!< u(1)
    bool     pps_deblocking_filter_disable_flag;    //!< u(1)
    int8_t   pps_beta_offset_div2;                  //!< se(v)
    int8_t   pps_tc_offset_div2;                    //!< se(v)
    bool     transquant_bypass_enable_flag;         //!< u(1)
    bool     pps_slice_chroma_qp_offsets_present_flag;//!< u(1)
    bool     output_flag_present_flag;              //!< u(1)
    bool     pps_scaling_list_data_present_flag;    //!< u(1)
    scaling_list_t scaling_list;                    //!< scaling_list_param()
    int8_t   pps_cb_qp_offset;                      //!< se(v)
    int8_t   pps_cr_qp_offset;                      //!< se(v)
    bool     slice_segment_header_extension_flag;   //!< u(1)
    uint8_t  log2_parallel_merge_level_minus2;      //!< ue(v)
    bool     lists_modification_present_flag;       //!< u(1)
    uint8_t  num_extra_slice_header_bits;
} HevcPictureParameterSet_t;

//! VUI data (part of the SPS)
typedef struct video_usability_information
{
    bool      aspect_ratio_info_present_flag;
    uint8_t   aspect_ratio_idc;
    uint16_t  sar_width;
    uint16_t  sar_height;
    bool      overscan_info_present_flag;
    bool      overscan_appropriate_flag;
    bool      video_signal_type_present_flag;
    uint8_t   video_format;
    bool      video_full_range_flag;
    bool      colour_description_present_flag;
    uint8_t   colour_primaries;
    uint8_t   transfer_characteristics;
    uint8_t   matrix_coefficients;
    bool      chroma_loc_info_present_flag;
    uint8_t   chroma_sample_loc_type_top_field;
    uint8_t   chroma_sample_loc_type_bottom_field;
    bool      neutral_chroma_indication_flag;
    bool      field_seq_flag;
    bool      default_display_window_flag;
    uint32_t  def_disp_win_left_offset;
    uint32_t  def_disp_win_right_offset;
    uint32_t  def_disp_win_top_offset;
    uint32_t  def_disp_win_bottom_offset;
    bool      frame_field_info_present_flag;
    bool      timing_info_present_flag;
    uint32_t  num_units_in_tick;
    uint32_t  time_scale;
    bool      poc_proportional_to_timing_flag;
    uint32_t  num_ticks_poc_diff_one_minus1;
    bool      hrd_parameters_present_flag;
    HevcHrd_t hrd;
    bool      bitstream_restriction_flag;
    bool      tiles_fixed_structure_flag;
    bool      motion_vectors_over_pic_boundaries_flag;
    bool      restricted_ref_pic_lists_flag;          //!< u(1)
    uint16_t  min_spatial_segmentation_idc;
    uint32_t  max_bytes_per_pic_denom;
    uint32_t  max_bits_per_mincu_denom;
    uint8_t   log2_max_mv_length_horizontal;
    uint8_t   log2_max_mv_length_vertical;
} HevcVui_t;

//! Sequence parameter set (SPS)
typedef struct seq_parameter_set_rbsp
{
    bool     is_valid;                             //!< Indicates the parameter set is valid
    bool     is_active;                            //!< Indicates the parameter set is in use
    uint32_t profile_idc;                          //!< u(8)
    uint32_t level_idc;                            //!< u(8)
    uint8_t  sps_seq_parameter_set_id;             //!< ue(v), 0..15
    uint8_t  sps_video_parameter_set_id;           //!< ue(v), 0..16
    uint8_t  sps_max_sub_layers;                   //!< u(3)
    uint8_t  chroma_format_idc;                    //!< ue(v), 0..3 MJ: to be aligned to lateset spec
    bool     separate_colour_plane_flag;           //!< u(1)
    uint32_t log2_max_pic_order_cnt_lsb;           //!< ue(v), 4..16
    uint32_t sps_max_dec_pic_buffering[MAX_TLAYER];//!< ue(v)
    uint8_t  sps_num_reorder_pics[MAX_TLAYER];     //!< ue(v)
    uint32_t sps_max_latency_increase[MAX_TLAYER]; //!< ue(v)
    uint32_t conf_win_left_offset;                 //!< ue(v)
    uint32_t conf_win_right_offset;                //!< ue(v)
    uint32_t conf_win_top_offset;                  //!< ue(v)
    uint32_t conf_win_bottom_offset;               //!< ue(v)
    uint32_t bit_depth_luma_minus8;                //!< ue(v)
    uint32_t bit_depth_chroma_minus8;              //!< ue(v)
    bool     pcm_enabled_flag;                     //!< u(1)
    uint8_t  pcm_bit_depth_luma_minus1;            //!< u(4)
    uint8_t  pcm_bit_depth_chroma_minus1;          //!< u(4)
    uint32_t pic_width_in_luma_samples;            //!< u(16)
    uint32_t pic_height_in_luma_samples;           //!< u(16)
    uint8_t  log2_min_coding_block_size;           //!< ue(v), 3..6
    uint8_t  log2_max_coding_block_size;           //!< ue(v), log2_min_coding_block_size..6
    uint8_t  log2_min_transform_block_size;        //!< ue(v), 2..5
    uint8_t  log2_max_transform_block_size;        //!< ue(v), log2_min_transform_block_size..5
    uint8_t  max_coding_block_depth;               //!< log2_max_coding_block_size-log2_min_transform_block_size
    uint8_t  log2_min_pcm_coding_block_size;       //!< ue(v), 3..5
    uint8_t  log2_max_pcm_coding_block_size;       //!< ue(v)
    bool     pcm_loop_filter_disable_flag;         //!< u(1)
    uint8_t  max_transform_hierarchy_depth_inter;  //!< ue(v), 0..max_coding_block_depth
    uint8_t  max_transform_hierarchy_depth_intra;  //!< ue(v), 0..max_coding_block_depth
    bool     scaling_list_enable_flag;             //!< u(1)
    bool     sps_scaling_list_data_present_flag;   //!< u(1)
    scaling_list_t scaling_list;                   //!< scaling_list_param()
    bool     cu_qp_delta_enabled_flag;             //!< u(1)
    uint16_t pic_width_in_ctb;                     //!< ceil(pic_width_in_luma_samples/ctb_size)
    uint16_t pic_height_in_ctb;                    //!< ceil(pic_height_in_luma_samples/ctb_size)
    uint32_t pic_size_in_ctb;                      //!< pic_width_in_ctb*pic_height_in_ctb
    uint8_t  iReqBitsOuter;                        //!< log2(pic_size_in_ctb)
    bool     sample_adaptive_offset_enabled_flag;  //!< u(1)
    uint8_t  num_short_term_ref_pic_sets;          //!< ue(v), 0..64
    uint8_t  log2_num_short_term_ref_pic_sets;
    st_rps_t st_rps[HEVC_MAX_SHORT_TERM_RPS];        //!< Short term RPS
    bool     long_term_ref_pics_present_flag;        //!< u(1)
    uint8_t  num_long_term_ref_pics_sps;             //!< ue(v), 0..32
    uint8_t  log2_num_long_term_ref_pics_sps;
    uint32_t lt_ref_pic_poc_lsb_sps[HEVC_MAX_LONG_TERM_SPS]; //!< u(v)
    bool     used_by_curr_pic_lt_sps_flag[HEVC_MAX_LONG_TERM_SPS]; //!< u(1)
    bool     sps_strong_intra_smoothing_enable_flag; //!< u(1)
    bool     sps_temporal_mvp_enabled_flag;          //!< u(1)
    bool     amp_enabled_flag;                       //!< u(1)
    HevcVui_t vui;                                   //!< VUI parameters
} HevcSequenceParameterSet_t;

//! Adaptation parameter set (VPS)
typedef struct video_parameter_set_rbsp
{
    uint8_t  vps_video_parameter_set_id; //!< VPS identifier
    uint8_t  vps_max_temporal_layers; //!< Maximum number of temporal scalability layers
    bool     vps_timing_info_present_flag;
    uint32_t vps_num_units_in_tick;
    uint32_t vps_time_scale;
    bool     vps_poc_proportional_to_timing_flag;
    uint32_t vps_num_ticks_poc_diff_one_minus1;
    HevcHrd_t    hrd[MAX_VPS_NUM_HRD_PARAMETERS];
} HevcVideoParameterSet_t;

typedef struct HevcSliceHeader_s
{
    // TODO CL refine this content according to HEVC and reused code from STHM' keep only what is required for SE & Preproc/fmw
    unsigned int                first_mb_in_slice;
    unsigned int                pic_parameter_set_id;                           // ue(v)
    unsigned int                slice_type;                                     // ue(v)
    unsigned int                output_flag;                                    // u(1)
    unsigned int                rap_pic_id;                                     // ue(v) noted idr_pic_id in STHM' ???
    unsigned int                no_output_of_prior_pics_flag;                   // u(1)
    unsigned int                pic_order_cnt_lsb;                              // u(v)
    unsigned int                short_term_ref_pic_set_idx;                     // ue(v)
    // what to do with slice_sample_adaptative_offset (if sps->sample_adaptive_offset_enabled_flag is set) ??
    // what to do with aps_id if adaptive_loop_filter_enabled_flag is set ??

    unsigned int                temporal_id;
    unsigned int        nal_unit_type;
    unsigned int                idr_flag;       // The current picture is an IDR
    unsigned int                rap_flag;       // The current picture is a RAP (so all slices are I)
    unsigned int                reference_flag; // The current picture is used as a reference by other pictures
    unsigned int                rasl_flag;      // Signals that the current picture is a RASL


    //
    // Supplementary data
    //

    HevcVideoParameterSet_t     *VideoParameterSet;             // Pointer to the appropriate video parameter set
    HevcSequenceParameterSet_t  *SequenceParameterSet;              // Pointer to the appropriate sequence parameter set
    HevcPictureParameterSet_t   *PictureParameterSet;               // Pointer to the appropriate picture parameter set
    scaling_list_t              *scaling_list;                  // Pointer to the appropriate scaling list in SPS or PPS

    //
    // Derived data - generally derived in the standard
    //

    int                 PicOrderCnt;
    unsigned long long          ExtendedPicOrderCnt;                            // Pic order count adjusted to be monotonically increasing
    // over multiples sequences.

    unsigned int                EntryPicOrderCntMsb;                            // Values used in the derivation
    unsigned int                ExitPicOrderCntMsb;
    unsigned int                ExitPicOrderCntMsbForced;

    unsigned int                CropUnitX;
    unsigned int                CropUnitY;

    unsigned int                Valid;                                          // indicates the slice header is valid.
} HevcSliceHeader_t;

typedef struct HevcStreamParameters_s
{
    HevcVideoParameterSet_t               *VideoParameterSet;
    HevcSequenceParameterSet_t            *SequenceParameterSet;
    HevcPictureParameterSet_t             *PictureParameterSet;
} HevcStreamParameters_t;

#define BUFFER_HEVC_STREAM_PARAMETERS           "HevcStreamParameters"
#define BUFFER_HEVC_STREAM_PARAMETERS_TYPE      {BUFFER_HEVC_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(HevcStreamParameters_t)}

typedef struct HevcFrameParameters_s
{
    HevcSliceHeader_t                           SliceHeader;
    unsigned int                                CodedSlicesSize;

#if 0 // TODO CL address the following later...
    HevcSEIPanScanRectangle_t                   SEIPanScanRectangle;            // Current pan scan rectangle.

    HevcSEIFramePackingArrangement_t            SEIFramePackingArrangement;
#endif
} HevcFrameParameters_t;

#define BUFFER_HEVC_FRAME_PARAMETERS        "HevcFrameParameters"
#define BUFFER_HEVC_FRAME_PARAMETERS_TYPE   {BUFFER_HEVC_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(HevcFrameParameters_t)}


#define HEVC_NUM_REF_FRAME_LISTS                2
#define REF_PIC_LIST_0                          0
#define REF_PIC_LIST_1                          1

#define ST_CURR0             0 //!< Index of the short term reference picture set 0 used by the current picture
#define ST_CURR1             1 //!< Index of the short term reference picture set 1 used by the current picture
#define LT_CURR              2 //!< Index of the long term reference picture set used by the current picture
#define ST_FOLL              3 //!< Index of the short term reference picture set used by the following pictures
#define LT_FOLL              4 //!< Index of the long term reference picture set used by the following pictures
#define MAX_NUM_PIC_SET      5 //!< Maximum number of picture sets
//
// ////////////////////////////////////////////////////////////////////////////
//
// Definitions and constraints set by Hades
//
// Maximum level supported by Hades
#define HADES_MAX_LEVEL_IDC (51 * 3) // level 5.1

//! Maximum picture width (in pixels) supported by Hades
#define HEVC_STANDARD_MAX_PIC_WIDTH_IN_LUMA_SAMPLE   4096
//! Maximum picture height (in pixels) supported by Hades
#define HEVC_STANDARD_MAX_PIC_HEIGHT_IN_LUMA_SAMPLE  2176

#endif
