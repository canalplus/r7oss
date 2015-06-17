/***********************************************************************
This file is part of stm-blitter driver

COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
License type: GPLv2

stm-blitter is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

stm-blitter is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with stm-blitter; if not, write to the Free Software

Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

The stm-blitter Library may alternatively be licensed under a proprietary
license from ST.

This file was created by STMicroelectronics on 2008-09-12
and modified by STMicroelectronics on 2014-05-28

***********************************************************************/

/*! \file blitter.h
 *  \brief public API for stm blitter
 */

/*!
 * \mainpage STM Blitter API documentation
 *
 * \section intro_section Introduction
 *
 * \subsection purpose_and_scope Purpose and scope
 *
 * The purpose of this document is to describe the public interface of the
 * kernel space graphics blitter API, which is used, for example, by the
 * Linux DVB backend. This kernel-level interface,
 * which is intended to be used by middleware, provides low-level interfaces
 * to accelerate graphics operations by using the integrated graphics
 * acceleration IP.
 *
 * The document also includes descriptions for common graphics composition
 * rules, as well as blitter kernel API use cases.
 *
 * The document provides:
 * - Data flow
 * - Definition and use of concepts
 * - Detailed API structures
 *
 * \subsection intended_audience Intended Audience
 *
 * This document is intended for the following audience:
 * - Driver writers needing to access Blitter Device functions.
 * - Developers responsible for the design and implementation of the
 *   middleware software requiring accelerated graphics operations.
 * - Developers responsible for the design and implementation of the kernel
 *   space blitter driver.
 * - Quality assurance engineers responsible to testing the kernel space
 *   blitter driver.
 * - Program and project managers responsible for planning and tracking the
 *   kernel space blitter engine.
 *
 * \subsection notes Notes
 *
 * Type definitions, functions, and events that are not specified in this
 * document are assumed to be private to the blitter driver and should not
 * be used by middleware. In most situations, this is enforced in the Linux
 * kernel module loader.
 * For more information on these private interfaces, refer to the BDisp user
 * space driver implementation, formally called STGFX2, source code.
 *
 * \subsection acronyms Acronyms
 *
 * - API     Application Program Interface
 * - AQ      Application Queue (part of the BDispII IP)
 * - BDispII STM's graphics acceleration IP block
 * - SoC     System on Chip
 *
 *
 * \subsection references References
 *
 * - Compositing Digital Images,
 *   http://keithp.com/~keithp/porterduff/p253-porter.pdf
 * - DirectFB API, Ver. 1.4, http://www.directfb.org
 * - Recommendation BT.601, Ver. 201103, http://www.itu.int/rec/R-REC-BT.601/en
 * - Recommendation BT.709, Ver. 200204, http://www.itu.int/rec/R-REC-BT.709/en
 *
 *
 * \section Overview Overview
 *
 * The Graphics Blitter API is a set of functions to abstract the acceleration
 * capabilities of the hardware blitter device.
 * The Graphics Blitter API provides surfaces creation/releasing/management.
 * This API provides accelerated support for the following graphics operations:
 * - \ref drawblit
 *   - Fill Rectangle
 *   - Draw Rectangle
 * - \ref drawblit
 *   - Blit (from source to destination without resizing and with format
 *     conversion, rotation, blending, etc.)
 *   - Stretch Blit (from source to destination with resizing, format
 *     conversion, rotation, etc.)
 *   - Blit Two (from two sources to destination with resizing, format
 *     conversion, rotation, etc.)
 * .
 * It also includes:
 * - \ref state
 *   - Clip rectangle
 *   - Foreground color
 *   - Porter-Duff rule
 *   - Source and/or destination color key
 *   - Drawing and blitting flags
 * - \ref surfaces
 *   - create and release surface handles
 * - \ref device
 *   - create and release device handles
 *
 *
 * \section usage_section Usage examples
 *
 * Put some examples here.
 *
 */


#ifndef __STM_BLITTER_H__
#define __STM_BLITTER_H__


/*!
 * \brief a blitter handle
 *
 * \ingroup device
 */
typedef struct stm_blitter_s stm_blitter_t;
/*!
 * \brief a surface handle
 *
 * \ingroup surfaces
 */
typedef struct stm_blitter_surface_s stm_blitter_surface_t;

/*!
 * \brief an opaque blitter handle
 *
 * \ingroup device
 */
typedef struct stm_blitter_s *stm_blitter_h;
/*!
 * \brief an opaque surface handle
 *
 * \ingroup surfaces
 */
typedef struct stm_blitter_surface_s *stm_blitter_surface_h;

/*!
 * \brief Wait mode.
 *
 * \apis ::stm_blitter_wait(), ::stm_blitter_get_serial(),
 * ::stm_blitter_surface_get_serial(), ::stm_blitter_surface_add_fence()
 */
typedef enum stm_blitter_wait_e {
	STM_BLITTER_WAIT_SYNC,   /*!< wait until device is idle */
	_STM_BLITTER_WAIT_SOME,  /*!< \hideinitializer */
	STM_BLITTER_WAIT_SERIAL, /*!< wait for serial number completed */
	STM_BLITTER_WAIT_FENCE,  /*!< wait for a fenced operation to complete */
} stm_blitter_wait_t;

static inline enum stm_blitter_wait_e
deprecated_stm_blitter_wait_mode(enum stm_blitter_wait_e wait)
__attribute__((deprecated, const));
static inline enum stm_blitter_wait_e
deprecated_stm_blitter_wait_mode(enum stm_blitter_wait_e wait)
{
	return wait;
}
/*!
 * \hideinitializer
 * \deprecated This will be removed soon. There should be no need to use it!
 */
#define STM_BLITTER_WAIT_SOME \
		deprecated_stm_blitter_wait_mode(_STM_BLITTER_WAIT_SOME)

/*!
 * \brief serial number of operation
 *
 * \apis ::stm_blitter_wait(), ::stm_blitter_get_serial(),
 * ::stm_blitter_surface_get_serial()
 */
typedef unsigned long long stm_blitter_serial_t;


/*!
 * This type is the definition of a point. It is defined by its position (x,y).
 */
typedef struct stm_blitter_point_s {
	long x; /*!< X coordinate of pixel. */
	long y; /*!< Y coordinate of pixel. */
} stm_blitter_point_t;

/*!
 * This type is the definition of a dimension. It is defined by its
 * size (width x height).
 */
typedef struct stm_blitter_dimension_s {
	long w; /*!< Width in pixels. */
	long h; /*!< Height in pixels. */
} stm_blitter_dimension_t;

/*!
 * This type is the definition of a rectangle. It is defined by its
 * position (x,y) and size (width x height).
 */
typedef struct stm_blitter_rect_s {
	stm_blitter_point_t     position; /*!< Offset / position of
					       rectangle. */
	stm_blitter_dimension_t size; /*!< Size of rectangle. */
} stm_blitter_rect_t;

