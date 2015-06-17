/***********************************************************************
 *
 * File: include/stm_display_source_pixel_stream.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_SOURCE_PIXEL_STREAM_H
#define _STM_DISPLAY_SOURCE_PIXEL_STREAM_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_source_pixel_stream.h
 *  \brief C interface to image source real-time pixel streams
 *
 */

/*!
 * \brief A source pixelstream interface handle, do not look inside.
 */
typedef struct stm_display_source_pixelstream_s *stm_display_source_pixelstream_h;

typedef enum stm_display_source_pixelstream_flags_e
{
    STM_PIXELSTREAM_SRC_INTERLACED = (1L<<0),
    STM_PIXELSTREAM_SRC_COLORSPACE_709 = (1L<<1),
    STM_PIXELSTREAM_SRC_ALPHA_6BIT = (1L<<2),
    STM_PIXELSTREAM_SRC_TRICKMODE = (1L<<3),
    STM_PIXELSTREAM_SRC_FIELD_REPEAT = (1L<<4),
    STM_PIXELSTREAM_SRC_CAPTURE_BASED_ON_DE = (1L<<5),
    STM_PIXELSTREAM_SRC_PC_GFX = (1L<<6),
    STM_PIXELSTREAM_SRC_VERTICAL_FIELD_ALIGN = (1L<<7),
    STM_PIXELSTREAM_SRC_UV_ALIGN = (1L<<8),
} stm_display_source_pixelstream_flags_t;

typedef enum stm_display_source_pixelstream_ColorType_e
{
    STM_PIXELSTREAM_SRC_RGB,
    STM_PIXELSTREAM_SRC_YUV_422,
    STM_PIXELSTREAM_SRC_YUV_444,
} stm_display_source_pixelstream_ColorType_t;

typedef enum stm_display_source_pixelstream_signal_status_e
{
    PIXELSTREAM_SOURCE_STATUS_STABLE,    /* Indicator regarding the system source stable state */
    PIXELSTREAM_SOURCE_STATUS_UNSTABLE,  /* Indicator regarding the system source stable state */
}stm_display_source_pixelstream_signal_status_t;

typedef struct stm_display_source_pixelstream_params_s
{
    uint32_t                        htotal;
    uint32_t                        vtotal;
    stm_rect_t                      active_window;
    stm_rect_t                      visible_area;
    uint32_t                        src_frame_rate;
    uint32_t                        colordepth;
    stm_rational_t                  pixel_aspect_ratio;
    stm_display_source_pixelstream_flags_t  flags;
    stm_display_source_pixelstream_ColorType_t colorType;
    stm_display_3d_video_t          config_3D;
} stm_display_source_pixelstream_params_t;


/*****************************************************************************
 * C interface to real-time pixel stream sources
 *
 */

/*!
 * Set the pixelstream source status to the given value according to the system state.
 *
 * \note The pointer is invalid after this call completes successfully.
 *
 * \param p      Pixelstream interface to set the control on.
 *
 * \param status The new status value.
 *
 * \return  0      on success
 *         -EINVAL The pixelstream source handle was invalid.
 *         -ERANGE The control value was out of range.
 *         -EINTR  The call was interrupted while obtaining the device lock.
 */
extern int stm_display_source_pixelstream_set_signal_status(stm_display_source_pixelstream_h p,
                                                            stm_display_source_pixelstream_signal_status_t status);

/*!
 * Set the params fully describing the Pixelstream source.
 *
 * \note The pointer is invalid after this call completes successfully.
 *
 * \param p      Pixelstream interface to set the control on.
 *
 * \param params Input params to set.
 *
 * \return  0           On success.
 *          -ENOLCK     If the device lock cannot be obtained.
 *          -EINTR      The call was interrupted while obtaining the device lock.
 *          -ENOTSUP    No support for the functionality.
 */

int stm_display_source_pixelstream_set_input_params(stm_display_source_pixelstream_h p,
                                                    const stm_display_source_pixelstream_params_t * params);


/*!
 * Release the given pixel stream interface.
 *
 * \note The pointer is invalid after this call completes successfully.
 *
 * \param p Interface handle to release.
 *
 * \returns 0 on success or -1 if the device lock cannot be obtained.
 *
 */
extern int stm_display_source_pixelstream_release(stm_display_source_pixelstream_h p);

/*!
 * Set the params fully describing the pixel stream source.
 *
 * \param p Pixelstream interface to set the control on.
 * \param params Input params to set.
 *
 * \returns 0 on success or -1 if the device lock cannot be obtained or the
 *         functionality is not supported.
 *
 */
extern int stm_display_source_pixelstream_set_input_params(stm_display_source_pixelstream_h p, const stm_display_source_pixelstream_params_t * params);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_SOURCE_PIXEL_STREAM_H */
