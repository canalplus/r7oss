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

#ifndef H_ADPCM_
#define H_ADPCM_

#include "pes.h"



// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//


// Define the structure of the private data for the adpcm PES packets.
// This is the structure that is expected by the Streaming Engine.
typedef struct AdpcmPrivateData_s
{
    unsigned char           StreamId;
    unsigned int            Frequency;
    unsigned char           NbOfCoefficients;
    unsigned char           NbOfChannels;
    unsigned short          NbOfSamplesPerBlock;
    unsigned char           LastPacket;
} __attribute__((packed)) AdpcmPrivateData_t;


// Define the size of the private data. CAUTION: this size must be lower than
// 16 which is the size of the optional bytes for private data in the PES packet header
// (PES specification).
#define ADPCM_PRIVATE_DATE_LENGTH  sizeof(struct AdpcmPrivateData_s)

#define ADPCM_DRC_VALUE_DISABLE                0x80

#define ADPCM_MAXIMUM_SAMPLE_COUNT             4096

////////////////////////////////////////////////////////////////
///
/// LPCM audio stream type
///
///
typedef enum
{
    TypeImaAdpcm,
    TypeMsAdpcm,
    TypeAdpcmInvalid = 16
} AdpcmStreamType_t;


////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the ADPCM audio frame header.
///
typedef struct AdpcmAudioParsedFrameHeader_s
{
    // These fields contain all these required by the audio mme codec
    AdpcmStreamType_t   Type;
    unsigned int        SamplingFrequency1;         ///< Expressed in Hz
    unsigned char       NumberOfChannels;           ///< Number of channels.
    unsigned char       NbOfCoefficients;           ///< Number of ADPCM coefficients.
    unsigned char       NbOfBlockPerBufferToDecode; ///< Number of ADPCM block to put in the buffer to decode.
    unsigned int        NbOfSamplesOverAllBlocks;   ///< Total number of samples ver all the blocks
    unsigned int        SizeOfBufferToDecode;       ///< The size if the buffer to decode.
    unsigned int        AdpcmBlockSize;             ///< The size of an ADPCM block.
    unsigned char       DrcCode;                    ///< Drc Coefficient.
    unsigned int        PrivateHeaderLength;        ///< Length of private data area
    unsigned char       SubStreamId;                ///< Substream identifier for this pes
} AdpcmAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

#define AdpcmAudioStreamParameters_t AdpcmAudioParsedFrameHeader_t

#define BUFFER_ADPCM_AUDIO_STREAM_PARAMETERS        "AdpcmAudioStreamParameters"
#define BUFFER_ADPCM_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_ADPCM_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AdpcmAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to ADPCM audio.
/// We already know from the collator all the codec parameters from the collator,
/// pass them from the collator to the codec
///
typedef struct AdpcmAudioFrameParameters_s
{
    unsigned char       DrcCode;                    ///< Drc Coefficient.
    unsigned int        NbOfSamplesOverAllBlocks;   ///< Total number of samples in the buffer to decode.
    unsigned int        NbOfBlockPerBufferToDecode; ///< Number of AccessUnit
} AdpcmAudioFrameParameters_t;

#define BUFFER_ADPCM_AUDIO_FRAME_PARAMETERS        "AdpcmAudioFrameParameters"
#define BUFFER_ADPCM_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_ADPCM_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AdpcmAudioFrameParameters_t)}

#endif /* H_ADPCM_ */



