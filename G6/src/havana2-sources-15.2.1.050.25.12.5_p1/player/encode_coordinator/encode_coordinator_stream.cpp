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

#include "encode_coordinator_stream.h"
#include "timestamps.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeCoordinatorStream_c"

#define MEDIA_STR(x)   (((x) == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) ? "VIDEO" : ((x) == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) ? "AUDIO" : "ANY")

#define RUNNING_STATE_STR(enum) case enum: return #enum
static const char *RunningStateStr(RunningState_t RunningState)
{
    switch (RunningState)
    {
        RUNNING_STATE_STR(RUNNINGSTATE);
        RUNNING_STATE_STR(GAPSTATE);
        RUNNING_STATE_STR(TIMEOUTSTATE);
        RUNNING_STATE_STR(OUTOFGAPSTATE);
        RUNNING_STATE_STR(EOSSTATE);
    default:
        return "???";
    }
}

#define OUTPUT_STR(enum) case enum: return #enum
static const char *OutputStr(DataOut_t DataOut)
{
    switch (DataOut)
    {
        OUTPUT_STR(NODATA);
        OUTPUT_STR(CURRENT);
        OUTPUT_STR(CLONE);
    default:
        return "???";
    }
}


// Is current time greater or equal
bool EncodeCoordinatorStream_c::TimeGE(uint64_t current_time, uint64_t low)
{
    // no rewrap case
    if (MSB(current_time) == MSB(low)) { return (current_time >= low); }
    if ((MSB(low) == 0)  && ((current_time - low) < PTSMODULO / 2)) { return true; }
    // rewrap case
    if ((MSB(current_time) == 0)  && ((low - current_time) > PTSMODULO / 2)) { return true; }

    return false;
}


// return true if  current_time in interval [low,high [
//  low <= current_time < high
bool EncodeCoordinatorStream_c::TimeInInterval(uint64_t current_time, uint64_t low, uint64_t high)
{

    // if high or low limits has rewrapped
    if ((MSB(low) == 1) && (MSB(high) == 0))
    {
        high += PTSMODULO;
        if (MSB(current_time) == 0) { current_time += PTSMODULO; }
    }

    if (current_time <  low) { return false; }
    if (current_time >= high) { return false; }
    return true;

}

EncodeCoordinatorStream_c::EncodeCoordinatorStream_c(EncodeStreamInterface_c *encodeStream,
                                                     EncodeCoordinatorInterface_c *encodeCoordinator,
                                                     ReleaseBufferInterface_c     *OriginalReleaseBufferInterface)
    : mLock()
    , mInputRing(NULL)
    , mReturnedRing(NULL)
    , mOutputPort(NULL)
    , mEncodeStream(encodeStream)
    , mEncodeCoordinator(encodeCoordinator)
    , mReleaseBufferInterface(OriginalReleaseBufferInterface)
    , mFrameDuration(0)
    , mHasGap(false)
    , mReady(false)
    , mShift(false)
    , mIsLastRepeat(false)
    , mDiscontinuity()
    , mDataOut()
    , mLastReceivedTime(0)
    , mCurrentState()
    , mNextState()
    , mStreamTime(0)
    , mNextStreamTime(0)
    , mEndGapTime(0)
    , mNextEndGapTime(0)
    , mCurrentFrameSlot(NULL)
    , mLastCloneBuffer(NULL)
    , mAfterFrameSlot(NULL)
    , mEncoderBufferTypes(NULL)
    , mEncoder(NULL)
    , mTranscodeBuffer()
    , mMedia(STM_SE_ENCODE_STREAM_MEDIA_ANY)
    , mFlushAsked(false)
    , mEndOfFlushEvent()
    , mEncIn(0)
    , mEncOut(0)
    , mEncCloneIn(0)
    , mEncCloneOut(0)
    , mBufferIn(0)
    , mBufferOut(0)
{
    OS_InitializeMutex(&mLock);
    OS_InitializeEvent(&mEndOfFlushEvent);
}

// FinalizeInit() function, to be called after the constructor
// Allocates all the resources needeed by the EncodeCoordinatorStream object
EncoderStatus_t EncodeCoordinatorStream_c::FinalizeInit(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    // Allocate input ring
    mInputRing = new RingGeneric_c(ENCODER_MAX_INPUT_BUFFERS, "EncodeCoordinatorStream_c::InputRing");
    if (mInputRing == NULL)
    {
        SE_ERROR("Failed to allocate input ring\n");
        return EncoderError;
    }
    if (mInputRing->InitializationStatus != RingNoError)
    {
        SE_ERROR("New allocated input ring InitializationStatus %d\n", mInputRing->InitializationStatus);
        delete mInputRing;
        return EncoderError;
    }

    // Allocate Returned ring
    mReturnedRing = new RingGeneric_c(ENCODER_MAX_INPUT_BUFFERS, "EncodeCoordinatorStream_c::ReturnedRing");
    if (mReturnedRing == NULL)
    {
        SE_ERROR("Failed to allocate returned ring\n");
        delete mInputRing;
        return EncoderError;
    }

    if (mReturnedRing->InitializationStatus != RingNoError)
    {
        SE_ERROR("New allocated returned ring InitializationStatus %d\n", mReturnedRing->InitializationStatus);
        delete mReturnedRing;
        delete mInputRing;
        return EncoderError;
    }

    // Retrieve Encoder object for buffer management
    mEncoder = mEncodeStream->GetEncoderObject();
    mEncoderBufferTypes = mEncoder->GetBufferTypes();

    // here we provide these information to Transcode Buffer
    for (unsigned int index = 0; index < ENCODER_MAX_INPUT_BUFFERS; index++)
    {
        mTranscodeBuffer[index].FinalizeInit(mEncoderBufferTypes);
    }

    // Set output port to EncodeStream input port
    mOutputPort = mEncodeStream->GetInputPort();

    // Init OK
    return EncoderNoError;
}

