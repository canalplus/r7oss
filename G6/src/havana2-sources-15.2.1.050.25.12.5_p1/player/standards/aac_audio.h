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

#ifndef H_AAC_AUDIO
#define H_AAC_AUDIO

#include "pes.h"
#include "codec_mme_audio_aac.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

// Elementary stream constants

#define AAC_AUDIO_FRAME_HEADER_SIZE                8

#define AAC_ADTS_START_CODE_BYTE0                  0xff
#define AAC_ADTS_START_CODE_BYTE1                  0xf0

#define AAC_ADTS_START_CODE_BYTE1_MASK             0xf6

#define AAC_LOAS_ASS_START_CODE_BYTE0              0x56 //  first byte of (0x2b7 << 5)
#define AAC_LOAS_ASS_START_CODE_BYTE1              0xe0 //  second byte of (0x2b7 << 5)

#define AAC_LOAS_ASS_START_CODE_BYTE1_MASK         0xe0

#define AAC_LOAS_ASS_START_CODE_BYTE3              0x00 //  useSameStreammux should not be set

#define AAC_LOAS_ASS_START_CODE_BYTE3_MASK         0x80

#define AAC_LOAS_EPASS_START_CODE_BYTE0            0x4d
#define AAC_LOAS_EPASS_START_CODE_BYTE1            0xe1

#define AAC_AUDIO_ADTS_SYNC_WORD                   0xfff
#define AAC_AUDIO_ADIF_SYNC_WORD                   0x41444946 // "ADIF"
#define AAC_AUDIO_LOAS_ASS_SYNC_WORD               0x2b7
#define AAC_AUDIO_LOAS_EPASS_SYNC_WORD             0x4de1

#define AAC_AUDIO_PROFILE_LC                       2
#define AAC_AUDIO_PROFILE_SBR                      5

#define AAC_LOAS_ASS_SYNC_LENGTH_HEADER_SIZE       3
#define AAC_LOAS_ASS_MAX_FRAME_SIZE                8192
#define AAC_LOAS_EP_ASS_HEADER_SIZE                7

// As per IEC 13818-7:1997(E) max raw bitrate is 6144 bit per frame per channel
// Max raw-data-block size in byte is thus obtained by :
#define AAC_MAX_RAW_DATA_BLOCK_SIZE(nchan)  (6144 * nchan) / 8
#define AAC_ADTS_MIN_FRAME_SIZE                    7
#define AAC_AUDIO_MAX_FORMAT_TYPE                  STM_SE_AAC_BSAC_PROFILE
#define AAC_AUDIO_MAX_APPLICATION_POLICY           STM_SE_CTRL_VALUE_AUDIO_APPLICATION_MS11

#define MIXER_HEAAC_PC_REP_PERIOD_960        (1 << 5) + (1 << 8)
#define MIXER_HEAAC_PC_REP_PERIOD_1024       (1 << 5)
#define MIXER_HEAAC_PC_REP_PERIOD_1920       (2 << 5) + (1 << 8)
#define MIXER_HEAAC_PC_REP_PERIOD_2048       (2 << 5)

#define AAC_DEFAULT_NBSAMPLES_PER_FRAME      1024

// STM-SE Limitations :
#define AAC_MAX_NB_CHANNELS                    6
#define AAC_ADTS_NB_MAX_RAW_DATA_BLOCK         2 // This is a StreamingEngine design constraint : we support up to 2 raw data block per ADTS frame
#define AAC_MAX_NBSAMPLES_PER_FRAME            (AAC_DEFAULT_NBSAMPLES_PER_FRAME * AAC_ADTS_NB_MAX_RAW_DATA_BLOCK)
#define AAC_ADTS_MAX_FRAME_SIZE(nchan, nblock) (AAC_ADTS_MIN_FRAME_SIZE + (AAC_MAX_RAW_DATA_BLOCK_SIZE(nchan) * nblock))

// same definition as the audio firwmare
typedef enum
{
    AAC_GET_SYNCHRO,
    AAC_GET_LENGTH,
    AAC_GET_FRAME_PROPERTIES
} AacFrameParsingPurpose_t;

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching aac audio headers
//

///////////////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the first four bytes of the MPEG audio frame header.
///
typedef struct AacAudioParsedFrameHeader_s
{
    eAacFormatType Type;

    // Directly interpretted values
    unsigned int SamplingFrequency; ///< Sampling frequency in Hz.

    // Derived values
    bool         ExpliciteSbrExtension;
    unsigned int NumberOfSamples; ///< Number of samples per channel within the frame.
    unsigned int Length; ///< Length of frame in bytes (including header).
} AacAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct AacAudioStreamParameters_s
{
    /// MPEG Layer to be decoded.
    ///
    /// The ACC firmware requires different configuration parameters for Layer III audio.
    /// This makes the Layer a stream parameters (since the layer can only be changed
    /// by issuing a MME_CommandCode_t::MME_SET_GLOBAL_TRANSFORM_PARAMS command).
    unsigned int Layer;
} AacAudioStreamParameters_t;

#define BUFFER_AAC_AUDIO_STREAM_PARAMETERS        "AacAudioStreamParameters"
#define BUFFER_AAC_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_AAC_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AacAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to AAC audio.
///
/// \todo This is actually pretty generic stuff; if nothing radical emerges
///       in the other audio codecs perhaps this should be combined. Alternatively
///       just get rid of this type entirely and rename AacAudioParsedFrameHeader_t.
///
typedef struct AacAudioFrameParameters_s
{
    eAacFormatType Type;
    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} AacAudioFrameParameters_t;

#define BUFFER_AAC_AUDIO_FRAME_PARAMETERS        "AacAudioFrameParameters"
#define BUFFER_AAC_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_AAC_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(AacAudioFrameParameters_t)}

#endif /* H_AAC_AUDIO */
