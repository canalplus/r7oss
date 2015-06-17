/***********************************************************************
 *
 * File: include/stm_display_source.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_SOURCE_H
#define _STM_DISPLAY_SOURCE_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_source.h
 *  \brief C interface to image sources
 *
 *  Sources provide pixel data to one or more image processors. Sources
 *  may represent a real-time digital pixel stream generated from some video
 *  input capture or a means of reading pixel data from in-memory buffers.
 */

#include "stm_display_source_pixel_stream.h"
#include "stm_display_source_queue.h"

/*! \enum  stm_display_source_interfaces_t
 *  \brief Optional interfaces that can be queried from an image source
 *
 *  Depending on the underlying type of the image source, it will implement
 *  one or more of these interfaces.
 */
typedef enum stm_display_source_interfaces_e
{
  STM_SOURCE_QUEUE_IFACE,           /*!< Interface to an image buffer queue                     */
  STM_SOURCE_PIXELSTREAM_IFACE      /*!< Interface to a real time pixel stream control          */
} stm_display_source_interfaces_t;

typedef struct stm_display_pixel_stream_params_s
{

    uint16_t source_type;
    uint16_t instance_number;
    stm_display_source_pixelstream_params_t worst_case_timing;
} stm_display_pixel_stream_params_t;

typedef struct stm_display_source_interface_params_s
{
    stm_display_source_interfaces_t interface_type;
    union
    {
        uint32_t        *reserved;
        stm_display_pixel_stream_params_t  *pixel_stream_params;
    }interface_params;
} stm_display_source_interface_params_t;

typedef enum stm_display_source_ctrl_e
{
    SOURCE_CTRL_CLOCK_ADJUSTMENT,
    SOURCE_CTRL_NOISE_MEASUREMENT_TYPE,
    SOURCE_CTRL_NOISE_MEASUREMENT_UNIT,
    SOURCE_CTRL_NOISE_MEASUREMENT_VALUE
} stm_display_source_ctrl_t;

typedef enum stm_display_source_caps_e
{
   CAPS_NONE,
   CAPS_FORMAT_3D_FRAME_SEQ = (1L<<0),
   CAPS_FORMAT_3D_STACKED_FRAME = (1L<<1),
   CAPS_FORMAT_3D_FIELD_ALTERNATE = (1L<<2),
   CAPS_FORMAT_3D_PICTURE_INTERLEAVE = (1L<<3),
   CAPS_FORMAT_3D_SIDEBYSIDE_FULL = (1L<<4),
   CAPS_FORMAT_3D_STACKED_HALF = (1L<<5),
   CAPS_FORMAT_3D_SIDEBYSIDE_HALF = (1L<<6),
   CAPS_FORMAT_3D_L_D = (1L<<7),
   CAPS_FORMAT_3D_L_D_G_GMINUSD = (1L<<8),
   CAPS_FORMAT_3D_HORIZ_SAMPLING = (1L<<9),
   CAPS_FORMAT_3D_QUINQUNX_SAMPLING = (1L<<10),
} stm_display_source_caps_t;

/*****************************************************************************
 * C interface to image sources
 *
 */


/*!
 * Get the value of a specified control.
 *
 * \param s         Source to query.
 * \param ctrl      Control identifier to get.
 * \param ctrl_val  Pointer to the control value variable to be filled in.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 * \returns -ENOTSUP  The control is not supported by the source.
 */
extern int stm_display_source_get_control(stm_display_source_h s, stm_display_source_ctrl_t ctrl, uint32_t * ctrl_val);

/*!
 * Get the name of an image source.
 *
 * \param s     Source to query.
 * \param name  A pointer to the name of the source.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 */
extern int stm_display_source_get_name(stm_display_source_h s, const char **name);

/*!
 * Get the numeric ID of a source's parent device.
 * A device handle can then be retrieved by a call to stm_display_get_device_id().
 *
 * \param s  Source to query.
 * \param id Pointer to the variable to fill in.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 */
extern int stm_display_source_get_device_id(stm_display_source_h s, uint32_t *id);

/* System infrastructure and platform integration */

/*!
 * Get a unique implementation dependent non-zero numeric identifier for the
 * video timing generator controlled by a source.
 *
 *
 * \param s          source to query
 * \param timing_id  The returned timing identifier
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The source handle was invalid
 * \returns -EFAULT  The timing_id pointer is invalid
 * \returns -ENOTSUP The source is not connected to a video timing generator
 *
 */
extern int stm_display_source_get_timing_identifier(stm_display_source_h s, uint32_t *timing_id);

