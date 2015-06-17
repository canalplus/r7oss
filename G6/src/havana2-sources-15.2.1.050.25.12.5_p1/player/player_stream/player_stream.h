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

#ifndef PLAYER_STREAM_H
#define PLAYER_STREAM_H

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "player_stream_interface.h"
#include "stm_pixel_capture.h"
#include "capture_device_priv.h"
#include "message.h"

#define group_event group_api | group_player

class PlayerStream_c : public PlayerStreamInterface_c
{
public:
    PlayerStream_c(Player_Generic_t         player,
                   PlayerPlayback_t         Playback,
                   HavanaStream_t           HavanaStream,
                   PlayerStreamType_t       StreamType,
                   unsigned int             InstanceId,
                   DecodeBufferManager_t    DecodeBufferManager,
                   Collator_t               Collator,
                   UserDataSource_t         UserDataSender);

    PlayerStatus_t  FinalizeInit(Collator_t                  Collator,
                                 FrameParser_t               FrameParser,
                                 Codec_t                     Codec,
                                 OutputTimer_t               OutputTimer,
                                 ManifestationCoordinator_t  ManifestationCoordinator,
                                 BufferManager_t             BufferManager,
                                 bool                        SignalEvent,
                                 void                       *EventUserData);

    virtual ~PlayerStream_c(void);

    void                                ApplyDecoderConfig(unsigned int Config);

    virtual Player_t                    GetPlayer() { return mPlayer; }
    virtual PlayerPlayback_t            GetPlayback() { return mPlayback; }
    virtual BufferType_t                GetCodedFrameBufferType() {return mCodedFrameBufferType; }
    virtual HavanaStream_t              GetHavanaStream() { return mHavanaStream; }

    virtual DecodeBufferManager_t       GetDecodeBufferManager() { return mDecodeBufferManager; }
    virtual Collator_t                  GetCollator() { return mCollator; }
    virtual FrameParser_t               GetFrameParser();
    virtual Codec_t                     GetCodec();
    virtual ManifestationCoordinator_t  GetManifestationCoordinator();
    virtual OutputTimer_t               GetOutputTimer();
    virtual ES_Processor_c             *GetEsProcessor() { return mEsProcessor; }
    virtual PlayerStreamType_t          GetStreamType() { return mStreamType; }
    virtual unsigned int                GetInstanceId() { return mInstanceId; }
    virtual Message_c                  *GetMessenger() { return &mMessenger; }

    virtual unsigned int                GetNumberOfDecodeBuffers() const { return mNumberOfDecodeBuffers; }

    virtual const PlayerStreamStatistics_t     &GetStatistics();
    virtual void                                ResetStatistics();
    virtual PlayerStreamStatistics_t           &Statistics() { return mStatistics; }

    virtual const PlayerStreamAttributes_t     &GetAttributes();
    virtual void                                ResetAttributes();
    virtual PlayerStreamAttributes_t           &Attributes() { return mAttributes; }

    virtual void              SetSpeed(Rational_t Speed, PlayDirection_t Direction);

    virtual PlayerStatus_t              MarkUnPlayable();
    virtual void                        SetUnPlayable() { mUnPlayable = true; }
    virtual bool                        IsUnPlayable() const { return mUnPlayable; }

    virtual void                        Terminate() {   mTerminating = true; }
    virtual bool                        IsTerminating() const { return mTerminating; }

    virtual void                        CodecSwitchingOn() { mCodecSwitching = true; }
    virtual void                        CodecSwitchingOff() { mCodecSwitching = false; }
    virtual bool                        IsCodecSwitching() const { return mCodecSwitching; }

    virtual PlayerStatus_t              GetElementaryBufferLevel(stm_se_ctrl_play_stream_elementary_buffer_level_t *ElementaryBufferLevel);

    virtual PlayerStatus_t              EndOfStream();

    virtual PlayerStatus_t              InputJump(bool SurplusData, bool ContinuousReverseJump, bool FromDiscontinuityControl);

    virtual PlayerStatus_t              Drain(bool NonBlocking, bool SignalEvent, void *EventUserData,
                                              PlayerPolicy_t PlayoutPolicy, bool ParseAllFrames,
                                              unsigned long long *pDrainSequenceNumber);

    virtual PlayerStatus_t              Step();

    virtual PlayerStatus_t              Switch(Collator_t Collator, FrameParser_t FrameParser,
                                               Codec_t Codec, bool NonBlocking);

