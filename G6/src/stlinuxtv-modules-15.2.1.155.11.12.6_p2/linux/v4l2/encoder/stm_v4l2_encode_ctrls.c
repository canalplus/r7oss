/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of stlinuxtv v4l2 encoder ctrl framework.
************************************************************************/
#include "stmedia.h"
#include "linux/stm/stmedia_export.h"


#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_v4l2_common.h"
#include "stm_v4l2_encode.h"


static const int stm_v4l2_videnc_h264_level_table[][2] = {

	{ V4L2_MPEG_VIDEO_H264_LEVEL_1_0, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_1B,  STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_1_1, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_1_2, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_1_3, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_2_0, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_2_1, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_2_2, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_3_0, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_3_1, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_3_2, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_4_0, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_4_1, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_4_2, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_5_0, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_0 },
	{ V4L2_MPEG_VIDEO_H264_LEVEL_5_1, STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_1 }
};

static const int stm_v4l2_videnc_h264_profile_table[][2] = {

	{ V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE, STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_MAIN,     STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN     },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED, STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_EXTENDED },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,     STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH     },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10,  STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_10  },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422, STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_422 },
	{ V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE, STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444 }
};

static const int stm_v4l2_audenc_sampling_freq_table[][2] = {
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_4000,     4000 }, /* 0 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_5000,     5000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_6000,     6000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_8000,     8000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_11025,   11025 }, /* 4 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_12000,   12000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_16000,   16000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_22050,   22050 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_24000,   24000 }, /* 8 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_32000,   32000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_44100,   44100 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_48000,   48000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_64000,   64000 }, /* 12 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_88200,   88200 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_96000,   96000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_128000, 128000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_176400, 176400 }, /* 16 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_192000, 192000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_256000, 256000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_352800, 352800 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_384000, 384000 }, /* 20 */
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_512000, 512000 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_705600, 705600 },
	{ V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_768000, 768000 }
};

static const int stm_v4l2_audenc_ac3_bitrate_table[][2] = {
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_32K,   32000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_40K,   40000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_48K,   48000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_56K,   56000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_64K,   64000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_80K,   80000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_96K,   96000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_112K, 112000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_128K, 128000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_160K, 160000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_192K, 192000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_224K, 224000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_256K, 256000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_320K, 320000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_384K, 384000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_448K, 448000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_512K, 512000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_576K, 576000 },
	{ V4L2_MPEG_AUDIO_AC3_BITRATE_640K, 640000 }
};

static const int stm_v4l2_audenc_mp3_bitrate_table[][2] = {
	{ V4L2_MPEG_AUDIO_L3_BITRATE_32K,   32000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_40K,   40000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_48K,   48000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_56K,   56000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_64K,   64000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_80K,   80000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_96K,   96000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_112K, 112000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_128K, 128000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_160K, 160000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_192K, 192000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_224K, 224000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_256K, 256000 },
	{ V4L2_MPEG_AUDIO_L3_BITRATE_320K, 320000 }
};

