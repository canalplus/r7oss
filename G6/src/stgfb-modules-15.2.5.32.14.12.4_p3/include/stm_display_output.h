/***********************************************************************
 *
 * File: include/stm_display_output.h
 * Copyright (c) 2000, 2004, 2005-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_OUTPUT_H
#define _STM_DISPLAY_OUTPUT_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "stm_display_modes.h"
#include "stm_analog_output.h"
#include "stm_hdmi_iframe.h"
#include "stm_hdmi_output.h"
#include "stm_lvds_output.h"
#include "stm_metadata.h"

/*! \file stm_display_output.h
 *  \brief C interface to output objects
 */

/*! \enum    stm_display_timing_events_e
 *  \brief   Timing generator interrupt reasons
 *  \apis    ::stm_display_output_get_last_timing_event()
 */
typedef enum stm_display_timing_events_e
{
  STM_TIMING_EVENT_NONE          = 0,       /*<! */
  STM_TIMING_EVENT_FRAME         = (1L<<1), /*<! */
  STM_TIMING_EVENT_TOP_FIELD     = (1L<<2), /*<! */
  STM_TIMING_EVENT_BOTTOM_FIELD  = (1L<<3), /*<! */
  STM_TIMING_EVENT_LEFT_EYE      = (1L<<4), /*<! */
  STM_TIMING_EVENT_RIGHT_EYE     = (1L<<5), /*<! */
  STM_TIMING_EVENT_REPEATED_EYE  = (1L<<6), /*<! */
  STM_TIMING_EVENT_LINE          = (1L<<7)  /*<! */
} stm_display_timing_events_t;


/*! \enum    stm_display_output_connection_status_e
 *  \brief   Output <-> display connection states
 *  \apis    ::stm_display_output_get_connection_status(),
 *           ::stm_display_output_set_connection_status()
 */
typedef enum stm_display_output_connection_status_e
{
  STM_DISPLAY_DISCONNECTED = 0, /*!< */
  STM_DISPLAY_CONNECTED,        /*!< */
  STM_DISPLAY_INACTIVE,         /*!< */
  STM_DISPLAY_NEEDS_RESTART     /*!< */
} stm_display_output_connection_status_t;


/*! \enum  stm_clock_ref_frequency_e
 *  \brief Defines an input crystal reference clock frequency, 27 or 30MHz.
 *  \apis  ::stm_display_output_set_clock_reference()
 */
typedef enum stm_clock_ref_frequency_e
{
  STM_CLOCK_REF_27MHZ, /*!< */
  STM_CLOCK_REF_30MHZ  /*!< */
} stm_clock_ref_frequency_t;


