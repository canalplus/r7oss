#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>

#include <linux/stm/blitter.h>

#include "blitter_os.h"
#include "class.h"
#include "blitter_device.h"
#include "surface.h"
#include "state.h"
#include "strings.h"
#include "blitter_backend.h"
#include "blit_debug.h"


struct stm_blitter_s {
	struct kref kref;

	unsigned int magic;

	struct stm_blitter_class_device *sbcd;

	void                     *data;
	struct stm_blitter_state *state;
	unsigned int              state_owner;

	struct mutex              mutex;
};

static stm_blitter_t blitter_devices[STM_BLITTER_MAX_DEVICES];

int
stm_blitter_register_device(struct device                     *dev,
			    char                              *device_name,
			    const struct stm_blitter_file_ops *fops,
			    void                              *data)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(blitter_devices); ++i) {
		if (!blitter_devices[i].data) {
			stm_blitter_t *blitter = &blitter_devices[i];

			blitter->sbcd = stm_blitter_classdev_init(i,
								  dev,
								  device_name,
								  data,
								  fops);
			if (IS_ERR(blitter->sbcd))
				return PTR_ERR(blitter->sbcd);

			blitter->data = data;
			blitter->state = NULL;
			blitter->state_owner = -1;

			mutex_init(&blitter->mutex);

			stm_blitter_magic_set(blitter, stm_blitter_t);

			return 0;
		}
	}

	return -ENOMEM;
}

int
stm_blitter_unregister_device(void *data)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(blitter_devices); ++i) {
		if (blitter_devices[i].data == data) {
			stm_blitter_classdev_deinit(blitter_devices[i].sbcd);

			blitter_devices[i].data = NULL;

			stm_blitter_magic_clear(&blitter_devices[i]);

			return 0;
		}
	}

	return -ENODEV;
}



int
stm_blitter_validate_address(stm_blitter_t *blitter,
			     unsigned long  start,
			     unsigned long  end)
{
	return stm_blitter_backend_validate_address(blitter->data,
						    start, end);
}



stm_blitter_t *
stm_blitter_get(int id)
{
	if (id < 0 || id >= STM_BLITTER_MAX_DEVICES)
		return ERR_PTR(-EINVAL);

	if (!blitter_devices[id].data)
		return ERR_PTR(-ENODEV);

	__module_get(THIS_MODULE);

	return &blitter_devices[id];
}
EXPORT_SYMBOL(stm_blitter_get);

int
stm_blitter_get_device(int             id,
		       stm_blitter_t **blitter)
{
	stm_blitter_t *b;

	if (!blitter)
		return -EINVAL;

	b = stm_blitter_get(id);
	if (IS_ERR(b))
		return PTR_ERR(b);

	*blitter = b;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_get_device);

void
stm_blitter_put(stm_blitter_t *blitter)
{
	stm_blitter_magic_assert(blitter, stm_blitter_t);

	module_put(THIS_MODULE);
}
EXPORT_SYMBOL(stm_blitter_put);

int
stm_blitter_wait(stm_blitter_t        *blitter,
		 stm_blitter_wait_t    type,
		 stm_blitter_serial_t  serial)
{
	int ret;
	stm_blitter_serial_t last_op;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	if (!blitter)
		return -EINVAL;

	ret = mutex_lock_interruptible(&blitter->mutex);
	if (ret)
		return ret;

	switch (type) {
	case STM_BLITTER_WAIT_SYNC:
		ret = stm_blitter_backend_engine_sync(blitter->data);
		break;
	case STM_BLITTER_WAIT_SERIAL:
		/* check for invalid serial */
		stm_blitter_backend_get_serial(blitter->data, &last_op);
		if (serial > last_op) {
			/* maybe we got a wrap-around, we allow for a window
			   of about 2**16 operations */
			if ((serial <= 0xffffffffffff8000LLU)
			    || (last_op >= 0x0000000000007fffLLU)) {
				ret = -EINVAL;
				break;
			}
		}
		ret = stm_blitter_backend_wait_serial(blitter->data, serial);
		break;
	case STM_BLITTER_WAIT_FENCE:
		ret = stm_blitter_backend_wait_space(blitter->data, &serial);
		break;
	case _STM_BLITTER_WAIT_SOME:
		ret = stm_blitter_backend_wait_space(blitter->data, NULL);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&blitter->mutex);

	return ret;
}
EXPORT_SYMBOL(stm_blitter_wait);

