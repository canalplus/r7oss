/************************************************************************
Copyright (C) 2010, 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the hdmirx Library.

hdmirx is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

hdmirx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with hdmirx; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The hdmirx Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_hdmirx.h
Author :           Sonia Varada

HDMI Receiver KPI definitions

Date        Modification                                    Name
----        ------------                                    --------
15-Jun-12   Created                                         Sonia Varada

************************************************************************/
#ifndef _STM_HDMIRX_H
#define _STM_HDMIRX_H

#include "stm_event.h"

typedef enum stm_hdmirx_signal_type_e {
	STM_HDMIRX_SIGNAL_TYPE_DVI,
	STM_HDMIRX_SIGNAL_TYPE_HDMI
} stm_hdmirx_signal_type_t;

typedef enum stm_hdmirx_signal_scan_type_e {
	STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED,
	STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE
} stm_hdmirx_signal_scan_type_t;

typedef enum stm_hdmirx_signal_polarity_e {
	STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE,
	STM_HDMIRX_SIGNAL_POLARITY_POSITIVE
} stm_hdmirx_signal_polarity_t;

typedef enum stm_hdmirx_color_depth_e {
	STM_HDMIRX_COLOR_DEPTH_24BPP,
	STM_HDMIRX_COLOR_DEPTH_30BPP,
	STM_HDMIRX_COLOR_DEPTH_36BPP,
	STM_HDMIRX_COLOR_DEPTH_48BPP
} stm_hdmirx_color_depth_t;

typedef enum stm_hdmirx_color_format_e {
	STM_HDMIRX_COLOR_FORMAT_RGB,
	STM_HDMIRX_COLOR_FORMAT_YUV_422,
	STM_HDMIRX_COLOR_FORMAT_YUV_444
} stm_hdmirx_color_format_t;

typedef enum stm_hdmirx_colorimetry_std_e {
	STM_HDMIRX_COLORIMETRY_STD_DEFAULT,
	STM_HDMIRX_COLORIMETRY_STD_BT_601,
	STM_HDMIRX_COLORIMETRY_STD_BT_709,
	STM_HDMIRX_COLORIMETRY_STD_XVYCC_601,
	STM_HDMIRX_COLORIMETRY_STD_XVYCC_709,
	STM_HDMIRX_COLORIMETRY_STD_sYCC_601,
	STM_HDMIRX_COLORIMETRY_STD_AdobeYCC_601,
	STM_HDMIRX_COLORIMETRY_STD_AdobeRGB
} stm_hdmirx_colorimetry_std_t;

typedef enum stm_hdmirx_scan_info_e {
	STM_HDMIRX_SCAN_INFO_NO_DATA,
	STM_HDMIRX_SCAN_INFO_FOR_OVERSCANNED_DISPLAY,
	STM_HDMIRX_SCAN_INFO_FOR_UNDER_SCANNED_DISPLAY,
	STM_HDMIRX_SCAN_INFO_RESERVED
} stm_hdmirx_scan_info_t;

typedef enum stm_hdmirx_scaling_info_e {
	STM_HDMIRX_SCALING_INFO_NO_SCALING,
	STM_HDMIRX_SCALING_INFO_H_SCALED,
	STM_HDMIRX_SCALING_INFO_V_SCALED,
	STM_HDMIRX_SCALING_INFO_HV_SCALED
} stm_hdmirx_scaling_info_t;

typedef enum stm_hdmirx_rgb_quant_range_e {
	STM_HDMIRX_RGB_QUANT_RANGE_DEFAULT,
	STM_HDMIRX_RGB_QUANT_RANGE_LIMITED,
	STM_HDMIRX_RGB_QUANT_RANGE_FULL,
	STM_HDMIRX_RGB_QUANT_RANGE_RESERVED
} stm_hdmirx_rgb_quant_range_t;

