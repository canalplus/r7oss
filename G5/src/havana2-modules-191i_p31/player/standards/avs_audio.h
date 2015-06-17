/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : avs_audio.h
Author :           Andy

Definition of the constants/macros that define useful things associated with 
AVS audio streams.


Date        Modification                                    Name
----        ------------                                    --------

************************************************************************/

#ifndef H_AVS_AUDIO
#define H_AVS_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

// Elementary stream constants

#define AVS_AUDIO_PARTIAL_FRAME_HEADER_SIZE        4
#define AVS_AUDIO_FRAME_HEADER_SIZE               8

#define AVS_AUDIO_START_CODE_MASK                 0xffe00000
#define AVS_AUDIO_START_CODE                      0xffe00000

#define AVS_AUDIO_LAYER_MASK                      0x00060000
#define AVS_AUDIO_LAYER_I                         0x00060000
#define AVS_AUDIO_LAYER_II                        0x00040000
#define AVS_AUDIO_LAYER_III                       0x00020000

#define AVS_AUDIO_AVS_MASK                       0x00180000
#define AVS_AUDIO_AVS_1                          0x00180000
#define AVS_AUDIO_AVS_2                          0x00100000
#define AVS_AUDIO_AVS_2_5                        0x00000000

#define AVS_AUDIO_BIT_RATE_MASK                   0x0000f000
#define AVS_AUDIO_BIT_RATE_SHIFT                  (4+8)

#define AVS_AUDIO_SAMPLING_FREQUENCY_MASK         0x00000c00
#define AVS_AUDIO_SAMPLING_FREQUENCY_SHIFT        (2+8)

#define AVS_AUDIO_PADDING_MASK                    0x00000200
#define AVS_AUDIO_PADDING_SHIFT                   (1+8)

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching avs audio headers
//

///////////////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the first four bytes of the AVS audio frame header.
///
typedef struct AvsAudioParsedFrameHeader_s
{
    // Raw value
    unsigned int Header; ///< Original header (big endian)

    // Directly interpretted values
    unsigned char Layer; ///< AVS audio layer. Either 1, 2 or 3.
    unsigned char Standard; ///< AVS standard (sample rate grouping). Either 1, 2 or 25 (unofficial AVS 2.5 standard)
    unsigned short BitRate; ///< AVS bit rate in kbits/sec.
    unsigned int SamplingFrequency; ///< Sampling frequency in Hz.
    bool PaddedFrame; ///< True if the padding bit is set.

    // Derived values
    unsigned int NumberOfSamples; ///< Number of samples per channel within the frame.
    unsigned int Length; ///< Length of frame in bytes (including header).
} AvsAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct AvsAudioStreamParameters_s
{
    /// Avs Layer to be decoded.
    ///
    /// The ACC firmware requires different configuration parameters for Layer III audio.
    /// This makes the Layer a stream parameters (since the layer can only be changed
    /// by issuing a MME_CommandCode_t::MME_SET_GLOBAL_TRANSFORM_PARAMS command).
    unsigned int Layer;
} AvsAudioStreamParameters_t;

#define BUFFER_AVS_AUDIO_STREAM_PARAMETERS        "AvsAudioStreamParameters"
#define BUFFER_AVS_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_AVS_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AvsAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to Avs audio.
///
/// \todo This is actually pretty generic stuff; if nothing radical emerges
///       in the other audio codecs perhaps this should be combined. Alternatively
///       just get rid of this type entirely and rename AvsAudioParsedFrameHeader_t.
///
typedef struct AvsAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int BitRate;

    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} AvsAudioFrameParameters_t;

#define BUFFER_AVS_AUDIO_FRAME_PARAMETERS        "AvsAudioFrameParameters"
#define BUFFER_AVS_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_AVS_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AvsAudioFrameParameters_t)}

#endif /* H_AVS_AUDIO */
