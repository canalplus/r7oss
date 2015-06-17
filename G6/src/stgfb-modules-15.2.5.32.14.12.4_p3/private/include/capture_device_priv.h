/***********************************************************************
 *
 * File: private/include/capture_device_priv.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef PIXEL_CAPTURE_DEVICE_PRIV_H
#define PIXEL_CAPTURE_DEVICE_PRIV_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct stm_pixel_capture_hw_features_s
{
  int chroma_hsrc;
} stm_pixel_capture_hw_features_t;

typedef enum stm_pixel_capture_content_type_e {
  STM_PIXEL_CAPTURE_CONTENT_TYPE_UNKNOWN,
  STM_PIXEL_CAPTURE_CONTENT_TYPE_GRAPHICS,
  STM_PIXEL_CAPTURE_CONTENT_TYPE_PHOTO,
  STM_PIXEL_CAPTURE_CONTENT_TYPE_CINEMA,
  STM_PIXEL_CAPTURE_CONTENT_TYPE_GAME
} stm_pixel_capture_content_type_t;

typedef struct stm_i_push_get_sink_get_desc
{
  /** Parameters set by the caller (user) */
  uint32_t                            width;                /*!< \Image width in pixel */
  uint32_t                            height;               /*!< \Image height in pixels */
  stm_pixel_capture_format_t          format;               /*!< \Video format */

  /** Parameters set by the callee (allocator) */
  uint32_t                            pitch;                /*!< \Length of a raster or luma line in byte */
  uint32_t                            video_buffer_addr ;   /*!< \Hardware bus address of video buffer */
  uint32_t                            chroma_buffer_offset; /*!< \Start of chroma in non interleaved YUV */
  void                               *allocator_data;       /*!< \Allocator private data */
} stm_i_push_get_sink_get_desc_t;

typedef struct stm_i_push_get_sink_push_desc
{
  struct stm_i_push_get_sink_get_desc buffer_desc;          /*!< \Descriptor allocated through get_buffer call */
  uint32_t                            flags;                /*!< \From enum stm_pixel_capture_flags_e, interlacing info */
  stm_pixel_capture_color_space_t     color_space;          /*!< \From enum stm_pixel_capture_color_space_e, color space info */
  stm_pixel_capture_rational_t        src_frame_rate;       /*!< \Source Frame Rate */
  stm_pixel_capture_content_type_t    content_type;         /*!< \Content type of the captured buffer */
  stm_pixel_capture_rational_t        pixel_aspect_ratio;   /*!< \Pixel aspect ratio */
  stm_pixel_capture_time_t            captured_time;        /*!< \Time of the first byte capture */
  int                                 content_is_valid;     /*!< \Flag stating if the content can be displayed. Buffer flushed are reported with flag=0 */
} stm_i_push_get_sink_push_desc_t;


/*****************************************************************************
 * C private interface for pixel capture device
 */

/*!
 * Register the pixel capture device power hooks.
 *
 * \param pixel_capture    A handle to the pixel capture object to check
 * \param get              A pointer to the power pm_runtime_get() function
 * \param put              A pointer to the power pm_runtime_put() function
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The device handle was invalid
 * \returns -EFAULT The power hooks parameter pointers are an invalids addresses
 */
extern int stm_pixel_capture_device_register_pm_runtime_hooks(stm_pixel_capture_device_type_t type, uint32_t instance, int (*get)(const uint32_t type, const uint32_t id), int (*put)(const uint32_t type, const uint32_t id));


/*!
 * Check the given pixel capture device handle validty using the registry
 * database.
 *
 * \param pixel_capture    A handle to the pixel capture object to check
 * \param type             The object type to be checked.
 *
 * \returns false   The pixel capture device handle is not valid
 * \returns true    The pixel capture device handle is valid
 */
extern bool stm_pixel_capture_is_handle_valid(const stm_pixel_capture_h pixel_capture,
                          stm_object_h type);


/*!
 * Open a pixel capture device.
 *
 * \param type             Capture device type to query.
 * \param instance         Capture device instance to query.
 * \param pixel_capture    A pointer to the capture's handle
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_device_open(stm_pixel_capture_device_type_t type, const uint32_t instance,
                                    stm_pixel_capture_h *pixel_capture);


/*!
 * Close the pixel capture device.
 *
 * \param pixel_capture  Capture to query
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_device_close(const stm_pixel_capture_h pixel_capture);


/*!
 * Freeze the given pixel capture device. This may cause the API
 * implementation to shut down all of its resources. Ressources are not
 * released and they are putted in reset state after this call completes.
 *
 * \param pixel_capture    A handle to the pixel capture object to freeze
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The pixel capture handle was invalid.
 */
extern int stm_pixel_capture_device_freeze(stm_pixel_capture_h pixel_capture);


/*!
 * Suspend the given pixel capture device. This may cause the API
 * implementation to shut down all of its resources. Ressources are not
 * released and they are putted in reset state after this call completes.
 *
 * \param pixel_capture    A handle to the pixel capture object to suspend
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The pixel capture handle was invalid.
 */
extern int stm_pixel_capture_device_suspend(stm_pixel_capture_h pixel_capture);


/*!
 * Resume the given pixel capture device. This may cause the API
 * implementation to restart all of its resources. Some of the ressources
 * are not restarted with the original previous configuration as they
 * were putted in reset state.
 *
 * \param pixel_capture    A handle to the pixel capture object to resume
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The pixel capture device handle was invalid.
 */
extern int stm_pixel_capture_device_resume(stm_pixel_capture_h pixel_capture);


/*!
 * Get the pixel capture device power status.
 *
 * \param pixel_capture    Pixel Capture device to query.
 * \param pm_state         A pointer to the power state's value
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The pixel capture device handle was invalid
 * \returns -EFAULT The power status parameter pointer is an invalid address
 */
extern int stm_pixel_capture_device_get_power_state(stm_pixel_capture_h pixel_capture, uint32_t *pm_state);


/*!
 * Check if the pixel capture device is suspended or not.
 *
 * \param pixel_capture    Pixel Capture device to query.
 *
 * \returns false   The pixel capture device is active
 * \returns true    The pixel capture device is not active
 */
extern bool stm_pixel_capture_device_is_suspended(stm_pixel_capture_h pixel_capture);


/*!
 * Hardware interrupt processing specific to this capture.
 *
 * The intended use is for this to be called by the OS platform specific
 * component that provides PixelCapture’s working environment (i.e. its OS
 * abstraction layer implementation and other system integration). It will be
 * called from the first level interrupt handlers registered with the operating
 * system by that platform component.
 *
 * \caps None
 *
 * \param pixel_capture    Pixel Capture device to query.
 * \param timingevent      A pointer to the timing event value
 * 
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The pixel capture device handle was invalid
 * \returns -EFAULT The timingevent status parameter pointer is an invalid address
 */
extern int stm_pixel_capture_handle_interrupts(stm_pixel_capture_h pixel_capture,
                            uint32_t * timingevent);

#if defined(__cplusplus)
}
#endif

#endif /* PIXEL_CAPTURE_DEVICE_PRIV_H */
