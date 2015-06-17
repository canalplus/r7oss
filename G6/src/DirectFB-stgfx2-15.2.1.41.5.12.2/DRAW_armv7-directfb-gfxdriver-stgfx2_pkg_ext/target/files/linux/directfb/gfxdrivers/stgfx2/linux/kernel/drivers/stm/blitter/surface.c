#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <asm/sizes.h>

#include <linux/stm/blitter.h>
#include "surface.h"
#include "blitter_device.h"
#include "blit_debug.h"


#define THIS_FILE ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)


static atomic_t surface_ctr = ATOMIC_INIT(0);

static int
validate_address(enum stm_blitter_surface_format_e           format,
		 const struct stm_blitter_surface_address_s *buffer_address,
		 unsigned long                               buffer_size,
		 unsigned long                               stride,
		 long                                        luma_height)
{
	unsigned long cr_size = cr_size; /* avoid gcc warning */
	unsigned long cb_size = cb_size; /* avoid gcc warning */
	struct stm_blitter_s *blitter;
	int res = 0;

	if (!buffer_address) {
		printk(KERN_ERR "%s:%s:%d: address is not valid\n",
		       THIS_FILE, __func__, __LINE__);
		return -EINVAL;
	}

	switch (format) {
	/* these are not supported via the normal API */
	case STM_BLITTER_SF_RLD_BD:
	case STM_BLITTER_SF_RLD_H2:
	case STM_BLITTER_SF_RLD_H8:
	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		return -EINVAL;

	case STM_BLITTER_SF_LUT8:
	case STM_BLITTER_SF_ALUT88:
	case STM_BLITTER_SF_AlLUT88:
		if (buffer_address->palette.num_entries < 256)
			return -EINVAL;
		/* fall through */
	case STM_BLITTER_SF_LUT4:
	case STM_BLITTER_SF_ALUT44:
		if (buffer_address->palette.num_entries < 16)
			return -EINVAL;
		/* fall through */
	case STM_BLITTER_SF_LUT2:
		if (buffer_address->palette.num_entries < 4)
			return -EINVAL;
		/* fall through */
	case STM_BLITTER_SF_LUT1:
		if (buffer_address->palette.num_entries < 2)
			return -EINVAL;

		if (!buffer_address->palette.entries)
			return -EINVAL;

		/* fall through */
	case STM_BLITTER_SF_RGB565:
	case STM_BLITTER_SF_RGB24:
	case STM_BLITTER_SF_BGR24:
	case STM_BLITTER_SF_RGB32:
	case STM_BLITTER_SF_ARGB1555:
	case STM_BLITTER_SF_ARGB4444:
	case STM_BLITTER_SF_ARGB8565:
	case STM_BLITTER_SF_AlRGB8565:
	case STM_BLITTER_SF_ARGB:
	case STM_BLITTER_SF_AlRGB:
	case STM_BLITTER_SF_BGRA:
	case STM_BLITTER_SF_BGRAl:
	case STM_BLITTER_SF_ABGR:
	case STM_BLITTER_SF_A8:
	case STM_BLITTER_SF_Al8:
	case STM_BLITTER_SF_A1:

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
		/* check that the chroma offsets are positive... */
		if ((buffer_address->cb_offset <= 0)
		    || (buffer_address->cr_offset <= 0)
		    || ((buffer_address->base + buffer_address->cb_offset)
			<= buffer_address->base)
		    || ((buffer_address->base + buffer_address->cr_offset)
			<= buffer_address->base)) {
			printk(KERN_ERR
			       "%s:%s:%d: Chroma offsets must be positive!\n",
			       THIS_FILE, __func__, __LINE__);
			return -EINVAL;
		}
		if ((format == STM_BLITTER_SF_YV12)
		    || (format == STM_BLITTER_SF_I420)) {
			cb_size = cr_size = (luma_height * stride) / 4;
		} else if ((format == STM_BLITTER_SF_YV16)
			   || (format == STM_BLITTER_SF_YV61)) {
			cb_size = cr_size = (luma_height * stride) / 2;
		} else if (format == STM_BLITTER_SF_YCBCR444P) {
			cb_size = cr_size = luma_height * stride;
		} else
			BUG();

		/* ...enforce offsets for endianess... */
		if ((((format == STM_BLITTER_SF_I420)
		      || (format == STM_BLITTER_SF_YV61)
		      || (format == STM_BLITTER_SF_YCBCR444P))
		     && (buffer_address->cb_offset
			 >= buffer_address->cr_offset))
		    || (((format == STM_BLITTER_SF_YV12)
			 || (format == STM_BLITTER_SF_YV16))
			&& (buffer_address->cr_offset
			    >= buffer_address->cb_offset))) {
			printk(KERN_ERR
			       "%s:%s:%d: Chroma offset endianess wrong!\n",
			       THIS_FILE, __func__, __LINE__);
			return -EINVAL;
		}

		/* ... and that base + offset + chroma_size <= total size. */
		if (((buffer_address->cb_offset + cb_size) > buffer_size)
		    || ((buffer_address->cr_offset + cr_size) > buffer_size)) {
			printk(KERN_ERR
			       "%s:%s:%d: Chroma must be within surface buffer!\n",
			       THIS_FILE, __func__, __LINE__);
			return -EINVAL;
		}
		break;

	case STM_BLITTER_SF_YCBCR420MB:
	case STM_BLITTER_SF_YCBCR422MB:
		/* for BDispII, must be aligned to 256 bytes */
		if (buffer_address->base & 0xff) {
			printk(KERN_ERR
			       "%s:%s:%d: Macroblock surface base address must be aligned to 256 bytes, (got %llx)\n",
			       THIS_FILE, __func__, __LINE__,
			       (unsigned long long) buffer_address->base);
			return -EINVAL;
		}
		/* fall through */
	case STM_BLITTER_SF_NV12:
	case STM_BLITTER_SF_NV21:
	case STM_BLITTER_SF_NV16:
	case STM_BLITTER_SF_NV61:
	case STM_BLITTER_SF_NV24:
		/* check that the chroma offset is positive... */
		if ((buffer_address->cbcr_offset <= 0)
		    || ((buffer_address->base + buffer_address->cbcr_offset)
			<= buffer_address->base)) {
			printk(KERN_ERR
			       "%s:%s:%d: Chroma offset must be positive!\n",
			       THIS_FILE, __func__, __LINE__);
			return -EINVAL;
		}
		/* ... and that base + offset + chroma_size <= total size. */
		if ((format == STM_BLITTER_SF_YCBCR420MB)
		    || (format == STM_BLITTER_SF_NV12)
		    || (format == STM_BLITTER_SF_NV21)) {
			cr_size = (luma_height * stride) / 4;
		} else if ((format == STM_BLITTER_SF_YCBCR422MB)
			   || (format == STM_BLITTER_SF_NV16)
			   || (format == STM_BLITTER_SF_NV61)) {
			cr_size = (luma_height * stride) / 2;
		} else if (format == STM_BLITTER_SF_NV24) {
			cr_size = luma_height * stride * 2;
		} else
			BUG();

		if ((buffer_address->cbcr_offset + cr_size) > buffer_size) {
			printk(KERN_ERR
			       "%s:%s:%d: Chroma must be within surface buffer!\n",
			       THIS_FILE, __func__, __LINE__);
			return -EINVAL;
		}
		break;
	};

	/* for BDispII, this should not cross a 64MByte bank boundary! */

	blitter = stm_blitter_get(0);
	if (!IS_ERR(blitter)) {
		/* stm_blitter_get() should never return an error here, but
		   better save than sorry. As we don't know which blitter
		   the surface will be used with, we assume all IPs have the
		   same capabilities. */
		res = stm_blitter_validate_address(
				blitter,
				buffer_address->base,
				buffer_address->base + buffer_size - 1);
		if (res)
			printk(KERN_ERR
			       "%s:%s:%d: Surface must not cross a 64MB boundary, (got %llu + %llu)\n",
			       THIS_FILE, __func__, __LINE__,
			       (unsigned long long) buffer_address->base,
			       (unsigned long long) buffer_size);

		stm_blitter_put(blitter);
	}

	return res;
}

