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

#ifndef H_H264_ENCODE_NVCL
#define H_H264_ENCODE_NVCL

#include "h264_encode_hard.h"

#define absm(A) ((A)<(0) ? (-(A)):(A))
#define NVCL_SCRATCH_BUFFER_SIZE 128
#define TIMING_PRECISION 16
#define APPEND(x, y) x ## y
#define ULL(x) APPEND(x, ull)
#define SEI_90KHZ_TIME_SCALE 90000
#define SEI_TIME_SCALE SEI_90KHZ_TIME_SCALE

/* Type declaration */
#define NALU_TYPE_SLICE    1
#define NALU_TYPE_DPA      2
#define NALU_TYPE_DPB      3
#define NALU_TYPE_DPC      4
#define NALU_TYPE_IDR      5
#define NALU_TYPE_SEI      6
#define NALU_TYPE_SPS      7
#define NALU_TYPE_PPS      8
#define NALU_TYPE_AUD      9
#define NALU_TYPE_EOSEQ    10
#define NALU_TYPE_EOSTREAM 11
#define NALU_TYPE_FILL     12

#define NALU_PRIORITY_HIGHEST     3
#define NALU_PRIORITY_HIGH        2
#define NALU_PRIRITY_LOW          1
#define NALU_PRIORITY_DISPOSABLE  0
#define SPS_PROFILE_IDC_BASELINE 66
#define SPS_PROFILE_IDC_MAIN     77
#define SPS_PROFILE_IDC_EXTENDED 88
#define SPS_PROFILE_IDC_HIGH     100
#define SPS_PROFILE_IDC_HIGH_10  110
#define SPS_PROFILE_IDC_HIGH_422 122
#define SPS_PROFILE_IDC_HIGH_444 144
#define SPS_LEVEL_IDC_0_9        9    //Special level to indicate 1B for PROFILE_HIGH variants output
#define SPS_LEVEL_IDC_1_0        10
#define SPS_LEVEL_IDC_1_B        101  //Special level to indicate 1B input
#define SPS_LEVEL_IDC_1_1        11
#define SPS_LEVEL_IDC_1_2        12
#define SPS_LEVEL_IDC_1_3        13
#define SPS_LEVEL_IDC_2_0        20
#define SPS_LEVEL_IDC_2_1        21
#define SPS_LEVEL_IDC_2_2        22
#define SPS_LEVEL_IDC_3_0        30
#define SPS_LEVEL_IDC_3_1        31
#define SPS_LEVEL_IDC_3_2        32
#define SPS_LEVEL_IDC_4_0        40
#define SPS_LEVEL_IDC_4_1        41
#define SPS_LEVEL_IDC_4_2        42
#define SPS_LEVEL_IDC_5_0        50
#define SPS_LEVEL_IDC_5_1        51
#define SPS_CHROMA_FORMAT_MONO 0
#define SPS_CHROMA_FORMAT_420  1
#define SPS_CHROMA_FORMAT_422  2
#define SPS_CHROMA_FORMAT_444  3

#define PPS_ENTROPY_CODING_CAVLC 0
#define PPS_ENTROPY_CODING_CABAC 1

#define VUI_ASPECT_RATIO_NONE    0
#define VUI_ASPECT_RATIO_1_1     1
#define VUI_ASPECT_RATIO_12_11   2
#define VUI_ASPECT_RATIO_10_11   3
#define VUI_ASPECT_RATIO_16_11   4
#define VUI_ASPECT_RATIO_40_33   5
#define VUI_ASPECT_RATIO_24_11   6
#define VUI_ASPECT_RATIO_20_11   7
#define VUI_ASPECT_RATIO_32_11   8
#define VUI_ASPECT_RATIO_80_33   9
#define VUI_ASPECT_RATIO_18_11   10
#define VUI_ASPECT_RATIO_15_11   11
#define VUI_ASPECT_RATIO_64_33   12
#define VUI_ASPECT_RATIO_160_99  13
#define VUI_ASPECT_RATIO_4_3     14
#define VUI_ASPECT_RATIO_3_2     15
#define VUI_ASPECT_RATIO_2_1     16
#define VUI_ASPECT_RATIO_EXTENDED_SAR 255

