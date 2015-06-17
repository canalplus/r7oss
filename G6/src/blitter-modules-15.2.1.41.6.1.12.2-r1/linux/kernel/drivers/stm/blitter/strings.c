#include "blitter_os.h"
#include "linux/stm/blitter.h"
#include "state.h"

#include "strings.h"

#ifdef __KERNEL__
#else /* __KERNEL__ */
#include <stdio.h>
#endif /* __KERNEL__ */

const char *
stm_blitter_get_surface_format_name(stm_blitter_surface_format_t fmt)
{
#define CMP(val) case val: return #val
	switch (fmt) {
	CMP(STM_BLITTER_SF_RGB565);
	CMP(STM_BLITTER_SF_RGB24);
	CMP(STM_BLITTER_SF_BGR24);
	CMP(STM_BLITTER_SF_RGB32);
	CMP(STM_BLITTER_SF_ARGB1555);
	CMP(STM_BLITTER_SF_ARGB4444);
	CMP(STM_BLITTER_SF_ARGB8565);
	CMP(STM_BLITTER_SF_AlRGB8565);
	CMP(STM_BLITTER_SF_ARGB);
	CMP(STM_BLITTER_SF_AlRGB);
	CMP(STM_BLITTER_SF_BGRA);
	CMP(STM_BLITTER_SF_BGRAl);
	CMP(STM_BLITTER_SF_ABGR);
	CMP(STM_BLITTER_SF_LUT8);
	CMP(STM_BLITTER_SF_LUT4);
	CMP(STM_BLITTER_SF_LUT2);
	CMP(STM_BLITTER_SF_LUT1);
	CMP(STM_BLITTER_SF_ALUT88);
	CMP(STM_BLITTER_SF_AlLUT88);
	CMP(STM_BLITTER_SF_ALUT44);
	CMP(STM_BLITTER_SF_A8);
	CMP(STM_BLITTER_SF_Al8);
	CMP(STM_BLITTER_SF_A1);
	CMP(STM_BLITTER_SF_AVYU);
	CMP(STM_BLITTER_SF_AlVYU);
	CMP(STM_BLITTER_SF_VYU);
	CMP(STM_BLITTER_SF_YUY2);
	CMP(STM_BLITTER_SF_UYVY);
	CMP(STM_BLITTER_SF_YV12);
	CMP(STM_BLITTER_SF_I420);
	CMP(STM_BLITTER_SF_YV16);
	CMP(STM_BLITTER_SF_YV61);
	CMP(STM_BLITTER_SF_YCBCR444P);
	CMP(STM_BLITTER_SF_NV12);
	CMP(STM_BLITTER_SF_NV21);
	CMP(STM_BLITTER_SF_NV16);
	CMP(STM_BLITTER_SF_NV61);
	CMP(STM_BLITTER_SF_YCBCR420MB);
	CMP(STM_BLITTER_SF_YCBCR422MB);
	CMP(STM_BLITTER_SF_RLD_BD);
	CMP(STM_BLITTER_SF_RLD_H2);
	CMP(STM_BLITTER_SF_RLD_H8);
	CMP(STM_BLITTER_SF_NV24);

	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		break;
	}

	return "unknown";
#undef CMP
}

const char *
stm_blitter_get_porter_duff_name(stm_blitter_porter_duff_rule_t pd)
{
#define CMP(val) case val: return #val
	switch (pd) {
	CMP(STM_BLITTER_PD_CLEAR);
	CMP(STM_BLITTER_PD_SOURCE);
	CMP(STM_BLITTER_PD_DEST);
	CMP(STM_BLITTER_PD_SOURCE_OVER);
	CMP(STM_BLITTER_PD_DEST_OVER);
	CMP(STM_BLITTER_PD_SOURCE_IN);
	CMP(STM_BLITTER_PD_DEST_IN);
	CMP(STM_BLITTER_PD_SOURCE_OUT);
	CMP(STM_BLITTER_PD_DEST_OUT);
	CMP(STM_BLITTER_PD_SOURCE_ATOP);
	CMP(STM_BLITTER_PD_DEST_ATOP);
	CMP(STM_BLITTER_PD_XOR);
	CMP(STM_BLITTER_PD_NONE);

	default:
		break;
	}

	return "unknown";
#undef CMP
}

