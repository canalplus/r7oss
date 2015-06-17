#ifdef __KERNEL__
#else /* __KERNEL__ */
#include <stdint.h>
#include <stdbool.h>
#endif /* __KERNEL__ */

#include <linux/types.h>
#include <linux/stm/blitter.h>
#include <linux/stm/bdisp2_features.h>
#include <linux/stm/bdisp2_registers.h>

const struct bdisp2_pixelformat_info
stm_blit_to_bdisp_template[STM_BLITTER_SF_COUNT] = {
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RGB565)] = {
		.stm_blitter_format = STM_BLITTER_SF_RGB565,
		.bdisp_type = BLIT_COLOR_FORM_RGB565,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RGB24)] = {
		.stm_blitter_format = STM_BLITTER_SF_RGB24,
		.bdisp_type = BLIT_COLOR_FORM_RGB888,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_BGR24)] = {
		.stm_blitter_format = STM_BLITTER_SF_BGR24,
		.bdisp_type = (BLIT_COLOR_FORM_RGB888
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RGB32)] = {
		.stm_blitter_format = STM_BLITTER_SF_RGB32,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8888
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ARGB1555)] = {
		.stm_blitter_format = STM_BLITTER_SF_ARGB1555,
		.bdisp_type = BLIT_COLOR_FORM_ARGB1555,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ARGB4444)] = {
		.stm_blitter_format = STM_BLITTER_SF_ARGB4444,
		.bdisp_type = BLIT_COLOR_FORM_ARGB4444,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ARGB8565)] = {
		.stm_blitter_format = STM_BLITTER_SF_ARGB8565,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8565
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_AlRGB8565)] = {
		.stm_blitter_format = STM_BLITTER_SF_AlRGB8565,
		.bdisp_type = BLIT_COLOR_FORM_ARGB8565,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ARGB)] = {
		.stm_blitter_format = STM_BLITTER_SF_ARGB,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8888
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_AlRGB)] = {
		.stm_blitter_format = STM_BLITTER_SF_AlRGB,
		.bdisp_type = BLIT_COLOR_FORM_ARGB8888,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_BGRA)] = {
		.stm_blitter_format = STM_BLITTER_SF_BGRA,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8888
			       | BLIT_TY_FULL_ALPHA_RANGE
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_BGRAl)] = {
		.stm_blitter_format = STM_BLITTER_SF_BGRAl,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8888
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ABGR)] = {
		.stm_blitter_format = STM_BLITTER_SF_ABGR,
		.bdisp_type = (BLIT_COLOR_FORM_ARGB8888
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = false,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_LUT8)] = {
		.stm_blitter_format = STM_BLITTER_SF_LUT8,
		.bdisp_type = BLIT_COLOR_FORM_CLUT8,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_LUT4)] = {
		.stm_blitter_format = STM_BLITTER_SF_LUT4,
		.bdisp_type = BLIT_COLOR_FORM_CLUT4,
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_LUT2)] = {
		.stm_blitter_format = STM_BLITTER_SF_LUT2,
		.bdisp_type = BLIT_COLOR_FORM_CLUT2,
		.supported_as_src = true,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_LUT1)] = {
		.stm_blitter_format = STM_BLITTER_SF_LUT1,
		.bdisp_type = BLIT_COLOR_FORM_CLUT1,
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ALUT88)] = {
		.stm_blitter_format = STM_BLITTER_SF_ALUT88,
		.bdisp_type = (BLIT_COLOR_FORM_ACLUT88
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_AlLUT88)] = {
		.stm_blitter_format = STM_BLITTER_SF_AlLUT88,
		.bdisp_type = BLIT_COLOR_FORM_ACLUT88,
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_ALUT44)] = {
		.stm_blitter_format = STM_BLITTER_SF_ALUT44,
		.bdisp_type = BLIT_COLOR_FORM_ACLUT44,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_A8)] = {
		.stm_blitter_format = STM_BLITTER_SF_A8,
		.bdisp_type = (BLIT_COLOR_FORM_A8
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_Al8)] = {
		.stm_blitter_format = STM_BLITTER_SF_Al8,
		.bdisp_type = BLIT_COLOR_FORM_A8,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_A1)] = {
		.stm_blitter_format = STM_BLITTER_SF_A1,
		.bdisp_type = BLIT_COLOR_FORM_A1,
		.supported_as_src = true,
		.supported_as_dst = false },
	/* YCbCr formats */
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_AVYU)] = {
		.stm_blitter_format = STM_BLITTER_SF_AVYU,
		.bdisp_type = (BLIT_COLOR_FORM_AYCBCR8888
			       | BLIT_TY_FULL_ALPHA_RANGE),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_AlVYU)] = {
		.stm_blitter_format = STM_BLITTER_SF_AlVYU,
		.bdisp_type = BLIT_COLOR_FORM_AYCBCR8888,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_VYU)] = {
		.stm_blitter_format = STM_BLITTER_SF_VYU,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR888,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YUY2)] = {
		.stm_blitter_format = STM_BLITTER_SF_YUY2,
		.bdisp_type = (BLIT_COLOR_FORM_YCBCR422R
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_UYVY)] = {
		.stm_blitter_format = STM_BLITTER_SF_UYVY,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR422R,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YV12)] = {
		.stm_blitter_format = STM_BLITTER_SF_YV12,
		.bdisp_type = BLIT_COLOR_FORM_YUV444P,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_I420)] = {
		.stm_blitter_format = STM_BLITTER_SF_I420,
		.bdisp_type = BLIT_COLOR_FORM_YUV444P,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YV16)] = {
		.stm_blitter_format = STM_BLITTER_SF_YV16,
		.bdisp_type = BLIT_COLOR_FORM_YUV444P,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YV61)] = {
		.stm_blitter_format = STM_BLITTER_SF_YV61,
		.bdisp_type = BLIT_COLOR_FORM_YUV444P,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YCBCR444P)] = {
		.stm_blitter_format = STM_BLITTER_SF_YCBCR444P,
		.bdisp_type = BLIT_COLOR_FORM_YUV444P,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_NV12)] = {
		.stm_blitter_format = STM_BLITTER_SF_NV12,
		.bdisp_type = (BLIT_COLOR_FORM_YCBCR42XR2B
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_NV21)] = {
		.stm_blitter_format = STM_BLITTER_SF_NV21,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR42XR2B,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_NV16)] = {
		.stm_blitter_format = STM_BLITTER_SF_NV16,
		.bdisp_type = (BLIT_COLOR_FORM_YCBCR42XR2B
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_NV61)] = {
		.stm_blitter_format = STM_BLITTER_SF_NV61,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR42XR2B,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YCBCR420MB)] = {
		.stm_blitter_format = STM_BLITTER_SF_YCBCR420MB,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR42XMB,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_YCBCR422MB)] = {
		.stm_blitter_format = STM_BLITTER_SF_YCBCR422MB,
		.bdisp_type = BLIT_COLOR_FORM_YCBCR42XMB,
		.supported_as_src = true,
		.supported_as_dst = true },
	[STM_BLITTER_SF_MASK (STM_BLITTER_SF_NV24)] = {
		.stm_blitter_format = STM_BLITTER_SF_NV24,
		.bdisp_type = (BLIT_COLOR_FORM_YCBCR42XR2B
			       | BLIT_TY_BIG_ENDIAN),
		.supported_as_src = true,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RLD_BD)] = {
		.stm_blitter_format = STM_BLITTER_SF_RLD_BD,
		.bdisp_type = BLIT_COLOR_FORM_RLD_BD,
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RLD_H2)] = {
		.stm_blitter_format = STM_BLITTER_SF_RLD_H2,
		.bdisp_type = BLIT_COLOR_FORM_RLD_H2,
		.supported_as_src = false,
		.supported_as_dst = false },
	[STM_BLITTER_SF_MASK(STM_BLITTER_SF_RLD_H8)] = {
		.stm_blitter_format = STM_BLITTER_SF_RLD_H8,
		.bdisp_type = BLIT_COLOR_FORM_RLD_H8,
		.supported_as_src = false,
		.supported_as_dst = false },
};
