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

Source file name : vorbis_audio.h
Author :           Julian

Definition of the constants/macros that define useful things associated with 
Ogg Vorbid audio streams.


Date        Modification                                    Name
----        ------------                                    --------
11-Mar-09   Created                                         Julian

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