static int stm_v4l2_encoder_aud_s_ctrl(struct v4l2_ctrl *ctrl_p)
{
	audio_encode_params_t *encode_param_p;
	struct stm_v4l2_encoder_device *dev_p = container_of(ctrl_p->handler, struct stm_v4l2_encoder_device, ctrl_handler);

	unsigned int ctrl_id = ctrl_p->id;
	int ctrl_value = ctrl_p->val;
	unsigned int table_length;
	int ret=0;

	mutex_lock(&dev_p->lock);

	encode_param_p = &dev_p->encode_params.audio;

	switch (ctrl_id) {

	case V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ:
	{
		int sampling_freq;

		table_length = sizeof(stm_v4l2_audenc_sampling_freq_table);
		sampling_freq = stm_v4l2_convert_v4l2_SE_define( ctrl_value,
				stm_v4l2_audenc_sampling_freq_table,
				table_length);
		if(sampling_freq == -1) {
			ret = -EINVAL;
			break;
		}

		encode_param_p->sample_rate = sampling_freq;
		break;
	}
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_MODE:
		if(ctrl_value == V4L2_MPEG_AUDIO_BITRATE_MODE_CBR)
			encode_param_p->is_vbr = 0;
		else if(ctrl_value == V4L2_MPEG_AUDIO_BITRATE_MODE_VBR)
			encode_param_p->is_vbr = 1;
		else {
			ret = -EINVAL;
			break;
		}
		break;
	case V4L2_CID_MPEG_STM_AUDIO_VBR_QUALITY_FACTOR:
		if(ctrl_value < 0 || ctrl_value > 100) {
			ret = -EINVAL;
			break;
		}

		encode_param_p->vbr_quality_factor = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_CAP:
		encode_param_p->bitrate_cap = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL:
	{
		stm_se_audio_bitrate_control_t value;

		if(ctrl_value) {
			memset(&value, 0, sizeof(stm_se_audio_bitrate_control_t));
			value.is_vbr  = encode_param_p->is_vbr;
			value.bitrate = encode_param_p->bitrate;
			value.vbr_quality_factor = encode_param_p->vbr_quality_factor;
			value.bitrate_cap = encode_param_p->bitrate_cap;

			ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL,
				(void *)&value);
			if(ret) {
			   printk(KERN_ERR "Error : V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL\n");
			   break;
			}
		}

		break;
	}

	case V4L2_CID_MPEG_STM_AUDIO_PROGRAM_LEVEL:
	{
		int prog_level;

		/* TODO : need to check the value */
		prog_level = ctrl_value;

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
					STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL,
					prog_level);
		break;
	}

	case V4L2_CID_MPEG_STM_ENCODE_NRT_MODE:
		ret = stm_se_encode_set_control(dev_p->encode_obj,
					STM_SE_CTRL_ENCODE_NRT_MODE,
					(ctrl_value ? STM_SE_CTRL_VALUE_APPLY :
					STM_SE_CTRL_VALUE_DISAPPLY));
		break;

	case V4L2_CID_MPEG_STM_AUDIO_SERIAL_CONTROL:
	{
		int serial_control;

		if(ctrl_value == V4L2_MPEG_AUDIO_NO_COPYRIGHT)
		   serial_control = STM_SE_AUDIO_NO_COPYRIGHT;
		else if(ctrl_value == V4L2_MPEG_AUDIO_ONE_MORE_COPY_AUTHORISED)
		   serial_control = STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED;
		else if(ctrl_value == V4L2_MPEG_AUDIO_NO_FUTHER_COPY_AUTHORISED)
		   serial_control = STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED;
		else {
			ret = -EINVAL;
			break;
		}

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
					STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS,
												serial_control);
		break;
	}

	case V4L2_CID_MPEG_STM_AUDIO_CHANNEL_COUNT:
		if(ctrl_value <= 0 || ctrl_value > V4L2_STM_AUDENC_MAX_CHANNELS) {
			ret = -EINVAL;
			break;
		}

		encode_param_p->channel_count = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_CHANNEL_MAP:
	{
		int ch_index;
		int channel_id;

		ch_index   = (ctrl_value >> 8) & 0xff;
		channel_id = ctrl_value & 0xff;

		encode_param_p->channel[ch_index] = channel_id;
		break;
	}

	case V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT:
	{
		stm_se_audio_core_format_t value;
		int i;

		if(ctrl_value) {
			memset(&value, 0, sizeof(stm_se_audio_core_format_t));
			value.sample_rate = encode_param_p->sample_rate;
			value.channel_placement.channel_count = encode_param_p->channel_count;
			for(i=0; i<encode_param_p->channel_count; i++) {
				value.channel_placement.chan[i] = encode_param_p->channel[i];
			}

			ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
						STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT,
						(void *)&value);
			if(ret) {
			   printk(KERN_ERR "Error : V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT\n");
			   break;
			}
		}

		break;
	}

	case V4L2_CID_MPEG_AUDIO_AAC_BITRATE:
		encode_param_p->bitrate = ctrl_value;
		break;

	case V4L2_CID_MPEG_AUDIO_AC3_BITRATE:
	{
		int bitrate;

		table_length = sizeof(stm_v4l2_audenc_ac3_bitrate_table);
		bitrate = stm_v4l2_convert_v4l2_SE_define(ctrl_value,
							stm_v4l2_audenc_ac3_bitrate_table,
							table_length);
		if(bitrate == -1) {
			ret = -EINVAL;
			break;
		}

		encode_param_p->bitrate = bitrate;

		break;
	}
	case V4L2_CID_MPEG_AUDIO_L3_BITRATE:
	{
		int bitrate;

		table_length = sizeof(stm_v4l2_audenc_mp3_bitrate_table);
		bitrate = stm_v4l2_convert_v4l2_SE_define(ctrl_value,
							stm_v4l2_audenc_mp3_bitrate_table,
							table_length);
		if(bitrate == -1) {
			ret = -EINVAL;
			break;
		}

		encode_param_p->bitrate = bitrate;
		break;
	}
	case V4L2_CID_MPEG_AUDIO_CRC:
	{
		int crc_enable;
		if(ctrl_value == V4L2_MPEG_AUDIO_CRC_NONE)
			crc_enable = STM_SE_CTRL_VALUE_DISAPPLY;
		else if(ctrl_value == V4L2_MPEG_AUDIO_CRC_CRC16)
			crc_enable = STM_SE_CTRL_VALUE_APPLY;
		else{
			ret = -EINVAL;
			break;
		}

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
							STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE,
							crc_enable);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dev_p->lock);
	return ret;
}

