/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 *************************************************************************/
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <asm/uaccess.h>
#include <asm/irq.h>

#include <media/v4l2-dev.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#include "stmhdmi_device.h"

static char *audio_formats[STM_CEA_AUDIO_LAST+1] = {
	"Reserved",
	"LPCM",
	"AC3",
	"MPEG1(L1/2)",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"One Bit Audio",
	"Dolby Digital+",
	"DTS-HD",
	"MLP",
	"DST",
	"WMA Pro",
	"Reserved"
};

static char *ex_audio_formats[STM_CEA_AUDIO_EX_LAST+1] = {
	"Reserved",
	"HE-ACC",
	"HE-ACCv2",
	"MPEG Surround",
};

static char *scan_support[STM_CEA_VCDB_CE_SCAN_BOTH+1] = {
	"Unsupported",
	"Overscanned",
	"Underscanned",
	"Selectable",
};

static char *ptscan_support[STM_CEA_VCDB_CE_SCAN_BOTH+1] = {
	"Undefined",
	"Overscanned",
	"Underscanned",
	"Selectable",
};

/**
 * stmhdmi_mode_string
 * Convert an stm_display_mode_t to ASCII, the ASCII format is defined in
 * fbsysfs.c:mode_string()
 */
static int stmhdmi_mode_string(char *buf, unsigned int offset,
						const stm_display_mode_t *mode)
{
	char m = 'U';
	char v = 'p';

	if (mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] ||
			mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9]) {
		/*
		 *  A standard (meaning, in our case, CEA-861) mode
		 */
		m = 'S';
	}

	if (mode->mode_params.scan_type == STM_INTERLACED_SCAN) {
		v = 'i';
	}

	return sprintf(&buf[offset], "%c:%ux%u%c-%u\n", m,
			mode->mode_params.active_area_width,
			mode->mode_params.active_area_height,
			v, mode->mode_params.vertical_refresh_rate / 1000);
}

/**
 * stmhdmi_enumerate_dvi_modes
 * When no display modes can be retrieved from the display device (probably
 * because the device uses DVI rather than true HDMI). Here, we examine every
 * display mode supported by the driver to determine if it meets the refresh
 * rate/pixel clock contraints reported by the display device.
 */
static int stmhdmi_enumerate_dvi_modes(struct stm_hdmi *hdmi, stm_display_mode_t *display_modes)
{
	unsigned int clock, max_clock;
	unsigned int vfreq, min_vfreq, max_vfreq;
	unsigned int hfreq, min_hfreq, max_hfreq;
	int i, ret, num_modes = 0;
	stm_display_mode_t  mode;

	mutex_lock(&hdmi->lock);

	max_clock = hdmi->edid_info.max_clock * 1000000;
	min_vfreq = hdmi->edid_info.min_vfreq * 1000;
	max_vfreq = hdmi->edid_info.max_vfreq * 1000;
	min_hfreq = hdmi->edid_info.min_hfreq * 1000000;
	max_hfreq = hdmi->edid_info.max_hfreq * 1000000;

	for (i = 0; i < STM_TIMING_MODE_COUNT; i++) {
		ret = stm_display_output_get_display_mode(hdmi->
						main_output, i, &mode);
		if (ret)
			continue;

		/*
		 * Ignore non CEA861 or VESA modes
		 */
		if(!(mode.mode_params.output_standards &
			 (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_VESA_MASK)))
			continue;

		clock = mode.mode_timing.pixel_clock_freq;
		vfreq = mode.mode_params.vertical_refresh_rate;
		hfreq = vfreq * mode.mode_timing.lines_per_frame;
		if (STM_INTERLACED_SCAN == mode.mode_params.scan_type)
			hfreq /= 2;

		if ((clock <= max_clock) && (min_vfreq <= vfreq) &&
			(vfreq <= max_vfreq) && (min_hfreq <= hfreq) &&
			(hfreq <= max_hfreq))
			display_modes[num_modes++] = mode;
	}

	mutex_unlock(&hdmi->lock);

	return num_modes;
}

/**
 * stmhdmi_show_aspect_ratio
 * Display the aspect ratio of the connected monitor
 */