/*!
 * This type is the defintion of a region. It is defined by two points.
 */
typedef struct stm_blitter_region_s {
	stm_blitter_point_t position1; /*!< Offset / position 1 */
	stm_blitter_point_t position2; /*!< Offset / position 2 */
} stm_blitter_region_t;

/*!
 * This type defines color data information. The data information is adressed to
 * represent an ARGB or AYCbCr or a ALUT color type.
 * Alpha-based color types are represented through the Alpha value.
 */
typedef struct stm_blitter_color_s {
	uint8_t a; /*!< alpha value */

	union {
		struct {
			uint8_t r; /*!< Red color component. */
			uint8_t g; /*!< Green color component. */
			uint8_t b; /*!< Blue color component. */
		};
		struct {
			uint8_t cr; /*!< Cr (Chroma) component. */
			uint8_t y;  /*!< Y (Luma) component. */
			uint8_t cb; /*!< Cb (Chroma) component. */
		};
		struct {
			uint8_t index; /*!< Index for LUT types. */
		};
	};
} stm_blitter_color_t;

/*!
 * color matrix for #STM_BLITTER_SBF_SRC_COLORIZE
 *
 * \apis #STM_BLITTER_SBF_SRC_COLORIZE
 *
 * \note The color matrix is used to perform per pixel operations as described
 * by the following formula:
 * \f[
 *   \left[ \begin{array}{c} Vo_1 \\
 *                           Vo_2 \\
 *                           Vo_3
 *          \end{array} \right]
 *       =
 *       \left[ \begin{array}{ccc} C_{11} & C_{12} & C_{13} \\
 *                                 C_{21} & C_{22} & C_{23} \\
 *                                 C_{31} & C_{32} & C_{33}
 *              \end{array} \right]
 *       \times
 *       \left[ \begin{array}{c} Vi_1 \\
 *                               Vi_2 \\
 *                               Vi_3
 *              \end{array} \right]
 *       +
 *       \left[ \begin{array}{c} Off_1 \\
 *                               Off_2 \\
 *                               Off_3
 *              \end{array} \right]
 * \f]
 */
typedef struct stm_blitter_color_matrix_s {
	short coeff[3][3]; /*!< 3x3 coefficient matrix */
	short offset[3];   /*!< 3x1 offset */
} stm_blitter_color_matrix_t;


/*!
 * a palette
 */
typedef struct stm_blitter_palette_s {
	int                  num_entries; /*!< number of palette entries */
	long                 clut_base; /*!< physical base address */
	stm_blitter_color_t *entries; /*!< virtual base address */
} stm_blitter_palette_t;

/*!
 * This is a surface address, which is different based on the surface's
 * surface format.
 *
 * \apis ::stm_blitter_surface_new_preallocated()
 */
typedef struct stm_blitter_surface_address_s {
	unsigned long base; /*!< Base address for raster buffer, or Y buffer */
	union {
		/*! for indexed formats */
		stm_blitter_palette_t palette; /*!< the color lookup table */
		/*! for YCbCr formats formats with two planes */
		long cbcr_offset; /*!< Offset in bytes from Luma to
				       CbCr buffer */
		/*! for YCbCr formats with three planes */
		struct {
			long cb_offset; /*!< Offset in bytes from Luma to Cb
					     buffer */
			long cr_offset; /*!< Offset in bytes from Luma to Cr
					     buffer */
		};
	};
} stm_blitter_surface_address_t;

/*!
 * \private
 */
enum {
	STM_BLITTER_SF_INDEXED = 0x80000000,
	STM_BLITTER_SF_ALPHA   = 0x40000000,
	STM_BLITTER_SF_PLANAR  = 0x20000000, /*!< 2 planes */
	STM_BLITTER_SF_YCBCR   = 0x10000000,
	STM_BLITTER_SF_ALPHA_LIMITED = 0x08000000 | STM_BLITTER_SF_ALPHA,
	STM_BLITTER_SF_PLANAR3  = 0x04000000 | STM_BLITTER_SF_PLANAR, /*!< 3 planes */
};

/*!
 * \brief This specifies a surface format.
 * \hideinitializer
 *
 * \apis ::stm_blitter_surface_new_preallocated(), #STM_BLITTER_SF_MASK
 */
