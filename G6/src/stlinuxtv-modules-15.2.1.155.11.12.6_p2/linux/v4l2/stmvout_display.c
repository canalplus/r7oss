/***********************************************************************
 *
 * File: linux/kernel/drivers/media/video/stmvout_display.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
 ***********************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>

#include <stm_display.h>

#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "linux/stmvout.h"

struct standard_list_entry_s {
	stm_display_mode_id_t mode;
	uint32_t tvStandard;
	v4l2_std_id v4l_standard;
	const char *name;
};

static const struct standard_list_entry_s standard_list[] = {
	{STM_TIMING_MODE_480I59940_13500, STM_OUTPUT_STD_NTSC_M,
	 V4L2_STD_NTSC_M, "NTSC-M"},
	{STM_TIMING_MODE_480I59940_13500, STM_OUTPUT_STD_NTSC_J,
	 V4L2_STD_NTSC_M_JP, "NTSC-J"},
	{STM_TIMING_MODE_480I59940_13500, STM_OUTPUT_STD_NTSC_443,
	 V4L2_STD_NTSC_443, "NTSC-443"},
	{STM_TIMING_MODE_480I59940_13500, STM_OUTPUT_STD_PAL_M, V4L2_STD_PAL_M,
	 "PAL-M"},
	{STM_TIMING_MODE_480I59940_13500, STM_OUTPUT_STD_PAL_60,
	 V4L2_STD_PAL_60, "PAL-60"},
	{STM_TIMING_MODE_576I50000_13500, STM_OUTPUT_STD_PAL_BDGHI,
	 V4L2_STD_PAL, "PAL"},
	{STM_TIMING_MODE_576I50000_13500, STM_OUTPUT_STD_PAL_N, V4L2_STD_PAL_N,
	 "PAL-N"},
	{STM_TIMING_MODE_576I50000_13500, STM_OUTPUT_STD_PAL_Nc,
	 V4L2_STD_PAL_Nc, "PAL-Nc"},
	{STM_TIMING_MODE_576I50000_13500, STM_OUTPUT_STD_SECAM, V4L2_STD_SECAM,
	 "SECAM"},
	{STM_TIMING_MODE_480P60000_27027, STM_OUTPUT_STD_SMPTE293M,
	 V4L2_STD_480P_60, "480p@60"},
	{STM_TIMING_MODE_480P59940_27000, STM_OUTPUT_STD_SMPTE293M,
	 V4L2_STD_480P_59_94, "480p@59.94"},
	{STM_TIMING_MODE_576P50000_27000, STM_OUTPUT_STD_SMPTE293M,
	 V4L2_STD_576P_50, "576p"},
	{STM_TIMING_MODE_1080P60000_148500, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_60, "1080p@60"},
	{STM_TIMING_MODE_1080P59940_148352, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_59_94, "1080p@59.94"},
	{STM_TIMING_MODE_1080P50000_148500, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_50, "1080p@50"},
	{STM_TIMING_MODE_1080P30000_74250, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_30, "1080p@30"},
	{STM_TIMING_MODE_1080P29970_74176, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_29_97, "1080p@29.97"},
	{STM_TIMING_MODE_1080P25000_74250, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_25, "1080p@25"},
	{STM_TIMING_MODE_1080P24000_74250, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_24, "1080p@24"},
	{STM_TIMING_MODE_1080P23976_74176, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080P_23_98, "1080p@23.98"},
	{STM_TIMING_MODE_1080I60000_74250, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080I_60, "1080i@60"},
	{STM_TIMING_MODE_1080I59940_74176, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080I_59_94, "1080i@59.94"},
	{STM_TIMING_MODE_1080I50000_74250_274M, STM_OUTPUT_STD_SMPTE274M,
	 V4L2_STD_1080I_50, "1080i@50"},
	{STM_TIMING_MODE_720P60000_74250, STM_OUTPUT_STD_SMPTE296M,
	 V4L2_STD_720P_60, "720p@60"},
	{STM_TIMING_MODE_720P59940_74176, STM_OUTPUT_STD_SMPTE296M,
	 V4L2_STD_720P_59_94, "720p@59.94"},
	{STM_TIMING_MODE_720P50000_74250, STM_OUTPUT_STD_SMPTE296M,
	 V4L2_STD_720P_50, "720p@50"},
	{STM_TIMING_MODE_480P60000_25200, STM_OUTPUT_STD_VGA, V4L2_STD_VGA_60,
	 "VGA@60"},
	{STM_TIMING_MODE_480P59940_25180, STM_OUTPUT_STD_VGA,
	 V4L2_STD_VGA_59_94, "VGA@59.94"},
	{STM_TIMING_MODE_768P60000_65000, STM_OUTPUT_STD_XGA, V4L2_STD_XGA_60,
	 "XGA@60"},
	{STM_TIMING_MODE_768P75000_78750, STM_OUTPUT_STD_XGA, V4L2_STD_XGA_75,
	 "XGA@75"},
	{STM_TIMING_MODE_768P85000_94500, STM_OUTPUT_STD_XGA, V4L2_STD_XGA_75,
	 "XGA@85"},
	{STM_TIMING_MODE_540P59940_36343, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD3660, "QFHD3660"},
	{STM_TIMING_MODE_540P49993_36343, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD3650, "QFHD3650"},
	{STM_TIMING_MODE_540P59945_54857_WIDE, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_WQFHD5660, "WQFHD5660"},
	{STM_TIMING_MODE_540P49999_54857_WIDE, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_WQFHD5650, "WQFHD5650"},
	{STM_TIMING_MODE_540P59945_54857, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD5660, "QFHD5660"},
	{STM_TIMING_MODE_540P49999_54857, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD5650, "QFHD5650"},
	{STM_TIMING_MODE_540P29970_18000, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD1830, "QFHD1830"},
	{STM_TIMING_MODE_540P25000_18000, STM_OUTPUT_STD_NTG5,
	 V4L2_STD_QFHD1825, "QFHD1825"},
	{STM_TIMING_MODE_4K2K29970_296703, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_29_97, "4K2Kp@29.97"},
	{STM_TIMING_MODE_4K2K30000_297000, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_30, "4K2Kp@30"},
	{STM_TIMING_MODE_4K2K25000_297000, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_25, "4K2Kp@25"},
	{STM_TIMING_MODE_4K2K23980_296703, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_23_98, "4K2Kp@23_98"},
	{STM_TIMING_MODE_4K2K24000_297000, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_24, "4K2Kp@24"},
	{STM_TIMING_MODE_4K2K24000_297000_WIDE, STM_OUTPUT_STD_HDMI_LLC_EXT,
	 V4L2_STD_4K2KP_24_WIDE, "4K2Kp_WIDE@24"},
};

/******************************************************************************
 *  stmvout_get_supported_standards - ioctl helper for VIDIOC_ENUMOUTPUT
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_supported_standards(stvout_device_t * pDev, v4l2_std_id * ids)
{
	uint32_t caps;
	stm_display_output_h hOutput;

	*ids = V4L2_STD_UNKNOWN;

	/* Via Media Controller get the Display handle of connected output */
	hOutput = stmvout_get_output_display_handle(pDev);
	if (!hOutput)
		return -ENODEV;

	if (stm_display_output_get_capabilities(hOutput, &caps) < 0) {
		debug_msg("%s @ %p: Unable to get output capabilities\n",
			  __FUNCTION__, pDev);
		return signal_pending(current) ? -ERESTARTSYS : -EIO;
	}

	debug_msg("%s @ %p: Output capabilities = 0x%x\n", __FUNCTION__, pDev,
		  caps);

	if (caps & OUTPUT_CAPS_SD_ANALOG) {
		debug_msg("%s @ %p: adding SD standards\n", __FUNCTION__, pDev);
		*ids |= (V4L2_STD_525_60 | V4L2_STD_625_50);
	}

	if (caps & OUTPUT_CAPS_ED_ANALOG) {
		debug_msg("%s @ %p: adding ED standards\n", __FUNCTION__, pDev);
		*ids |= V4L2_STD_SMPTE293M;
	}

	if (caps & OUTPUT_CAPS_HD_ANALOG) {
		stm_display_mode_t dummy;

		debug_msg("%s @ %p: adding HD standards\n", __FUNCTION__, pDev);
		*ids |= (V4L2_STD_VESA |
			 V4L2_STD_SMPTE296M |
			 V4L2_STD_1080P_23_98 |
			 V4L2_STD_1080P_24 |
			 V4L2_STD_1080P_25 |
			 V4L2_STD_1080P_29_97 |
			 V4L2_STD_1080P_30 |
			 V4L2_STD_1080I_60 |
			 V4L2_STD_1080I_59_94 | V4L2_STD_1080I_50);

		/*
		 * Test to see if the high framerate 1080p modes are available by trying
		 * to get the mode descriptor for one.
		 */
		if (!stm_display_output_get_display_mode
		    (hOutput, STM_TIMING_MODE_1080P60000_148500, &dummy)) {
			debug_msg("%s @ %p: adding fast 1080p standards\n",
				  __FUNCTION__, pDev);
			*ids |=
			    (V4L2_STD_1080P_60 | V4L2_STD_1080P_59_94 |
			     V4L2_STD_1080P_50);
		} else {
			if (signal_pending(current))
				return -ERESTARTSYS;
		}
	}

	if (caps & OUTPUT_CAPS_UHD_DIGITAL) {
		stm_display_mode_t dummy;

		/*
		 * Test to see if the ultra high framerate 4K2KP modes are available by
		 * trying to get the mode descriptor for one.
		 */
		if (!stm_display_output_get_display_mode
		    (hOutput, STM_TIMING_MODE_4K2K30000_297000, &dummy)) {
			debug_msg("%s @ %p: adding UHD 4K2K progressive standards\n",
				  __FUNCTION__, pDev);
			*ids |= V4L2_STD_4K2KP;
		} else {
			if (signal_pending(current))
				return -ERESTARTSYS;
		}
	}

	return 0;
}