typedef enum stm_hdmirx_yc_quant_range_e {
	STM_HDMIRX_YC_QUANT_RANGE_LIMITED,
	STM_HDMIRX_YC_QUANT_RANGE_FULL,
	STM_HDMIRX_YC_QUANT_RANGE_RESERVED
} stm_hdmirx_yc_quant_range_t;

typedef enum stm_hdmirx_pixel_repeat_factor_e {
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_1,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_2,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_3,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_4,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_5,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_6,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_7,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_8,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_9,
	STM_HDMIRX_PIXEL_REPEAT_FACTOR_10
} stm_hdmirx_pixel_repeat_factor_t;

typedef enum stm_hdmirx_aspect_ratio_e {
	STM_HDMIRX_ASPECT_RATIO_NONE,
	STM_HDMIRX_ASPECT_RATIO_4_3,
	STM_HDMIRX_ASPECT_RATIO_16_9,
	STM_HDMIRX_ASPECT_RATIO_16_9_TOP,
	STM_HDMIRX_ASPECT_RATIO_14_9_TOP,
	STM_HDMIRX_ASPECT_RATIO_4_3_CENTER,
	STM_HDMIRX_ASPECT_RATIO_16_9_CENTER,
	STM_HDMIRX_ASPECT_RATIO_14_9_CENTER,
	STM_HDMIRX_ASPECT_RATIO_4_3_CENTER_14_9_SP,
	STM_HDMIRX_ASPECT_RATIO_16_9_CENTER_14_9_SP,
	STM_HDMIRX_ASPECT_RATIO_16_9_CENTER__4_3_SP
} stm_hdmirx_aspect_ratio_t;

typedef enum stm_hdmirx_iec_audio_std_e {
	STM_HDMIRX_IEC_AUDIO_STD_60958_3,
	STM_HDMIRX_IEC_AUDIO_STD_60958_4,
	STM_HDMIRX_IEC_AUDIO_STD_61937,
	STM_HDMIRX_IEC_AUDIO_STD_SMPTE_337M
} stm_hdmirx_iec_audio_std_t;

typedef enum stm_hdmirx_audio_stream_type_e {
	STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN = 0x00,
	STM_HDMIRX_AUDIO_STREAM_TYPE_IEC = 0x01,
	STM_HDMIRX_AUDIO_STREAM_TYPE_DSD = 0x02,
	STM_HDMIRX_AUDIO_STREAM_TYPE_DST = 0x04,
	STM_HDMIRX_AUDIO_STREAM_TYPE_HBR = 0x08
} stm_hdmirx_audio_stream_type_t;

typedef enum stm_hdmirx_audio_coding_type_e {
	STM_HDMIRX_AUDIO_CODING_TYPE_NONE,
	STM_HDMIRX_AUDIO_CODING_TYPE_PCM,
	STM_HDMIRX_AUDIO_CODING_TYPE_AC3,
	STM_HDMIRX_AUDIO_CODING_TYPE_MPEG1,
	STM_HDMIRX_AUDIO_CODING_TYPE_MP3,
	STM_HDMIRX_AUDIO_CODING_TYPE_MPEG2,
	STM_HDMIRX_AUDIO_CODING_TYPE_AAC,
	STM_HDMIRX_AUDIO_CODING_TYPE_DTS,
	STM_HDMIRX_AUDIO_CODING_TYPE_ATRAC,
	STM_HDMIRX_AUDIO_CODING_TYPE_DSD,
	STM_HDMIRX_AUDIO_CODING_TYPE_DDPLUS,
	STM_HDMIRX_AUDIO_CODING_TYPE_DTS_HD,
	STM_HDMIRX_AUDIO_CODING_TYPE_MAT,
	STM_HDMIRX_AUDIO_CODING_TYPE_DST,
	STM_HDMIRX_AUDIO_CODING_TYPE_WMA_PRO
} stm_hdmirx_audio_coding_type_t;