/* calculate minimum pitch */
static unsigned long
bytes_per_line(enum stm_blitter_surface_format_e format,
	       long                              width)
{
	if (width < 1)
		return 0;

	switch (format) {
	/* these are not supported via the normal API */
	case STM_BLITTER_SF_RLD_BD:
	case STM_BLITTER_SF_RLD_H2:
	case STM_BLITTER_SF_RLD_H8:
	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		return 0;

	case STM_BLITTER_SF_RGB565:
		width *= 16;
		break;
	case STM_BLITTER_SF_RGB24:
	case STM_BLITTER_SF_BGR24:
		width *= 24;
		break;
	case STM_BLITTER_SF_RGB32:
		width *= 32;
		break;
	case STM_BLITTER_SF_ARGB1555:
		width *= 16;
		break;
	case STM_BLITTER_SF_ARGB4444:
		width *= 16;
		break;
	case STM_BLITTER_SF_ARGB8565:
	case STM_BLITTER_SF_AlRGB8565:
		width *= 24;
		break;
	case STM_BLITTER_SF_ARGB:
	case STM_BLITTER_SF_AlRGB:
	case STM_BLITTER_SF_BGRA:
	case STM_BLITTER_SF_BGRAl:
	case STM_BLITTER_SF_ABGR:
		width *= 32;
		break;
	case STM_BLITTER_SF_LUT8:
		width *= 8;
		break;
	case STM_BLITTER_SF_LUT4:
		width *= 4;
		break;
	case STM_BLITTER_SF_LUT2:
		width *= 2;
		break;
	case STM_BLITTER_SF_LUT1:
		width *= 1;
		break;
	case STM_BLITTER_SF_ALUT88:
	case STM_BLITTER_SF_AlLUT88:
		width *= 16;
		break;
	case STM_BLITTER_SF_ALUT44:
		width *= 8;
		break;
	case STM_BLITTER_SF_A8:
	case STM_BLITTER_SF_Al8:
		width *= 8;
		break;
	case STM_BLITTER_SF_A1:
		width *= 1;
		break;

	case STM_BLITTER_SF_AVYU:
	case STM_BLITTER_SF_AlVYU:
		width *= 32;
		break;
	case STM_BLITTER_SF_VYU:
		width *= 24;
		break;
	case STM_BLITTER_SF_YUY2:
		width *= 16;
		break;
	case STM_BLITTER_SF_UYVY:
		width *= 16;
		break;

	case STM_BLITTER_SF_YV12:
	case STM_BLITTER_SF_I420:
		width *= 8;
		break;
	case STM_BLITTER_SF_YV16:
	case STM_BLITTER_SF_YV61:
		width *= 8;
		break;
	case STM_BLITTER_SF_YCBCR444P:
		width *= 8;
		break;
	case STM_BLITTER_SF_NV12:
	case STM_BLITTER_SF_NV21:
		width *= 8;
		break;
	case STM_BLITTER_SF_NV16:
	case STM_BLITTER_SF_NV61:
		width *= 8;
		break;
	case STM_BLITTER_SF_NV24:
		width *= 8;
		break;
	case STM_BLITTER_SF_YCBCR420MB:
	case STM_BLITTER_SF_YCBCR422MB:
		width *= 8;
		break;
	}

	width += 7;
	width /= 8;

	return width;
}