#define VUI_VIDEO_FORMAT_COMPONENT   0
#define VUI_VIDEO_FORMAT_PAL         1
#define VUI_VIDEO_FORMAT_NTSC        2
#define VUI_VIDEO_FORMAT_SECAM       3
#define VUI_VIDEO_FORMAT_MAC         4
#define VUI_VIDEO_FORMAT_UNSPECIFIED 5

#define VUI_COLOR_STD_BT_709_5           1
#define VUI_COLOR_STD_UNSPECIFIED        2
#define VUI_COLOR_STD_BT_470_6_M         4
#define VUI_COLOR_STD_BT_470_6_BG        5
#define VUI_COLOR_STD_SMPTE_170M         6
#define VUI_COLOR_STD_SMPTE_240M         7
#define VUI_COLOR_STD_MAX                8


#define VUI_COLOR_BT_709_5           1
#define VUI_COLOR_UNSPECIFIED        2
#define VUI_COLOR_BT_470_6_M         4
#define VUI_COLOR_BT_470_6_BG        5
#define VUI_COLOR_SMPTE_170M         6
#define VUI_COLOR_SMPTE_240M         7
#define VUI_COLOR_FILM               8

#define VUI_TRANSFER_709_5           1
#define VUI_TRANSFER_UNSPECIFIED     2
#define VUI_TRANSFER_BT_470_6_M      4
#define VUI_TRANSFER_BT_470_6_BG     5
#define VUI_TRANSFER_SMPTE_170M      6
#define VUI_TRANSFER_SMPTE_240M      7
#define VUI_TRANSFER_LINEAR          8
#define VUI_TRANSFER_LOG_100_1       9
#define VUI_TRANSFER_LOG_316_1       10

#define VUI_MATRIX_709_5           1
#define VUI_MATRIX_UNSPECIFIED     2
#define VUI_MATRIX_BT_470_6_M      4
#define VUI_MATRIX_BT_470_6_BG     5
#define VUI_MATRIX_SMPTE_170M      6
#define VUI_MATRIX_SMPTE_240M      7
#define VUI_MATRIX_YCGCO           8

#define SEI_PIC_STRUCT_FRAME            0
#define SEI_PIC_STRUCT_FIELD_TOP        1
#define SEI_PIC_STRUCT_FIELD_BOT        2
#define SEI_PIC_STRUCT_FIELD_TOP_FIRST  3
#define SEI_PIC_STRUCT_FIELD_BOT_FIRST  4
#define SEI_PIC_STRUCT_FIELD_TOP_REPEAT 5
#define SEI_PIC_STRUCT_FIELD_BOT_REPEAT 6
#define SEI_PIC_STRUCT_FRAME_DOUBLE     7
#define SEI_PIC_STRUCT_FRAME_TRIPLE     8

#define SEI_CT_TYPE_PROGRESSIVE  0
#define SEI_CT_TYPE_INTERLACE    1

#define SEI_COUNTING_TYPE_0      0
#define SEI_COUNTING_TYPE_1      1
#define SEI_COUNTING_TYPE_2      2
#define SEI_COUNTING_TYPE_3      3
#define SEI_COUNTING_TYPE_4      4
#define SEI_COUNTING_TYPE_5      5
#define SEI_COUNTING_TYPE_6      6

#define ZEROBYTES_SHORTSTARTCODE    2
#define INITIAL_CPB_REMOVAL_DELAY_LENGTH  28 /* 24 *//* Default in H.264 */
#define CPB_REMOVAL_DELAY_LENGTH          16 /* 24 *//* Default in H.264 */
#define DPB_OUTPUT_DELAY_LENGTH           16 /* 24 *//* Default in H.264 */
#define TIME_OFFSET_LENGTH                0  /* 25 *//* Default in H.264 */

#define MAX_NUM_REF_FRAME         16
#define MAX_NUM_SLICE_GROUPS      4 /* TBC */
#define MAX_PIC_SIZE_IN_MAP_UNITS 4 /* TBC */
#define MAX_NUM_BLOCK_TS          3 /* Dependent on pic_struct Table D-1 */
#define MAX_CPB_SIZE              1 /* FIXED -Hw */

#define BITRATE_SCALE_OFFSET 6
#define CPB_SIZE_SCALE_OFFSET 4

typedef struct {
	uint32_t value;
	int32_t scale;
} value_scale_deconstruction_t;