/******************************************************************************
 *  stmvout_set_current_standard - ioctl helper for VIDIOC_S_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_set_current_standard(stvout_device_t * pDev, v4l2_std_id stdid)
{
	int i;
	int ret;
	stm_display_output_h hOutput;
	stm_display_mode_t newModeLine = { STM_TIMING_MODE_RESERVED };
	stm_display_mode_t currentModeLine = { STM_TIMING_MODE_RESERVED };

	/* Via Media Controller get the Display handle of connected output */
	hOutput = stmvout_get_output_display_handle(pDev);
	if (!hOutput)
		return -EIO;	/* internal error */

	if ((ret = stm_display_output_get_current_display_mode(hOutput,
							       &currentModeLine))
	    < 0) {
		if (signal_pending(current))
			return -ERESTARTSYS;

		if (ret != -ENOMSG)
			return ret;
	}

	if (stdid == V4L2_STD_UNKNOWN) {
		if ((ret = stm_display_output_stop(hOutput)) < 0) {
			return signal_pending(current) ? -ERESTARTSYS : ret;
		}

		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(standard_list); i++) {
		if ((standard_list[i].v4l_standard & stdid) == stdid) {
			debug_msg
			    ("%s: found mode = %d tvStandard = 0x%x (%s) i = %d stdid = 0x%llx\n",
			     __FUNCTION__, (s32) standard_list[i].mode,
			     standard_list[i].tvStandard, standard_list[i].name,
			     i, stdid);

			if ((ret =
			     stm_display_output_get_display_mode(hOutput,
								 standard_list
								 [i].mode,
								 &newModeLine))
			    < 0) {
				debug_msg("%s: std %llu not supported\n",
					  __FUNCTION__, stdid);
				return signal_pending(current) ? -ERESTARTSYS :
				    ret;
			}

			break;
		}
	}

	if (i == ARRAY_SIZE(standard_list)) {
		debug_msg("%s: std not found stdid = 0x%llx\n", __FUNCTION__,
			  stdid);
		return -EINVAL;
	}

	/*
	 * If there is nothing to do, just return
	 */
	if ((newModeLine.mode_id == currentModeLine.mode_id) &&
	    (standard_list[i].tvStandard ==
	     currentModeLine.mode_params.output_standards))
		return 0;

	newModeLine.mode_params.output_standards = standard_list[i].tvStandard;
	newModeLine.mode_params.flags = STM_MODE_FLAGS_NONE;

	/*
	 * Try starting the display with new mode. If we cannot start the display
	 * then we try to stop it and restart it again.
	 */
	if (stm_display_output_start(hOutput, &newModeLine) < 0) {
		if ((ret = stm_display_output_stop(hOutput)) < 0)
			return signal_pending(current) ? -ERESTARTSYS : ret;

		if (stm_display_output_start(hOutput, &newModeLine) < 0) {
			err_msg("%s: cannot start standard (%d) '%s'\n", __FUNCTION__,
				i, standard_list[i].name);
			return signal_pending(current) ? -ERESTARTSYS : -EBUSY;
		}
	}

	debug_msg("%s: std started ok stdid = 0x%llx\n", __FUNCTION__, stdid);

	return 0;
}

