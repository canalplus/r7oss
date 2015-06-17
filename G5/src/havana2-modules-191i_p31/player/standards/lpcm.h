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

Source file name : lpcm.h
Author :           Sylvain

Definition of the types and constants that are used by several components
dealing with lpcm audio decode/display for havana.


Date        Modification                                    Name
----        ------------                                    --------
24-Apr-06   Created                                         Mark
06-June-07  Ported to player2                               Sylvain

************************************************************************/

#ifndef H_LPCM_
#define H_LPCM_

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//
#define LPCM_FRAME_HEADER_SIZE                 0

// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching AC3 audio headers
//

#define LPCM_DVD_VIDEO_SUBSTREAM_ID        ((unsigned char)0xA0)
#define LPCM_DVD_VIDEO_SUBSTREAM_ID_MASK   ((unsigned char)0xF8)
#define LPCM_DVD_AUDIO_SUBSTREAM_ID        ((unsigned char)0xA0)
#define LPCM_DVD_AUDIO_SUBSTREAM_ID_MASK   ((unsigned char)0xFF)
#define LPCM_HD_DVD_SUBSTREAM_ID           ((unsigned char)0xA0)
#define LPCM_HD_DVD_SUBSTREAM_ID_MASK      ((unsigned char)0xF8)

#define LPCM_DVD_AUDIO_NO_CH_GR2           0x0F


#define LPCM_DVD_AUDIO_PRIVATE_HEADER_LENGTH      0x4
#define LPCM_DVD_AUDIO_MIN_PRIVATE_HEADER_LENGTH   13
#define LPCM_DVD_VIDEO_PRIVATE_HEADER_LENGTH      0x7
#define LPCM_HD_DVD_PRIVATE_HEADER_LENGTH         0x9
#define LPCM_BD_PRIVATE_HEADER_LENGTH             0x4

#define LPCM_MAX_PRIVATE_HEADER_LENGTH             20

#define LPCM_DVD_WS_20                        0x1

#define LPCM_AUDIO_BASE_FRAME_SIZE            40

#define LPCM_DRC_VALUE_DISABLE                0x80


#define LPCM_DVD_VIDEO_FIRST_ACCES_UNIT       0x3
#define LPCM_DVD_AUDIO_FIRST_ACCES_UNIT       0x5
#define LPCM_HD_DVD_FIRST_ACCES_UNIT          0x3
#define LPCM_BD_FIRST_ACCES_UNIT              0x3

#define LPCM_DVD_VIDEO_NB_FRAME_HEADER_OFFSET 0x1

#define NUMBER_OF_AUDIO_FRAMES_TO_GLOB        20

////////////////////////////////////////////////////////////////
///
/// LPCM audio stream type
///
///
typedef enum
{
  TypeLpcmDVDVideo,       ///< frame is a DVD video lpcm
  TypeLpcmDVDAudio,       ///< frame is a DVD audio lpcm
  TypeLpcmDVDHD,          ///< frame is a DVD HD lpcm
  TypeLpcmDVDBD,          ///< frame is a DVD BD lpcm
  TypeLpcmSPDIFIN,        ///< frame is a SPDIFIN frame.
  TypeLpcmInvalid = 16    ///< invalid frame type.
} LpcmStreamType_t;

typedef enum
{
  LpcmWordSize16,                   ///< sample is 16-bit long
  LpcmWordSize20,                   ///< sample is 20-bit long
  LpcmWordSize24,                   ///< sample is 24-bit long
  LpcmWordSize32,                   ///< sample is 32-bit long (only for SPDIFIN or Wav file standards)
  LpcmWordSizeNone = LpcmWordSize32 ///< undefined sample size (for (HD)DVD standards)
} LpcmWordSize_t;

typedef enum
{
  /// Used by DVD-audio secondary streams (means no CH_GR2). This value is larger than LpcmSamplingFreqLast.
  /// This is safe only because it is never used to index lookup tables.
  LpcmSamplingFreqNotSpecififed = 15,
  
  // Common Lpcm formats frequencies
  LpcmSamplingFreq48 = 0,   ///< 48 kHz 
  LpcmSamplingFreq96,       ///< 96 kHz
  LpcmSamplingFreq192,      ///< 192 kHz

  // Spdifin additional frequencies :
  LpcmSamplingFreq32   = 4, ///< 32    kHz
  LpcmSamplingFreq16      , ///< 16    kHz
  LpcmSamplingFreq22p05,    ///< 22.05 kHz
  LpcmSamplingFreq24,       ///< 24    kHz

  
  // DvdAudio additional frequencies :
  LpcmSamplingFreq44p1 = 8, ///< 44.1 kHz
  LpcmSamplingFreq88p2,     ///< 88.2 kHz
  LpcmSamplingFreq176p4,    ///< 176.4 kHz
  LpcmSamplingFreqNone,     ///< undefined sampling frequncy

  // Do not edit beyond this point
  LpcmSamplingFreqLast
  
} LpcmSamplingFreq_t;

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the LPCM audio frame header.
///
typedef struct LpcmAudioParsedFrameHeader_s
{
  // These fields contain all these required by the audio mme codec
  LpcmStreamType_t    Type;
  bool                MuteFlag;                 ///< Mute to be activated
  bool                EmphasisFlag;             ///< Empahsis to be activated
  LpcmWordSize_t      WordSize1;                ///< Sample Size.
  LpcmWordSize_t      WordSize2;                ///< Sample Size for channel group 2 (DVD audio specific).
  unsigned char       SamplingFrequency1;       ///< Sampling frequency in Hz.
  unsigned char       SamplingFrequency2;       ///< Sampling frequency in Hz for channel group 2 (DVD audio specific).

  unsigned char       NumberOfChannels;         ///< Number of channels within the frame.
  unsigned char       BitShiftChannel2;         ///< BitShift for group channel 2 (DVD audio specific)
  unsigned char       ChannelAssignment;        ///< Channel Assignement
  unsigned char       NbAccessUnits;            ///< Number of audio frames in this packet.
  unsigned char       DrcCode;                  ///< Drc Coefficient.
  unsigned int        NumberOfSamples;          ///< Number of samples per channel within the frame.
  //
  unsigned int        Length;                   ///< Length of frame in bytes (including header).
  unsigned int        FirstAccessUnitPointer;   ///< Pointer to the first access unit.
  unsigned int        PrivateHeaderLength;      ///< Length of private data area
  unsigned int        AudioFrameNumber;         ///< Identifier of the first audio frame in this packet
  unsigned char       SubStreamId;              ///< Substream identifier for this pes
  unsigned int        AudioFrameSize;           ///< Size of one access unit
} LpcmAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

#define LpcmAudioStreamParameters_t LpcmAudioParsedFrameHeader_t

#define BUFFER_LPCM_AUDIO_STREAM_PARAMETERS        "LpcmAudioStreamParameters"
#define BUFFER_LPCM_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_LPCM_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(LpcmAudioStreamParameters_t)}

//

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to LPCM audio.
/// We already know from the collator all the codec parameters from the collator, 
/// pass them from the collator to the codec
///
typedef struct LpcmAudioFrameParameters_s
{
  unsigned char       DrcCode;                  ///< Drc Coefficient.
  unsigned int        NumberOfSamples;          ///< Number of samples per channel within the frame.
} LpcmAudioFrameParameters_t;

#define BUFFER_LPCM_AUDIO_FRAME_PARAMETERS        "LpcmAudioFrameParameters"
#define BUFFER_LPCM_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_LPCM_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(LpcmAudioFrameParameters_t)}

#endif /* H_LPCM_ */