    virtual PlayerStatus_t              SetAlarm(stm_se_play_stream_alarm_t alarm, bool  enable, void *value);

    virtual PlayerStatus_t              ResetDiscardTrigger(void);
    virtual PlayerStatus_t              SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    virtual BufferPool_t GetCodedFrameBufferPool(unsigned int         *MaximumCodedFrameSize = NULL);

    virtual PlayerStatus_t      LowPowerEnter();
    virtual PlayerStatus_t      LowPowerExit();
    virtual bool                IsLowPowerState() { return mIsLowPowerState; }

    virtual ActiveEdgeInterface_c      *GetParseToDecodeEdge() { return (ActiveEdgeInterface_c *) ParseToDecodeEdge; }

    virtual void                SetLowPowerEnterEvent() { OS_SetEventInterruptible(&mLowPowerEnterEvent); }
    virtual void                WaitForLowPowerExitEvent()
    {
        OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&mLowPowerExitEvent, OS_INFINITE);
        if (WaitStatus == OS_INTERRUPTED)
        {
            SE_INFO(group_player, "wait for LP exit interrupted; LowPowerExitEvent:%d\n", mLowPowerExitEvent.Valid);
        }
    }


    PlayerStatus_t              CreateEsProcessor(void);
    void                        DestroyEsProcessor(void);

    void                        SwitchCollator();

    PlayerStatus_t              GetDecodeBuffer(stm_pixel_capture_format_t    Format,
                                                unsigned int                  Width,
                                                unsigned int                  Height,
                                                Buffer_t                      *Buffer,
                                                uint32_t                      *LumaAddress,
                                                uint32_t                      *ChromaOffset,
                                                unsigned int                  *Stride,
                                                bool                          NonBlockingInCaseOfFailure);
    PlayerStatus_t              ReturnDecodeBuffer(Buffer_t Buffer);

    PlayerStatus_t              InsertMarkerFrame(markerType_t markerType, unsigned long long *sequenceNumber);
    unsigned int                WaitForDrainCompletion(bool Discard);

    virtual PlayerStatus_t      SignalEvent(struct PlayerEventRecord_s *PlayerEvent);

    // Virtual to allow mocking and consequently collator unit-testing.
    virtual CollateTimeFrameParserInterface_c *GetCollateTimeFrameParser();

    // See mManifestorSharedLock comment.
    void EnterManifestorSharedSection()     { OS_LockRead(&mManifestorSharedLock); }
    void ExitManifestorSharedSection()      { OS_UnLockRead(&mManifestorSharedLock); }
    void EnterManifestorExclusiveSection()  { OS_LockWrite(&mManifestorSharedLock); }
    void ExitManifestorExclusiveSection()   { OS_UnLockWrite(&mManifestorSharedLock); }

    PlayerStream_t              Next;


    CollateToParseEdge_c       *CollateToParseEdge;
    ParseToDecodeEdge_c        *ParseToDecodeEdge;
    DecodeToManifestEdge_c     *DecodeToManifestEdge;
    PostManifestEdge_c         *PostManifestEdge;

    UserDataSource_t            UserDataSender;

    unsigned int                ProcessRunningCount;
    OS_Mutex_t                  StartStopLock;
    OS_Event_t                  StartStopEvent;

    bool                        BuffersComingOutOfManifestation;

    bool                        SwitchStreamInProgress;

    OS_Event_t            SwitchStreamLastOneOutOfTheCodec;
    Collator_t            SwitchingToCollator;
    FrameParser_t         SwitchingToFrameParser;
    Codec_t               SwitchingToCodec;
    OS_Semaphore_t        mSemaphoreStreamSwitchCollator;
    OS_Semaphore_t        mSemaphoreStreamSwitchFrameParser;
    OS_Semaphore_t        mSemaphoreStreamSwitchCodec;
    OS_Semaphore_t        mSemaphoreStreamSwitchOutputTimer;
    OS_Semaphore_t        mSemaphoreStreamSwitchComplete;

    bool                  mStep;
    OS_Event_t            SingleStepMayHaveHappened;

    unsigned int          CodedFrameCount;
    unsigned int          CodedMemorySize;
    unsigned int          CodedFrameMaximumSize;
    char                  CodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    allocator_device_t    CodedFrameMemoryDevice;
    BufferType_t          TranscodedFrameBufferType;
    BufferType_t          CompressedFrameBufferType;

    BufferType_t          DecodeBufferType;
    BufferPool_t          DecodeBufferPool;

    BufferType_t          AuxFrameBufferType;

    bool                  DisplayDiscontinuity;

    PlayerPolicyState_t   PolicyRecord;

    PlayerControslStorageState_t ControlsRecord;

    //
    // Presentation interval values
    //

    unsigned long long        RequestedPresentationIntervalStartNormalizedTime;
    unsigned long long        RequestedPresentationIntervalEndNormalizedTime;

    //
    // Accumulated list of coded data buffers that were not decoded,
    // passed from parse->decode to decode->manifest, used to patch
    // holes in the display frame indices (avoids the latency of waiting
    // for max decodes out of order before correcting for missing indices).
    //

    unsigned int                InsertionsIntoNonDecodedBuffers;
    unsigned int                RemovalsFromNonDecodedBuffers;
    unsigned int                DisplayIndicesCollapse;
    PlayerBufferRecord_t        NonDecodedBuffers[PLAYER_MAX_DISCARDED_FRAMES];
    OS_Mutex_t                  NonDecodedBuffersLock;

    unsigned long long          DecodeCommenceTime;

    //
    // Useful counts/debugging data (added/removed at will
    //

    unsigned int          FramesToManifestorCount;
    unsigned int          FramesFromManifestorCount;

