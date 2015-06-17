/***********************************************************************
 *
 * File: stm_pixel_capture.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STMPIXELCAPTURE_H
#define STMPIXELCAPTURE_H

#include <stm_registry.h>
#include <stm_event.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_pixel_capture.h
 *  \brief C interface to pixel capture objects
 */


/*! \enum    stm_pixel_capture_events_e
 *  \brief   Event issued when there is a new buffer produced.
 */
typedef enum stm_pixel_capture_events_e
{
  STM_PIXEL_CAPTURE_EVENT_NEW_BUFFER  = 1,     /*!< \new buffer were produced */
} stm_pixel_capture_events_t;



/*! \enum    stm_pixel_capture_time_t
 *  \brief   Represent the time (as it is used in stkpi)
 *  \apis    ::stm_pixel_capture_queue_buffer()
 *           ::stm_pixel_capture_dequeue_buffer()
 */
typedef int64_t stm_pixel_capture_time_t;


/*! \enum    stm_pixel_capture_device_type_t
 *  \brief   Represent the type of device (as it is used in stkpi)
 *  \apis    ::stm_pixel_capture_open()
 */
typedef enum stm_pixel_capture_device_type_e
{
	STM_PIXEL_CAPTURE_COMPO,
	STM_PIXEL_CAPTURE_FVDP,
	STM_PIXEL_CAPTURE_DVP,

}stm_pixel_capture_device_type_t;


/*! \enum    stm_pixel_format_e
 *  \brief   Lists the pixel capture formats
 *  \apis    ::stm_pixel_capture_try_format()
 *           ::stm_pixel_capture_get_format()
 *           ::stm_pixel_capture_set_format()
 */
typedef enum stm_pixel_capture_format_e
{
  STM_PIXEL_FORMAT_NONE,
  STM_PIXEL_FORMAT_RGB565,                                              /*!< Single plane, supported by compositor */
  STM_PIXEL_FORMAT_RGB888,                                              /*!< Single plane, supported by compositor, fvdp (O2),  DVP */
  STM_PIXEL_FORMAT_RGB_8B8B8B_SP = STM_PIXEL_FORMAT_RGB888,             /*!< Single plane, supported by compositor, fvdp (O2),  DVP (same as STM_PIXEL_FORMAT_RGB888) renamed for more descriptive name */
  STM_PIXEL_FORMAT_ARGB1555,                                            /*!< Supported by compositor */
  STM_PIXEL_FORMAT_ARGB8565,                                            /*!< Supported by compositor */
  STM_PIXEL_FORMAT_ARGB8888,                                            /*!< Supported by compositor */
  STM_PIXEL_FORMAT_ARGB4444,                                            /*!< Supported by compositor */
  STM_PIXEL_FORMAT_YUV_NV12,                                            /*!< Dual   planes, supported by FVDP (O2)*/
  STM_PIXEL_FORMAT_YUV_NV16,                                            /*!< Dual   planes, supported by FVDP (O2), DVP */
  STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP     = STM_PIXEL_FORMAT_YUV_NV16,  /*!< Dual   planes, supported by FVDP (O2), DVP (same as STM_PIXEL_FORMAT_YUV_NV16) renamed for more descriptive name */
  STM_PIXEL_FORMAT_YUV,                                                 /*!< Single plane,  supported by compositor, DVP */
  STM_PIXEL_FORMAT_YUV_8B8B8B_SP          = STM_PIXEL_FORMAT_YUV,       /*!< Single plane,  supported by compositor, DVP (same as STM_PIXEL_FORMAT_YUV) renamed for more descriptive name */
  STM_PIXEL_FORMAT_YCbCr422R,                                           /*!< Single plane,  supported by compositor */
  STM_PIXEL_FORMAT_RGB_10B10B10B_SP,                                    /*!< Single plane,  10 bits format, supported by DVP */
  STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP,                                  /*!< Single plane,  10 bits format, supported by DVP */
  STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP,                               /*!< Dual   planes, 10 bits format, supported by DVP */

  STM_PIXEL_FORMAT_END,
  STM_PIXEL_FORMAT_COUNT = STM_PIXEL_FORMAT_END                         /*!< Must remain last */
} stm_pixel_capture_format_t;