// Halt() function, to be called before the destructor
// Blocking until flush termination
void EncodeCoordinatorStream_c::Halt(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    DoFlush();
}

EncodeCoordinatorStream_c::~EncodeCoordinatorStream_c(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    delete mInputRing;
    delete mReturnedRing;

    OS_TerminateEvent(&mEndOfFlushEvent);
    OS_TerminateMutex(&mLock);
}

// EncodeStream flush, called by the coordinator
RingStatus_t EncodeCoordinatorStream_c::Flush(void)
{
    SE_DEBUG(group_encoder_stream, "%s: >\n", MEDIA_STR(mMedia));

    // notify that a flush is requested
    mFlushAsked = true;

    // Wake up coordinator process
    mEncodeCoordinator->SignalNewStreamInput();

    // Wait for flush to be terminated (will always finish, even the error)
    OS_WaitForEventAuto(&mEndOfFlushEvent, OS_INFINITE);

    OS_ResetEvent(&mEndOfFlushEvent);

    SE_DEBUG(group_encoder_stream, "%s: <\n", MEDIA_STR(mMedia));

    return RingNoError;
}

// New just decoded frame inserted into input ring
RingStatus_t EncodeCoordinatorStream_c::Insert(uintptr_t Value)
{
    RingStatus_t    RingStatus;

    SE_DEBUG(group_encoder_stream, "%s %p\n", MEDIA_STR(mMedia), (void *)Value);
    if (Value != 0)
    {
        // Insert buffer in input ring
        RingStatus = mInputRing->Insert(Value);
        if (RingStatus != RingNoError)
        {
            SE_ERROR("Ring insert error - %s %p\n", MEDIA_STR(mMedia), (void *)Value);
            return RingStatus;
        }
        mBufferIn++;
    }

    // Signal new input to encode coordinator thread
    mEncodeCoordinator->SignalNewStreamInput();
    return RingNoError;
}



//***********************  all functions below are called from the internal thread ******************************
void EncodeCoordinatorStream_c::PerformPendingFlush()
{
    if (mFlushAsked == true)
    {
        DoFlush();
        mFlushAsked = false;
        OS_SetEvent(&mEndOfFlushEvent);
    }
}

void EncodeCoordinatorStream_c::DoFlush(void)
{
    SE_DEBUG(group_encoder_stream, "%s: >\n", MEDIA_STR(mMedia));

    // Release remaining buffers in the input ring
    if (mInputRing != NULL)
    {
        Buffer_t        Buffer;
        while (mInputRing->NonEmpty())
        {
            mInputRing->Extract((uintptr_t *)(&Buffer));

            if (Buffer != NULL)
            {
                mReleaseBufferInterface->ReleaseBuffer(Buffer);
                mBufferOut++;
            }
        }
    }

    // Wait for all buffer return by encoder
    uint64_t WaitTimeout = OS_GetTimeInMicroSeconds() + (ENCODER_MAX_INPUT_BUFFERS * FRAME_DURATION_US);
    while (BufferStateInEncoder())
    {
        ProcessEncoderReturnedBuffer();
        OS_SleepMilliSeconds(10);
        uint64_t Now = OS_GetTimeInMicroSeconds();
        if (Now > WaitTimeout)
        {
            BufferStateLog();
            break;
        }
    }

    // Release next frame if exists
    if (mAfterFrameSlot != NULL)
    {
        mAfterFrameSlot->BufferStateSet(BufferUnused);
        mReleaseBufferInterface->ReleaseBuffer(mAfterFrameSlot->GetBuffer());
        mBufferOut++;
        mAfterFrameSlot = NULL;
    }

    // Release current frame if exists
    if (mCurrentFrameSlot != NULL)
    {
        mCurrentFrameSlot->BufferStateSet(BufferUnused);
        mReleaseBufferInterface->ReleaseBuffer(mCurrentFrameSlot->GetBuffer());
        mBufferOut++;
        mCurrentFrameSlot = NULL;
    }

    // Reset state
    mNextState = EOSSTATE;
    mCurrentState = mNextState;

    SE_DEBUG(group_encoder_stream, "%s: <\n", MEDIA_STR(mMedia));
}