typedef enum stm_hdmirx_audio_sample_size_e {
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_16_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_17_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_18_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_19_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_20_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_21_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_22_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_23_BITS,
	STM_HDMIRX_AUDIO_SAMPLE_SIZE_24_BITS
} stm_hdmirx_audio_sample_size_t;

typedef enum stm_hdmirx_audio_clock_accuracy_e {
	STM_HDMIRX_AUDIO_CLOCK_ACCURACY_LEVEL_1,
	STM_HDMIRX_AUDIO_CLOCK_ACCURACY_LEVEL_2,
	STM_HDMIRX_AUDIO_CLOCK_ACCURACY_LEVEL_3,
	STM_HDMIRX_AUDIO_CLOCK_ACCURACY_UNMATCHED
} stm_hdmirx_audio_audio_clock_accuracy_t;

typedef enum stm_hdmirx_audio_content_protection_e {
	STM_HDMIRX_AUDIO_CP_GENERIC,
	STM_HDMIRX_AUDIO_CP_IEC60958,
	STM_HDMIRX_AUDIO_CP_DVD,
	STM_HDMIRX_AUDIO_CP_SACD
} stm_hdmirx_audio_content_protection_t;

typedef enum stm_hdmirx_information_e {
	STM_HDMIRX_INFO_VSINFO = (1 << 0),
	STM_HDMIRX_INFO_SPDINFO = (1 << 1),
	STM_HDMIRX_INFO_MPEGSOURCEINFO = (1 << 2),
	STM_HDMIRX_INFO_ISRCINFO = (1 << 3),
	STM_HDMIRX_INFO_ACPINFO = (1 << 4),
	STM_HDMIRX_INFO_GBDINFO = (1 << 5)
} stm_hdmirx_information_t;

typedef enum stm_hdmirx_gbd_profile_e {
	STM_HDMIRX_GBD_PROFILE_0,
	STM_HDMIRX_GBD_PROFILE_1,
	STM_HDMIRX_GBD_PROFILE_2,
	STM_HDMIRX_GBD_PROFILE_3
} stm_hdmirx_gbd_profile_t;

typedef enum stm_hdmirx_gbd_format_e {
	STM_HDMIRX_GBD_FORMAT_VERTEX,
	STM_HDMIRX_GBD_FORMAT_RGBRANGE
} stm_hdmirx_gbd_format_t;

typedef enum stm_hdmirx_mpeg_picture_type_e {
	STM_HDMIRX_MPEG_PICTURE_TYPE_I,
	STM_HDMIRX_MPEG_PICTURE_TYPE_P,
	STM_HDMIRX_MPEG_PICTURE_TYPE_B
} stm_hdmirx_mpeg_picture_type_t;

typedef enum stm_hdmirx_content_type_e {
	STM_HDMIRX_CONTENT_TYPE_UNKNOWN,
	STM_HDMIRX_CONTENT_TYPE_GRAPHICS,
	STM_HDMIRX_CONTENT_TYPE_PHOTO,
	STM_HDMIRX_CONTENT_TYPE_CINEMA,
	STM_HDMIRX_CONTENT_TYPE_GAME
} stm_hdmirx_content_type_t;

typedef enum stm_hdmirx_audio_lfe_pb_level_e {
	STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_UNKNOWN,
	STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_0Db,
	STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_10Db
} stm_hdmirx_audio_lfe_pb_level_t;

typedef enum stm_hdmirx_3d_format_e {
	STM_HDMIRX_3D_FORMAT_UNKNOWN,
	STM_HDMIRX_3D_FORMAT_FRAME_PACK,
	STM_HDMIRX_3D_FORMAT_FIELD_ALT,
	STM_HDMIRX_3D_FORMAT_LINE_ALT,
	STM_HDMIRX_3D_FORMAT_SBYS_FULL,
	STM_HDMIRX_3D_FORMAT_SBYS_HALF,
	STM_HDMIRX_3D_FORMAT_L_D,
	STM_HDMIRX_3D_FORMAT_L_D_G_GMINUSD,
	STM_HDMIRX_3D_FORMAT_TOP_AND_BOTTOM
} stm_hdmirx_3d_format_t;