/*! \enum stm_output_control_e
 *  \brief Output control names
 *
 *  \apis ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_output_control_e
{
  /*
   * Generic output controls
   */
  OUTPUT_CTRL_MAX_PIXEL_CLOCK=1,           /*!< \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER   */
  OUTPUT_CTRL_CLOCK_ADJUSTMENT,            /*!< \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER   */
  OUTPUT_CTRL_VIDEO_SOURCE_SELECT,         /*!< */
  OUTPUT_CTRL_AUDIO_SOURCE_SELECT,         /*!< */
  OUTPUT_CTRL_VIDEO_OUT_SELECT,            /*!< */
  OUTPUT_CTRL_CLIP_SIGNAL_RANGE,           /*!< */
  OUTPUT_CTRL_BACKGROUND_ARGB,             /*!< \caps OUTPUT_CAPS_MIXER_BACKGROUND        */
  OUTPUT_CTRL_BACKGROUND_VISIBILITY,       /*!< \caps OUTPUT_CAPS_MIXER_BACKGROUND        */
  OUTPUT_CTRL_FORCED_RGB_VALUE,            /*!< */
  OUTPUT_CTRL_FORCE_COLOR,                 /*!< */
  OUTPUT_CTRL_YCBCR_COLORSPACE,            /*!< \caps OUTPUT_CAPS_PLANE_MIXER             */
  OUTPUT_CTRL_RGB_QUANTIZATION_CHANGE,     /*!< */
  OUTPUT_CTRL_EXTERNAL_SYNC_SHIFT,         /*!< \caps OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS   */
  OUTPUT_CTRL_EXTERNAL_SYNC_INVERT,        /*!< \caps OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS   */
  OUTPUT_CTRL_422_CHROMA_FILTER,           /*!< \caps OUTPUT_CAPS_422_CHROMA_FILTER       */
  OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,        /*!< display aspect ratio, active in automatic mode */

  /*
   * Analog output controls
   */
  OUTPUT_CTRL_BRIGHTNESS,                  /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_SATURATION,                  /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_CONTRAST,                    /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_HUE,                         /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_CVBS_TRAP_FILTER,            /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_LUMA_NOTCH_FILTER,           /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_CHROMA_SCALE,                /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_CHROMA_DELAY,                /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_IF_LUMA_DELAY,               /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_WSS_INSERTION,               /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_CC_INSERTION_ENABLE,         /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_SD_CGMS,                     /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_VPS,                         /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_TELETEXT_SYSTEM,             /*!< \caps OUTPUT_CAPS_SD_ANALOG         */
  OUTPUT_CTRL_DAC123_MAX_VOLTAGE,          /*!< \caps OUTPUT_CAPS_{SD|ED|HD}_ANALOG */
  OUTPUT_CTRL_DAC456_MAX_VOLTAGE,          /*!< \caps OUTPUT_CAPS_{SD|ED|HD}_ANALOG */
  OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS,     /*!< \caps OUTPUT_CAPS_{SD|ED|HD}_ANALOG */
  OUTPUT_CTRL_DAC_HD_POWER_DOWN,           /*!< \caps OUTPUT_CAPS_HD_ANALOG         */
  OUTPUT_CTRL_DAC_HD_ALT_FILTER,           /*!< \caps OUTPUT_CAPS_HD_ANALOG         */
  OUTPUT_CTRL_VIDEO_OUT_CALIBRATION,       /*!< \caps OUTPUT_CAPS_{SD|ED|HD}_ANALOG */
  OUTPUT_CTRL_DAC_POWER_DOWN,              /*!< \caps OUTPUT_CAPS_{SD|ED|HD}_ANALOG */

  /*
   * HDMI output controls
   */
  OUTPUT_CTRL_HDMI_AVMUTE,                  /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_ENABLE_HOTPLUG_INTERRUPT,     /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT,        /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR,      /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_AVI_VIC_SELECT,               /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT,    /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_AVI_QUANTIZATION_MODE,        /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_AVI_SCAN_INFO,                /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_AVI_CONTENT_TYPE,             /*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO,/*!< \caps OUTPUT_CAPS_HDMI */
  OUTPUT_CTRL_HDMI_PHY_CONF_TABLE,          /*!< \caps OUTPUT_CAPS_HDMI */

  /*
   * DVO output controls
   */
  OUTPUT_CTRL_DVO_INVERT_DATA_CLOCK,        /*!< \caps OUTPUT_CAPS_DVO_XXX    */
  OUTPUT_CTRL_DVO_ALLOW_EMBEDDED_SYNCS,     /*!< \caps OUTPUT_CAPS_DVO_XXX    */

  /*
   * LVDS output controls for TV panels
   */
  OUTPUT_CTRL_PANEL_CONFIGURE,              /*!< */
  OUTPUT_CTRL_PANEL_POWER,                  /*!< */
  OUTPUT_CTRL_PANEL_FLIP,                   /*!< */
  OUTPUT_CTRL_PANEL_WRITE_LOOKUP_TABLE,     /*!< */
  OUTPUT_CTRL_PANEL_READ_LOOKUP_TABLE,      /*!< */
  OUTPUT_CTRL_PANEL_LOOKUP_TABLE_ENABLE,    /*!< */
  OUTPUT_CTRL_PANEL_WRITE_COLOR_REMAP_TABLE,/*!< */
  OUTPUT_CTRL_PANEL_READ_COLOR_REMAP_TABLE, /*!< */
  OUTPUT_CTRL_PANEL_DITHER_MODE,            /*!< */
  OUTPUT_CTRL_PANEL_LOCK_METHOD,            /*!< */
  OUTPUT_CTRL_PANEL_FLAGLINE,               /*!< */

  /*
   * LED Backlight controls
   */
  OUTPUT_CTRL_LEDBLU_LOCAL_DIMMING,         /*!< */
  OUTPUT_CTRL_LEDBLU_BRIGHTNESS,            /*!< */
  OUTPUT_CTRL_LEDBLU_FLOOR_BRIGHTNESS,      /*!< */
  OUTPUT_CTRL_LEDBLU_CONFIGURE,             /*!< */
  OUTPUT_CTRL_LEDBLU_DATA,                  /*!< */
  OUTPUT_CTRL_LEDBLU_AMBIENT_LIGHT,         /*!< */
  OUTPUT_CTRL_LEDBLU_SCAN_BLIGHT,           /*!< */
  OUTPUT_CTRL_LEDBLU_READ_CTRL_RANGE,       /*!< */

  /*
   * DisplayPort output control
   */
  OUTPUT_CTRL_DPTX_AVMUTE,                  /*!< \caps OUTPUT_CAPS_DISPLAYPORT */

} stm_output_control_t;


