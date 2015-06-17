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

#ifndef H_ENCODE_COORDINATOR
#define H_ENCODE_COORDINATOR

#include "player_types.h"
#include "port.h"
#include "ring_generic.h"
#include "buffer.h"

#include "encode_stream.h"
#include "release_buffer_interface.h"
#include "encode_coordinator_interface.h"
#include "encode_coordinator_stream.h"

#define MAX_ENCODE_COORDINATOR_STREAM_NB    5   // Max number of streams that can be handled by encode coordinator
#define ENCODE_COORDINATOR_MAX_EVENT_WAIT   500 // Max wait for thread creation/termination (in milliseconds)
#define NRT_INACTIVE_SIGNAL_TIMEOUT         10000000 // Large timeout value used to detect not active streams


typedef enum EncodeCoordinatorState_e
{
    StartingStateStep0,
    StartingStateStep1,
    RunningState
} EncodeCoordinatorState_t;

class EncodeCoordinator_c : public EncodeCoordinatorInterface_c
{
public:
    EncodeCoordinator_c(void);
    virtual EncoderStatus_t FinalizeInit(void);
    virtual void            Halt(void);
    virtual ~EncodeCoordinator_c(void);

    virtual EncoderStatus_t Connect(EncodeStreamInterface_c  *EncodeStream,
                                    ReleaseBufferInterface_c *OriginalReleaseBufferItf,
                                    ReleaseBufferInterface_c **EncodeCoordinatorStreamReleaseBufferItf,
                                    Port_c **InputPort);


    virtual EncoderStatus_t Disconnect(EncodeStreamInterface_c *EncodeStream);
    virtual EncoderStatus_t Flush(EncodeStreamInterface_c *EncodeStream);
    virtual void SignalNewStreamInput() { OS_SetEvent(&mInputStreamEvent); }
    virtual int64_t GetPtsOffset()      { return mPtsOffset; }

    void  ProcessEncodeCoordinator(void);

private:
    bool StreamsHaveGap(uint64_t *GapEndTime);
    bool StreamsHaveData(void);
    bool StreamsUpdateTimeCompression(uint64_t EndOfGapTime);
    bool StreamsReady(void);
    void StreamsEncode(void);
    bool StreamsGetNextCurrentTime(uint64_t *nextCurrentTime);


    void  StartingStateStep0Processing(void);
    void  StartingStateStep1Processing(void);
    void  RunningStateProcessing(void);
    void  ProcessEncoderReturnedBuffer(void);
    void  ProcessPendingFlush(void);

    unsigned int                                  mEncodeCoordinatorStreamNb;
    EncodeCoordinatorStream_c                    *mEncodeCoordinatorStream[MAX_ENCODE_COORDINATOR_STREAM_NB];

    OS_Mutex_t                                    mLock;
    OS_Event_t                                    mStartStopEvent;
    OS_Event_t                                    mInputStreamEvent;
    EncodeCoordinatorState_t                      mEncodeCoordinatorState;
    uint64_t                                      mTimeOut;
    uint64_t                                      mCurrentTime;
    uint64_t                                      mStartingStateInitialTime;
    int64_t                                       mPtsOffset;
    bool                                          mTerminating;

    DISALLOW_COPY_AND_ASSIGN(EncodeCoordinator_c);
};

#endif // H_ENCODE_COORDINATOR
