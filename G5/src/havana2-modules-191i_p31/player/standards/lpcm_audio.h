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

Source file name : lpcm_audio.h
Author :           Adam

Definition of the constants/macros that define useful things associated with 
LPCM audio streams.


Date        Modification                                    Name
----        ------------                                    --------
10-Jul-07   Created                                         Adam

************************************************************************/

#ifndef H_LPCM_AUDIO
#define H_LPCM_AUDIO

//#include "audio_buffer.h"
#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines
//

#define LPCM_AUDIO_START_CODE_MASK 0xF8
#define LPCM_AUDIO_START_CODE      0xA0

enum {
	LPCM_QUANTIZATION_16_BIT,
	LPCM_QUANTIZATION_20_BIT,
	LPCM_QUANTIZATION_24_BIT
};

enum {
	LPCM_SAMPLING_FREQ_48KHZ,
	LPCM_SAMPLING_FREQ_96KHZ
};

#define LPCM_BITS_PER_SAMPLE(x)         (16 + ((x)*4))
#define LPCM_SAMPLES_PER_FRAME(x)       (80 * ((x)+1))
#define LPCM_SAMPLING_FREQ(x)           (48 * ((x)+1))
#define LPCM_NUM_CHANNELS(x)            ((x)+1)



// ////////////////////////////////////////////////////////////////////////////
//
//  Definitions of types matching mpeg audio headers
//

///////////////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the first four bytes of the LPCM audio frame header.
///
typedef struct LpcmAudioParsedFrameHeader_s
{
    // Raw value
//    unsigned int Header; ///< Original header (big endian)

    // Directly interpretted values
//    unsigned char Layer; ///< MPEG audio layer. Either 1, 2 or 3.
//    unsigned char MpegStandard; ///< MPEG standard (sample rate grouping). Either 1, 2 or 25 (unofficial MPEG 2.5 standard) 
//    unsigned short BitRate; ///< MPEG bit rate in kbits/sec.
//    unsigned int SamplingFrequency; ///< Sampling frequency in Hz.
//    bool PaddedFrame; ///< True if the padding bit is set.

//    unsigned long long                    Pts;
//    unsigned int                          PtsValid;
    // see [1], Table 5.3.3-1
    unsigned int NumberOfFrameHeaders;
    unsigned int FirstAccessUnitPointer;
    bool         AudioEmphasisFlag;
    bool         AudioMuteFlag;
    bool         ReservedFlag0;
    unsigned int AudioFrameNumber;
    unsigned int QuantizationWordLength;
    unsigned int AudioSamplingFrequency;
    unsigned int NumberOfAudioChannels;
    unsigned int DynamicRangeControl;
	
    // Derived values
    unsigned int SamplingFrequency; ///< Sampling frequency in Hz.
    unsigned int NumberOfSamples; ///< Number of samples per channel within the frame.
    unsigned int Length; ///< Length of frame in bytes (including header).
} LpcmAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////


typedef struct LpcmAudioStreamParameters_s
{
//    unsigned int Unused;
    unsigned int NumberOfFrameHeaders;
    bool         AudioEmphasisFlag;
    bool         AudioMuteFlag;
    unsigned int QuantizationWordLength;
    unsigned int AudioSamplingFrequency;
    unsigned int NumberOfAudioChannels;
    unsigned int DynamicRangeControl;
} LpcmAudioStreamParameters_t;

#define BUFFER_LPCM_AUDIO_STREAM_PARAMETERS        "LpcmAudioStreamParameters"
#define BUFFER_LPCM_AUDIO_STREAM_PARAMETERS_TYPE   {BUFFER_LPCM_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(LpcmAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to LPCM audio.
///
/// \todo This is actually pretty generic stuff; if nothing radical emerges
///       in the other audio codecs perhaps this should be combined. Alternatively
///       just get rid of this type entirely and rename MpegAudioParsedFrameHeader_t.
///
typedef struct LpcmAudioFrameParameters_s
{
    /// The bit rate of the frame
    unsigned int BitRate;
    
    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} LpcmAudioFrameParameters_t;

#define BUFFER_LPCM_AUDIO_FRAME_PARAMETERS        "LpcmAudioFrameParameters"
#define BUFFER_LPCM_AUDIO_FRAME_PARAMETERS_TYPE   {BUFFER_LPCM_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(LpcmAudioFrameParameters_t)}

#endif /* H_LPCM_AUDIO */
