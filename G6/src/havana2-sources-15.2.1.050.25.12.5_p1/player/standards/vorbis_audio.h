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

#ifndef H_VORBIS_AUDIO
#define H_VORBIS_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

/* Incoming information header values */
#define VORBIS_IDENTIFICATION_HEADER            0x01
#define VORBIS_COMMENT_HEADER                   0x03
#define VORBIS_SETUP_HEADER                     0x05

////////////////////////////////////////////////////////////////////
//
//  Standard constants
//

// From Vorbis_I_spec.pdf , page 10 :
//  In Vorbis I, legal frame sizes are powers of two from 64 to 8192 samples
#define VORBIS_MAX_NB_SAMPLES_PER_FRAME         8192

#define VORBIS_MAX_DECODED_SAMPLE_COUNT         1024

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the ogg Vorbis audio frame header.
///
typedef struct VorbisAudioParsedFrameHeader_s
{

} VorbisAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct VorbisAudioStreamParameters_s
{
    unsigned int        VorbisVersion;
    unsigned int        ChannelCount;
    unsigned int        SampleRate;
    unsigned int        BlockSize0;
    unsigned int        BlockSize1;
    unsigned int        SampleSize;
    unsigned int        CodecId;
    unsigned int        SamplesPerFrame;
} VorbisAudioStreamParameters_t;

#define BUFFER_VORBIS_AUDIO_STREAM_PARAMETERS           "VorbisAudioStreamParameters"
#define BUFFER_VORBIS_AUDIO_STREAM_PARAMETERS_TYPE      {BUFFER_VORBIS_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(VorbisAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to Ogg Vorbis audio.
///
typedef struct VorbisAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int        BitRate;

    /// Size of the compressed frame (in bytes)
    unsigned int        FrameSize;

    bool                SamplesPresent;                 /* true if frame contains sample data, false if header only */
} VorbisAudioFrameParameters_t;

#define BUFFER_VORBIS_AUDIO_FRAME_PARAMETERS            "VorbisAudioFrameParameters"
#define BUFFER_VORBIS_AUDIO_FRAME_PARAMETERS_TYPE       {BUFFER_VORBIS_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(VorbisAudioFrameParameters_t)}


#endif /* H_VORBIS_AUDIO_ */