typedef enum stm_blitter_surface_format_e {
	STM_BLITTER_SF_INVALID    = 0x00000000,

	STM_BLITTER_SF_RGB565     = 0x00000001, /*!< 16 bit RGB565 */
	STM_BLITTER_SF_RGB24      = 0x00000002, /*!< 24 bit RGB888 */
	STM_BLITTER_SF_RGB32      = 0x00000003, /*!< 32 bit RGBx888 */
	STM_BLITTER_SF_ARGB1555   = 0x00000004 | STM_BLITTER_SF_ALPHA,
						/*!< 16 bit ARGB1555 */
	STM_BLITTER_SF_ARGB4444   = 0x00000005 | STM_BLITTER_SF_ALPHA,
						/*!< 16 bit ARGB4444 */
	STM_BLITTER_SF_ARGB8565   = 0x00000006 | STM_BLITTER_SF_ALPHA,
						/*!< 24 bit ARGB8565 */
	STM_BLITTER_SF_ARGB       = 0x00000007 | STM_BLITTER_SF_ALPHA,
						/*!< 32 bit ARGB8888 */
	STM_BLITTER_SF_BGRA       = 0x00000008 | STM_BLITTER_SF_ALPHA,
						/*!< 32 bit BGRA8888 */
	STM_BLITTER_SF_LUT8       = 0x00000009 | (STM_BLITTER_SF_INDEXED
						  | STM_BLITTER_SF_ALPHA),
						/*!< 8 bit with LUT */
	STM_BLITTER_SF_LUT4       = 0x0000000a | STM_BLITTER_SF_INDEXED,
						/*!< 4 bit with LUT */
	STM_BLITTER_SF_LUT2       = 0x0000000b | STM_BLITTER_SF_INDEXED,
						/*!< 2 bit with LUT */
	STM_BLITTER_SF_LUT1       = 0x0000000c | STM_BLITTER_SF_INDEXED,
						/*!< 1 bit with LUT */
	STM_BLITTER_SF_ALUT88     = 0x0000000d | (STM_BLITTER_SF_INDEXED
						  | STM_BLITTER_SF_ALPHA),
						/*!< 16 bit with alpha and
						  LUT */
	STM_BLITTER_SF_ALUT44     = 0x0000000e | (STM_BLITTER_SF_INDEXED
						  | STM_BLITTER_SF_ALPHA),
						/*!< 8 bit with alpha and LUT */
	STM_BLITTER_SF_A8         = 0x0000000f | STM_BLITTER_SF_ALPHA,
						/*!< 8 bit alpha */
	STM_BLITTER_SF_A1         = 0x00000010 | STM_BLITTER_SF_ALPHA,
						/*!< 1 bit alpha */

	/* YCbCr formats */
	STM_BLITTER_SF_AVYU       = 0x00000011 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_ALPHA),
						/*!< YCbCr 32 bit raster AVYU
						  4:4:4, in stmfb this is
						  SURF_ACRYCB8888 */
	STM_BLITTER_SF_VYU        = 0x00000012 | STM_BLITTER_SF_YCBCR,
						/*!< YCbCr 24 bit raster VYU
						  4:4:4, in stmfb this is
						  SURF_CRYCB888 */

	STM_BLITTER_SF_YUY2       = 0x00000013 | STM_BLITTER_SF_YCBCR,
						/*!< YCbCr 16 bit raster
						4:2:2 */
	STM_BLITTER_SF_YUYV       = STM_BLITTER_SF_YUY2, /*!< alias for
						#STM_BLITTER_SF_YUY2, in stmfb
						this is SURF_YUYV */

	STM_BLITTER_SF_UYVY       = 0x00000014 | STM_BLITTER_SF_YCBCR,
						/*!< YCbCr 16 bit raster 4:2:2,
						  in stmfb this is
						  SURF_YCBCR422R */

	STM_BLITTER_SF_YV12       = 0x00000015 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR3),
						/*!< YCbCr 3 planes 4:2:0, in
						  stmfb this is SURF_YVU420 */
	STM_BLITTER_SF_I420       = 0x00000016 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR3),
						/*!< YCbCr 3 planes 4:2:0, in
						  stmfb this is SURF_YUV420 */
	STM_BLITTER_SF_YV16       = 0x00000017 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR3),
						/*!< YCbCr 3 planes 4:2:2, in
						  stmfb this is SURF_YVU422 */
	STM_BLITTER_SF_YV61       = 0x00000018 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR3),
						/*!< YCbCr 3 planes 4:2:2, in
						  stmfb this is SURF_YUV422P */

	STM_BLITTER_SF_YCBCR444P  = 0x00000019 | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR3),
						/*!< YCbCr 3 planes 4:4:4 */

	STM_BLITTER_SF_NV12       = 0x0000001a | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:0 */
	STM_BLITTER_SF_NV21       = 0x0000001b | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:0 */
	STM_BLITTER_SF_NV16       = 0x0000001c | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:2 */
	STM_BLITTER_SF_NV61       = 0x0000001d | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:2 */

	STM_BLITTER_SF_YCBCR420MB = 0x0000001e | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:0, in
						  stmfb this is
						  SURF_YCBCR420MB */
	STM_BLITTER_SF_YCBCR422MB = 0x0000001f | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:2:0, in
						  stmfb this is
						  SURF_YCBCR422MB */

	/* formats with limited alpha range */
	STM_BLITTER_SF_AlRGB8565  = 0x00000020 | STM_BLITTER_SF_ALPHA_LIMITED,
						/*!< 24 bit ARGB8565. This
						  differs from
						  #STM_BLITTER_SF_ARGB8565 in
						  that the alpha component has
						  a limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */
	STM_BLITTER_SF_AlRGB      = 0x00000021 | STM_BLITTER_SF_ALPHA_LIMITED,
						/*!< 32 bit ARGB8888. This
						  differs from
						  #STM_BLITTER_SF_ARGB in that
						  the alpha component has a
						  limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */
	STM_BLITTER_SF_BGRAl      = 0x00000022 | STM_BLITTER_SF_ALPHA_LIMITED,
						/*!< 32 bit BGRA8888. This
						  differs from
						  #STM_BLITTER_SF_BGRA in that
						  the alpha component has a
						  limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */
	STM_BLITTER_SF_AlLUT88    = 0x00000023 | (STM_BLITTER_SF_ALPHA_LIMITED
						  | STM_BLITTER_SF_INDEXED),
						/*!< 16 bit with Alpha and
						  LUT. This differs from
						  #STM_BLITTER_SF_ALUT88 in
						  that the alpha component has
						  a limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */
	STM_BLITTER_SF_Al8        = 0x00000024 | STM_BLITTER_SF_ALPHA_LIMITED,
						/*!< 8 bit alpha. This differs
						  from #STM_BLITTER_SF_A8 in
						  that the alpha component has
						  a limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */
	STM_BLITTER_SF_AlVYU      = 0x00000025 | STM_BLITTER_SF_ALPHA_LIMITED,
						/*!< YCbCr 32 bit raster AVYU
						  4:4:4. This differs from
						  #STM_BLITTER_SF_AVYU in that
						  the alpha component has a
						  limited range of 0 to 0x80
						  (instead of the full range
						  0 to 0xff). */

	/* more formats */
	STM_BLITTER_SF_BGR24      = 0x00000026, /*!< 24 bit BGR888 */

	STM_BLITTER_SF_RLD_BD     = 0x00000027, /*!< RLD (BD)
						  \note Currently not
						  implemented! */
	STM_BLITTER_SF_RLD_H2     = 0x00000028, /*!< RLD (HD-DVD)
						  \note Currently not
						  implemented! */
	STM_BLITTER_SF_RLD_H8     = 0x00000029, /*!< RLD (HD-DVD)
						  \note Currently not
						  implemented! */
	STM_BLITTER_SF_NV24       = 0x0000002a | (STM_BLITTER_SF_YCBCR
						  | STM_BLITTER_SF_PLANAR),
						/*!< YCbCr 2 planes 4:4:4 */
	STM_BLITTER_SF_ABGR       = 0x0000002b | STM_BLITTER_SF_ALPHA,
						/*!< 32 bit ABGR8888 */


	/* FIXME: incomplete */

	STM_BLITTER_SF_COUNT      = 0x0000002c /*!< number of surface
						formats */
} stm_blitter_surface_format_t;

/*!
 * allows to easily create a table (array) of surface formats with
 * indexes
 * \hideinitializer
 *
 * \apis #stm_blitter_surface_format_t
 */
#define STM_BLITTER_SF_MASK(sf)   ((sf) & 0x0000003f) /*!<  */

/*!
 * \brief This specifies a color space.
 *
 * \apis ::stm_blitter_surface_new_preallocated()
 */
typedef enum stm_blitter_surface_colorspace_e {
	STM_BLITTER_SCS_RGB,             /*!< full RGB range */
	STM_BLITTER_SCS_RGB_VIDEORANGE,  /*!< video range RGB */
	STM_BLITTER_SCS_BT601,           /*!< BT.601 (video range) */
	STM_BLITTER_SCS_BT601_FULLRANGE, /*!< BT.601 (full range) */
	STM_BLITTER_SCS_BT709,           /*!< BT.709 (video range) */
	STM_BLITTER_SCS_BT709_FULLRANGE, /*!< BT.709 (full range) */
} stm_blitter_surface_colorspace_t;

