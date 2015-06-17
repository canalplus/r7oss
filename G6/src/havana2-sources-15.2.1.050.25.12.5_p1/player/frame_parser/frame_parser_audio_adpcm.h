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

#ifndef H_FRAME_PARSER_AUDIO_ADPCM
#define H_FRAME_PARSER_AUDIO_ADPCM

#include "adpcm.h"
#include "frame_parser_audio.h"
#include "codec_mme_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioAdpcm_c"

class FrameParser_AudioAdpcm_c : public FrameParser_Audio_c
{
public:
    explicit FrameParser_AudioAdpcm_c(unsigned int DecodeLatencyInSamples = 0);
    ~FrameParser_AudioAdpcm_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    FrameParserStatus_t   ReadHeaders(void);
    FrameParserStatus_t   ResetReferenceFrameList(void);
    FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   PrepareReferenceFrameList(void);
    FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(void);
    FrameParserStatus_t   GeneratePostDecodeParameterSettings(void);
    FrameParserStatus_t   UpdateReferenceFrameList(void);

    FrameParserStatus_t   ProcessReverseDecodeUnsatisfiedReferenceStack(void);
    FrameParserStatus_t   ProcessReverseDecodeStack(void);
    FrameParserStatus_t   PurgeReverseDecodeUnsatisfiedReferenceStack(void);
    FrameParserStatus_t   PurgeReverseDecodeStack(void);
    FrameParserStatus_t   TestForTrickModeFrameDrop(void);

    static FrameParserStatus_t  ParseFrameHeader(unsigned char *FrameHeader,
                                                 AdpcmAudioParsedFrameHeader_t *ParsedFrameHeader,
                                                 int RemainingElementaryLength);


private:
    AdpcmAudioStreamParameters_t *StreamParameters;
    AdpcmAudioStreamParameters_t  CurrentStreamParameters;
    AdpcmAudioFrameParameters_t  *FrameParameters;

    /// The ADPCM type (IMA, MS, etc).
    AdpcmStreamType_t StreamType;

    /// Number of samples of intrinsic latency in the decoder.
    /// The output timer assumes that the decode latency any decode is smaller than the length of time that the
    /// frame will be displayed for. Decoders such as SPDIF have additional latency that the output time must be made
    /// aware of in the decode time stamp. This value contains that latency.
    unsigned int IndirectDecodeLatencyInSamples;

    unsigned char LastPacket;

    DISALLOW_COPY_AND_ASSIGN(FrameParser_AudioAdpcm_c);
};

#endif /* H_FRAME_PARSER_AUDIO_LPCM */