int
stm_blitter_get_serial(stm_blitter_t        *blitter,
		       stm_blitter_serial_t *serial)
{
	int ret;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	if (!blitter || !serial)
		return -EINVAL;

	ret = mutex_lock_interruptible(&blitter->mutex);
	if (ret)
		return ret;

	stm_blitter_backend_get_serial(blitter->data, serial);

	mutex_unlock(&blitter->mutex);

	return 0;
}
EXPORT_SYMBOL(stm_blitter_get_serial);


int
stm_blitter_surface_get_serial(stm_blitter_surface_t *surface,
			       stm_blitter_serial_t  *serial)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !serial)
		return -EINVAL;

	*serial = surface->serial;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_get_serial);


int
stm_blitter_surface_add_fence(stm_blitter_surface_t *surface)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	surface->state.fence = 1;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_add_fence);


int
stm_blitter_surface_clear(stm_blitter_t             *blitter,
			  stm_blitter_surface_t     *dst,
			  const stm_blitter_color_t *color)
{
	stm_blitter_color_t saved_color;
	stm_blitter_surface_drawflags_t saved_drawflags;
	stm_blitter_rect_t rect;
	int res;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	if (!blitter || !dst)
		return -EINVAL;

	/* FIXME: support STM_BLITTER_CM_CLIP_OUTSIDE */
	rect.position.x = dst->state.clip.position1.x;
	rect.position.y = dst->state.clip.position1.y;
	rect.size.w = (dst->state.clip.position2.x
		       - dst->state.clip.position1.x + 1);
	rect.size.h = (dst->state.clip.position2.y
		       - dst->state.clip.position1.y + 1);

	/* save current state */
	saved_color = dst->state.colors[0];
	saved_drawflags = dst->state.drawflags;

	/* set a temporary state */
	stm_blitter_surface_set_color(dst, color);
	stm_blitter_surface_set_drawflags(dst, STM_BLITTER_SDF_NONE);

	/* draw */
	res = stm_blitter_surface_fill_rects(blitter, dst, &rect, 1);

	/* restore original state */
	stm_blitter_surface_set_drawflags(dst, saved_drawflags);
	stm_blitter_surface_set_color(dst, &saved_color);

	return res;
}
EXPORT_SYMBOL(stm_blitter_surface_clear);


int
stm_blitter_surface_fill_rect(stm_blitter_t            *blitter,
			      stm_blitter_surface_t    *dst,
			      const stm_blitter_rect_t *dst_rect)
{
	return stm_blitter_surface_fill_rects(blitter, dst, dst_rect, 1);
}
EXPORT_SYMBOL(stm_blitter_surface_fill_rect);

static int
clip_rectangle(stm_blitter_rect_t             *clipped_rect,
	       const stm_blitter_region_t     *clip,
	       const stm_blitter_rect_t       *rect,
	       const struct stm_blitter_state *state,
	       int                             one)
{
	stm_blitter_dimension_t rotated_size = rect->size;
	stm_blitter_dimension_t rotated_clip;

	if (state->blitflags & (STM_BLITTER_SBF_ROTATE90
				| STM_BLITTER_SBF_ROTATE270)) {
		rotated_size.w = rect->size.h;
		rotated_size.h = rect->size.w;
	}

	if (rect->position.x + rotated_size.w <= clip->position1.x
	    || rect->position.x > clip->position2.x
	    || rect->position.y + rotated_size.h <= clip->position1.y
	    || rect->position.y > clip->position2.y)
		/* rectangle is outside clipping area */
		return 1;

	*clipped_rect = *rect;
	rotated_clip = rect->size;

	if (state->blitflags & (STM_BLITTER_SBF_ROTATE90
				| STM_BLITTER_SBF_ROTATE270)) {
		rotated_clip.w = clipped_rect->size.h;
		rotated_clip.h = clipped_rect->size.w;
	}

	/* modify rectangle to be completely inside clipping region */
	/* FIXME: support STM_BLITTER_CM_CLIP_OUTSIDE */
	if (clipped_rect->position.x < clip->position1.x) {
		clipped_rect->size.w -= (clip->position1.x
					 - clipped_rect->position.x);
		clipped_rect->position.x = clip->position1.x;
	}

	if (clipped_rect->position.y < clip->position1.y) {
		clipped_rect->size.h -= (clip->position1.y
					 - clipped_rect->position.y);
		clipped_rect->position.y = clip->position1.y;
	}

	if (clipped_rect->position.x + rotated_clip.w - one
	    > clip->position2.x)
		clipped_rect->size.w = (clip->position2.x
					- clipped_rect->position.x + one);

	if (clipped_rect->position.y + rotated_clip.h - one
	    > clip->position2.y)
		clipped_rect->size.h = (clip->position2.y
					- clipped_rect->position.y + one);

	return 0;
}

