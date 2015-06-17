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

Source file name : mlp.h
Author :           Sylvain

Definition of the types and constants that are used by several components
dealing with lpcm audio decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
01-Oct-07   Creation                                        Sylvain

************************************************************************/

#ifndef H_MLP_
#define H_MLP_

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

#define MLP_FORMAT_SYNC_A               0xF8726FBA
#define MLP_FORMAT_SYNC_B               0xF8726FBB

#define MLP_MINOR_SYNC_CRC              0xF

#define MLP_SIGNATURE                   0xB752U

#define MLP_DVD_AUDIO_NO_CH_GR2         0x0F

#define MLP_CODEC_DIALREF_DEFAULT_VALUE 31
#define MLP_CODEC_LDR_DEFAULT_VALUE     0x64
#define MLP_CODEC_HDR_DEFAULT_VALUE     0x64
#define MLP_MAX_ACCESS_UNIT_SIZE        1536
#define MLP_PRIVATE_DATA_AREA_SIZE      10

#define MLP_STREAM_ID_EXTENSION_MLP     0x72
#define MLP_STREAM_ID_EXTENSION_AC3     0x76

////////////////////////////////////////////////////////////////
///
/// MLP audio stream type
///
///
typedef enum
{
  MlpWordSize16,       ///< sample is 16-bit long
  MlpWordSize20,       ///< sample is 20-bit long
  MlpWordSize24,       ///< sample is 24-bit long
  MlpWordSizeNone      ///< undefined sample size
} MlpWordSize_t;

typedef enum
{
  MlpSamplingFreq48,       ///< 48 kHz 
  MlpSamplingFreq96,       ///< 96 kHz
  MlpSamplingFreq192,      ///< 192 kHz
  MlpSamplingFreq44p1 = 8, ///< 44.1 kHz
  MlpSamplingFreq88p2,     ///< 88.2 kHz
  MlpSamplingFreq176p4,    ///< 176.4 kHz
  MlpSamplingFreqNone      ///< undefined sampling frequncy
} MlpSamplingFreq_t;

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the MLP audio frame header.
///
typedef struct MlpAudioParsedFrameHeader_s
{
  // These fields contain all these required by the audio mme codec
  bool                IsMajorSync;              ///< if true, this frame holds a major sync
  MlpSamplingFreq_t   SamplingFrequency;        ///< Sampling frequency identifier.
  unsigned int        NumberOfSamples;          ///< Number of samples per channel within the frame.
  unsigned int        Length;                   ///< Length of frame in bytes (including header).
  unsigned int        AudioFrameNumber;         ///< Number of audio frames accumulated
} MlpAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct MlpAudioStreamParameters_s
{
    unsigned int AccumulatedFrameNumber;
} MlpAudioStreamParameters_t;

#define BUFFER_MLP_AUDIO_STREAM_PARAMETERS        "MlpAudioStreamParameters"
#define BUFFER_MLP_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_MLP_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MlpAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to MLP audio.
///
typedef struct MlpAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int Unsused;
} MlpAudioFrameParameters_t;

#define BUFFER_MLP_AUDIO_FRAME_PARAMETERS        "MlpAudioFrameParameters"
#define BUFFER_MLP_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_MLP_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(MlpAudioFrameParameters_t)}

#endif /* H_MLP_ */