static unsigned long
multiply_planes(enum stm_blitter_surface_format_e format,
		unsigned long                     stride,
		long                              height)
{
	if (stride < 1)
		return 0;
	if (height < 1)
		return 0;

	switch (format) {
	/* these are not supported via the normal API */
	case STM_BLITTER_SF_RLD_BD:
	case STM_BLITTER_SF_RLD_H2:
	case STM_BLITTER_SF_RLD_H8:
	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		return 0;

	case STM_BLITTER_SF_RGB565:
	case STM_BLITTER_SF_RGB24:
	case STM_BLITTER_SF_BGR24:
	case STM_BLITTER_SF_RGB32:
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
	case STM_BLITTER_SF_AVYU:
	case STM_BLITTER_SF_AlVYU:
	case STM_BLITTER_SF_VYU:
	case STM_BLITTER_SF_YUY2:
	case STM_BLITTER_SF_UYVY:
		break;

	case STM_BLITTER_SF_YV12:
	case STM_BLITTER_SF_I420:
	case STM_BLITTER_SF_NV12:
	case STM_BLITTER_SF_NV21:
	case STM_BLITTER_SF_YCBCR420MB:
		height += ((height+1) / 2);
		break;

	case STM_BLITTER_SF_YV16:
	case STM_BLITTER_SF_YV61:
	case STM_BLITTER_SF_NV16:
	case STM_BLITTER_SF_NV61:
	case STM_BLITTER_SF_YCBCR422MB:
		height *= 2;
		break;

	case STM_BLITTER_SF_YCBCR444P:
	case STM_BLITTER_SF_NV24:
		height *= 3;
		break;
	}

	return stride * height;
}