typedef enum stm_hdmirx_3d_metadata_type_e {
	STM_HDMIRX_3D_METADATA_NONE,
	STM_HDMIRX_3D_METADATA_ISO23002_3
} stm_hdmirx_3d_metadata_type_t;

typedef enum stm_hdmirx_3d_sbs_half_sampling_e {
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLOR,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLER,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELOR,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELER,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLOR,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLER,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELOR,
	STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELER
} stm_hdmirx_3d_sbs_half_sampling_t;

typedef enum stm_hdmirx_route_opmode_e {
	STM_HDMIRX_ROUTE_OPMODE_AUTO,
	STM_HDMIRX_ROUTE_OPMODE_HDMI,
	STM_HDMIRX_ROUTE_OPMODE_DVI
} stm_hdmirx_route_opmode_t;

typedef enum stm_hdmirx_route_ctrl_e {
	STM_HDMIRX_ROUTE_CTRL_OPMODE,
	STM_HDMIRX_ROUTE_CTRL_AUDIO_OUT_ENABLE,
	STM_HDMIRX_ROUTE_CTRL_REPEATER_MODE_ENABLE,
	STM_HDMIRX_ROUTE_CTRL_REPEATER_STATUS_READY
} stm_hdmirx_route_ctrl_t;

typedef struct stm_hdmirx_signal_timing_s {
	stm_hdmirx_signal_scan_type_t scan_type;
	uint32_t pixel_clock_hz;
	stm_hdmirx_signal_polarity_t hsync_polarity;
	uint16_t hactive_start;
	uint16_t hactive;
	uint16_t htotal;
	stm_hdmirx_signal_polarity_t vsync_polarity;
	uint16_t vactive_start;
	uint16_t vActive;
	uint16_t vtotal;
} stm_hdmirx_signal_timing_t;

typedef struct stm_hdmirx_bar_info_s {
	bool hbar_valid;
	uint16_t end_of_left_bar;
	uint16_t start_of_right_bar;
	bool vbar_valid;
	uint16_t end_of_top_bar;
	uint16_t start_of_bottom_bar;
} stm_hdmirx_bar_info_t;

typedef struct stm_hdmirx_3d_video_metadata_s {
	stm_hdmirx_3d_metadata_type_t type;
	uint8_t length;
	uint8_t metadata[32];
} stm_hdmirx_3d_video_metadata_t;

typedef struct stm_hdmirx_3d_video_sbs_half_extdata_s {
	stm_hdmirx_3d_sbs_half_sampling_t sampling_mode;
} stm_hdmirx_3d_video_sbs_half_extdata_t;

typedef union stm_hdmirx_3d_video_extdata_s {
	stm_hdmirx_3d_video_sbs_half_extdata_t sbs_half_ext_data;
} stm_hdmirx_3d_video_extdata_t;

typedef struct stm_hdmirx_3d_video_property_s {
	stm_hdmirx_3d_format_t format;
	stm_hdmirx_3d_video_extdata_t extdata;
	stm_hdmirx_3d_video_metadata_t metatdata;
} stm_hdmirx_3d_video_property_t;

typedef struct stm_hdmirx_video_property_s {
	stm_hdmirx_color_depth_t color_depth;
	stm_hdmirx_color_format_t color_format;
	stm_hdmirx_colorimetry_std_t colorimetry;
	stm_hdmirx_bar_info_t bar_info;
	stm_hdmirx_scan_info_t scan_info;
	stm_hdmirx_scaling_info_t scaling_info;
	stm_hdmirx_aspect_ratio_t picture_aspect;
	stm_hdmirx_aspect_ratio_t active_format_aspect;
	stm_hdmirx_rgb_quant_range_t rgb_quant_range;
	stm_hdmirx_pixel_repeat_factor_t pixel_repeat_factor;
	stm_hdmirx_yc_quant_range_t yc_quant_range;
	stm_hdmirx_content_type_t content_type;
	bool it_content;
	bool hdmi_video_format;
	uint32_t video_timing_code;
	bool video_3d;
	stm_hdmirx_3d_video_property_t video_property_3d;
} stm_hdmirx_video_property_t;