/*!
 * \brief This specifies a color space.
 *
 * \apis ::stm_blitter_surface_set_color_colorspace()
 */
typedef enum stm_blitter_color_colorspace_e {
	STM_BLITTER_COLOR_RGB,             /*!< RGB range */
	STM_BLITTER_COLOR_YUV,             /*!< YUV range */
} stm_blitter_color_colorspace_t;

/*!
 * \brief This specifies a Porter/Duff rule.
 *
 * \apis ::stm_blitter_surface_set_porter_duff(), #STM_BLITTER_SDF_BLEND,
 * #STM_BLITTER_SBF_BLEND_ALPHACHANNEL, #STM_BLITTER_SBF_BLEND_COLORALPHA
 */
typedef enum stm_blitter_porter_duff_rule_e {
	STM_BLITTER_PD_CLEAR,       /*!< clear */
	STM_BLITTER_PD_SOURCE,      /*!< source */
	STM_BLITTER_PD_DEST,        /*!< destination */
	STM_BLITTER_PD_SOURCE_OVER, /*!< source over destination */
	STM_BLITTER_PD_DEST_OVER,   /*!< destination over source */
	STM_BLITTER_PD_SOURCE_IN,   /*!< source in destination */
	STM_BLITTER_PD_DEST_IN,     /*!< destination in source */
	STM_BLITTER_PD_SOURCE_OUT,  /*!< source out destination */
	STM_BLITTER_PD_DEST_OUT,    /*!< destination out source */
	STM_BLITTER_PD_SOURCE_ATOP, /*!< source atop destination */
	STM_BLITTER_PD_DEST_ATOP,   /*!< destination atop source */
	STM_BLITTER_PD_XOR,         /*!< source xor destination */

	STM_BLITTER_PD_NONE,        /*!< the rule used by DirectFB by default.
					 \note this is not a Porter/Duff rule
					 and is not supported by the hardware
					 or driver. */
} stm_blitter_porter_duff_rule_t;

/*!
 * \brief All available drawing flags.
 * \hideinitializer
 *
 * \apis ::stm_blitter_surface_set_drawflags()
 */
typedef enum stm_blitter_surface_drawflags_e {
	STM_BLITTER_SDF_NONE            = 0x00000000, /*!< nothing */
	STM_BLITTER_SDF_DST_COLORKEY    = 0x00000001, /*!< only write if dst
					pixel matches the dst color key */
	STM_BLITTER_SDF_XOR             = 0x00000002, /*!< xor dst with color
					(after premultiply) (mutually
					exclusive with blend) */
	STM_BLITTER_SDF_GRADIENT        = 0x00000004, /*!< enable gradient fill
					using the color and color stop.
					\note Currently not implemented! */
	STM_BLITTER_SDF_BLEND           = 0x00000008, /*!< blend using alpha
							 from color */
	STM_BLITTER_SDF_SRC_PREMULTIPLY = 0x00000010, /*!< modulate color with
							 alpha from color */
	STM_BLITTER_SDF_DST_PREMULTIPLY = 0x00000020, /*!< modulate dst color
							 with alpha from dst */
	STM_BLITTER_SDF_ANTIALIAS       = 0x00000040, /*!< anti-aliasing.
					\note Currently not implemented! */

	STM_BLITTER_SDF_ALL             = 0x0000007f, /*!< all drawing flags */
} stm_blitter_surface_drawflags_t;

/*!
 * \brief All available blitting flags.
 * \hideinitializer
 *
 * \apis ::stm_blitter_surface_set_blitflags()
 */
typedef enum stm_blitter_surface_blitflags_e {
	STM_BLITTER_SBF_NONE                  = 0x00000000, /*!< nothing */

	STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT = 0x00000001, /*!< src x/y is
						specified in 16.16 fixed
						point */
	STM_BLITTER_SBF_ALL_IN_FIXED_POINT    = (0x00000002
				| STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT),
				/*!< all coordinates and sizes
				     are specified in 16.16 fixed point*/

	STM_BLITTER_SBF_ANTIALIAS             = 0x00000004,
						/*!< anti-aliasing.
						\note Currently not
						implemented! */

	STM_BLITTER_SBF_SRC_COLORIZE          = 0x00000008, /*!< modulate the
						src color with the matrix'
						color.
						For standard YCbCr <-> RGB
						conversions the matrix given
						here will be used in addition
						(if enabled) to the matrix
						used internally by the driver
						to achieve the color
						conversion. */

	STM_BLITTER_SBF_SRC_COLORKEY          = 0x00000020, /*!< don't blit src
						pixels that match the source
						color key */
	STM_BLITTER_SBF_DST_COLORKEY          = 0x00000040, /*!< only write if
						dst pixel matches the dst
						color key */

	STM_BLITTER_SBF_COLORMASK             = 0x00000080, /*!< plane mask in
						BDisp.
						\note Currently not
						implemented! */

	STM_BLITTER_SBF_XOR                   = 0x00000100, /*!< xor src with
						dst (mutually exclusive with
						blend) */

	STM_BLITTER_SBF_BLEND_ALPHACHANNEL    = 0x00000200, /*!< blend using
						alpha from src */
	STM_BLITTER_SBF_BLEND_COLORALPHA      = 0x00000400, /*!< blend using
						alpha from color */
	STM_BLITTER_SBF_SRC_PREMULTCOLOR      = 0x00000800, /*!< modulate src
						color with alpha from color */
	STM_BLITTER_SBF_SRC_PREMULTIPLY       = 0x00001000, /*!< modulate src
						color with (modulated) alpha
						from src */
	STM_BLITTER_SBF_DST_PREMULTIPLY       = 0x00002000, /*!< modulate dst
						color with alpha from dst */

	STM_BLITTER_SBF_READ_SOURCE2          = 0x00004000, /*!< instead of
							       dst */

	STM_BLITTER_SBF_FLIP_HORIZONTAL       = 0x00008000, /*!< flip image
							       horizontally */
	STM_BLITTER_SBF_FLIP_VERTICAL         = 0x00010000, /*!< flip image
							       vertically */
	STM_BLITTER_SBF_ROTATE90              = 0x00020000, /*!< anti-clockwise
						rotation by 90 degrees */
	STM_BLITTER_SBF_ROTATE180             = (STM_BLITTER_SBF_FLIP_HORIZONTAL
				| STM_BLITTER_SBF_FLIP_VERTICAL),
				/*!< 180 degree rotation, this is short for
				     (#STM_BLITTER_SBF_FLIP_HORIZONTAL
				      | #STM_BLITTER_SBF_FLIP_VERTICAL) */
	STM_BLITTER_SBF_ROTATE270             = 0x00040000, /*!< anti-clockwise
						rotation by 270 degrees */

	STM_BLITTER_SBF_VC1RANGE_LUMA         = 0x00080000, /*!< enable VC1
						range mapping on luma.
						\note Currently not
						implemented! */
	STM_BLITTER_SBF_VC1RANGE_CHROMA       = 0x00100000, /*!< enable VC1
						range mapping on chroma.
						\note Currently not
						implemented! */

	STM_BLITTER_SBF_NO_FILTER             = 0x00200000, /*!< disable
						filtering during rescale
						operations. This can only be
						regarded as a hint, disabling
						filtering does not make sense
						for (downsampled) YCbCr 4:2:x
						formats, and filtering is
						enforced to off for 1:1 LUT
						blits. */

	STM_BLITTER_SBF_DEINTERLACE_TOP       = 0x00400000, /*!< enable
						de-interlacing of the top
						field during current
						operations. */

	STM_BLITTER_SBF_DEINTERLACE_BOTTOM    = 0x00800000, /*!< enable
						de-interlacing of the bottom
						field during current
						operations. */

	STM_BLITTER_SBF_FLICKER_FILTER        = 0x01000000, /*!< enable
						anti flicker filter
						during current
						operations. */

	STM_BLITTER_SBF_STRICT_INPUT_RECT     = 0x02000000, /*!< Force
						usage of input rectangle as
						bitmap boundary for filtering
						during current
						operations. */

	/* TODO: do we need BDisp's clip mask operations??? */

	STM_BLITTER_SBF_ALL                   = 0x03ffffef, /*!< all blitting
						flags */
} stm_blitter_surface_blitflags_t;

