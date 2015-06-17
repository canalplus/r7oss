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

#ifndef H_FRAME_PARSER_AUDIO
#define H_FRAME_PARSER_AUDIO

#include "frame_parser_base.h"

#undef TRACE_TAG
#define TRACE_TAG "FrameParser_Audio_c"

/// Framework to unify the approach to audio frame parsing
class FrameParser_Audio_c : public FrameParser_Base_c
{
public:
    FrameParser_Audio_c(void);
    ~FrameParser_Audio_c(void);

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   Connect(Port_c *Port);

    FrameParserStatus_t   Input(Buffer_t          CodedBuffer);

    // Retun 10 : minimum estimated amount of audio frame references
    // This value could be better estimated after stream parsing
    virtual unsigned int GetMaxReferenceCount(void) const { return 10; };

    //
    // Common portion of read headers
    //

    virtual FrameParserStatus_t   ReadHeaders(void);

protected:
    ParsedAudioParameters_t  *ParsedAudioParameters;

    /// The maximum value by which the actual PTS is permitted to differ from the synthesised PTS
    /// (use to identify bad streams or poor predictions).
    int                       PtsJitterTollerenceThreshold;

    /// Time at which that previous frame was to be presented.
    /// This is primarily used for debugging PTS sequencing problems (e.g. calculating deltas between
    /// 'then' and 'now'.
    unsigned long long        LastNormalizedPlaybackTime;
    /// Expected time at which the following frame must be presented.
    unsigned long long        NextFrameNormalizedPlaybackTime;
    /// Error that is accumulated by calculating the PTS based on its last recorded value.
    unsigned long long        NextFramePlaybackTimeAccumulatedError;

    bool UpdateStreamParameters; ///< True if we need to update the stream parameters on the next frame.

    // Functions

    FrameParserStatus_t HandleCurrentFrameNormalizedPlaybackTime();
    void HandleUpdateStreamParameters();
    void GenerateNextFrameNormalizedPlaybackTime(unsigned int SampleCount, unsigned SamplingFrequency);

private:
    unsigned int RelayfsIndex;

    void HandleInvalidPlaybackTime();

    DISALLOW_COPY_AND_ASSIGN(FrameParser_Audio_c);
};

#endif
