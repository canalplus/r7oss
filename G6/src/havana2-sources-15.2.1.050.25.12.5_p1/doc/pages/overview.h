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

Source file name : stm_se/pages/overview.h

************************************************************************/

#ifndef STM_SE_PAGES_OVERVIEW_H__
#define STM_SE_PAGES_OVERVIEW_H__

/*!
 * \page overview Overview
 *
 * \internal
 * \warning The chapter comes verbatim from the API manual and is describing
 *          the streaming engine from a caller's point-of-view. See
 *          \ref pure_classes_sec to learn about the the internal structure.
 * \endinternal
 *
 * \section introduction Introduction
 * 
 * The streaming engine (SE) provides functionality both for the decoding of streams of media data and
 * rendering to a suitable device at the correct time and for encoding media data in real time for
 * transmission of local storage. The functions can be combined in order to enable real time transcoding of
 * media data.
 * 
 * The streaming engine exposes the functionality through an API that is hardware independent, meaning that
 * the API does not expose any hardware specific control. All hardware specific configurations are
 * centralized and performed at STKPI initialization. A default configuration is provided by the system that
 * should fit the majority of customer needs.
 * 
 * Some of the controls presented by objects, especially those that manipulate post-processing operations,
 * may influence hardware behavior and be unavailable on platforms where the required hardware is absent.
 * However, no explicit knowledge of the hardware is required to operate them.
 * 
 * \section concepts Concepts
 * 
 * It is out of the scope of this API document to fully explain or justify the philosophy behind the
 * concepts of STKPI Component API design. For a full explanation of the architecture and usage, refer to
 * the STKPI Architectural Concepts documentation.
 * 
 * The streaming engine object, like all in the STKPI suite, enables the user to write software that closely
 * models the data paths used in their architectural model. Data interfaces between STKPI components are
 * defined internally between the components. The interfaces abstract details of the communication
 * mechanisms implemented away from the user of the components. The user need only specify what components
 * are attached in the data paths. This strategy allows the implementation of the software and hardware to
 * evolve without affecting the conceptual model, thereby preventing an enforced change of the API and, by
 * implication, a change to the interfacing user code.
 * 
 * \section streaming_engine_se_playback_model Streaming Engine (SE) playback model
 * 
 * The streaming engine playback model revolves around the playback and play stream objects. While several
 * other objects are introduced in this document, their role is either to source data for, or sink data
 * from, the play objects.
 * 
 * The playback object is a container for play streams that groups together play streams with coherent
 * timing information. All members of a playback are presented against the same system clock and enter trick
 * mode playback as a single unit.
 * 
 * A play stream object takes PES input, decodes that input producing frames for rendering, and supplies
 * timing information to the renderer to ensure the frames are presented at the correct time. A play stream
 * supports audio and video media types. The media type is specified during object instantiation and cannot
 * be changed after this point.
 *
 * \image html playback_model.svg
 * \image latex playback_model.pdf
 *
 * \section streaming_engine_se_encode_model Streaming Engine (SE) encode model
 * 
 * The streaming engine encode model is similar to the playback model albeit but around the encode and
 * encode stream objects.
 * 
 * The encode object is an encode streams container that groups together encode streams
 * with coherent timing information.
 * 
 * An encode stream object takes uncompressed data as input, encodes that input producing frames of
 * elementary stream, and supplied the frames, together with meta data (including presentation time) to a
 * sink device, typically multiplexor.
 *
 * \image html encode_model.svg
 * \image latex encode_model.pdf
 *
 * \note The multiplexor forms part of the transport engine and is therefore not described in this API
 *       specification.
 * 
 * \section supported_objects Supported objects
 * 
 * The table below shows the objects implemented by the streaming engine. To input data to the filter
 * objects, the stream needs to be sent to the parent demux; it is then automatically routed to all input
 * filters (STM_TE_PID_FILTERs).
 * 
 * This will become clearer when understanding object hierarchy, explained later.
 * 
 * <table>
 *     <tr>
 *         <td>
 *             Object Name
 *         </td>
 *         <td>
 *             Supported Interfaces
 *         </td>
 *         <td>
 *             Description
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             playback
 *         </td>
 *         <td>
 *             \image html stm_se_playback.svg
 *             \image latex stm_se_playback.pdf
 *         </td>
 *         <td>
 *             The playback object is a container for play stream objects.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             play stream
 *         </td>
 *         <td>
 *             \image html stm_se_play_stream.svg
 *             \image latex stm_se_play_stream.pdf
 *         </td>
 *         <td>
 *             The play stream object is used to decode a media stream.
 * 
 *             The object provides audio/video data via the port marked Out and user/ancillary data via the
 *             port marked UD, semantically the UD output is considered metadata and notated on the long
 *             edge of the component.
 * 
 *             \note The Out port may have multiple sink objects attached to it to meet use cases with
 *                   multiple output surfaces.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             encode
 *         </td>
 *         <td>
 *             \image html stm_se_encode.svg
 *             \image latex stm_se_encode.pdf
 *         </td>
 *         <td>
 *             The encode object is an encode streams container that groups together encode streams
 *             with coherent timing information.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             encode stream
 *         </td>
 *         <td>
 *             \image html stm_se_encode_stream.svg
 *             \image latex stm_se_encode_stream.pdf
 *         </td>
 *         <td>
 *             The encode stream object is used to encode a media stream.
 * 
 *             The object issues compressed audio/video data via the port marked Out.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             audio generator
 *         </td>
 *         <td>
 *             \image html stm_se_audio_generator.svg
 *             \image latex stm_se_audio_generator.pdf
 *         </td>
 *         <td>
 *             The audio generator is an adapter object used to connect a stream of audio generated outside
 *             the streaming engine directly to an audio mixer.
 * 
 *             It is used to inject sound effects and UI beeps into the audio output system with minimal
 *             latency. It is not suitable for long running streams, which must be injected via a play
 *             stream object in order to undergo synchronization.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             audio mixer
 *         </td>
 *         <td>
 *             \image html stm_se_audio_mixer.svg
 *             \image latex stm_se_audio_mixer.pdf
 *         </td>
 *         <td>
 *             The audio mixer combines multiple inputs, both from audio play streams and audio generators
 *             and prepares them for output by an audio player.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             audio player
 *         </td>
 *         <td>
 *             \image html stm_se_audio_player.svg
 *             \image latex stm_se_audio_player.pdf
 *         </td>
 *         <td>
 *             The audio player accepts data from the audio mixer and renders it. It can also be used to
 *             configure the post-processing chain for each audio output.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             audio reader
 *         </td>
 *         <td>
 *             \image html stm_se_audio_reader.svg
 *             \image latex stm_se_audio_reader.pdf
 *         </td>
 *         <td>
 *             The audio reader injects data read from PCM reader hardware found on the SoC, combines it
 *             with timing information, and injects it into a play stream.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             audio card
 *         </td>
 *         <td>
 *             \image html stm_se_audio_card.svg
 *             \image latex stm_se_audio_card.pdf
 *         </td>
 *         <td>
 *             The audio card provides a control interface to configure the shared audio hardware of a SoC.
 *             This includes ADC and DAC gains as well as any muxes found in the audio input/output paths.
 *         </td>
 *     </tr>
 * </table>
 * 
 * \note Sinks to render video pixel data to a display device are provided by the CoreDisplay sub-system's
 *       queue source object and therefore do not appear in the above list.
 * 
 * \subsection example_graphs Example graphs
 * 
 * The following graph shows a playback-from-live scenario. There are three play streams, primary audio and
 * video together with an audio description stream. These are decoded to one video output and three audio
 * outputs, where all three outputs play the same content.
 *
 * \image html example_graph_single_mixer.svg
 * \image latex example_graph_single_mixer.pdf
 *
 * \note Both the demux input chain and the video output chain shown above are incomplete because they are
 *       outside the scope of this document. Specifically, the PES and PCR filters are not shown contained by a
 *       demux object and the queue source has a dangling output port.
 *
 * The following graph shows how multiple mixer objects can be used to allow different content to be sent to
 * the audio players. In this example, one mixer is presenting the main feature while the other is presenting both the main
 * feature and the audio description.
 *
 * \image html example_graph_dual_mixer.svg
 * \image latex example_graph_dual_mixer.pdf
 *
 * \note In addition to obviously different content, such as the presence/absence of audio description, the
 *       content might differ only in the time domain it is presented in. Specifically, if one audio player
 *       presents at a fixed clock rate, with sync maintained by digital sample rate conversion, while another is
 *       derived from the recovered clock, then the content is different meaning two audio mixers must be used to
 *       present it.
 *
 * \section time_units_within_the_streaming_engine Time units within the streaming engine
 * 
 * The streaming engine is, for many parts of the system, responsible for managing time and manipulating
 * timestamps. It is therefore useful to define the time units used by the streaming engine.
 * 
 * \subsection local_clock Local clock
 * 
 * The local clock provides an absolute time reference for use system wide. This clock is provided by the
 * operating system and is used by all STKPI sub-systems that must communicate or exchange time values. For
 * example the display queue source object accepts frames stamped with this clock and reports the actual
 * presentation time using the same clock.
 * 
 * The local clock is always expressed in microseconds. Time values expressed against the local clock are
 * considered to be in monotonic time.
 * 
 * In user space this clock can be accessed using the following calls:
 * 
 * \code
 * #define UNSPECIFIED_TIME 0xfeedfacedeadbef0ull
 * uint64_t get_monotonic_system_clock()
 * {
 * 	int res;
 * 	struct timespec t;
 * 	res = clock_gettime(CLOCK_MONOTONIC, &t);
 * 	if (0 != res)
 * 		return UNSPECIFIED_TIME;
 * 	return (1000000ull * t.tv_sec) + (t.tv_nsec / 1000);
 * }
 * \endcode
 * 
 * \subsection system_time_clock System time clock
 * 
 * This clock is a local derivative of the system time clock of the upstream transmitter. This is typically
 * reconstructed locally from PCR packets embedded in the transport stream from the transmitter. The process
 * of deriving the system time clock is called clock recovery.
 * 
 * The system time clock is not implemented using a physical oscillator but is maintained algorithmically,
 * allowing system clock values to be converted to and from local clock values. It can be extracted from
 * playback and encode objects as clock data points. A clock data point is a pair of timestamps, one is a
 * system time clock value and the other is a local clock value.
 * 
 * \note Each playback and encode object within the system may operate with different system time clocks.
 *       This allows multiple upstream transmitters to be supported without any constraint on the time
 *       relationship between these upstream transmitters.
 * 
 * The system time clock uses the time units of the transmission system, typically this is 90 kHz or some
 * exact multiple for MPEG derived systems. Time values expressed against the system time clock are
 * considered to be in native time. By way of example PTS and DTS values are expressed in native time.
 */


#endif /* STM_SE_PAGES_OVERVIEW_H__ */