/*! \enum    stm_display_output_video_source_e
 *  \brief   Control argument to set the output's video data source
 *  \ctrlarg OUTPUT_CTRL_VIDEO_SOURCE_SELECT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_display_output_video_source_e
{
  STM_VIDEO_SOURCE_MAIN_COMPOSITOR,         /*!< */
  STM_VIDEO_SOURCE_AUX_COMPOSITOR,          /*!< */
  STM_VIDEO_SOURCE_MAIN_COMPOSITOR_BYPASS,  /*!< */
  STM_VIDEO_SOURCE_AUX_COMPOSITOR_BYPASS,   /*!< */
} stm_display_output_video_source_t;


/*! \enum    stm_display_output_audio_source_e
 *  \brief   Control argument to set the output's audio data source
 *  \ctrlarg OUTPUT_CTRL_AUDIO_SOURCE_SELECT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_display_output_audio_source_e
{
  STM_AUDIO_SOURCE_NONE,    /*!< */
  STM_AUDIO_SOURCE_SPDIF,   /*!< */
  STM_AUDIO_SOURCE_2CH_I2S, /*!< */
  STM_AUDIO_SOURCE_4CH_I2S, /*!< */
  STM_AUDIO_SOURCE_6CH_I2S, /*!< */
  STM_AUDIO_SOURCE_8CH_I2S  /*!< */
} stm_display_output_audio_source_t;


/*! \enum    stm_video_output_format_e
 *  \brief   Control argument to set the output data format
 *  \ctrlarg OUTPUT_CTRL_VIDEO_OUT_SELECT
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_video_output_format_e
{
  STM_VIDEO_OUT_NONE       = 0,       /*<! Can be used to disable all active component, CVBS and
                                           S-Video DACs on an analog output                             */

  STM_VIDEO_OUT_RGB        = (1L<<0), /*<! RGB on component analog, HDMI, DP and DVO outputs            */
  STM_VIDEO_OUT_YUV        = (1L<<1), /*<! YUV on component analog (YPbPr), HDMI, DP and DVO outputs    */
  STM_VIDEO_OUT_YC         = (1L<<2), /*<! Y/C analog S-Video output                                    */
  STM_VIDEO_OUT_CVBS       = (1L<<3), /*<! CVBS analog output                                           */
  STM_VIDEO_OUT_422        = (1L<<4), /*<! YUV 4:2:2 sampling on digital output interfaces              */
  STM_VIDEO_OUT_444        = (1L<<5), /*<! YUV 4:4:4 sampling on digital output interfaces              */
  STM_VIDEO_OUT_DVI        = (1L<<6), /*<! DVI signaling used on HDMI output                            */
  STM_VIDEO_OUT_HDMI       = (1L<<7), /*<! HDMI signaling used on HDMI output                           */
  STM_VIDEO_OUT_ITUR656    = (1L<<8), /*<! 8bit wide DVO output supported only for YUV 4:2:2 sampling;
                                           the output data clock will be 2 x pixel clock.               */
  STM_VIDEO_OUT_420        = (1L<<9), /*<! YUV 4:2:0 sampling on digital output interfaces              */

  STM_VIDEO_OUT_16BIT      = (0L<<24),/*<! 16bit color DP output or 16bit wide DVO Output with
                                           either YUV 4:4:4 ( data clock = 2 x pixel clock)
                                           or YUV 4:2:2 sampling.                                       */
  STM_VIDEO_OUT_18BIT      = (1L<<24),/*<! 18bit color DP output                                        */
  STM_VIDEO_OUT_20BIT      = (2L<<24),/*<! 20bit color DP output                                        */
  STM_VIDEO_OUT_24BIT      = (3L<<24),/*<! 24bit color HDMI and DP output, or
                                           24bit wide RGB or YUV 4:4:4 DVO output                       */
  STM_VIDEO_OUT_30BIT      = (4L<<24),/*<! 30bit deepcolor HDMI and DP output                           */
  STM_VIDEO_OUT_36BIT      = (5L<<24),/*<! 36bit deepcolor HDMI and DP output                           */
  STM_VIDEO_OUT_48BIT      = (6L<<24),/*<! 48bit deepcolor HDMI output                                  */
  STM_VIDEO_OUT_DEPTH_MASK = (7L<<24)
} stm_video_output_format_t;


/*! \enum    stm_ycbcr_colorspace_e
 *  \brief   Control argument to set the output compositor's colourspace conversion
 *  \ctrlarg OUTPUT_CTRL_YCBCR_COLORSPACE
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_ycbcr_colorspace_e
{
  STM_YCBCR_COLORSPACE_AUTO_SELECT = 0, /*!< Follow the output display standard */
  STM_YCBCR_COLORSPACE_601,             /*!< Force ITU-R BT.601 matrix          */
  STM_YCBCR_COLORSPACE_709              /*!< Force ITU-R BT.709 matrix          */
} stm_ycbcr_colorspace_t;