static int
validate_size(enum stm_blitter_surface_format_e     format,
	      unsigned long                         buffer_size,
	      const struct stm_blitter_dimension_s *size,
	      unsigned long                         stride)
{
	unsigned long min_bpl, min_size;

	if (!size) {
		printk(KERN_ERR "%s:%s:%d: size is NULL\n",
		       THIS_FILE, __func__, __LINE__);
		return -EINVAL;
	}

	min_bpl = bytes_per_line(format, size->w);
	if (min_bpl == 0 || stride < min_bpl) {
		printk(KERN_ERR
		       "%s:%s:%d: unknown format or stride too small (want >= %llu, got %llu)\n",
		       THIS_FILE, __func__, __LINE__,
		       (unsigned long long) min_bpl,
		       (unsigned long long) stride);
		return -EINVAL;
	}

	min_size = multiply_planes(format, stride, size->h);
	if (min_size == 0 || buffer_size < min_size) {
		printk(KERN_ERR
		       "%s:%s:%d: unknown format or buffer size too small (want >= %llu, got %llu)\n",
		       THIS_FILE, __func__, __LINE__,
		       (unsigned long long) min_size,
		       (unsigned long long) buffer_size);
		return -EINVAL;
	}

	switch (format) {
	/* these are not supported via the normal API */
	case STM_BLITTER_SF_RLD_BD:
	case STM_BLITTER_SF_RLD_H2:
	case STM_BLITTER_SF_RLD_H8:
	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		return -EINVAL;

	case STM_BLITTER_SF_RGB565:
	case STM_BLITTER_SF_RGB24:
	case STM_BLITTER_SF_BGR24:
	case STM_BLITTER_SF_RGB32:
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
	case STM_BLITTER_SF_AVYU:
	case STM_BLITTER_SF_AlVYU:
	case STM_BLITTER_SF_VYU:
	case STM_BLITTER_SF_YUY2:
	case STM_BLITTER_SF_UYVY:

	case STM_BLITTER_SF_YV12:
	case STM_BLITTER_SF_I420:
	case STM_BLITTER_SF_YV16:
	case STM_BLITTER_SF_YV61:
	case STM_BLITTER_SF_YCBCR444P:
	case STM_BLITTER_SF_NV12:
	case STM_BLITTER_SF_NV21:
	case STM_BLITTER_SF_NV16:
	case STM_BLITTER_SF_NV61:
	case STM_BLITTER_SF_NV24:
		break;

	case STM_BLITTER_SF_YCBCR420MB:
	case STM_BLITTER_SF_YCBCR422MB:
		/* for BDispII, width and height must be multiples of 16 */
		if (size->w & 0xf || size->h & 0xf)
			return -EINVAL;
		break;
	}

	/* FIXME: implement! */
	return 0;
}

/* surface state */
stm_blitter_surface_t *
stm_blitter_surface_new_preallocated(
		stm_blitter_surface_format_t         format,
		stm_blitter_surface_colorspace_t     colorspace,
		const stm_blitter_surface_address_t *buffer_address,
		unsigned long                        buffer_size,
		const stm_blitter_dimension_t       *size,
		unsigned long                        stride)
{
	stm_blitter_surface_t *surf;
	int                    res;

	/* check provided buffer size against minimum requirements */
	res = validate_size(format, buffer_size, size, stride);
	if (res < 0)
		return ERR_PTR(res);

	/* check provided address and size against whitelist */
	res = validate_address(format, buffer_address, buffer_size, stride,
			       size->h);
	if (res < 0)
		return ERR_PTR(res);

	surf = kzalloc(sizeof(*surf), GFP_KERNEL);
	if (!surf) {
		/* FIXME: do more? */
		return ERR_PTR(-ENOMEM);
	}

	kref_init(&surf->kref);

	surf->format = format;
	surf->colorspace = colorspace;
	surf->buffer_address = *buffer_address;
	surf->buffer_size = buffer_size;
	surf->size = *size;
	surf->stride = stride;

	surf->creation = atomic_inc_return(&surface_ctr);
	surf->state.dst = surf;
	surf->state.src2 = surf;
	surf->state.clip_mode = STM_BLITTER_CM_CLIP_INSIDE;
	surf->state.clip.position1.x = 0;
	surf->state.clip.position1.y = 0;
	surf->state.clip.position2.x = size->w - 1;
	surf->state.clip.position2.y = size->h - 1;
	surf->state.blitflags = STM_BLITTER_SBF_NONE;
	surf->state.drawflags = STM_BLITTER_SDF_NONE;
	surf->state.color_colorspace = STM_BLITTER_COLOR_RGB;
	memset(surf->state.colors, 0xff, sizeof(surf->state.colors));
	surf->state.color_index = 0x00;
	memset(surf->state.dst_ckey, 0xff, sizeof(surf->state.dst_ckey));
	surf->state.dst_ckey_mode = (STM_BLITTER_CKM_R_IGNORE
				     | STM_BLITTER_CKM_G_IGNORE
				     | STM_BLITTER_CKM_B_IGNORE);
	memset(surf->src_ckey, 0xff, sizeof(surf->src_ckey));
	surf->src_ckey_mode = (STM_BLITTER_CKM_R_IGNORE
			       | STM_BLITTER_CKM_G_IGNORE
			       | STM_BLITTER_CKM_B_IGNORE);
	surf->state.pd = STM_BLITTER_PD_SOURCE_OVER;
	/* FIXME: is this right? */
	memset(&surf->state.color_matrix, 0,
	       sizeof(surf->state.color_matrix));
	surf->state.modified = STM_BLITTER_SMF_ALL;

	surf->serial = 0;

	stm_blitter_magic_set(surf, stm_blitter_surface_t);

	__module_get(THIS_MODULE);

	return surf;
}
EXPORT_SYMBOL(stm_blitter_surface_new_preallocated);

