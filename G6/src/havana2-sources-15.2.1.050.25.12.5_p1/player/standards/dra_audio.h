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

#ifndef H_DRA_AUDIO
#define H_DRA_AUDIO

#include "pes.h"

// ////////////////////////////////////////////////////////////////////////////
//
//  General defines of start codes etc.
//

/* Incoming information header values */
#define DRA_AUDIO_FRAME_HEADER_SIZE     4
#define DRA_START_SYNC_WORD             0x7FFF
#define DRA_ES_FRAME_HEADER_GEN         0x0
#define DRA_ES_FRAME_HEADER_EXT         0x1
#define DRA_START_SYNC_CODE_BYTE0       0x7f
#define DRA_START_SYNC_CODE_BYTE0_MASK  0x7f
#define DRA_START_SYNC_CODE_BYTE1       0xff

//header bits used                        BITS
#define DRA_START_SYNC                    16
#define DRA_HEADER_FRAME_TYPE_MASK        1
#define DRA_GEN_HDR_AUDIO_FRAME_LEN       10
#define DRA_EXT_HDR_AUDIO_FRAME_LEN       13
#define DRA_NO_OF_BLOCKS_PER_FRAME        2
#define DRA_SAMPLE_RATE_INDEX             4
#define DRA_NUM_OF_CHANNEL_FOR_GEN_HDR    3
#define DRA_NUM_OF_CHANNEL_FOR_EXT_HDR    6
#define DRA_NUM_OF_LFE_FOR_GEN_HDR        1
#define DRA_NUM_OF_LFE_FOR_EXT_HDR        2
#define DRA_AUX_DATA_INFO                 1
#define DRA_SUM_DIFF_CODING_FOR_GEN_HDR   1
#define DRA_JOINT_INTS_CODING_FOR_GEN_HDR 1
#define DRA_MAX_DECODED_SAMPLE_COUNT      1024

////////////////////////////////////////////////////////////////////
///
/// Exploded copy of the DRA audio frame header.
///
typedef struct DraAudioParsedFrameHeader_s
{
    unsigned int   Header;          // DRA sync word
    unsigned char  HeaderType;      // General Frame Header or Extension Frame Header
    unsigned short Audio_FrameSize; // 10 bits for gen frame header and 13 bit for Ext frame haeder
    unsigned char  MDCT_BLKPerFrame;
    unsigned int   PCMSamplePerFrame;
    unsigned char  SampleRateIndex;
    unsigned short Num_NormalChannel;
    unsigned char  Num_LfeChannel;
    unsigned int   SamplingFrequency;
    unsigned char  AuxDataInfoPresent;
    unsigned char  SumDiffUsedinFrame;
    unsigned char  JointIntensityCodPresent;
} DraAudioParsedFrameHeader_t;

////////////////////////////////////////////////////////////////

typedef struct DraAudioStreamParameters_s
{
    unsigned int Frequency;
} DraAudioStreamParameters_t;

#define BUFFER_DRA_AUDIO_STREAM_PARAMETERS           "DraAudioStreamParameters"
#define BUFFER_DRA_AUDIO_STREAM_PARAMETERS_TYPE      {BUFFER_DRA_AUDIO_STREAM_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(DraAudioStreamParameters_t)}

////////////////////////////////////////////////////////////////
///
/// Meta-data unique to DRA audio.
///
typedef struct DraAudioFrameParameters_s
{
    /// Size of the compressed frame (in bytes)
    unsigned int FrameSize;
} DraAudioFrameParameters_t;

#define BUFFER_DRA_AUDIO_FRAME_PARAMETERS            "DraAudioFrameParameters"
#define BUFFER_DRA_AUDIO_FRAME_PARAMETERS_TYPE       {BUFFER_DRA_AUDIO_FRAME_PARAMETERS, BufferDataTypeBase, AllocateFromOSMemory, 4, 0, true, true, sizeof(DraAudioFrameParameters_t)}


#endif /* H_DRA_AUDIO_ */

