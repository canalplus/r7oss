/*
 * STMicroelectronics BDispII driver - BDispII device features
 *
 * Copyright (c) 2013 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef __KERNEL__
#include <linux/types.h>
#else /* __KERNEL__ */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#endif /* __KERNEL__ */

#include <linux/stm/bdisp2_shared.h>
#include "bdisp2/bdispII_device_features.h"

#include "bdisp2/bdispII_aq_state.h"
#include "bdisp2/bdisp2_os.h"
#include "bdisp2/bdisp_accel.h"


/* the following are hardware features and they will be different depending
   on the BDisp implementation we're running on. */
static const struct _stm_bdisp2_hw_features _dev_features[] = {
	/* other versions are not supported! */
	[STM_BLITTER_VERSION_7109c3] = {
		.flicker_filter = 1,
	},
	[STM_BLITTER_VERSION_7200c1] = {
		.rle_bd = 1,

		.flicker_filter = 1,
	},
	[STM_BLITTER_VERSION_7200c2_7111_7141_7105] = {
		.nmdk_macroblock = 1,
		.planar_r = 1,
		.planar2_w = 1,
		.rle_bd = 1,
		.rle_hd = 1,

		.flicker_filter = 1,
		.rotation = 1,
	},
	[STM_BLITTER_VERSION_5206] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.rotation = 1,
	},
	[STM_BLITTER_VERSION_7106_7108] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.rle_bd = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.rotation = 1,
	},
	[STM_BLITTER_VERSION_FLI75x0] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.rotation = 1,
	},
	[STM_BLITTER_VERSION_STiH415_FLI7610] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.rle_bd = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.rotation = 1,
	},
	[STM_BLITTER_VERSION_STiH205] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.rgb32 = 1,
		.rle_bd = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.rotation = 1,

		.no_address_banks = 1,
		.size_4k = 1,
		.aq_lock = 1,
	},
	[STM_BLITTER_VERSION_STiH416] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.rgb32 = 1,
		.rle_bd = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.dst_colorkey = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.owf = 1,
		.porterduff = 1,
		.rotation = 1,
		.spatial_dei = 0, /* FIXME: available in hardware, but
				     untested, therefore disabled! */

		.no_address_banks = 1,
		.size_4k = 1,
		.aq_lock = 1,
		.write_posting = 1,
	},
	[STM_BLITTER_VERSION_STiH407] = {
		.planar_r = 1,
		.planar2_w = 1,
		.planar3_w = 1,
		.rgb32 = 1,
		.rle_bd = 1,
		.s1_422r = 1,
		.s1_subbyte = 1,

		.boundary_bypass = 1,
		.dst_colorkey = 1,
		.flicker_filter = 1,
		.gradient = 1,
		.owf = 1,
		.porterduff = 1,
		.rotation = 1,
		.spatial_dei = 0, /* FIXME: available in hardware, but
				     untested, therefore disabled! */

		.no_address_banks = 1,
		.size_4k = 1,
		.aq_lock = 1,
		.write_posting = 1,
		.need_itm = 1,
		.upsampled_nbpix_min = 4,
	},
};


static void
bdisp2_init_features(struct bdisp2_features    *features,
		     struct stm_plat_blit_data *plat_blit)
{
	int idx;

	memcpy(features->stm_blit_to_bdisp, stm_blit_to_bdisp_template,
	       sizeof(features->stm_blit_to_bdisp));

	features->hw = _dev_features[plat_blit->device_type];

	/* Update buffer length info, either from DT or static platform data */
	features->hw.mb_buffer_length = plat_blit->mb_buffer_length;
	features->hw.line_buffer_length = plat_blit->line_buffer_length;
	features->hw.rotate_buffer_length = plat_blit->rotate_buffer_length;

	if (features->hw.size_4k)
		features->hw.size_mask = 0x1fff;
	else
		features->hw.size_mask = 0x0fff;

