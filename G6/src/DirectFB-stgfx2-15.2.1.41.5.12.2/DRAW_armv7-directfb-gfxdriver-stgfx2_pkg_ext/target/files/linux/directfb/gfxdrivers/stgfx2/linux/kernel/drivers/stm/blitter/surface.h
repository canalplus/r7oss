#ifndef __SURFACE_H__
#define __SURFACE_H__

#ifdef __KERNEL__
#include <linux/kref.h>
#endif /* __KERNEL__ */

#include <linux/stm/blitter.h>
#include "state.h"


struct stm_blitter_surface_s {
#ifdef __KERNEL__
	struct kref kref;
	unsigned int magic;
#endif /* __KERNEL__ */

	stm_blitter_surface_format_t     format;
	stm_blitter_surface_colorspace_t colorspace;

	stm_blitter_surface_address_t    buffer_address;
	unsigned long                    buffer_size;

	stm_blitter_dimension_t          size;
	unsigned long                    stride;

	stm_blitter_color_t              src_ckey[2]; /* low and high */
	stm_blitter_colorkey_mode_t      src_ckey_mode;

	unsigned int creation; /* serial number of surface/state */
	struct stm_blitter_state state;

	stm_blitter_serial_t serial; /*!< serial number of last operation on
					  this surface */
};


#endif /* __SURFACE_H__ */
