/*
 * STMicroelectronics BDispII driver - BDispII device features
 *
 * Copyright (c) 2013 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BDISPII_DEVICE_FEATURES_H__
#define __BDISPII_DEVICE_FEATURES_H__

#include <linux/stm/bdisp2_shared.h>


struct _stm_bdisp2_hw_features {
	__u16 size_mask;
	__u16 line_buffer_length;
	__u16 mb_buffer_length;
	__u16 rotate_buffer_length;
	__u16 upsampled_nbpix_min;

	/* see https://bugzilla.stlinux.com/show_bug.cgi?id=19843#c1 */
	unsigned int nmdk_macroblock:1; /*!< nomadik macroblock support */
	unsigned int planar_r:1; /*!< can read 2 & 3 planar surfaces */
	unsigned int planar2_w:1; /*!< can write 2-planar surfaces */
	unsigned int planar3_w:1; /*!< can write 3-planar surfaces */
	unsigned int rgb32:1; /*!< 4byte xRGB  */
	unsigned int rle_bd:1; /*!< RLE (BD) decoding */
	unsigned int rle_hd:1; /*!< RLE (HD-DVD) decoding */
	unsigned int s1_422r:1; /*!< 422 raster supported on S1 */
	unsigned int s1_subbyte:1; /*!< subbyte formats on S1 */

	unsigned int boundary_bypass:1; /*!< boundary bypass */
	unsigned int dst_colorkey:1; /*!< working destination color keying */
	unsigned int flicker_filter:1; /*!< flicker filter */
	unsigned int gradient:1; /*!< support gradient fill */
	unsigned int owf:1; /*!< hw support for OWF blending */
	unsigned int plane_mask:1; /*!< hw support for plane mask */
	unsigned int porterduff:1; /*!< hw support for PorterDuff blending */
	unsigned int rotation:1; /*!< support rotation */
	unsigned int spatial_dei:1; /*!< spatial DEI */

	unsigned int no_address_banks:1; /*!< linear memory access, not in
					banks of 64MB  */
	unsigned int size_4k:1; /*!< 4Ki sizes supported (x/y: -8192...8191
				w/h: 0...8191) */

	unsigned int aq_lock:1; /*!< lock node feature for AQs */

	unsigned int write_posting:1; /*!< write posting on target plug */
	unsigned int need_itm:1; /*!< route interrupts to girq0 */
};

#endif /* __BDISPII_DEVICE_FEATURES_H__ */
