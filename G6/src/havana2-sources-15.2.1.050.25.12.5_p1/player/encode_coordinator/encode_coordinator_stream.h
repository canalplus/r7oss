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

#ifndef H_ENCODE_COORDINATOR_STREAM
#define H_ENCODE_COORDINATOR_STREAM

#include "player_types.h"
#include "port.h"
#include "ring_generic.h"
#include "buffer.h"
#include "encoder.h"

#include "encode_coordinator_interface.h"
#include "encode_coordinator_buffer.h"
#define MAX_FRAME_REWIND_DETECTION 5
#define TIMEOUT_FRAME_COUNT     5
#define FRAME_DURATION_US   300000ULL
#define MIN_FRAME_DURATION_US   1000ULL
#define DURATION2SEC        2000000

#define N 33
#define PTSMODULO (1ULL<<(N+1))
#define MSB(a)  ((a>>N) & 1)
#define MASK(a) (a & (PTSMODULO-1))


typedef enum RunningState_s
{
    // Normal running state
    RUNNINGSTATE = 0,
    // Gap state , this stream is filling a gap between two frames
    GAPSTATE,
    // streams is waiting for a frame and is repeating last correct frame
    TIMEOUTSTATE,
    // transition state betwen Gap and running , needed can we may face a new Gap
    OUTOFGAPSTATE,
    // stream has received an EOS and is stopped
    EOSSTATE

} RunningState_t;

typedef enum DataOut_s
{
    // Current frame is sent to encoder
    CURRENT = 0,
    // Current frame is clone , clone is sent to encoder
    CLONE,
    // no data are sent out
    NODATA,

} DataOut_t;


class EncodeCoordinatorStream_c : public Port_c, public ReleaseBufferInterface_c
{
public:
    EncodeCoordinatorStream_c(EncodeStreamInterface_c      *encodeStream,
                              EncodeCoordinatorInterface_c *encodeCoordinator,
                              ReleaseBufferInterface_c     *OriginalReleaseBufferInterface);
    virtual EncoderStatus_t FinalizeInit(void);
    virtual void            Halt(void);

    virtual ~EncodeCoordinatorStream_c();

    // Port interface
    RingStatus_t Insert(uintptr_t Value);
    RingStatus_t InsertFromIRQ(uintptr_t Value) { SE_ASSERT(0); return RingNoError; }
    RingStatus_t Extract(uintptr_t *Value, unsigned int BlockingPeriod = OS_INFINITE)   { return RingNoError; }
    RingStatus_t Flush(void);
    bool         NonEmpty(void)                                                         { return mInputRing->NonEmpty(); }

    Port_c                      *GetInputPort(void)    { return mInputRing; }
    EncodeStreamInterface_c     *GetEncodeStream(void) { return mEncodeStream; }

    // Release buffer interface
    PlayerStatus_t ReleaseBuffer(Buffer_t Buffer);

    // Features
    bool GetCurrentFrame(void);
    bool GetNextValidFrame(uint64_t CurrentTime);


    bool GetFrameDuration(uint64_t *FrameDuration);
    bool GetTime(uint64_t *streamTime);

    bool TransmitEOS(void);
    bool DiscardUntil(uint64_t highestCurrentTime);

    bool HasTimeOut(uint64_t CurrentTime, bool *wait);
    bool HasGap(uint64_t CurrentTime, uint64_t *endOfTimeGap);
    bool IsReady(uint64_t CurrentTime);
    bool IsTimeOut(uint64_t CurrentTime);
    void AdjustJump(uint64_t newtime);

    void Encode(uint64_t current_time);

    bool GetNextTime(uint64_t *streamTime);

    void PerformPendingFlush(void);
    void ProcessEncoderReturnedBuffer(void);


    void ComputeState(uint64_t current_time);
    void UpdateState(void);


private:
    OS_Mutex_t                       mLock;
    RingGeneric_c                   *mInputRing;
    RingGeneric_c                   *mReturnedRing;
    Port_c                          *mOutputPort;
    EncodeStreamInterface_c         *mEncodeStream;
    EncodeCoordinatorInterface_c    *mEncodeCoordinator;
    ReleaseBufferInterface_c        *mReleaseBufferInterface;

    // Status and state stream variables
    uint64_t            mFrameDuration;
    bool            mHasGap;
    bool            mReady;
    bool            mShift;
    bool            mIsLastRepeat;
    stm_se_discontinuity_t  mDiscontinuity;
    DataOut_t           mDataOut;
    uint64_t            mLastReceivedTime;
    RunningState_t      mCurrentState;
    RunningState_t      mNextState;
    uint64_t            mStreamTime;
    uint64_t            mNextStreamTime;
    uint64_t            mEndGapTime;
    uint64_t            mNextEndGapTime;
    TranscodeBuffer_t                mCurrentFrameSlot;
    TranscodeBuffer_t                mLastCloneBuffer;
    TranscodeBuffer_t                mAfterFrameSlot;


    const EncoderBufferTypes        *mEncoderBufferTypes;
    Encoder_c                       *mEncoder;
    TranscodeBuffer_c                mTranscodeBuffer[ENCODER_MAX_INPUT_BUFFERS];
    stm_se_encode_stream_media_t     mMedia;
    bool                             mFlushAsked;
    OS_Event_t                       mEndOfFlushEvent;

    // Counter for debug purpose
    unsigned int                     mEncIn;
    unsigned int                     mEncOut;
    unsigned int                     mEncCloneIn;
    unsigned int                     mEncCloneOut;
    unsigned int                     mBufferIn;
    unsigned int                     mBufferOut;

    TranscodeBuffer_c            *ExtractInputFrame(void);
    bool GetCloneInputBuffer(TranscodeBuffer_t OriginalBuffer, TranscodeBuffer_t *ClonedBuffer, uint64_t encodeTime);
    void CheckForBufferRelease(TranscodeBuffer_t buffer);
    void DoFlush(void);
    void SendBufferToEncoder(TranscodeBuffer_t buffer);

    void RunningState(uint64_t current_time);
    void GapState(uint64_t current_time);
    void TimeOutState(uint64_t current_time);
    void OutOfGapState(uint64_t current_time);
    void EosState(uint64_t current_time);

    bool TimeInInterval(uint64_t current_time, uint64_t low, uint64_t high);
    bool TimeGE(uint64_t current_time, uint64_t low);

    bool BufferStateInEncoder(void);
    void BufferStateLog(void);

    DISALLOW_COPY_AND_ASSIGN(EncodeCoordinatorStream_c);
};

#endif // H_ENCODE_COORDINATOR_STREAM