static int stm_v4l2_encoder_aud_g_ctrl(struct v4l2_ctrl *ctrl_p)
{
	audio_encode_params_t *encode_param_p;
	struct stm_v4l2_encoder_device *dev_p = container_of(ctrl_p->handler, struct stm_v4l2_encoder_device, ctrl_handler);

	unsigned int ctrl_id = ctrl_p->id;
	int ctrl_value = ctrl_p->val;
	unsigned int table_length;
	int ret=0;

	mutex_lock(&dev_p->lock);

	encode_param_p = &dev_p->encode_params.audio;

	switch (ctrl_id) {

	case V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ:
	{
		int sampling_freq;
		sampling_freq = encode_param_p->sample_rate;

		table_length = sizeof(stm_v4l2_audenc_sampling_freq_table);
		ctrl_value = stm_v4l2_convert_SE_v4l2_define(sampling_freq,
					stm_v4l2_audenc_sampling_freq_table,
					table_length);
		if(ctrl_value == -1) {
			ret = -EINVAL;
			break;
		}
		break;
	}
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_MODE:
		if(encode_param_p->is_vbr == 0)
			ctrl_value =  V4L2_MPEG_AUDIO_BITRATE_MODE_CBR;
		else if(encode_param_p->is_vbr == 1)
			ctrl_value = V4L2_MPEG_AUDIO_BITRATE_MODE_VBR;
		else {
			ret = -EINVAL;
			break;
		}
		break;
	case V4L2_CID_MPEG_STM_AUDIO_VBR_QUALITY_FACTOR:
		ctrl_value = encode_param_p->vbr_quality_factor;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_CAP:
		ctrl_value = encode_param_p->bitrate_cap;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL:
	/*FIXME: Currently we need to be able to set this control every time. Hence g_ctrl
	will return 0 every time so as to enable this. Better logic required here*/
		ctrl_value = 0;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_PROGRAM_LEVEL:
		/* TODO : need to check the value */
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
					STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL,
					&ctrl_value);
		break;

	case V4L2_CID_MPEG_STM_ENCODE_NRT_MODE:
		ret = stm_se_encode_get_control(dev_p->encode_obj,
					STM_SE_CTRL_ENCODE_NRT_MODE,
					&ctrl_value);
		break;

	case V4L2_CID_MPEG_STM_AUDIO_SERIAL_CONTROL:
	{
		int serial_control;

		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
					STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS,
					&serial_control);

		if(ret)
			break;

		if(serial_control == STM_SE_AUDIO_NO_COPYRIGHT)
		   ctrl_value = V4L2_MPEG_AUDIO_NO_COPYRIGHT;
		else if(serial_control == STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED)
		   ctrl_value = V4L2_MPEG_AUDIO_ONE_MORE_COPY_AUTHORISED;
		else if(serial_control == STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED)
		   ctrl_value = V4L2_MPEG_AUDIO_NO_FUTHER_COPY_AUTHORISED;
		else {
			ret = -EINVAL;
			break;
		}
		break;
	}

	case V4L2_CID_MPEG_STM_AUDIO_CHANNEL_COUNT:
		ctrl_value = encode_param_p->channel_count;
		break;
	case V4L2_CID_MPEG_STM_AUDIO_CHANNEL_MAP:
	{
		int ch_index;
		int channel_id;

		ch_index   = (ctrl_value >> 8) & 0xff;
		channel_id = encode_param_p->channel[ch_index];
		ctrl_value = (ctrl_value & ~(0xff)) | (channel_id & 0xff);

		break;
	}

	case V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT:
	/*FIXME: Currently we need to be able to set this control every time. Hence g_ctrl
	will return 0 every time so as to enable this. Better logic required here*/
		ctrl_value = 0;
		break;


	case V4L2_CID_MPEG_AUDIO_AAC_BITRATE:
		ctrl_value = encode_param_p->bitrate;
		break;

	case V4L2_CID_MPEG_AUDIO_AC3_BITRATE:
	{
		int bitrate;
		bitrate = encode_param_p->bitrate;

		table_length = sizeof(stm_v4l2_audenc_ac3_bitrate_table);
		ctrl_value = stm_v4l2_convert_SE_v4l2_define(bitrate,
							stm_v4l2_audenc_ac3_bitrate_table,
							table_length);
		if(ctrl_value == -1) {
			ret = -EINVAL;
			break;
		}
		break;
	}
	case V4L2_CID_MPEG_AUDIO_L3_BITRATE:
	{
		int bitrate;
		bitrate = encode_param_p->bitrate;

		table_length = sizeof(stm_v4l2_audenc_mp3_bitrate_table);
		ctrl_value = stm_v4l2_convert_SE_v4l2_define(bitrate,
							stm_v4l2_audenc_mp3_bitrate_table,
							table_length);
		if(ctrl_value == -1) {
			ret = -EINVAL;
			break;
		}
		break;
	}
	case V4L2_CID_MPEG_AUDIO_CRC:
	{
		int crc_enable;
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
					STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE,
					&crc_enable);
		if(ret)
			break;

		if(crc_enable == STM_SE_CTRL_VALUE_DISAPPLY)
			ctrl_value = V4L2_MPEG_AUDIO_CRC_NONE;
		else if(crc_enable == STM_SE_CTRL_VALUE_APPLY)
			ctrl_value = V4L2_MPEG_AUDIO_CRC_CRC16;
		else{
			ret = -EINVAL;
			break;
		}

		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	if(!ret)
		ctrl_p->val = ctrl_value;

	mutex_unlock(&dev_p->lock);
	/* we want to be able to return the default values when encode_stream is not created*/
	if(ret && !dev_p->encode_stream){
		ctrl_p->val = ctrl_p->cur.val;
		ret = 0;
	}

	return ret;
}