/******************************************************************************
 *  stmvout_get_current_standard - ioctl helper for VIDIOC_G_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_current_standard(stvout_device_t * pDev, v4l2_std_id * stdid)
{
	int i;
	int ret;
	stm_display_mode_t currentMode;
	stm_display_output_h hOutput;

	/* Via Media Controller get the Display handle of connected output */
	hOutput = stmvout_get_output_display_handle(pDev);
	if (!hOutput)
		return -EIO;	/* internal error */

	if ((ret = stm_display_output_get_current_display_mode(hOutput,
							       &currentMode)) <
	    0) {
		if (signal_pending(current))
			return -ERESTARTSYS;

		if (ret != -ENOMSG)
			return ret;

		/*
		 * Display pipeline is stopped, so return V4L2_STD_UNKNOWN,
		 * there is no V4L2_STD_NONE unfortunately.
		 */
		*stdid = V4L2_STD_UNKNOWN;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(standard_list); i++) {
		if (standard_list[i].mode == currentMode.mode_id &&
		    standard_list[i].tvStandard ==
		    currentMode.mode_params.output_standards) {
			debug_msg
			    ("%s: found mode = %d tvStandard = 0x%x i = %d stdid = 0x%llx\n",
			     __FUNCTION__, (s32) currentMode.mode_id,
			     currentMode.mode_params.output_standards, i,
			     standard_list[i].v4l_standard);
			*stdid = standard_list[i].v4l_standard;
			return 0;
		}
	}

	return -EIO;
}