static ssize_t stmhdmi_show_aspect_ratio(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	switch(hdmi->edid_info.tv_aspect) {
	case STM_WSS_ASPECT_RATIO_4_3:
		ret = sprintf(buf, "%s\n", "4:3");
		break;

	case STM_WSS_ASPECT_RATIO_16_9:
		ret = sprintf(buf, "%s\n", "16:9");
		break;

	case STM_WSS_ASPECT_RATIO_14_9:
		ret = sprintf(buf, "%s\n", "14:9");
		break;

	case STM_WSS_ASPECT_RATIO_GT_16_9:
		ret = sprintf(buf, "%s\n", "GT|16:9");
		break;

	case STM_WSS_ASPECT_RATIO_UNKNOWN:
	default:
		ret = sprintf(buf, "%s\n", "UNKNOWN");
		break;
	}

	return ret;
}


/**
 * stmhdmi_show_cea861_codes
 */
static ssize_t stmhdmi_show_cea861_codes(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0, i;

	ret = sprintf(buf, "NATIVE: ");
	for(i = 0; i < STM_MAX_CEA_MODES; i++) {
		if (hdmi->edid_info.cea_codes[i].cea_code_descriptor ==
					STM_CEA_VIDEO_CODE_NATIVE) {
			ret += sprintf(buf + ret, "%d, ", i);
		}
	}
	ret += sprintf(buf + ret - 2, "\n");

	ret += sprintf(buf + ret, "NON-NATIVE: ");
	for(i = 0; i < STM_MAX_CEA_MODES; i++) {
		if (hdmi->edid_info.cea_codes[i].cea_code_descriptor ==
					STM_CEA_VIDEO_CODE_NON_NATIVE) {
			ret += sprintf(buf + ret, "%d, ", i);
		}
	}
	ret += sprintf(buf + ret - 2, "\n");

	ret += sprintf(buf + ret, "NON-SUPPORTED: ");
	for(i = 0; i < STM_MAX_CEA_MODES; i++) {
		if (hdmi->edid_info.cea_codes[i].cea_code_descriptor ==
					STM_CEA_VIDEO_CODE_UNSUPPORTED) {
			ret += sprintf(buf + ret, "%d, ", i);
		}
	}
	ret += sprintf(buf + ret - 2, "\n");

	return ret;
}

/**
 * stmhdmi_show_display_type
 */
static ssize_t stmhdmi_show_display_type(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	switch(hdmi->edid_info.display_type) {
		case STM_DISPLAY_DVI:
			ret = sprintf(buf, "%s\n", "DVI");
			break;

		case STM_DISPLAY_HDMI:
			ret = sprintf(buf, "%s\n", "HDMI");
			break;

		case STM_DISPLAY_INVALID:
			ret = sprintf(buf, "%s\n", "INVALID");
			break;

		default:
			ret = sprintf(buf, "%s\n", "UNKNOWN");
	}

	return ret;
}

/**
 * show_hdmi_monitor_name
 * Display the name of the monitor connected to the HDMI
 */
static ssize_t stmhdmi_show_monitor_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);

	return sprintf(buf, "%s\n", (hdmi->edid_info.monitor_name[0] != 0) ?
				hdmi->edid_info.monitor_name : "UNKNOWN");
}

/**
 * stmhdmi_show_hotplug
 */
static ssize_t stmhdmi_show_hotplug(struct device *dev, struct device_attribute *attr, char *buf)
{
	stm_display_link_hpd_state_t state;
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	ret =  stm_display_link_hpd_get_state(hdmi->link, &state);
	if (ret) {
		HDMI_ERROR("Cannot get HPD state\n");
		goto show_hotplug_get_hpd_state_failed;
	}

	ret = sprintf(buf, "%s\n", (state == STM_DISPLAY_LINK_HPD_STATE_HIGH) ?
							 "HIGH" : "LOW");

show_hotplug_get_hpd_state_failed:
	return ret;
}

/**
 * stmhdmi_show_modes
 */