/*! definition of SEI payload type */
typedef enum {
	SEI_BUFFERING_PERIOD,
	SEI_PICTURE_TIMING,
	SEI_PAN_SCAN_RECT,
	SEI_FILLER_PAYLOAD,
	SEI_USER_DATA_REGISTERED_ITU_T_T35,
	SEI_USER_DATA_UNREGISTERED,
	SEI_RECOVERY_POINT,
	SEI_REC_PIC_MARKING_REPETITION,
	SEI_SPARE_PIC,
	SEI_SCENE_INFO,
	SEI_SUB_SEQ_INFO,
	SEI_SUB_SEQ_LAYER_CHARACTERISTICS,
	SEI_SUB_SEQ_CHARACTERISTICS,
	SEI_FULL_FRAME_FREEZE,
	SEI_FULL_FRAME_FREEZE_RELEASE,
	SEI_FULL_FRAME_SNAPSHOT,
	SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START,
	SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END,
	SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET,
	SEI_FILM_GRAIN_CHARACTERISTICS,
	SEI_DEBLOCKING_FILTER_DISPLAY_CHARACTERISTICS,
	SEI_STEREO_VIDEO_INFO,

	SEI_POST_FILTER_HINT,
	SEI_TONE_MAPPING_INFO,
	SEI_SCALABILITY_INFO,
	SEI_SUB_PIC_SCALABLE_LAYER,
	SEI_NON_REQUIRED_LAYER_REP,
	SEI_PRIORITY_LAYER_INFO,
	SEI_LAYERS_NOT_PRESENT,
	SEI_LAYER_DEPENDENCY_CHANGE,
	SEI_SCALABLE_NESTING,
	SEI_BASE_LAYER_TEMPORAL_HRD,
	SEI_QUALITY_LAYER_INTEGRETY_CHECK,
	SEI_REDUNDANT_PIC_PROPERTY,
	SEI_TL0_DEP_REP_INDEX,
	SEI_TL_SWITCHING_POINT,
	SEI_PARALLEL_DECODING_INFO,
	SEI_MVC_SCALABLE_NESTING,
	SEI_VIEW_SCALABILITY_INFO,
	SEI_MULTIVIEW_SCENE_INFO,
	SEI_MULTIVIEW_ACQUISITION_INFO,
	SEI_NON_REQUIERED_VIEW_COMPONENT,
	SEI_VIEW_DEPENDENCY_CHANGE,
	SEI_OPERATION_POINTS_NOT_PRESENT,
	SEI_BASE_VIEW_TEMPORAL_HRD,
	SEI_FRAME_PACKING_ARRANGEMENT,
	SEI_RESERVED_MESSAGE,
	SEI_MAX_TYPE
} sei_type;