/******************************************************************************
 *  stmvout_get_display_standard - ioctl helper VIDIOC_ENUM_OUTPUT_STD
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_display_standard(stvout_device_t * pDev,
				 struct v4l2_standard *std)
{
	int i, found, ret;
	stm_display_mode_t ModeLine = { STM_TIMING_MODE_RESERVED };
	stm_display_output_h hOutput;

	/* Via Media Controller get the Display handle of connected output */
	hOutput = stmvout_get_output_display_handle(pDev);
	if (!hOutput)
		return -EIO;	/* internal error */

	debug_msg("%s: pDev = %p std = %p\n", __FUNCTION__, pDev, std);

	BUG_ON(pDev == NULL || std == NULL);

	debug_msg("%s: index = %d\n", __FUNCTION__, std->index);

	for (i = 0, found = -1; i < ARRAY_SIZE(standard_list); i++) {
		/*
		 * stm_display_output_get_display_mode() only returns a valid mode line
		 * if the requested mode is available on this output. So we keep trying
		 * modes until we have found the nth one to match the enumeration index
		 * in the v4l2_standard structure.
		 */
		if ((ret = stm_display_output_get_display_mode(hOutput,
							       standard_list[i].
							       mode,
							       &ModeLine)) <
		    0) {
			if (signal_pending(current))
				return -ERESTARTSYS;
		} else {
			found++;
			if (found == (s32) std->index)
				break;
		}
	}

	if (i == ARRAY_SIZE(standard_list)) {
		debug_msg("%s: end of list\n", __FUNCTION__);
		return -EINVAL;
	}

	debug_msg("%s: i = %d found = %d ModeLine ID = %d\n",
		  __FUNCTION__, i, found, ModeLine.mode_id);

	strlcpy(std->name, standard_list[i].name, sizeof(std->name));
	std->id = standard_list[i].v4l_standard;
	std->framelines = ModeLine.mode_timing.lines_per_frame;
	std->frameperiod.numerator = 1000;
	std->frameperiod.denominator =
	    ModeLine.mode_params.vertical_refresh_rate;

	if (ModeLine.mode_params.scan_type == STM_INTERLACED_SCAN)
		std->frameperiod.denominator = std->frameperiod.denominator / 2;

	return 0;
}