/*! \enum    stm_display_signal_range_e
 *  \brief   Control argument to set the output's clipping behaviour
 *  \ctrlarg OUTPUT_CTRL_CLIP_SIGNAL_RANGE
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 *
 *  \note not all output hardware implementations support both
 *        STM_SIGNAL_FULL_RANGE and STM_SIGNAL_FILTER_SAV_EAV in which case
 *        the actual hardware behaviour will be identical for both settings.
 *        You may not be able to tell this is happening via the API.
 *
 */
typedef enum stm_display_signal_range_e
{
  STM_SIGNAL_FULL_RANGE = 0, /*!< All values are output including 0 and 255   */
  STM_SIGNAL_FILTER_SAV_EAV, /*!< All values are output except 0 and 255      */
  STM_SIGNAL_VIDEO_RANGE     /*!< Values are clamped to 16-235/240 depending on
                              *   the signal type, i.e. RGB, Luma, Chroma     */
} stm_display_signal_range_t;

/*! \enum    stm_display_chroma_filter_e
 *  \brief   Control argument to set the output's chroma filter
 *  \ctrlarg OUTPUT_CTRL_422_CHROMA_FILTER
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 *
 *  \note not all output hardware implementations support all filter controls
 *        STM_CHROMA_FILTER_VID_ONLY and STM_CHROMA_FILTER_GFX_ONLY will be considered
 *        as same as STM_CHROMA_FILTER_ENABLED.
 */
typedef enum stm_display_chroma_filter_e
{
  STM_CHROMA_FILTER_DISABLED = 0, /*!< Chroma filter is disabled                          */
  STM_CHROMA_FILTER_ENABLED,      /*!< Chroma filter is enabled                           */
  STM_CHROMA_FILTER_VID_ONLY,     /*!< Chroma filter is enabled when video is enabled     */
  STM_CHROMA_FILTER_GFX_ONLY      /*!< Chroma filter is enabled when graphics are enabled */
} stm_display_chroma_filter_t;


/*! \enum    stm_rgb_quantization_change_e
 *  \brief   Control argument to set the output quantization conversion of RGB video data
 *  \ctrlarg OUTPUT_CTRL_RGB_QUANTIZATION_CHANGE
 *  \apis    ::stm_display_output_set_control(), ::stm_display_output_get_control()
 */
typedef enum stm_rgb_quantization_change_e
{
  STM_RGB_QUANTIZATION_BYPASS, /*!< Do not change the video data before output    */
  STM_RGB_QUANTIZATION_EXPAND, /*!< Expand values 16-235 to 1-254 (in 8bit range) */
  STM_RGB_QUANTIZATION_REDUCE  /*!< Reduce values 1-254 to 16-235 (in 8bit range) */
} stm_rgb_quantization_change_t;


/*****************************************************************************
 * C interface for display outputs
 */
/*!
 * Get the human readable name of an output object.
 *
 * \param output Output to query.
 * \param name   A pointer to the output's name
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 * \returns -EFAULT The name output parameter pointer is an invalid address
 */
extern int stm_display_output_get_name(stm_display_output_h output, const char **name);

/*!
 * Get the numeric ID of an output's parent device.
 *
 * \apis ::stm_display_open_device()
 *
 * \param output Output to query.
 * \param id     Pointer to the variable to fill in.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 * \returns -EFAULT The id output parameter pointer is an invalid address
 */
extern int stm_display_output_get_device_id(stm_display_output_h output, uint32_t *id);

/*!
 * Get an OR’d bit mask of the capabilities that are supported by an output.
 *
 * \caps None
 *
 * \param output  Output to query
 * \param caps    The returned capabilities
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 * \returns -EFAULT The caps output parameter pointer is an invalid address
 *
 */
extern int stm_display_output_get_capabilities(stm_display_output_h output, uint32_t *caps);

/* Output controls */

/*!
 * Set an output control to a new value. Supported controls are documented in
 * following sections of this document.
 *
 * If an attempt is made to set an out of range value,  the request may be
 * clamped to the valid range or may return -ERANGE depending on the control.
 * The behavior in each case is documented for each individual control, but in
 * general invalid “enumeration” control values will return an error and
 * continuous numeric values will be clamped to the minimum and maximum of the
 * control’s range.
 *
 * Controls that represent a Boolean state are defined to take
 * the values zero (false) and any other integer value (true); this is so any C
 * or C++ logical or integer numeric expression can be used directly as the
 * argument.
 *
 * Some controls may quantize the supplied value to one actually supported by
 * the hardware, again this will be stated in individual control’s description.
 *
 * If it is important to know if the actual control value that
 * has been used you can subsequently call ::stm_display_output_get_control()
 * to read the value back.
 *
 * \caps None
 *
 * \param  output  Output to query
 * \param  ctrl    The control to change
 * \param  newVal  The new control value
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -ENOTSUP The control is not supported by the output
 * \returns -ERANGE  The control value was out of range
 *
 */
