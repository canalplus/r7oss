/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

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
#ifndef H_PLAYER_STREAM_INTERFACE
#define H_PLAYER_STREAM_INTERFACE

#include "player_types.h"

class ActiveEdgeInterface_c;

class PlayerStreamInterface_c
{
public:

    virtual ~PlayerStreamInterface_c();

    virtual Player_t                    GetPlayer() = 0;
    virtual PlayerPlayback_t            GetPlayback() = 0;
    virtual BufferType_t                GetCodedFrameBufferType() = 0;
    virtual HavanaStream_t              GetHavanaStream() = 0;
    virtual DecodeBufferManager_t       GetDecodeBufferManager() = 0;
    virtual Collator_t                  GetCollator() = 0;
    virtual FrameParser_t               GetFrameParser() = 0;
    virtual Codec_t                     GetCodec() = 0;
    virtual ManifestationCoordinator_t  GetManifestationCoordinator() = 0;
    virtual OutputTimer_t               GetOutputTimer() = 0;
    virtual ES_Processor_c             *GetEsProcessor() = 0;
    virtual PlayerStreamType_t          GetStreamType() = 0;
    virtual unsigned int                GetInstanceId() = 0;

    virtual unsigned int                GetNumberOfDecodeBuffers() const = 0;

    virtual const PlayerStreamStatistics_t     &GetStatistics() = 0;
    virtual void                                ResetStatistics() = 0;
    virtual PlayerStreamStatistics_t           &Statistics() = 0;

    virtual const PlayerStreamAttributes_t     &GetAttributes() = 0;
    virtual void                                ResetAttributes() = 0;

    virtual void              SetSpeed(Rational_t Speed, PlayDirection_t Direction) = 0;

    virtual PlayerStatus_t              MarkUnPlayable() = 0;
    virtual bool                        IsUnPlayable() const = 0;

    virtual void                        Terminate() = 0;
    virtual bool                        IsTerminating() const = 0;

    virtual PlayerStatus_t              GetElementaryBufferLevel(
        stm_se_ctrl_play_stream_elementary_buffer_level_t *ElementaryBufferLevel) = 0;

    virtual PlayerStatus_t              EndOfStream() = 0;

    virtual PlayerStatus_t              InputJump(bool SurplusData, bool ContinuousReverseJump, bool FromDiscontinuityControl) = 0;

    virtual PlayerStatus_t              Drain(bool NonBlocking, bool SignalEvent, void *EventUserData,
                                              PlayerPolicy_t PlayoutPolicy, bool ParseAllFrames,
                                              unsigned long long *pDrainSequenceNumber) = 0;

    virtual PlayerStatus_t              Step() = 0;

    virtual PlayerStatus_t              Switch(Collator_t Collator, FrameParser_t FrameParser,
                                               Codec_t Codec, bool NonBlocking) = 0;

    virtual PlayerStatus_t              SetAlarm(stm_se_play_stream_alarm_t     alarm,
                                                 bool  enable, void *value) = 0;

    virtual PlayerStatus_t              ResetDiscardTrigger(void) = 0;
    virtual PlayerStatus_t              SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger) = 0;

    virtual BufferPool_t                GetCodedFrameBufferPool(unsigned int *MaximumCodedFrameSize = NULL) = 0;

    virtual PlayerStatus_t              LowPowerEnter() = 0;
    virtual PlayerStatus_t              LowPowerExit() = 0;
    virtual bool                        IsLowPowerState() = 0;
    virtual void                        SetLowPowerEnterEvent() = 0;
    virtual void                        WaitForLowPowerExitEvent() = 0;
    virtual PlayerStatus_t              SignalEvent(struct PlayerEventRecord_s *PlayerEvent) = 0;
    virtual ActiveEdgeInterface_c      *GetParseToDecodeEdge() = 0;

    // Virtual to allow mocking and consequently collator unit-testing.
    virtual CollateTimeFrameParserInterface_c *GetCollateTimeFrameParser() = 0;
};

#endif // H_PLAYER_STREAM_INTERFACE
