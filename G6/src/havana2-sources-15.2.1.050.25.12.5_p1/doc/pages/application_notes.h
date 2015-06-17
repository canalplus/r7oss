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

Source file name : stm_se/pages/application_notes.h

************************************************************************/

#ifndef STM_SE_PAGES_APPLICATION_NOTES_H__
#define STM_SE_PAGES_APPLICATION_NOTES_H__

/*!
 * \page application_notes Application notes
 *
 * \section prepare_for_audiovideo_decode Prepare for audio/video decode
 *
 * \code
 * stm_se_playback_new(..., &playback);
 * stm_se_play_stream_new(playback, &vid_stream);
 * stm_se_play_stream_attach(vid_stream, display_source);
 * stm_se_play_stream_enable(vid_stream, true);
 * // Establish a very simple mixer topology with only one
 * // simple stereo output.
 * //
 * stm_se_audio_player_new("hw:0,0", &aud_player);
 * stm_se_audio_mixer_new(&aud_mixer);
 * stm_se_audio_mixer_attach(aud_mixer, aud_player);
 * stm_se_play_stream_new(playback, &aud_stream);
 * stm_se_play_stream_attach(aud_stream, aud_mixer);
 * stm_se_play_stream_enable(aud_stream, true);
 * // ready for data
 * \endcode
 *
 * \section optimize_video_decode_memory Optimize video decode bpa2 memory footprint using profiles
 *
 * To optimize memory footprint, it is possible to set memory profile based on
 * stream resolution. By default it is set to 1080p HD buffer size.
 * Changing it should be done *before* adding stream to playback:
 *
 * \code
 * stm_se_playback_new(..., &playback);
 * // Now set a 720p memory profile (by default: HD 1080p buffer allocation):
 *  stm_se_playback_set_control(
 *          playback,
 *          STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE,
 *          STM_SE_CTRL_VALUE_VIDEO_DECODE_720P_PROFILE);
 *
 * stm_se_play_stream_new(playback, &vid_stream);
 * stm_se_play_stream_attach(vid_stream, display_source);
 * stm_se_play_stream_enable(vid_stream, true);
 * \endcode
 *
 * \section handling_play_stream_events Handling play stream events
 *
 * \code
 * //
 * // The application/middleware will known the performance parameters of
 * // its event handling logic. This should allow it to request a queue
 * // of the correct depth.
 * //
 * #define MESSAGE_DEPTH 16
 * //
 * // This is example code so we use global variables to keep things simple.
 * // Ordinarily these values would be stored in a context structure or class.
 * //
 * // We assume the play_stream is initialized elsewhere but the
 * // subscription variables are initialized as part of init_subscription()
 * //
 * stm_se_play_stream_h play_stream;
 * stm_event_subscription_h event_subscription;
 * stm_se_play_stream_subscription_h message_subscription;
 * //
 * // Subscribe to some of the play_stream's event notifications and messages.
 * //
 * int init_subscription()
 * {
 *   uint32_t event_mask =
 *     STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED |
 *     STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED |
 *     STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
 *   stm_event_subscription_entry_t event_entry = {
 *     .object = (stm_object_h) play_stream,
 *     .event_mask = event_mask,
 *   };
 *   uint32_t message_mask =
 *     STM_SE_PLAY_STREAM_MSG_SIZE_CHANGED |
 *     STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED;
 *   int res;
 *   //
 *   // subscribe to events (for notification)
 *   //
 *   res = stm_event_subscription_create(&event_entry,
 * 				      1,
 * 				      &event_subscription);
 *   if (0 != res) {
 *     return res;
 *   }
 *   //
 *   // subscribe to messages from the play stream
 *   //
 *   res = stm_se_play_stream_subscribe(play_stream,
 * 				     message_mask,
 * 				     MESSAGE_DEPTH,
 * 				     &message_subscription);
 *   if (0 != res) {
 *     (void) stm_event_subscription_delete(&event_subscription);
 *     return res;
 *   }
 *   return 0;
 * }
 * //
 * // Logic for the event handling thread.
 * //
 * // This could be expanded if event_subscription sought data from
 * // more than one object type (or instance).
 * //
 * void handle_events()
 * {
 *   stm_event_info_t event;
 *   stm_se_play_stream_msg_t message;
 *   int res;
 *   while (1) {
 *     //
 *     // wait for an event to be delivered.
 *     //
 *     res = stm_event_wait(event_subscription,
 * 			 -1, // wait forever
 * 			 1,
 * 			 NULL, // use return code instead
 * 			 &event);
 *     if (0 != res) {
 *       log_error("Cannot extract event notification\n");
 *       continue;
 *     }
 *     switch(event.event.event_id) {
 *     case STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED:
 *       handle_frame_decoded(event.events_missed);
 *       break;
 *     case STM_SE_PLAY_STREAM_EVENT_FRAME_DISPLAYED:
 *       handle_frame_displayed(event.events_missed);
 *       break;
 *     case STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY:
 *       //
 *       // Extract messages until no more are available.
 *       //
 *       // The loop is required to handle cases where events_missed is
 *       // true but its simpler just to ignore events_missed and always
 *       // loop.
 *       //
 *       while (1) {
 * 	res = stm_se_play_stream_get_message(play_stream,
 * 					     message_subscription,
 * 					     &message);
 * 	if (0 != res)
 * 	  break;
 * 	handle_message(&message);
 *       }
 *       if (-EAGAIN != res) {
 * 	log_error("Cannot extract message\n");
 *       }
 *       break;
 * \endcode
 *
 *     default:
 *
 * \code
 *       log_error("Unexpected event notification\n");
 *       break;
 *     }
 *   } // event handling loop
 * }
 * //
 * // Logic to handle a single message.
 * //
 * // This is called from the event handling code but extracted
 * // to maintain readability.
 * //
 * void handle_message(stm_se_play_stream_msg_t *message)
 * {
 *   switch (message.message_id) {
 *   case STM_SE_PLAY_STREAM_MSG_SIZE_CHANGED:
 *     handle_size_changed(message.u.size);
 *     break;
 *   case STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED:
 *     handle_frame_rate_changed(message.u.video_parameters);
 *     break;
 *   case STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST:
 *     //
 *     // If the message queue overflows the final event will be discarded
 *     // and replaced with a message lost message. Additional overflows whilst
 *     // the message lost message is the final event in the queue will result
 *     // in the messages_lost counter being incremented.
 *     //
 *     // This is primarily a diagnostic aid. Effectively it informs the
 *     // application that its assumptions about its own performance (i.e. how
 *     // often it can service messages) is not correct.
 *     //
 *     log_error("Lost %d messages. Increase message depth?\n",
 * 	      message.u.messages_lost);
 *     break;
 * \endcode
 *
 *   default:
 *
 * \code
 *     log_error("Unexpected message\n");
 *     break;
 *   }
 * }
 * \endcode
 *
 * \section inject_still_pictures_single_i_frame Inject still pictures (single I-frame)
 *
 * TBD
 *
 * \section grab_decoded_audio_or_video_frames_synchronously_or_asynchronously Grab decoded audio or video frames synchronously or asynchronously
 *
 * \subsection memory_sourcesink_grab Memory source/sink grab
 *
 * \code
 * 	  //
 * 	  // This feature relies on memory source API. To grab a decoded frame,
 * // a buffer must be provided in which the frame will be copied.
 * //
 * stm_se_playback_new(..., &playback);
 * stm_se_play_stream_new(playback, &vid_stream);
 * stm_memsink_new(...,&memsink_object);
 * stm_se_play_stream_attach(vid_stream, memsink_object);
 * p_memsink_pull_buffer->virtual_address  = &providedBufer;
 * p_memsink_pull_buffer->buffer_length    = providedBuferSize;
 * retval = stm_memsink_pull_data(memsink_object,
 * 					p_memsink_pull_buffer,
 * 					providedBuferSize,
 * 					&memsinkDescriptor->m_availableLen);
 * [TBC]
 * \endcode
 *
 * \subsection zero_copy_grab Zero copy grab
 *
 * \code
 * //
 * // This feature doesn't rely on memory hence the user need to provide
 * // STM_DATA_INTERFACE_PUSH_RELEASE interface.
 * //
 * stm_data_interface_push_release_sink_t push_release_interface = {
 *         .connect    = stm_zc_attach,
 *         .disconnect = stm_zc_detach,
 *         .push_data  = stm_zc_push_data
 * };
 * // function called when connecting play stream to the interface
 * int stm_zc_attach (stm_object_h src_object,
 *                    stm_object_h sink_object,
 *                    struct stm_data_interface_release_src *release_src)
 * {
 * // save buffer release callback
 * ((struct dvb_zc_cont_s*)sink_object)->my_released_data =
 *                                          release_src->release_data;
 * // save source object
 * ((struct dvb_zc_cont_s*)sink_object)->src_object = src_object;
 * 	return 0;
 * }
 * // function called when disconnecting play stream from the interface
 * int stm_zc_detach (stm_object_h src_object,
 *                    stm_object_h sink_object)
 * {
 * 	 // perform internal actions here
 *    // release all remaing pushed buffers
 * 	 return 0;
 * }
 * // function called when a new buffer is decoded
 * int stm_zc_push_data (stm_object_h sink_object,
 *                       stm_object_h pushed_object)
 * {
 *     // pushed_object should be cast to capture_buffer_s*
 *     // to get decode buffer reference
 *     // perform internal actions here
 * \endcode
 *
 *     ...
 *
 * \code
 *     //
 *     // Pushed buffer must be released asap to guarantee a smooth, decode
 *     // without artifacts.
 *     // User must guarantee to release (return to streaming engine) all
 *     // buffers before to call the detach
 *     // ((struct dvb_zc_cont_s*)sink_object)->my_released_data((
 *     //    (struct dvb_zc_cont_s*)sink_object)->src_object, pushed_object);
 *     //
 * 	return 0;
 * }
 * //
 * // To provide user decode buffers to the play stream
 * // Folowing actions must be done
 * //
 * stm_se_playback_new(..., &playback);
 * //
 * // No decode buffer allocation for the next video play stream new
 * // Note that the application shall ensure no concurrent play_stream
 * // creation on the same playback.
 * //
 * //
 * stm_se_playback_set_control(
 *          playback,
 *          STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED,
 *          STM_SE_CTRL_VALUE_APPLY);
 *
 * stm_se_play_stream_new(playback, &vid_stream);
 * //
 * // Following structure should be available
 * //
 * 	stm_se_play_stream_decoded_frame_buffer_info_t	     shared_frame_buffer;
 * 	stm_se_play_stream_decoded_frame_buffer_reference_t  buffer_ref_array[16];
 * //
 * // Ask for mandatory setting of decode resources
 * //
 * if ( stm_se_play_stream_get_compound_control(
 *         vid_stream,
 *         STM_SE_CTRL_NEGOTIATE_DECODE_BUFFERS,
 *         (void *)& shared_frame_buffer) != 0)
 * {
 *   ZC_ERROR(" Can't get  my compount control .... ");
 *   // release created object
 *   return;
 * }
 * //
 * // initialize BufferRefArray with user buffer addresses
 * //
 * for (i=0; i < shared_frame_buffer.number_of_buffers; i++)
 * {
 *    buffer_ref_array[i].virtual_address = ...
 *    buffer_ref_array[i].physical_address  = ...
 *    buffer_ref_array[i].buffer_size  = ...
 * }
 * sharedFrameBuffer.buf_list = buffer_ref_array;
 * //
 * // transmit them to SE
 * //
 * if ( stm_se_play_stream_set_compound_control(
 *         vid_stream,
 *         STM_SE_CTRL_SHARE_DECODE_BUFFERS,
 *         (void *)&shared_frame_buffer) != 0)
 * {
 *   ZC_ERROR(" Can't set my compount control .... ");
 *   // release created objects
 *   return;
 * }
 * //
 * // now register push release interface
 * // This part can be done any time after stm_se_play_stream_new
 * //
 * error = stm_registry_add_object(STM_REGISTRY_TYPES,		// parent
 * 	       							 "dvbsink",
 * 	       							 (stm_object_h)&zc_type);
 * if(error){
 * 	  ZC_ERROR("stm_registry_add_object failed(%d)\n",error);
 *   // release created objects
 *   return;
 * }
 * // add the sink interface to the MEMORY-SINK object type
 * error= stm_registry_add_attribute((stm_object_h)&zc_type, // parent
 * 	            		STM_DATA_INTERFACE_PUSH_RELEASE,      // tag
 *          				STM_REGISTRY_ADDRESS,	            // data type tag
 *       				&push_release_interface,	      // value
 * 	            		sizeof(push_release_interface)); // value size
 * if(error){
 *    ZC_ERROR("stm_registry_add_attribute failed(%d)\n",error);
 * 	 error = stm_registry_remove_object((stm_object_h)&dvb_type);
 *    // release created objects
 *    return;
 * }
 * // now create push release interface object
 * error = stm_registry_add_instance(STM_REGISTRY_INSTANCES,	  // parent
 * 	    							  &dvb_type,		// type
 * 	    							  "XXX_ZERO_COPY",
 * 	    								(stm_object_h)&ZC_obj_p);
 * If (error){
 *    ZC_ERROR("stm_registry_add_instance failed(%d)\n", error);
 *    // release created objects
 *    return;
 * }
 * // now notify that we want primary buffers
 * stm_se_play_stream_set_control(vid_stream,
 * 	                 STM_SE_CTRL_DECIMATE_DECODER_OUTPUT,
 *                    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED);
 * //
 * // now attach stream to push release interface object
 * //
 * error = stm_se_play_stream_attach (vid_stream,
 * 	   									(stm_object_h )&ZC_obj_p,
 * 	   									(stm_se_play_stream_output_port_t) 0);
 * If (error){
 *    ZC_ERROR("stm_se_play_stream_attach failed(%d)\n",error);
 *    // release created objects
 *    return;
 * }
 * \endcode
 *
 * \section markers_detection_and_injection Markers detection and injection.
 *
 * Data injected into the streaming engine may include specially crafted data packets called markers. When
 * the streaming engine detects a marker it will trigger an action similar to an API function call.
 *
 * For example, when a single injection consists of fragment of PES, followed by a discontinuity marker and
 * a second fragment of PES then the streaming engine will act as though:
 *
 * 1. The frirst fragment of PES was injected using ::stm_se_play_stream_inject_data (or its memory sink
 * interface equivalent),
 *
 * 2. The discontinuity was injected using ::stm_se_play_stream_inject_discontinuity,
 *
 * 3. The second fragment of PES was injected using  ::stm_se_play_stream_inject_data.
 *
 * This operating model brings, as a benefit, a way to perform actions that must be made synchronously
 * regarding the incoming compressed data, or, in other words to have these actions performed on data that
 * precede or follow the insertion of such a packet.
 *
 * The injection model remains unchanged and a  PES marker frame can be inserted at any position in the
 * bitstream. Therefore a marker pes packet should be injected thanks to the ::stm_se_play_stream_inject_data,
 * among the rest of the compressed bitstream it is inserted in
 *
 * <table>
 *     <tr>
 *         <td>
 *             field id
 *         </td>
 *         <td>
 *             Syntax
 *         </td>
 *         <td>
 *             Value
 *         </td>
 *         <td>
 *             Size in bits
 *         </td>
 *         <td>
 *             Comments
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *             marker_pes_packet {
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *                     packet_start_code_prefix
 *         </td>
 *         <td>
 *             000001h
 *         </td>
 *         <td>
 *             24
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             2
 *         </td>
 *         <td>
 *                     stream_id
 *         </td>
 *         <td>
 *             FBh
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             3
 *         </td>
 *         <td>
 *                     PES_packet_length
 *         </td>
 *         <td>
 *             0014h
 *         </td>
 *         <td>
 *             16
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             4
 *         </td>
 *         <td>
 *                      reserved
 *         </td>
 *         <td>
 *             10b
 *         </td>
 *         <td>
 *             2
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             5
 *         </td>
 *         <td>
 *                     PES_scrambling_control
 *         </td>
 *         <td>
 *             00b
 *         </td>
 *         <td>
 *             2
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             6
 *         </td>
 *         <td>
 *                     PES_priority
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             7
 *         </td>
 *         <td>
 *                     data_alignment_indicator
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *                     copyright
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             9
 *         </td>
 *         <td>
 *                     original_or_copy
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             10
 *         </td>
 *         <td>
 *                     PTS_DTS_flags
 *         </td>
 *         <td>
 *             00b
 *         </td>
 *         <td>
 *             2
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             11
 *         </td>
 *         <td>
 *                     ESCR_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             12
 *         </td>
 *         <td>
 *                     ES_rate_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             13
 *         </td>
 *         <td>
 *                     DSM_trick_mode_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             14
 *         </td>
 *         <td>
 *                     additionnal_copy_into_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             15
 *         </td>
 *         <td>
 *                     PES_CRC_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             16
 *         </td>
 *         <td>
 *                     PES_extension_flag
 *         </td>
 *         <td>
 *             1b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             17
 *         </td>
 *         <td>
 *                     PES_header_data_length
 *         </td>
 *         <td>
 *             11h
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *                     if (PES_extension_flag == 1) {
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             18
 *         </td>
 *         <td>
 *                             PES_private_data_flag
 *         </td>
 *         <td>
 *             1b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             19
 *         </td>
 *         <td>
 *                             pack_header_field_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             20
 *         </td>
 *         <td>
 *                             program_packet_sequence_counter_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             21
 *         </td>
 *         <td>
 *                             P-STD_buffer_flag
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             22
 *         </td>
 *         <td>
 *                             reserved
 *         </td>
 *         <td>
 *             000b
 *         </td>
 *         <td>
 *             3
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             23
 *         </td>
 *         <td>
 *                             PES_extension_flag_2
 *         </td>
 *         <td>
 *             0b
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *                             if (PES_private_flag == 1) {
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             24
 *         </td>
 *         <td>
 *                                     PES_private_data[13]
 *         </td>
 *         <td>
 *             reserved : 'STMM'
 *         </td>
 *         <td>
 *             32
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             25
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *             action_id
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             26
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *             user_data
 *         </td>
 *         <td>
 *             80
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             27
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *             user_data_size
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *                            }
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *                     }
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *         </td>
 *         <td>
 *             }
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 * </table>
 *
 * Here, the action_id (field 25) is a field that is aimed to specify the action to be performed
 *
 * user_data and user_data_size are the characteristics of the payload that is parsed by the streaming
 * engine.
 *
 * The following sections describe the correspondance between the action identifier, their meaning, and
 * their corresponding API calls.
 *
 * \subsection pts_alarm PTS alarm
 *
 * Action_id value : 0
 *
 * Action summary : PTS alarm
 *
 * Details : Requesting the sending of the ::STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS, message with user_data
 * (field 26) attached to it.
 *
 * \code
 * Representative API call :  stm_se_play_stream_set_alarm(play_stream,
 * STM_SE_PLAY_STREAM_ALARM_NEXT_PARSED_PTS, &user_data, user_data_size);
 * Note that the user data size can not exceed 10 bytes, and that in case user_data_size is lower than 10,
 * \endcode
 * the latest bytes are stuffing bytes.
 *
 * \subsection break_forward_or_break_backward Break forward or break backward
 *
 * Action_id value : 1 or 2
 *
 * Action summary : Break forward or break backward
 *
 * Details : Discontinuity in the compressed data flow injection.
 *
 * Representative API call: ::stm_se_play_stream_inject_discontinuity (play_stream, true, false, false);
 *
 * \subsection break_backward_smooth_reverse Break backward smooth reverse
 *
 * Action_id value : 3
 *
 * Action summary : Break backward for smooth reverse.
 *
 * Details : Discontinuity in the compressed data flow injection, but the latest injected chunk of data is
 * contiguous with the previously injected one. Relevant only if the playback speed is negative
 *
 * Representative API call: ::stm_se_play_stream_inject_discontinuity (play_stream, true, true, false);
 *
 * \subsection break_end_of_stream Break end of stream
 *
 * Action_id value : 4
 *
 * Action summary : End of stream
 *
 * Details : This marker indicates the playstream that the compressed bitstream has been entirely injected.
 * It forces the compressed bitstream to be entirely decoded and played out.
 *
 * Some of the connected sinks (depending on their type) will be receiving a zero sized frame, accompanied
 * with a flag indicating that the compressed data injected before this marker have been enterily decoded
 * played out.
 *
 * Representative API call : ::stm_se_play_stream_inject_discontinuity (play_stream, true, true, true);
 *
 * \section issuing_sound_effects_using_the_audio_generator Issuing sound effects using the audio generator
 *
 * TBD
 *
 * \section configure_output_topology Configure output topology
 *
 * \code
 * stm_se_audio_mixer_new(&primary_mixer);
 * //
 * // Multi-channel HDMI output
 * //
 * stm_se_audio_player_new("HDMI-TX","hw:0,6", &hdmi_player);
 * hdmi_desc = {
 * 	      .num_channels = 8,
 * 	      .spdif_mode = true,
 * };
 * stm_se_audio_player_set_compound_control(
 * 	hdmi_player,
 * 	STM_SE_CTRL_AUDIO_PLAYER_HW_MODE,
 * 	&hdmi_desc);
 * // Set other compound controls to configure per-output
 * // post processing (e.g. spacialization, cabinet frequency
 * // response compentation filters, bass redirection).
 * // All digital post-processing (except downmix)
 * // is off by default.
 * //
 * stm_se_audio_mixer_attach(primary_mixer, hdmi_player);
 * //
 * // Stereo SPDIF output
 * //
 * stm_se_audio_player_new("SPDIF-TX", "hw:0,5", &spdif_player);
 * spdif_desc = {
 * 	      .num_channels = 2,
 * 	      .spdif_mode = true,
 * };
 * stm_se_audio_player_set_compound_control(
 * 	spdif_player,
 * 	STM_SE_AUDIO_PLAYER_CTRL_HW_MODE,
 * 	&spdif_desc);
 * stm_se_audio_mixer_attach(primary_mixer, spdif_player);
 * //
 * // Stereo analog output (e.g. internal DAC)
 * //
 * stm_se_audio_player_new("Headphone-out", "hw:0,2", &analog_player);
 * // default value for STM_SE_AUDIO_PLAYER_CTRL_HW_MODE is
 * // analog stereo so no control setting is needed to make
 * // noise.
 * //
 * stm_se_audio_mixer_attach(primary_mixer, analog_player);
 * //
 * // Finally attach the mixer to a play stream.
 * //
 * stm_se_play_stream_attach(aud_stream, primary_mixer);
 * \endcode
 *
 * \section user_data User data
 *
 * The user data are a collection of bytes encoded in several audio and video standards, for various usage,
 * that the streaming has no knowledge on. One of the most widespread usage of the user data is the
 * conveying of close captions data.
 *
 * The recovery of the user data should be enabled thanks to a memory sink connection to a play stream:
 *
 *      ::stm_se_play_stream_attach(play_stream, memory_sink,  ::STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY);
 *
 * This function provides a play stream with a memory sink. This memory sink embeds a buffer that one or
 * several contiguous user data blocks are inserted into.
 *
 * Below is the user data formatted block definition:
 *
 * <table>
 *     <tr>
 *         <td>
 *             User data
 *         </td>
 *         <td>
 *             Number of bits
 *         </td>
 *         <td>
 *             Description
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             reserved
 *         </td>
 *         <td>
 *             32
 *         </td>
 *         <td>
 *             'STUD'
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             block_length
 *         </td>
 *         <td>
 *             16
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             header_length
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             reserved
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             num_padding_bytes
 *         </td>
 *         <td>
 *             5
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             stream_abridgement
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             overflow
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             codec_id
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             reserved
 *         </td>
 *         <td>
 *             6
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             is_there_a_pts
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             is_pts_interpolated
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             reserved
 *         </td>
 *         <td>
 *             15
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             pts_msb
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             pts
 *         </td>
 *         <td>
 *             32
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             if (codec_id == H264) {
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     reserved
 *         </td>
 *         <td>
 *             7
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     is_registered
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     itu_t_t35_country_code
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *             relevant only if is_registered == 1
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     itu_t_t35_country_code_extension_byte
 *         </td>
 *         <td>
 *             8
 *         </td>
 *         <td>
 *             relevant only if is_registered == 1
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     itu_t_t35_provider_code
 *         </td>
 *         <td>
 *             16
 *         </td>
 *         <td>
 *             relevant only if is_registered == 1
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     uuid_iso_iec_11578}
 *         </td>
 *         <td>
 *             128
 *         </td>
 *         <td>
 *             relevant only if is_registered == 0
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             if (codec_id == MPEG2) {
 *         </td>
 *         <td>
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     reserved
 *         </td>
 *         <td>
 *             31
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *                     top_field_first }
 *         </td>
 *         <td>
 *             1
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             padding
 *         </td>
 *         <td>
 *             num_padding_bytes * 8
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             payload
 *         </td>
 *         <td>
 *             8 * (block_length - header_length- num_padding_bytes)
 *         </td>
 *         <td>
 *         </td>
 *     </tr>
 * </table>
 *
 * (*) : Provided the fact that the generic and codec specific header sizes are multiple of 32 bits, padding
 * bytes are expected to be missing for most of the CPU units. Nevertheless, that should not be considered
 * as granted.
 *
 * \subsection user_data_parsing_logic User data parsing logic
 *
 * Despite the bitstream logic that the user data providing is implemented with, the mapping of the
 * following structure on recovered buffers is used.
 *
 * \code
 * typedef struct {
 *    char id[4];                       // 'STUD'
 *    unsigned block_length:16;         // value of the whole block lenght
 *    unsigned header_length:16;        // value of the whole header lenght
 *    unsigned reserved_1:1;
 *    unsigned num_padding_bytes:5;
 *    unsigned stream_abridgement:1;    // 1 if this packet may is collected from a spoiled or uncontiguous
 *    bitstream, 0 otherwise
 *    unsigned overflow:1;              // 1 if this packet id the first one after a discontinuity resulting
 *    from a too low providing of buffers through the memory sink
 *    unsigned codec_id:8;              // codec_id 0 if H264, 1 if MPEG2
 *    unsigned reserved_2:6;
 *    unsigned is_there_a_pts:1;        // if 1, pts_msb and pts reflect a PTS belonging to the picture the
 *    user are collected from
 *    unsigned is_pts_interpolated:1;   // if 1, mssing pts from the picture the user are collected from, is
 *    internally calculated
 *    unsigned reserved_3:7;
 *    unsigned pts_msb:1;               // pts 33 bits most significant bit
 *    unsigned pts:32;                  // pts 32 bits less significant bits
 * } stm_se_play_stream_user_data_generic_t;
 * typedef struct {
 *    unsigned reserved:31;
 *    unsigned is_registered:1;
 *    unsigned itu_t_t35_country_code:8;
 *    unsigned itu_t_t35_country_code_extension_byte:8;
 *    unsigned itu_t_t35_provider_code:16;
 *    char uuid_iso_iec_11578[16];
 * } stm_se_play_stream_user_data_h264_t;
 * typedef struct {
 *    unsigned reserved:31;
 *    unsigned top_field_first:1;
 * } stm_se_play_stream_user_data_mpeg2_t;
 * \endcode
 *
 * The parsing logic of the recovered buffer is as follows :
 *
 * \code
 * // buffer collected from user data memory sink collector
 * unsigned char user_data_buffer[];
 * unsigned int user_data_size;
 * // pointers on generic and codec specific headers
 * stm_se_play_stream_user_data_generic_t * udgdump;
 * stm_se_play_stream_user_data_h264_t * udh264dump;
 * stm_se_play_stream_user_data_mpeg2_t * udmpeg2dump;
 * unsigned char * payload;
 * // indexes
 * unsigned char * p;
 * unsigned int i = 0;
 * for (p = user_data_buffer; p < user_data_buffer + user_data_size; p+=
 * ((stm_se_play_stream_user_data_generic_t * )p)->block_length)
 * {
 *    udgdump = (stm_se_play_stream_user_data_generic_t * )p;
 *    payload = p + udgdump->header_length + udgdump->num_padding_bytes;
 *    ..;
 *    .. = udgdump->stream_abridgement;
 *    .. = udgdump->pts;
 *    ..;
 *    // WARNING:
 *    //   Don't access any specific codec header until you have verified the
 *    //   codec id. It is pre-initialized to separate the type manipulation
 *    //   from the decode logic but is only valid if the codec id is correct.
 *    if (udgdump->codec_id == H264)
 *    {
 *       udh264dump = (stm_se_play_stream_user_data_h264_t *)
 *       (p+sizeof(stm_se_play_stream_user_data_generic_t));
 *       ..;
 *       .. = udh264dump->itu_t_t35_provider_code;
 *       ..;
 *    }
 *    else if (udgdump->codec_id == MPEG2)
 *    {
 *       udmpeg2dump = (stm_se_play_stream_user_data_mpeg2_t *)
 *       (p+sizeof(stm_se_play_stream_user_data_generic_t));
 *       .. = udmpeg2dump->top_field_first;
 *    }
 *    for (i=0; i<(udgdump->block_length -
 *           udgdump->header_length - udgdump->num_padding_bytes); i++)
 *    {
 *       // getting raw user data, byte after byte;
 *       .. = *(payload + i);
 *    }
 * }
 * \endcode
 *
 * \section common_channel_assignments Common channel assignments
 *
 * This section gives examples of common channel assignments and how they can be expressed using the
 * ::stm_se_audio_channel_assignment_t type.
 *
 * \subsection mono Mono
 *
 * \code
 * stm_se_audio_channel_assignment_t channel_map_1_0 =
 * {
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_0,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
 * };
 * \endcode
 *
 * \subsection stereo Stereo
 *
 * \code
 * stm_se_audio_channel_assignment_t channel_map_2_0 =
 * {
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
 * };
 * \endcode
 *
 * \subsection pro_logic_ii Pro-logic II
 *
 * \code
 * stm_se_audio_channel_assignment_t channel_map_3_1_LFE =
 * {
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_0,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
 * };
 * \endcode
 *
 * \subsection soundbar_and_lfe_31 Soundbar and LFE (3.1)
 *
 * \code
 * stm_se_audio_channel_assignment_t hannel_map_3_0_LFE =
 * {
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
 * };
 * \endcode
 *
 * \subsection basic_surround_sound_51 Basic surround sound (5.1)
 *
 * \code
 * stm_se_audio_channel_assignment_t channel_map_3_2_LFE =
 * {
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSUR_RSUR,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
 * 	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
 * };
 * \endcode
 *
 * \section audio_description Audio description
 *
 * \subsection dvb_audio_description DVB audio description
 *
 * TBD
 *
 * \section prepare_for_audiovideo_encode Prepare for audio/video encode
 *
 * \code
 * stm_se_encode_new(..., &encode);
 * stm_se_encode_stream_new(encode, &encode_stream);
 * stm_se_encode_stream_attach(encode_stream, memory_sink);
 * // Provide an audio or video frame to be encoded
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address,
 * frame_length, &frame_descriptor);
 * \endcode
 *
 * \section optimize_video_encode_memory Optimize video encode bpa2 memory using profiles & color format
 *
 * This section gives guidelines to optimize video encode stream bpa2 memory consumption.
 *
 * It consists in providing hints on memory profile and injected color format before encode_stream creation
 *
 * \code
 * stm_se_encode_new(..., &encode);
 * //Provide profile information related to the next encode stream to be created:
 * //here internal buffers will be dimensioned for 720p frames
 * stm_se_encode_set_control(encode, STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE, STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE);
 * //Provide color format information related to the next encode stream to be created:
 * //Depending on color format of the injected frames, internal buffer intended for color format conversion can be removed
 * stm_se_encode_set_control(encode, STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT, SURFACE_FORMAT_VIDEO_422_YUYV);
 * //Profile & color format properties will then be taken into account by the next video encode_stream to be created.
 * stm_se_encode_stream_new(encode, &encode_stream);
 * \endcode
 *
 * \section configure_video_encoder Configure video encoder
 *
 * This section gives guidelines to configure video encode stream,
 * highlighting the main parameters that can be customized to fit a dedicated use case.
 *
 * \code
 * //create an encode object
 * stm_se_encode_new(..., &encode);
 * //create a video encode_stream object
 * stm_se_encode_stream_new(encode, &encode_stream);
 * //Customize encode parameters: only static configuration allowed now for a specific encode_stream
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_ENCODE_STREAM_BITRATE, 6000000);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, H264_CPB_SIZE);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE, 30);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL, H264_LEVEL);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE, H264_PROFILE);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, H264_RESOLUTION);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, H264_FRAMERATE);
 * stm_se_encode_stream_set_control(encode_stream, STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, ASPECT_RATIO);
 * \endcode
 *
 * \section inject_discontinuity_in_encoder Inject discontinuity in encoder
 *
 * This section gives guidelines to inject discontinuity in a video encode stream
 *
 * \subsection time_discontinuity Time discontinuity
 *
 * STKPI client can inform encoder about a time discontinuity, meaning a new sequence should be encoded.
 * This is achieved by injecting a STM_SE_DISCONTINUITY_DISCONTINUOUS discontinuity.
 *
 * \code
 * //create an encode object
 * stm_se_encode_new(..., &encode);
 * //create a video encode_stream object
 * stm_se_encode_stream_new(encode, &encode_stream);
 * //repeat frame injection
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address, frame_length, &frame_descriptor);
 * //inform encoder that next frame to be injected is impacted by a time discontinuity
 * //a new GOP will be created from the next frame and discontinuity information will be propagated downstream through metadata
 * stm_se_encode_stream_inject_discontinuity(encode_stream, STM_SE_DISCONTINUITY_DISCONTINUOUS);
 * //inject frame involving a time discontinuity
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address, frame_length, &frame_descriptor);
 * \endcode
 *
 * \subsection end_of_stream End of stream (EOS)
 *
 * STKPI client can inform encoder that the latest injected frame is the last frame of the sequence.
 * This is achieved by injecting a STM_SE_DISCONTINUITY_EOS discontinuity.
 * Information will be propagated downstream through metadata and STKPI client can then rely on this information
 * to know encode stream completion.
 *
 * \code
 * //create an encode object
 * stm_se_encode_new(..., &encode);
 * //create a video encode_stream object
 * stm_se_encode_stream_new(encode, &encode_stream);
 * //repeat frame injection until the end of the stream
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address, frame_length, &frame_descriptor);
 * //inform encoder that the last frame of the stream has been injected
 * //EOS information will be propagated downstream through discontinuity metadata
 * stm_se_encode_stream_inject_discontinuity(encode_stream, STM_SE_DISCONTINUITY_EOS);
 * //wait for EOS buffer encoding before closing the encode stream
 * \endcode
 *
 * \subsection force_new_gop Force new GOP
 *
 * STKPI client can ask encoder to start a new GOP (SPS/PPS/IDR) from next injected frame,
 * This is achieved by injecting a STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST discontinuity.
 * Such discontinuity injection is useful in particular in use case like adaptive streaming
 * where encode chunks have fixed frame number.
 *
 * \code
 * //create an encode object
 * stm_se_encode_new(..., &encode);
 * //create a video encode_stream object
 * stm_se_encode_stream_new(encode, &encode_stream);
 * //repeat frame injection
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address,
 * frame_length, &frame_descriptor);
 * //inform encoder that next frame to be injected must be encoded in a new GOP
 * //Information will be propagated downstream through discontinuity metadata
 * stm_se_encode_stream_inject_discontinuity(encode_stream, STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST);
 * //inject frame involving a new GOP to be created
 * stm_se_encode_stream_inject_frame(encode_stream, frame_virtual_address, frame_physical_address, frame_length, &frame_descriptor);
 * \endcode
 *
 * \section tunneled_transcode_use_case Tunneled transcode use case
 *
 * \subsection tunneled_video_transcode_setup Tunneled video transcode setup
 *
 * This subsection gives guidelines to setup a tunneled video transcode.
 *
 * A tunneled video transcode consists in attaching a video encode_stream to a video play_stream,
 * connection being handled as STKPI attachment.
 * By default the transcode is made in real-time mode (RT), the playback is slave of rendering speed,
 * and the decoded video stream can be displayed while transcoding.
 *
 * \code
 * //create playback and encode objects
 * stm_se_playback_new("playback0", &playback);
 * stm_se_encode_new("encode0", &encode);
 * //create video play_stream and encode_stream objects
 * stm_se_play_stream_new("MPEG2 Decode", playback, STM_SE_STREAM_ENCODING_VIDEO_MPEG2, &play_stream);
 * stm_se_encode_stream_new("H264 Encode", encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &encode_stream);
 * //create a memory sink to consume the encoded frames
 * stm_memsink_new("Video MemSink", STM_IOMODE_BLOCKING_IO, KERNEL, &video_sink);
 * //attach the memsink to the encode_stream
 * stm_se_encode_stream_attach(encode_stream, video_sink);
 * //attach the encode_stream to the play_stream
 * stm_se_play_stream_attach(play_stream, encode_stream, STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
 * \endcode
 *
 * \subsection tunneled_transcode_nrt_setup Tunneled transcode setup in NRT mode
 *
 * This subsection gives guidelines to setup a tunneled audio/video transcode in non real-time mode (NRT).
 *
 * NRT is a particular mode of tunneled transcode that consists in transcoding as fast as possible an audio/video muxed
 * file to another one in a synchronized manner.
 * This is achieved by setting the STM_SE_CTRL_ENCODE_NRT_MODE encode control to STM_SE_CTRL_VALUE_APPLY.
 *
 * To obtain a correct behaviour in NRT mode, some other conditions must be satisfied:
 * - play_streams synchronization (A/V sync) must be disabled.
 * - no other sinks than encode_streams should be connected to play_streams, especially those that would not consume the 
 * decoded frames, or consume them at a lower speed than encode_streams.
 *
 * \code
 * //create playback and encode objects
 * stm_se_playback_new("playback0", &playback);
 * stm_se_encode_new("encode0", &encode);
 * //disable streams synchronization (A/V sync)
 * stm_se_playback_set_control(playback, STM_SE_CTRL_ENABLE_SYNC, STM_SE_CTRL_VALUE_DISAPPLY);
 * //apply the NRT control to encode
 * //this must be done before any play_stream is attached to an encode_stream
 * stm_se_encode_set_control(encode, STM_SE_CTRL_ENCODE_NRT_MODE, STM_SE_CTRL_VALUE_APPLY);
 * //create video play_stream and encode_stream objects
 * stm_se_play_stream_new("MPEG2 Decode", playback, STM_SE_STREAM_ENCODING_VIDEO_MPEG2, &video_play_stream);
 * stm_se_encode_stream_new("H264 Encode", encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &video_encode_stream);
 * //create a video memory sink to consume the encoded frames
 * stm_memsink_new("Video MemSink", STM_IOMODE_BLOCKING_IO, KERNEL, &video_sink);
 * //attach the video_sink to the video_encode_stream
 * stm_se_encode_stream_attach(video_encode_stream, video_sink);
 * //attach the video_encode_stream to the video_play_stream
 * stm_se_play_stream_attach(video_play_stream, video_encode_stream, STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
 * //create audio play_stream and encode_stream objects
 * stm_se_play_stream_new("MC3 Decode", playback, STM_SE_STREAM_ENCODING_AUDIO_MP3, &audio_play_stream);
 * stm_se_encode_stream_new("AC3 Encode", encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3, &audio_encode_stream);
 * //create a audio memory sink to consume the encoded frames
 * stm_memsink_new("Audio MemSink", STM_IOMODE_BLOCKING_IO, KERNEL, &audio_sink);
 * //attach the audio_sink to the audio_encode_stream
 * stm_se_encode_stream_attach(audio_encode_stream, audio_sink);
 * //attach the audio_encode_stream to the audio_play_stream
 * stm_se_play_stream_attach(audio_play_stream, audio_encode_stream, STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
 * \endcode
 *
 * \subsection tunneled_video_transcode_usage Tunneled video transcode usage
 *
 * This subsection gives guidelines to use a tunneled video transcode, once the network is properly set-up.
 *
 * STKPI client injects PES data in play_stream.
 * The decoded frames at play_stream output will be transparently injected & encoded by the attached encode_stream.
 * STKPI client will then get ES encoded frames and compressed metadata thanks to the attached memsink.
 *
 * At the end of the transcode sequence, STKPI client can inject an 'end_of_stream' discontinuity (EOS) in play_stream
 * and look after this discontinuity downstream, at memsink side.
 * STKPI client will then know that transcode is completed.
 *
 * \code
 * //repeat compressed frames injection in play_stream
 * stm_se_play_stream_inject_data(play_stream, (void *)compressed_video_frame.virtual , COMPRESSED_VIDEO_BUFFER_SIZE);
 * //insert EOS discontinuity in play_stream when stream injection is complete
 * stm_se_play_stream_inject_discontinuity(playback_context->video_stream_handle, false, false, EOS);
 * //Retrieve encoded frames at memsink until receiving EOS discontinuity, as part of compressed metadata
 * //At this step, we consider transcode is over, STKPI network can then be deleted
 * //Remove STKPI objects bindings and delete objects
 * stm_se_play_stream_detach(play_stream, encode_stream);
 * stm_se_encode_stream_detach(encode_stream, video_sink);
 * stm_se_play_stream_delete(play_stream);
 * stm_se_encode_stream_delete(encode_stream);
 * stm_memsink_delete(video_sink);
 * stm_se_encode_delete(encode);
 * stm_se_playback_delete(playback);
 * \endcode
 */


#endif /* STM_SE_PAGES_APPLICATION_NOTES_H__ */
