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
#ifndef STM_SE_AUDIO_READER_H_
#define STM_SE_AUDIO_READER_H_

/*!
 * \defgroup audio_reader Audio reader
 *
 * An audio reader object represents the input to a PCM or SPDIF reader.
 *
 * \section audio_reader_controls Controls
 *
 * The audio reader object supports the following classes of control selector:
 *
 *  - ::STM_SE_CTRL_AUDIO_READER_SOURCE_INFO
 *
 * \note All controls supported by the audio reader object are compound controls and, therefore, no API is
 *       provided to manipulate simple controls.
 *
 * @{
 */

/*!
 * An audio reader handle is an opaque data type with which to identify an audio reader. It cannot be
 * meaningfully de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_audio_reader_h;

/*!
 * This function allocates an audio reader context.
 *
 * The mapping between the hw_name and the associated PCM or SPDIF reader resource is implementation
 * dependant. On Linux systems, it is derived from the ALSA configuration:
 *
 * \code
 * res = stm_se_audio_reader_new("HDMI-RX", "hw:STiH407,6", &hdmi_reader);
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[in] hw_name Name to identify the hardware resource linked to this audio reader.
 * \param[out] audio_reader Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -EINVAL Hardware name value is invalid on this platform.
 * \retval -ENOMEM Insufficient memory to complete operation.
 */
int             stm_se_audio_reader_new(const char                         *name,
                                        const char                          *hw_name,
                                        stm_se_audio_reader_h               *audio_reader);

/*!
 * This function deletes an audio reader context.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio reader parameter must have been previously provided by ::stm_se_audio_reader_new and not deleted.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 *
 * \retval 0 No error
 * \retval -EBUSY Audio reader is still in use.
 */
int             stm_se_audio_reader_delete(stm_se_audio_reader_h              audio_reader);

/*!
 * This function connects an audio reader to a play stream.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio reader parameter must have been previously provided by ::stm_se_audio_reader_new and not deleted.
 *
 * \pre The play stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] play_stream Previously allocated play stream handle.
 *
 * \retval 0 No errors
 * \retval -EALREADY The audio reader is already connected to a play stream.
 * \retval -ENOMEM The play stream is already connected to an audio reader.
 */
int             stm_se_audio_reader_attach(stm_se_audio_reader_h              audio_reader,
                                           stm_se_play_stream_h                play_stream);

/*!
 * This function disconnects an audio reader to a play stream.
 *
 * \pre The streaming engine must be initialized.
 * \pre The audio reader parameter must have been previously provided by ::stm_se_audio_reader_new and not deleted.
 *
 * \pre The play stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] play_stream Previously allocated play stream handle.
 *
 * \retval 0 No errors
 * \retval -ENOTCONN The audio reader is not connected to a play stream.
 */
int             stm_se_audio_reader_detach(stm_se_audio_reader_h              audio_reader,
                                           stm_se_play_stream_h                play_stream);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_reader_set_compound_control(stm_se_audio_reader_h    audio_reader,
                                                         stm_se_ctrl_t             ctrl,
                                                         const void                *value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_reader_get_compound_control(stm_se_audio_reader_h    audio_reader,
                                                         stm_se_ctrl_t             ctrl,
                                                         void                      *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_reader_set_control(stm_se_audio_reader_h     audio_reader,
                                                stm_se_ctrl_t             ctrl,
                                                const int32_t             value);
/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] audio_reader Previously allocated audio reader handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_audio_reader_get_control(stm_se_audio_reader_h     audio_reader,
                                                stm_se_ctrl_t             ctrl,
                                                int32_t                  *value);

/*! @} */ /* audio_reader */

#endif /* STM_SE_AUDIO_READER_H_ */
