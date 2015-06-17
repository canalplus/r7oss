/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : mpeg_audio.h
Author :           Nick

Definition of the constants/macros that define useful things associated with 
MPEG audio streams.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-03   Created                                         Nick
18-Apr-07   Ported to player2                               Daniel

************************************************************************/

#ifndef H_MPEG_AUDIO
#define H_MPEG_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

// Elementary stream constants

#define MPEG_AUDIO_PARTIAL_FRAME_HEADER_SIZE	   4
#define MPEG_AUDIO_FRAME_HEADER_SIZE               8

#define MPEG_AUDIO_START_CODE_MASK                 0xffe00000
#define MPEG_AUDIO_START_CODE                      0xffe00000

#define MPEG_AUDIO_LAYER_MASK                      0x00060000
#define MPEG_AUDIO_LAYER_I                         0x00060000
#define MPEG_AUDIO_LAYER_II                        0x00040000
#define MPEG_AUDIO_LAYER_III                       0x00020000

#define MPEG_AUDIO_MPEG_MASK                       0x00180000
#define MPEG_AUDIO_MPEG_1                          0x00180000
#define MPEG_AUDIO_MPEG_2                          0x00100000
#define MPEG_AUDIO_MPEG_2_5                        0x00000000

#define MPEG_AUDIO_BIT_RATE_MASK                   0x0000f000
#define MPEG_AUDIO_BIT_RATE_SHIFT                  (4+8)

#define MPEG_AUDIO_SAMPLING_FREQUENCY_MASK         0x00000c00
#define MPEG_AUDIO_SAMPLING_FREQUENCY_SHIFT        (2+8)

#define MPEG_AUDIO_PADDING_MASK                    0x00000200
#define MPEG_AUDIO_PADDING_SHIFT                   (1+8)

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching mpeg audio headers
//

///////////////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the first four bytes of the MPEG audio frame header.
///
typedef struct MpegAudioParsedFrameHeader_s
{
    // Raw value
    unsigned int Header; ///< Original header (big endian)

    // Directly interpretted values
    unsigned char Layer; ///< MPEG audio layer. Either 1, 2 or 3.
    unsigned char MpegStandard; ///< MPEG standard (sample rate grouping). Either 1, 2 or 25 (unofficial MPEG 2.5 standard) 
    unsigned short BitRate; ///< MPEG bit rate in kbits/sec.
    unsigned int SamplingFrequency; ///< Sampling frequency in Hz.
    bool PaddedFrame; ///< True if the padding bit is set.
	
    // Derived values
    unsigned int NumberOfSamples; ///< Number of samples per channel within the frame.
    unsigned int Length; ///< Length of frame in bytes (including header).
} MpegAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct MpegAudioStreamParameters_s
{
    /// MPEG Layer to be decoded.
    ///
    /// The ACC firmware requires different configuration parameters for Layer III audio.
    /// This makes the Layer a stream parameters (since the layer can only be changed
    /// by issuing a MME_CommandCode_t::MME_SET_GLOBAL_TRANSFORM_PARAMS command).
    unsigned int Layer;
} MpegAudioStreamParameters_t;

#define BUFFER_MPEG_AUDIO_STREAM_PARAMETERS        "MpegAudioStreamParameters"
#define BUFFER_MPEG_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_MPEG_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MpegAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to MPEG audio.
///
/// \todo This is actually pretty generic stuff; if nothing radical emerges
///       in the other audio codecs perhaps this should be combined. Alternatively
///       just get rid of this type entirely and rename MpegAudioParsedFrameHeader_t.
///
typedef struct MpegAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int BitRate;
    
    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} MpegAudioFrameParameters_t;

#define BUFFER_MPEG_AUDIO_FRAME_PARAMETERS        "MpegAudioFrameParameters"
#define BUFFER_MPEG_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_MPEG_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MpegAudioFrameParameters_t)}

#endif /* H_MPEG_AUDIO */
