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

#ifndef STM_SE_PLAYBACK_H_
#define STM_SE_PLAYBACK_H_

/*!
 * \defgroup playback Playback
 *
 * A playback is a collection of play streams that have coherent timing information.
 *
 * Typical examples of a playback include:
 *
 * <table>
 *     <tr>
 *         <td>
 *             Music playback
 *         </td>
 *         <td>
 *             One play stream, configured for MP3 decoding.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Simple broadcast TV playback
 *         </td>
 *         <td>
 *             Two play streams, one for H.264 video and another for AC3 audio.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Above with audio description
 *         </td>
 *         <td>
 *             Three play streams, those above plus an additional audio decoder.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             BD-ROM with directors commentary
 *         </td>
 *         <td>
 *             Four play streams, two video play streams (main feature and director shown PiP) and two audio
 *             play streams (main feature and directory commentary).
 *         </td>
 *     </tr>
 * </table>
 *
 * All of the above examples have coherent timing information and must be synchronized together as a single
 * group. Likewise, they should remain synchronized during trick mode playback.
 *
 * Note that conventional PiP (where two independent channels are presented on the same display) does not
 * form a single playback because they are not coherent. Specifically, if the primary channel were paused,
 * the PiP need not be paused.
 *
 * \section playback_events Events
 *
 * The playback object does not issue any events.
 *
 * \section playback_controls Controls
 *
 * The playback object supports the following control selectors:
 *
 * \subsection playback_core_controls Core controls
 *
 *  - ::STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE
 *  - ::STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE
 *  - ::STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT
 *  - ::STM_SE_CTRL_DISCARD_LATE_FRAMES
 *  - ::STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY
 *  - ::STM_SE_CTRL_ENABLE_SYNC
 *  - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING
 *  - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED
 *  - ::STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS
 *  - ::STM_SE_CTRL_LIMIT_INJECT_AHEAD
 *  - ::STM_SE_CTRL_MASTER_CLOCK
 *  - ::STM_SE_CTRL_PLAYOUT_ON_DRAIN
 *  - ::STM_SE_CTRL_PLAYOUT_ON_SWITCH
 *  - ::STM_SE_CTRL_PLAYOUT_ON_TERMINATE
 *  - ::STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE
 *  - ::STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION
 *  - ::STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD
 *  - ::STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES
 *  - ::STM_SE_CTRL_LIVE_PLAY
 *  - ::STM_SE_CTRL_PLAYBACK_LATENCY
 *  - ::STM_SE_CTRL_HDMI_RX_MODE
 *
 * \subsection playback_video_controls Video controls
 *
 *  - ::STM_SE_CTRL_VIDEO_START_IMMEDIATE
 *  - ::STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED
 *  - ::STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
 *  - ::STM_SE_CTRL_TRICK_MODE_DOMAIN
 *  - ::STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
 *  - ::STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES
 *  - ::STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES
 *  - ::STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION
 *  - ::STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING
 *  - ::STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST
 *  - ::STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED
 *  - ::STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES
 *  - ::STM_SE_CTRL_MPEG2_APPLICATION_TYPE
 *  - ::STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG
 *  - ::STM_SE_CTRL_VIDEO_DISCARD_FRAMES
 *  - ::STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR
 *  - ::STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY
 *  - ::STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION
 *  - ::STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD
 *  - ::STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD
 *
 * \subsection playback_audio_controls Audio controls
 *
 *  - ::STM_SE_CTRL_AUDIO_APPLICATION_TYPE
 *  - ::STM_SE_CTRL_AAC_DECODER_CONFIG
 *  - ::STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION
 *  - ::STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL
 *  - ::STM_SE_CTRL_BASSMGT
 *  - ::STM_SE_CTRL_DCREMOVE
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_STEREO
 *  - ::STM_SE_CTRL_SPEAKER_CONFIG
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
 *  - ::STM_SE_CTRL_DUALMONO
 *  - ::STM_SE_CTRL_DEEMPHASIS
 *
 * @{
 */

