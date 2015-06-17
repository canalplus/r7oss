#ifndef __BDISPII_AQ_OPERATIONS_H__
#define __BDISPII_AQ_OPERATIONS_H__


#include <linux/stm/blitter.h>

struct stm_bdisp2_driver_data;
struct stm_bdisp2_device_data;

struct bdisp2_funcs {
	int (*fill_rect) (struct stm_bdisp2_driver_data       *stdrv,
			  const struct stm_bdisp2_device_data *stdev,
			  const stm_blitter_rect_t            *dst);
	int (*draw_rect) (struct stm_bdisp2_driver_data       *stdrv,
			  const struct stm_bdisp2_device_data *stdev,
			  const stm_blitter_rect_t            *dst);

	int (*blit)         (struct stm_bdisp2_driver_data  *stdrv,
			     struct stm_bdisp2_device_data  *stdev,
			     const stm_blitter_rect_t       *src,
			     const stm_blitter_point_t      *dst_pt);
	int (*stretch_blit) (struct stm_bdisp2_driver_data  *stdrv,
			     struct stm_bdisp2_device_data  *stdev,
			     const stm_blitter_rect_t       *src,
			     const stm_blitter_rect_t       *dst);
	int (*blit2)        (struct stm_bdisp2_driver_data  *stdrv,
			     struct stm_bdisp2_device_data  *stdev,
			     const stm_blitter_rect_t       *src1,
			     const stm_blitter_point_t      *src2_pt,
			     const stm_blitter_point_t      *dst_pt);
};


#endif /* __BDISPII_AQ_OPERATIONS_H__ */