typedef struct stm_hdmirx_audio_property_s {
	stm_hdmirx_audio_stream_type_t stream_type;
	stm_hdmirx_audio_coding_type_t coding_type;
	uint8_t channel_count;
	uint8_t channel_allocation;
	uint32_t sampling_frequency;
	stm_hdmirx_audio_sample_size_t sample_size;
	bool down_mix_inhibit;
	uint8_t level_shift;
	stm_hdmirx_audio_lfe_pb_level_t lfe_pb_level;
} stm_hdmirx_audio_property_t;

typedef struct stm_hdmirx_signal_property_s {
	stm_hdmirx_signal_type_t signal_type;
	stm_hdmirx_signal_timing_t timing;
	stm_hdmirx_video_property_t video_property;
	stm_hdmirx_audio_property_t audio_property;
} stm_hdmirx_signal_property_t;

typedef struct stm_hdmirx_hdcp_downstream_status_s {
	bool max_cascade_exceeded;
	uint8_t depth;
	bool max_devs_exceeded;
	uint8_t device_count;
} stm_hdmirx_hdcp_downstream_status_t;

typedef unsigned char stm_hdmirx_audio_channel_status_t[24];

typedef struct stm_hdmirx_vs_info_s {
	uint32_t ieee_reg_id;
	uint8_t length;
	uint8_t payload[30];
} stm_hdmirx_vs_info_t;

typedef struct stm_hdmirx_spd_info_s {
	uint8_t vendor_name[8];
	uint8_t description[16];
	uint8_t device_info;
} stm_hdmirx_spd_info_t;

typedef struct stm_hdmirx_mpeg_source_info_s {
	uint32_t bitrate;
	bool field_repeat;
	stm_hdmirx_mpeg_picture_type_t picture_type;
} stm_hdmirx_mpeg_source_info_t;

typedef struct stm_hdmirx_isrc_info_s {
	uint8_t status;
	uint8_t length;
	uint8_t info[32];
} stm_hdmirx_isrc_info_t;

typedef struct stm_hdmirx_acp_info_s {
	stm_hdmirx_audio_content_protection_t acp_type;
	uint8_t acp_data[28];
} stm_hdmirx_acp_info_t;

typedef struct stm_hdmirx_gbd_vertex_format_p0_s {
	stm_hdmirx_colorimetry_std_t colorimetry;
	uint8_t number_of_vertices;
	uint16_t vertices[4];
} stm_hdmirx_gbd_vertex_format_p0_t;

typedef struct stm_hdmirx_gbd_range_format_s {
	uint8_t precision;
	stm_hdmirx_colorimetry_std_t colorimetry;
	uint16_t min_red;
	uint16_t max_red;
	uint16_t min_green;
	uint16_t max_green;
	uint16_t min_blue;
	uint16_t max_blue;
} stm_hdmirx_gbd_range_format_t;

/*typedef struct stm_hdmirx_gbd_info_s
{
	stm_hdmirx_gbd_profile_t				profile;
	stm_hdmirx_gbd_format_t				format;
	union
	{
	 stm_hdmirx_gbd_vertex_format_p0_t		vertex_form_p0;
	 stm_hdmirx_gbd_range_format_t		range_form;
	} desc;

} stm_hdmirx_gbd_info_t;*/
typedef struct {
	uint8_t GBD[28];
} stm_hdmirx_gbd_info_t;

typedef struct {
	void *handle;
} stm_hdmirx_device_s;

typedef struct {
	void *handle;
} stm_hdmirx_port_s;

typedef struct {
	void *handle;
} stm_hdmirx_route_s;

typedef struct stm_hdmirx_device_s *stm_hdmirx_device_h;