extern int stm_display_output_set_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t newVal);

/*!
 * Get an output control’s value.
 *
 * If the control is not supported by the output then the return variable will
 * be set to zero.
 *
 * \caps None
 *
 * \param output  Output to query
 * \param ctrl    The control to change
 * \param ctrlVal The returned control value
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -EFAULT  The control value output parameter pointer is an invalid
 *                   address
 * \returns -ENOTSUP The control is not supported by the output
 *
 */
extern int stm_display_output_get_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t *ctrlVal);

/*!
 * Set an output compound control to a new set of data values pointed to by the
 * new_val argument.
 *
 * If an attempt is made to set any out of range values as part of the new data
 * set, the values may be clamped to the valid range or may return -ERANGE
 * depending on the control. The behavior in each case is documented for each
 * individual control, but in general invalid “enumeration” type values will
 * return an error and continuous numeric values will be clamped to the minimum
 * and maximum of the value’s range.
 *
 * Some controls may quantize the supplied data to that actually supported by
 * the hardware, again this will be stated in individual control’s description.
 * If it is important to know the actual control data values that have been
 * used you can subsequently call ::stm_display_output_get_compound_control() to
 * read those values back.
 *
 * This is intended to allow components to customize the behavior of an output
 * that requires a multi-value configuration. For example to change the output
 * TV panel configuration or to change the analog DAC filter coefficients to
 * match a customer board’s electrical characteristics.
 *
 * \caps None
 *
 * \param  output  Output to query
 * \param  ctrl    The control to change
 * \param  new_val The new control value data
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -ENOTSUP The control is not supported by the output
 * \returns -ERANGE  The control value was out of range
 *
 */
extern int stm_display_output_set_compound_control(stm_display_output_h output, stm_output_control_t ctrl, void *new_val);

/*!
 * Get an output compound control’s value data, which will be copied into the
 * memory address provided by ctrl_val. This must be large enough to contain
 * the expected data structure returned by the control.
 *
 * If the control is not supported by the output then no data will be written to
 * the memory pointed to by ctrl_val.
 *
 * \caps None
 *
 * \param output   Output to query
 * \param ctrl     The control to change
 * \param ctrl_val The returned control value data
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -EFAULT  The control value output parameter pointer is an invalid
 *                   address
 * \returns -ENOTSUP The control is not supported by the output
 *
 */
extern int stm_display_output_get_compound_control(stm_display_output_h output, stm_output_control_t ctrl, void *ctrl_val);

/* Display modes */

/*!
 * This function gets a copy of the detailed mode description for a specified
 * standard mode identifier, if the output supports the
 * OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability and the mode can be started on
 * this output.
 *
 * \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 *
 * \param output  Output to query.
 * \param mode_id The mode identifier required.
 * \param mode    The returned mode descriptor.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -EINVAL  The output handle was invalid.
 * \returns -EFAULT  The mode output parameter pointer is an invalid address.
 * \returns -ENOTSUP No supported mode matched the requested parameters.
 *
 */
extern int stm_display_output_get_display_mode(stm_display_output_h output, stm_display_mode_id_t mode_id, stm_display_mode_t *mode);

/*!
 * This function searches for a standard display mode descriptor that is
 * supported on the given output, which matches the provided parameters. If
 * the output does not support the OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability,
 * or no match is found, the function will return -ENOTSUP.
 *
 * This is intended to be used in the implementation of other drivers that
 * specify display modes with another generic timing descriptor, specifically
 * the Linux framebuffer. The parameters match with the following rules:
 *
 * 1. \a xres and \a yres indicate the active video area required; these must match
 *    exactly.
 *
 * 2. \a min_lines and \a min_pixels refer to the total video lines and total pixels
 *    per line, but these are optional in that a mode matches if its values
 *    are >= to the values specified. Using 0 for these values will match the
 *    first mode that matches the other parameters.
 *
 * 3. \a pixel_clock (in Hz) matches with a "fuzz" factor so that the loss of
 *    precision in conversions between clock period (in picoseconds) to Hz, for
 *    example, the integer math in the Linux kernel framebuffer driver, is
 *    ignored.
 *
 * 4. \a scan_type indicates if the required mode is interlaced or progressive and
 *    must match exactly.
 *
 * \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 *
 * \param output       Output to query.
 * \param xres         The required active video width.
 * \param yres         The required active video height.
 * \param min_lines    The minimum total lines per frame including blanking and porches.
 * \param min_pixels   The minimum total pixels per line including blanking and porches.
 * \param pixel_clock  The required pixel clock in Hz.
 * \param scan_type    The requires scan type.
 * \param mode         The returned mode descriptor.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -EINVAL  The output handle was invalid.
 * \returns -EFAULT  The mode output parameter pointer is an invalid address.
 * \returns -ENOTSUP No supported mode matched the requested parameters.
 *
 */
