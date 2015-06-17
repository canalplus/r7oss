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

Source file name : wma.h
Author :           Sylvain

Definition of the types and constants that are used by several components
dealing with wma audio decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
24-Apr-06   Created                                         Mark
06-June-07  Ported to player2                               Sylvain

************************************************************************/

#ifndef H_WMA_
#define H_WMA_

#include "pes.h"
#include "asf.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//
#define WMA_FRAME_HEADER_SIZE                 0

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the WMA audio frame header.
///
typedef struct WmaAudioParsedFrameHeader_s
{

} WmaAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct WmaAudioStreamParameters_s
{
//    unsigned int Unused;

    unsigned int        StreamNumber;
    unsigned int        FormatTag;
    unsigned int        NumberOfChannels;
    unsigned int        SamplesPerSecond;
    unsigned int        AverageNumberOfBytesPerSecond;
    unsigned int        BlockAlignment;
    unsigned int        BitsPerSample;
    unsigned int        ValidBitsPerSample;
    unsigned int        ChannelMask;
    unsigned int        SamplesPerBlock;
    unsigned int        EncodeOptions;
    unsigned int        SuperBlockAlign;
    // calculated parameters
    unsigned int        SamplesPerFrame;
    unsigned int        SamplingFrequency;
//


} WmaAudioStreamParameters_t;

#define BUFFER_WMA_AUDIO_STREAM_PARAMETERS        "WmaAudioStreamParameters"
#define BUFFER_WMA_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_WMA_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(WmaAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to WMA audio.
///
/// \todo This is actually pretty generic stuff; if nothing radical emerges
///       in the other audio codecs perhaps this should be combined. Alternatively
///       just get rid of this type entirely and rename MpegAudioParsedFrameHeader_t.
///
typedef struct WmaAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int BitRate;

    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} WmaAudioFrameParameters_t;

#define BUFFER_WMA_AUDIO_FRAME_PARAMETERS        "WmaAudioFrameParameters"
#define BUFFER_WMA_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_WMA_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(WmaAudioFrameParameters_t)}


#endif /* H_WMA_ */