char *
stm_blitter_get_accel_string(enum stm_blitter_accel accel)
{
	char *str = stm_blitter_allocate_mem(220);
	int pos = 0;

	pos += sprintf(str + pos, "%.8x ->", accel);
	if (accel == 0)
		pos += sprintf(str + pos, " NONE");
	else {
		if (accel & STM_BLITTER_ACCEL_FILLRECT)
			pos += sprintf(str + pos, " FILLRECTANGLE");
		if (accel & STM_BLITTER_ACCEL_DRAWRECT)
			pos += sprintf(str + pos, " DRAWRECTANGLE");
		if (accel & STM_BLITTER_ACCEL_BLIT)
			pos += sprintf(str + pos, " BLIT");
		if (accel & STM_BLITTER_ACCEL_STRETCHBLIT)
			pos += sprintf(str + pos, " STRETCHBLIT");
		if (accel & STM_BLITTER_ACCEL_BLIT2)
			pos += sprintf(str + pos, " BLIT2");
	}

	return str;
}

char *
stm_blitter_get_modified_string(enum stm_blitter_state_modification_flags mod)
{
	char *str = stm_blitter_allocate_mem(220);
	int pos = 0;

	pos += sprintf(str + pos, "%.8x ->", mod);
	if (!mod)
		pos += sprintf(str + pos, " none");
	else if (mod == STM_BLITTER_SMF_ALL)
		pos += sprintf(str + pos, " ALL");
	else {
		if (mod & STM_BLITTER_SMF_CLIP)
			pos += sprintf(str + pos, " CLIP");
		if (mod & STM_BLITTER_SMF_DRAWFLAGS)
			pos += sprintf(str + pos, " DRAWFLAGS");
		if (mod & STM_BLITTER_SMF_BLITFLAGS)
			pos += sprintf(str + pos, " BLITFLAGS");
		if (mod & STM_BLITTER_SMF_PORTER_DUFF)
			pos += sprintf(str + pos, " PORTER_DUFF");
		if ((mod & STM_BLITTER_SMF_DST) == STM_BLITTER_SMF_DST)
			pos += sprintf(str + pos, " DST");
		else if (mod & STM_BLITTER_SMF_DST_ADDRESS)
			pos += sprintf(str + pos, " DST_ADDRESS");
		if (mod & STM_BLITTER_SMF_DST_COLORKEY)
			pos += sprintf(str + pos, " DST_COLORKEY");
		if (mod & STM_BLITTER_SMF_COLOR)
			pos += sprintf(str + pos, " COLOR");
		if (mod & STM_BLITTER_SMF_SRC)
			pos += sprintf(str + pos, " SRC");
		if (mod & STM_BLITTER_SMF_SRC_COLORKEY)
			pos += sprintf(str + pos, " SRC_COLORKEY");
		if (mod & STM_BLITTER_SMF_SRC2)
			pos += sprintf(str + pos, " SRC2");

		if (mod & STM_BLITTER_SMF_FILTERING)
			pos += sprintf(str + pos, " FILTERING");
		if (mod & STM_BLITTER_SMF_MATRIX)
			pos += sprintf(str + pos, " MATRIX");
		if (mod & STM_BLITTER_SMF_COLOR_MATRIX)
			pos += sprintf(str + pos, " COLOR_MATRIX");
	}

	return str;
}

char *
stm_blitter_get_drawflags_string(stm_blitter_surface_drawflags_t flags)
{
	char *str = stm_blitter_allocate_mem(220);
	int pos = 0;

	pos += sprintf(str + pos, "%.8x ->", flags);
	if (flags == STM_BLITTER_SDF_NONE)
		pos += sprintf(str + pos, " NONE");
	else {
		if (flags & STM_BLITTER_SDF_DST_COLORKEY)
			pos += sprintf(str + pos, " DST_COLORKEY");
		if (flags & STM_BLITTER_SDF_XOR)
			pos += sprintf(str + pos, " XOR");
		if (flags & STM_BLITTER_SDF_GRADIENT)
			pos += sprintf(str + pos, " GRADIENT");
		if (flags & STM_BLITTER_SDF_BLEND)
			pos += sprintf(str + pos, " BLEND");
		if (flags & STM_BLITTER_SDF_SRC_PREMULTIPLY)
			pos += sprintf(str + pos, " SRC_PREMULTIPLY");
		if (flags & STM_BLITTER_SDF_DST_PREMULTIPLY)
			pos += sprintf(str + pos, " DST_PREMULTIPLY");
		if (flags & STM_BLITTER_SDF_ANTIALIAS)
			pos += sprintf(str + pos, " ANTIALIAS");
	}

	return str;
}