/*!
 * ::stm_se_playback_set_speed and ::stm_se_playback_get_speed both use integer values to represent the playback
 * speed. The STM_SE_PLAYBACK_SPEED constants are to select special behavior.
 *
 * The normal play constant has a relatively large numeric value and can appear in expressions
 * such as the following:
 *
 * <table>
 *     <tr>
 *         <td>
 *             ::STM_SE_PLAYBACK_SPEED_NORMAL_PLAY * 2
 *         </td>
 *         <td>
 *             Forward play at double speed (2x)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             ::STM_SE_PLAYBACK_SPEED_NORMAL_PLAY * -2
 *         </td>
 *         <td>
 *             Reverse play at double speed
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             ::STM_SE_PLAYBACK_SPEED_NORMAL_PLAY / 4
 *         </td>
 *         <td>
 *             Forward play at quarter speed
 *         </td>
 *     </tr>
 * </table>
 */
enum stm_se_playback_speed_e
{
    /*!
     * Forward play at normal speed (1x).
     */
    STM_SE_PLAYBACK_SPEED_NORMAL_PLAY = 1000,

    /*!
     * Forward play stopped.
     */
    STM_SE_PLAYBACK_SPEED_STOPPED = 0,

    /*!
     * Reverse play stopped. This behaves identically to ::STM_SE_PLAYBACK_SPEED_STOPPED unless the
     * user calls ::stm_se_play_stream_step while stopped.
     */
    STM_SE_PLAYBACK_SPEED_REVERSE_STOPPED = (int) 0x80000000,
};

/*!
 * \typedef typedef void *stm_se_playback_h
 *
 * A playback handle is an opaque data type with which to identify playback. It cannot be meaningfully
 * de-referenced outside of the streaming engine implementation.
 */


/*!
 * This function allocates a new playback. The allocated playback is empty; it contains no play streams.
 *
 * \note If the playback is required to start stopped (paused), then the playback speed must be set to
 *       stopped before any play streams are added to the playback.
 *
 * \code
 * res = stm_se_playback_new ("main", &context->playback);
 * if (0 != res) {
 *    return res;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[out] playback Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -EINVAL The streaming engine is not initialized.
 * \retval -ENOMEM Insufficient resources to complete operation.
 */
int             stm_se_playback_new(const char             *name,
                                    stm_se_playback_h      *playback);

/*!
 * This function deletes a playback releasing all associated resources.
 *
 * \code
 * res = stm_se_playback_delete(&playback);
 * if (0 != res) {
 *    return res;
 * }
 * res = stm_se_play_stream_new(playback,
 *                      STM_SE_STREAM_ENCODING_AUDIO_VORBIS,
 *                      &stream);
 * if (0 != res) {
 *    int inner_res = stm_se_playback_delete(playback);
 *    BUG_ON(0 != inner_res); // handle known good!
 *    return res;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Previously allocated playback handle.
 *
 * \retval 0 No errors
 * \retval -EINVAL The streaming engine is not initialized.
 * \retval -EINVAL Invalid playback
 * \retval -EBUSY Allocated objects (play streams, etc.) attached to this playback preclude
 * deletion.
 */
int             stm_se_playback_delete(stm_se_playback_h       playback);

/*!
 * This function sets the play speed for the playback.
 *
 * The speed applies to all streams in the playback. Forward play speeds are specified by positive values
 * and reverse play by negative values.
 *
 * The speed is expressed in multiples of ::STM_SE_PLAYBACK_SPEED_NORMAL_PLAY; for further information
 * see ::stm_se_playback_speed_e.
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback to set the speed.
 * \param[in] speed Desired speed.
 *
 * \retval 0 No error
 * \retval -EINVAL The streaming engine is not initialized.
 * \retval -EINVAL Invalid playback.
 */
int             stm_se_playback_set_speed(stm_se_playback_h       playback,
                                          int32_t                 speed);

/*!
 * This function returns the current speed.
 *
 * If the implementation is unable to achieve the exact speed requested by ::stm_se_playback_set_speed then
 * the value reported will be the actual play speed and may differ from the requested value.
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback to get the speed.
 * \param[out] speed Pointer to variable to hold returned speed.
 *
 * \retval 0 No errors
 * \retval -EINVAL The streaming engine is not initialized.
 * \retval -EINVAL NULL pointer.
 * \retval -EINVAL Invalid playback
 */
int             stm_se_playback_get_speed(stm_se_playback_h       playback,
                                          int32_t                *speed);

