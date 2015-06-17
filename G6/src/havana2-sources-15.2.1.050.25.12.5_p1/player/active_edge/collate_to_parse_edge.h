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
#ifndef H_COLLATE_TO_PARSE
#define H_COLLATE_TO_PARSE

#include "active_edge_base.h"

class CollateToParseEdge_c : public ActiveEdge_Base_c
{
public:
    CollateToParseEdge_c(int ThreadDescId, PlayerStream_t Stream, int InputRingSize, FrameParser_t FrameParser)
        : ActiveEdge_Base_c(ThreadDescId, Stream, InputRingSize)
        , mSequenceNumber(INVALID_SEQUENCE_VALUE)
        , mNextBufferSequenceNumber(0)
        , mMaximumActualSequenceNumberSeen(0)
        , mFrameParser(FrameParser)
    {}

    virtual PlayerStatus_t   CallInSequence(PlayerSequenceType_t      SequenceType,
                                            PlayerSequenceValue_t     SequenceValue,
                                            PlayerComponentFunction_t Fn,
                                            ...);

    unsigned long long GetMarkerFrameSequenceNumber() { return mNextBufferSequenceNumber + PLAYER_MAX_RING_SIZE; }

    FrameParser_t GetFrameParser() { return mFrameParser; }

private:
    unsigned long long        mSequenceNumber;
    unsigned long long        mNextBufferSequenceNumber;
    unsigned long long        mMaximumActualSequenceNumberSeen;
    FrameParser_t             mFrameParser;

    virtual void MainLoop();
    virtual PlayerStatus_t  PerformInSequenceCall(PlayerControlStructure_t *ControlStructure);

    void SwitchFrameParser();

    void HandleMarkerFrame(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure);
    void HandleCodedFrameBuffer(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure);
    void HandleCodedFrameBufferType(Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(CollateToParseEdge_c);
};

#endif // H_COLLATE_TO_PARSE