static ssize_t stmhdmi_show_modes(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	ssize_t ret = 0;
	int i;

	/*
	 * If no modes were retrieved from the display device then we must
	 * infer them from the display characteristics.
	 */
	if ((hdmi->edid_info.display_type == STM_DISPLAY_DVI) &&
					 (hdmi->edid_info.num_modes <= 1)) {
		stm_display_mode_t *display_modes;
		int num_modes;

		display_modes = (stm_display_mode_t *)kzalloc
			(sizeof(stm_display_mode_t) * STM_TIMING_MODE_COUNT,
								GFP_KERNEL);
		if (!display_modes) {
			HDMI_ERROR("Out of memory for modes\n");
			return 0;
		}

		num_modes = stmhdmi_enumerate_dvi_modes(hdmi, display_modes);
		for (i = 0; i < num_modes; i++) {
			ret += stmhdmi_mode_string(buf, ret, &display_modes[i]);
		}
	
		kfree(display_modes);
		return ret;
	}

	for (i = 0; i < hdmi->edid_info.num_modes; i++)
		ret += stmhdmi_mode_string(buf, ret, &(hdmi->edid_info.display_modes[i]));

	return ret;
}

/**
 * stmhdmi_show_speaker_allocs
 */
static ssize_t stmhdmi_show_speaker_allocs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLFR)
		ret += sprintf(buf + ret, "%s", "FL/FR");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_LFE)
		ret += sprintf(buf + ret, "%s", ":LFE");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FC)
		ret += sprintf(buf + ret, "%s", ":FC");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RLRR)
		ret += sprintf(buf + ret, "%s", ":RL/RR");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RC)
		ret += sprintf(buf + ret, "%s", ":RC");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLCFRC)
		ret += sprintf(buf + ret, "%s", ":FLC/FRC");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RLCRRC)
		ret += sprintf(buf + ret, "%s", ":RLC/RRC");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLWFRW)
		ret += sprintf(buf + ret, "%s", ":FLW/FRW");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLHFRH)
		ret += sprintf(buf + ret, "%s", ":FLH/FRH");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_TC)
		ret += sprintf(buf + ret, "%s", ":TC");

	if (hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FCH)
		ret += sprintf(buf + ret, "%s", ":FCH");

	ret += sprintf(buf + ret, "\n");

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_audio_caps
 */
