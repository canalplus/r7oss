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

Source file name : stm_se/pages/introduction.h

************************************************************************/

#ifndef STM_SE_PAGES_INTRODUCTION_H__
#define STM_SE_PAGES_INTRODUCTION_H__

/*!
 * \page introduction Introduction
 *
 * \section purpose_and_scope Purpose and scope
 *
 * The purpose of this document is to describe the public interface of the streaming engine. This
 * kernel-level interface, which is intended to be used by middleware, provides low-level interfaces to
 * configure and supply data to media transform pipelines (streams). As such, the streaming engine API hides
 * the details as to how the various audio/video stream players and encoders are implemented.
 *
 * \subsection api_stability API stability
 *
 * The API in this document reflects a work in progress. It is important to clearly communicate the
 * stability of the APIs proposed. The majority are intended to be final, enabling the development of
 * high-level functionality, above them. However, some parts of the API are transitory. There are designed
 * to act as scaffolding until suitable system-wide frameworks are developed to replace them.
 *
 * All functions, types, or constants within this document fall into one of the following categories:
 *
 * <table>
 *     <tr>
 *         <td>
 *             Stable
 *         </td>
 *         <td>
 *             This function, type, or constant has been proven both useful and usable in a practical media
 *             playback system. The name is frozen, so too is the parameter list, member list or value
 *             (respectively). Future extensions will be made in a backwards compatible manner.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Unstable
 *         </td>
 *         <td>
 *             This function, type, or constant is needed to construct a practical media playback system
 *             but, directional changes required to elegantly support G4 platforms (and beyond) mean that it
 *             has not been proven by real world use. The name is frozen but other aspects such as the
 *             parameter or member list is not.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Proposed
 *         </td>
 *         <td>
 *             This function, type, or constant is not yet implemented but it expected to be introduced in
 *             future implementations.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Temporary
 *         </td>
 *         <td>
 *             This function, type, or constant is temporarily provided to permit a useful system to be
 *             constructed before suitable framework exists to provide the supported features. It is likely
 *             to be removed or substantially redesigned in future revisions of the API.
 *         </td>
 *     </tr>
 * </table>
 *
 * Any function, type, or constant that is not Stable is marked appropriately under a specially headed
 * section, Stability.
 *
 * \subsection api_roadmap API roadmap
 *
 * The API in this document does not yet address all the use cases supported by upcoming ST systems-on-chip.
 * It is anticipated that these use cases can be addressed through the introduction of new API rather than
 * by modifying the existing API. Thus, subject to the stability restrictions above, source code
 * compatibility problems are anticipated as new features are added to the API.
 *
 * The following use cases are currently identified as features to be addressed in future revisions of the
 * API:
 *
 * * Interfaces to ST proprietary tuning tools (for example, the audio post processing tool)
 *
 * * Detailed specification of encoded output data (especially meta data)
 *
 * \section intended_audience Intended audience
 *
 * This document is targeted at the following audiences:
 *
 * * Developers responsible for the design and implementation of the middleware software on top of the
 *   streaming engine.
 *
 * * Developers responsible for the design and implementation of the streaming engine.
 *
 * * Quality assurance engineers responsible for testing the streaming engine.
 *
 * * Program and project managers responsible for planning and tracking the streaming engine.
 *
 * \section acronyms_and_abbreviations Acronyms and abbreviations
 *
 * <table>
 *     <tr>
 *         <td>
 *             AAC
 *         </td>
 *         <td>
 *             AdvanceAudioCoding refers to a multichannel music compression technology developed by
 *             Fraunhofer Institute and Coding Technology (now Dolby(r) Laboratories)
 *
 *             Also referred as MPEG2-AAC Low Complexity , MPEG4-AAC LC, HighEfficiency-AACv1 (that is
 *             AAC+SubBandReplication or HE-AACv2 (that is AAC + ParametricStereo) or DolbyPulse (that is
 *             HE-AACv2 plus Dolby metadata)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             Absolute time
 *         </td>
 *         <td>
 *             An absolute time reference derived from the monotonic system clock (CLOCK_MONOTONIC)
 *             converted into micro seconds.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             AC3
 *         </td>
 *         <td>
 *             Audio Code number 3 refers to a multi channel music compression technology developed by
 *             Dolby(r) Laboratories.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             API
 *         </td>
 *         <td>
 *             Application Program Interface
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             DTS
 *         </td>
 *         <td>
 *             DigitalTheaterSound CoherentAccoustic refers to a multichannel music compression technology
 *             developed by DTS inc.
 *
 *             Variant of this technology can be referred as DTS-HighResolution or DTS-MasterAudio
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             ED
 *         </td>
 *         <td>
 *             Extended Definition video signal (480p and 576p)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             ES
 *         </td>
 *         <td>
 *             Elementary Stream
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             HBRA
 *         </td>
 *         <td>
 *             HighBitRateAudio refers to the transmission mode of compressed high bitrate audio content (up
 *             to 24 Mbps) like Dolby-TrueHD or DTS-MA.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             HD
 *         </td>
 *         <td>
 *             High Definition video signal (1080i, 720P and 1080p)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             MAT
 *         </td>
 *         <td>
 *             Metadata-enhancedAudioTransmission refers to a technology developed by Dolby(r) Laboratories
 *             to convert VBR high bitrate audio to CBR for transmission over HDMI (primarily applies to the
 *             transmission of Dolby-TrueHD over HDMI)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             MPEG
 *         </td>
 *         <td>
 *             The Moving Picture Experts Group - An ISO/IEC standard for compressing of video and audio.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             MP3
 *         </td>
 *         <td>
 *             Audio Layer III for MPEG1, MPEG2 or MPEG2.5
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             OS
 *         </td>
 *         <td>
 *             Operating System
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             PCM
 *         </td>
 *         <td>
 *             Pulse Code Modulation
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             PES
 *         </td>
 *         <td>
 *             Packetized Elementary Stream
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             PIP
 *         </td>
 *         <td>
 *             Picture In Picture
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             PMT
 *         </td>
 *         <td>
 *             Program Map Table.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             RTC
 *         </td>
 *         <td>
 *             Real Time Clock
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             SD
 *         </td>
 *         <td>
 *             Standard Definition video signals - NTSC (480i) and PAL (576i)
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             SIT
 *         </td>
 *         <td>
 *             Selection Information Table
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             SPDIF
 *         </td>
 *         <td>
 *             Sony/Philips Digital Interface - A serial interface for transferring digital audio
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             SPTS
 *         </td>
 *         <td>
 *             Single Program Transport Stream
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             SRS
 *         </td>
 *         <td>
 *             SRS audio technology enhancements developed by SRS labs.
 *         </td>
 *     </tr>
 *     <tr>
 *         <td>
 *             TrueHD
 *         </td>
 *         <td>
 *             Refers to a multichannel lossless audio compression technology developed by Dolby(r)
 *             Laboratories.
 *         </td>
 *     </tr>
 * </table>
 *
 * \section header_files Header files
 *
 * All functions, types, and constants introduced within this document can be included in client source code
 * using a single header file, stm_se.h.
 */


#endif /* STM_SE_PAGES_INTRODUCTION_H__ */
