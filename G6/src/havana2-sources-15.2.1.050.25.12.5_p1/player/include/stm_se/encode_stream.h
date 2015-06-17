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
#ifndef STM_SE_ENCODE_STREAM_H_
#define STM_SE_ENCODE_STREAM_H_

/*!
 * \defgroup encode_stream Encode stream
 *
 * An encode stream is responsible for pre-processing and encoding uncompressed data.
 * It provides ES + metadata (in particular timing information like PTS).
 * All encode streams are members of one encode.
 * All controls and compound controls should be set before injecting frames.
 *
 * \section encode_stream_controls Controls
 *
 * The encode stream object supports the following control selectors:
 *
 * \subsection encode_video_controls Video controls
 *
 *  - ::STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE
 *  - ::STM_SE_CTRL_ENCODE_STREAM_BITRATE

 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING
 *
 * \subsection encode_audio_controls Audio controls
 *
 *  - ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT     (compound - preproc level)
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO              (preproc level)
 *  - ::STM_SE_CTRL_DUALMONO                            (preproc level)

 *  - ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL (compound - coder level)
 *  - ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL   (coder level)
 *  - ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS            (coder level)
 *  - ::STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE             (coder level)
 *
 * @{
 */

/*!
 * A encode stream handle is an opaque data type with which to identify encode stream. It cannot be
 * meaningfully de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_encode_stream_h;

/*!
 * This type enumerates the events that can be issued by an encode stream.
 */
typedef enum stm_se_encode_stream_event_e
{
    /*!
     * This event is not a valid / functional event and should never occur.
     */
    STM_SE_ENCODE_STREAM_EVENT_INVALID                      = (0),

    /*!
     * This event reports a serious internal failure. A serious internal failure indicates the encode stream
     * object has been damaged beyond repair.
     *
     * The event implies a serious bug in the implementation from which no internal recovery is possible.
     *
     * If this event is observed during development, then it should be treated as a bug in the drivers.
     *
     * If observed in a deployed system, then the application should, at minimum, delete the current encode and
     * all encode streams contained within it. Another alternative would simply be to trigger a watchdog reset.
     *
     * The application may also choose to implement postmortem or failure reporting logic.
     */
    STM_SE_ENCODE_STREAM_EVENT_FATAL_ERROR                  = (1 << 0),

    /*!
     * This event reports that the hardware has entered an unrecoverable failure state.
     *
     * If this event is observed during development, then it should be treated as a system level bug (that is,
     * encompassing the entire system, from board design and bootstrap logic, to software and hardware
     * implementation).
     *
     * If observed in a deployed system, then the application has no alternative but to deploy a watchdog reset.
     *
     * The application may also choose to implement postmortem or failure reporting logic.
     */
    STM_SE_ENCODE_STREAM_EVENT_FATAL_HARDWARE_FAILURE       = (1 << 1),

    /*!
     * This event acts as a wake up alarm and fires every time an encoded frame is emitted by the encoder.
     * Warning, this event is not generated for 2 specific cases involving fake buffers being output from encoder:
     * Video encode frame skipped: see dedicated event below.
     * 'End Of Stream' buffer: this buffer is a fake buffer (0 payload) highlighting last frame of the sequence has been encoded.
     *
     * This event is of no semantic importance and no application action is required after receiving it.
     * However, subscribing to this event may be required to meet specific middleware requirements.
     */
    STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED                = (1 << 2),

    /*!
     * This event acts as a wake up alarm and fires every time a video frame is skipped by encoder.
     * Frame can be skipped to match bitrate constraints.
     * In that case, 'STM_SE_ENCODE_STREAM_EVENT_VIDEO_FRAME_SKIPPED' event is generated
     * and an empty frame (0 size payload) is output from video encoder with discontinuity metadata
     * indicating this is an encode frame skipped
     *
     * This event is of no semantic importance and no application action is required after receiving it.
     * However, subscribing to this event may be required to meet specific middleware requirements.
     */
    STM_SE_ENCODE_STREAM_EVENT_VIDEO_FRAME_SKIPPED          = (1 << 3)
} stm_se_encode_stream_event_t;