bool EncodeCoordinatorStream_c::BufferStateInEncoder(void)
{
    for (unsigned int index = 0; index < ENCODER_MAX_INPUT_BUFFERS; index++)
    {
        if (mTranscodeBuffer[index].IsBufferState(BufferOwnByEncoder)) { return true; }
    }
    return false;
}

#define MAX_STRING  80
void  EncodeCoordinatorStream_c::BufferStateLog(void)
{
    char s[MAX_STRING];
    for (unsigned int index = 0; index < ENCODER_MAX_INPUT_BUFFERS; index++)
    {
        if (mTranscodeBuffer[index].IsBufferStateUnused() == false)
        {
            SE_ERROR("Still some buffers not back from encode index %d state %s\n", index, mTranscodeBuffer[index].BufferStateString(s, sizeof(s)));
        }
    }
}

TranscodeBuffer_t EncodeCoordinatorStream_c::ExtractInputFrame()
{
    Buffer_t   buff = NULL;
    while (mInputRing->NonEmpty())
    {
        unsigned int index;
        unsigned long long frameduration;
        bool    eos;

        TranscodeBuffer_t   tbuffer;
        mInputRing->Extract((uintptr_t *)(&buff));

        buff->GetIndex(&index);
        tbuffer = &mTranscodeBuffer[index];

        tbuffer->SetBuffer(buff);

        eos = tbuffer->IsEOS();
        tbuffer->GetBufferFrameDuration(&frameduration);

        SE_VERBOSE(group_encoder_stream, "INPUT_IO %s: current frame 0x%p   index %d  pts:%lld frameduration:%lld eos:%d\n", MEDIA_STR(mMedia), buff, index, tbuffer->Pts(), frameduration, eos);
        mLastReceivedTime = OS_GetTimeInMicroSeconds();

        // We need to initialize the mStreamTime with current frame PTS
        // when getting the very first frame of the stream
        if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_ANY)
        {
            mMedia = tbuffer->GetMedia();
            SE_DEBUG(group_encoder_stream, "Media is %s\n", MEDIA_STR(mMedia));
        }

        // current PTS must be greater than mStreamTime or less than a rewind amount to avoid some blocking
        if (((frameduration > MIN_FRAME_DURATION_US) || eos) && (TimeInInterval(tbuffer->Pts(), mStreamTime - mFrameDuration * MAX_FRAME_REWIND_DETECTION, mStreamTime) == 0))
        {
            tbuffer->BufferStateSet(BufferDecode);
            return tbuffer;
        }
        else
        {
            SE_DEBUG(group_encoder_stream, "Discarding Input %s frame  Pts:%lld  Duration:%lld StreamTime:%lld\n", MEDIA_STR(mMedia), tbuffer->Pts(), frameduration, mStreamTime);
        }
        CheckForBufferRelease(tbuffer);
    }

    return NULL;
}

// Gets the PTS of the current frame
bool EncodeCoordinatorStream_c::GetTime(uint64_t *StreamTime)
{
    SE_VERBOSE(group_encoder_stream, "%s\n", MEDIA_STR(mMedia));

    if (mCurrentFrameSlot == NULL)
    {
        SE_DEBUG(group_encoder_stream, "%s: no current frame available\n", MEDIA_STR(mMedia));
        return false;
    }
    *StreamTime = mCurrentFrameSlot->Pts();
    return true;
}


// In case of time compression, the next expected must be incremented to jump to the end of the gap
void EncodeCoordinatorStream_c::AdjustJump(uint64_t newtime)
{
    int64_t    LocalJump = 0;
    uint64_t    currentFrameDuration = 0;

    mCurrentFrameSlot->GetBufferFrameDuration(&currentFrameDuration);

    // The jump must be a multiple of the frame duration
    if (currentFrameDuration != 0)
    {
        LocalJump = (((int64_t)(newtime - mStreamTime) / (int64_t) currentFrameDuration)) * (int64_t) currentFrameDuration;
        // we should always jump to newtime or before newtime
        // in case of negative jump correct the rounding effect , localjump is a negative value
        if (LocalJump != (newtime - mStreamTime))
        {
            if (LocalJump < 0) { LocalJump -= currentFrameDuration; }
        }
    }
    else
    {
        SE_ERROR("%s:< currentFrameDuration is 0 - %s\n", MEDIA_STR(mMedia), RunningStateStr(mCurrentState));
    }

    SE_DEBUG(group_encoder_stream, " %s :< streamtime from  %lld to %lld newtime %lld\n", MEDIA_STR(mMedia), mStreamTime, mStreamTime + LocalJump, newtime);
    mStreamTime = mStreamTime + LocalJump;
    mNextStreamTime = mStreamTime;

}