static int stm_v4l2_encoder_vid_s_ctrl(struct v4l2_ctrl *ctrl_p)
{
	struct stm_v4l2_encoder_device *dev_p = container_of(ctrl_p->handler, struct stm_v4l2_encoder_device, ctrl_handler);
	video_encode_params_t *encode_param_p = &dev_p->encode_params.video;

	unsigned int ctrl_id = ctrl_p->id;
	int ctrl_value = ctrl_p->val ;
	unsigned int table_length;
	int ret=0;

	mutex_lock(&dev_p->lock);

	switch (ctrl_id) {
	case V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE:
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING,
				ctrl_value);
		if(ret)
			break;
		encode_param_p->deinterlace = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING:
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING,
				ctrl_value);
		if(ret)
			break;
		encode_param_p->noise_filter = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO:
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
			   STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO,
			   ctrl_value);
		break;

	case V4L2_CID_MPEG_STM_ENCODE_NRT_MODE:
		ret = stm_se_encode_set_control(dev_p->encode_obj,
					STM_SE_CTRL_ENCODE_NRT_MODE,
					(ctrl_value ? STM_SE_CTRL_VALUE_APPLY :
					STM_SE_CTRL_VALUE_DISAPPLY));
		break;

	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		int bitrate_mode;

		if(ctrl_value == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
			bitrate_mode = STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR;
		else if(ctrl_value == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)
			bitrate_mode = STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR;
		else {
			ret = -EINVAL;
			break;
		}

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE,
				bitrate_mode);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		/* TODO : check the value */
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE,
				ctrl_value);
		break;

	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	{
		int h264_level;

		table_length = sizeof(stm_v4l2_videnc_h264_level_table);
		h264_level = stm_v4l2_convert_v4l2_SE_define(ctrl_value,
					stm_v4l2_videnc_h264_level_table,
					table_length);
		if(h264_level == -1) {
			ret = -EINVAL;
			break;
		}

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL,
				h264_level);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	{
		int h264_profile;

		table_length = sizeof(stm_v4l2_videnc_h264_profile_table);
		h264_profile = stm_v4l2_convert_v4l2_SE_define(ctrl_value,
				stm_v4l2_videnc_h264_profile_table,
				table_length);
		if(h264_profile == -1) {
			ret = -EINVAL;
			break;
		}

		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE,
				h264_profile);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_BITRATE:
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
					STM_SE_CTRL_ENCODE_STREAM_BITRATE,
					ctrl_value);
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		video_multi_slice.control = STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE;
		ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		video_multi_slice.control = STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB;
		video_multi_slice.slice_max_mb_size = ctrl_value;
		ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		video_multi_slice.control = STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES;
		video_multi_slice.slice_max_byte_size = ctrl_value;
		ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);
		break;
	}

	case V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE:
		ret = stm_se_encode_stream_set_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE,
				ctrl_value);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dev_p->lock);
	return ret;
}