/*!
 * This function allocates a encode stream and associates it with an encode object. All of the necessary
 * management objects and the internal encoder components (pre-processor and coder) are allocated during
 * this call. The default stream controls will be applied and buffers allocated.
 *
 * \code
 * res = stm_se_encode_stream_new("audio0", encode,
 *                           STM_SE_STREAM_ENCODING_AUDIO_AC3,
 *                           &stream);
 * if (0 != res) {
 *    return res;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode parameter must have been previously provided by ::stm_se_encode_new and not deleted.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[in] encode Handle of encode to which the encode stream is to be associated.
 * \param[in] encoding For example, ::STM_SE_STREAM_ENCODING_VIDEO_H264
 * \param[out] encode_stream Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -EINVAL Encoding value is out of range.
 * \retval -ENOMEM Insufficient resources to complete operation.
 */
int             stm_se_encode_stream_new(const char                      *name,
                                         stm_se_encode_h                  encode,
                                         stm_se_encode_stream_encoding_t  encoding,
                                         stm_se_encode_stream_h          *encode_stream);

/*!
 * This function deletes a encode stream by disassociating it from its encode and releasing all associated
 * resources.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_encode_stream_new and not deleted.
 *
 * \param[in] encode_stream Handle of stream to be removed.
 *
 * \retval 0 No error
 * \retval -EBUSY Encoder stream is still is use (connection to another object still exists)
 */
int             stm_se_encode_stream_delete(stm_se_encode_stream_h           encode_stream);

/*!
 * This function connects a sink object to a encode stream.
 *
 * The sink object must support generic memory transfer interfaces. Supported sink objects include:
 *
 * * Memory sink (from Infrastructure)
 *
 * * Multiplexor (from Transport Engine)
 *
 * * Play stream (from Streaming Engine)
 *
 * \note Connecting an encode_stream to a play_stream is possible, for testing, but is not otherwise
 *       recommended since the encode_stream input can already be directly rendered.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_encode_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by another STKPI function and not deleted.
 *
 * \param[in] encode_stream Handle of encode stream to connect the sink to.
 * \param[in] sink Handle of an object that can accept data from a encode stream.
 *
 * \retval 0 No errors
 * \retval -EINVAL Sink object type cannot be attached to this encode stream.
 * \retval -EALREADY The encode stream is already connected to a sink object.
 */
int             stm_se_encode_stream_attach(stm_se_encode_stream_h           encode_stream,
                                            stm_object_h                     sink);

/*!
 * This function disconnects a sink object from an encode stream.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_encode_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by another STKPI function and not deleted.
 * \pre The encode stream must have been previously attached to the provided sink by stm_se_encode_stream_attach,
 * and not already detached.
 *
 * \param[in] encode_stream Handle of encode stream to disconnect the sink from.
 * \param[in] sink Handle of sink object to be disconnected from the encode stream.
 *
 * \retval 0 No errors
 * \retval -ENOTCON The connection does not exist.
 */
int             stm_se_encode_stream_detach(stm_se_encode_stream_h           encode_stream,
                                            stm_object_h                     sink);

