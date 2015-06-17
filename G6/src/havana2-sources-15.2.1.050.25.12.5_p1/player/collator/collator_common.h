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

#ifndef H_COLLATOR_COMMON
#define H_COLLATOR_COMMON

#include "player.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "collate_to_parse_edge.h"

#undef  TRACE_TAG
#define TRACE_TAG "Collator_Common_c"

class Collator_Common_c : public Collator_c
{
public:
    Collator_Common_c(void)
        : mOutputPort(NULL)
        , mCollatedFrameCounter(0)
        , ThrottlingWakeUpEvent()
        , mLimitHandlingLastPTS(INVALID_TIME)
        , mLimitHandlingJumpInEffect(false)
        , mLimitHandlingJumpAt(INVALID_TIME)
        , mIsDraining(false)
    {
        OS_InitializeEvent(&ThrottlingWakeUpEvent);
    }
    ~Collator_Common_c(void)
    {
        OS_TerminateEvent(&ThrottlingWakeUpEvent);
    }

    // from Collator_c
    virtual CollatorStatus_t   Input(PlayerInputDescriptor_t *Input,
                                     unsigned int             DataLength,
                                     void                    *Data,
                                     bool                     NonBlocking = false,
                                     unsigned int            *DataLengthRemaining = NULL);
protected:
    Port_c *mOutputPort;
    CollatorStatus_t InsertInOutputPort(Buffer_t Buffer);
    CollatorStatus_t InsertMarkerFrameToOutputPort(PlayerStream_t Stream, markerType_t markerType, void *Data);
    void StampFrame(PlayerSequenceNumber_t *SequenceNumberStructure);
    void   DelayForInjectionThrottling(CodedFrameParameters_t *CodedFrameParameters);
    void SetDrainingStatus(bool DrainingStatus);
    bool GetDrainingStatus(void)
    {
        return mIsDraining;
    }

private:
    unsigned long long mCollatedFrameCounter;

    OS_Event_t ThrottlingWakeUpEvent;

private:

    // Called at Input()-time.
    virtual CollatorStatus_t   DoInput(PlayerInputDescriptor_t *Input,
                                       unsigned int             DataLength,
                                       void                    *Data,
                                       bool                     NonBlocking,
                                       unsigned int            *DataLengthRemaining) = 0;

    // Throttling status variables
    unsigned long long        mLimitHandlingLastPTS;
    bool                      mLimitHandlingJumpInEffect;
    unsigned long long        mLimitHandlingJumpAt;
    bool                      mIsDraining;

    DISALLOW_COPY_AND_ASSIGN(Collator_Common_c);

};

#endif