static int stm_v4l2_encoder_vid_g_ctrl(struct v4l2_ctrl *ctrl_p)
{
	struct stm_v4l2_encoder_device *dev_p = container_of(ctrl_p->handler, struct stm_v4l2_encoder_device, ctrl_handler);
	video_encode_params_t *encode_param_p = &dev_p->encode_params.video;

	unsigned int ctrl_id = ctrl_p->id;
	unsigned int table_length;
	int ret = 0;
	int32_t ctrl_value = ctrl_p->val;

	mutex_lock(&dev_p->lock);

	switch (ctrl_id) {
	case V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE:
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
			STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING,
			&ctrl_value);
		if(ret)
			break;
		encode_param_p->deinterlace = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING:
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
			STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING,
			&ctrl_value);
		if(ret)
			break;
		encode_param_p->noise_filter = ctrl_value;
		break;
	case V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO:
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
			STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO,
			&ctrl_value);
		break;

	case V4L2_CID_MPEG_STM_ENCODE_NRT_MODE:
		ret = stm_se_encode_get_control(dev_p->encode_obj,
					STM_SE_CTRL_ENCODE_NRT_MODE,
					&ctrl_value);
		break;

	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		int bitrate_mode;

		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
					STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE,
					&bitrate_mode);
		if(ret)
			break;

		if(bitrate_mode == STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR)
			ctrl_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
		else if(bitrate_mode == STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR)
			ctrl_value = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
		else {
			ret = -EINVAL;
			break;
		}
		break;
	}

	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		/* TODO : check the value */
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE,
				&ctrl_value);
		break;

	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	{
		int h264_level;

		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL,
				&h264_level);
		if(ret)
			break;

		table_length = sizeof(stm_v4l2_videnc_h264_level_table);
		ctrl_value = stm_v4l2_convert_SE_v4l2_define(h264_level,
					stm_v4l2_videnc_h264_level_table,
					table_length);
		if(ctrl_value == -1) {
			ret = -EINVAL;
			break;
		}
		break;
	}

	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	{
		int h264_profile;
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE,
				&h264_profile);
		if(ret)
			break;

		table_length = sizeof(stm_v4l2_videnc_h264_profile_table);
		ctrl_value = stm_v4l2_convert_SE_v4l2_define(h264_profile,
					stm_v4l2_videnc_h264_profile_table,
					table_length);
		if(ctrl_value == -1) {
			ret = -EINVAL;
			break;
		}
		break;
	}

	case V4L2_CID_MPEG_VIDEO_BITRATE:
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
					STM_SE_CTRL_ENCODE_STREAM_BITRATE,
					&ctrl_value);
		break;

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		ret = stm_se_encode_stream_get_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);

		if(ret)
			break;

		if(video_multi_slice.control == STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE)
			ctrl_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		else if(video_multi_slice.control == STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB)
			ctrl_value = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB;
		else if(video_multi_slice.control == STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES)
		video_multi_slice.control = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES;
		else
			ret = -EINVAL;

		break;
	}

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		ret = stm_se_encode_stream_get_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);
		if(ret)
			break;

		if(video_multi_slice.control == STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB)
			ctrl_value = video_multi_slice.slice_max_mb_size;
		else
			ctrl_value = 0;/* we need to return a sensible value in case of other slice modes*/

		break;
	}

	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	{
		stm_se_video_multi_slice_t video_multi_slice;

		ret = stm_se_encode_stream_get_compound_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE,
				(void *)&video_multi_slice);
		if(ret)
			break;

		if(video_multi_slice.control == STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES)
			ctrl_value = video_multi_slice.slice_max_byte_size;
		else
			ctrl_value = 0;/* we need to return a sensible value in case of other slice modes*/

		break;
	}

	case V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE:
		ret = stm_se_encode_stream_get_control(dev_p->encode_stream,
				STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE,
				&ctrl_value);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if(!ret)
		ctrl_p->val = ctrl_value;

	mutex_unlock(&dev_p->lock);
	/* we want to be able to return the default values when encode_stream is not created*/
	if(ret && !dev_p->encode_stream){
		ctrl_p->val = ctrl_p->cur.val;
		ret = 0;
	}
	return ret;
}

static int stm_v4l2_encoder_try_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	union {
		s32 val;
		s64 val64;
		char *string;
	} temp_val;

	/* since the values in v4l2 layer are not in sync with those in the kpi, we update
	the current values by querying them here. This is required for the ctrl framework to
	decide to invoke s_ctrl */
	if(ctrl->flags & V4L2_CTRL_FLAG_VOLATILE){

		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			temp_val.string = kzalloc(ctrl->maximum, GFP_KERNEL);
			/* strings are always 0-terminated */
			strcpy(temp_val.string, ctrl->string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			temp_val.val64 = ctrl->val64;
			break;
		default:
			temp_val.val = ctrl->val;
			break;
		}

		ret = ctrl->ops->g_volatile_ctrl(ctrl);
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			/* strings are always 0-terminated */
			strcpy(ctrl->cur.string, ctrl->string);
			strcpy(ctrl->string, temp_val.string);
			kfree(temp_val.string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			ctrl->cur.val64 = ctrl->val64;
			ctrl->val64 = temp_val.val64;
			break;
		default:
			ctrl->cur.val = ctrl->val;
			ctrl->val = temp_val.val;
			break;
		}
	}
	return ret;
}

