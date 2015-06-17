/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef STM_SE_STREAMING_ENGINE_H_
#define STM_SE_STREAMING_ENGINE_H_

/*!
 * \defgroup streaming_engine Streaming engine
 *
 * \section introduction Introduction
 *
 * The majority of the streaming engine's API is based upon actions that can be performed on object
 * instances. However, there are a small set of globally applicable actions that influence the entire
 * streaming engine. Since the streaming engine is a single global resource, these API calls have no object
 * on which to act. This section introduces these global APIs.
 *
 * \section error_handling Error handling
 *
 * The API documentation only describes return values that the client can be reasonably expected to handle
 * and recover from. Many errors are due to incorrectly coded API function calls and can only be corrected
 * through debugging.
 *
 * \subsection recoverable_errors Recoverable errors
 *
 * These are generally errors that occur dynamically during execution. An example is the unavailability of a
 * media resource. An attempt to play a media resource could fail at runtime because that resource is not
 * accessible. Recoverable errors will be reported as a negative return value from a function that detects
 * them.
 *
 * \subsection non_recoverable_errors Non-recoverable errors
 *
 * Non-recoverable errors are generally due to errors in coding calls to the streaming engine. Many of these
 * involve calling functions such that their preconditions are violated. Hardware failures are also
 * considered non-recoverable unless they are caused by resource or memory.
 *
 * If a non-recoverable error is detected, the streaming engine will call its error handler.
 *
 * \note The implementation cannot detect all forms of pre-condition violations, only a best-effort to
 *       detect obvious violations (such as NULL pointer) will be attempted by the implementation.
 *
 * If the error handler returns control to its caller, then the function that invoked error handler will
 * return immediately with -EINVAL. This return value is implicit to all functions with pre-conditions and
 * is not listed in the return value section of the API document.
 *
 * The default implementation of the error handler calls the BUG macro and continues execution. This
 * approach follows existing Linux kernel conventions and, while its behavior is platform dependant, on most
 * platforms it will issue diagnostics to the kernel log and cause the current thread to terminate.
 *
 * \note The kernel log itself can be redirected in any manner deemed appropriate. Typical targets include
 *       serial port, memory buffer, system log (syslogd) or ST's KPTrace tool. It can also be redirected to
 *       hardware diagnostic ports if provided by the platform.
 *
 * The default implementation can be overridden by calling ::stm_se_set_error_handler.
 *
 * \section streaming_engine_events Events
 *
 * The streaming engine sub-objects generate events, which are documented under each of the sub-object
 * sections. The streaming engine does not issue any events that are global to the entire subsystem.
 *
 * \section streaming_engine_controls Controls
 *
 * The streaming engine supports the following control selectors:
 *
 * \subsection streaming_engine_core_controls Core controls
 *
 *   - ::STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE
 *   - ::STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE
 *   - ::STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT
 *   - ::STM_SE_CTRL_DISCARD_LATE_FRAMES
 *   - ::STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY
 *   - ::STM_SE_CTRL_ENABLE_SYNC
 *   - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING
 *   - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED
 *   - ::STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS
 *   - ::STM_SE_CTRL_LIMIT_INJECT_AHEAD
 *   - ::STM_SE_CTRL_MASTER_CLOCK
 *   - ::STM_SE_CTRL_PLAYOUT_ON_DRAIN
 *   - ::STM_SE_CTRL_PLAYOUT_ON_SWITCH
 *   - ::STM_SE_CTRL_PLAYOUT_ON_TERMINATE
 *   - ::STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE
 *   - ::STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION
 *   - ::STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD
 *   - ::STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES
 *   - ::STM_SE_CTRL_LIVE_PLAY
 *   - ::STM_SE_CTRL_PLAYBACK_LATENCY
 *   - ::STM_SE_CTRL_HDMI_RX_MODE
 *   - ::STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE
 *   - ::STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE
 *   - ::STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE
 *   - ::STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE
 *
 * \subsection streaming_engine_video_controls Video controls
 *
 *   - ::STM_SE_CTRL_VIDEO_START_IMMEDIATE
 *   - ::STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED
 *   - ::STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
 *   - ::STM_SE_CTRL_TRICK_MODE_DOMAIN
 *   - ::STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
 *   - ::STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES
 *   - ::STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES
 *   - ::STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION
 *   - ::STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING
 *   - ::STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST
 *   - ::STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED
 *   - ::STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES
 *   - ::STM_SE_CTRL_MPEG2_APPLICATION_TYPE
 *   - ::STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG
 *   - ::STM_SE_CTRL_VIDEO_DISCARD_FRAMES
 *   - ::STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR
 *   - ::STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY
 *   - ::STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION
 *   - ::STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD
 *   - ::STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD
 *
 * \subsection streaming_engine_audio_controls Audio controls
 *
 *   - ::STM_SE_CTRL_AUDIO_APPLICATION_TYPE
 *   - ::STM_SE_CTRL_AAC_DECODER_CONFIG
 *   - ::STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION
 *   - ::STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL
 *   - ::STM_SE_CTRL_BASSMGT
 *   - ::STM_SE_CTRL_DCREMOVE
 *   - ::STM_SE_CTRL_STREAM_DRIVEN_STEREO
 *   - ::STM_SE_CTRL_SPEAKER_CONFIG
 *   - ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
 *   - ::STM_SE_CTRL_DUALMONO
 *   - ::STM_SE_CTRL_DEEMPHASIS
 *
 * @{
 */