/*!
 * This function gets the numeric IDs of planes connected to this source.
 * The order is not significant. The "stm_display_plane_h" type handle can then
 * be retrieved by a call to stm_display_device_open_plane (device, id).
 * This function can be used to query the actual number of connected planes.
 * This is done by setting a NULL pointer as id. In that case, max_ids can be
 * null also.
 *
 * \param s       Source to query.
 * \param id      Pointer to an array to fill in with planes' IDs.
 * \param max_ids Max number of elements in the array.
 *
 * \returns 0..n      Actual number of planes.If id is not NULL, this is also
 *                    the actual number of plane ids written to the array.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 */
extern int stm_display_source_get_connected_plane_id(stm_display_source_h s, uint32_t *id, uint32_t max_ids);

/*!
 * This function gets the list of capabilities of all planes currently
 * connected to this source.
 *
 * \param s         Source to query.
 * \param caps      Pointer to an array to fill with the capabilities of each
 *                  planes this source is connected to.
 * \param max_caps  Max number of elements in the array passed by the
 *                  application and the function returns the maximum number of
 *                  elements that would be available.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 * \returns -1        The call failed.
 */
extern int stm_display_source_get_connected_plane_caps(stm_display_source_h s, stm_plane_capabilities_t * caps, uint32_t max_caps);

/*!
 * Get an interface pointer supported by this image source.
 *
 * \param s          Source to query.
 * \param iface_params    Interface type and the worst case constraints for
 *                        the interface type are passed in as input parameters.
 * \param iface_handle    Handle to the returned interface type is returned.
 *                        Pointer to the handle is passed in as input.
 *                        Depending on the interface type specified through
 *                        iface_type, the handle can be either
 *                        stm_display_source_queue_t * or a
 *                        stm_display_source_pixelstream_t *
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 */
extern int stm_display_source_get_interface(stm_display_source_h s, stm_display_source_interface_params_t iface_params, void **iface_handle);

/*!
 * This is to be called by the platform specific first level interrupt handlers
 * to handle all hardware interrupts associated with the source.
 *
 * \param s          Source to query.
 */
extern void stm_display_source_handle_interrupts(stm_display_source_h s);

/*!
 * Get Information about the last VSync for the source, i.e. top/bottom field
 * in interlaced modes and the time interval between the last two VSyncs. It is
 * only defined for sources that own a timing generator, the result of calling
 * this on other outputs is undefined.
 *
 * If a source is stopped then \a field will be set to STM_UNKNOWN_FIELD and
 * \a interval set to 0.
 *
 * This can be called from interrupt context, therefore it neither takes
 * any locks nor does it return errors.
 *
 *
 * \param s        Source to query.
 * \param field    Pointer to field variable to fill in
 * \param interval Pointer to the vsync interval variable to fill in
 *
 */
extern void stm_display_source_get_last_timing_event(stm_display_source_h s, stm_display_timing_events_t *field, stm_time64_t *interval);

/*!
 * Release the given source handle.
 *
 * \note The pointer is invalid after this call completes successfully.
 *       Calling this function with a NULL handle is allowed and will not
 *       cause an error or system failure.
 *
 * \param s Source handle to release
 *
 */
extern void stm_display_source_close(stm_display_source_h s);

/*!
 * Set the value of a specified control.
 *
 * \param s Source to query.
 * \param ctrl      Control identifier to change.
 * \param new_val   The new control value.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -ERANGE   The control value was out of range.
 * \returns -ENOTSUP  The control is not supported by the source.
 */
extern int stm_display_source_set_control(stm_display_source_h s, stm_display_source_ctrl_t ctrl, uint32_t new_val);

/*!
 * This function gets an OR'd bit mask of the capabilities listed below that
 * are supported by a source object. This is intended to allow a component to
 * determine that the source supports certain functionality.:
 *
 * \param s     Source to query.
 * \param caps  The returned or'd capabilities.
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 */
extern int stm_display_source_get_capabilities(stm_display_source_h s, stm_display_source_caps_t *caps);

/*!
 * This function gets the current source status flags, an Or'd value of STM_STATUS_*
 *
 * \param s         Source to query.
 * \param status    Pointer to the status variable to fill in
 *
 * \returns 0         On success.
 * \returns -EINVAL   The device handle was invalid.
 * \returns -EFAULT   The parameter pointer is an invalid address.
 * \returns -EINTR    The call was interrupted while obtaining the device lock.
 */
extern int stm_display_source_get_status(stm_display_source_h s, uint32_t *status);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_SOURCE_H */