bool EncodeCoordinatorStream_c::GetFrameDuration(uint64_t *FrameDuration)
{
    *FrameDuration = 0;
    if (mCurrentFrameSlot == NULL)
    {
        // No current frame
        return false;
    }

    return mCurrentFrameSlot->GetBufferFrameDuration(FrameDuration);
}

// Transmit EOS buffer
bool EncodeCoordinatorStream_c::TransmitEOS(void)
{
    if ((mCurrentFrameSlot != NULL) && mCurrentFrameSlot->IsEOS())
    {
        SE_DEBUG(group_encoder_stream, "_OUT (transmit) %s:out %p(%lld) eos:1\n", MEDIA_STR(mMedia), mCurrentFrameSlot->GetBuffer(), mCurrentFrameSlot->Pts());
        SendBufferToEncoder(mCurrentFrameSlot);
        mCurrentFrameSlot->BufferStateClear(BufferDecode);
        mEncIn++;
        mCurrentFrameSlot = NULL;
        return true;
    }

    return false;
}

// Extracts current frame from the input ring
bool EncodeCoordinatorStream_c::GetCurrentFrame(void)
{
    // If current frame slot empty, fill it
    if (mCurrentFrameSlot == NULL) { mCurrentFrameSlot = ExtractInputFrame(); }
    return (mCurrentFrameSlot != NULL);
}

bool EncodeCoordinatorStream_c::GetNextTime(uint64_t *streamTime)
{
    if (mCurrentFrameSlot == NULL)
    {
        // Happen if stream has received an EOS
        return false;
    }
    *streamTime = mStreamTime;
    return true;
}

// Check PTS of current frame and release it if it doesn't overlap highestCurrentTime
// then move to the next one until overlap is OK or no more buffer
bool EncodeCoordinatorStream_c::DiscardUntil(uint64_t highestCurrentTime)
{
    // this is called from init phase
    // so use to set internal state value
    mNextState = RUNNINGSTATE;
    mCurrentState = RUNNINGSTATE;
    while ((mCurrentFrameSlot != NULL) || (mCurrentFrameSlot = ExtractInputFrame()))
    {
        uint64_t    frameTime;
        uint64_t    frameDuration;
        frameTime = mCurrentFrameSlot->Pts();
        if (mCurrentFrameSlot->GetBufferFrameDuration(&frameDuration) == true)
            //FIXME     && (mCurrentFrameSlot->IsEOS() == false))
        {
            // If frame PTS in in the future stop discarding
            if (highestCurrentTime < frameTime)
            {
                SE_DEBUG(group_encoder_stream, "%s: Buffer in future (time %lld, ref time %lld)\n", MEDIA_STR(mMedia), frameTime, highestCurrentTime);
                return true;
            }

            if ((highestCurrentTime >= frameTime)
                && (highestCurrentTime < (frameTime + frameDuration)))
            {
                SE_DEBUG(group_encoder_stream, "%s: Buffer in synch (time %lld, ref time %lld)\n", MEDIA_STR(mMedia), frameTime, highestCurrentTime);

                // The current time is now the next expected PTS
                mStreamTime = frameTime;
                return true;
            }

            // EOS frames are sent directly to encoder to trig EOS at application level
            if (TransmitEOS() == false)
            {
                SE_DEBUG(group_encoder_stream, "%s: ReleaseBuffer %p\n", MEDIA_STR(mMedia), mCurrentFrameSlot);
                mCurrentFrameSlot->BufferStateSet(BufferUnused);
                mReleaseBufferInterface->ReleaseBuffer(mCurrentFrameSlot->GetBuffer());
                mBufferOut++;
                mCurrentFrameSlot = NULL;
            }
        }
    }

    SE_DEBUG(group_encoder_stream, "%s: No more input buffer to process \n", MEDIA_STR(mMedia));
    return false;
}

// Try to release a buffer via the provided release_buffer_interface_c
// buffer could still be in use by Encoder
void EncodeCoordinatorStream_c::CheckForBufferRelease(TranscodeBuffer_t buffer)
{
    if (buffer == NULL) { return; }
    SE_VERBOSE(group_encoder_stream, "%s: Buffer %p\n", MEDIA_STR(mMedia), buffer);

    if (buffer->IsBufferState(BufferOwnByEncoder))
    {
        return;
    }

    // still own by coordinator
    if (buffer->IsBufferState(BufferDecode))
    {
        return;
    }

    SE_VERBOSE(group_encoder_stream, "%s: Buffer not own by encoder %p\n", MEDIA_STR(mMedia), buffer);

    SE_DEBUG(group_encoder_stream, "IO_RET %s:%p Buffer(in %d,out %d):%d Enc(in %d,out %d):%d Clone(in %d :out %d )%d\n", MEDIA_STR(mMedia), buffer->GetBuffer(), mBufferIn, mBufferOut,
             mBufferIn - mBufferOut, mEncIn, mEncOut, mEncIn - mEncOut, mEncCloneIn, mEncCloneOut, mEncCloneIn - mEncCloneOut);


    // Deal with clone buffer only
    if (buffer->IsBufferState(BufferClone))
    {
        // for last buffer of the Gap , we take care of freeing the original source of the clone buffer
        if (buffer->IsBufferState(BufferLastClone))
        {

            TranscodeBuffer_t clonesource = buffer->BufferStateGetCloneReference();
            clonesource->BufferStateClear(BufferReference);
            SE_DEBUG(group_encoder_stream, "IO_RETURN %s  Last clone reference: %p\n", MEDIA_STR(mMedia), clonesource->GetBuffer());

            CheckForBufferRelease(clonesource);

        }
        buffer->GetBuffer()->DecrementReferenceCount(IdentifierGetInjectBuffer);
        buffer->BufferStateSet(BufferUnused);
    }
    // Deal with original buffer from decoder
    else
    {
        // if buffer is not used as a reference for a clone then it can be returned
        if (! buffer->IsBufferState(BufferReference))
        {
            buffer->BufferStateSet(BufferUnused);
            mReleaseBufferInterface->ReleaseBuffer(buffer->GetBuffer());
            mBufferOut++;
        }
        else
        {
            SE_DEBUG(group_encoder_stream, "%s: Buffer %p is clone source\n", MEDIA_STR(mMedia), buffer);
        }
    }
}