/*!
 * This function initializes the streaming engine. It will allocate the internal buffer manager and other
 * resources required by streaming engine objects.
 *
 * stm_se_init must be called before any other call.  It is typically from a kernel module initialization
 * routine and need not be called by the adaptation layer.
 *
 * \code
 * static int __init StreamingEngineLoadModule (void)
 * {
 *    int res;
 *          res = stm_se_init ());
 *          if (0 != res) {
 *              pr_err("%s: Cannot initialized streaming engine\n", __func__);
 *              return res;
 *          }
 *    pr_info("%s: Streaming engine loaded\n", __func__);
 *    return 0;
 * }
 * \endcode
 *
 *
 * None.
 *
 * \retval 0 No error
 * \retval -EALREADY Already initialized.
 * \retval -ENOMEM Insufficient memory to create necessary objects.
 */
int             stm_se_init(void);

/*!
 * This function un-initializes the streaming engine and releases all allocated resources.
 *
 * stm_se_term should be called when the player is finished. It is normally called as part of the system
 * shutdown, for example, when the STKPI modules are unloaded in a Linux system and need not be called by
 * the adaptation layer.
 *
 * \code
 * static void __exit StreamingEngineUnLoadModule (void)
 * {
 *     int res;
 *     res = stm_se_term ();
 *     BUG_ON(0 != res); // bad resource tracking
 *     pr_info ("%s: Streaming engine unloaded\n", __func__);
 * }
 * \endcode
 *
 *
 * None.
 *
 * \retval 0 No error
 * \retval -EALREADY Not currently initialized.
 * \retval -EBUSY Termination is not permitted because some streaming engine objects have not been deleted.
 */
int             stm_se_term(void);

/*!
 * This function is used to set the value of a control that is cascaded from the streaming engine defaults
 * (set by this function) to playback and play stream objects or encode and encode stream objects.
 *
 * The function applies the control to all objects that do not have overridden default settings.
 *
 * Only cascaded controls may be accessed using this function. For more information on streaming engine
 * controls, see \ref streaming_engine_controls.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] ctrl Select which control to alter.
 * \param[in] value The control's new value.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_set_control(stm_se_ctrl_t           ctrl,
                                   int32_t                 value);
/*!
 * This function is used to read the value of a control that is cascaded from the streaming engine defaults
 * to specific objects.
 *
 * Only cascaded controls may be accessed using this function. For more information on streaming engine
 * controls, see \ref streaming_engine_controls.
 *
 * \pre The streaming engine must be initialized.
 * \pre The value parameter must be a valid pointer.
 *
 * \param[in] ctrl Select which control to fetch.
 * \param[out] value Pointer that will be populated with the control's value.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_get_control(stm_se_ctrl_t           ctrl,
                                   int32_t                *value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \pre The streaming engine must be initialized.
 * \pre The value parameter must be a valid pointer.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_get_compound_control(stm_se_ctrl_t               ctrl,
                                            void                        *value);

/*!
 * This function is used to supply a function that will be called
 * whenever the implementation detects that a pre-condition has been violated.
 *
 * The error handler is global to all streaming engine API functions.
 *
 * \param[in] ctx Context pointer that will be provided as an argument to the callback.
 * \param[in] handler Function pointer to be called when a pre-condition violation is detected.
 *
 * \retval 0 No error
 * \retval -EINVAL The streaming engine is not initialized.
 */
int             stm_se_set_error_handler(void                   *ctx,
                                         stm_error_handler       handler);

/*! @} */ /* streaming_engine */

#endif /* STM_SE_STREAMING_ENGINE_H_ */