static inline const char *StringifySeiType(sei_type aSeiType)
{
	switch (aSeiType) {
		ENTRY(SEI_BUFFERING_PERIOD);
		ENTRY(SEI_PICTURE_TIMING);
		ENTRY(SEI_PAN_SCAN_RECT);
		ENTRY(SEI_FILLER_PAYLOAD);
		ENTRY(SEI_USER_DATA_REGISTERED_ITU_T_T35);
		ENTRY(SEI_USER_DATA_UNREGISTERED);
		ENTRY(SEI_RECOVERY_POINT);
		ENTRY(SEI_REC_PIC_MARKING_REPETITION);
		ENTRY(SEI_SPARE_PIC);
		ENTRY(SEI_SCENE_INFO);
		ENTRY(SEI_SUB_SEQ_INFO);
		ENTRY(SEI_SUB_SEQ_LAYER_CHARACTERISTICS);
		ENTRY(SEI_SUB_SEQ_CHARACTERISTICS);
		ENTRY(SEI_FULL_FRAME_FREEZE);
		ENTRY(SEI_FULL_FRAME_FREEZE_RELEASE);
		ENTRY(SEI_FULL_FRAME_SNAPSHOT);
		ENTRY(SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START);
		ENTRY(SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END);
		ENTRY(SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET);
		ENTRY(SEI_FILM_GRAIN_CHARACTERISTICS);
		ENTRY(SEI_DEBLOCKING_FILTER_DISPLAY_CHARACTERISTICS);
		ENTRY(SEI_STEREO_VIDEO_INFO);

		ENTRY(SEI_POST_FILTER_HINT);
		ENTRY(SEI_TONE_MAPPING_INFO);
		ENTRY(SEI_SCALABILITY_INFO);
		ENTRY(SEI_SUB_PIC_SCALABLE_LAYER);
		ENTRY(SEI_NON_REQUIRED_LAYER_REP);
		ENTRY(SEI_PRIORITY_LAYER_INFO);
		ENTRY(SEI_LAYERS_NOT_PRESENT);
		ENTRY(SEI_LAYER_DEPENDENCY_CHANGE);
		ENTRY(SEI_SCALABLE_NESTING);
		ENTRY(SEI_BASE_LAYER_TEMPORAL_HRD);
		ENTRY(SEI_QUALITY_LAYER_INTEGRETY_CHECK);
		ENTRY(SEI_REDUNDANT_PIC_PROPERTY);
		ENTRY(SEI_TL0_DEP_REP_INDEX);
		ENTRY(SEI_TL_SWITCHING_POINT);
		ENTRY(SEI_PARALLEL_DECODING_INFO);
		ENTRY(SEI_MVC_SCALABLE_NESTING);
		ENTRY(SEI_VIEW_SCALABILITY_INFO);
		ENTRY(SEI_MULTIVIEW_SCENE_INFO);
		ENTRY(SEI_MULTIVIEW_ACQUISITION_INFO);
		ENTRY(SEI_NON_REQUIERED_VIEW_COMPONENT);
		ENTRY(SEI_VIEW_DEPENDENCY_CHANGE);
		ENTRY(SEI_OPERATION_POINTS_NOT_PRESENT);
		ENTRY(SEI_BASE_VIEW_TEMPORAL_HRD);
		ENTRY(SEI_FRAME_PACKING_ARRANGEMENT);
		ENTRY(SEI_RESERVED_MESSAGE);
		ENTRY(SEI_MAX_TYPE);
	default: return "<unknown sei type>";
	}
}

typedef enum {
	HVA_ENCODE_CAVLC = 0,
	HVA_ENCODE_CABAC = 1
} entropy_coding_mode;

static inline const char *StringifyEntropyCodingMode(entropy_coding_mode aEntropyCodingMode)
{
	switch (aEntropyCodingMode) {
		ENTRY(HVA_ENCODE_CAVLC);
		ENTRY(HVA_ENCODE_CABAC);
	default: return "<unknown entropy coding mode>";
	}
}

typedef struct {
	uint32_t cpb_cnt_minus1;
	uint32_t bit_rate_scale;
	uint32_t cpb_size_scale;
	uint32_t bit_rate_value_minus1[MAX_CPB_SIZE];
	uint32_t cpb_size_value_minus1[MAX_CPB_SIZE];
	uint32_t cbr_flag[MAX_CPB_SIZE];
	uint32_t initial_cpb_removal_delay_length_minus1;
	uint32_t cpb_removal_delay_length_minus1;
	uint32_t dpb_output_delay_length_minus1;
	uint32_t time_offset_length;
	uint32_t nvclNALUSize;
} hrd_parameter_t;

typedef struct {
	uint8_t  aspect_ratio_info_present_flag;
	uint8_t  aspect_ratio_idc;
	uint16_t sar_width;
	uint16_t sar_height;
	uint8_t  overscan_info_present_flag;
	uint8_t  overscan_appropriate_flag;
	uint8_t  video_signal_type_present_flag;
	uint8_t  video_format;
	uint8_t  video_full_range_flag;
	uint8_t  colour_description_present_flag;
	uint8_t  colour_primaries;
	uint8_t  transfer_characteristics;
	uint8_t  matrix_coefficients;
	uint8_t  chroma_loc_info_present_flag;
	uint32_t chroma_sample_loc_type_top_field;
	uint32_t chroma_sample_loc_type_bottom_field;
	uint8_t  timing_info_present_flag;
	uint32_t num_units_in_tick;
	uint32_t time_scale;
	uint8_t  fixed_frame_rate_flag;
	uint8_t  nal_hrd_parameters_present_flag;
	hrd_parameter_t nal_hrd_param;
	uint8_t  vcl_hrd_parameters_present_flag;
	hrd_parameter_t vcl_hrd_param;
	uint8_t  low_delay_hrd_flag;
	uint8_t  pic_struct_present_flag;
	uint8_t  bitstream_restriction_flag;
	uint8_t  motion_vectors_over_pic_boundaries_flag;
	uint32_t max_bytes_per_pic_denom;
	uint32_t max_bits_per_mb_denom;
	uint32_t log2_max_mv_length_horizontal;
	uint32_t log2_max_mv_length_vertical;
	uint32_t num_reorder_frames;
	uint32_t max_dec_frame_buffering;
} vui_parameter_t;