// Release callback called by EncodeStream after encoding
PlayerStatus_t EncodeCoordinatorStream_c::ReleaseBuffer(Buffer_c *buffer)
{
    (void) mReturnedRing->Insert((uintptr_t)(buffer));
    mEncodeCoordinator->SignalNewStreamInput();
    return PlayerNoError;
}

// Process buffer return by the encoders
void EncodeCoordinatorStream_c::ProcessEncoderReturnedBuffer(void)
{
    Buffer_t        Buffer;
    while (mReturnedRing->NonEmpty())
    {
        mReturnedRing->Extract((uintptr_t *)(&Buffer));

        if (Buffer != NULL)
        {
            unsigned int index;
            TranscodeBuffer_t tbuffer;

            Buffer->GetIndex(&index);
            tbuffer = &mTranscodeBuffer[index];
            SE_ASSERT(tbuffer->GetBuffer() == Buffer);
            tbuffer->BufferStateClear(BufferOwnByEncoder);
            if (tbuffer->IsBufferState(BufferClone)) { mEncCloneOut++; }
            else { mEncOut++; }
            CheckForBufferRelease(tbuffer);
        }
    }
}

// Check if there is a gap between current and next frame
// If yes, set the mCurrentState in [entering_gap/in the gap/leaving_gap]
// return true if current time is on the frame before the gap or in the gap
bool EncodeCoordinatorStream_c::HasGap(uint64_t CurrentTime, uint64_t *endOfTimeGap)
{
    *endOfTimeGap = mNextEndGapTime;
    return mHasGap;
}

// Checks if stream is ready for the CurrenTime, that is the case when:
// - a buffer is available to be transmitted
// - a buffer covering this time has already been sent
// - the stream is switched off
bool EncodeCoordinatorStream_c::IsReady(uint64_t CurrentTime)
{
    return mReady;
}

bool EncodeCoordinatorStream_c::IsTimeOut(uint64_t CurrentTime)
{
    return (mNextState == TIMEOUTSTATE);
}

void EncodeCoordinatorStream_c::SendBufferToEncoder(TranscodeBuffer_t buffer)
{
    buffer->BufferStateSet(BufferOwnByEncoder);
    mOutputPort->Insert((uintptr_t)(buffer->GetBuffer()));
}

void EncodeCoordinatorStream_c::RunningState(uint64_t current_time)
{
    //  Previous sent frame covered this time
    if ((mStreamTime != 0) && TimeInInterval(current_time, mStreamTime - mFrameDuration, mStreamTime))
    {
        // a frame has already been sent
        mReady  = true;
        mDataOut = NODATA;
        return;
    }

    // special case: can happen only if first frame is EOS
    // in normal case EOS is detected on AfterFrameSlot
    if (mCurrentFrameSlot->IsEOS())
    {
        mReady  = true;
        mDataOut = CURRENT;
        mNextStreamTime = mStreamTime + mFrameDuration;
        mDiscontinuity = STM_SE_DISCONTINUITY_EOS;
        mShift = true;
        mNextState = EOSSTATE;
        return;
    }

    if (mAfterFrameSlot == NULL)
    {
        // waiting for next frame for more than 2second
        if ((OS_GetTimeInMicroSeconds() - mLastReceivedTime) > DURATION2SEC)
        {
            // Moving to TimeOut, send last correct buffer before repeating
            mReady = true;
            mDataOut = CURRENT;
            mNextState = TIMEOUTSTATE;
            // Discontinuity for Audio only, video is OK
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_FADEOUT; }
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }
        else
        {
            // No timeout, waiting frame
            mReady  = false;
            mDataOut = NODATA;
            return;
        }
    }
    else
    {
        // Got last stream buffer
        if (mAfterFrameSlot->IsEOS())
        {
            mReady = true;
            mShift = true;
            mDataOut = CURRENT;
            mNextState = EOSSTATE;
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }

        // Next frame is present, so check for a GAP or not
        if (TimeInInterval(mAfterFrameSlot->Pts(), mCurrentFrameSlot->Pts(), mCurrentFrameSlot->Pts() + (3 * mFrameDuration) / 2))
        {
            // No gap, so send current frame
            mReady = true;
            mNextState = RUNNINGSTATE;
            mDataOut = CURRENT;
            mNextStreamTime = mAfterFrameSlot->Pts();
            mShift = true;
            return;
        }
        else
        {
            // That is the beginning of a GAP
            mHasGap = false;
            mReady = true;
            mNextEndGapTime = mAfterFrameSlot->Pts();
            mNextState = GAPSTATE;
            mDataOut = CURRENT;
            // Discontinuity for Audio only , video is sent normaly
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_FADEOUT; }
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;

        }
    }
}



