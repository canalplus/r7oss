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

#ifndef H_FRAME_PARSER_AUDIO_WMA
#define H_FRAME_PARSER_AUDIO_WMA

#include "wma_audio.h"
#include "frame_parser_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_AudioWma_c"

class FrameParser_AudioWma_c : public FrameParser_Audio_c
{
public:
    FrameParser_AudioWma_c(void);
    ~FrameParser_AudioWma_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    //
    // Stream specific functions
    //

    static FrameParserStatus_t ParseStreamHeader(unsigned char                  *FrameHeaderBytes,
                                                 WmaAudioStreamParameters_t     *StreamParameters,
                                                 bool                            Verbose = true);

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

private:
    /*WmaAudioParsedFrameHeader_t ParsedFrameHeader;*/
    WmaAudioStreamParameters_t         *StreamParameters;
    WmaAudioFrameParameters_t          *FrameParameters;

    WmaAudioStreamParameters_t          CurrentStreamParameters;

    unsigned long long                  LastNormalizedDecodeTime;

    DISALLOW_COPY_AND_ASSIGN(FrameParser_AudioWma_c);
};

#endif /* H_FRAME_PARSER_AUDIO_WMA */