/*!
 * \brief The clipping mode.
 *
 * \apis ::stm_blitter_surface_set_clip()
 */
typedef enum stm_blitter_clip_mode_e {
	STM_BLITTER_CM_CLIP_INSIDE, /*!< only write inside the clipping region
				       specified (inclusive) */
	STM_BLITTER_CM_CLIP_OUTSIDE, /*!< only write outside of clipping region
					specified (exclusive).
					\note Currently not implemented! */
} stm_blitter_clip_mode_t;

/*!
 * \brief colorkey mode
 * \hideinitializer
 *
 * The color key mode allows to specify the polarity of the colorkey. It also
 * allows to specify which color channels should be compared to the color key.
 *
 * \note For YCbCr formats, R corresponds to Cr, G corresponds to Y and B
 * corresponds to Cb.
 * For indexed formats, the blue channel corresponds to the index.
 *
 * \apis ::stm_blitter_surface_set_src_colorkey(),
 * ::stm_blitter_surface_set_dst_colorkey(),
 * #STM_BLITTER_SBF_SRC_COLORKEY, #STM_BLITTER_SBF_DST_COLORKEY,
 * #STM_BLITTER_CKM_RGB, #STM_BLITTER_CKM_RGB_INV
 */
typedef enum stm_blitter_colorkey_mode_e {
	STM_BLITTER_CKM_R_IGNORE   = 0x00000000, /*!< ignore red channel */
	STM_BLITTER_CKM_R_ENABLE   = 0x00000001, /*!< enable red channel */
	STM_BLITTER_CKM_R_INVERTED = (0x00000002 | STM_BLITTER_CKM_R_ENABLE),
						 /*!< invert meaning on red
						   channel */
	STM_BLITTER_CKM_G_IGNORE   = 0x00000000, /*!< ignore green channel */
	STM_BLITTER_CKM_G_ENABLE   = 0x00000010, /*!< enable green channel */
	STM_BLITTER_CKM_G_INVERTED = (0x00000020 | STM_BLITTER_CKM_G_ENABLE),
						 /*!< invert meaning on green
						   channel */
	STM_BLITTER_CKM_B_IGNORE   = 0x00000000, /*!< ignore blue channel */
	STM_BLITTER_CKM_B_ENABLE   = 0x00000100, /*!< enable blue channel */
	STM_BLITTER_CKM_B_INVERTED = (0x00000200 | STM_BLITTER_CKM_B_ENABLE),
						 /*!< invert meaning on blue
						   channel */

	STM_BLITTER_CKM_ALL        = 0x00000333, /*!< mask of all valid modes */

/*!
 * macro to enable colorkey on all channels
 * \hideinitializer
 *
 * \apis #stm_blitter_colorkey_mode_t
 * */
#define STM_BLITTER_CKM_RGB     (STM_BLITTER_CKM_R_ENABLE   \
				 | STM_BLITTER_CKM_G_ENABLE \
				 | STM_BLITTER_CKM_B_ENABLE)
/*!
 * macro to enable inverted colorkey on all channels
 * \hideinitializer
 *
 * \apis #stm_blitter_colorkey_mode_t
 * */
#define STM_BLITTER_CKM_RGB_INV (STM_BLITTER_CKM_R_INVERTED   \
				 | STM_BLITTER_CKM_G_INVERTED \
				 | STM_BLITTER_CKM_B_INVERTED)
} stm_blitter_colorkey_mode_t;


typedef enum stm_blitter_flicker_filter_mode_e {
	STM_BLITTER_FF_SIMPLE          = 0x00000000,
	STM_BLITTER_FF_ADAPTATIVE      = 0x00000001,
	STM_BLITTER_FF_FIELD_NOT_FRAME = 0x00000002,

	STM_BLITTER_FF_ALL             = 0x00000003,
}stm_blitter_flicker_filter_mode_t;


/*!
 * \defgroup device Device handle related APIs.
 *
 * These APIs allow one to retrieve a device handle that can later be used to
 * draw and blit.
 */

/*!
 * \ingroup device
 *
 * Creates a new blitter object that can be used by stm-blitter APIs.
 *
 * \param[in] id Blitter instance identifier: 0..n
 *
 * \return stm_blitter_h The return value can either be an error code
 * or a blitter object. To distinguish between the two, use IS_ERR() on the
 * result and query the actual error using PTR_ERR().
 *
 * \retval stm_blitter_h blitter
 * \retval IS_ERR() PTR_ERR()
 */
stm_blitter_h
stm_blitter_get(int id);

/*!
 * \ingroup device
 *
 * Creates a new blitter object that can be used by stm-blitter APIs.
 *
 * \note This API is similar to #stm_blitter_get(), it simply has a slightly
 * modified signature.
 *
 * \param[in]  id      Blitter instance identifier: 0..n
 * \param[out] blitter Pointer to created Blitter instance
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_get_device(int            id,
		       stm_blitter_h *blitter);

/*!
 * \ingroup device
 *
 * Decrement reference count for blitter instance.
 *
 * \param[in] handle Blitter instance to decrement reference count on.
 */
void
stm_blitter_put(stm_blitter_h handle);