/*!
 * This function sets the value of a playback wide option such as the chosen master clock.
 *
 * \code
 * res = stm_se_playback_set_control(
 *         Context->Playback,
 *         STM_SE_CTRL_MASTER_CLOCK,
 *         STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER);
 * if (res != 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback to set the control.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Value to apply.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_playback_set_control(stm_se_playback_h       playback,
                                            stm_se_ctrl_t           ctrl,
                                            int32_t                 value);

/*!
 * This function gets the value of a playback wide control such as the chosen master clock.
 *
 * \code
 * res = stm_se_playback_get_control(
 *         Context->Playback,
 *         STM_SE_CTRL_MASTER_CLOCK,
 *         &value);
 * if (res < 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback to set the control.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[out] value Pointer where currently applied control value can be stored.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_playback_get_control(stm_se_playback_h       playback,
                                            stm_se_ctrl_t           ctrl,
                                            int32_t                *value);

/*!
 * This function forces the player to adopt an external mapping between the native playback time (for
 * example, PTS) and the system time. The player then uses this mapping to control the translation of
 * playback time to system time. This function allows an application to specify the display time of a
 * playback, for example, when there are specific latency requirements.
 *
 * \code
 * Ktime      = ktime_get();
 * SystemTime = ktime_to_us (Ktime) + (Latency * 1000);
 * res        = stm_se_playback_set_native_time(Playback, 3000, SystemTime);
 * if (res != 0) {
 *     return res;
 * }
 * // now starting injecting stream with PES starting at 3000 and the
 * // first frame will appear at SystemTime (subject to the laws
 * // of time and space).
 * //
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback to apply the mapping.
 * \param[in] native_time Playback native time in PTS.
 * \param[in] system_time Time corresponding to the PTS in monotonic time units.
 *
 * \retval 0 No error
 * \retval -ERANGE Native time is out of range.
 */
int             stm_se_playback_set_native_time(stm_se_playback_h       playback,
                                                uint64_t                native_time,
                                                unsigned long long      system_time);

/*!
 * This function is used to provide data points to the clock recovery mechanism. The clock data point can be
 * raw captured data or clock reconstructed data.
 *
 * Normally this function is called automatically by the demux subsystem. However, this facility can be used
 * to synchronize media delivered from an unusual media source.
 *
 * See \ref time_units_within_the_streaming_engine for details of the time formats used by this function.
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] playback Handle of playback use clock must be tuned.
 * \param[in] time_format Indicates whether the units used for the source_time are the stream's native
 * format (typically 90KHz) or usec.
 *
 * \param[in] new_sequence Indicates whether the clock recovery mechanism needs to be reset.
 * \param[in] source_time Received clock value (for example, PCR) in native time units.
 * \param[in] system_time Timestamp of received value in monotonic time units.
 *
 * \retval 0 No errors
 * \retval -EINVAL Time format specifier is invalid.
 */
int             stm_se_playback_set_clock_data_point(stm_se_playback_h       playback,
                                                     stm_se_time_format_t    time_format,
                                                     bool                    new_sequence,
                                                     uint64_t                source_time,
                                                     uint64_t                system_time);

/*!
 * This function is used to return an estimation of the source time that is continuously maintained by the
 * streaming engine's output coordinator, using the data points provided to the clock recovery mechanism by
 * ::stm_se_playback_set_clock_data_point. It also returns the system time that corresponds to this source
 * time.
 *
 * See \ref time_units_within_the_streaming_engine for details of the time formats used by this function.
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted
 *
 * \param[in] playback Handle of playback use clock must be tuned.
 *
 * \param[out] source_time Received clock value (for example, PCR) in native time units.
 * \param[out] system_time Timestamp of received value in monotonic time units.
 *
 * \retval 0 No errors
 * \retval -EINVAL Time format specifier is invalid.
 *
 * - EFAULT
 *
 * No estimation can be performed. This is the case if the output coordinator has not been either provided
 * with any value or any enough coherent values.
 */
int             stm_se_playback_get_clock_data_point(stm_se_playback_h       playback,
                                                     uint64_t               *source_time,
                                                     uint64_t               *system_time);

/*! @} */ /* playback */

#endif /* STM_SE_PLAYBACK_H_ */
