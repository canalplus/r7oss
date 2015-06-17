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

#ifndef STM_SE_ENCODE_H_
#define STM_SE_ENCODE_H_

/*!
 * \defgroup encode Encode
 *
 * A encode is a collection of encode streams that have coherent timing information.
 *
 * \section encode_events Events
 *
 * The encode object does not issue any events.
 *
 * \section encode_controls Controls
 *
 * The encode object supports the following control selectors:
 *
 * \subsection encode_core_controls Core controls
 *
 *  - ::STM_SE_CTRL_ENCODE_NRT_MODE
 *
 * \subsection encode_video_controls Video controls
 *
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE
 *  - ::STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT
 *
 * @{
 */

/*!
 * An encode handle is an opaque data type with which to identify encode object. It cannot be meaningfully
 * de-referenced outside of the streaming engine implementation.
 */
typedef void           *stm_se_encode_h;

/*!
 * This structure describes the state the audio encode stream process was in when this frame was encoded:
 */
typedef struct stm_se_encode_process_metadata_audio_s
{
    stm_se_audio_bitrate_control_t bitrate; //!< The bitrate control used
    stm_se_audio_scms_t            scms; //!< The Serial Copy Management System
    bool                           crc_on; //!< Whether CRC is computed and added to the stream for mpeg1/2 streams.
    bool                           dual_mono_forced; //!< Whether input data was considered as dual-mono independently of it metadata
    stm_se_dual_mode_t             dual_mode; //!< When encoding dual-mono audio, which of left and/or right channels are encoded
    int16_t                        loudness;
} stm_se_encode_process_metadata_audio_t;

/*!
 * This structure describes the state the video encode stream process was in when this frame was
 * encoded:.All refers to specific video encode controls (description embedded in control description)
 */
typedef struct stm_se_encode_process_metadata_video_s
{
    uint32_t                       bitrate_control_mode;
    uint32_t                       bitrate_control_value;
    stm_se_picture_resolution_t    encoded_picture_resolution;
    //later add stm_se_framerate_t             encoded_framerate;
    bool                           deinterlacing_on;
    bool                           noise_filtering_on;
    stm_se_video_multi_slice_t     multi_slice_mode;
    uint32_t                       gop_size;
    uint32_t                       cpb_size;
    uint16_t                       h264_level;
    uint16_t                       h264_profile;
} stm_se_encode_process_metadata_video_t;

/*!
 * This structure describes the encode stream process state for a given encoded frame.
 *
 * The 'update' flag warn encode client about a possible update in the encode process:
 *
 * Encode client only needs to parse the process metadata if the 'update' flag is true
 */
typedef struct stm_se_encode_process_metadata_s
{
    /*!
     * Indicates to the client of the encode stream that process metadata has been updated and should be parsed.
     *
     * True: audio and/or video process metadata has been updated
     *
     * False: audio and video process metadata are same as previous frame
     */
    bool                          updated_metadata;

    union
    {
        stm_se_encode_process_metadata_audio_t audio; //!< audio specific metadata
        stm_se_encode_process_metadata_video_t video; //!< video specific metadata
    };
} stm_se_encode_process_metadata_t;

/*!
 * This structure describes a compressed frame at the output of an encode stream; it describes both the
 * compressed frame and the process that lead to the frame encoding.
 *
 * \warning Stability: Unstable
 */
typedef struct stm_se_encoded_frame_metadata_s
{
    /*!
     * Describes the frame with enough information for multiplexing or for further processing, like
     * decoding.
     */
    stm_se_compressed_frame_metadata_t compressed_frame_metadata;

    /*!
     * Describes the status of the process that was used to encode the frame.
     *
     * It may be used by the client to help in the processing of the frame.
     */
    stm_se_encode_process_metadata_t encode_process_metadata;
} stm_se_encoded_frame_metadata_t;

/*!
 * This structure describes the internal data output by the encode coordinator for each frame to be encoded.
 *
 * \warning internal
 * This encode metadata type prefixed with underscores to indicate
 * it should not be exported beyond this module.
 */