int
stm_blitter_surface_get(stm_blitter_surface_format_t         format,
			stm_blitter_surface_colorspace_t     colorspace,
			const stm_blitter_surface_address_t *buffer_address,
			unsigned long                        buffer_size,
			const stm_blitter_dimension_t       *size,
			unsigned long                        pitch,
			stm_blitter_surface_t              **surface)
{
	stm_blitter_surface_t *s;

	if (!surface)
		return -EINVAL;

	s = stm_blitter_surface_new_preallocated(format, colorspace,
						 buffer_address, buffer_size,
						 size, pitch);
	if (IS_ERR(s))
		return PTR_ERR(s);

	*surface = s;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_get);

stm_blitter_surface_t *
stm_blitter_surface_ref(stm_blitter_surface_t *surface)
{
	if (surface) {
		stm_blitter_magic_assert(surface, stm_blitter_surface_t);

		kref_get(&surface->kref);
	}

	return surface;
}
EXPORT_SYMBOL(stm_blitter_surface_ref);

static void
stm_blitter_surface_destroy(struct kref *ref)
{
	stm_blitter_surface_t *surface = container_of(ref,
						      stm_blitter_surface_t,
						      kref);

	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	stm_blitter_magic_clear(surface);

	/* to avoid indestructible (self referencing) surfaces, we only added
	   a reference if src != this in
	   stm_blitter_surface_update_src_on_dst() and
	   stm_blitter_surface_update_src2_on_dst(). Therefore, we obviously
	   can only release src and src2 if they are != this. */
	if (surface->state.src != surface)
		stm_blitter_surface_put(surface->state.src);
	if (surface->state.src2 != surface)
		stm_blitter_surface_put(surface->state.src2);

	kfree(surface);

	module_put(THIS_MODULE);
}

void
stm_blitter_surface_put(stm_blitter_surface_t *surface)
{
	if (surface) {
		stm_blitter_magic_assert(surface, stm_blitter_surface_t);

		kref_put(&surface->kref, stm_blitter_surface_destroy);
	}
}
EXPORT_SYMBOL(stm_blitter_surface_put);

int
stm_blitter_surface_update_address(
		stm_blitter_surface_t               *surface,
		const stm_blitter_surface_address_t *buffer_address)
{
	int res;

	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !buffer_address)
		return -EINVAL;

	if (!memcmp(&surface->buffer_address, buffer_address,
		    sizeof(*buffer_address)))
		return 0;

	res = validate_address(surface->format, buffer_address,
			       surface->buffer_size, surface->stride,
			       surface->size.h);
	if (res < 0)
		return res;

	surface->buffer_address = *buffer_address;
	surface->state.modified |= STM_BLITTER_SMF_DST_ADDRESS;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_update_address);

int
stm_blitter_surface_set_clip(stm_blitter_surface_t      *surface,
			     const stm_blitter_region_t *clip,
			     stm_blitter_clip_mode_t     mode)
{
	struct stm_blitter_point_s surface_end;
	stm_blitter_region_t     newclip;

	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !clip)
		return -EINVAL;

	switch (mode) {
	case STM_BLITTER_CM_CLIP_INSIDE:
	case STM_BLITTER_CM_CLIP_OUTSIDE:
		break;

	default:
		return -EINVAL;
	}

	surface_end.x = surface->size.w - 1;
	surface_end.y = surface->size.h - 1;

	/* don't allow clip to be outside surface size */
	if (clip->position1.x > surface_end.x
	    || clip->position1.y > surface_end.y
	    || clip->position2.x < 0
	    || clip->position2.y < 0) {
		return -EINVAL;
	}

	/* otherwise clamp clip to surface size (if necessary) */
	newclip.position1.x = max(clip->position1.x, 0l);
	newclip.position1.y = max(clip->position1.y, 0l);
	newclip.position2.x = min(clip->position2.x, surface_end.x);
	newclip.position2.y = min(clip->position2.y, surface_end.y);

	if (memcmp(&newclip, &surface->state.clip, sizeof(newclip))) {
		surface->state.clip = newclip;
		surface->state.modified |= STM_BLITTER_SMF_CLIP;
	}

	if (surface->state.clip_mode != mode) {
		surface->state.clip_mode = mode;
		surface->state.modified |= STM_BLITTER_SMF_CLIP;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_clip);