	for (idx = STM_BLITTER_SF_INVALID; idx < STM_BLITTER_SF_COUNT; ++idx) {
		struct bdisp2_pixelformat_info * const info =
			&features->stm_blit_to_bdisp[STM_BLITTER_SF_MASK(idx)];
		stm_blitter_surface_format_t fmt = info->stm_blitter_format;

		if (fmt & STM_BLITTER_SF_PLANAR) {
			if (!features->hw.planar_r)
				info->supported_as_src = false;
		}

		switch (fmt) {
		case STM_BLITTER_SF_INVALID:
		case STM_BLITTER_SF_COUNT:
		default:
			/* should not be reached */
			break;

		case STM_BLITTER_SF_RGB565:
		case STM_BLITTER_SF_RGB24:
		case STM_BLITTER_SF_BGR24:
		  break;

		case STM_BLITTER_SF_RGB32:
			if (features->hw.rgb32)
				info->bdisp_type = BLIT_COLOR_FORM_RGB888_32;
			break;

		case STM_BLITTER_SF_ARGB1555:
		case STM_BLITTER_SF_ARGB4444:
		case STM_BLITTER_SF_ARGB8565:
		case STM_BLITTER_SF_AlRGB8565:
		case STM_BLITTER_SF_ARGB:
		case STM_BLITTER_SF_AlRGB:
		case STM_BLITTER_SF_BGRA:
		case STM_BLITTER_SF_BGRAl:
		case STM_BLITTER_SF_ABGR:
		case STM_BLITTER_SF_LUT8:
		case STM_BLITTER_SF_LUT4:
		case STM_BLITTER_SF_LUT2:
		case STM_BLITTER_SF_LUT1:
		case STM_BLITTER_SF_ALUT88:
		case STM_BLITTER_SF_AlLUT88:
		case STM_BLITTER_SF_ALUT44:
		case STM_BLITTER_SF_A8:
		case STM_BLITTER_SF_Al8:
		case STM_BLITTER_SF_A1:

		/* YCbCr formats */
		case STM_BLITTER_SF_AVYU:
		case STM_BLITTER_SF_AlVYU:
		case STM_BLITTER_SF_VYU:
		case STM_BLITTER_SF_YUY2:
		case STM_BLITTER_SF_UYVY:
			break;

		case STM_BLITTER_SF_YV12:
		case STM_BLITTER_SF_I420:
		case STM_BLITTER_SF_YV16:
		case STM_BLITTER_SF_YV61:
		case STM_BLITTER_SF_YCBCR444P:
			if (!features->hw.planar3_w)
				info->supported_as_dst = false;
			break;

		case STM_BLITTER_SF_NV12:
		case STM_BLITTER_SF_NV21:
		case STM_BLITTER_SF_NV16:
		case STM_BLITTER_SF_NV61:
		case STM_BLITTER_SF_YCBCR420MB:
		case STM_BLITTER_SF_YCBCR422MB:
		case STM_BLITTER_SF_NV24:
			if (!features->hw.planar2_w)
				info->supported_as_dst = false;
			break;

		/* RLD formats */
		case STM_BLITTER_SF_RLD_BD:
			if (features->hw.rle_bd)
				info->supported_as_src = true;
			break;

		case STM_BLITTER_SF_RLD_H2:
		case STM_BLITTER_SF_RLD_H8:
			if (features->hw.rle_hd)
				info->supported_as_src = true;
			break;
		}
	}

	features->drawflags = STM_BLITTER_SDF_ALL; /* FIXME */
	features->blitflags = STM_BLITTER_SBF_ALL; /* FIXME */

	features->drawflags = (0
			       | STM_BLITTER_SDF_XOR
			       | STM_BLITTER_SDF_BLEND
			       | STM_BLITTER_SDF_SRC_PREMULTIPLY
			       | STM_BLITTER_SDF_ANTIALIAS
			      );

	features->blitflags = (0
			       | STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT
			       | STM_BLITTER_SBF_ALL_IN_FIXED_POINT
			       | STM_BLITTER_SBF_ANTIALIAS
			       | STM_BLITTER_SBF_SRC_COLORIZE
			       | STM_BLITTER_SBF_SRC_COLORKEY
			       | STM_BLITTER_SBF_XOR
			       | STM_BLITTER_SBF_BLEND_ALPHACHANNEL
			       | STM_BLITTER_SBF_BLEND_COLORALPHA
			       | STM_BLITTER_SBF_SRC_PREMULTCOLOR
			       | STM_BLITTER_SBF_SRC_PREMULTIPLY
			       | STM_BLITTER_SBF_DST_PREMULTIPLY
			       | STM_BLITTER_SBF_READ_SOURCE2
			       | STM_BLITTER_SBF_FLIP_HORIZONTAL
			       | STM_BLITTER_SBF_FLIP_VERTICAL
			       | STM_BLITTER_SBF_NO_FILTER
			       | STM_BLITTER_SBF_DEINTERLACE_TOP
			       | STM_BLITTER_SBF_DEINTERLACE_BOTTOM
			       | STM_BLITTER_SBF_FLICKER_FILTER
			       | STM_BLITTER_SBF_STRICT_INPUT_RECT
			      );

