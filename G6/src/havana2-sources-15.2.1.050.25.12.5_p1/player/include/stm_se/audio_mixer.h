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

#ifndef STM_SE_AUDIO_MIXER_H_
#define STM_SE_AUDIO_MIXER_H_

/*!
 * \defgroup audio_mixer Audio mixer
 *
 * An audio mixer object is used to control the audio mixer. Conceptually, the audio mixer receives data
 * from one or more play streams and mixes them together before emitting data to one or more audio players.
 * The audio mixer consists in a mixing-stage that cope with the mixing of all inputs (play-stream and
 * audio-generators) and in a postprocessing stage that will apply specific audio processings required by
 * each audio-players connected to the mixer.
 *
 * \section audio_mixer_events Events
 *
 * The audio mixer object does not issue any events.
 *
 * \section audio_mixer_controls Controls
 *
 * The audio mixer object supports the following control selectors:
 *
 *  - ::STM_SE_CTRL_AUDIO_MIXER_GRAIN
 *  - ::STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENT
 *  - ::STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_STEREO
 *  - ::STM_SE_CTRL_SPEAKER_CONFIG
 *  - ::STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY
 *  - ::STM_SE_CTRL_AUDIO_GAIN
 *  - ::STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION
 *
 * @{
 */

/*!
 * An audio mixer handle is an opaque data type with which to identify audio mixer. It cannot be
 * meaningfully de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_audio_mixer_h;

/*!
 * the following defines should be used to specify the mixing stage
 * that the mixer object should implement.
 * Whatever the mixing-stage type, its output will be postprocessed
 * as per the requirements of the players connected to the mixer.
 */

/*!
 * The PlayStream clients and Application sounds are mixed together in one pass.
 */
#define STM_SE_MIXER_SINGLE_STAGE_MIXING  0

/*!
 * The mixing stage is split into 2 cascaded stages :
 *     - a DVB mixer that mixes the PRIMARY and SECONDARY input
 *     - a System mixer that mixes the result of the DVB mixer with
 *           - any auxilliary decoded input
 *           - application sounds
 *           - system sounds
 *
 * The output of the DVB mixer may be postprocessed before being injected into the system-mixer.
 */
#define STM_SE_MIXER_DUAL_STAGE_MIXING    1


#define STM_SE_MIXER_MAX_TYPE STM_SE_MIXER_DUAL_STAGE_MIXING

#define STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS     4
#define STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS 8
#define STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS 8
#define STM_SE_MIXER_NB_MAX_OUTPUTS                  4

#define STM_SE_DUAL_STAGE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS 4
#define STM_SE_DUAL_STAGE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS 0


/*!
 * This type specifies the mixer object topology that should be created with
 * ::stm_se_advanced_audio_mixer_new.
 *
 * type defines the mixing-stage complexity.
 * nb_max_decoded_audio defines the maximum number of play-stream handled by the mixing-stage
 * nb_max_application_audio defines the maximum number of audio-generators handled by the mixing-stage
 * nb_max_interactive_audio defines the maximum number of audio-generators of BD application_type handled by the mixing-stage
 * nb_max_players defines the maximum number of audio-players that can be attached to the mixer object
 */
typedef struct
{
    uint32_t     type;
    uint32_t     nb_max_decoded_audio;
    uint32_t     nb_max_application_audio;
    uint32_t     nb_max_interactive_audio;
    uint32_t     nb_max_players;
} stm_se_mixer_spec_t;

/*!
 * This function allocates the fully specified audio mixer context.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[in] topology specifies the number and arrangement of the mixer internal processing nodes.
 *
 * \param[out] audio_mixer Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -ENOMEM Insufficient memory to complete operation.
 */
int             stm_se_advanced_audio_mixer_new(const char                *name,
                                                const stm_se_mixer_spec_t  topology,
                                                stm_se_audio_mixer_h      *audio_mixer);

/*!
 * This function allocates an audio mixer context.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[out] audio_mixer Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -ENOMEM Insufficient memory to complete operation.
 *
 * \deprecated Replaced by ::
 * stm_se_advanced_audio_mixer_new("name", STM_SE_MIXER_SIMPLE,
 *                                 STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS,
 *                                 STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS,
 *                                 STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS,
 *                                 STM_SE_MIXER_NB_MAX_OUTPUTS,
 *                                 &mixer)
 */
int             stm_se_audio_mixer_new(const char                     *name,
                                       stm_se_audio_mixer_h           *audio_mixer);


/*!
 * This function deletes an audio mixer context.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio mixer handle must have been previously provided by ::stm_se_audio_mixer_new and not deleted.
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 *
 * \retval 0 No error
 * \retval -EINVAL Audio mixer handle is obviously invalid (pre-condition violated, validity testing may be
 * incomplete).
 *
 * \retval -EBUSY Audio mixer is still in use.
 */
int             stm_se_audio_mixer_delete(stm_se_audio_mixer_h            audio_mixer);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Value to apply.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_mixer_set_control(stm_se_audio_mixer_h           audio_mixer,
                                               stm_se_ctrl_t                  ctrl,
                                               int32_t                        value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[out] value Pointer where currently applied control value can be stored.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_mixer_get_control(stm_se_audio_mixer_h           audio_mixer,
                                               stm_se_ctrl_t                  ctrl,
                                               int32_t                        *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_mixer_set_compound_control(stm_se_audio_mixer_h   audio_mixer,
                                                        stm_se_ctrl_t                   ctrl,
                                                        const void                     *value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_mixer_get_compound_control(stm_se_audio_mixer_h   audio_mixer,
                                                        stm_se_ctrl_t                   ctrl,
                                                        void                           *value);

/*!
 * This function connects an audio player to an audio mixer.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio mixer parameter must have been previously provided by ::stm_se_audio_mixer_new and not deleted.
 *
 * \pre The audio player parameter must have been previously provided by ::stm_se_audio_player_new and not deleted.
 *
 * \warning \note This function supports dynamic connection, allowing the audio player to be attached during
 *       operation. However, preliminary implementations may not support this feature and will defer the
 *       connection until the mixer becomes idle (no active inputs).
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] audio_player Previously allocated audio player handle.
 *
 * \retval 0 No errors
 * \retval -EALREADY The audio mixer is full. No further audio players can be attached to it.
 * \retval -ENOMEM The audio player is already connected to an audio mixer.
 */
int             stm_se_audio_mixer_attach(stm_se_audio_mixer_h               audio_mixer,
                                          stm_se_audio_player_h               audio_player);

/*!
 * This function disconnects an audio player from an audio mixer.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio mixer parameter must have been previously provided by ::stm_se_audio_mixer_new and not deleted.
 *
 * \pre The audio player parameter must have been previously provided by ::stm_se_audio_player_new and not deleted.
 *
 * \warning \note This function supports dynamic disconnection, allowing the audio player to be removed during
 *       operation. However, preliminary implementations will not support this feature and will defer the detach
 *       until the mixer becomes idle (no active inputs).
 *
 * \param[in] audio_mixer Previously allocated audio mixer handle.
 * \param[in] audio_player Previously allocated audio player handle.
 *
 * \retval 0 No errors
 * \retval -ENOTCONN The audio player is not attached to an audio mixer.
 */
int             stm_se_audio_mixer_detach(stm_se_audio_mixer_h               audio_mixer,
                                          stm_se_audio_player_h               audio_player);

/*! @} */ /* audio_mixer */

#endif /* STM_SE_AUDIO_MIXER_H_ */