static int
stm_blitter_state_apply(stm_blitter_t            *blitter,
			struct stm_blitter_state *state,
			unsigned int              creation,
			enum stm_blitter_accel    accel)
{
	int retval;

	/* check if state is supported */
	retval = stm_blitter_backend_state_supported(blitter->data,
						     state, accel);
	if (retval) {
		char * const a = stm_blitter_get_accel_string(accel);
		pr_info("Operation (%s) not supported\n", a);
		stm_blitter_free_mem(a);
		return retval;
	}

	retval = mutex_lock_interruptible(&blitter->mutex);
	if (retval)
		return retval;

	/* set state */
	if (blitter->state != state
	    || blitter->state_owner != creation) {
		/* we are switching to a completely new state */
		state->modified = STM_BLITTER_SMF_ALL;
		blitter->state = state;
		blitter->state_owner = creation;
	}

	if (state->modified
	    || !(state->set & accel)) {
		stm_blitter_backend_state_update(blitter->data,
						 state, accel);
	}

	return 0;
}

static void
stm_blitter_state_release(stm_blitter_t            *blitter,
			  struct stm_blitter_state *state,
			  stm_blitter_serial_t     *serial)
{
	if (state->fence) {
		stm_blitter_backend_fence(blitter->data);
		state->fence = 0;
	}

	stm_blitter_backend_emit_commands(blitter->data);
	stm_blitter_backend_get_serial(blitter->data, serial);

	mutex_unlock(&blitter->mutex);
}

int
stm_blitter_surface_fill_rects(stm_blitter_t            *blitter,
			       stm_blitter_surface_t    *dst,
			       const stm_blitter_rect_t *dst_rects,
			       unsigned int              count)
{
	int retval;
	unsigned int i;
	stm_blitter_rect_t tmp_rect;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	if (!blitter || !dst)
		return -EINVAL;

	if (!count)
		return 0;
	/* we allow dst_rects to be == NULL if count == 1 */
	if (count == 1) {
		if (dst_rects == NULL) {
			dst_rects = &tmp_rect;
			tmp_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			tmp_rect.size = dst->size;
		}
	} else if (count && !dst_rects)
		return -EINVAL;

	retval = stm_blitter_state_apply(blitter, &dst->state,
					 dst->creation,
					 STM_BLITTER_ACCEL_FILLRECT);
	if (retval)
		return retval;

	for (i = 0; (retval == 0) && i < count; ++i) {
		stm_blitter_rect_t rect;

		if (dst_rects[i].size.w < 1
		    || dst_rects[i].size.h < 1) {
			retval = -EINVAL;
			goto out;
		}

		/* apply clip (as hardware can't reliably do it). This might
		   change sometime in the future, though. */
		if (!clip_rectangle(&rect, &dst->state.clip, &dst_rects[i],
				    &dst->state, 1))
			retval = stm_blitter_backend_fill_rect(blitter->data,
							       &rect);
	}

out:
	stm_blitter_state_release(blitter, &dst->state, &dst->serial);

	return retval;
}
EXPORT_SYMBOL(stm_blitter_surface_fill_rects);

int
stm_blitter_surface_fill_rect2(stm_blitter_t         *blitter,
			       stm_blitter_surface_t *dst,
			       long                   dst_x,
			       long                   dst_y,
			       long                   dst_w,
			       long                   dst_h)
{
	stm_blitter_rect_t r = {
		.position.x = dst_x,
		.position.y = dst_y,
		.size.w = dst_w,
		.size.h = dst_h
	};

	return stm_blitter_surface_fill_rect(blitter, dst, &r);
}
EXPORT_SYMBOL(stm_blitter_surface_fill_rect2);

int
stm_blitter_surface_draw_rect(stm_blitter_t            *blitter,
			      stm_blitter_surface_t    *dst,
			      const stm_blitter_rect_t *dst_rect)
{
	return stm_blitter_surface_draw_rects(blitter, dst, dst_rect, 1);
}
EXPORT_SYMBOL(stm_blitter_surface_draw_rect);