typedef struct stm_hdmirx_device_capability_s {
	uint32_t number_of_ports;
	uint32_t number_of_routes;
} stm_hdmirx_device_capability_t;

typedef struct stm_hdmirx_port_s *stm_hdmirx_port_h;

typedef enum stm_hdmirx_port_source_plug_status_e {
	STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN,
	STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT
} stm_hdmirx_port_source_plug_status_t;

typedef enum stm_hdmirx_port_hpd_state_e {
	STM_HDMIRX_PORT_HPD_STATE_LOW,
	STM_HDMIRX_PORT_HPD_STATE_HIGH
} stm_hdmirx_port_hpd_state_t;

typedef struct stm_hdmirx_port_capability_s {
	bool hpd_support;
	bool internal_edid;
	bool ddc2bi;
} stm_hdmirx_port_capability_t;

typedef uint8_t stm_hdmirx_port_edid_block_t[128];

typedef struct stm_hdmirx_port_ddc2bi_msg_s {
	uint8_t dest_addr;
	uint8_t source_addr;
	uint8_t length;
	uint8_t data[127];
	uint8_t checksum;
} stm_hdmirx_port_ddc2bi_msg_t;

#define STM_HDMIRX_PORT_SOURCE_PLUG_EVT (1 << 0)
#define STM_HDMIRX_PORT_DDC2BI_MSG_RCVD_EVT (1 << 1)

typedef enum stm_hdmirx_route_signal_status_e {
	STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT,
	STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT
} stm_hdmirx_route_signal_status_t;

typedef enum stm_hdmirx_route_hdcp_status_t {
	STM_HDMIRX_ROUTE_HDCP_STATUS_NOT_AUTHENTICATED,
	STM_HDMIRX_ROUTE_HDCP_STATUS_AUTHENTICATION_DETECTED,
	STM_HDMIRX_ROUTE_HDCP_STATUS_NOISE_DETECTED
} stm_hdmirx_route_hdcp_status_t;

typedef struct stm_hdmirx_route_s *stm_hdmirx_route_h;

typedef struct stm_hdmirx_route_capability_s {
	bool video_3d_support;
	bool repeater_fn_support;
} stm_hdmirx_route_capability_t;

#define STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT (1 << 0)
#define STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT (1 << 1)
#define STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT (1 << 2)
#define STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT (1 << 3)
#define STM_HDMIRX_ROUTE_SPD_INFO_UPDATE_EVT (1 << 4)
#define STM_HDMIRX_ROUTE_MPEGSOURCE_INFO_UPDATE_EVT (1 << 5)
#define STM_HDMIRX_ROUTE_ISRC_INFO_UPDATE_EVT (1 << 6)
#define STM_HDMIRX_ROUTE_ACP_INFO_UPDATE_EVT (1 << 7)
#define STM_HDMIRX_ROUTE_GBD_INFO_UPDATE_EVT (1 << 8)
#define STM_HDMIRX_ROUTE_HDCP_AUTHENTICATION_DETECTED_EVT (1 << 9)

int stm_hdmirx_device_open(stm_hdmirx_device_h *device, uint32_t id);
int stm_hdmirx_device_close(stm_hdmirx_device_h device);
int stm_hdmirx_device_get_capability(stm_hdmirx_device_h device,
				    stm_hdmirx_device_capability_t *capability);
int stm_hdmirx_port_open(stm_hdmirx_device_h device, uint32_t id,
			 stm_hdmirx_port_h *port);

int stm_hdmirx_port_close(stm_hdmirx_port_h port);

int stm_hdmirx_port_get_capability(stm_hdmirx_port_h port,
				   stm_hdmirx_port_capability_t *capability);

int stm_hdmirx_port_set_hpd_signal(stm_hdmirx_port_h port,
				   stm_hdmirx_port_hpd_state_t state);
int stm_hdmirx_port_get_hpd_signal(stm_hdmirx_port_h port,
				   stm_hdmirx_port_hpd_state_t *state);
