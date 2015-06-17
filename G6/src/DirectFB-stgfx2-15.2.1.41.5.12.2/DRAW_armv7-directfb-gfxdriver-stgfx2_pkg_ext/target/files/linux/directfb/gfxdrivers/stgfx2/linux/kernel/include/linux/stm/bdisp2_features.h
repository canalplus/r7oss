#ifndef __BDISPII_AQ_FEATURES_H__
#define __BDISPII_AQ_FEATURES_H__

#include <linux/types.h>


struct bdisp2_pixelformat_info {
	enum stm_blitter_surface_format_e stm_blitter_format;
	__u32 bdisp_type; /* BLIT_COLOR_FORM_RGB565 etc. */
	unsigned int supported_as_src:1;
	unsigned int supported_as_dst:1;
};

extern const struct bdisp2_pixelformat_info stm_blit_to_bdisp_template[];

#endif /* __BDISPII_AQ_FEATURES_H__ */
