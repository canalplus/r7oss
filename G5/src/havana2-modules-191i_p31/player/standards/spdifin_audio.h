/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : spdifin_audio.h
Author :           Gael

Definition of the types and constants that are used by several components
dealing with spdifin audio decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
18-July-07  built from eac3_audio.h                          Gael

************************************************************************/

#ifndef H_SPDIFIN_AUDIO
#define H_SPDIFIN_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

#define SPDIFIN_START_CODE                          0x0000
#define SPDIFIN_START_CODE_MASK                     0xffff
#define SPDIFIN_FRAME_HEADER_SIZE                   0
#define SPDIFIN_EXTRA_INFO_SIZE                     0
#define SPDIFIN_BYTES_NEEDED                        (SPDIFIN_FRAME_HEADER_SIZE + SPDIFIN_EXTRA_INFO_SIZE)

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching SPDIFIN audio headers
//

////////////////////////////////////////////////////////////////
///
/// SPDIFIN audio stream type, for synchronization of SPDIFIN frames
///
///
typedef enum
{
  TypeSpdifIn,  ///< frame is coming from SPDIF-IN
  TypeHdmiIn,   ///< frame is coming from HDMI-IN
} SpdifinStreamType_t;


///////////////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the first four bytes of the SPDIFIN audio frame header.
///
typedef struct SpdifinAudioParsedFrameHeader_s
{
  // Directly interpretted values
  SpdifinStreamType_t Type;
  unsigned char   SubStreamId;
  unsigned int    SamplingFrequency; ///< Sampling frequency in Hz.
  
  // Derived values
  unsigned int    NumberOfSamples; ///< Number of samples per channel within the frame.
  unsigned int    Length; ///< Length of frame in bytes (including header).
} SpdifinAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct SpdifinAudioStreamParameters_s
{
  /// SPDIFIN to be decoded.
  ///
  /// The ACC firmware requires different configuration parameters information extracted from the 
  /// PES headers to properly process the spdifin input buffers.
  unsigned int SamplingFreq;
  unsigned int NbSamples;
} SpdifinAudioStreamParameters_t;


#define BUFFER_SPDIFIN_AUDIO_STREAM_PARAMETERS        "SpdifinAudioStreamParameters"
#define BUFFER_SPDIFIN_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_SPDIFIN_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(SpdifinAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to Spdifin audio.
///
/// We already know from collator all codec params
/// just transmit them from collator to codec

#define SpdifinAudioFrameParameters_t SpdifinAudioParsedFrameHeader_t 

#define BUFFER_SPDIFIN_AUDIO_FRAME_PARAMETERS        "SpdifinAudioFrameParameters"
#define BUFFER_SPDIFIN_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_SPDIFIN_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(SpdifinAudioFrameParameters_t)}

#endif /* H_SPDIFIN_AUDIO */