/*!
 * This function gets the value of a encode stream specific simple control such as the encoder bitrate.
 *
 * \code
 * res = stm_se_encode_stream_get_control(
 *         Context->EncodeStream,
 *         STM_SE_CTRL_ENCODE_STREAM_BITRATE,
 *         &value);
 * if (res < 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode stream parameter must have been previously provided by ::stm_se_encode_stream_new and not
 * deleted.
 *
 * \param[in] encode_stream Handle of playback to set the control.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[out] value Pointer where currently applied control value can be stored.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_encode_stream_get_control(stm_se_encode_stream_h           encode_stream,
                                                 stm_se_ctrl_t                    ctrl,
                                                 int32_t                         *value);

/*!
 * This function sets the value of a encode stream specific simple control such as the encoder bitrate.
 *
 * \code
 * res = stm_se_encode_stream_set_control(
 *         Context->EncodeStream,
 *         STM_SE_CTRL_ENCODE_STREAM_BITRATE,
 *         2000000);
 * if (res != 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode stream parameter must have been previously provided by ::stm_se_encode_stream_new
 * and not deleted.
 *
 * \param[in] encode_stream Handle of encode stream to set the control.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Value to apply.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_encode_stream_set_control(stm_se_encode_stream_h           encode_stream,
                                                 stm_se_ctrl_t                    ctrl,
                                                 int32_t                          value);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode stream parameter must have been previously provided by ::stm_se_encode_stream_new
 * and not deleted.
 *
 * \param[in] encode_stream Previously allocated encode stream handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_encode_stream_get_compound_control(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_ctrl_t                    ctrl,
                                                          void                            *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode stream parameter must have been previously provided by ::stm_se_encode_stream_new
 * and not deleted.
 *
 * \param[in] encode_stream Previously allocated encode stream handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_encode_stream_set_compound_control(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_ctrl_t                    ctrl,
                                                          const void                      *value);

/*!
 * This function directs the encode stream to encode or discard all buffered data.
 * stm_se_encode_stream_drain is a blocking call and returns when all data have been discarded or passed to encoder client.
 *
 * \warning This function is not implemented
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_encode_stream_new and not deleted.
 *
 * \param[in] encode_stream Handle of stream to drain.
 * \param[in] discard False = encode all buffered data, True = discard buffered data.
 *
 * \retval 0 No error.
 */
int             stm_se_encode_stream_drain(stm_se_encode_stream_h           encode_stream,
                                           bool                             discard);

/*!
 * This function injects an uncompressed audio or video frame to an encode stream object,
 * in order to have it preprocessed, encoded then provided to encoder client
 *
 * The call is a blocking call, meaning it will only return when the frame has been consumed and may be
 * reused by the application.
 *
 * The data pointer must be directly addressable in the current execution context. Thus, for a Linux kernel
 * client, the pointer should be a kernel virtual address.
 *
 * Due to h/w constraints in video pre-processing unit, for video data the given frame_physical_address
 * param must be 16-bytes aligned in memory.
 *
 * Typically, this function is called by a connected source such as a play stream or memory source object.
 * However, it can also be called directly providing the encode stream is not attached to any source.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_encode_stream_new and not deleted.
 *
 * \pre A source object must not be attached to the encode stream.
 * \pre The descriptor parameter must correctly describe the frame of data.
 *
 * \param[in] encode_stream Handle of stream to inject data to.
 * \param[in] frame_virtual_address virtual address of the contiguous frame of picture/audio data.
 * \param[in] frame_physical_address Physical address of the contiguous frame of picture/audio data.
 * \param[in] frame_length Size of the frame in Number of bytes to transfer.
 * \param[in] metadata Meta-data describing the properties of the injected frame.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid parameter value.
 */
int             stm_se_encode_stream_inject_frame(stm_se_encode_stream_h           encode_stream,
                                                  const void                      *frame_virtual_address,
                                                  unsigned long                    frame_physical_address,
                                                  uint32_t                         frame_length,
                                                  const stm_se_uncompressed_frame_metadata_t metadata);

/*!
 * This function informs the encoder of a discontinuity in the injection process
 *
 * Following discontinuities can be injected in an encode stream:
 *
 * "STM_SE_DISCONTINUITY_DISCONTINUOUS" indicates there is a time discontinuity, then a break in the PTS from the next frame
 *
 * "STM_SE_DISCONTINUITY_EOS" indicates an End Of Stream, meaning that the stream to be encoded is entirely injected.
 * Encode client can then monitor encoder output frame metadata to catch the EOS, thus signaling last encoded frame of the stream.
 * To be noted that for EOS, compressed buffer payload size will be '0' with suitable metadata.
 *
 * Following discontinuity is specific to video encode stream:
 *
 * "STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST" triggers a request for the next injected video frame to be encoded as an IDR.
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode stream parameter must have been previously provided by ::stm_se_encode_stream_new and not
 * deleted.
 *
 * \param[in] encode_stream Handle of encode stream to inject discontinuity to.
 * \param[in] discontinuity Information on the type of discontinuity
 *
 * \retval 0 No error.
 */
int             stm_se_encode_stream_inject_discontinuity(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_discontinuity_t    discontinuity);

/*! @} */ /* encode_stream */

#endif /* STM_SE_ENCODE_STREAM_H_ */