#ifdef LOWMEMORYBANDWIDTH
    virtual stm_se_stream_encoding_t    GetEncoding() { return mEncoding; }
    virtual void                        SetEncoding(stm_se_stream_encoding_t e) {  mEncoding = e; }
#endif

private:
    PlayerStatus_t      CreateCodedFrameBufferPool(BufferManager_t BufferManager);
    BufferFormat_t      ConvertVibeBufferFormatToPlayerBufferFormat(stm_pixel_capture_format_t Format);
    void                DestroyCodedFrameBufferPool(void);
    virtual PlayerStatus_t          CheckEvent(struct PlayerEventRecord_s *PlayerEvent, stm_se_play_stream_msg_t *streamMsg, stm_event_t *streamEvtMngt);

    Player_Generic_t                mPlayer;
    PlayerPlayback_t                mPlayback;
    BufferType_t                    mCodedFrameBufferType;
    HavanaStream_t                  mHavanaStream;
    DecodeBufferManager_t           mDecodeBufferManager;
    unsigned int                    mNumberOfDecodeBuffers;
    bool                            mUnPlayable;
    bool                            mTerminating;
    bool                            mCodecSwitching;
    PlayerStreamStatistics_t        mStatistics;
    PlayerStreamAttributes_t        mAttributes;
    BufferFormat_t                  mLastGetDecodeBufferFormat;
    Collator_t                      mCollator;
    ES_Processor_c                 *mEsProcessor;
    PlayerStreamType_t              mStreamType;
    OS_Event_t                      mDrained;
    unsigned int                    mInstanceId;
    //
    // Low power data
    //
    bool                            mIsLowPowerState;    // HPS/CPS: indicates when SE is in low power state
    OS_Event_t                      mLowPowerEnterEvent; // HPS/CPS: event used to synchronize with the ParseToDecode thread on low power entry
    OS_Event_t                      mLowPowerExitEvent;  // HPS/CPS: event used to synchronize with the ParseToDecode thread on low power exit

    Message_c                       mMessenger;
    BufferPool_t                    mCodedFrameBufferPool;

    // Coarse-grained shared/exclusive section preventing addition and removal
    // of manifestors by client threads while the the decode-to-manifest or
    // post-manifest edge threads run.
    // TODO(theryn): The manifestation code is race-prone so eventually we will
    // rewrite it but as a stop-gap measure we address the most frequent race
    // which occurs when a manifestor is removed (bug 60729), typically during
    // main-PIP swap.  We bend the read-write lock concept because edges both
    // read and write the manifestation state but this is for a good cause as
    // allowing both edges to access concurrently the manifestation state
    // minimizes changes to the overall timing of events and is good-enough in
    // practice probably because there are implicit hand-over of data between
    // threads minimizing the number of races.
    OS_RwLock_t                     mManifestorSharedLock;

    Rational_t                      mSpeed;
    PlayDirection_t                 mDirection;

#ifdef LOWMEMORYBANDWIDTH
    stm_se_stream_encoding_t        mEncoding;
#endif

    DISALLOW_COPY_AND_ASSIGN(PlayerStream_c);
};

#endif
