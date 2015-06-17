#ifndef __BLITTER_BACKEND_H__
#define __BLITTER_BACKEND_H__

#include <linux/types.h>
#include <linux/stm/blitter.h>
#include "state.h"



int
stm_blitter_backend_state_supported(void                           *backend,
				    const struct stm_blitter_state *state,
				    enum stm_blitter_accel          accel);
int stm_blitter_backend_state_update(void                     *backend,
				     struct stm_blitter_state *state,
				     enum stm_blitter_accel    accel);


int stm_blitter_backend_validate_address(void          * const backend,
					 unsigned long  start,
					 unsigned long  end);


int stm_blitter_backend_fill_rect(void                     *backend,
				  const stm_blitter_rect_t *rect);

int stm_blitter_backend_draw_rect(void                     *backend,
				  const stm_blitter_rect_t *rect);

int stm_blitter_backend_blit(void                      *backend,
			     const stm_blitter_rect_t  *src,
			     const stm_blitter_point_t *dst_pt);

int stm_blitter_backend_stretch_blit(void                     *backend,
				     const stm_blitter_rect_t *src,
				     const stm_blitter_rect_t *dst);

int stm_blitter_backend_blit2(void                      *backend,
			      const stm_blitter_rect_t  *src1,
			      const stm_blitter_point_t *src2_pt,
			      const stm_blitter_point_t *dst_pt);

int stm_blitter_backend_fence(void *backend);


int stm_blitter_backend_emit_commands(void *backend);


int stm_blitter_backend_get_serial(void                 *backend,
				   stm_blitter_serial_t *serial);

int stm_blitter_backend_wait_serial(void                       *backend,
				    const stm_blitter_serial_t  serial);

int stm_blitter_backend_engine_sync(void *backend);
int stm_blitter_backend_wait_space(void                       *backend,
				   const stm_blitter_serial_t *serial);


#endif /* __BLITTER_BACKEND_H__ */