/*! \enum    stm_pixel_capture_color_space_e
 *  \brief   List the color space variant suported by the pixel capture.
 *  \apis    ::stm_display_output_get_last_timing_event()
 */
typedef enum stm_pixel_capture_color_space_e
{
   STM_PIXEL_CAPTURE_RGB,
   STM_PIXEL_CAPTURE_RGB_VIDEORANGE,
   STM_PIXEL_CAPTURE_BT601,
   STM_PIXEL_CAPTURE_BT601_FULLRANGE,
   STM_PIXEL_CAPTURE_BT709,
   STM_PIXEL_CAPTURE_BT709_FULLRANGE
} stm_pixel_capture_color_space_t;



/*! \enum    stm_pixel_capture_flags_e
 *  \brief   Lists pixel capture flags.
 *  \apis    ::stm_pixel_capture_set_input_params()
 */
typedef enum stm_pixel_capture_flags_e
{
  STM_PIXEL_CAPTURE_BUFFER_INTERLACED   = 0x10000000,
  STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY     = 0x00000001 | STM_PIXEL_CAPTURE_BUFFER_INTERLACED,
  STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY  = 0x00000002 | STM_PIXEL_CAPTURE_BUFFER_INTERLACED,
  STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM   = 0x00000003 | STM_PIXEL_CAPTURE_BUFFER_INTERLACED,
  STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP   = 0x00000004 | STM_PIXEL_CAPTURE_BUFFER_INTERLACED,
  STM_PIXEL_CAPTURE_BUFFER_3D           = 0x20000000,
} stm_pixel_capture_flags_t;



/*! \enum    stm_pixel_capture_field_polarity_t
 *  \brief   Lists pixel capture field polarities flags
 *  \apis    ::stm_pixel_capture_set_input_params()
 *           ::stm_pixel_capture_set_input_params()
 */
typedef enum stm_pixel_capture_field_polarity_e {
  STM_PIXEL_CAPTURE_FIELD_POLARITY_HIGH,  /*!< \input field polarity is high */
  STM_PIXEL_CAPTURE_FIELD_POLARITY_LOW    /*!< \input field polarity is low */
} stm_pixel_capture_field_polarity_t;



/*! \struct  stm_pixel_capture_capabilities_flags_t
 *  \brief   Capabilities flags
 *  \apis    ::stm_pixel_capture_query_capabilities()
 */
typedef enum stm_pixel_capture_capabilities_flags_e
{
  STM_PIXEL_CAPTURE,      /*!< \captured for application purposes */
  STM_PIXEL_DISPLAY,      /*!< \captured for display purposes through display_kpi interfaces */
}stm_pixel_capture_capabilities_flags_t;



/*! \enum    stm_pixel_capture_status_t
 *  \brief   Lists pixel capture status flags
 *  \apis    ::stm_pixel_capture_set_input_params()
 *           ::stm_pixel_capture_set_input_params()
 */
typedef enum stm_pixel_capture_status_e
{
  STM_PIXEL_CAPTURE_LOCKED    = 0x00000001,                               /*!< \The capture device is locked by the user
                                                                                and not available for any new assignment.
                                                                                If this flag is not set, then the pixel_capture
                                                                                is not locked and is available for locking by
                                                                                the application. */
  STM_PIXEL_CAPTURE_STARTED   = 0x00000002 | STM_PIXEL_CAPTURE_LOCKED,    /*!< \The capture device locked and the capture process
                                                                                has been started. */
  STM_PIXEL_CAPTURE_NO_BUFFER = 0x00000004 | STM_PIXEL_CAPTURE_STARTED,   /*!< \The capture device is in the started state. However
                                                                                it does not have any buffer to capture/store the video. */
  STM_PIXEL_CAPTURE_UNDERFLOW = 0x00000008 | STM_PIXEL_CAPTURE_STARTED,   /*!< \The capture device is in started state. However the
                                                                                pixel stream being captured has stopped with buffer
                                                                                not being captured (underflow of buffer from user's
                                                                                perspective). */
  STM_PIXEL_CAPTURE_OVERFLOW  = 0x00000010 | STM_PIXEL_CAPTURE_STARTED,   /*!< \The capture device is in started state. However, the
                                                                                captured buffers are not being read by the user at the
                                                                                desired rate, leading to oldest captured buffer
                                                                                overwritten. (overflow of buffers from the user's perspective). */
} stm_pixel_capture_status_t;