int
stm_blitter_surface_set_porter_duff(stm_blitter_surface_t          *surface,
				    stm_blitter_porter_duff_rule_t  pd)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	switch (pd) {
	case STM_BLITTER_PD_CLEAR:
	case STM_BLITTER_PD_SOURCE:
	case STM_BLITTER_PD_DEST:
	case STM_BLITTER_PD_SOURCE_OVER:
	case STM_BLITTER_PD_DEST_OVER:
	case STM_BLITTER_PD_SOURCE_IN:
	case STM_BLITTER_PD_DEST_IN:
	case STM_BLITTER_PD_SOURCE_OUT:
	case STM_BLITTER_PD_DEST_OUT:
	case STM_BLITTER_PD_SOURCE_ATOP:
	case STM_BLITTER_PD_DEST_ATOP:
	case STM_BLITTER_PD_XOR:
		if (pd != surface->state.pd) {
			surface->state.pd = pd;
			surface->state.modified |= STM_BLITTER_SMF_PORTER_DUFF;
		}
		return 0;

	case STM_BLITTER_PD_NONE:
		/* we don't want to support this in the kernel */
		break;

	/* _no_ default case! */
	}

	return -EINVAL;
}
EXPORT_SYMBOL(stm_blitter_surface_set_porter_duff);

static int
stm_blitter_colors_equal(enum stm_blitter_surface_format_e  format,
			 const struct stm_blitter_color_s  *color1,
			 const struct stm_blitter_color_s  *color2)
{
	if (format & STM_BLITTER_SF_ALPHA
	    && color1->a != color2->a)
		return false;

	if (format & STM_BLITTER_SF_YCBCR
	    && (color1->y != color2->y
		|| color1->cb != color2->cb
		|| color1->cr != color2->cr))
		return false;

	if (color1->r != color2->r
	    || color1->g != color2->g
	    || color1->b != color2->b)
		return false;

	return true;
}

int
stm_blitter_surface_set_color(stm_blitter_surface_t     *surface,
			      const stm_blitter_color_t *color)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !color)
		return -EINVAL;

	if (!stm_blitter_colors_equal(surface->format,
				      &surface->state.colors[0], color)) {
		surface->state.colors[0] = *color;
		surface->state.modified |= STM_BLITTER_SMF_COLOR;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_color);

int
stm_blitter_surface_set_color_index(stm_blitter_surface_t *surface,
				    uint8_t                index)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	if (!(surface->format & STM_BLITTER_SF_INDEXED))
		return -EINVAL;

	switch (surface->format) {
	case STM_BLITTER_SF_LUT8:
	case STM_BLITTER_SF_ALUT88:
	case STM_BLITTER_SF_AlLUT88:
		if (index > 255)
			return -EINVAL;
		break;

	case STM_BLITTER_SF_LUT4:
	case STM_BLITTER_SF_ALUT44:
		if (index > 15)
			return -EINVAL;
		break;

	case STM_BLITTER_SF_LUT2:
		if (index > 3)
			return -EINVAL;
		break;

	case STM_BLITTER_SF_LUT1:
		if (index > 1)
			return -EINVAL;
		break;

	case STM_BLITTER_SF_RGB565:
	case STM_BLITTER_SF_RGB24:
	case STM_BLITTER_SF_BGR24:
	case STM_BLITTER_SF_RGB32:
	case STM_BLITTER_SF_ARGB1555:
	case STM_BLITTER_SF_ARGB4444:
	case STM_BLITTER_SF_ARGB8565:
	case STM_BLITTER_SF_AlRGB8565:
	case STM_BLITTER_SF_ARGB:
	case STM_BLITTER_SF_AlRGB:
	case STM_BLITTER_SF_BGRA:
	case STM_BLITTER_SF_BGRAl:
	case STM_BLITTER_SF_ABGR:
	case STM_BLITTER_SF_A8:
	case STM_BLITTER_SF_Al8:
	case STM_BLITTER_SF_A1:
	case STM_BLITTER_SF_AVYU:
	case STM_BLITTER_SF_AlVYU:
	case STM_BLITTER_SF_VYU:
	case STM_BLITTER_SF_YUY2:
	case STM_BLITTER_SF_UYVY:
	case STM_BLITTER_SF_YV12:
	case STM_BLITTER_SF_I420:
	case STM_BLITTER_SF_YV16:
	case STM_BLITTER_SF_YV61:
	case STM_BLITTER_SF_YCBCR444P:
	case STM_BLITTER_SF_YCBCR420MB:
	case STM_BLITTER_SF_YCBCR422MB:
	case STM_BLITTER_SF_NV12:
	case STM_BLITTER_SF_NV21:
	case STM_BLITTER_SF_NV16:
	case STM_BLITTER_SF_NV61:
	case STM_BLITTER_SF_NV24:
	case STM_BLITTER_SF_RLD_BD:
	case STM_BLITTER_SF_RLD_H2:
	case STM_BLITTER_SF_RLD_H8:

	case STM_BLITTER_SF_INVALID:
	case STM_BLITTER_SF_COUNT:
	default:
		/* shouldn't be reached */
		return -EINVAL;
	}

	if (index != surface->state.color_index) {
		surface->state.color_index = index;
		surface->state.modified |= STM_BLITTER_SMF_COLOR;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_color_index);

