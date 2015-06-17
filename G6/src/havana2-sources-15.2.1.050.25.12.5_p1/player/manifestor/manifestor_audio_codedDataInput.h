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

#ifndef MANIFESTOR_AUDIO_CODED_DATA_INPUT_H
#define MANIFESTOR_AUDIO_CODED_DATA_INPUT_H

#include "manifestor_audio_ksound.h"
#include "pcmplayer.h"

class Manifestor_AudioKsound_c;

#undef TRACE_TAG
#define TRACE_TAG   "CodedDataInput_c"

// When the repetition frame is of fixed duration whatever the number of samples in the compressed frame.
// Set the oversampling to SCALABLE_BYPASS_OVERSAMPLING.
const uint32_t SCALABLE_BYPASS_OVERSAMPLING(0xFFFF);

////////////////////////////////////////////////////////////////////////////
///
/// Helper for coded data input management in manifestor sub-system.
///

class CodedDataInput_c
{
public:
    enum BypassChannelIdx
    {
        BYPASS_SPDIF_IDX = 0,
        BYPASS_HDMI_IDX = 1,
        BYPASS_CHANNEL_MAX_NUM
    };

    inline CodedDataInput_c(void)
        : BypassChannel(-1)
        , SamplesUntilNextRepetitionPeriod(0)
        , FirstBufferPartiallyConsumed(false)
        , Playing(false)
        , Encoding(PcmPlayer_c::OUTPUT_IEC60958)
        , RepetitionPeriod(1)
        , BypassOversampling(1)
        , FrameSampleCount(1)
        , PeriodChange(false)
    {
        SE_DEBUG(group_manifestor_audio_ksound, ">: %p\n", this);
    }

    inline ~CodedDataInput_c(void)
    {
        SE_DEBUG(group_manifestor_audio_ksound, ">: %p\n", this);
    }