static const struct v4l2_ctrl_ops stm_v4l2_vid_enc_ctrl_ops = {
	.s_ctrl = stm_v4l2_encoder_vid_s_ctrl,
	.try_ctrl = stm_v4l2_encoder_try_ctrl,
	.g_volatile_ctrl = stm_v4l2_encoder_vid_g_ctrl,
};

static const struct v4l2_ctrl_ops stm_v4l2_aud_enc_ctrl_ops = {
	.s_ctrl = stm_v4l2_encoder_aud_s_ctrl,
	.try_ctrl = stm_v4l2_encoder_try_ctrl,
	.g_volatile_ctrl = stm_v4l2_encoder_aud_g_ctrl,
};

int stm_v4l2_vid_enc_init_ctrl(struct stm_v4l2_encoder_device *dev_p)
{
	static const char * const stm_vid_enc_aspect_ratio_ctrl[] = {
		"Aspect ratio ignore",
		"Aspect ratio 4:3",
		"Aspect ratio 16:9",
		NULL
	};

	static struct v4l2_ctrl_config stm_v4l2_videnc_custom_ctrls[] = {
		{
		 .ops = &stm_v4l2_vid_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_VIDEO_H264_DEI_SET_MODE,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Deinterlacing(on/off)",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &stm_v4l2_vid_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_VIDEO_H264_NOISE_FILTERING,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "Noise Filtering(on/off)",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		.ops = &stm_v4l2_vid_enc_ctrl_ops,
		.id = V4L2_CID_MPEG_STM_VIDEO_H264_ASPECT_RATIO,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Aspect ratio control",
		.min = V4L2_STM_ENCODER_ASPECT_RATIO_IGNORE,
		.max = V4L2_STM_ENCODER_ASPECT_RATIO_16_9,
		.menu_skip_mask = 0,
		.def = V4L2_STM_ENCODER_ASPECT_RATIO_IGNORE,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = stm_vid_enc_aspect_ratio_ctrl,
		},
		{
		.ops = &stm_v4l2_vid_enc_ctrl_ops,
		.id = V4L2_CID_MPEG_STM_ENCODE_NRT_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Video Encoding Non-Real Time Mode",
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		}
	};

	static struct v4l2_ctrl_config stm_v4l2_videnc_ctrls[] = {
		{
		 .id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		 .min = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		 .max = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR,
		 .step = 1,
		 .def = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		 .min = 0,
		 .max = (1 << 16) - 1,
		 .step = 1,
		 .def = 10,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		 .min = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		 .max = V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		 .step = 1,
		 .def = V4L2_MPEG_VIDEO_H264_LEVEL_3_1,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		 .min = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		 .max = V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH,
		 .step = 1,
		 .def = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_BITRATE,
		 .min = 1,
		 .max = (1 << 30) - 1,
		 .step = 1,
		 .def = 4000000,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		 .min = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		 .max = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES,
		 .step = 1,
		 .def = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		 },
		{
		 .id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB,
		 .min = 0,
		 .max = INT_MAX,
		 .step = 1,
		 .def = 0,
		},
		{
		 .id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES,
		 .min = 0,
		 .max = INT_MAX,
		 .step = 1,
		 .def = 0,
		},
		{
		 .id = V4L2_CID_MPEG_VIDEO_H264_CPB_SIZE,
		 .min = 0,
		 .max = (1 << 30) - 1,
		 .step = 1,
		 .def = 2000000,
		 },
	};

	int ret = 0, i;
	struct v4l2_ctrl_config *cfg;
	struct v4l2_ctrl *ctrl;
	/* Even though the number of controls is not required to be accurate,
	we try to give good estimate*/

	ret = v4l2_ctrl_handler_init(&dev_p->ctrl_handler,
		ARRAY_SIZE(stm_v4l2_videnc_ctrls) + ARRAY_SIZE(stm_v4l2_videnc_custom_ctrls));
	if(ret){
		printk(KERN_ERR"Control registration failed for vid encoder ret = %d", ret);
		return ret;
	}


	for(i = 0; i <  ARRAY_SIZE(stm_v4l2_videnc_custom_ctrls); i++){
		v4l2_ctrl_new_custom(&dev_p->ctrl_handler, &stm_v4l2_videnc_custom_ctrls[i], NULL);
		if (dev_p->ctrl_handler.error) {
			ret = dev_p->ctrl_handler.error;
			v4l2_ctrl_handler_free(&dev_p->ctrl_handler);
			printk(KERN_ERR"Custom Control reg vid encoder failed. Ret = %x", ret);
			return ret;
		}
	}

	for(i = 0; i < ARRAY_SIZE(stm_v4l2_videnc_ctrls); i++){
		cfg = &stm_v4l2_videnc_ctrls[i];
		if(v4l2_ctrl_get_menu(cfg->id))
			ctrl = v4l2_ctrl_new_std_menu(&dev_p->ctrl_handler, &stm_v4l2_vid_enc_ctrl_ops, cfg->id, cfg->max, 0, cfg->def);
		else
			ctrl = v4l2_ctrl_new_std(&dev_p->ctrl_handler, &stm_v4l2_vid_enc_ctrl_ops, cfg->id, cfg->min, cfg->max, cfg->step, cfg->def);

		if (dev_p->ctrl_handler.error) {
			ret = dev_p->ctrl_handler.error;
			v4l2_ctrl_handler_free(&dev_p->ctrl_handler);
			printk(KERN_ERR"Std Control reg vid encoder failed. Ret = %d Array elem = %d", ret, i);
			return ret;
		}
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	return 0;
}

int stm_v4l2_aud_enc_init_ctrl(struct stm_v4l2_encoder_device *dev_p)
{
	static const char * const stm_enc_ctrl_aud_sampling_freq[] = {
		"4 kHz",
		"5 Khz",
		"6 kHz",
		"8 kHz",
		"11.025 kHz",
		"12 kHz",
		"16 kHz",
		"22.05 kHz",
		"24 kHz",
		"32 kHz",
		"44.1 kHz"
		"48 kHz",
		"64 kHz",
		"88.2 kHz",
		"96 kHz",
		"128 kHz",
		"176.4 kHz",
		"192 kHz",
		"256 kHz",
		"352.8 kHz",
		"384 kHz",
		"512 kHz",
		"705.6 kHz",
		"768 kHz",
		NULL
	};

	static const char * const stm_aud_enc_bitrate_mode[] = {
		"Bit rate mode CBR",
		"Bit rate mode VBR",
		NULL
	};

	static const char * const stm_aud_enc_serial_ctrl[] = {
		"No Copyright",
		"One more copy authorised",
		"No further copy authorised",
		NULL
	};

	static struct v4l2_ctrl_config stm_v4l2_custom_audenc_ctrls[] = {

		/*
		 * The V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ control is actually
		 * a standard control, but due to kernel small menu for this
		 * control, this is made as custom control, so, that apps
		 * can expedite all the options without kernel patch.
		 */
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
		 .type = V4L2_CTRL_TYPE_MENU,
		 .name = "Custom Audio Sampling Frequency",
		 .min = V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_4000,
		 .max = V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_768000,
		 .menu_skip_mask = 0,
		 .def = V4L2_MPEG_AUDIO_STM_SAMPLING_FREQ_4000,
		 .qmenu = stm_enc_ctrl_aud_sampling_freq,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_MODE,
		 .type = V4L2_CTRL_TYPE_MENU,
		 .name = "audenc bitrate mode",
		 .min = V4L2_MPEG_AUDIO_BITRATE_MODE_CBR,
		 .max = V4L2_MPEG_AUDIO_BITRATE_MODE_VBR,
		 .menu_skip_mask = 0,
		 .qmenu = stm_aud_enc_bitrate_mode,
		 .def = V4L2_MPEG_AUDIO_BITRATE_MODE_CBR,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		 },
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_VBR_QUALITY_FACTOR,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "audenc vbr quality factor",
		 .min = 0,
		 .max = 100,
		 .step = 1,
		 .def = 100,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_CAP,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "audenc bitrate cap",
		 .min = 0,
		 .max = INT_MAX,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_BITRATE_CONTROL,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "audenc audio bitrate control",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 1,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_SERIAL_CONTROL,
		 .type = V4L2_CTRL_TYPE_MENU,
		 .name = "audenc serial control management",
		 .min = V4L2_MPEG_AUDIO_NO_COPYRIGHT,
		 .max = V4L2_MPEG_AUDIO_NO_FUTHER_COPY_AUTHORISED,
		 .menu_skip_mask = 0,
		 .qmenu = stm_aud_enc_serial_ctrl,
		 .def = V4L2_MPEG_AUDIO_NO_COPYRIGHT,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_CHANNEL_MAP,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "channel map",
		 .min = 0,
		 .max = 8191,
		 .step = 1,
		 .def = 0,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_CHANNEL_COUNT,
		 .type = V4L2_CTRL_TYPE_INTEGER,
		 .name = "channel count",
		 .min = 0,
		 .max = 32,
		 .step = 1,
		 .def = 2,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_CORE_FORMAT,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "channel core format",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 1,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{/* FIXME this control is not implemented properly in SE. Not sure of values*/
		 .ops = &stm_v4l2_aud_enc_ctrl_ops,
		 .id = V4L2_CID_MPEG_STM_AUDIO_PROGRAM_LEVEL,
		 .type = V4L2_CTRL_TYPE_BOOLEAN,
		 .name = "encoding program level",
		 .min = 0,
		 .max = 1,
		 .step = 1,
		 .def = 1,
		 .flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
		.ops = &stm_v4l2_aud_enc_ctrl_ops,
		.id = V4L2_CID_MPEG_STM_ENCODE_NRT_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Audio Encoding Non-Real Time Mode",
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
	};

	static struct v4l2_ctrl_config stm_v4l2_audenc_std_ctrls[] = {
		{
		 .id = V4L2_CID_MPEG_AUDIO_AAC_BITRATE,
		 .min = 0,
		 .max = (1 << 30) - 1,
		 .step = 1,
		 .def = 48000,
		},
		{
		 .id = V4L2_CID_MPEG_AUDIO_AC3_BITRATE,
		 .min = V4L2_MPEG_AUDIO_AC3_BITRATE_32K,
		 .max = V4L2_MPEG_AUDIO_AC3_BITRATE_640K,
		 .step = 1,
		 .def = V4L2_MPEG_AUDIO_AC3_BITRATE_48K,
		},
		{
		 .id = V4L2_CID_MPEG_AUDIO_L3_BITRATE,
		 .min = V4L2_MPEG_AUDIO_L3_BITRATE_32K,
		 .max = V4L2_MPEG_AUDIO_L3_BITRATE_320K,
		 .step = 1,
		 .def = V4L2_MPEG_AUDIO_L3_BITRATE_48K,
		},
		{
		 .id = V4L2_CID_MPEG_AUDIO_CRC,
		 .min = V4L2_MPEG_AUDIO_CRC_NONE,
		 .max = V4L2_MPEG_AUDIO_CRC_CRC16,
		 .step = 1,
		 .def = V4L2_MPEG_AUDIO_CRC_NONE,
		},
	};
	int ret = 0, i;
	struct v4l2_ctrl_config *cfg;
	struct v4l2_ctrl *ctrl;
	/* Even though the number of controls is not required to be accurate,
	we try to give good estimate*/

	ret = v4l2_ctrl_handler_init(&dev_p->ctrl_handler,
		ARRAY_SIZE(stm_v4l2_custom_audenc_ctrls) + ARRAY_SIZE(stm_v4l2_audenc_std_ctrls));
	if(ret){
		printk(KERN_ERR"Control registration failed for aud encoder ret = %d", ret);
		return ret;
	}

	for(i = 0; i <  ARRAY_SIZE(stm_v4l2_custom_audenc_ctrls); i++){
		v4l2_ctrl_new_custom(&dev_p->ctrl_handler, &stm_v4l2_custom_audenc_ctrls[i], NULL);
		if (dev_p->ctrl_handler.error) {
			ret = dev_p->ctrl_handler.error;
			v4l2_ctrl_handler_free(&dev_p->ctrl_handler);
			printk(KERN_ERR"Custom Control reg aud encoder failed. Ret = %x", ret);
			return ret;
		}
	}

	for(i = 0; i < ARRAY_SIZE(stm_v4l2_audenc_std_ctrls); i++){
		cfg = &stm_v4l2_audenc_std_ctrls[i];
		if(v4l2_ctrl_get_menu(cfg->id))
			ctrl = v4l2_ctrl_new_std_menu(&dev_p->ctrl_handler, &stm_v4l2_aud_enc_ctrl_ops, cfg->id, cfg->max, 0, cfg->def);
		else
			ctrl = v4l2_ctrl_new_std(&dev_p->ctrl_handler, &stm_v4l2_aud_enc_ctrl_ops, cfg->id, cfg->min, cfg->max, cfg->step, cfg->def);

		if (dev_p->ctrl_handler.error) {
			ret = dev_p->ctrl_handler.error;
			v4l2_ctrl_handler_free(&dev_p->ctrl_handler);
			printk(KERN_ERR"Std Control reg aud encoder failed. Ret = %d Array elem = %d", ret, i);
			return ret;
		}
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	return 0;
}
