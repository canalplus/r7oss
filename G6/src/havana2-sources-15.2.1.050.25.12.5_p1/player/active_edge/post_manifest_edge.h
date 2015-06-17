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
#ifndef H_POST_MANIFEST
#define H_POST_MANIFEST

#include "active_edge_base.h"

#undef TRACE_TAG
#define TRACE_TAG "PostManifestEdge_c"

class PostManifestEdge_c : public ActiveEdge_Base_c
{
public:
    PostManifestEdge_c(int ThreadDescId, PlayerStream_t Stream, int InputRingSize)
        : ActiveEdge_Base_c(ThreadDescId, Stream, InputRingSize)
        , mSequenceNumber(INVALID_SEQUENCE_VALUE)
        , mMaximumActualSequenceNumberSeen(0)
        , mAtLeastOneFrameToReTime(false)
        , mTime(INVALID_TIME)
        , mLastOutputTimeValid(false)
        , mReTimeQueuedFrames(false)
        , mReTimeStart(OS_GetTimeInMicroSeconds())
    {}

    virtual PlayerStatus_t   CallInSequence(PlayerSequenceType_t      SequenceType,
                                            PlayerSequenceValue_t     SequenceValue,
                                            PlayerComponentFunction_t Fn,
                                            ...);

    void DiscardUntilMarkerFrame()
    {
        mReTimeQueuedFrames = false;
        ActiveEdge_Base_c::DiscardUntilMarkerFrame();
    }

    void PerformReTiming()
    {
        mReTimeQueuedFrames = true;
        mReTimeStart        = OS_GetTimeInMicroSeconds();
    }

    bool IsReTiming() { return mReTimeQueuedFrames; }

private:
    unsigned long long        mSequenceNumber;
    unsigned long long        mMaximumActualSequenceNumberSeen;
    bool                      mAtLeastOneFrameToReTime;
    unsigned long long        mTime;
    bool                      mLastOutputTimeValid;
    bool                      mReTimeQueuedFrames;
    unsigned long long        mReTimeStart;

    virtual void MainLoop();
    virtual PlayerStatus_t  PerformInSequenceCall(PlayerControlStructure_t *ControlStructure);

    void SwitchComplete();

    void HandleMarkerFrame(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure);
    void HandleCodedFrameBuffer(Buffer_t Buffer, unsigned long long Now, PlayerSequenceNumber_t *SequenceNumberStructure, ParsedFrameParameters_t  *ParsedFrameParameters);
    void HandleCodedFrameBufferType(Buffer_t Buffer, unsigned long long Now);

    DISALLOW_COPY_AND_ASSIGN(PostManifestEdge_c);
};

#endif // H_POST_MANIFEST