typedef struct {
	uint8_t  profile_idc;
	uint8_t  constraint_set0_flag;
	uint8_t  constraint_set1_flag;
	uint8_t  constraint_set2_flag;
	uint8_t  constraint_set3_flag;
	uint8_t  level_idc;
	uint32_t seq_parameter_set_id;
	uint32_t chroma_format_idc;
	uint8_t  residual_colour_transform_flag;
	uint32_t bit_depth_luma_minus8;
	uint32_t bit_depth_chroma_minus8;
	uint8_t  qpprime_y_zero_transform_bypass_flag;
	uint8_t  seq_scaling_matrix_present_flag;
	uint8_t  seq_scaling_list_present_flag[8]; /* fixed to zero */
	uint32_t log2_max_frame_num_minus4;
	uint32_t pic_order_cnt_type;
	uint32_t log2_max_pic_order_cnt_lsb_minus4;
	uint8_t  delta_pic_order_always_zero_flag;
	int32_t  offset_for_non_ref_pic;
	int32_t  offset_for_top_to_bottom_field;
	uint32_t num_ref_frames_in_pic_order_cnt_cycle;
	int32_t  offset_for_ref_frame[MAX_NUM_REF_FRAME];
	uint32_t num_ref_frames;
	uint8_t  gaps_in_frame_num_value_allowed_flag;
	uint32_t pic_width_in_mbs_minus1;
	uint32_t pic_height_in_map_units_minus1;
	uint8_t  frame_mbs_only_flag;
	uint8_t  mbs_adaptive_frame_field_flag;
	uint8_t  direct_8x8_inference_flag;
	uint8_t  frame_cropping_flag;
	uint32_t frame_crop_left_offset;
	uint32_t frame_crop_right_offset;
	uint32_t frame_crop_top_offset;
	uint32_t frame_crop_bottom_offset;
	uint8_t  vui_parameters_present_flag;
	vui_parameter_t vui_param;
} seq_parameter_set_rbsp_t;

typedef struct {
	uint32_t pic_parameter_set_id;
	uint32_t seq_parameter_set_id;
	uint8_t  entropy_coding_mode_flag;
	uint8_t  pic_order_present_flag;
	uint32_t num_slice_groups_minus1;
	uint32_t slice_group_map_type;
	uint32_t run_length_minus1[MAX_NUM_SLICE_GROUPS];
	uint32_t top_left[MAX_NUM_SLICE_GROUPS];
	uint32_t bottom_right[MAX_NUM_SLICE_GROUPS];
	uint8_t  slice_group_change_direction_flag;
	uint32_t slice_group_change_rate_minus1;
	uint32_t pic_size_in_map_units_minus1;
	uint32_t slice_group_id[MAX_NUM_SLICE_GROUPS];
	uint32_t num_ref_idx_10_active_minus1;
	uint32_t num_ref_idx_11_active_minus1;
	uint8_t  weighted_pred_flag;
	uint8_t  weighted_bipred_idc;
	int32_t  pic_init_qp_minus26;
	int32_t  pic_init_qs_minus26;
	int32_t  chroma_qp_index_offset;
	uint8_t  deblocking_filter_control_present_flag;
	uint8_t  contrained_intra_pred_flag;
	uint8_t  redundant_pic_cnt_present_flag;
	uint8_t  transform_8x8_mode_flag;
	uint8_t  pic_scaling_matrix_present_flag;
	uint8_t  pic_scaling_list_present_flag[8];
	int32_t  second_chroma_qp_index_offset;
} pic_parameter_set_rbsp_t;

typedef struct {
	uint32_t seq_parameter_set_id;
	uint32_t nal_initial_cbp_removal_delay;
	uint32_t nal_initial_cbp_removal_delay_offset;
	uint32_t vcl_initial_cbp_removal_delay;
	uint32_t vcl_initial_cbp_removal_delay_offset;
} sei_buffering_period_t;

