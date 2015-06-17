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

Source file name : pcm_audio.h
Author :           Julian

Definition of the constants/macros that define useful things associated with 
pcm audio streams.


Date        Modification                                    Name
----        ------------                                    --------
13-Aug-09   Created                                         Julian

************************************************************************/

#ifndef H_PCM_AUDIO
#define H_PCM_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

#define PCM_ENDIAN_LITTLE                       0
#define PCM_ENDIAN_BIG                          1
#define PCM_COMPRESSION_CODE_PCM                1

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the pcm  audio frame header.
///
typedef struct PcmAudioParsedFrameHeader_s
{

} PcmAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct PcmAudioStreamParameters_s
{
    unsigned int        CompressionCode;
    unsigned int        ChannelCount;
    unsigned int        SampleRate;
    unsigned int        BytesPerSecond;
    unsigned int        BlockAlign;
    unsigned int        BitsPerSample;
    unsigned int        DataEndianness;
} PcmAudioStreamParameters_t;

#define BUFFER_PCM_AUDIO_STREAM_PARAMETERS              "PcmAudioStreamParameters"
#define BUFFER_PCM_AUDIO_STREAM_PARAMETERS_TYPE         {BUFFER_PCM_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(PcmAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to Pcm audio.
///
typedef struct PcmAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int        BitRate;

    /// Size of the compressed frame (in bytes)
    unsigned int        FrameSize;

} PcmAudioFrameParameters_t;

#define BUFFER_PCM_AUDIO_FRAME_PARAMETERS            "PcmAudioFrameParameters"
#define BUFFER_PCM_AUDIO_FRAME_PARAMETERS_TYPE       {BUFFER_PCM_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(PcmAudioFrameParameters_t)}


#endif /* H_PCM_AUDIO_ */