extern int
  stm_display_output_find_display_mode(stm_display_output_h output,
                                       uint32_t             xres,
                                       uint32_t             yres,
                                       uint32_t             min_lines,
                                       uint32_t             min_pixels,
                                       uint32_t             pixel_clock,
                                       stm_scan_type_t      scan_type,
                                       stm_display_mode_t  *mode);

/*!
 * This function queries the current output display mode. A copy of the
 * output’s current mode descriptor is placed in the memory pointed to by the
 * \a mode parameter. If the output is currently stopped, the call will return
 * -ENOMSG.
 *
 * This is intended to be used by components that need to query the current
 * display mode set on an output, for instance, to discover the output
 * framerate when they are not the component responsible for starting the
 * output in the first place.
 *
 * \caps None
 *
 * \param output Output to query
 * \param mode   The returned mode descriptor
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The output handle was invalid.
 * \returns -EFAULT The mode output parameter pointer is an invalid address.
 * \returns -ENOMSG The output is stopped.
 *
 */
extern int stm_display_output_get_current_display_mode(stm_display_output_h output, stm_display_mode_t* mode);

/* Start/Stop */
/*!
 * This function starts the output with a specified display mode description.
 * If the output is already running, then there are a limited number of changes
 * that are allowed without stopping the output first. For example:
 *
 * - It is possible to change dynamically between standards supported by the
 *   currently running mode, for example, an analog output running a 480i mode
 *   could switch dynamically between NTSC and NTSC-J.
 *
 * - It is possible to switch dynamically between modes that only differ by a
 *   small change in the pixel clock frequency, for example, 1080p 59.94/60Hz.
 *
 * - It is possible to switch dynamically between 3D and 2D configurations of
 *   the same mode, as long as this does not require an internal change in the
 *   timing configuration, such as, SbS Half or Top/Bottom.
 *
 * For an output that supports the OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 * capability, the mode descriptor can be any configuration supported by that
 * output. For outputs that are clocked from another output's timing generator,
 * for example, HDMI, DVO, and DisplayPort, the mode timings and the active
 * area parameters will typically be identical to the "master" output's mode,
 * although the standard and flags parameters may be different. However, there
 * are HDMI use cases where the timings may be different to the master output
 * (all multiplied by a constant factor) in order to configure pixel
 * repetition. The parameters provided in this case must be compatible with
 * the current mode configured on the output that is the timing master,
 * otherwise this output will not be started. As a consequence, the output
 * acting as the timing master must be started with a valid mode first.
 *
 * When a mode description is identified as one of the standard supported
 * modes in stm_display_mode_id_t the structure is expected to be identical to
 * that returned by ::stm_display_output_get_display_mode() or
 * ::stm_display_output_find_display_mode() except for the standards and flags
 * fields of the mode parameters. These fields, however, must only specify
 * standards and flags that were indicated as supported by the originally
 * returned description.
 *
 * If, for instance, it is required to slightly modify the timings or active
 * video position of a standard mode, then the mode_id field of the mode
 * descriptor must be set to STM_TIMING_MODE_CUSTOM.
 *
 * \caps None
 *
 * \param output     Output to be started
 * \param mode       The required display mode descriptor.
 *
 * \returns  0      Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 * \returns -EFAULT The mode parameter pointer is an invalid address
 * \returns -EIO    The requested mode and standard could not be started
 *
 */
extern int stm_display_output_start(stm_display_output_h output, const stm_display_mode_t *mode);

/*!
 * Stop the given output such that it no longer produces any signal on the
 * SoC’s external interfaces. This will usually result in physical interfaces
 * being powered down, in order to meet current leakage or other power related
 * requirements.
 *
 * It is not an error to call this function if the output is already stopped.
 * For outputs that support the OUTPUT_CAPS_PLANE_MIXER capability, this call
 * will fail if any planes that are still connected to the output are enabled
 * on the output’s mixer.
 *
 * Stopping an output that owns a timing generator, i.e. reports the
 * OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability, will implicitly stop any
 * other outputs that are clocked from the same video timing generator.
 *
 * \caps None
 *
 * \param output Output to stop
 *
 * \returns  0      Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 * \returns -EBUSY  The output could not be stopped because it was still
 *                  processing active planes
 */
extern int stm_display_output_stop(stm_display_output_h output);