    inline int GetBypassChannel() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Returning %d\n",
                 this,
                 BypassChannel);
        return BypassChannel;
    }

    inline bool IsBypassPossible() const
    {
        bool Result(false);
        Result = (PcmPlayer_c::BYPASS_LOWEST <= Encoding) && (Encoding < PcmPlayer_c::BYPASS_HIGHEST);
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline void SetPlaying(bool Playing)
    {
        SE_DEBUG(group_manifestor_audio_ksound, ">: %p Playing (%s) -> %s\n",
                 this,
                 (CodedDataInput_c::Playing ? "true" : "false"),
                 (Playing ? "true" : "false"));
        CodedDataInput_c::Playing = Playing;
    }

    inline bool IsPlaying() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, Playing ? "true" : "false");
        return Playing;
    }

    inline void SetEncoding(PcmPlayer_c::OutputEncoding Encoding)
    {
        SE_DEBUG(group_manifestor_audio_ksound, ">: %p Encoding (%s) -> %s\n",
                 this,
                 PcmPlayer_c::LookupOutputEncoding(CodedDataInput_c::Encoding),
                 PcmPlayer_c::LookupOutputEncoding(Encoding));
        CodedDataInput_c::Encoding = Encoding;
    }

    inline PcmPlayer_c::OutputEncoding GetEncoding() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Returning %s\n",
                 this,
                 PcmPlayer_c::LookupOutputEncoding(Encoding));
        return Encoding;
    }

    inline void SetFirstBufferPartiallyConsumed(bool FirstBufferPartiallyConsumed)
    {
        SE_DEBUG(group_manifestor_audio_ksound, ">: %p %s -> %s\n",
                 this,
                 (CodedDataInput_c::FirstBufferPartiallyConsumed ? "true" : "false"),
                 (FirstBufferPartiallyConsumed ? "true" : "false"));
        CodedDataInput_c::FirstBufferPartiallyConsumed = FirstBufferPartiallyConsumed;
    }

    inline bool IsFirstBufferPartiallyConsumed() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, FirstBufferPartiallyConsumed ? "true" : "false");
        return FirstBufferPartiallyConsumed;
    }

    inline void SetSamplesUntilNextRepetitionPeriod(uint32_t SamplesUntilNextRepetitionPeriod)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p (%d) -> %d\n",
                 this,
                 CodedDataInput_c::SamplesUntilNextRepetitionPeriod,
                 SamplesUntilNextRepetitionPeriod);
        CodedDataInput_c::SamplesUntilNextRepetitionPeriod = SamplesUntilNextRepetitionPeriod;
    }

    inline uint32_t GetSamplesUntilNextRepetitionPeriod() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Returning %d\n",
                 this,
                 SamplesUntilNextRepetitionPeriod);
        return SamplesUntilNextRepetitionPeriod;
    }

    inline void SetRepetitionPeriod(uint32_t CurrentRepetitionPeriod)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p (%d) -> %d\n",
                 this,
                 CodedDataInput_c::RepetitionPeriod,
                 CurrentRepetitionPeriod);
        CodedDataInput_c::RepetitionPeriod = CurrentRepetitionPeriod;
    }

    inline uint32_t GetRepetitionPeriod() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Returning %d\n",
                 this,
                 RepetitionPeriod);
        return RepetitionPeriod;
    }

    inline void SetFrameSampleCount(uint32_t FrameSampleCount)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p FrameSampleCount (%d) -> %d\n",
                 this,
                 CodedDataInput_c::FrameSampleCount,
                 FrameSampleCount);
        CodedDataInput_c::FrameSampleCount = FrameSampleCount;
    }

    inline void SetBypassOversampling(uint32_t BypassOversampling)
    {
        CodedDataInput_c::BypassOversampling = BypassOversampling;
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p BypassOversampling -> %d\n",
                 this,
                 CodedDataInput_c::BypassOversampling);
    }

    inline uint32_t GetBypassOversampling() const
    {
        uint32_t RatioCompressedVsPcm(BypassOversampling);

        if ((RatioCompressedVsPcm == SCALABLE_BYPASS_OVERSAMPLING) && FrameSampleCount)
        {
            RatioCompressedVsPcm = RepetitionPeriod / FrameSampleCount;
        }

        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Returning %d\n",
                 this,
                 RatioCompressedVsPcm);
        return RatioCompressedVsPcm;
    }

    inline void SetPeriodChange(bool PeriodChange)
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p (%s) -> %s\n",
                 this,
                 (true == CodedDataInput_c::PeriodChange) ? "true" : "false",
                 (true == PeriodChange) ? "true" : "false");
        CodedDataInput_c::PeriodChange = PeriodChange;
    }

    inline bool IsPeriodChange() const
    {
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, PeriodChange ? "true" : "false");
        return PeriodChange;
    }

    inline bool IsBypassRatioVariable() const
    {
        bool Result(BypassOversampling == SCALABLE_BYPASS_OVERSAMPLING);
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline bool IsCompatibleEncodingChange(PcmPlayer_c::OutputEncoding CurrentEncoding)
    {
        bool Result(false);

        // Only Allow DD <-> DD+ and SPDIFIN_PCM <-> SPDIFIN_COMPRESSED for now
        // Others if required can be added later
        if (((CurrentEncoding == PcmPlayer_c::BYPASS_AC3) &&
             (Encoding        == PcmPlayer_c::BYPASS_DDPLUS)) ||
            ((CurrentEncoding == PcmPlayer_c::BYPASS_DDPLUS) &&
             (Encoding        == PcmPlayer_c::BYPASS_AC3)) ||
            ((CurrentEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED) &&
             (Encoding        == PcmPlayer_c::BYPASS_SPDIFIN_PCM)) ||
            ((CurrentEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM) &&
             (Encoding        == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED)))
        {
            Result = true;
        }

        SE_DEBUG(group_manifestor_audio_ksound, "<: %p returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    /// To be called in case bypass is ending (stop, flush ...).
    inline void Reset(int BypassChannel)
    {
        CodedDataInput_c::BypassChannel = BypassChannel;
        SamplesUntilNextRepetitionPeriod = 0;
        FirstBufferPartiallyConsumed = false;
        Playing = false;
        SetEncoding(PcmPlayer_c::OUTPUT_IEC60958);
        RepetitionPeriod = 1;
        BypassOversampling = 1;
        FrameSampleCount = 1;
        PeriodChange = false;
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p Encoding -> %s\n",
                 this,
                 PcmPlayer_c::LookupOutputEncoding(Encoding));
    }

    inline void DebugDump()
    {
        if (SE_IS_DEBUG_ON(group_manifestor_audio_ksound) == 0) { return; }

        SE_DEBUG(group_manifestor_audio_ksound, ">: %p BypassChannel                   :%d\n", this, BypassChannel);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p SamplesUntilNextRepetitionPeriod:%d\n", this, SamplesUntilNextRepetitionPeriod);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p FirstBufferPartiallyConsumed    :%d\n", this, FirstBufferPartiallyConsumed);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p Playing                         :%s\n", this, (true == Playing) ? "true" : "false");
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p Encoding                        :%s\n", this, PcmPlayer_c::LookupOutputEncoding(Encoding));
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p RepetitionPeriod                :%d\n", this, RepetitionPeriod);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p BypassOversampling              :%d\n", this, BypassOversampling);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p FrameSampleCount                :%d\n", this, FrameSampleCount);
        SE_DEBUG(group_manifestor_audio_ksound, "@: %p PeriodChange                    :%s\n", this, (true == PeriodChange) ? "true" : "false");
        SE_DEBUG(group_manifestor_audio_ksound, "<: %p IsBypassPossible()              :%s\n", this, ((PcmPlayer_c::BYPASS_LOWEST <= Encoding) && (Encoding < PcmPlayer_c::BYPASS_HIGHEST)) ? "true" : "false");
    }

private:
    int BypassChannel; ///< The kind of bypass for this coded input.
    uint32_t SamplesUntilNextRepetitionPeriod;
    bool FirstBufferPartiallyConsumed;
    bool Playing; ///< Are coded data actually mixed, based on audio processor status.
    PcmPlayer_c::OutputEncoding Encoding; ///< The encoding for this coded input.
    uint32_t RepetitionPeriod;
    uint32_t BypassOversampling;
    uint32_t FrameSampleCount;
    bool PeriodChange;
};

#endif // MANIFESTOR_AUDIO_CODED_DATA_INPUT_H
