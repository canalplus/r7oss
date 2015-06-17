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

#ifndef H_RMA_AUDIO
#define H_RMA_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//
#define RMA_FRAME_HEADER_SIZE                 0
#define RMA_MAX_OPAQUE_DATA_SIZE              80
#define RMA_MAX_DECODED_SAMPLE_COUNT          1024

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the Rma audio frame header.
///
typedef struct RmaAudioParsedFrameHeader_s
{

} RmaAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct RmaAudioStreamParameters_s
{
    unsigned int        Length;
    unsigned int        HeaderSignature;
    unsigned int        Version;
    unsigned int        RaSignature;
    unsigned int        Size;
    unsigned int        Version2;
    unsigned int        HeaderSize;
    unsigned int        CodecFlavour;
    unsigned int        CodedFrameSize;
    unsigned int        SubPacket;
    unsigned int        FrameSize;
    unsigned int        SubPacketSize;
    unsigned int        SampleRate;
    unsigned int        SampleSize;
    unsigned int        ChannelCount;
    unsigned int        InterleaverId;
    unsigned int        CodecId;
    unsigned int        CodecOpaqueDataLength;
    unsigned int        RmaVersion;
    unsigned int        SamplesPerFrame;
} RmaAudioStreamParameters_t;

#define BUFFER_RMA_AUDIO_STREAM_PARAMETERS        "RmaAudioStreamParameters"
#define BUFFER_RMA_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_RMA_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(RmaAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to Rma audio.
///
typedef struct RmaAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int        BitRate;

    /// Size of the compressed frame (in bytes)
    unsigned int        FrameSize;
} RmaAudioFrameParameters_t;

#define BUFFER_RMA_AUDIO_FRAME_PARAMETERS        "RmaAudioFrameParameters"
#define BUFFER_RMA_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_RMA_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(RmaAudioFrameParameters_t)}


#endif /* H_RMA_AUDIO_ */