void EncodeCoordinatorStream_c::GapState(uint64_t current_time)
{
    // A GAP has been detected on all streams and current time has been forced to mEndGapTime
    if (current_time == mEndGapTime)
    {
        SE_DEBUG(group_encoder_stream, "%s _GAP_: Jump from %lld to %lld\n", MEDIA_STR(mMedia), mStreamTime, mEndGapTime);
        mReady  = true;
        mHasGap = false;
        mNextState = OUTOFGAPSTATE;
        mDataOut = NODATA;
        mIsLastRepeat = true;
        mNextStreamTime = mEndGapTime;
        mShift = true;
        return;
    }

    // a frame has already been sent
    if ((mStreamTime != 0) && TimeInInterval(current_time, mStreamTime - mFrameDuration, mStreamTime))
    {
        mReady  = true;
        mHasGap = true;
        mDataOut = NODATA;
        return;
    }

    // filling the Gap with repeated frame
    // until time reaches last frame of the Gap
    if (TimeInInterval(current_time, mEndGapTime - mFrameDuration, mEndGapTime))
    {
        SE_DEBUG(group_encoder_stream, "_GAP_ last gap frame %lld < %lld < %lld \n", mEndGapTime - mFrameDuration, current_time, mEndGapTime);

        // That is the last frame of the Gap
        mReady  = true;
        mHasGap = false;
        mNextState = OUTOFGAPSTATE;
        mDataOut = CLONE;
        mIsLastRepeat = true;
        if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
        mNextStreamTime = mEndGapTime ;
        mShift = true;
        return;
    }

    // Cloning
    mReady  = true;
    mHasGap = true;
    mDataOut = CLONE;
    if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
    mNextStreamTime = mStreamTime + mFrameDuration;
}


void EncodeCoordinatorStream_c::OutOfGapState(uint64_t current_time)
{
    if ((mStreamTime != 0) && TimeInInterval(current_time, mStreamTime - mFrameDuration, mStreamTime))
    {
        // a frame has already been sent
        mReady  = true;
        mHasGap = false;
        mDataOut = NODATA;
        return;
    }

    if (mAfterFrameSlot == NULL)
    {
        if ((OS_GetTimeInMicroSeconds() - mLastReceivedTime) > DURATION2SEC)
        {
            // TimeOut detection
            // so FadeIn has not been sent, we continue to Mute or repeat
            // for video we repeat a different frame from the Gap
            mReady = true;
            mHasGap = false;
            mDataOut = CURRENT;
            mNextState = TIMEOUTSTATE;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }
        else
        {
            // No timeout, so we wait
            mReady  = false;
            mHasGap = false;
            mDataOut = NODATA;
            return;
        }
    }
    else
    {
        // Just Got EOS
        if (mAfterFrameSlot->IsEOS())
        {
            mReady = true;
            mHasGap = false;
            mDataOut = CURRENT;
            mNextState = EOSSTATE;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_FADEIN; }
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) { mDiscontinuity = STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST; }
            mNextState = EOSSTATE;
            mShift = true;
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }

        // Next frame is present, so check for a  GAP again or not
        if (TimeInInterval(mAfterFrameSlot->Pts(), mCurrentFrameSlot->Pts(), mCurrentFrameSlot->Pts() + (3 * mFrameDuration) / 2))
        {
            // No more gap, so send current frame with FadeIN or NewGOP
            mReady = true;
            mHasGap = false;
            mDataOut = CURRENT;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_FADEIN; }
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) { mDiscontinuity = STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST; }
            mNextState = RUNNINGSTATE;
            mNextStreamTime = mStreamTime + mFrameDuration;
            mShift = true;
            return;
        }
        else
        {
            // Entering inside a new gap
            mReady = true;
            mHasGap = false;
            mNextEndGapTime = mAfterFrameSlot->Pts();
            mNextState = GAPSTATE;
            mDataOut = CURRENT;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }
    }
}