/*!
 * \ingroup device
 *
 * Block until the given blitter has completed certain queued operations.
 *
 * \param[in] handle Blitter instance.
 * \param[in] type   Type of wait.
 * \param[in] serial Serial number of operation to wait for.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_wait(stm_blitter_h        handle,
		 stm_blitter_wait_t   type,
		 stm_blitter_serial_t serial);

/*!
 * \ingroup device
 *
 * Get the serial number of the last operation on this blitter instance. As
 * none of the drawing / copying operations block for completion, this can be
 * used to wait for an operation to finish by calling stm_blitter_wait().
 *
 * \param[in]  handle Blitter instance.
 * \param[out] serial Pointer to where the serial number should be stored.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_get_serial(stm_blitter_h         handle,
		       stm_blitter_serial_t *serial);


/*!
 * \defgroup surfaces Surface related APIs.
 *
 * Add some documentation to surface.
 */

/*!
 * \ingroup surfaces
 *
 * Creates a new surface object that can be used by stm-blitter APIs. The
 * memory described by the surface object must be valid and must have been
 * pre-allocated by the caller.
 *
 * \param[in] format         The surface's pixel format.
 * \param[in] colorspace     The surface's color space.
 * \param[in] buffer_address The surface's physical addresses.
 * \param[in] buffer_size    The size of the buffer in bytes.
 * \param[in] size           The dimension of the surface in width and height.
 * \param[in] stride         The stride or pitch, that is the number of bytes
 *                           to get from one line to the next.
 *
 * \return stm_blitter_surface_h The return value can either be an error code
 * or a surface object. To distinguish between the two, use IS_ERR() on the
 * result and query the actual error using PTR_ERR().
 *
 * \retval stm_blitter_surface_h surface
 * \retval IS_ERR() PTR_ERR()
 */
stm_blitter_surface_h
stm_blitter_surface_new_preallocated(
		stm_blitter_surface_format_t         format,
		stm_blitter_surface_colorspace_t     colorspace,
		const stm_blitter_surface_address_t *buffer_address,
		unsigned long                        buffer_size,
		const stm_blitter_dimension_t       *size,
		unsigned long                        stride);

/*!
 * \ingroup surfaces
 *
 * Creates a new surface object that can be used by stm-blitter APIs. The
 * memory described by the surface object must be valid and must have been
 * pre-allocated by the caller.
 *
 * \note This API is similar to #stm_blitter_surface_new_preallocated(), it
 * simply has a slightly modified signature.
 *
 * \param[in] format         The surface's pixel format.
 * \param[in] colorspace     The surface's color space.
 * \param[in] buffer_address The surface's physical addresses.
 * \param[in] buffer_size    The size of the buffer in bytes.
 * \param[in] size           The dimension of the surface in width and height.
 * \param[in] stride         The stride or pitch, that is the number of bytes
 *                           to get from one line to the next.
 * \param[out] surface       Pointer to the created surface object.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_get(stm_blitter_surface_format_t         format,
			stm_blitter_surface_colorspace_t     colorspace,
			const stm_blitter_surface_address_t *buffer_address,
			unsigned long                        buffer_size,
			const stm_blitter_dimension_t       *size,
			unsigned long                        stride,
			stm_blitter_surface_h               *surface);

/*!
 * \ingroup surfaces
 *
 * Increment reference count for surface.
 *
 * \param[in] surface Surface to increment reference count on.
 *
 * \retval stm_blitter_surface_h surface
 */
stm_blitter_surface_h
stm_blitter_surface_ref(stm_blitter_surface_h surface);

/*!
 * \ingroup surfaces
 *
 * Decrement reference count for surface.
 *
 * \param[in] surface Surface to decrement reference count on.
 */
void
stm_blitter_surface_put(stm_blitter_surface_h surface);

/*!
 * \ingroup surfaces
 *
 * Decrement reference count for surface.
 * \deprecated Use ::stm_blitter_surface_put() instead
 *
 * \param[in] surface Surface to decrement reference count on.
 */
static inline void
stm_blitter_surface_release(stm_blitter_surface_h surface)
__attribute__((deprecated));
static inline void
stm_blitter_surface_release(stm_blitter_surface_h surface)
{
	stm_blitter_surface_put(surface);
}

/*!
 * \ingroup surfaces
 *
 * Update the \a surface's physical address.
 *
 * \param[in] surface        Surface to update address of.
 * \param[in] buffer_address New physical address.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_update_address(
		stm_blitter_surface_h                surface,
		const stm_blitter_surface_address_t *buffer_address);


/*!
 * \defgroup state State related APIs.
 *
 * All drawing and blitting APIs work by setting a certain state on a surface,
 * e.g. drawing color, that is later used during e.g. the rectangle fill
 * operation.
 * This means it is best to queue similar operations in one go, without having
 * to change state often.
 */