int stm_hdmirx_port_get_source_plug_status(stm_hdmirx_port_h port,
				  stm_hdmirx_port_source_plug_status_t *status);
int stm_hdmirx_port_init_edid(stm_hdmirx_port_h port,
			      stm_hdmirx_port_edid_block_t *edid_block,
			      uint8_t number_of_blocks);

int stm_hdmirx_port_read_edid_block(stm_hdmirx_port_h port,
				    uint8_t block_number,
				    stm_hdmirx_port_edid_block_t *edid_block);

int stm_hdmirx_port_update_edid_block(stm_hdmirx_port_h port,
				      uint8_t block_number,
				      stm_hdmirx_port_edid_block_t *edid_block);

int stm_hdmirx_port_set_ddc2bi_msg(stm_hdmirx_port_h port,
				   stm_hdmirx_port_ddc2bi_msg_t *msg);
int stm_hdmirx_port_get_ddc2bi_msg(stm_hdmirx_port_h port,
				   stm_hdmirx_port_ddc2bi_msg_t *msg);

int stm_hdmirx_route_open(stm_hdmirx_device_h device,
			  uint32_t id, stm_hdmirx_route_h *route);

int stm_hdmirx_route_close(stm_hdmirx_route_h route);

int stm_hdmirx_route_get_capability(stm_hdmirx_route_h route,
				    stm_hdmirx_route_capability_t *capability);
int stm_hdmirx_port_connect(stm_hdmirx_port_h port, stm_hdmirx_route_h route);

int stm_hdmirx_port_disconnect(stm_hdmirx_port_h port);
int stm_hdmirx_route_get_signal_status(stm_hdmirx_route_h route,
				      stm_hdmirx_route_signal_status_t *status);

int stm_hdmirx_route_get_hdcp_status(stm_hdmirx_route_h route,
				     stm_hdmirx_route_hdcp_status_t *status);

int stm_hdmirx_route_get_signal_property(stm_hdmirx_route_h route,
					stm_hdmirx_signal_property_t *property);

int stm_hdmirx_route_get_video_property(stm_hdmirx_route_h route,
					stm_hdmirx_video_property_t *property);

int stm_hdmirx_route_get_audio_property(stm_hdmirx_route_h route,
					stm_hdmirx_audio_property_t *property);

int stm_hdmirx_route_get_audio_channel_status(stm_hdmirx_route_h route,
				    stm_hdmirx_audio_channel_status_t status[]);

int stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_route_h route,
					  bool enable);

int stm_hdmirx_route_get_audio_out_enable(stm_hdmirx_route_h route,
					  bool *status);
int stm_hdmirx_route_get_info(stm_hdmirx_route_h route,
			      stm_hdmirx_information_t info_type, void *info);
int stm_hdmirx_route_video_connect(stm_hdmirx_route_h route,
				   stm_object_h video_sink);
int stm_hdmirx_route_video_disconnect(stm_hdmirx_route_h route);
int stm_hdmirx_route_audio_connect(stm_hdmirx_route_h route,
				   stm_object_h audio_sink);
int stm_hdmirx_route_audio_disconnect(stm_hdmirx_route_h route);

int stm_hdmirx_route_set_ctrl (stm_hdmirx_route_h route,
				   stm_hdmirx_route_ctrl_t ctrl,
				   uint32_t value);
int stm_hdmirx_route_get_ctrl (stm_hdmirx_route_h route,
				   stm_hdmirx_route_ctrl_t ctrl,
				   uint32_t *value);
int stm_hdmirx_route_set_hdcp_downstream_status (stm_hdmirx_route_h route,
				   stm_hdmirx_hdcp_downstream_status_t *status);
int stm_hdmirx_route_set_hdcp_downstream_ksv_list (stm_hdmirx_route_h route,
				   uint8_t *ksv_list,
				   uint32_t size);
#endif /*_STM_HDMIRX_H*/