/* Additional APIs */
/*!
 * Queue a metadata payload descriptor on the output for future transmission by
 * the output.
 *
 * For more details on metadata payloads see stm_metadata.h
 *
 * \caps None
 *
 * \param output Output to queue metadata on
 * \param d      The metadata payload descriptor to queue
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -EFAULT  The payload pointers is an invalid address
 * \returns -ENOTSUP The payload type is not supported by the output
 * \returns -ETIME   The requested presentation timestamp has already passed,
 *                   or it is too late to meet that deadline because of hardware
 *                   limitations
 * \returns -EBADMSG It was determined that the payload data was invalid in
 *                   some way
 * \returns -EBUSY   The queue for the payload type could temporarily not
 *                   accept the new payload, e.g. it may have been full
 * \returns -EAGAIN  The queue for the payload type is not currently available
 *                   in the current output configuration, although it may be
 *                   available in some other configuration in the future.
 */
extern int stm_display_output_queue_metadata(stm_display_output_h output, stm_display_metadata_t *d);

/*!
 * Flush all entries from the output’s queue for a specified metadata type or
 * if the type parameter is zero flush all metadata queues supported by the
 * output. If the output does not support the metadata type the call has no
 * effect, this is not considered an error.
 *
 * This is used to clean up queued metadata, for instance if the playback of a
 * media stream is stopped part way through or there is a broadcast channel
 * change.
 *
 * \caps None
 *
 * \param output Output to flush
 * \param type   The metadata type to flush or 0 to flush all metadata
 *               supported by the output
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 *
 */
extern int stm_display_output_flush_metadata(stm_display_output_h output, stm_display_metadata_type_t type);

/* System infrastructure and platform integration */

/*!
 * Get a unique implementation dependent non-zero numeric identifier for the
 * video timing generator controlled by an output. It is only defined for
 * outputs that support the OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability.
 *
 * This is intended to be called by a platform infrastructure component to
 * manage calls to stm_display_device_update_vsync_irq().
 *
 * \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 *
 * \param output   Output to queried
 * \param timingID The returned timing identifier
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The output handle was invalid
 * \returns -EFAULT  The timingID output parameter pointer is an invalid address
 * \returns -ENOTSUP The output is not a timing master
 *
 */
extern int stm_display_output_get_timing_identifier(stm_display_output_h output, uint32_t *timingID);

/*!
 * Hardware interrupt processing specific to this output. For outputs that
 * support OUTPUT_CAPS_DISPLAY_TIMING_MASTER this will at least include the
 * associated VSync interrupt. However this may be called on any output type
 * that has associated interrupts, such as HDMI for example.
 *
 * The intended use is for this to be called by the OS platform specific
 * component that provides CoreDisplay’s working environment (i.e. its OS
 * abstraction layer implementation and other system integration). It will be
 * called from the first level interrupt handlers registered with the operating
 * system by that platform component.
 *
 * \caps None
 *
 * \param output Output to be processed
 */
extern void stm_display_output_handle_interrupts(stm_display_output_h output);

/*!
 * Get information about the last timing event recorded for the output,
 * i.e. top/bottom field in interlaced modes the left/right eye in certain
 * 3D modes and the 64bit timestamp of when the CPU recorded the event happened.
 *
 * This is only defined for outputs that support the
 * OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability. The result of calling this on
 * outputs that do not support this capability or if the output is stopped is
 * event will be set to STM_TIMING_EVENT_NONE and the timestamp set to 0.
 *
 * This will almost always be called immediately after
 * stm_display_handle_interrupts() in the OS platform first level interrupt
 * handler, but it can be called by any component interested in the information.
 * As it can be called from interrupt context it does not take the device lock
 * nor does it return errors.
 *
 * \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 *
 * \param output    Output to query
 * \param event     The returned 32bit bitmask of Or’d
 *                  ::stm_display_timing_events_e values.
 * \param timestamp The returned timestamp of the last event
 *
 */
extern void stm_display_output_get_last_timing_event(stm_display_output_h output, uint32_t *event, stm_time64_t *timestamp);

/*!
 * Get the connection status of the output to the physical display for outputs
 * that manage hotplugged displays. The states are:
 *
 * STM_DISPLAY_DISCONNECTED   No display is connected to the output.
 * STM_DISPLAY_CONNECTED      A display is connected and the output is
 *                            transmitting video data to it.
 * STM_DISPLAY_NEEDS_RESTART  A display is connected but the output is stopped
 *                            and requires a restart.
 *
 * Outputs that do not support hotplug management always return
 * STM_DISPLAY_CONNECTED as there is no way of telling if a display is
 * connected or not.
 *
 * This may be called from interrupt context, therefore it neither takes the
 * device lock nor does it return errors. This is also why this property of an
 * output is not exposed as a control.
 *
 * This is intended to be used as part of HDMI hotplug management. An HDMI
 * output will change its connection state based on changes to the hot plug
 * pin on the HDMI connector when that is connected to the HDMI hardware’s
 * interrupt in the SoC. Also see the control
 * OUTPUT_CTRL_ENABLE_HOTPLUG_INTERRUPT.
 *
 * \caps None
 *
 * \param output Output to query the status from
 * \param status Pointer to the status variable to fill in
 *
 */