int
stm_blitter_surface_set_color_colorspace(stm_blitter_surface_t     *surface,
			      const stm_blitter_color_colorspace_t colorspace)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	if (surface->state.color_colorspace != colorspace) {
		surface->state.color_colorspace = colorspace;
		surface->state.modified |= STM_BLITTER_SMF_COLOR;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_color_colorspace);

int
stm_blitter_surface_set_color_stop(stm_blitter_surface_t     *surface,
				   const stm_blitter_point_t *offset,
				   const stm_blitter_color_t *color)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !offset || !color)
		return -EINVAL;

	return -ENOSYS;
}
EXPORT_SYMBOL(stm_blitter_surface_set_color_stop);

int
stm_blitter_surface_set_color_matrix(stm_blitter_surface_t            *surface,
				     const stm_blitter_color_matrix_t *matrix)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !matrix)
		return -EINVAL;

	if (memcmp(&surface->state.color_matrix, matrix, sizeof(*matrix))) {
		memcpy(&surface->state.color_matrix, matrix,
		       sizeof(*matrix));

		surface->state.modified |= STM_BLITTER_SMF_COLOR_MATRIX;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_color_matrix);

int
stm_blitter_surface_set_src_colorkey(stm_blitter_surface_t       *surface,
				     const stm_blitter_color_t   *low,
				     const stm_blitter_color_t   *high,
				     stm_blitter_colorkey_mode_t  mode)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !low || !high)
		return -EINVAL;

	if (!mode || (mode & ~STM_BLITTER_CKM_ALL))
		return -EINVAL;

	surface->src_ckey[0] = *low;
	surface->src_ckey[1] = *high;
	surface->src_ckey_mode = mode;

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_src_colorkey);

int
stm_blitter_surface_set_dst_colorkey(stm_blitter_surface_t       *surface,
				     const stm_blitter_color_t   *low,
				     const stm_blitter_color_t   *high,
				     stm_blitter_colorkey_mode_t  mode)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if (!surface || IS_ERR(surface) || !low || !high)
		return -EINVAL;

	if (!mode || (mode & ~STM_BLITTER_CKM_ALL))
		return -EINVAL;

	if (!stm_blitter_colors_equal(surface->format,
				      &surface->state.dst_ckey[0], low)) {
		surface->state.dst_ckey[0] = *low;
		surface->state.modified |= STM_BLITTER_SMF_DST_COLORKEY;
	}
	if (!stm_blitter_colors_equal(surface->format,
				      &surface->state.dst_ckey[1], high)) {
		surface->state.dst_ckey[1] = *high;
		surface->state.modified |= STM_BLITTER_SMF_DST_COLORKEY;
	}

	if (surface->state.dst_ckey_mode != mode) {
		surface->state.dst_ckey_mode = mode;
		surface->state.modified |= STM_BLITTER_SMF_DST_COLORKEY;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_dst_colorkey);

int
stm_blitter_surface_set_ff_mode(stm_blitter_surface_t     *surface,
				stm_blitter_flicker_filter_mode_t mode)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	if (mode & ~STM_BLITTER_FF_ALL)
		return -EINVAL;

	if (surface->state.ff_mode != mode) {
		surface->state.ff_mode = mode;
		surface->state.modified |=STM_BLITTER_SMF_FILTERING;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_ff_mode);

int
stm_blitter_surface_set_vc1_range(stm_blitter_surface_t *surface,
				  unsigned char          luma_coeff,
				  unsigned char          chroma_coeff)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);

	return -ENOSYS;
}
EXPORT_SYMBOL(stm_blitter_surface_set_vc1_range);