/*!
 * \ingroup state
 *
 * Set the clipping region \a clip on the given \a surface. The clipping region
 * \a clip is the region that limits the area in which any drawing or blitting
 * operations to this \a surface have an effect.
 * The clipping region \a clip can either specify an inside region, i.e. all
 * pixels within the region are affected (including the region), or an outside
 * region, i.e. all pixel outside the region are affected. This is specified
 * using the \a mode.
 * \a Clip can be NULL to denote the whole surface's size.
 *
 * \param[in] surface Surface to set clipping region on.
 * \param[in] clip    Clipping rectangle.
 * \param[in] mode    Clipping mode.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_clip(stm_blitter_surface_h       surface,
			     const stm_blitter_region_t *clip,
			     stm_blitter_clip_mode_t     mode);

/*!
 * \ingroup state
 *
 * Set the Porter/Duff rule for a blending operation on the given \a surface.
 *
 * \param[in] surface Surface to set Porter/Duff rule on.
 * \param[in] pd      Porter/Duff rule.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_porter_duff(stm_blitter_surface_h          surface,
				    stm_blitter_porter_duff_rule_t pd);

/*!
 * \ingroup state
 *
 * Set the \a color for a (rectangle) fill operation on the given \a surface,
 * or the start color for a gradient fill operation at coordinates { 0, 0 }, or
 * the global alpha to be used when #STM_BLITTER_SBF_BLEND_COLORALPHA is in
 * effect.
 *
 * \param[in] surface Surface to set color on.
 * \param[in] color   Color.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_color(stm_blitter_surface_h      surface,
			      const stm_blitter_color_t *color);

/*!
 * \ingroup state
 *
 * Set the \a color index for a (rectangle) fill operation on the given \a
 * surface.
 *
 * \param[in] surface Surface to set color on.
 * \param[in] color   Color index.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_color_index(stm_blitter_surface_t *surface,
				    uint8_t                index);
/*!
 * \ingroup state
 *
 * Set the \a color space of the color being using for the (rectangle) fill
 * operation on the given \a surface,
 *
 * \param[in] surface Surface to set color color space.
 * \param[in] color   Colorspace.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_color_colorspace(stm_blitter_surface_t *surface,
				stm_blitter_color_colorspace_t  colorspace);

/*!
 * \ingroup state
 *
 * Set the \a color at the given coordinate \a offset for a gradient fill
 * operation on the given \a surface.
 *
 * \note At most one coordinate can currently be set.
 *
 * \note Currently not implemented!
 *
 * \param[in] surface Surface to set color stop on.
 * \param[in] offset  Coordinates for color stop.
 * \param[in] color   Color.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_color_stop(stm_blitter_surface_h      surface,
				   const stm_blitter_point_t *offset,
				   const stm_blitter_color_t *color);

/*!
 * \ingroup state
 *
 * Set the color conversion \a matrix on the given \a surface.
 *
 * \param[in] surface Surface to set color conversion matrix on.
 * \param[in] matrix  Color conversion matrix.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_color_matrix(stm_blitter_surface_h             surface,
				     const stm_blitter_color_matrix_t *matrix);

/*!
 * \ingroup state
 *
 * Set the source color key (range) on the given \a surface. The source
 * color key denotes the (only) color that is excluded when copying from this
 * surface, if the source color key flag is enabled on the destination during
 * an operation.
 * For example, if a 'green' source color key is set, then all except the
 * 'green' pixels, are copied (in the default mode).
 * If the color key mode was 'inverted', then all 'green' pixels, and only
 * those, would be copied.
 *
 * \param[in] surface Surface to set source colorkey on.
 * \param[in] low     Low bounds.
 * \param[in] high    High bounds.
 * \param[in] mode    Mode of color key.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_src_colorkey(stm_blitter_surface_h        surface,
				     const stm_blitter_color_t   *low,
				     const stm_blitter_color_t   *high,
				     stm_blitter_colorkey_mode_t  mode);

/*!
 * \ingroup state
 *
 * Set the destination color key (range) on the given \a surface. The
 * destination color key denotes the (only) color that is affected when
 * blitting / drawing to this surface, if the destination color key flag is
 * enabled during an operation.
 * For example, if a 'green' destination color key is set, then all
 * 'green' pixels, and only those, are overwritten (in the default mode).
 * If the color key mode was 'inverted', then all except the 'green' pixels
 * would be modified.
 *
 * \note Destination color keying is currently not supported.
 *
 * \note Currently not implemented!
 *
 * \param[in] surface Surface to set destination colorkey on.
 * \param[in] low     Low bounds.
 * \param[in] high    High bounds.
 * \param[in] mode    Mode of color key.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_dst_colorkey(stm_blitter_surface_h        surface,
				     const stm_blitter_color_t   *low,
				     const stm_blitter_color_t   *high,
				     stm_blitter_colorkey_mode_t  mode);

/*!
 * \ingroup state
 *
 * Set Flicker Filter mode on the given \a surface.
 *
 * \param[in] surface      Surface on which to apply flicker filter.
 * \param[in] mode         Mode of Flicker Filter.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_ff_mode(stm_blitter_surface_h       surface,
				stm_blitter_flicker_filter_mode_t  mode);

/*!
 * \ingroup state
 *
 * Set VC1 range parameters on the given \a surface.
 *
 * \param[in] surface      Surface to set VC1 range on.
 * \param[in] luma_coeff   VC1 range luma coefficient.
 * \param[in] chroma_coeff VC1 range chroma coefficient.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_vc1_range(stm_blitter_surface_h surface,
				  unsigned char         luma_coeff,
				  unsigned char         chroma_coeff);

/*!
 * \ingroup state
 *
 * Set \a flags for all subsequent blitting / rectangle copy operations for the
 * given \a surface.
 *
 * \param[in] surface Surface to set blitting flags on.
 * \param[in] flags   Blitting flags.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_blitflags(stm_blitter_surface_h           surface,
				  stm_blitter_surface_blitflags_t flags);

/*!
 * \ingroup state
 *
 * Set \a flags for all subsequent drawing / rectangle fill operations for the
 * given \a surface.
 *
 * \param[in] surface Surface to set drawing flags on.
 * \param[in] flags   Drawing flags.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_set_drawflags(stm_blitter_surface_h           surface,
				  stm_blitter_surface_drawflags_t flags);


/*!
 * \defgroup drawblit Drawing and blitting APIs.
 */