int
stm_blitter_surface_draw_rects(stm_blitter_t            *blitter,
			       stm_blitter_surface_t    *dst,
			       const stm_blitter_rect_t *dst_rects,
			       unsigned int              count)
{
	stm_blitter_rect_t rects[4];
	unsigned int       i;
	int                ret;
	stm_blitter_rect_t tmp_rect;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	if (!blitter || !dst)
		return -EINVAL;

	if (!count)
		return 0;
	/* we allow dst_rects to be == NULL if count == 1 */
	if (count == 1) {
		if (dst_rects == NULL) {
			dst_rects = &tmp_rect;
			tmp_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			tmp_rect.size = dst->size;
		}
	} else if (count && !dst_rects)
		return -EINVAL;

	for (i = 0, ret = 0; (ret == 0) && i < count; ++i) {
		stm_blitter_rect_t *tmp = rects;
		int                 n_rects = 0;

		/* top line */
		tmp->position.x = dst_rects[i].position.x;
		tmp->position.y = dst_rects[i].position.y;
		tmp->size.w = dst_rects[i].size.w;
		tmp->size.h = 1;
		++tmp;
		++n_rects;

		if (dst_rects[i].size.h > 1) {
			/* bottom line */
			tmp->position.x = dst_rects[i].position.x;
			tmp->position.y = (dst_rects[i].position.y
					   + dst_rects[i].size.h - 1);
			tmp->size.w = dst_rects[i].size.w;
			tmp->size.h = 1;
			++tmp;
			++n_rects;

			if (dst_rects[i].size.h > 2) {
				long y2 = dst_rects[i].position.y + 1;
				long h2 = dst_rects[i].size.h - 2;

				/* left line */
				tmp->position.x = dst_rects[i].position.x;
				tmp->position.y = y2;
				tmp->size.w = 1;
				tmp->size.h = h2;
				++tmp;
				++n_rects;

				if (dst_rects[i].size.w > 2) {
					/* right line */
					tmp->position.x
						= (dst_rects[i].position.x
						   + dst_rects[i].size.w
						   - 1);
					tmp->position.y = y2;
					tmp->size.w = 1;
					tmp->size.h = h2;
					++n_rects;
				}
			}
		}

		ret = stm_blitter_surface_fill_rects(blitter,
						     dst, rects, n_rects);
	}

	return ret;
}
EXPORT_SYMBOL(stm_blitter_surface_draw_rects);


int
stm_blitter_surface_blit(stm_blitter_t             *blitter,
			 stm_blitter_surface_t     *src,
			 const stm_blitter_rect_t  *src_rects,
			 stm_blitter_surface_t     *dst,
			 const stm_blitter_point_t *dst_points,
			 unsigned int               count)
{
	int retval;
	unsigned int i;
	stm_blitter_rect_t tmp_rect;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src, stm_blitter_surface_t);
	if (!blitter || !dst || !src)
		return -EINVAL;

	if (!count)
		return 0;
	/* we allow src_rects and/or dst_points to be == NULL
	   if count == 1 */
	if (count == 1) {
		if (src_rects == NULL
		    || dst_points == NULL) {
			tmp_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			if (src_rects == NULL) {
				src_rects = &tmp_rect;
				tmp_rect.size = src->size;
			}
			if (dst_points == NULL)
				dst_points = &tmp_rect.position;
		}
	} else if (count && (!src_rects || !dst_points))
		return -EINVAL;

	stm_blitter_surface_update_src_on_dst(dst, src);
	if (dst->state.blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
		stm_blitter_surface_update_src_colorkey_on_dst(dst, src);

	retval = stm_blitter_state_apply(blitter, &dst->state, dst->creation,
					 STM_BLITTER_ACCEL_BLIT);
	if (retval)
		return retval;

	for (i = 0; (retval == 0) && i < count; ++i) {
		stm_blitter_rect_t drect = { .position = dst_points[i],
					     .size = src_rects[i].size };
		stm_blitter_rect_t srect;

		/* the source width and height must be > 0 */
		if (src_rects[i].size.w <= 0
		    || src_rects[i].size.h <= 0) {
			retval = -EINVAL;
			goto out;
		}

		/* apply clip (as hardware can't reliably do it). This might
		   change sometime in the future, though. */
		if (!clip_rectangle(&drect, &dst->state.clip, &drect,
				    &dst->state, 1)) {
			srect.position.x = (src_rects[i].position.x
					    + drect.position.x
					    - dst_points[i].x);
			srect.position.y = (src_rects[i].position.y
					    + drect.position.y
					    - dst_points[i].y);
			srect.size = drect.size;

			retval = stm_blitter_backend_blit(blitter->data,
							  &srect,
							  &drect.position);
		}
	}

out:
	stm_blitter_state_release(blitter, &dst->state, &dst->serial);

	return retval;
}
EXPORT_SYMBOL(stm_blitter_surface_blit);

