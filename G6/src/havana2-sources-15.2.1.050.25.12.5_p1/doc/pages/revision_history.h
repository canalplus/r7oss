/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as published
by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine alternatively be licensed under a proprietary
license from ST.

Source file name : stm_se/pages/revision_history.h

************************************************************************/

#ifndef STM_SE_PAGES_REVISION_HISTORY_H__
#define STM_SE_PAGES_REVISION_HISTORY_H__

/* Nota: Doxygen use of latex to pdf does not work well for tables.. */
/*!
 * \page revision_history Revision history
 *
 * Document revision history.
 *
 * <table border=0>  <tr><th>v1.0 17-Jan-2011</th></tr>
 *          <tr><td><ul>
 * <li>Initial revision
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.1 21-Jan-2011</th></tr>
 *          <tr><td><ul>
 * <li>Documented the events that a play stream can issue
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.2 20-Mar-2011</th></tr>
 *          <tr><td><ul>
 * <li>Internal release
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.3 27-May-2011</th></tr>
 *          <tr><td><ul>
 * <li>Prepared for internal review, adoption of STKPI event model
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.4 22-Jun-2011</th></tr>
 *          <tr><td><ul>
 * <li>Acted upon review comments, adoption of STKPI stylistic conventions
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.5 14-Oct-2011</th></tr>
 *          <tr><td><ul>
 * <li>Refactorized of stm_se_play_stream_encoding_t constants
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.6 30-Apr-2012</th></tr>
 *          <tr><td><ul>
 * <li>Consolidated revision history tables
 * <li>Updated headers and footers and disclaimer
 * <li>Updated Description in section 3.11.8.9
 *          </ul></ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.7 12-Jun-2012</th></tr>
 *          <tr><td><ul>
 * <li>Done minor corrections made throughout the document
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.8 03-Aug-2012</th></tr>
 *          <tr><td><ul>
 * <li>Promoted ::stm_se_audio_channel_assignment_t from the control specific type to a sub-system wide data type
 * <li>Promoted stm_se_emphasis_mode_t from the control specific type to a sub-system wide data type
 * <li>Re-instated ::STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
 * <li>Added ::STM_SE_CTRL_AUDIO_SERVICE_TYPE
 * <li>Added ::STM_SE_CTRL_AUDIO_SUBSTREAM_ID
 * <li>Tidied up audio channel description in STM_SE_CTRL_AUDIO_GENERATOR_FORMAT
 * <li>Added ::STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_GRAIN Add STM_SE_CTRL_AUDIO_MIXER_INPUT_IS_MASTER
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_ROUTING
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_INPUT_COEFFICIENTS
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_OUTPUT_GAIN
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_APPLY_CLEAN_AUDIO
 * <li>Added ::STM_SE_CTRL_AUDIO_MIXER_ASSOCIATE_SECONDARY 
 * <li>Replaced ::STM_SE_CTRL_COMPRESSION_MODE with ::STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION
 * <li>Added ::STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE
 * <li>Added ::STM_SE_CTRL_SPEAKER_CONFIG
 * <li>Added ::STM_SE_CTRL_AUX_SPEAKER_CONFIG 
 * <li>Added ::STM_SE_CTRL_STREAM_DRIVEN_STEREO
 *          </ul></td></tr>
 * </table>
 * <table>  <tr><th>v1.9 28-Sep-2012</th></tr>
 *          <tr><td><ul>
 * <li>Clarified PROGRAM_LEVEL Control
 * <li>Modified "multi-provider" pcmprocessing control API so that type be out of the union
 * <li>Updated Dolby-volume API
 * <li>Added a "Sink Type" control to hint the driver about what sink appliance the player is connected to
 * <li>Markers and alarms: Added set_alarm function and message + application note section (to be filled)</li>
 * <li>Encoders: Partially updated ::stm_se_encode_stream_inject_frame with virtual and physical address +
 * application note section (to be filled), replaced frame descriptor by uncompressed meta data structures,
 * added compressed meta data structures</li>
 * <li>Audio and video grab app note: added section to be filled 
 * <li>Controls: Added live_play & Playback latency to the SE and playback controls list, but not described
 * <li>Controls: Added ::STM_SE_CTRL_ERROR_DECODING_LEVEL, ::STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION,
 * ::STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD,
 * ::STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD to streaming engine, playback, play stream
 * controls list, but not described
 * <li>Deprecated: stm_se_play_stream_aspect_ratio_t,
 * stm_se_play_stream_scan_type_t, stm_se_play_stream_format_3d_t, replaced by ::stm_se_aspect_ratio_t,
 * ::stm_se_scan_type_t, ::stm_se_format_3d_t
 * <li>Added the ::STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC and ::STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION
 * play_stream events
 * <li>Removed the ::STM_SE_CTRL_ERROR_DECODING_LEVEL
 *          </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.10 </th></tr>
 *         <tr><td><ul>
 * <li>Deprecated stm_se_play_stream_encoding_mode_t
 * <li>Formalized the concept of banked compound controls (were introduced in SDK2-06.5 but without a generalized description)
 * <li>Reworked STM_SE_CTRL_LIMITER into STM_SE_CTRL_AUDIO_GAIN, STM_SE_CTRL_AUDIO_DELAY, STM_SE_CTRL_AUDIO_SOFTMUTE 
 * <li>Removed struct-sizes to compound controls
 * <li>Filled markers application notes section
 * <li>Updated the markers detection application note after review with Roselyne and Gilles
 * <li>Removed the address parameter from the stm_se_play_stream_set_alarm (the uint8_t is though as being enough)
 * <li>Renamed STM_SE_PLAY_STREAM_ALARM_PARSED_PTS into STM_SE_PLAY_STREAM_ALARM_NEXT_PARSED_PTS
 * <li>Updated the stm_se_play_stream_switch faulty description
 * <li>Added end_of_stream parameter to the stm_se_play_stream_inject_discontinuity
 * <li>reported the stm_se.h encoders definitions and controls, but with a large correction in the namings .... <li> Added marker end of stream <li> Removed weird duplication of following controls description STM_SE_CTRL_TRICK_MODE_DOMAIN, STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED, STM_SE_CTRL_DECIMATE_DECODER_OUTPUT [3] Added stm_se_surface_format_t, stm_se_picture_rectangle_t, stm_se_video_parameters_t, stm_se_compressed_frame_metadata_video_t <li> Completed stm_se_uncompressed_frame_metadata_video_t <li> Added STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, STM_SE_CTRL_ENCODE_STREAM_BITRATE <li> removed STM_SE_CTRL_ENCODE_AUDIO_CONTROL, STM_SE_CTRL_ENCODE_VIDEO_CONTROL <li> Added STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, STM_SE_CTRL_VIDEO_ENCODE_FRAMERATE, STM_SE_CTRL_VIDEO_ENCODE_DEINTERLACING, STM_SE_CTRL_VIDEO_ENCODE_NOISE_FILTERING, STM_SE_CTRL_VIDEO_ENCODE_GOP_SIZE, STM_SE_CTRL_VIDEO_ENCODE_MULTI_SLICE, STM_SE_CTRL_H264_ENCODE_CPB_SIZE, STM_SE_CTRL_H264_ENCODE_LEVEL, STM_SE_CTRL_H264_ENCODE_PROFILE
 * <li>Added stm_se_compressed_frame_metadata_audio_t and stm_se_compressed_frame_metadata_t sections. Details to be filled <li>moved some freshly added encoded parameters from uint16_t to uint32_t <li>moved compressed/uncompressed meta data definition structures to common type definitions
 *          </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.10 (cont) </th></tr>
 *         <tr><td><ul>
 * <li>Put _uncompressed_metadata_ structures before _compressed_metadata_ <li>Added definition of stm_se_audio_pcm_format_t <li>Filled stm_se_uncompressed_frame_metadata_audio_t, stm_se_uncompressed_frame_metadata_t, stm_se_compressed_frame_metadata_audio_t, stm_se_compressed_frame_metadata_t <li>Removed referenced to nonexistent audio controls in Encode Stream > Controls <li>Replaced STM_SE_CTRL_STREAM_BITRATE_MODE and STM_SE_CTRL_STREAM_BITRATE, by existing STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, STM_SE_CTRL_ENCODE_STREAM_BITRATE: it is compliant with our decision to segregate player and encoder controls. <li>Filled STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, STM_SE_CTRL_ENCODE_STREAM_BITRATE <li>Added PROPOSED stm_se_encode_process_metadata_audio_t, stm_se_encode_process_metadata_video_t, stm_se_encode_process_metadata_t, stm_se_encoded_frame_metadata_t. <li>Filled PROPOSED stm_se_encode_process_metadata_audio_t, stm_se_encode_process_metadata_t, stm_se_encoded_frame_metadata_t. <li>Added and filled PROPOSED stm_se_audio_bitrate_control_t, stm_se_audio_scms_t, stm_se_audio_channel_id_t, stm_se_audio_channel_placement_t, stm_se_audio_core_format_t
 *          </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.11 (version for SDK2.08)</th></tr>
 *         <tr><td><ul>
 * <li>add stm_se_colorspace_t <li>add stm_se_discontinuity_t <li>update stm_se_uncompressed_frame_metadata_video_t <li>update stm_se_encode_process_metadata_video_t <li>add stm_se_encode_stream_inject_discontinuity <li>update Cascaded video encode controls
 * <li>Remove comments on window_of_interest from stm_se_uncompressed_frame_metadata_video_t + update index table
 * <li>Updates after meeting review with Dan/Philippe/Charles Updates highlighted as 'Word Review'
 * <li>*Removes* old Audio Encoder APIs we are deprecating: (not sure how we should handle them in the doc: keep old fields, mark each field as *DEPRECATED*?). *Adds* new Audio Encoder API - stm_se_audio_pcm_format_t : removed extraneous "store number" in names - stm_se_uncompressed_metadata_audio_t : replaced old structure definition with new one. - replaces stm_se_encode_stream_encoding_t with stm_se_stream_encoding_t - stm_se_uncompressed_metadata_t : replaces old structure definition with new one. - stm_se_compressed_metadata_audio_t : replaces old structure definition with new one. - stm_se_compressed_metadata_t : replaces old structure definition with new one. - Adds Audio Encode Controls: STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL, STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT, STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS, STM_SE_CTRL_AUDIO_ENCODE_DUALMONO_OVERRIDE_ENABLE, STM_SE_CTRL_AUDIO_ENCODE_DUAL_MONO_OUT_MODE, STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE, STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL
 * <li>Restores discontinuity field in stm_se_uncompressed_frame_metadata_t (removed by mistake in previous version) Moves discontinuity field from stm_se_compressed_frame_metadata_video_t to stm_se_compressed_frame_metadata_t since it should be common to audio and video minor description changes for discontinuity fields
 * <li>Add STM_SE_CTRL_VALUE_FIRST_KEY_FRAME_ONLY in STM_SE_CTRL_VIDEO_DISCARD_FRAMES policy
 * <li>Review classification of every not-Stable API
 *         </td></tr>
 * </table>
 * <table> <tr><th>v1.12 (version for SDK2.09)</th></tr>
 *         <tr><td><ul>
 * <li>Moved STM_SE_CTRL_AAC_DECODER_CONFIG to specific new section : Compound audio policy controls. Indeed the compound controls are not cascadable. <li>Created Compound video policy control: STM_SE_CTRL_MPEG2_TIME_CODE read only compound control aimed to provide to the client the MPEG2 time code
 * <li>Added frame_rate to the stm_se_play_stream_video_parameters_t, to unfortunately resolve bug 23340
 * <li>Fixed typo error stm_se_encode_inject_discontinuity -> stm_se_encode_stream_inject_discontinuity
 * <li>clarify AudioGenerator API (aligning it to encode-stream definitions) minor fixes
 * <li>AudioEncApiRealityCheck	Modified the Audio Encoding APIs to reflect what we actually decided to implement. - stm_se_audio_channel_placement_t : structure with channel_count - stm_se_audio_channel_id_t: removal of "END" id - stm_se_audio_bitrate_t: removed union between vbr_quality_factor and bitrate compressed_frame_metadata_t - adds back system_time as Reserved unstable - renames timestamps to currents native_time, native_time_format - marked encoding as Proposed: as we have not yet changed the code from stm_se_encode_stream_encoding_t to stm_se_stream_encoding_t
 * <li>Create video control STM_SE_CTRL_NEXT_PLAYSTREAM_NO_DECODE_BUFFER_ALLOCATION. Create video control compound STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS and STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS All these controls are relative to "user decode buffer allocation" for "zero copy grab". <li>Add play stream event STM_SE_PLAY_STREAM_EVENT_END_OF_DECODE. <li>In the Application notes section, add sub-section "Memory source/sink grab" and "Zero copy grab"
 * <li>Added MVC and HEVC video codec types replaced STM_SE_CTRL_NEXT_PLAYSTREAM_NO_DECODE_BUFFER_ALLOCATION by STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED STM_SE_PLAY_STREAM_EVENT_END_OF_DECODE replaced by STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.13 02-Jul-2013 (version for SDK2.10)</th></tr>
 *         <tr><td><ul>
 * <li>Reviewed and Updated zero copy grab
 * <li>Set message STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED to temporary state, and to be deprecated
 * <li>replaced remaining STM_SE_PLAY_STREAM_EVENT_END_OF_DECODE definition by STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM
 * <li>Added STM_SE_CTRL_REDUCE_COLLATED_DATA policy control
 * <li>Re-word some of the new text. Remove to concept of "write-only" controls (unacceptable impediment to debugging). Remove the idea of an "apply only" control (now inherited from play_stream). Make the NEGOTIATE/SHARE_DECODED_BUFFERS generic for audio and video
 * <li>Corrected typo related to NEGOTIATE/SHARE VIDEO BUFFER CONTROLS Moved "proposed" statement for negotiate and share video buffers controls to Unstable
 * <li>Updated AudioGenerator API as per implementation
 * <li>Added BTSC
 * <li>Added AVSync extra outputrate debug info gradients in struct statistics_s
 * <li>Added event ::STM_SE_PLAY_STREAM_EVENT_PLAY_STREAM_SWITCH_COMPLETE
 * <li>Added control ::STM_SE_CTRL_PACKET_INJECTOR
 * <li>Added event ::STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED
 * <li>Added new api ::stm_se_get_compound_control
 * <li>Added control ::STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE 
 * <li>Added control ::STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE (not implemented yet)
 * <li>Added control ::STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE (not implemented yet
 * <li>Added control ::STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE (not implemented yet
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.14 (version for SDK2.11) </th></tr>
 *         <tr><td><ul>
 * <li>added callback ::releaseCaptureBuffer to release SE buffer after Display
 * <li>Added AVSync error in outputrate debug info gradients in struct statistics_s
 * <li>Streaming engine API : correct set_interval function
 * <li>Removed the zero copy video grab controls centricity. STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS -> STM_SE_CTRL_NEGOTIATE_DECODE_BUFFERS STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS -> STM_SE_CTRL_SHARE_DECODE_BUFFERS
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.15 (version for SDK2.12) </th></tr>
 *         <tr><td><ul>
 * <li>Added a compound control ::STM_SE_CTRL_MPEG2_TIME_CODE ::stm_se_ctrl_mpeg2_time_code_t for obtaining the latest time-code
 * information present in the Group of Pictures header of an MPEG-2 stream
 * <li>Introduced ::STM_SE_PLAY_MAIN_COMPRESSED_SD_OUT value to ::STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE STKPI control
 * <li>Removed unwanted pointer to ::stm_se_encode_stream_h user of APIs was already using the right form
 * <li>Changed default mixer grain ::SND_PSEUDO_MIXER_DEFAULT_GRAIN to 768 samples (15ms @ 48kHz)
 * <li>Modified stm_se_encode_stream_inject_frame() API documentation, to expose video pre-processor H/W memory constraints
 * <li>Returned error in Preproc_Video_c::Input() if input buffer address is not 16-bytes aligned
 * <li>Cleaned-up encoder controls documentation (list of controls supported audio & video)
 * <li>Removed unimplemented encode apis: stm_se_encode_attach, stm_se_encode_detach, stm_se_encode_get_control, stm_se_encode_set_control, stm_se_encode_get_clock_data_point
 * <li>Fixed values supported by ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
 * <li>Removed obsolete audio encode controls ::STM_SE_CTRL_AUDIO_DUALMONO_METADATA_OVERRIDE
 * <li>Removed obsolete audio encode controls ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_CHANNEL_MAP, ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_SAMPLING_FREQUENCY
 * <li>Removed obsolete audio encode controls ::STM_SE_CTRL_AUDIO_ENCODE_STREAM_VBR_Q
 * <li>Added ::STM_SE_CTRL_VALUE_PLAYOUT and ::STM_SE_CTRL_VALUE_DISCARD for controls STM_SE_CTRL_PLAYOUT_ON_[TERMINATE/SWITCH/DRAIN]
 * <li>Removed ::STM_SE_CTRL_AUDIO_MIXER_ENABLE_CLEANAUDIO stm_se_apply_cleanaudio_t, ::STM_SE_CTRL_AUDIO_MIXER_ROLE, ::STM_SE_CTRL_AUDIO_MIXER_PRIORITY
 * <li>Cleaned and reordered controls and control values
 * <li>Updated list of controls supported by mixer, player, playback, etc
 * <li>Added values ::STM_SE_CTRL_VALUE_TRICK_AUDIO_MUTE and ::STM_SE_CTRL_VALUE_TRICK_AUDIO_PITCH_CORRECTED for control ::STM_SE_CTRL_TRICK_MODE_AUDIO
 * <li>Removed ::STM_SE_CTRL_PLAY_STREAM_AUDIOMODE, ::STM_SE_CTRL_AUDIO_DECODER_CRC_ENABLE, ::STM_SE_CTRL_AUDIO_DECODER_LFE_ENABLE
 * <li>Removed ::STM_SE_CTRL_SRS_ and ::STM_SE_CTRL_IEC958_x controls
 * <li>Added ::STM_SE_CTRL_DEEMPHASIS; updated ::STM_SE_CTRL_VIRTUALIZER, ::STM_SE_CTRL_VOLUME_MANAGER, ::STM_SE_CTRL_UPMIXER, ::STM_SE_CTRL_ST_MDRC
 * <li>Updated statistics_s for hevc decoder
 * <li>Updated stm_se_limiter_s to add apply field
 * <li>Added memory footprint limitation control ::STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE with profiles HD (default), 4K2K
 * <li>Renamed struct stm_se_memsink_pull_buffer_t to ::stm_se_capture_buffer_t
 * <li>Documentated video error concealments and FrameRate controls
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.16 (version for SDK2.13) </th></tr>
 *         <tr><td><ul>
 * <li>Added module_params/sysfs entries to select Transformer for each audio Mixer
 * <li>Added SD, 720p, UHD, HD_legacy memory profiles associated to control ::STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE
 * <li>Added new encode control for memory optimization ::STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE with profiles CIF, SD, 720p, HD (default)
 * <li>Added new encode control ::STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT for input color format injected to a video encoder
 * <li>Added new video stream type ::STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP
 * <li>Added new video stream type ::STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED
 * <li>Added new video encode control for window of interest cropping ::STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING
 * <li>Added Adsmart API ::stm_se_play_stream_set_alarm, ::stm_se_play_stream_set_discard_trigger, ::stm_se_play_stream_reset_discard_triggers, updated ::stm_se_play_stream_msg_id_t, ::stm_se_play_stream_alarm_t, ::stm_se_play_stream_msg_t added ::stm_se_play_stream_alarm_pts_t, ::stm_se_play_stream_trigger_type_t, ::stm_se_play_stream_pts_and_tolerance_t, ::stm_se_play_stream_discard_trigger_t
 * <li>Added encoded_time information in compressed metadata ::stm_se_compressed_frame_metadata_t
 * <li>Removed obsolete APIs stm_se_playback_get_player_environment(), stm_se_play_stream_get_player_environment()
 * <li>Removed obsolete APIs stm_se_play_stream_capture_on(), stm_se_play_stream_capture_off() stm_se_play_stream_capture_latency()
 * <li>Removed obsolete APIs stm_se_play_stream_channel_select(), stm_se_play_stream_inject_data_packet()
 * <li>Removed obsolete APIs stm_se_play_stream_alloc_decode_buffer(), stm_se_play_stream_free_decode_buffer()
 * <li>Removed obsolete typedefs and associated enums stm_se_play_stream_encoding_t, stm_se_play_stream_format_3d_t, stm_se_play_stream_scan_type_t, stm_se_play_stream_aspect_ratio_t
 * <li>Removed obsolete enums reason_code_e, aspect_ratio_e, stm_se_stereo_mode_e, stm_se_speaker_map_e
 * <li>Removed obsolete struct picture_size_s, callbacks stm_se_event_streaming_engine_callback_t, substream_callback_t
 * <li>Added deprecated compilation flag for stm_se_memsink_pull_buffer_t: needs to be replaced by ::stm_se_capture_buffer_t
 * <li>various documentation updates
 * <li>updated stability level of apis
 * <li>added discontinuity markers mute, fadein, fadeout
 * <li>added audio application type ::STM_SE_CTRL_VALUE_AUDIO_APPLICATION_TYPE_HDMIRX_GAME_MODE
 * <li>added control ::STM_SE_CTRL_CONTAINER_FRAMERATE to force container framerate
 * <li>removed control ::STM_SE_CTRL_PACKET_INJECTOR : policy packet injector set by default
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.17 (version for SDK2.13.1) </th></tr>
 *         <tr><td><ul>
 * <li>Added new encode control ::STM_SE_CTRL_ENCODE_NRT_MODE for enabling/disabling NRT encoding mode
 * <li>updated ::stm_se_uncompressed_frame_metadata_s by adding uint64_t encoded_time field
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.18 (version for SDK2.14) </th></tr>
 *         <tr><td><ul>
 * <li>Added ::STM_SE_CTRL_AUDIO_READER_GRAIN to set audio reader capture grain
 * <li>Removed ::STM_SE_ENCODE_STREAM_ENCODING_VIDEO_FAKE video fake coder
 * <li>Added new encode control ::STM_SE_CTRL_ENCODE_NRT_MODE for enabling/disabling NRT encoding mode
 * <li>updated ::stm_se_uncompressed_frame_metadata_s by adding uint64_t encoded_time field
 * <li>created ::__stm_se_encode_coordinator_metadata_t
 * <li>added ::stm_se_compression_port_t output field for ::stm_se_compression_s
 * <li>removed obsolete stm_se_demux apis, stm_se_memsink_pull_buffer_t, stm_se_playback_time_format_t, stm_se_buffer_h, STM_SE_STREAM_INCOMPLETE
 * <li>Added ::STM_SE_CTRL_HDMI_RX_MODE control to set the HDMI Rx mode on a playback. Can be set to DISABLED (default), REPEATER, or NON_REPEATER
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.19 (version for SDK2.15) </th></tr>
 *         <tr><td><ul>
 * <li>Added ::STM_SE_CTRL_DIALOG_ENHANCER to specify the type of dialog enhancer processing
 * <li>Increased the number of playstream attachable to a mixer
 * <li>Added ::STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2
 * <li>Added generic "amount" control for PcmProcessing Tuning ::STM_SE_CTRL_VOLUME_MANAGER_AMOUNT, ::STM_SE_CTRL_VIRTUALIZER_AMOUNT, ::STM_SE_CTRL_UPMIXER_AMOUNT, ::STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT
 * <li>Introduced stm_se_play_stream_attach_to_pad new API to be able to specify mixer input port during attachement; playstream to audio Mixer attachement through stm_se_play_stream_attach is now deprecated
 * <li>Clarified api usage of video encode controls
 * <li>Added ::STM_SE_CTRL_PLAY_STREAM_MUTE to be performed on decoder side; obsoleting stm_se_play_stream_set_enable and stm_se_play_stream_get_enable for audio
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.20 (version for SDK2.15.1) </th></tr>
 *         <tr><td><ul>
 * <li>Added ::STM_SE_PLAY_STREAM_MSG_REASON_CODE_INSUFFICIENT_MEMORY error code
 * <li>Added ::STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_HD_PROFILE, ::STM_SE_CTRL_VALUE_VIDEO_HDMIRX_REP_UHD_PROFILE, ::STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_HD_PROFILE, ::STM_SE_CTRL_VALUE_VIDEO_HDMIRX_NON_REP_UHD_PROFILE
 * <li>Added Encoder Statistics ::__stm_se_encode_stream_reset_statistics(), ::__stm_se_encode_stream_get_statistics()
 *         </ul></td></tr>
 * </table>
 * <table> <tr><th>v1.21 (version for SDK2.15.2) </th></tr>
 *         <tr><td><ul>
 * <li>Added ::STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES policy control to allow the caller to request additional effort be applied to decode frames that are known to be partly damaged
 * <li>Added mixer dual stage topology
 *         </ul></td></tr>
 * </table>
 */

/*!
 */
#define UNDEFINED 0