void EncodeCoordinatorStream_c::TimeOutState(uint64_t current_time)
{
    if ((mStreamTime != 0) && TimeInInterval(current_time, mStreamTime - mFrameDuration, mStreamTime))
    {
        // a frame has already been sent
        mReady  = true;
        mHasGap = false;
        mDataOut = NODATA;
        return;
    }

    if (mAfterFrameSlot == NULL)
    {
        // we remain in TimeOut and clone forever
        mReady = true;
        mHasGap = false;
        mDataOut = CLONE;
        if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
        mNextStreamTime = mStreamTime + mFrameDuration;
        return;
    }
    else
    {
        // Got EOS
        if (mAfterFrameSlot->IsEOS())
        {
            mReady = true;
            mIsLastRepeat = true;
            mDataOut = CLONE;
            mNextState = EOSSTATE;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
            mShift = true;
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;

        }

        // next frame would be on time, so move to transition state
        if (TimeInInterval(current_time + mFrameDuration, mAfterFrameSlot->Pts()  , mAfterFrameSlot->Pts() + mFrameDuration))
        {
            mReady  = true;
            mHasGap = false;
            mDataOut = CLONE;
            mIsLastRepeat = true;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
            mNextStreamTime = mStreamTime + mFrameDuration;
            mNextState = OUTOFGAPSTATE;
            return;
        }

        // Frame is in future, after-pts > current_time
        // move to  Gap state again
        if (TimeGE(mAfterFrameSlot->Pts(), current_time + mFrameDuration))
        {
            mReady = true;
            mNextState = GAPSTATE;
            mDataOut = CLONE;
            if (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) { mDiscontinuity = STM_SE_DISCONTINUITY_MUTE; }
            mNextEndGapTime = mAfterFrameSlot->Pts();
            mNextStreamTime = mStreamTime + mFrameDuration;
            return;
        }

        // frame is in the past, so discarding incoming frame until catching back the current time
        // other streams wait because current_time is not updated
        if (TimeGE(current_time + mFrameDuration, mAfterFrameSlot->Pts()))
        {
            mReady  = true;
            mHasGap = false;
            mDataOut = NODATA;

            // release immediatly old frame
            mAfterFrameSlot->BufferStateClear(BufferDecode);
            CheckForBufferRelease(mAfterFrameSlot);
            mAfterFrameSlot = NULL;

            return;
        }
    }

    SE_ERROR("Unexpected case stream: 0x%p Media: %s  %lld end gap %lld frame duration %lld current\n", this, (mMedia == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) ? "AUDIO" : "VIDEO", current_time,
             mEndGapTime,
             mFrameDuration);

}

void EncodeCoordinatorStream_c::EosState(uint64_t current_time)
{
    if ((mStreamTime != 0) && TimeInInterval(current_time, mStreamTime - mFrameDuration, mStreamTime))
    {
        // a frame has already been sent
        mReady  = true;
        mHasGap = false;
        mDataOut = NODATA;
        return;
    }

    if ((mCurrentFrameSlot != NULL) && mCurrentFrameSlot->IsEOS())
    {
        mReady  = true;
        mHasGap = false;
        mDataOut = CURRENT;
        mNextStreamTime = mStreamTime + mFrameDuration;
        mDiscontinuity = STM_SE_DISCONTINUITY_EOS;
        mShift = true;
        mNextState = EOSSTATE;
        return;
    }

    // otherwise let other stream continue forever
    mReady  = true;
    mHasGap = false;
    mDataOut = NODATA;
    mNextStreamTime = 0;
}


void EncodeCoordinatorStream_c::ComputeState(uint64_t current_time)
{
    mHasGap  = false;
    mReady   = false;
    mDataOut = NODATA;
    mShift   = false;
    mIsLastRepeat = false;
    mDiscontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    // loading frames
    if (mCurrentFrameSlot == NULL)
    {
        mCurrentFrameSlot = mAfterFrameSlot;
        mAfterFrameSlot = NULL;
    }
    if (mAfterFrameSlot == NULL)
    {
        mAfterFrameSlot = ExtractInputFrame();
    }

    if (mCurrentFrameSlot == NULL)
    {
        // inactive stream
        mReady = true;
        mNextStreamTime = 0;
        return;
    }

    mCurrentFrameSlot->GetBufferFrameDuration(&mFrameDuration);

    switch (mCurrentState)
    {
    case RUNNINGSTATE:
        RunningState(current_time);
        break;

    case GAPSTATE:
        GapState(current_time);
        break;

    case TIMEOUTSTATE:
        TimeOutState(current_time);
        break;

    case OUTOFGAPSTATE:
        OutOfGapState(current_time);
        break;

    case EOSSTATE:
        EosState(current_time);
        break;

    default:
        SE_ERROR("unexepected state\n");
        break;
    }
}

void EncodeCoordinatorStream_c::UpdateState()
{
    mCurrentState = mNextState;
    mStreamTime  = mNextStreamTime;
    mEndGapTime   = mNextEndGapTime;

    if (mShift)
    {
        mCurrentFrameSlot->BufferStateClear(BufferDecode);
        CheckForBufferRelease(mCurrentFrameSlot);
        mCurrentFrameSlot = mAfterFrameSlot;
        mAfterFrameSlot = NULL;
    }
}