/*!
 * \ingroup drawblit
 *
 * Get the serial number of the last operation on this \a surface. As none of
 * the operations block for completion, this can be used to wait for an
 * operation to finish by calling stm_blitter_wait().
 *
 * \param[in]  surface Surface to get serial number of operation from.
 * \param[out] serial  Pointer to where the serial number should be stored.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_get_serial(stm_blitter_surface_h  surface,
			       stm_blitter_serial_t  *serial);

/*!
 * \ingroup drawblit
 *
 * Set the fence flag on the \a surface, to append a fence node operation
 * after the next blitter operation. This fence operation can be used to
 * wait for long operations to finish without polling. To use it call
 * stm_blitter_wait() with the #STM_BLITTER_WAIT_FENCE wait type.
 *
 * \param[in]  surface Surface to set the fence flag on
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_add_fence(stm_blitter_surface_h surface);


/*!
 * \ingroup drawblit
 *
 * Clear (draw rectangle on) destination \a dst using the \a color and ignoring
 * any drawing flags (but not the clip).
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter Blitter handle.
 * \param[in] dst     Destination surface.
 * \param[in] color   The color.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_clear(stm_blitter_h              blitter,
			  stm_blitter_surface_h      dst,
			  const stm_blitter_color_t *color);

/*!
 * \ingroup drawblit
 *
 * Color fill one rectangle on destination \a dst. \a dst_rect may be NULL to
 * denote the whole destination surface's size.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter  Blitter handle.
 * \param[in] dst      Destination surface.
 * \param[in] dst_rect Destination rectangle.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_fill_rect(stm_blitter_h             blitter,
			      stm_blitter_surface_h     dst,
			      const stm_blitter_rect_t *dst_rect);

/*!
 * \ingroup drawblit
 *
 * Color fill \a count rectangles on destination. If \a count == 1, then
 * \a dst_rects may be NULL to denote the whole destination surface's size.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter   Blitter handle.
 * \param[in] dst       Destination surface.
 * \param[in] dst_rects Destination rectangle(s).
 * \param[in] count     Number of rectangles to draw.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_fill_rects(stm_blitter_h             blitter,
			       stm_blitter_surface_h     dst,
			       const stm_blitter_rect_t *dst_rects,
			       unsigned int              count);

/*!
 * \ingroup drawblit
 *
 * Color fill one rectangle on destination \a dst.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter   Blitter handle.
 * \param[in] dst       Destination surface.
 * \param[in] dst_x     Destination x coordinate.
 * \param[in] dst_y     Destination y coordinate.
 * \param[in] dst_w     Destination width.
 * \param[in] dst_h     Destination height.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_fill_rect2(stm_blitter_h         blitter,
			       stm_blitter_surface_h dst,
			       long                  dst_x,
			       long                  dst_y,
			       long                  dst_w,
			       long                  dst_h);

/*!
 * \ingroup drawblit
 *
 * Draw one rectangle outline on destination \a dst. \a dst_rect may be NULL to
 * denote the whole destination surface's size.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter  Blitter handle.
 * \param[in] dst      Destination surface.
 * \param[in] dst_rect Destination rectangle.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_draw_rect(stm_blitter_h             blitter,
			      stm_blitter_surface_h     dst,
			      const stm_blitter_rect_t *dst_rect);

/*!
 * \ingroup drawblit
 *
 * Draw \a count rectangle outlines on destination \a dst. If \a count == 1,
 * then \a dst_rects may be NULL to denote the whole destination surface's
 * size.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter   Blitter handle.
 * \param[in] dst       Destination surface.
 * \param[in] dst_rects Destination rectangle(s).
 * \param[in] count     Number of rectangles to draw.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_draw_rects(stm_blitter_h             blitter,
			       stm_blitter_surface_h     dst,
			       const stm_blitter_rect_t *dst_rects,
			       unsigned int              count);


/*!
 * \ingroup drawblit
 *
 * Blit \a count rectangles from source \a src to destination \a dst. \a src
 * and \a dst may be the same surface. If \a count == 1, then \a src_rects may
 * be NULL to denote the whole source surface's size, similarly, \a dst_points
 * may be NULL in this case to denote point { 0, 0 }.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter    Blitter handle.
 * \param[in] src        Source surface.
 * \param[in] src_rects  Rectangle(s) to be transferred.
 * \param[in] dst        Destination surface.
 * \param[in] dst_points Coordinates of destination rectangle(s).
 * \param[in] count      Number of rectangles to transfer.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_blit(stm_blitter_h              blitter,
			 stm_blitter_surface_h      src,
			 const stm_blitter_rect_t  *src_rects,
			 stm_blitter_surface_h      dst,
			 const stm_blitter_point_t *dst_points,
			 unsigned int               count);

/*!
 * \ingroup drawblit
 *
 * Blit (with resize if necessary) \a count rectangles from source \a src to
 * destination \a dst. \a src and \a dst may be the same surface. If \a count
 * == 1, then \a src_rects and / or \a dst_rects may be NULL to denote the
 * whole source or destination surface's size.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter   Blitter handle.
 * \param[in] src       Source surface.
 * \param[in] src_rects Rectangle(s) to be transferred.
 * \param[in] dst       Destination surface.
 * \param[in] dst_rects Destination rectangle(s).
 * \param[in] count     Number of rectangles to transfer.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_stretchblit(stm_blitter_h             blitter,
				stm_blitter_surface_h     src,
				const stm_blitter_rect_t *src_rects,
				stm_blitter_surface_h     dst,
				const stm_blitter_rect_t *dst_rects,
				unsigned int              count);

/*!
 * \ingroup drawblit
 *
 * Blit \a count rectangles from the two sources \a src \a src2 to the
 * destination \a dst. This is similar to stm_blitter_surface_blit() except
 * that the second source \a src2 will be used for reading instead of the
 * destination \a dst. \a src, \a src2 and \a dst may be the same surface.
 * If \a count == 1, then \a src_rects may be NULL to denote the whole
 * source surface's size, similarly, \a src2_points and / or \a dst_points
 * may be NULL in this case to denote point { 0, 0 }.
 *
 * \note The call does not block for the operation to complete.
 *
 * \param[in] blitter     Blitter handle.
 * \param[in] src         Source surface one.
 * \param[in] src_rects   Rectangle(s) to be transferred.
 * \param[in] src2        Source surface two.
 * \param[in] src2_points Coordinates of rectangle(s) to be transferred from
 *                        source two.
 * \param[in] dst         Destination surface.
 * \param[in] dst_points  Coordinates of destination rectangle(s).
 * \param[in] count       Number of rectangles to transfer.
 *
 * \retval 0 success
 * \retval <0 error
 */
int
stm_blitter_surface_blit_two(stm_blitter_h              blitter,
			     stm_blitter_surface_h      src,
			     const stm_blitter_rect_t  *src_rects,
			     stm_blitter_surface_h      src2,
			     const stm_blitter_point_t *src2_points,
			     stm_blitter_surface_h      dst,
			     const stm_blitter_point_t *dst_points,
			     unsigned int               count);



/* compatibility macros for existing code, they will be removed at some
   point */
#define BLITTER_DEPRECATED \
		__attribute__((deprecated("please use var_t or struct var_s " \
					  "for variable declaration")))

#define stm_blitter              stm_blitter_s BLITTER_DEPRECATED
#define stm_blitter_surface      stm_blitter_surface_s BLITTER_DEPRECATED
#define stm_blitter_point        stm_blitter_point_s BLITTER_DEPRECATED
#define stm_blitter_dimension    stm_blitter_dimension_s BLITTER_DEPRECATED
#define stm_blitter_rect         stm_blitter_rect_s BLITTER_DEPRECATED
#define stm_blitter_region       stm_blitter_region_s BLITTER_DEPRECATED
#define stm_blitter_color        stm_blitter_color_s BLITTER_DEPRECATED
#define stm_blitter_palette      stm_blitter_palette_s BLITTER_DEPRECATED
#define stm_blitter_point        stm_blitter_point_s BLITTER_DEPRECATED

#define stm_blitter_color_matrix \
			stm_blitter_color_matrix_s BLITTER_DEPRECATED
#define stm_blitter_surface_address \
			stm_blitter_surface_address_s BLITTER_DEPRECATED
#define  stm_blitter_surface_format \
			stm_blitter_surface_format_e BLITTER_DEPRECATED
#define  stm_blitter_surface_colorspace \
			stm_blitter_surface_colorspace_e BLITTER_DEPRECATED
#define  stm_blitter_porter_duff_rule \
			stm_blitter_porter_duff_rule_e BLITTER_DEPRECATED
#define  stm_blitter_surface_drawflags \
			stm_blitter_surface_drawflags_e BLITTER_DEPRECATED
#define  stm_blitter_surface_blitflags \
			stm_blitter_surface_blitflags_e BLITTER_DEPRECATED
#define  stm_blitter_clip_mode \
			stm_blitter_clip_mode_e BLITTER_DEPRECATED
#define  stm_blitter_colorkey_mode \
			stm_blitter_colorkey_mode_e BLITTER_DEPRECATED


#endif /* __STM_BLITTER_H__ */
