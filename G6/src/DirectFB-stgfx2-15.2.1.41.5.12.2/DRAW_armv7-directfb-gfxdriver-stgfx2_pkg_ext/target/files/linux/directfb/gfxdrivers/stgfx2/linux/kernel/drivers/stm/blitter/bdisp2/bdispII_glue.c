#include <linux/errno.h>

#include "blitter_backend.h"
#include "bdisp2/bdispII_aq.h"
#include "bdisp2/bdispII_aq_state.h"
#include "bdisp2/bdisp_accel.h"
#include "bdisp2/bdispII_aq_operations.h"


int
stm_blitter_backend_state_supported(
		void                           * const backend,
		const struct stm_blitter_state * const state,
		enum stm_blitter_accel          accel)
{
	const struct stm_bdisp2_aq * const aq = backend;
	enum stm_blitter_accel ret_accel;

	ret_accel = bdisp2_state_supported(&aq->stdev, state, accel);
	if (!(ret_accel & accel))
		return -EOPNOTSUPP;

	return 0;
}

int
stm_blitter_backend_state_update(void                     * const backend,
				 struct stm_blitter_state * const state,
				 enum stm_blitter_accel    accel)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_state_update(&aq->stdrv, &aq->stdev, state, accel);

	return 0;
}

int
stm_blitter_backend_validate_address(void          * const backend,
				     unsigned long  start,
				     unsigned long  end)
{
	const struct stm_bdisp2_aq * const aq = backend;

	return bdisp2_check_memory_constraints(&aq->stdev, start, end);
}

int
stm_blitter_backend_fill_rect(void                     * const backend,
			      const stm_blitter_rect_t * const rect)
{
	struct stm_bdisp2_aq * const aq = backend;

	return aq->stdrv.funcs.fill_rect(&aq->stdrv, &aq->stdev, rect);
}

int
stm_blitter_backend_draw_rect(void                     * const backend,
			      const stm_blitter_rect_t * const rect)
{
	struct stm_bdisp2_aq * const aq = backend;

	return aq->stdrv.funcs.draw_rect(&aq->stdrv, &aq->stdev, rect);
}

int
stm_blitter_backend_blit(void                      * const backend,
			 const stm_blitter_rect_t  * const src,
			 const stm_blitter_point_t * const dst_pt)
{
	struct stm_bdisp2_aq * const aq = backend;

	return aq->stdrv.funcs.blit(&aq->stdrv, &aq->stdev, src, dst_pt);
}

int
stm_blitter_backend_stretch_blit(void                     * const backend,
				 const stm_blitter_rect_t * const src,
				 const stm_blitter_rect_t * const dst)
{
	struct stm_bdisp2_aq * const aq = backend;

	return aq->stdrv.funcs.stretch_blit(&aq->stdrv, &aq->stdev,
					    src, dst);
}

int
stm_blitter_backend_blit2(void                      * const backend,
			  const stm_blitter_rect_t  * const src1,
			  const stm_blitter_point_t * const src2_pt,
			  const stm_blitter_point_t * const dst_pt)
{
	struct stm_bdisp2_aq * const aq = backend;

	return aq->stdrv.funcs.blit2(&aq->stdrv, &aq->stdev,
				     src1, src2_pt, dst_pt);
}

int
stm_blitter_backend_fence(void * const backend)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_fence(&aq->stdrv, &aq->stdev);

	return 0;
}

int
stm_blitter_backend_emit_commands(void * const backend)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_emit_commands(&aq->stdrv, &aq->stdev, 0);

	return 0;
}

int
stm_blitter_backend_get_serial(void                 * const backend,
			       stm_blitter_serial_t * const serial)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_get_serial(&aq->stdrv, &aq->stdev, serial);

	return 0;
}

int
stm_blitter_backend_wait_serial(void                       * const backend,
				const stm_blitter_serial_t  serial)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_wait_serial(&aq->stdrv, &aq->stdev, serial);

	return 0;
}

int
stm_blitter_backend_engine_sync(void * const backend)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_engine_sync(&aq->stdrv, &aq->stdev);

	return 0;
}

int
stm_blitter_backend_wait_space(void * const backend,
			       const stm_blitter_serial_t * const serial)
{
	struct stm_bdisp2_aq * const aq = backend;

	bdisp2_wait_event(&aq->stdrv, &aq->stdev, serial);

	return 0;
}