void EncodeCoordinatorStream_c::Encode(uint64_t current_time)
{
    uint64_t  encodeTime = mStreamTime + mEncodeCoordinator->GetPtsOffset();
    SE_DEBUG(group_encoder_stream, "encodeTime %lld  mStreamTime %lld Offset %lld\n", encodeTime , mStreamTime , mEncodeCoordinator->GetPtsOffset());

    SE_DEBUG(group_encoder_stream,
             "%s:0x%p %s time%lld stream_time:%lld xstreamtime:%lld duration:%lld Slot:0x%p NextSlot:0x%p Rdy:%d Gap:%d xGapend:%lld EOS:%d XState:%s DataOut:%s Discont:%s\n", MEDIA_STR(mMedia), this,
             RunningStateStr(mCurrentState), current_time, mStreamTime, mAfterFrameSlot ? mAfterFrameSlot->Pts() : 0, mFrameDuration, mCurrentFrameSlot ? mCurrentFrameSlot->GetBuffer() : NULL,
             mAfterFrameSlot ? mAfterFrameSlot->GetBuffer() : NULL, mReady, mHasGap, mNextEndGapTime , mAfterFrameSlot ? mAfterFrameSlot->IsEOS() : 0, RunningStateStr(mNextState), OutputStr(mDataOut),
             StringifyDiscontinuity(mDiscontinuity));

    if (mDataOut == CLONE)
    {
        unsigned long long      timeNowMs;
        TranscodeBuffer_c      *clonedBuffer = NULL;

        timeNowMs               = OS_GetTimeInMicroSeconds();
        while (true)
        {
            clonedBuffer = mCurrentFrameSlot->CloneBuffer(mTranscodeBuffer, mEncoder, encodeTime);

            if (clonedBuffer != NULL) { break; }

            if (OS_GetTimeInMicroSeconds() > (timeNowMs + 1000))
            {
                SE_WARNING("Waiting for encode buffer for clone\n");
                timeNowMs   = OS_GetTimeInMicroSeconds();
            }
            OS_SleepMilliSeconds(5);

            // release returned clone buffers
            ProcessEncoderReturnedBuffer();
        }

        mCurrentFrameSlot->BufferStateSet(BufferReference);
        if (mIsLastRepeat)
        {
            clonedBuffer->BufferStateSet(BufferLastClone);
            clonedBuffer->BufferStateSetCloneReference(mCurrentFrameSlot);
        }
        clonedBuffer->SetEncodeTime(encodeTime);
        clonedBuffer->SetNativeTime(mStreamTime);
#ifndef UNITTESTS
        // do not send CLOSED Gop discontinuity
        if (mDiscontinuity == STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST) { mDiscontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;  }
#endif
        clonedBuffer->SetEncodeDiscontinuity(mDiscontinuity);
        clonedBuffer->BufferStateSet(BufferClone);
        mLastCloneBuffer = clonedBuffer;
        SE_DEBUG(group_encoder_stream, "_OUT %s:clone %p(%lld,%lld) %s\n", MEDIA_STR(mMedia), clonedBuffer->GetBuffer(), clonedBuffer->Pts(), encodeTime, StringifyDiscontinuity(mDiscontinuity));
        SendBufferToEncoder(clonedBuffer);
        mEncCloneIn++;
    }

    if (mDataOut == CURRENT)
    {
        mCurrentFrameSlot->SetEncodeTime(encodeTime);
        SE_DEBUG(group_encoder_stream, "_OUT %s:out %p(%lld,%lld) %s\n", MEDIA_STR(mMedia), mCurrentFrameSlot->GetBuffer(), mCurrentFrameSlot->Pts(), encodeTime, StringifyDiscontinuity(mDiscontinuity));
#ifndef UNITTESTS
        // do not send CLOSED GOP discontinuity
        if (mDiscontinuity == STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST) { mDiscontinuity = STM_SE_DISCONTINUITY_CONTINUOUS; }
#endif
        mCurrentFrameSlot->SetEncodeDiscontinuity(mDiscontinuity);
        SendBufferToEncoder(mCurrentFrameSlot);
        mEncIn++;
    }

    if (mDataOut == NODATA && mIsLastRepeat)
    {
        // Condition is true only when coming from GAP state after a seek
        if (mCurrentFrameSlot->IsBufferState(BufferReference))
        {
            // Buffer has been cloned
            if (mLastCloneBuffer->IsBufferState(BufferOwnByEncoder))
            {
                // Clone Still in encoder, so mark it as last clone
                mLastCloneBuffer->BufferStateSet(BufferLastClone);
                mLastCloneBuffer->BufferStateSetCloneReference(mCurrentFrameSlot);
            }
            else
            {
                // Already returned, so clear the reference flag
                mCurrentFrameSlot->BufferStateClear(BufferReference);
            }
        }
    }
}