	if (features->hw.dst_colorkey) {
		features->drawflags |= STM_BLITTER_SDF_DST_COLORKEY;
//		features->blitflags |= STM_BLITTER_SBF_DST_COLORKEY;
	}

	if (features->hw.flicker_filter) {
		/* FIXME: once we have an API (draw and blit flag) for this,
		   we have to add the support for these here. */
		features->drawflags |= 0;
		features->blitflags |= 0;
	}

	if (features->hw.gradient)
		features->drawflags |= STM_BLITTER_SDF_GRADIENT;

	if (features->hw.porterduff)
		features->drawflags |= STM_BLITTER_SDF_DST_PREMULTIPLY;

	if (features->hw.rotation) {
		if (features->hw.rotate_buffer_length)
			features->blitflags |= (STM_BLITTER_SBF_ROTATE90
						| STM_BLITTER_SBF_ROTATE270);
		else
			stm_blit_printw(
				"BDisp2: no rotate buffer specified but rotation supported!\n\n");
	} else if (features->hw.rotate_buffer_length) {
		stm_blit_printw(
			"BDisp2: rotate buffer specified but rotation not supported!\n\n");
	}

#if 0
	if (STM_BLITTER_SDF_ALL & ~features->drawflags)
		stm_blit_printe(
			"BDisp2: not all draw flags supported by hardware: %.8x\n",
			STM_BLITTER_SDF_ALL & ~features->drawflags);
	if (STM_BLITTER_SBF_ALL & ~features->blitflags)
		stm_blit_printe(
			"BDisp2: not all blitflags supported by hardware: %.8x\n",
			STM_BLITTER_SBF_ALL & ~features->blitflags);
#endif
}

int
bdisp2_init_features_by_type(struct bdisp2_features    *features,
			     struct stm_plat_blit_data *plat_blit)
{
	const char *name;

	if (plat_blit->device_type < 0
	    || plat_blit->device_type >= STM_BLITTER_N_ELEMENTS(_dev_features))
		return -EINVAL;

	bdisp2_init_features(features, plat_blit);

	if (!features->hw.line_buffer_length)
		return -EINVAL;

	switch (plat_blit->device_type) {
	case STM_BLITTER_VERSION_7109c3:
		name = "STb7109c3";
		break;
	case STM_BLITTER_VERSION_7200c1:
		name = "STi7200c1";
		break;
	case STM_BLITTER_VERSION_7200c2_7111_7141_7105:
		name = "STi7200c2/7111/7141/7105";
		break;
	case STM_BLITTER_VERSION_5206:
		name = "STi5206";
		break;
	case STM_BLITTER_VERSION_7106_7108:
		name = "STi7106/7108";
		break;
	case STM_BLITTER_VERSION_FLI75x0:
		name = "FLi75x0";
		break;
	case STM_BLITTER_VERSION_STiH415_FLI7610:
		name = "STiH415/FLi7610";
		break;
	case STM_BLITTER_VERSION_STiH205:
		name = "STiH205/STiH207";
		break;
	case STM_BLITTER_VERSION_STiH416:
		name = "STiH416/STiH315";
		break;
	case STM_BLITTER_VERSION_STiH407:
		name = "STiH407";
		break;
	default:
		name = "unknown";
		break;
	}

	stm_blit_printi("BDisp/BLT: blitter is a '%s' (%d)\n",
			name, plat_blit->device_type);

	return 0;
}

#define MEMORY64MB_SIZE           (1<<26)
#define _ALIGN_DOWN(addr, size)   ((addr)&(~((size)-1)))

int
bdisp2_check_memory_constraints(const struct stm_bdisp2_device_data * const stdev,
				unsigned long                        start,
				unsigned long                        end)
{
	if (stdev->features.hw.no_address_banks)
		return 0;

	return (_ALIGN_DOWN (start, MEMORY64MB_SIZE)
		!= _ALIGN_DOWN (end, MEMORY64MB_SIZE)) ? -EINVAL : 0;
}