static ssize_t stmhdmi_show_audio_caps(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	stm_cea_audio_formats_t fmt;
	struct stm_cea_audio_descriptor *mode;
	int ret = 0;

	mutex_lock(&hdmi->lock);

	for(fmt = STM_CEA_AUDIO_LPCM; fmt <= STM_CEA_AUDIO_LAST ; fmt++) {
		mode = &hdmi->edid_info.audio_modes[fmt];
		if(!hdmi->edid_info.audio_modes[fmt].format) {
			continue;
		}

		if(mode->format < STM_CEA_AUDIO_LAST)
			ret += sprintf(buf, "%u:%s", mode->format,
					audio_formats[mode->format]);
		else
			ret += sprintf(buf, "%u.%u:%s", mode->format, mode->
					ex_format, ex_audio_formats[mode->ex_format]);

		ret += sprintf(buf + ret, ":%uch", mode->max_channels);

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_32_KHZ)
			ret += sprintf(buf + ret, "%s", ":32KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_44_1_KHZ)
			ret += sprintf(buf + ret, "%s", ":44.1KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_48_KHZ)
			ret += sprintf(buf + ret, "%s", ":48KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_88_2_KHZ)
			ret += sprintf(buf + ret, "%s", ":88.2KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_96_KHZ)
			ret += sprintf(buf + ret, "%s", ":96KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_176_4_KHZ)
			ret += sprintf(buf + ret, "%s", ":176.4KHz");

		if(mode->sample_rates & STM_CEA_AUDIO_RATE_192_KHZ)
			ret += sprintf(buf + ret, "%s", ":192KHz");

		if(mode->format == STM_CEA_AUDIO_LPCM) {
			if(mode->lpcm_bit_depths & 0x1)
				ret += sprintf(buf + ret, "%s", ":16bit");

			if(mode->lpcm_bit_depths & 0x2)
				ret += sprintf(buf + ret, "%s", ":20bit");

			if(mode->lpcm_bit_depths & 0x4)
				ret += sprintf(buf + ret, "%s", ":24bit");
		}

		if(mode->format >= STM_CEA_AUDIO_AC3 &&
				 mode->format <= STM_CEA_AUDIO_ATRAC)
			ret += sprintf(buf + ret, ":%ukbps", mode->max_bit_rate);

		if(mode->format == STM_CEA_AUDIO_WMA_PRO)
			ret += sprintf(buf + ret, ":profile-%u", mode->profile);

		ret += sprintf(buf + ret,"\n");

	}

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_cec_address
 */
static ssize_t stmhdmi_show_cec_address(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	ret += sprintf(buf,"%x.%x.%x.%x\n",hdmi->edid_info.cec_address[0],
					hdmi->edid_info.cec_address[1],
					hdmi->edid_info.cec_address[2],
					hdmi->edid_info.cec_address[3]);

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_ai_support
 */
static ssize_t stmhdmi_show_ai_support(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	ret += sprintf(buf,"%s\n",(hdmi->edid_info.hdmi_vsdb_flags &
				STM_HDMI_VSDB_SUPPORTS_AI) ? "Yes" : "No");

	mutex_unlock(&hdmi->lock);

	return ret;
}



/**
 * stmhdmi_show_scanmode
 */
static ssize_t stmhdmi_show_scanmode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	ret += sprintf(buf, "CEA:%s\n", (hdmi->edid_info.cea_capabilities &
				STM_CEA_CAPS_UNDERSCAN) ?
				"Underscanned" : "Overscanned");

	ret += sprintf(buf + ret, "VCDB:%s", scan_support
					[hdmi->edid_info.cea_vcdb_flags & 0x3]);

	ret += sprintf(buf + ret, ":%s", scan_support
				[(hdmi->edid_info.cea_vcdb_flags >> 2) & 0x3]);

	ret += sprintf(buf + ret, ":%s\n", ptscan_support
				[(hdmi->edid_info.cea_vcdb_flags >> 4) & 0x3]);

	mutex_unlock(&hdmi->lock);

	return ret;
}


static char *quantization[4] = {
	"Unsupported",
	"RGB",
	"YCC",
	"RGB:YCC",
};

/**
 * stmhdmi_show_quantization
 */
static ssize_t stmhdmi_show_quantization(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	ret += sprintf(buf, "%s\n", quantization
				[((hdmi->edid_info.cea_vcdb_flags>>6)&0x3)]);

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_colorspaces
 */
static ssize_t stmhdmi_show_colorspaces(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	if(hdmi->edid_info.cea_capabilities & STM_CEA_CAPS_SRGB)
		ret += sprintf(buf + ret, "%s", "sRGB:");

	if(hdmi->edid_info.cea_capabilities &
				(STM_CEA_CAPS_YUV | STM_CEA_CAPS_422))
		ret += sprintf(buf + ret, "%s", "Ycc601:Ycc709:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_XVYCC601)
		ret += sprintf(buf + ret, "%s", "xvYcc601:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_XVYCC709)
		ret += sprintf(buf + ret, "%s", "xvYcc709:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_SYCC601)
		ret += sprintf(buf + ret, "%s", "sYcc601:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_ADOBEYCC601)
		ret += sprintf(buf + ret, "%s", "AdobeYcc601:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_ADOBERGB)
		ret += sprintf(buf + ret, "%s", "AdobeRGB:");

	ret += sprintf(buf + ret - 1, "\n");

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_colorgamut
 */
static ssize_t stmhdmi_show_colorgamut(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_MD_PROFILE0)
		ret += sprintf(buf + ret, "%s", "0:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_MD_PROFILE1)
		ret += sprintf(buf + ret, "%s", "1:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_MD_PROFILE2)
		ret += sprintf(buf + ret, "%s", "2:");

	if(hdmi->edid_info.cea_coldb_flags & STM_CEA_COLDB_MD_PROFILE3)
		ret += sprintf(buf + ret, "%s", "3:");

	if(!ret)
		ret += sprintf(buf + ret, "%s", "none");

	ret += sprintf(buf + ret - 1, "\n");

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_colorformat
 */
static ssize_t stmhdmi_show_colorformat(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

	if(hdmi->edid_info.cea_capabilities & STM_CEA_CAPS_SRGB)
		ret += sprintf(buf + ret, "%s", "RGB:24");

	if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_30BIT)
		ret += sprintf(buf + ret, "%s", ":30");

	if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_36BIT)
		ret += sprintf(buf + ret, "%s", ":36");

	if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_48BIT)
		ret += sprintf(buf + ret, "%s", ":48");

	ret += sprintf(buf + ret, "\n");

	if(hdmi->edid_info.cea_capabilities & STM_CEA_CAPS_422)
		ret += sprintf(buf + ret, "%s", "YUV422:24\n");

	if(hdmi->edid_info.cea_capabilities & STM_CEA_CAPS_422)
		ret += sprintf(buf + ret, "%s", "YUV444:24");

	if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_Y444)
	{
		if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_30BIT)
		ret += sprintf(buf + ret, "%s", ":30");

		if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_36BIT)
		ret += sprintf(buf + ret, "%s", ":36");

		if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_48BIT)
		ret += sprintf(buf + ret, "%s", ":48");
	}

	ret += sprintf(buf + ret, "\n");

	mutex_unlock(&hdmi->lock);

	return ret;
}

/**
 * stmhdmi_show_avlatency
 */
static ssize_t stmhdmi_show_avlatency(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int ret = 0;

	mutex_lock(&hdmi->lock);

# define DISPLAY_LATENCY(x)						\
		if((x)==255) 						\
			ret += sprintf(buf + ret, "No output\n");	\
		else if (((x) > 0) && ((x) <= 251)) 			\
			ret += sprintf(buf + ret, "%d\n",((x)-1)*2);	\
		else							\
			ret += sprintf(buf + ret, "%d\n",0);		\

	ret += sprintf(buf + ret, "%s", "Progressive Video:");
	DISPLAY_LATENCY(hdmi->edid_info.progressive_video_latency);

	ret += sprintf(buf + ret, "%s", "Progressive Audio:");
	DISPLAY_LATENCY(hdmi->edid_info.progressive_audio_latency);

	ret += sprintf(buf + ret, "%s", "Interlaced Video:");
	DISPLAY_LATENCY(hdmi->edid_info.interlaced_video_latency);

	ret += sprintf(buf + ret, "%s", "Interlaced Audio:");
	DISPLAY_LATENCY(hdmi->edid_info.interlaced_audio_latency);

	mutex_unlock(&hdmi->lock);

	return ret;
}


static ssize_t stmhdmi_show_tmds_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	stm_display_mode_t current_hdmi_mode = { STM_TIMING_MODE_RESERVED };
	int ret = 0;

	mutex_lock(&hdmi->lock);

	ret = stm_display_output_get_current_display_mode(hdmi->hdmi_output,
							&current_hdmi_mode);
	if (ret)
		ret += sprintf(buf + ret, "%s", "Stopped");
	else
		ret += sprintf(buf + ret, "%s", "Running");

	ret += sprintf(buf + ret, "\n");

	mutex_unlock(&hdmi->lock);

	return ret;
}

/*
 * stmhdmi_show_3D_modes
 */
static ssize_t stmhdmi_show_3D_modes(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(dev);
	struct stm_hdmi *hdmi = video_get_drvdata(vdev);
	int i, ret = 0;

	ret += sprintf(buf + ret, "%s", "2D (CEA_CODE): ");
	for(i = 0;i < STM_MAX_CEA_MODES; i++) {
		if(hdmi->edid_info.cea_codes[i].cea_code_descriptor ==
						 STM_CEA_VIDEO_CODE_UNSUPPORTED)
			continue;


		if (!(hdmi->edid_info.cea_codes[i].supported_3d_flags &
				 STM_MODE_FLAGS_3D_MASK)) {
			ret += sprintf(buf + ret, "%d, ", i);
		}
	}
	ret += sprintf(buf + ret - 2, "\n");

	ret += sprintf(buf + ret, "%s", "3D (CEA_CODE): ");
	for(i=0;i<STM_MAX_CEA_MODES;i++) {
		if ((hdmi->edid_info.cea_codes[i].cea_code_descriptor ==
			STM_CEA_VIDEO_CODE_UNSUPPORTED) ||
			!(hdmi->edid_info.cea_codes[i].supported_3d_flags &
				 STM_MODE_FLAGS_3D_MASK))
			continue;

		if (hdmi->edid_info.cea_codes[i].supported_3d_flags &
					 STM_MODE_FLAGS_3D_FRAME_PACKED)
			ret += sprintf(buf + ret, "%d-%s, ", i, "FRAME_PACKED");

		if (hdmi->edid_info.cea_codes[i].supported_3d_flags &
					STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE)
			ret += sprintf(buf + ret, "%d-%s, ", i,
							"FIELD_ALTERNATIVE");

		if (hdmi->edid_info.cea_codes[i].supported_3d_flags &
						STM_MODE_FLAGS_3D_TOP_BOTTOM)
			ret += sprintf(buf + ret, "%d-%s, ", i, "TOP_BOTTOM");

		if (hdmi->edid_info.cea_codes[i].supported_3d_flags &
					 STM_MODE_FLAGS_3D_SBS_HALF) {
			ret += sprintf(buf + ret, "%d-%s, ", i, "SBS_HALF");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x0)
				ret += sprintf(buf + ret, "%s",
							"[ALL_SUB_SAMPLING]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x1)
				ret += sprintf(buf + ret, "%s",
							"[H_SUB_SAMPLING]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x6)
				ret += sprintf(buf + ret, "%s",
						"[ALL_QUINCUNX_MATRIX]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x7)
				ret += sprintf(buf + ret, "%s",
						"[QUINCUNX_MATRIX_OLP_ORP]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x8)
				ret += sprintf(buf + ret, "%s",
						"[QUINCUNX_MATRIX_OLP_ERP]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x9)
				ret += sprintf(buf + ret, "%s",
						"[QUINCUNX_MATRIX_ELP_ORP]");

			if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0xA)
				ret += sprintf(buf + ret, "%s",
						"[QUINCUNX_MATRIX_ELP_ERP]");
			}

		ret += sprintf(buf + ret - 2, "%s", "");
	}
	ret += sprintf(buf + ret, "\n");

	return ret;
}

/*
 * sthdmi device attributes
 */
static struct device_attribute stmhdmi_device_attrs[] = {
	__ATTR(hdmi_aspect_ratio, S_IRUGO, stmhdmi_show_aspect_ratio, NULL),
	__ATTR(hdmi_cea861_codes, S_IRUGO, stmhdmi_show_cea861_codes, NULL),
	__ATTR(hdmi_display_type, S_IRUGO, stmhdmi_show_display_type, NULL),
	__ATTR(hdmi_monitor_name, S_IRUGO, stmhdmi_show_monitor_name, NULL),
	__ATTR(hdmi_hotplug, S_IRUGO, stmhdmi_show_hotplug, NULL),
	__ATTR(hdmi_modes, S_IRUGO, stmhdmi_show_modes, NULL),
	__ATTR(hdmi_speaker_allocs, S_IRUGO, stmhdmi_show_speaker_allocs, NULL),
	__ATTR(hdmi_audio_caps, S_IRUGO, stmhdmi_show_audio_caps, NULL),
	__ATTR(hdmi_cec_address, S_IRUGO, stmhdmi_show_cec_address, NULL),
	__ATTR(hdmi_ai_support, S_IRUGO, stmhdmi_show_ai_support, NULL),
	__ATTR(hdmi_scanmode, S_IRUGO, stmhdmi_show_scanmode, NULL),
	__ATTR(hdmi_quantization, S_IRUGO, stmhdmi_show_quantization, NULL),
	__ATTR(hdmi_colorspaces, S_IRUGO, stmhdmi_show_colorspaces, NULL),
	__ATTR(hdmi_colorgamut, S_IRUGO, stmhdmi_show_colorgamut, NULL),
	__ATTR(hdmi_color_format, S_IRUGO, stmhdmi_show_colorformat, NULL),
	__ATTR(hdmi_av_latency, S_IRUGO, stmhdmi_show_avlatency, NULL),
	__ATTR(hdmi_tmds_status, S_IRUGO, stmhdmi_show_tmds_status, NULL),
	__ATTR(hdmi_3d_modes, S_IRUGO, stmhdmi_show_3D_modes, NULL)
};

/**
 * stmhdmi_register_sysfs_attributes
 * Registers sysfs attributes for HDMI driver
 */
int stmhdmi_register_sysfs_attributes(struct device *device)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(stmhdmi_device_attrs); i++) {
		ret = device_create_file(device, &stmhdmi_device_attrs[i]);
		if(ret) {
			HDMI_ERROR("sysfs entry failed at index: %d\n", i);
			break;
		}
	}

	if (i < ARRAY_SIZE(stmhdmi_device_attrs)) {
		for (;i >= 0; i--)
			device_remove_file(device, &stmhdmi_device_attrs[i]);
	}

	return ret;
}

/**
 * stmhdmi_unregister_sysfs_attributes
 * Unregister sysfs attributes for HDMI driver
 */
void stmhdmi_unregister_sysfs_attributes(struct device *device)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(stmhdmi_device_attrs); i++)
		device_remove_file(device, &stmhdmi_device_attrs[i]);

}