char *
stm_blitter_get_blitflags_string(stm_blitter_surface_blitflags_t flags)
{
	char *str = stm_blitter_allocate_mem(220);
	int pos = 0;

	pos += sprintf(str + pos, "%.8x ->", flags);
	if (flags == STM_BLITTER_SBF_NONE)
		pos += sprintf(str + pos, " NONE");
	else {
		if ((flags & STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
		    == STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
			pos += sprintf(str + pos, " ALL_IN_FIXED_POINT");
		else if (flags & STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT)
			pos += sprintf(str + pos, " SRC_XY_IN_FIXED_POINT");
		if (flags & STM_BLITTER_SBF_ANTIALIAS)
			pos += sprintf(str + pos, " ANTIALIAS");
		if (flags & STM_BLITTER_SBF_SRC_COLORIZE)
			pos += sprintf(str + pos, " SRC_COLORIZE");
		if (flags & STM_BLITTER_SBF_SRC_COLORKEY)
			pos += sprintf(str + pos, " SRC_COLORKEY");
		if (flags & STM_BLITTER_SBF_DST_COLORKEY)
			pos += sprintf(str + pos, " DST_COLORKEY");
		if (flags & STM_BLITTER_SBF_COLORMASK)
			pos += sprintf(str + pos, " COLORMASK");
		if (flags & STM_BLITTER_SBF_XOR)
			pos += sprintf(str + pos, " XOR");
		if (flags & STM_BLITTER_SBF_BLEND_ALPHACHANNEL)
			pos += sprintf(str + pos, " BLEND_ALPHACHANNEL");
		if (flags & STM_BLITTER_SBF_BLEND_COLORALPHA)
			pos += sprintf(str + pos, " BLEND_COLORALPHA");
		if (flags & STM_BLITTER_SBF_SRC_PREMULTCOLOR)
			pos += sprintf(str + pos, " SRC_PREMULTCOLOR");
		if (flags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
			pos += sprintf(str + pos, " SRC_PREMULTIPLY");
		if (flags & STM_BLITTER_SBF_DST_PREMULTIPLY)
			pos += sprintf(str + pos, " DST_PREMULTIPLY");
		if (flags & STM_BLITTER_SBF_READ_SOURCE2)
			pos += sprintf(str + pos, " READ_SOURCE2");
		if (flags & (STM_BLITTER_SBF_FLIP_HORIZONTAL
			     | STM_BLITTER_SBF_FLIP_VERTICAL
			     | STM_BLITTER_SBF_ROTATE90
			     | STM_BLITTER_SBF_ROTATE180
			     | STM_BLITTER_SBF_ROTATE270))
			pos += sprintf(str + pos, " FLIP/ROTATE (0x%x)",
				       flags & (STM_BLITTER_SBF_FLIP_HORIZONTAL
						| STM_BLITTER_SBF_FLIP_VERTICAL
						| STM_BLITTER_SBF_ROTATE90
						| STM_BLITTER_SBF_ROTATE180
						| STM_BLITTER_SBF_ROTATE270));
		if (flags & STM_BLITTER_SBF_VC1RANGE_LUMA)
			pos += sprintf(str + pos, " VC1RANGE_LUMA");
		if (flags & STM_BLITTER_SBF_VC1RANGE_CHROMA)
			pos += sprintf(str + pos, " VC1RANGE_CHROMA");
		if (flags & STM_BLITTER_SBF_NO_FILTER)
			pos += sprintf(str + pos, " NO_FILTER");
		if (flags & STM_BLITTER_SBF_DEINTERLACE_TOP)
			pos += sprintf(str + pos, " DEINTERLACE_TOP");
		if (flags & STM_BLITTER_SBF_DEINTERLACE_BOTTOM)
			pos += sprintf(str + pos, " DEINTERLACE_BOTTOM");
		if (flags & STM_BLITTER_SBF_FLICKER_FILTER)
			pos += sprintf(str + pos, " FLICKER_FILTER");
		if (flags & STM_BLITTER_SBF_STRICT_INPUT_RECT)
			pos += sprintf(str + pos, " STRICT_INPUT_RECT");

	}

	return str;
}