extern int stm_display_output_get_connection_status(stm_display_output_h output, stm_display_output_connection_status_t *status);

/*!
 * Force the connection status of the output to the physical display for
 * outputs that manage hotplugged displays, but do not have an internal
 * hotplug interrupt, to one of the following values:
 *
 * STM_DISPLAY_DISCONNECTED  Tell the output the display has been disconnected.
 * STM_DISPLAY_NEEDS_RESTART Tell the output a display has been hotplugged.
 *
 * Outputs that do not support hotplug management will ignore this call.
 *
 * This may be called from interrupt context, therefore it neither takes the
 * device lock nor does it return errors. This is also why this property of an
 * output is not exposed as a control.
 *
 * This is intended to be used as part of HDMI hotplug management when hotplug
 * events are being polled or managed by a PIO interrupt owned by the system
 * platform component, to force the connection status of the output to be
 * consistent with the hotplug state. The component that is actually managing
 * HDMI hotplug and EDID behavior should then be signaled that the output
 * state has changed so it can act accoridingly.
 *
 * \caps None
 *
 * \param output Output to change
 * \param status The new connection status
 *
 */
extern void stm_display_output_set_connection_status(stm_display_output_h output, stm_display_output_connection_status_t status);

/*!
 * Change the reference clock configuration for the frequency synthesizers an
 * output uses from the default value. The implementation is typically
 * configured by default for a 30MHz crystal oscillator input from the board.
 *
 * The ability to specify an error in the reference clock allows for issues
 * with early board designs; but in a production system the clock reference
 * should always be within accepted tolerances and the error parameter should
 * be zero.
 *
 * Output implementations that use a frequency synthesizer via an OS abstracted
 * clock interface (that already has this information) or do not use a
 * frequency synthesizer at all will ignore this call, this is not considered
 * an error.
 *
 * This is intended to be used by a board support component to configure
 * outputs that set timing modes, i.e. support the
 * OUTPUT_CAPS_DISPLAY_TIMING_MASTER capability, or to inform HDMI outputs of
 * the audio clock reference frequency for the correct selection of the HDMI
 * audio clock regeneration parameters.
 *
 * \caps None
 *
 * \param output        Output to configured the clocks for
 * \param refClock      The input crystal reference frequency, 27 or 30MHz
 * \param refClockError An error specified in parts per million (ppm) of the
 *                      input crystal from the ideal.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The output handle was invalid
 *
 */
extern int stm_display_output_set_clock_reference(stm_display_output_h output,stm_clock_ref_frequency_t refClock, int refClockError);

/*!
 * Issue an output specific "soft reset".
 *
 * This is currently defined for outputs that own a video timing generator,
 * where the timing generator will be reset and will immediately issue a top
 * field vsync; using this on other output types produces no effect. If the
 * output is not currently started, hence its timing generator is stopped,
 * then this call will have no effect.
 *
 * The intended use of this function is to do a hard realignment of the output
 * vsync to be as close to an input vsync on the digital video capture hardware
 * as software latencies allow, as part of an overall latency management
 * strategy in an AV receiver use case. It is likely to be used when an input
 * source change has occurred. As it is expected to be called in the interrupt
 * handler of a video capture component the call does not take any locks or
 * return errors.
 *
 * This reset may damage HDCP authentication and will at the very least produce
 * a disturbance on the screen. It is recommended that an associated HDMI
 * output is stopped before the output owning the timing generator is reset,
 * then restarted and HDCP re-authenticated.
 *
 * \caps OUTPUT_CAPS_DISPLAY_TIMING_MASTER
 *
 * \param output Output to reset
 *
 */
extern void stm_display_output_soft_reset(stm_display_output_h output);

/*!
 * This function closes the given output handle, which will be invalid after
 * this call completes. Closing the output handle has no effect on the output
 * object it provides access to; that is if the output is running it will
 * continue to do so until explicitly stopped via another handle to the same
 * output or because the display device is destroyed.
 *
 * Calling this function with a NULL handle is allowed and will not cause
 * an error or system failure.
 *
 * \caps None
 *
 * \param output Output handle to close
 *
 */
extern void stm_display_output_close(stm_display_output_h output);

#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_OUTPUT_H */