/*! \struct  stm_pixel_capture_format_s
 *  \brief   Pixel capture buffer format.
 *  \apis    ::stm_pixel_capture_try_format()
 *           ::stm_pixel_capture_get_format()
 *           ::stm_pixel_capture_set_format()
 */
typedef struct stm_pixel_capture_buffer_format_s
{
  uint32_t                  width;
  uint32_t                  height;
  uint32_t                  stride;

  uint32_t                  flags;
  stm_pixel_capture_color_space_t color_space;
  stm_pixel_capture_format_t      format;
}stm_pixel_capture_buffer_format_t;



/*! \struct  stm_pixel_capture_buffer_descr_s
 *  \brief   Pixel capture buffer description.
 *  \apis    ::stm_pixel_capture_queue_buffer()
 *           ::stm_pixel_capture_dequeue_buffer()
 */
typedef struct stm_pixel_capture_buffer_descr_s
{
  uint32_t          bytesused;                /*!< \number of bytes occupied by data in the buffer (payload) */
  uint32_t          length;                   /*!< \size in bytes of the buffer (NOT its payload) */

  stm_pixel_capture_buffer_format_t cap_format;

  union
  {
    uint32_t rgb_address; /*!< Base address for RGB buffer */
    struct
    {
      uint32_t luma_address; /*!< Base address for raster buffer, or Y buffer */
      uint32_t chroma_offset;                      /*!< Chroma buffer offset */
    };
  };

  stm_pixel_capture_time_t  captured_time;    /*!< \time of the first byte capture */
                                              /*!< \time to configure the display time */
} stm_pixel_capture_buffer_descr_t;



/*! \struct  stm_pixel_capture_rect_t
 *  \brief   Representation of a (rectangle based on the upper left point reference)
 *  \apis    ::stm_pixel_capture_get_input_window()
 *           ::stm_pixel_capture_set_input_window()
 */
typedef struct stm_pixel_capture_rect_s
{
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
}stm_pixel_capture_rect_t;



/*! \struct  stm_pixel_capture_rational_t
 *  \brief   Representation of a rational number
 *  \apis    ::stm_pixel_capture_set_input_params()
 *           ::stm_pixel_capture_set_input_params()
 */
typedef struct stm_pixel_capture_rational_s
{
  int32_t  numerator;
  int32_t  denominator;
} stm_pixel_capture_rational_t;



/*! \struct stm_pixel_capture_input_window_capabilities_t
 *  \brief   Pixel capture input window capabilities
 *  \apis    ::stm_pixel_capture_get_input_window_capabilities()
 */
typedef struct stm_pixel_capture_input_window_capabilities_s
{
  stm_pixel_capture_rect_t max_input_window_area;
  stm_pixel_capture_rect_t default_input_window_area;
}stm_pixel_capture_input_window_capabilities_t;


/*! \enum    stm_pixel_capture_params_t
 *  \brief   Streaming parameters
 *  \apis    ::stm_pixel_capture_set_input_params()
 *           ::stm_pixel_capture_set_input_params()
 */
typedef struct stm_pixel_capture_params_s
{
  uint32_t timeperframe;
}stm_pixel_capture_params_t;



/*! \enum    stm_pixel_capture_input_params_t
 *  \brief   Streaming parameters
 *  \apis    ::stm_pixel_capture_set_input_params()
 *           ::stm_pixel_capture_glet_input_params()
 */