/******************************************************************************
 *  stmvout_get_display_size - ioctl helper for VIDIOC_CROPCAP
 *
 *  returns 0 on success or a standard error code.
 */
int stmvout_get_display_size(stvout_device_t * pDev, struct v4l2_cropcap *cap)
{
	int ret;
	stm_display_mode_t currentMode;
	int32_t numerator;
	uint32_t denominator;
	stm_display_output_h hOutput;

	/* Via Media Controller get the Display handle of connected output */
	hOutput = stmvout_get_output_display_handle(pDev);
	if (!hOutput)
		return -EIO;	/* internal error */

	if ((ret = stm_display_output_get_current_display_mode(hOutput,
							       &currentMode)) <
	    0)
		return signal_pending(current) ? -ERESTARTSYS : ret;

	/*
	 * The output bounds and default output size are the full active video area
	 * for the display mode with an origin at (0,0). The display driver will
	 * position plane horizontally and vertically on the display, based on the
	 * current display mode.
	 */
	cap->bounds.left = 0;
	cap->bounds.top = 0;
	cap->bounds.width = currentMode.mode_params.active_area_width;
	cap->bounds.height = currentMode.mode_params.active_area_height;

	cap->defrect = cap->bounds;

	/*
	 * We have a pixel aspect ratio in the mode line, but it is the ratio x/y
	 * to convert the image ratio to the picture aspect ratio, i.e. 4:3 or 16:9.
	 * This is not what the V4L2 API reports, it reports the ratio y/x (yes the
	 * other way about) to convert from the pixel sampling frequency to square
	 * pixels. But we use the mode line pixel aspect ratios to determine square
	 * pixel modes.
	 */
	numerator =
	    currentMode.mode_params.pixel_aspect_ratios[STM_AR_INDEX_4_3].
	    numerator;
	denominator =
	    currentMode.mode_params.pixel_aspect_ratios[STM_AR_INDEX_4_3].
	    denominator;

	if (denominator == 0) {
		/*
		 * This must be a 16:9 only HD or VESA mode, which means it should always
		 * have a 1:1 pixel aspect ratio.
		 */
		numerator =
		    currentMode.mode_params.
		    pixel_aspect_ratios[STM_AR_INDEX_16_9].numerator;
		denominator =
		    currentMode.mode_params.
		    pixel_aspect_ratios[STM_AR_INDEX_16_9].denominator;
	}

	if (numerator == 1 && denominator == 1) {
		cap->pixelaspect.numerator = 1;
		cap->pixelaspect.denominator = 1;
	} else {
		/*
		 * We assume that the SMPTE293M SD progressive modes have the
		 * same aspect ratio as the SD interlaced modes.
		 */
		if (currentMode.mode_timing.lines_per_frame == 525) {
			cap->pixelaspect.numerator = 11;	/* From V4L2 API Spec for NTSC(-J) */
			cap->pixelaspect.denominator = 10;
		} else if (currentMode.mode_timing.lines_per_frame == 625) {
			cap->pixelaspect.numerator = 54;	/* From V4L2 API Spec for PAL/SECAM */
			cap->pixelaspect.denominator = 59;
		} else {
			/*
			 * Err, don't know, default to 1:1
			 */
			cap->pixelaspect.numerator = 1;
			cap->pixelaspect.denominator = 1;
		}
	}

	return 0;
}