typedef struct {
	uint32_t cpb_removal_delay;   //clock ticks to wait after removal from CBP the access unit with most recent buffering period before removal of current access unit
	uint32_t dpb_output_delay;    //clock ticks to wait after removal of access unit from CBP before decoded picture can be output from DPB
	uint32_t pic_struct;
	uint8_t  clock_timestamp_flag[MAX_NUM_BLOCK_TS];
	uint8_t  ct_type[MAX_NUM_BLOCK_TS];
	uint8_t  nuit_field_based_flag[MAX_NUM_BLOCK_TS];
	uint8_t  counting_type[MAX_NUM_BLOCK_TS];
	uint8_t  full_timestamp_flag[MAX_NUM_BLOCK_TS];
	uint8_t  discontinuity_flag[MAX_NUM_BLOCK_TS];
	uint8_t  cnt_dropped_flag[MAX_NUM_BLOCK_TS];
	uint8_t  n_frames[MAX_NUM_BLOCK_TS];
	uint8_t  seconds_value[MAX_NUM_BLOCK_TS];
	uint8_t  minutes_value[MAX_NUM_BLOCK_TS];
	uint8_t  hours_value[MAX_NUM_BLOCK_TS];
	uint8_t  seconds_flag[MAX_NUM_BLOCK_TS];
	uint8_t  minutes_flag[MAX_NUM_BLOCK_TS];
	uint8_t  hours_flag[MAX_NUM_BLOCK_TS];
	int32_t  time_offset[MAX_NUM_BLOCK_TS];
} sei_pic_timing_t;

typedef struct {
	uint16_t recovery_frame_cnt;
	uint8_t  exact_match_flag;
	uint8_t  broken_link_flag;
	uint8_t  changing_slice_group_idc;
} sei_recovery_point_t;


typedef struct {
	uint8_t  itu_t_t35_country_code;
	uint8_t  itu_t_t35_country_code_extension_byte;
	uint8_t *itu_t_t35_payload_byte;
} sei_user_data_registered_itu_t_t35_t;

typedef struct {
	uint64_t sei_type;
	sei_buffering_period_t buffering_period;
	sei_pic_timing_t       pic_timing;
	sei_recovery_point_t   recovery_pt;
	sei_user_data_registered_itu_t_t35_t usr_data_t35;

	uint64_t nalLastBPAURemovalTime;
	uint64_t vclLastBPAURemovalTime;
	uint64_t nalFinalArrivalTime;
	uint64_t vclFinalArrivalTime;

	uint32_t currAUts;
	uint32_t lastBPAUts;
	uint32_t forceBP;

	/* parameters from HRD */
	uint32_t initial_cpb_removal_delay_length;
	uint32_t cpb_removal_delay_length;
	uint32_t dpb_output_delay_length;
	uint32_t time_offset_length;
	/* parameters from VUI */
	uint8_t  picStructPresentFlag;

} sei_message_t;

typedef struct {
	uint32_t stuffingBits;
} filler_t;

typedef struct {
	/* parameters structures */
	seq_parameter_set_rbsp_t seqParams;
	pic_parameter_set_rbsp_t picParams;
	sei_message_t            seiMsg;
	filler_t                 fil;
	/* parameters to be updated */
	uint32_t                 seq_parameter_set_id;
	uint32_t                 pic_parameter_set_id;
	uint32_t                 nvclNALUSize; /* In bytes */
	/* parameters  to pass from Input to SEI */
	uint16_t                 brcType;
	uint16_t                 framerateNum;
	uint16_t                 framerateDen;
	uint8_t                  seiPicTimingPresentFlag;     // relates with SEI
	uint8_t                  seiBufPeriodPresentFlag;     // relates with SEI
	uint8_t                  vuiParametersPresentFlag;    /* determine pic struct flag - always present when vui present */
	/* parameters  to pass from Seq VUI to SEI */
	uint32_t                 bitrate; /* after VUI computation */
	uint8_t                  picStructPresentFlag;        /* determine pic struct flag - always present when vui present */

	/* parameters  to pass from Seq to Pic */
	uint8_t                contrainedIntraPredFlag;     // relates with PPS
	uint8_t                transform8x8ModeFlag;        // relates with PPS
	/* static parameters initialized to pass to SEI and HRD */
	uint32_t                 initial_cpb_removal_delay_length;
	uint32_t                 cpb_removal_delay_length;
	uint32_t                 dpb_output_delay_length;
	uint32_t                 time_offset_length;
	entropy_coding_mode      entropyCodingMode;
	int16_t                  chromaQpIndexOffset;       // relates with PPS
} H264EncodeNVCLContext_t;

