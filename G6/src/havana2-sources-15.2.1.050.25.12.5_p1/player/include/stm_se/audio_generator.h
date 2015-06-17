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

#ifndef STM_SE_AUDIO_GENERATOR_H_
#define STM_SE_AUDIO_GENERATOR_H_

/*!
 * \defgroup audio_generator Audio generator
 *
 * An audio generator object represents an external source of PCM samples in memory. The PCM samples are
 * typically interactively generated (sound effects, user-interface beeps, etc.).
 *
 * # Controls
 *
 * The audio generator object supports the following control selectors:
 *
 * <table>
 *     <tr>
 *         <td>
 *             ::STM_SE_CTRL_AUDIO_GENERATOR_BUFFER
 *         </td>
 *         <td>
 *             ::STM_SE_CTRL_AUDIO_INPUT_FORMAT
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             ::STM_SE_CTRL_AUDIO_INPUT_EMPHASIS
 *         </td>
 *     </tr>
 * </table>
 *
 * @{
 */


/*!
 * An audio generator handle is an opaque data type with which to identify an audio generator. It cannot be
 * meaningfully de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_audio_generator_h;

/*!
 * Describes the current state of the audio generator.
 */
typedef enum stm_se_audio_generator_state_e
{
    /*!
     * Audio generator is not connected to mixer.
     */
    STM_SE_AUDIO_GENERATOR_DISCONNECTED,

    /*!
     * Audio generator is idle at user request.
     */
    STM_SE_AUDIO_GENERATOR_STOPPED,

    /*!
     * Audio Generator state is changing to started state.
     */
    STM_SE_AUDIO_GENERATOR_STARTING,

    /*!
     * Audio generator is operational and rendering samples from the circular buffer supplied by
     * ::STM_SE_CTRL_AUDIO_GENERATOR_BUFFER as per content description supplied by
     * ::STM_SE_CTRL_AUDIO_INPUT_FORMAT and ::STM_SE_CTRL_AUDIO_INPUT_EMPHASIS .
     */
    STM_SE_AUDIO_GENERATOR_STARTED,

    /*!
     * Audio Generator state is changing from started to stopped state.
     */
    STM_SE_AUDIO_GENERATOR_STOPPING,
} stm_se_audio_generator_state_t;

/*!
 * This type describes a circular buffer shared between the application which emits samples and the
 * streaming engine which consumes samples and managed flow control.
 *
 * \note The audio mixer does not attempt to perform any de-pop during transitions from
 *       STM_SE_AUDIO_GENERATOR_STARTED to STM_SE_AUDIO_GENERATOR_UNDERFLOW. For this reason observing an audio
 *       generator in the underflow state should normally be regarded as an application error. The
 *       stm_se_audio_generator_event_t::STM_SE_AUDIO_GENERATOR_EVENT_UNDERFLOW event is raised
 *       during these transitions and can be used to debug
 *       the application.
 *
 * The position of the read pointer, which may be useful for latency critical applications such as
 * telephony, can be inferred from the information above when combined with the applications existing
 * knowledge regarding the size of the circular buffer
 */
typedef struct stm_se_audio_generator_info_s
{
    /*!
     * Describes the current state of the audio generator.
     */
    stm_se_audio_generator_state_t  state;

    /*!
     * Indicates the remaining space within the audio generator's circular buffer, measured in
     * samples. The application must not generate (or copy) more than avail samples into the
     * circular buffer.
     */
    uint32_t                        avail;

    /*!
     * Indicates the position of the circular buffer's write pointer, measured in samples. The next
     * sample generated should be copied to this position within the circular buffer.
     */
    uint32_t                        head_offset;
} stm_se_audio_generator_info_t;

/*!
 * This type enumerates the keys that may be used to identify events.
 *
 * The enumeration above contains an entry for each supported event.
 */
typedef enum stm_se_audio_generator_event_e
{
    /*!
     * This event reports that data has been consumed from the audio buffer. The application should call
     * ::stm_se_audio_generator_get_info and refill the buffer.
     *
     * \warning Stability: Unstable
     */
    STM_SE_AUDIO_GENERATOR_EVENT_DATA_CONSUMED = (1 << 0),

    /*!
     * This event reports that the application did not deliver samples to the audio generator early enough. This
     * event is generated whenever the audio generator state transitions from
     * stm_se_audio_generator_state_e::STM_SE_AUDIO_GENERATOR_STARTED to
     * stm_se_audio_generator_state_e::STM_SE_AUDIO_GENERATOR_UNDERFLOW.
     *
     * \warning Stability: Unstable
     */
    STM_SE_AUDIO_GENERATOR_EVENT_UNDERFLOW = (1 << 1)
} stm_se_audio_generator_event_t;

/*!
 * This function allocates an audio generator context.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[out] audio_generator Pointer to an opaque handle variable which will be set to a valid handle if
 * the function succeeds.
 *
 * \retval 0 No error
 * \retval -ENOMEM Insufficient memory to complete operation.
 */
int             stm_se_audio_generator_new(const char *name,
                                           stm_se_audio_generator_h *audio_generator);
/*!
 * This function deletes an audio generator context.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 *
 * \retval 0 No error
 * \retval -EBUSY Audio generator is still in use.
 */
int             stm_se_audio_generator_delete(stm_se_audio_generator_h audio_generator);