typedef struct stm_pixel_capture_input_params_s
{
  uint32_t            htotal;
  uint32_t            vtotal;
  stm_pixel_capture_rect_t    active_window;
  uint32_t            src_frame_rate;
  stm_pixel_capture_rational_t  pixel_aspect_ratio;
  uint32_t            flags;
  stm_pixel_capture_color_space_t color_space;
  stm_pixel_capture_format_t pixel_format;
  stm_pixel_capture_field_polarity_t vsync_polarity;
  stm_pixel_capture_field_polarity_t hsync_polarity;
} stm_pixel_capture_input_params_t;


/*! \struct  stm_pixel_capture_h
 * \brief    A handle to pixel capture device, do not look inside.
 */
typedef struct stm_pixel_capture_s *stm_pixel_capture_h;


/*****************************************************************************
 * C interface for pixel capture device
 */

/*!
 * Open a picture capture device.
 *
 * \param type              Capture device type to query.
 * \param instance         Capture device instance to query.
 * \param pixel_capture   A pointer to the capture's handle
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_open(stm_pixel_capture_device_type_t type, const uint32_t instance,
                                  stm_pixel_capture_h *pixel_capture);


/*!
 * Close the pixel capture device.
 *
 * \param pixel_capture  Capture to query
 *
 * \returns None
 *
 */
extern void stm_pixel_capture_close(const stm_pixel_capture_h pixel_capture);


/*!
 * Lock the use of the pixel capture device.
 *
 * \param pixel_capture  Capture to query
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 *
 */
extern int stm_pixel_capture_lock(const stm_pixel_capture_h pixel_capture);


/*!
 * Unlock the use of the pixel capture device.
 *
 * \param pixel_capture  Capture to query
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 *
 */
extern int stm_pixel_capture_unlock(const stm_pixel_capture_h pixel_capture);



/*!
 * Enumerate video inputs.
 *
 * \apis ::stm_pixel_capture_set_input()
 *       ::stm_pixel_capture_get_input()
 *
 * \param pixel_capture  Capture to query
 * \param input          Input id.
 * \param name           A pointer to the input's name
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid or if the input is not supported
 * \returns -EFAULT The input name parameter pointer is an invalid address
 */
extern int stm_pixel_capture_enum_inputs(const stm_pixel_capture_h pixel_capture,
                                         const uint32_t input,
                                         const char **name);


/*!
 * Gets a pointer to a list of supported image formats and places it in "formats".
 * The list itself is owned by the capture device and must not be modified.
 *
 * \param pixel_capture  Capture to query
 * \param formats        Pointer to the list pointer variable to fill in.
 *
 * \returns the number of formats in the returned list on success.
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid or if the input is not supported
 * \returns -EFAULT The formats parameter pointer is an invalid address
 */
extern int stm_pixel_capture_enum_image_formats(const stm_pixel_capture_h pixel_capture,
                                         const stm_pixel_capture_format_t **formats);


