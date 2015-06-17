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

#ifndef STM_SE_AUDIO_PLAYER_H_
#define STM_SE_AUDIO_PLAYER_H_

/*!
 * \defgroup audio_player Audio player
 *
 * An audio player object represents the output to PCM or SPDIF playback hardware. It is used to configure
 * that outputs' unique PCM post-processing chain.
 *
 * \section audio_player_controls Controls
 *
 * The audio player object supports the following control selectors:
 *
 * \subsection audio_player_specific_controls Audio player controls
 *
 *  - ::STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE
 *  - ::STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY
 *  - ::STM_SE_CTRL_SPEAKER_CONFIG
 *  - ::STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL
 *  - ::STM_SE_CTRL_AUDIO_DELAY
 *  - ::STM_SE_CTRL_AUDIO_GAIN
 *  - ::STM_SE_CTRL_AUDIO_SOFTMUTE
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
 *  - ::STM_SE_CTRL_DUALMONO
 *
 * \subsection audio_player_post_processing_controls Audio post-processing controls
 *
 *  - ::STM_SE_CTRL_BASSMGT
 *  - ::STM_SE_CTRL_DCREMOVE
 *  - ::STM_SE_CTRL_LIMITER
 *  - ::STM_SE_CTRL_BTSC
 *
 * @{
 */

/*!
 * An audio player handle is an opaque data type with which to identify an audio player. It cannot be
 * meaningfully de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_audio_player_h;

/*!
 * This type enumerates the keys that may be used to identify events.
 *
 * The enumeration contains an entry for each supported event.
 */
typedef enum stm_se_audio_player_event_e
{
    /*!
     * This event reports that the audio player experienced a data underflow.
     *
     * This is a serious error that results in a glitch being delivered to the audio system that has not been
     * concealed with a soft mute.
     *
     * This event is used solely for debugging. The system will automatically recover from underflow by stopping
     * and restarting the audio mixer that the audio player is connected to.
     *
     * \warning Stability: Unstable
     */
    STM_SE_AUDIO_PLAYER_EVENT_UNDERFLOW
} stm_se_audio_player_event_t;

/*!
 * This function allocates an audio player context.
 *
 * The mapping between the hw_name and the associated PCM or SPDIF player resource is implementation
 * dependant. On Linux systems, it is derived from the ALSA configuration:
 *
 * \code
 * res = stm_se_audio_player_new("headphones", "hw:STiH407,1", &headphone_player);
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[in] hw_name Name to identify the hardware resource linked to this audio player.
 * \param[out] audio_player Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -EINVAL Hardware name value is invalid on this platform.
 * \retval -ENOMEM Insufficient memory to complete operation.
 */
int             stm_se_audio_player_new(const char                         *name,
                                        const char                          *hw_name,
                                        stm_se_audio_player_h               *audio_player);

/*!
 * This function deletes an audio player context.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio player parameter must have been previously provided by ::stm_se_audio_player_new and not deleted.
 *
 * \param[in] audio_player Previously allocated audio player handle.
 *
 * \retval 0 No error
 * \retval -EBUSY Audio player is still in use.
 */
int             stm_se_audio_player_delete(stm_se_audio_player_h              audio_player);

/*!
 * This function sets the compound value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_player Previously allocated audio player handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_player_set_compound_control(stm_se_audio_player_h    audio_player,
                                                         stm_se_ctrl_t             ctrl,
                                                         const void                *value);

/*!
 * This function gets the compound value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_player Previously allocated audio player handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_player_get_compound_control(stm_se_audio_player_h    audio_player,
                                                         stm_se_ctrl_t             ctrl,
                                                         void                      *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_player Previously allocated audio player handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_player_set_control(stm_se_audio_player_h    audio_player,
                                                stm_se_ctrl_t             ctrl,
                                                const int32_t             value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_player Previously allocated audio player handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_player_get_control(stm_se_audio_player_h    audio_player,
                                                stm_se_ctrl_t             ctrl,
                                                int32_t                  *value);

/*! @} */ /* audio_player */

#endif /* STM_SE_AUDIO_PLAYER_H_ */