/*!
 * This function connects an audio mixer object to an audio generator.
 *
 * After this call completes, the sound hardware will be initialized and running (it will be playing digital
 * silence if there are no other active inputs).
 *
 * \note The audio generator cannot be used until the ::STM_SE_CTRL_AUDIO_GENERATOR_BUFFER control is set. Any
 *       attempt to attach the audio generator to a sink object will fail until the generator format is specified.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \pre The sink parameter must have been previously provided by ::stm_se_audio_mixer_new and not deleted.
 *
 * \param[in] audio_generator Handle of audio generator to connect the sink to.
 * \param[in] sink Handle of an object that can accept data from the audio generator.
 *
 * \retval 0 No errors
 * \retval -EINVAL Sink object type cannot be attached to this audio generator.
 * \retval -EALREADY The audio generator is already connected to a sink object.
 */
int             stm_se_audio_generator_attach(stm_se_audio_generator_h audio_generator,
                                              stm_se_audio_mixer_h sink);

/*!
 * This function disconnects an audio generator from an audio mixer.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by ::stm_se_audio_mixer_new and not deleted.
 *
 * \param[in] audio_generator Handle of audio generator to disconnect the sink from.
 * \param[in] sink Handle of sink object to be disconnected from the audio generator.
 *
 * \retval 0 No errors
 * \retval -ENOTCON The connection does not exist.
 */
int             stm_se_audio_generator_detach(stm_se_audio_generator_h audio_generator,
                                              stm_se_audio_mixer_h sink);

/*!
 * This function sets the value specified by the control type.
 *
 * \note The audio generator cannot be used until the ::STM_SE_CTRL_AUDIO_GENERATOR_BUFFER control is set. Any
 *       attempt to attach the audio generator to a sink object will fail until the generator format is specified.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -EAGAIN audio-generator is stopping , the caller should try again later
 * (until the generator is in STOPPED state)
 * \retval -EBUSY  audio-generator is starting or started, the controls can't be modified in this state
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_generator_set_compound_control(stm_se_audio_generator_h audio_generator,
                                                            stm_se_ctrl_t ctrl, const void *value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_generator_get_compound_control(stm_se_audio_generator_h audio_generator,
                                                            stm_se_ctrl_t ctrl, void *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \note The audio generator cannot be used until the ::STM_SE_CTRL_AUDIO_GENERATOR_BUFFER control is set. Any
 *       attempt to attach the audio generator to a sink object will fail until the generator format is specified.
 *
 * \pre The streaming engine must be initialized. Audio generator created and attached to the mixer
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 */
int             stm_se_audio_generator_set_control(stm_se_audio_generator_h audio_generator,
                                                   stm_se_ctrl_t ctrl, int32_t value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized. The Audio Generator must have been created already and attached
 * to the mixer
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -EINVAL Audio Generator is not yet attached to the mixer or ctrl is invalid
 */
int             stm_se_audio_generator_get_control(stm_se_audio_generator_h audio_generator,
                                                   stm_se_ctrl_t ctrl, int32_t *value);

/*!
 * This function reports information about the current state of the audio generator.
 *
 * This function is critical to flow control between the driver and the application. When called, it informs
 * the application how much space remains in the hardware buffer.
 *
 * Additionally, it reports the offset within the audio buffer that the driver expects data to be written
 * (meaning the application need not track this independently).
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \param[in] audio_generator Previously allocated audio player handle.
 * \param[out] info Pointer to location to store information.
 *
 * \retval 0 No error
 */
int             stm_se_audio_generator_get_info(stm_se_audio_generator_h audio_generator,
                                                stm_se_audio_generator_info_t *info);

/*!
 * This function is used to notify the driver that the application has written data into the audio buffer.
 *
 * This is used to track buffer levels and inform the application if there is an underflow.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 * \param[in] nspl Number of samples committed to the hardware buffer.
 *
 * \retval 0 No error
 * \retval -EINVAL Too many samples committed (buffer overflow).
 */
int             stm_se_audio_generator_commit(stm_se_audio_generator_h audio_generator,
                                              uint32_t nspl);

/*!
 * This function requests the audio mixer associated with this audio generator to start consuming samples from
 * the circular buffer supplied during creation.
 * The call is not blocking and will only submit the START request to the attached mixer. The mixer will
 * grant the request at the beginning of its next processing period.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \warning Stability: Unstable
 *
 * \param[in] audio_generator Previously allocated audio generator handle.
 *
 * \retval 0 No error
 * \retval -EALREADY Audio generator is already started.
 * \retval -EBADE Audio generator is not attached to a mixer.
 */
int             stm_se_audio_generator_start(stm_se_audio_generator_h audio_generator);

/*!
 * This function requests the audio mixer associated with this audio generator to stop consuming samples from
 * the circular buffer supplied during creation.
 * The call is not blocking and will only submit the STOP request to the attached mixer. The mixer will
 * grant the request at the beginning of its next processing period.
 *
 * This function can also be used to bring the audio generator into a known state during application error
 * handling (in which case -EALREADY should not be regarded as a failure)
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio generator parameter must have been previously provided by ::stm_se_audio_generator_new and not
 * deleted.
 *
 * \warning Stability: Unstable
 *
 * \param[in] audio_generator Previously allocated audio player handle.
 *
 * \retval 0 No error
 * \retval -EALREADY Audio generator is already stopped.
 * \retval -EBADE Audio generator has not been setup.
 */
int             stm_se_audio_generator_stop(stm_se_audio_generator_h audio_generator);

/*! @} */ /* audio_generator */

#endif /* STM_SE_AUDIO_GENERATOR_H_ */