typedef struct {
	int32_t  byte_pos;           /* current position in bitstream; */
	int32_t  bits_to_go;         /* current bitcounter */
	uint16_t byte_buf;           /* current buffer for last written byte */
	uint8_t  *streamBuf_p;        /* actual buffer for written bytes */
} bitstream_t;

typedef struct {
	int32_t  startCodePrefixLen;  /* 4 for parameter sets and first slice in picture, 3 for everything else (suggested) */
	uint32_t len;                 /* Length of the NAL unit (Excluding the start code, which does not belong to the NALU) */
	uint32_t maxLen;              /* Nal Unit Buffer size */
	int32_t  forbidden_bit;       /* should be always FALSE */
	int32_t  nal_unit_type;       /* NALU_TYPE_xxxx */
	int32_t  nal_reference_idc;   /* NALU_PRIORITY_xxxx */
	uint8_t  *buf_p;              /* contains the first byte followed by the EBSP */
} NALU_t;

typedef struct {
	int32_t  value;    /* numerical value of syntax element */
	int16_t  len;      /* length of code */
	int16_t  info;     /* info part of UVLC code */
	uint32_t codeword; /* encoded bitpattern UVLC or otherwise */
} symbol_t;

#ifdef __cplusplus
extern "C" {
#endif
/* Function prototype declaration */
uint32_t H264HwEncodeInitParamsNAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardSequenceParams_t *seqParams_p,
                                   bool newSequenceFlag);
uint32_t H264HwEncodeCreateAccessUnitDelimiterNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                                  H264EncodeHardFrameParams_t *frameParams_p, uint8_t *streamBuf_p);
uint32_t H264HwEncodeCreateSeqParamSetNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                          H264EncodeHardSequenceParams_t *seqParams_p, uint8_t *streamBuf_p);
uint32_t H264HwEncodeCreatePicParamSetNAL(H264EncodeNVCLContext_t *nVCLContext_p,
                                          H264EncodeHardFrameParams_t *frameParams_p, uint8_t *streamBuf_p);
uint32_t H264HwEncodeCreateSEINAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardFrameParams_t *frameParams_p,
                                  H264EncodeHardParamOut_t *outParams_p, uint8_t *streamBuf_p);
uint32_t H264HwEncodeEstimateSEISize(H264EncodeNVCLContext_t *nVCLContext_p,
                                     H264EncodeHardFrameParams_t *frameParams_p);
uint32_t H264HwEncodeCreateFillerNAL(H264EncodeNVCLContext_t *nVCLContext_p, H264EncodeHardParamOut_t *outParams_p,
                                     uint8_t *streamBuf_p);
value_scale_deconstruction_t H264HwEncodeDeconstructBitrate(uint32_t bitrate);
value_scale_deconstruction_t H264HwEncodeDeconstructCpbSize(uint32_t cpb_size);
uint32_t H264HwEncodeReconstructBitrate(uint32_t value, int32_t scale);
uint32_t H264HwEncodeReconstructCpbSize(uint32_t value, int32_t scale);

// Flexible slice header
#define FLEXIBLE_SLICE_HEADER_MAX_BYTESIZE 80
typedef struct flexible_slice_header_tag {
	uint32_t frame_num;
	uint32_t header_bitsize;
	uint32_t offset0_bitsize; //FirstMbInSlice offset
	uint32_t offset1_bitsize; //slice_qp_delta offset
	uint8_t  *buffer;
} FLEXIBLE_SLICE_HEADER;

uint32_t H264HwEncodeFillFlexibleSliceHeader(FLEXIBLE_SLICE_HEADER *flexible_slice_header,
                                             H264EncodeHardFrameParams_t *frameParams_p, H264EncodeNVCLContext_t *nVCLContext_p);

#ifdef __cplusplus
}
#endif


#endif // H_H264_ENCODE_NVCL