int
stm_blitter_surface_stretchblit(stm_blitter_t            *blitter,
				stm_blitter_surface_t    *src,
				const stm_blitter_rect_t *src_rects,
				stm_blitter_surface_t    *dst,
				const stm_blitter_rect_t *dst_rects,
				unsigned int              count)
{
	int retval;
	unsigned int i;
	stm_blitter_rect_t src_rect, dst_rect;

	stm_blitter_region_t src_surf_region, clip;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src, stm_blitter_surface_t);
	if (!blitter || !dst || !src)
		return -EINVAL;

	if (!count)
		return 0;
	/* we allow src_rects and/or dst_rects to be == NULL
	   if count == 1 */
	if (count == 1) {
		if (src_rects == NULL) {
			src_rects = &src_rect;
			src_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			src_rect.size = src->size;
		}
		if (dst_rects == NULL) {
			dst_rects = &dst_rect;
			dst_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			dst_rect.size = dst->size;
		}
	} else if (count && (!dst_rects || !src_rects))
		return -EINVAL;

	stm_blitter_surface_update_src_on_dst(dst, src);
	if (dst->state.blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
		stm_blitter_surface_update_src_colorkey_on_dst(dst, src);

	retval = stm_blitter_state_apply(blitter, &dst->state, dst->creation,
					 STM_BLITTER_ACCEL_STRETCHBLIT);
	if (retval)
		return retval;

	src_surf_region.position1.x = 0;
	src_surf_region.position1.y = 0;
	src_surf_region.position2.x = src->size.w - 1;
	src_surf_region.position2.y = src->size.h - 1;

	clip = dst->state.clip;

#define FIXED_POINT_ONE  (1 << 16)

	src_surf_region.position1.x *= FIXED_POINT_ONE;
	src_surf_region.position1.y *= FIXED_POINT_ONE;
	src_surf_region.position2.x *= FIXED_POINT_ONE;
	src_surf_region.position2.y *= FIXED_POINT_ONE;

	clip.position1.x *= FIXED_POINT_ONE;
	clip.position1.y *= FIXED_POINT_ONE;
	clip.position2.x *= FIXED_POINT_ONE;
	clip.position2.y *= FIXED_POINT_ONE;

	for (i = 0; (retval == 0) && i < count; ++i) {
		stm_blitter_rect_t drect1, drect2;
		stm_blitter_rect_t srect1, srect2;

		srect1 = src_rects[i];
		drect1 = dst_rects[i];
		/* need to upscale the rectangles to make the clipping easier */
		if ((dst->state.blitflags
		     & STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
		    != STM_BLITTER_SBF_ALL_IN_FIXED_POINT) {
			/* not (completely) in fixed point */
			if (!(dst->state.blitflags
			      & STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT)) {
				/* src x/y are not in fixed point */
				srect1.position.x *= FIXED_POINT_ONE;
				srect1.position.y *= FIXED_POINT_ONE;
			}

			/* src w/h will always need adjusting */
			srect1.size.w *= FIXED_POINT_ONE;
			srect1.size.h *= FIXED_POINT_ONE;
			/* as will destination */
			drect1.position.x *= FIXED_POINT_ONE;
			drect1.position.y *= FIXED_POINT_ONE;
			drect1.size.w *= FIXED_POINT_ONE;
			drect1.size.h *= FIXED_POINT_ONE;
		}

		/* the destination width and height can't be < 1 pixel */
		if (drect1.size.w < FIXED_POINT_ONE
		    || drect1.size.h < FIXED_POINT_ONE) {
			retval = -EINVAL;
			goto out;
		}

		/* the source can */
		if (src_rects[i].size.w <= 0
		    || src_rects[i].size.h <= 0) {
			retval = -EINVAL;
			goto out;
		}

		/* check that the source rect is within the surface bounds */
		if (clip_rectangle(&srect2, &src_surf_region, &srect1,
				   &dst->state, FIXED_POINT_ONE)) {
			/* it isn't, so there's nothing to do, of course  */
			continue;
		}

		/* if we adjusted the source rect, we have to adjust the
		   destination rect accordingly, too */
		if (srect2.position.x != srect1.position.x) {
			long long x;

			x = srect2.position.x - srect1.position.x;
			x *= (long long) drect1.size.w;
			x = div64_u64(x, srect1.size.w);

			drect1.position.x += x;
		}

		if (srect2.position.y != srect1.position.y) {
			long long y;

			y = srect2.position.y - srect1.position.y;
			y *= (long long) drect1.size.h;
			y = div64_u64(y, srect1.size.h);

			drect1.position.y += y;
		}

		if (srect2.size.w != srect1.size.w) {
			long long w;

			w = drect1.size.w;
			w *= (long long) srect2.size.w;

			drect1.size.w = div64_u64(w, srect1.size.w);
		}

		if (srect2.size.h != srect1.size.h) {
			long long h;

			h = drect1.size.h;
			h *= (long long) srect2.size.h;
			drect1.size.h = div64_u64(h, srect1.size.h);
		}

		/* apply clip (as hardware can't reliably do it). This might
		   change sometime in the future, though. */
		if (clip_rectangle(&drect2, &clip, &drect1,
				   &dst->state, FIXED_POINT_ONE)) {
			continue;
		}


		/* if we adjusted the destination rect, we have to
		   adjust the source rect accordingly, too */
		if (drect2.position.x != drect1.position.x) {
			long long x;

			x = drect2.position.x - drect1.position.x;
			x *= (long long) srect2.size.w;
			x = div64_u64(x, drect1.size.w);

			srect2.position.x += x;
		}

		if (drect2.position.y != drect1.position.y) {
			long long y;

			y = drect2.position.y - drect1.position.y;
			y *= (long long) srect2.size.h;
			y = div64_u64(y, drect1.size.h);

			srect2.position.y += y;
		}

		if (drect2.size.w != drect1.size.w) {
			long long w;

			w = srect2.size.w;
			w *= (long long) drect2.size.w;

			srect2.size.w = div64_u64(w, drect1.size.w);
		}

		if (drect2.size.h != drect1.size.h) {
			long long h;

			h = srect2.size.h;
			h *= (long long) drect2.size.h;

			srect2.size.h = div64_u64(h, drect1.size.h);
		}

		/* restore flags as necessary */
		if ((dst->state.blitflags
		     & STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
		    != STM_BLITTER_SBF_ALL_IN_FIXED_POINT) {
			/* not (completely) in fixed point */
			if (!(dst->state.blitflags
			      & STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT)) {
				/* src x/y are not in fixed point */
				srect2.position.x /= FIXED_POINT_ONE;
				srect2.position.y /= FIXED_POINT_ONE;
			}
			/* src w/h will always need adjusting */
			srect2.size.w /= FIXED_POINT_ONE;
			srect2.size.h /= FIXED_POINT_ONE;
			/* as will destination */
			drect2.position.x /= FIXED_POINT_ONE;
			drect2.position.y /= FIXED_POINT_ONE;
			drect2.size.w /= FIXED_POINT_ONE;
			drect2.size.h /= FIXED_POINT_ONE;
		}

		retval = stm_blitter_backend_stretch_blit(blitter->data,
							  &srect2,
							  &drect2);
	}

out:
	stm_blitter_state_release(blitter, &dst->state, &dst->serial);

	return retval;
}
EXPORT_SYMBOL(stm_blitter_surface_stretchblit);

int
stm_blitter_surface_blit_two(stm_blitter_t             *blitter,
			     stm_blitter_surface_t     *src,
			     const stm_blitter_rect_t  *src_rects,
			     stm_blitter_surface_t     *src2,
			     const stm_blitter_point_t *src2_points,
			     stm_blitter_surface_t     *dst,
			     const stm_blitter_point_t *dst_points,
			     unsigned int               count)
{
	int retval;
	unsigned int i;
	stm_blitter_rect_t tmp_rect;
	stm_blitter_region_t src_surf_region;
	stm_blitter_region_t src2_surf_region;

	stm_blitter_magic_assert(blitter, stm_blitter_t);
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src, stm_blitter_surface_t);
	stm_blitter_magic_assert(src2, stm_blitter_surface_t);
	if (!blitter || !dst || !src || !src2)
		return -EINVAL;

	if (!count)
		return 0;
	/* we allow src_rects and/or dst_points to be == NULL
	   if count == 1 */
	if (count == 1) {
		if (src_rects == NULL
		    || src2_points == NULL
		    || dst_points == NULL) {
			tmp_rect.position = (stm_blitter_point_t) { .x = 0,
								    .y = 0 };
			if (src_rects == NULL) {
				src_rects = &tmp_rect;
				tmp_rect.size = src->size;
			}
			if (src2_points == NULL)
				src2_points = &tmp_rect.position;
			if (dst_points == NULL)
				dst_points = &tmp_rect.position;
		}
	} else if (count && (!src_rects || !src2_points || !dst_points))
		return -EINVAL;

	stm_blitter_surface_update_src_on_dst(dst, src);
	stm_blitter_surface_update_src2_on_dst(dst, src2);
	if (dst->state.blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
		stm_blitter_surface_update_src_colorkey_on_dst(dst, src);

	retval = stm_blitter_state_apply(blitter, &dst->state, dst->creation,
					 STM_BLITTER_ACCEL_BLIT2);
	if (retval)
		return retval;

	src_surf_region = (stm_blitter_region_t)
		{ .position1 = { .x = 0,
				 .y = 0 },
		  .position2 = { .x = src->size.w - 1,
				 .y = src->size.h - 1 } };

	src2_surf_region = (stm_blitter_region_t)
		{ .position1 = { .x = 0,
				 .y = 0 },
		  .position2 = { .x = src2->size.w - 1,
				 .y = src2->size.h - 1 } };

	for (i = 0; (retval == 0) && i < count; ++i) {
		stm_blitter_rect_t  srect1;
		stm_blitter_rect_t  srect2 = { .position = src2_points[i] };
		stm_blitter_rect_t  drect = { .position = dst_points[i] };
		stm_blitter_rect_t  tmp;

		/* the source width and height must be > 0 */
		if (src_rects[i].size.w <= 0
		    || src_rects[i].size.h <= 0) {
			retval = -EINVAL;
			goto out;
		}

		/* check that the source rect is within the surface bounds */
		if (clip_rectangle(&srect1, &src_surf_region, &src_rects[i],
				   &dst->state, 1))
			continue;

		/* if we adjusted the source rect, we have to adjust the
		   other rects accordingly, too */
		if (srect1.position.x != src_rects[i].position.x) {
			long x = srect1.position.x - src_rects[i].position.x;

			srect2.position.x += x;
			drect.position.x += x;
		}
		if (srect1.position.y != src_rects[i].position.y) {
			long y = srect1.position.y - src_rects[i].position.y;

			srect2.position.y += y;
			drect.position.y += y;
		}

		/* check that the source2 rect is within the surface bounds */
		tmp.position = srect2.position;
		tmp.size = srect1.size;
		if (clip_rectangle(&srect2, &src2_surf_region, &tmp,
				   &dst->state, 1))
			continue;

		/* if we adjusted the source2 rect, we have to adjust the
		   other rects accordingly, too */
		if (srect2.position.x != tmp.position.x) {
			long x = srect2.position.x - tmp.position.x;

			srect1.position.x += x;
			drect.position.x += x;
		}
		if (srect2.position.y != tmp.position.y) {
			long y = srect2.position.y - tmp.position.y;

			srect1.position.y += y;
			drect.position.y += y;
		}

		/* apply clip (as hardware can't reliably do it). This might
		   change sometime in the future, though. */
		tmp.position = drect.position;
		tmp.size = srect2.size;
		if (clip_rectangle(&drect, &dst->state.clip, &tmp,
				   &dst->state, 1))
			continue;

		/* if we adjusted the dest rect, we have to adjust the
		   other rects accordingly, too */
		if (drect.position.x != tmp.position.x) {
			long x = drect.position.x - tmp.position.x;

			srect1.position.x += x;
			srect2.position.x += x;
		}
		if (drect.position.y != tmp.position.y) {
			long y = drect.position.y - tmp.position.y;

			srect1.position.y += y;
			srect2.position.y += y;
		}

		srect1.size = drect.size;

		retval = stm_blitter_backend_blit2(blitter->data, &srect1,
					           &srect2.position, &drect.position);
	}

out:
	stm_blitter_state_release(blitter, &dst->state, &dst->serial);

	return retval;
}
EXPORT_SYMBOL(stm_blitter_surface_blit_two);