/*!
 * Select the input.
 *
 * \param pixel_capture  Capture to query
 * \param input          Input id.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_set_input(const stm_pixel_capture_h pixel_capture,
                                       const uint32_t input );



/*!
 * Retrieve the current input.
 *
 * \param pixel_capture  Capture to query
 * \param input          Pointer to the variable to fill in.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_input(const stm_pixel_capture_h pixel_capture,
                                       uint32_t *input);



/*!
 * Get the video input window capabilities
 *
 * \param pixel_capture  Capture to query
 * \param input_window_capability  Pointer to the input window capabilities to fill in.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_input_window_capabilities(const stm_pixel_capture_h pixel_capture,
                                           stm_pixel_capture_input_window_capabilities_t * input_window_caps);



/*!
 * Get the current input window
 *
 * \param pixel_capture  Capture to query
 * \param input_window  Pointer to the input window value to fill in.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_input_window(const stm_pixel_capture_h pixel_capture,
                                      stm_pixel_capture_rect_t * input_window);



/*!
 * Set the input window
 *
 * \param pixel_capture  Capture to query
 * \param input_window  Input window value.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_set_input_window(const stm_pixel_capture_h  pixel_capture,
                                      stm_pixel_capture_rect_t input_window);



/*!
 * Try a format
 *
 * \param pixel_capture  Capture to query
 * \param format         Pixel capture format.
 * \param supported      Pointer to a boolean variable set to true if supported
 *                       and false if is not supported.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_try_format(const stm_pixel_capture_h pixel_capture,
                                        const stm_pixel_capture_buffer_format_t format,
                                        bool *supported);



/*!
 * Set a format.
 * This also performs the required scaling ratio.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_set_format(const stm_pixel_capture_h pixel_capture,
                                        const stm_pixel_capture_buffer_format_t format);



/*!
 * Retrive the current format (configured or the default after initialization).
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_format(const stm_pixel_capture_h pixel_capture,
                                      stm_pixel_capture_buffer_format_t * const format);



/*!
 * Start to capture.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_start(const stm_pixel_capture_h pixel_capture);



/*!
 * Stop to capture
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_stop(const stm_pixel_capture_h pixel_capture);



/*!
 * Query the pixel capture for its capabilities.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_query_capabilities(const stm_pixel_capture_h pixel_capture,
                                        stm_pixel_capture_capabilities_flags_t *capabilities_flags);



/*
There are kept two buffer queues, an incoming and an outgoing queue.
They separate the synchronous capture operation locked to a video clock
from the application which is subject to other processes, thereby reducing
the probability of data loss. The queues are organized as FIFOs,
buffers will be output in the order enqueued in the incoming FIFO,
and were captured in the order dequeued from the outgoing FIFO.
*/


/*!
 * Add a buffer to the fifo to be used for the capture process.
 * It is part of the exchange buffer sequence between application and driver.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_queue_buffer(const stm_pixel_capture_h pixel_capture,
                                   stm_pixel_capture_buffer_descr_t *buffer);



/*!
 * Retrieve the buffer; it is part of the exchange buffer sequence between application and driver.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_dequeue_buffer(const stm_pixel_capture_h pixel_capture,
                                           stm_pixel_capture_buffer_descr_t *buffer);



/*!
 * Attach a sink to the pixel capture.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_attach(const stm_pixel_capture_h pixel_capture,
                             const stm_object_h sink);



/*!
 * Detach a sink from the pixel capture.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_detach(const stm_pixel_capture_h pixel_capture,
                             const stm_object_h sink);



/*!
 * Get the device status.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_status( const stm_pixel_capture_h pixel_capture,
                    stm_pixel_capture_status_t * status);




/*!
 * Set the the streaming parameters.
 * How to change the rate of buffer exchange between app and driver.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_set_stream_params(const stm_pixel_capture_h pixel_capture,
                                               stm_pixel_capture_params_t params);



/*!
 * Get the the streaming parameters.
 * What is the rate of buffer exchange between app and driver.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_stream_params(const stm_pixel_capture_h pixel_capture,
                                               stm_pixel_capture_params_t * params);



/*!
 * Set the input parameters.
 * Inform what the input paramaters are for the pixel capture device.
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_set_input_params( const stm_pixel_capture_h pixel_capture,
                            stm_pixel_capture_input_params_t params);




/*!
 * Get the input parameters.
 * What are the input paramaters are for the pixel capture device
 *
 * \param pixel_capture  Capture to query
 * \param format         Pointer to a pixel capture format.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The capture handle was invalid
 */
extern int stm_pixel_capture_get_input_params( const stm_pixel_capture_h pixel_capture,
                            stm_pixel_capture_input_params_t * params);




/*!
 * Update all programming, for hardware managed by a device that is clocked by
 * the provided video timing generator identifier, for the next video frame or
 * field.
 *
 * \param pixel_capture   capture device to update
 * \param timing_id       timing generator we are updating for.
 * \param vsyncTime       current VSync time in micoscondes.
 * \param timingevent     current VSync timing event.
 */
extern int stm_pixel_capture_device_update(const stm_pixel_capture_h pixel_capture,
                          uint32_t timing_id,
                          stm_pixel_capture_time_t vsyncTime,
                          uint32_t timingevent);


#if defined(__cplusplus)
}
#endif

#endif /* STMPIXELCAPTURE_H */