typedef struct __stm_se_encode_coordinator_metadata_s
{
    /*!
     * Describes the unit in which encoded_time is expressed
     */
    stm_se_time_format_t            encoded_time_format;

    /*!
     * In case of tunneled transcode in NRT mode, carries the retimed presentation time stamp
     * all along the encode pipe. Is is expressed in the time unit given by encoded_time_format.
     *
     * This value, computed by the internal EncodeCoordinator object, is used to fill-in
     * the encoded_time field in the stm_se_compressed_frame_metadata_t struct associated
     * with each encoded frame.
     */
    uint64_t                        encoded_time;
} __stm_se_encode_coordinator_metadata_t;

/*!
 * This structure describes a frame inside an encode stream.
 * It describes both the frame and the different encode controls applied to that frame.
 *
 * \warning internal
 * This encode metadata type prefixed with underscores to indicate
 * it should not be exported beyond this module.
 */
typedef struct __stm_se_frame_metadata_s
{
    /*!
     * Describes the frame with enough information for multiplexing or for further processing, like
     * decoding.
     */
    union
    {
        stm_se_uncompressed_frame_metadata_t uncompressed_frame_metadata;
        stm_se_compressed_frame_metadata_t   compressed_frame_metadata;
    };

    /*!
     * Describes the status of the process that was used to encode the frame.
     *
     * It may be used by the client to help in the processing of the frame.
     */
    stm_se_encode_process_metadata_t        encode_process_metadata;

    /*!
     * Contains internal data output by the encode coordintator object and used for frame encoding.
     */
    __stm_se_encode_coordinator_metadata_t  encode_coordinator_metadata;
} __stm_se_frame_metadata_t;

/*!
 * This function allocates a new encode. The allocated encode is empty; it contains no encode streams.
 *
 * \note If the encode is required to start paused, then this action must be performed before any encode
 *       streams are added to the encode.
 *
 * \code
 * res = stm_se_encode_new ("main", &context->encode);
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
 * \param[out] encode Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -ENOMEM Insufficient memory to complete operation.
 */
int             stm_se_encode_new(const char                      *name,
                                  stm_se_encode_h                 *encode);

/*!
 * This function deletes a encode releasing all associated resources.
 *
 * \code
 * res = stm_se_encode_new(&encode);
 * if (0 != res) {
 *    return res;
 * }
 * res = stm_se_encode_stream_new("EncodeStream0",
 *                                encode,
 *                                STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3,
 *                                &stream);
 * if (0 != res) {
 *    int inner_res = stm_se_encode_delete(encode);
 *    BUG_ON(0 != inner_res); // handle known good!
 *          return res;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode parameter must have been previously provided by ::stm_se_encode_new and not deleted.
 *
 * \param[in] encode Previously allocated encode handle.
 *
 * \retval 0 No errors
 * \retval -EBUSY Allocated objects (play streams, etc.) attached to this encode preclude
 * deletion.
 */
int             stm_se_encode_delete(stm_se_encode_h                  encode);

/*!
* This function gets the value of an encode wide simple control such as encode memory profile
*
* \code
* res = stm_se_encode_get_control(
*         Context->Encode,
*         STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE,
*         &value);
* if (res < 0)
*     return res;
* \endcode
*
* \pre The streaming engine must be initialized.
* \pre The encode parameter must have been previously provided by ::stm_se_encode_new and not deleted.
*
* \param[in] encode Handle of encode to set the control.
* \param[in] ctrl Selector identifying the control to be updated.
* \param[out] value Pointer where currently applied control value can be stored.
*
* \retval 0 No error
* \retval -ENOTSUP Control type is not supported in this context.
*/
int             stm_se_encode_get_control(stm_se_encode_h                  encode,
                                          stm_se_ctrl_t                    ctrl,
                                          int32_t                         *value);

/*!
 * This function sets the value of a encode wide option such as encode memory profile
 *
 * \code
 * res = stm_se_encode_set_control(
 *         Context->Encode,
 *         STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE,
 *         STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE);
 * if (res != 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The encode parameter must have been previously provided by ::stm_se_encode_new and not deleted.
 *
 * \param[in] encode Handle of encode to set the control.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Value to apply.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_encode_set_control(stm_se_encode_h                  encode,
                                          stm_se_ctrl_t                    ctrl,
                                          int32_t                          value);

/*! @} */ /* encode */

#endif /* STM_SE_ENCODE_H_ */