int
stm_blitter_surface_set_blitflags(stm_blitter_surface_t           *surface,
				  stm_blitter_surface_blitflags_t  flags)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	flags &= STM_BLITTER_SBF_ALL;

	/* it doesn't make sense to read from the 'other' (2nd)
	   source if no actual blending (with the destination)
	   was requested */
	if (!(flags & (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
		       | STM_BLITTER_SBF_BLEND_COLORALPHA)))
		flags &= ~STM_BLITTER_SBF_READ_SOURCE2;

	if (surface->state.blitflags != flags) {
		/* for SMF_FILTERING, we have to figure out if the filtering
		   has changed. */
		if ((surface->state.blitflags ^ flags)
		    & (STM_BLITTER_SBF_NO_FILTER
		       | STM_BLITTER_SBF_FLICKER_FILTER
		       | STM_BLITTER_SBF_STRICT_INPUT_RECT)) {
			surface->state.modified |= STM_BLITTER_SMF_FILTERING;
		}

		/* for SMF_BLITFLAGS, we have to figure out if any of
		   the other flags have changed. */
		if ((surface->state.blitflags & ~(STM_BLITTER_SBF_NO_FILTER
		                                  | STM_BLITTER_SBF_FLICKER_FILTER
						  | STM_BLITTER_SBF_STRICT_INPUT_RECT))
		    != (flags & ~(STM_BLITTER_SBF_NO_FILTER
		                  | STM_BLITTER_SBF_FLICKER_FILTER
				  | STM_BLITTER_SBF_STRICT_INPUT_RECT))){
			surface->state.modified |= STM_BLITTER_SMF_BLITFLAGS;
		}
		surface->state.blitflags = flags;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_blitflags);

int
stm_blitter_surface_set_drawflags(stm_blitter_surface_t           *surface,
				  stm_blitter_surface_drawflags_t  flags)
{
	stm_blitter_magic_assert(surface, stm_blitter_surface_t);
	if ((!surface) || IS_ERR(surface))
		return -EINVAL;

	flags &= STM_BLITTER_SDF_ALL;
	if (surface->state.drawflags != flags) {
		surface->state.drawflags = flags;
		surface->state.modified |= STM_BLITTER_SMF_DRAWFLAGS;
	}

	return 0;
}
EXPORT_SYMBOL(stm_blitter_surface_set_drawflags);


/* internal helpers */
void
stm_blitter_surface_update_src_on_dst(stm_blitter_surface_t *dst,
				      stm_blitter_surface_t *src)
{
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src, stm_blitter_surface_t);

	/* if the new source surface is different from what we had before... */
	if (dst->state.src != src) {
		/* ...then we need to release the old one and make sure the
		   new one does not get destroyed while we use it. */
		if (src != dst)
			/* but to avoid indestructible (self referencing)
			   surfaces, we only add a reference if src != dst */
			src = stm_blitter_surface_ref(src);

		if (dst->state.src != dst)
			/* similarly, only release the old src if it is not
			   dst, as we will not have added a reference
			   otherwise */
			stm_blitter_surface_put(dst->state.src);

		dst->state.src = src;
		dst->state.modified |= STM_BLITTER_SMF_SRC;
	}
}

void
stm_blitter_surface_update_src2_on_dst(stm_blitter_surface_t *dst,
				       stm_blitter_surface_t *src2)
{
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src2, stm_blitter_surface_t);

	/* if the new source2 surface is different from what we had before... */
	if (dst->state.src2 != src2) {
		/* ...then we need to release the old one and make sure the
		   new one does not get destroyed while we use it. */
		if (src2 != dst)
			/* but to avoid indestructible (self referencing)
			   surfaces, we only add a reference if src2 != dst */
			src2 = stm_blitter_surface_ref(src2);

		if (dst->state.src2 != dst)
			/* similarly, only release the old src2 if it is not
			   dst, as we will not have added a reference
			   otherwise */
			stm_blitter_surface_put(dst->state.src2);

		dst->state.src2 = src2;
		dst->state.modified |= STM_BLITTER_SMF_SRC2;
	}
}

void
stm_blitter_surface_update_src_colorkey_on_dst(
		stm_blitter_surface_t       *dst,
		const stm_blitter_surface_t *src)
{
	stm_blitter_magic_assert(dst, stm_blitter_surface_t);
	stm_blitter_magic_assert(src, stm_blitter_surface_t);

	if (!stm_blitter_colors_equal(src->format,
				      &dst->state.src_ckey[0],
				      &src->src_ckey[0])) {
		dst->state.src_ckey[0] = src->src_ckey[0];
		dst->state.modified |= STM_BLITTER_SMF_SRC_COLORKEY;
	}

	if (!stm_blitter_colors_equal(src->format,
				      &dst->state.src_ckey[1],
				      &src->src_ckey[1])) {
		dst->state.src_ckey[1] = src->src_ckey[1];
		dst->state.modified |= STM_BLITTER_SMF_SRC_COLORKEY;
	}

	if (dst->state.src_ckey_mode != src->src_ckey_mode) {
		dst->state.src_ckey_mode = src->src_ckey_mode;
		dst->state.modified |= STM_BLITTER_SMF_SRC_COLORKEY;
	}
}
