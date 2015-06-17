#ifndef __STATE_H__
#define __STATE_H__

#include <linux/stm/blitter.h>


enum stm_blitter_accel {
	STM_BLITTER_ACCEL_FILLRECT    = 0x00000001,
	STM_BLITTER_ACCEL_DRAWRECT    = 0x00000002,

	STM_BLITTER_ACCEL_BLIT        = 0x00000100,
	STM_BLITTER_ACCEL_STRETCHBLIT = 0x00000200,
	STM_BLITTER_ACCEL_BLIT2       = 0x00000400,
};
#define STM_BLITTER_ACCEL_IS_DRAW(accel) ((accel) & 0x000000ff)
#define STM_BLITTER_ACCEL_IS_BLIT(accel) ((accel) & 0x0000ff00)


enum stm_blitter_state_modification_flags {
	STM_BLITTER_SMF_NONE         = 0x00000000,

	STM_BLITTER_SMF_CLIP         = 0x00000001,

	STM_BLITTER_SMF_DRAWFLAGS    = 0x00000002,
	STM_BLITTER_SMF_BLITFLAGS    = 0x00000004,
	STM_BLITTER_SMF_PORTER_DUFF  = 0x00000008,

	STM_BLITTER_SMF_DST_ADDRESS  = 0x00000010,
	STM_BLITTER_SMF_DST          = (0x00000020
					| STM_BLITTER_SMF_DST_ADDRESS),
	STM_BLITTER_SMF_DST_COLORKEY = 0x00000040,

	STM_BLITTER_SMF_COLOR        = 0x00000080,

	STM_BLITTER_SMF_SRC          = 0x00000100,
	STM_BLITTER_SMF_SRC_COLORKEY = 0x00000200,

	STM_BLITTER_SMF_SRC2         = 0x00000400,

	STM_BLITTER_SMF_FILTERING    = 0x00000800,
	STM_BLITTER_SMF_MATRIX       = 0x00001000,

	STM_BLITTER_SMF_COLOR_MATRIX = 0x00002000,

	STM_BLITTER_SMF_ALL          = 0x00003fff,
};

struct stm_blitter_state {
	struct stm_blitter_device *blitter;

	stm_blitter_surface_t *dst;
	stm_blitter_surface_t *src;
	stm_blitter_surface_t *src2;

	enum stm_blitter_state_modification_flags modified;
	enum stm_blitter_accel                    set;

	stm_blitter_clip_mode_t clip_mode;
	stm_blitter_region_t  clip;

	stm_blitter_surface_blitflags_t blitflags;
	stm_blitter_surface_drawflags_t drawflags;

	stm_blitter_color_t colors[2]; /* for colour stop / gradient */
	uint8_t             color_index; /* for simple fill of indexed surf */
	stm_blitter_color_colorspace_t color_colorspace;

	stm_blitter_color_t       dst_ckey[2]; /* low and high */
	stm_blitter_colorkey_mode_t dst_ckey_mode;
	stm_blitter_color_t       src_ckey[2]; /*!< low and high */
	stm_blitter_colorkey_mode_t src_ckey_mode;

	stm_blitter_flicker_filter_mode_t ff_mode;

	stm_blitter_porter_duff_rule_t pd;

	stm_blitter_color_matrix_t color_matrix; /*!< for colour
							   modulation */

	unsigned int fence:1;
};


#ifdef __KERNEL__
void
stm_blitter_surface_update_src_on_dst(stm_blitter_surface_t *dst,
				      stm_blitter_surface_t *src);

void
stm_blitter_surface_update_src2_on_dst(stm_blitter_surface_t *dst,
				       stm_blitter_surface_t *src2);

void
stm_blitter_surface_update_src_colorkey_on_dst(stm_blitter_surface_t     *dst,
					     const stm_blitter_surface_t *src);
#endif /* __KERNEL__ */


#endif /* __STATE_H__ */
