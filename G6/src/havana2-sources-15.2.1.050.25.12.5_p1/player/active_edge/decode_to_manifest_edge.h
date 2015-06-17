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
#ifndef H_DECODE_TO_MANIFEST
#define H_DECODE_TO_MANIFEST

#include "active_edge_base.h"

class DecodeToManifestEdge_c : public ActiveEdge_Base_c
{
public:
    DecodeToManifestEdge_c(int ThreadDescId, PlayerStream_t Stream, int InputRingSize,
                           ManifestationCoordinator_t ManifestationCoordinator, OutputTimer_t OutputTimer)
        : ActiveEdge_Base_c(ThreadDescId, Stream, InputRingSize)
        , mAccumulatedDecodeBufferTable()
        , mAccumulatedDecodeBufferTableOccupancy(0)
        , mManifestationCoordinator(ManifestationCoordinator)
        , mOutputTimer(OutputTimer)
        , mMaximumActualSequenceNumberSeen(0)
        , mMinimumSequenceNumberAccumulated(0xffffffffffffffffULL)
        , mSequenceNumber(INVALID_SEQUENCE_VALUE)
        , mDesiredFrameIndex(0)
        , mMarkerFrameBuffer(NULL)
        , mTime(INVALID_TIME)
        , mPreviousPlayBackTime(INVALID_TIME)
        , mLastPreManifestDiscardBuffer(false)
        , mVideoDisplayParameters()
        , mThrottlingEvent()
        , mQueueBufferLock()
        , mFirstFrame(true)
    {
        OS_InitializeEvent(&mThrottlingEvent);
        OS_InitializeMutex(&mQueueBufferLock);
    }

    virtual ~DecodeToManifestEdge_c(void)
    {
        OS_TerminateEvent(&mThrottlingEvent);
        OS_TerminateMutex(&mQueueBufferLock);
    }

    virtual PlayerStatus_t   CallInSequence(PlayerSequenceType_t      SequenceType,
                                            PlayerSequenceValue_t     SequenceValue,
                                            PlayerComponentFunction_t Fn,
                                            ...);

    ManifestationCoordinator_t GetManifestationCoordinator() { return mManifestationCoordinator; }

    OutputTimer_t GetOutputTimer() { return mOutputTimer; }

    void ReleaseQueuedDecodeBuffers();

    void WakeUp()
    {
        OS_SetEvent(&mStream->SingleStepMayHaveHappened);
        OS_SetEvent(&mThrottlingEvent);
    }

    virtual void Stop()
    {
        WakeUp();
        ActiveEdge_Base_c::Stop();
    }

    virtual void DiscardUntilMarkerFrame()
    {
        WakeUp();
        ActiveEdge_Base_c::DiscardUntilMarkerFrame();
    }

private:
    PlayerBufferRecord_t        mAccumulatedDecodeBufferTable[MAX_DECODE_BUFFERS];
    unsigned int                mAccumulatedDecodeBufferTableOccupancy;
    ManifestationCoordinator_t  mManifestationCoordinator;
    OutputTimer_t               mOutputTimer;
    unsigned long long          mMaximumActualSequenceNumberSeen;
    unsigned long long          mMinimumSequenceNumberAccumulated;
    unsigned long long          mSequenceNumber;
    unsigned int                mDesiredFrameIndex;
    Buffer_t                    mMarkerFrameBuffer;
    unsigned long long          mTime;
    unsigned long long          mPreviousPlayBackTime;
    bool                        mLastPreManifestDiscardBuffer;
    VideoDisplayParameters_t    mVideoDisplayParameters;
    OS_Event_t                  mThrottlingEvent;
    OS_Mutex_t                  mQueueBufferLock;
    bool                        mFirstFrame;

    virtual void MainLoop();
    virtual PlayerStatus_t  PerformInSequenceCall(PlayerControlStructure_t *ControlStructure);

    bool    CheckForNonDecodedFrame(unsigned int DisplayFrameIndex);

    void    FlushNonDecodedFrameList();

    void    CheckForVideoDisplayParametersChange(Buffer_t Buffer);

    void    SwitchOutputTimer();

    Buffer_t GetNextBufferInDisplayOrder();

    void    HandleCodedFrameBuffer(Buffer_t Buffer);
    void    HandleMarkerFrame(Buffer_t Buffer);

    void    CheckPtsDiscontinuity(unsigned long long CurNormPlayBackTime);

    void    FlushAccumulatedBuffers(unsigned long long *pCurNormPlayBackTime);

    unsigned int GetMaxDecodesOutOfOrder();

    Buffer_t SearchNextDisplayOrderBufferInAccumulatedBuffers();

    void    InsertInAccumulatedBuffers(Buffer_t Buffer);
    Buffer_t ExtractFromAccumulatedBuffers(int i);

    void HandleFirstFrame(Buffer_t Buffer);

    bool TestDiscardBuffer(Buffer_t Buffer, ParsedFrameParameters_t *ParsedFrameParameters);

    ParsedFrameParameters_t *GetParsedFrameParameters(Buffer_t Buffer)
    {
        ParsedFrameParameters_t *ParsedFrameParameters;
        Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataParsedFrameParametersReferenceType, (void **) &ParsedFrameParameters);
        SE_ASSERT(ParsedFrameParameters != NULL);

        return ParsedFrameParameters;
    }

    PlayerSequenceNumber_t   *GetSequenceNumberStructure(Buffer_t Buffer)
    {
        Buffer_t OriginalCodedFrameBuffer;
        Buffer->ObtainAttachedBufferReference(mStream->GetCodedFrameBufferType(), &OriginalCodedFrameBuffer);
        SE_ASSERT(OriginalCodedFrameBuffer != NULL);

        PlayerSequenceNumber_t   *SequenceNumberStructure;
        OriginalCodedFrameBuffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataSequenceNumberType, (void **) &SequenceNumberStructure);
        SE_ASSERT(SequenceNumberStructure != NULL);

        return SequenceNumberStructure;
    }

    void ReleaseDecodeBuffer(Buffer_t Buffer)
    {
        mStream->GetCodec()->ReleaseDecodeBuffer(Buffer);
    }

    void DelayForManifestationThrottling(Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(DecodeToManifestEdge_c);
};

#endif // H_DECODE_TO_MANIFEST
