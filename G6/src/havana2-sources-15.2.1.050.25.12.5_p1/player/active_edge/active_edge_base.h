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

#ifndef H_ACTIVE_EDGE
#define H_ACTIVE_EDGE

#include "active_edge_interface.h"
#include "player_threads.h"

#include "ring_generic.h"

#define ACTIVE_EDGE_MAX_MESSAGES                 8

#undef TRACE_TAG
#define TRACE_TAG "ActiveEdge_Base_c"

class ActiveEdge_Base_c: public ActiveEdgeInterface_c
{
public:
    ActiveEdge_Base_c(int ThreadDescId, PlayerStream_t Stream, int InputRingSize)
        : ActiveEdgeInterface_c()
        , mInputRing(NULL)
        , mInputRingSize(InputRingSize)
        , mStream(Stream)
        , mDiscardingUntilMarkerFrame(false)
        , mThreadHandle(0)
        , mThreadDescId(ThreadDescId)
        , mAccumulatedBeforeControlMessagesCount(0)
        , mAccumulatedAfterControlMessagesCount(0)
        , mAccumulatedBeforeControlMessages()
        , mAccumulatedAfterControlMessages()
        , mMaximumActualSequenceNumberSeen(0)
    {
    }

    virtual ~ActiveEdge_Base_c()
    {
        if (mInputRing != NULL)
        {
            if (mInputRing->NonEmpty())
            {
                Stop();
            }
            delete mInputRing;
        }
    }

    virtual OS_Status_t Start()
    {
        mInputRing = new RingGeneric_c(mInputRingSize, player_tasks_desc[mThreadDescId].name);
        if (mInputRing == NULL || mInputRing->InitializationStatus != RingNoError) { return PlayerInsufficientMemory; }

        return OS_CreateThread(&mThreadHandle, StartTrampoline, this, &player_tasks_desc[mThreadDescId]);
    }

    virtual void Stop()
    {
        if (mInputRing)
        {
            mInputRing->Insert(0);
            while (mInputRing->NonEmpty())
            {
                Buffer_t Buffer = NULL;
                mInputRing->Extract((uintptr_t *)(&Buffer));
                if (Buffer != NULL)
                {
                    unsigned int OwnerIdentifier;
                    Buffer->GetOwnerList(1, &OwnerIdentifier);
                    Buffer->DecrementReferenceCount(OwnerIdentifier);
                }
            }
        }
    }

    virtual void DiscardUntilMarkerFrame() { SE_DEBUG(group_player, "\n"); mDiscardingUntilMarkerFrame = true; }
    bool isDiscardingUntilMarkerFrame() { return mDiscardingUntilMarkerFrame; }

    Port_c *GetInputPort() { return mInputRing; }
    RingStatus_t   Insert(uintptr_t      Value) { return mInputRing->Insert(Value); }

protected:
    Ring_t              mInputRing;
    int                 mInputRingSize;
    PlayerStream_t      mStream;
    bool                mDiscardingUntilMarkerFrame;

    void            HandlePlayerControlStructure(Buffer_t Buffer, unsigned long long SequenceNumber, unsigned long long Time, unsigned long long MaximumActualSequenceNumberSeen);
    void            ProcessAccumulatedBeforeControlMessages(unsigned long long SequenceNumber, unsigned long long Time);
    void            ProcessAccumulatedAfterControlMessages(unsigned long long SequenceNumber, unsigned long long Time);

private:
    OS_Thread_t                 mThreadHandle;
    int                         mThreadDescId;
    unsigned int                mAccumulatedBeforeControlMessagesCount;
    unsigned int                mAccumulatedAfterControlMessagesCount;
    PlayerBufferRecord_t        mAccumulatedBeforeControlMessages[ACTIVE_EDGE_MAX_MESSAGES];
    PlayerBufferRecord_t        mAccumulatedAfterControlMessages[ACTIVE_EDGE_MAX_MESSAGES];
    unsigned long long          mMaximumActualSequenceNumberSeen;

    static void *StartTrampoline(void *param)
    {
        ActiveEdge_Base_c *activeEdge = reinterpret_cast<ActiveEdge_Base_c *>(param);
        activeEdge->MainLoop();
        OS_TerminateThread();
        return NULL;
    }

    PlayerStatus_t   ProcessAccumulatedControlMessages(
        unsigned int *MessageCount, unsigned int MessageTableSize,
        PlayerBufferRecord_t *MessageTable, unsigned long long SequenceNumber,
        unsigned long long Time);

    PlayerStatus_t   ProcessControlMessage(Buffer_t Buffer, PlayerControlStructure_t *Message);

    PlayerStatus_t   AccumulateControlMessage(Buffer_t            Buffer,
                                              PlayerControlStructure_t     *Message,
                                              unsigned int             *MessageCount,
                                              unsigned int              MessageTableSize,
                                              PlayerBufferRecord_t         *MessageTable);



    virtual void MainLoop() = 0;
    virtual PlayerStatus_t  PerformInSequenceCall(PlayerControlStructure_t *ControlStructure) = 0;

    DISALLOW_COPY_AND_ASSIGN(ActiveEdge_Base_c);
};

#endif // H_THREAD
