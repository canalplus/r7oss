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

#include "player_stream.h"
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"
#include "es_processor_base.h"

#undef TRACE_TAG
#define TRACE_TAG "PlayerStream_c"

PlayerStreamInterface_c::~PlayerStreamInterface_c() {}

PlayerStream_c::PlayerStream_c(
    Player_Generic_t        player,
    PlayerPlayback_t        Playback,
    HavanaStream_t          HavanaStream,
    PlayerStreamType_t      streamType,
    unsigned int            InstanceId,
    DecodeBufferManager_t   decodeBufferManager,
    Collator_t              Collator,
    UserDataSource_t        UserDataSender)
    : Next(NULL)
    , CollateToParseEdge(NULL)
    , ParseToDecodeEdge(NULL)
    , DecodeToManifestEdge(NULL)
    , PostManifestEdge(NULL)
    , UserDataSender(UserDataSender)
    , ProcessRunningCount(0)
    , StartStopLock()
    , StartStopEvent()
    , BuffersComingOutOfManifestation(false)
    , SwitchStreamInProgress(false)
    , SwitchStreamLastOneOutOfTheCodec()
    , SwitchingToCollator(NULL)
    , SwitchingToFrameParser(NULL)
    , SwitchingToCodec(NULL)
    , mSemaphoreStreamSwitchCollator()
    , mSemaphoreStreamSwitchFrameParser()
    , mSemaphoreStreamSwitchCodec()
    , mSemaphoreStreamSwitchOutputTimer()
    , mSemaphoreStreamSwitchComplete()
    , mStep(false)
    , SingleStepMayHaveHappened()
    , CodedFrameCount(0)
    , CodedMemorySize(0)
    , CodedFrameMaximumSize(0)
    , CodedMemoryPartitionName()
    , CodedFrameMemoryDevice(NULL)
    , TranscodedFrameBufferType(0)
    , CompressedFrameBufferType(0)
    , DecodeBufferType(0)
    , DecodeBufferPool(NULL)
    , AuxFrameBufferType(0)
    , DisplayDiscontinuity(false)
    , PolicyRecord()
    , ControlsRecord()
    , RequestedPresentationIntervalStartNormalizedTime(0)
    , RequestedPresentationIntervalEndNormalizedTime(0)
    , InsertionsIntoNonDecodedBuffers(0)
    , RemovalsFromNonDecodedBuffers(0)
    , DisplayIndicesCollapse(0)
    , NonDecodedBuffers()
    , NonDecodedBuffersLock()
    , DecodeCommenceTime(0)
    , FramesToManifestorCount(0)
    , FramesFromManifestorCount(0)
    , mPlayer(player)
    , mPlayback(Playback)
    , mCodedFrameBufferType(0)
    , mHavanaStream(HavanaStream)
    , mDecodeBufferManager(decodeBufferManager)
    , mNumberOfDecodeBuffers(0)
    , mUnPlayable(false)
    , mTerminating(false)
    , mCodecSwitching(false)
    , mStatistics()
    , mAttributes()
    , mLastGetDecodeBufferFormat(FormatUnknown)
    , mCollator(Collator)
    , mEsProcessor(NULL)
    , mStreamType(streamType)
    , mDrained()
    , mInstanceId(InstanceId)
    , mIsLowPowerState(false)
    , mLowPowerEnterEvent()
    , mLowPowerExitEvent()
    , mMessenger()
    , mCodedFrameBufferPool(NULL)
    , mManifestorSharedLock()
    , mSpeed(1)
    , mDirection(PlayForward)
#ifdef LOWMEMORYBANDWIDTH
    , mEncoding(STM_SE_STREAM_ENCODING_AUDIO_NONE)
#endif
{
    OS_InitializeMutex(&StartStopLock);
    OS_InitializeMutex(&NonDecodedBuffersLock);
    OS_InitializeRwLock(&mManifestorSharedLock);

    OS_InitializeEvent(&StartStopEvent);
    OS_InitializeEvent(&mDrained);
    OS_InitializeEvent(&SwitchStreamLastOneOutOfTheCodec);
    OS_InitializeEvent(&SingleStepMayHaveHappened);
    OS_InitializeEvent(&mLowPowerEnterEvent);
    OS_InitializeEvent(&mLowPowerExitEvent);

    OS_SemaphoreInitialize(&mSemaphoreStreamSwitchCollator, 0);
    OS_SemaphoreInitialize(&mSemaphoreStreamSwitchFrameParser, 0);
    OS_SemaphoreInitialize(&mSemaphoreStreamSwitchCodec, 0);
    OS_SemaphoreInitialize(&mSemaphoreStreamSwitchOutputTimer, 0);
    OS_SemaphoreInitialize(&mSemaphoreStreamSwitchComplete, 0);
}

#define DEFAULT_RING_SIZE       64

static int ThreadDescIds[4][4] =
{
    { SE_TASK_OTHER, SE_TASK_OTHER, SE_TASK_OTHER, SE_TASK_OTHER},
    { SE_TASK_AUDIO_CTOP, SE_TASK_AUDIO_PTOD, SE_TASK_AUDIO_DTOM, SE_TASK_AUDIO_MPost },
    { SE_TASK_VIDEO_CTOP, SE_TASK_VIDEO_PTOD, SE_TASK_VIDEO_DTOM, SE_TASK_VIDEO_MPost },
    { SE_TASK_OTHER, SE_TASK_OTHER, SE_TASK_OTHER, SE_TASK_OTHER},
};

// FinalizeInit
// for non trivial inits to be performed after ctor
PlayerStatus_t   PlayerStream_c::FinalizeInit(Collator_t                    Collator,
                                              FrameParser_t                 FrameParser,
                                              Codec_t                       Codec,
                                              OutputTimer_t                 OutputTimer,
                                              ManifestationCoordinator_t    ManifestationCoordinator,
                                              BufferManager_t               BufferManager,
                                              bool                          SignalEvent,
                                              void                         *EventUserData)
{
    PlayerStatus_t            Status;
    OS_Status_t               OSStatus;
    unsigned int              Count;
    PlayerEventRecord_t       Event;
    int                       MemProfilePolicy;
    unsigned int              CurrentProcessRunningCount;
    unsigned int              ProcessCreatedCount;

    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p StreamType %d Collator 0x%p FrameParser 0x%p Codec 0x%p "
             "OutputTimer 0x%p ManifestationCoordinator 0x%p\n",
             mPlayback, this, mStreamType, Collator, FrameParser, Codec, OutputTimer, ManifestationCoordinator);

    // Message subscription init
    MessageStatus_t MessageStatus = mMessenger.Init();
    if (MessageStatus != MessageNoError)
    {
        return PlayerError;
    }

    Status = CreateEsProcessor();
    if (Status != PlayerNoError)
    {
        SE_ERROR("issue when creating EsProcessor\n");
        return Status;
    }

    // Memorize that we just create a stream so that when
    // the first picture is send to display,
    // we can inform VIBE that there is a temporal discontinuity and FVDP queue needs to be flushed.
    DisplayDiscontinuity     = true;

    RequestedPresentationIntervalStartNormalizedTime = mPlayback->RequestedPresentationIntervalStartNormalizedTime;
    RequestedPresentationIntervalEndNormalizedTime   = mPlayback->RequestedPresentationIntervalEndNormalizedTime;

    ResetStatistics();
    ResetAttributes();

    MemProfilePolicy = mPlayer->PolicyValue(mPlayback,
                                            PlayerAllStreams, PolicyVideoPlayStreamMemoryProfile);
    if ((MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfile4K2K) ||
        (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileUHD))
    {
        mPlayback->VideoCodedFrameMaximumSize = PLAYER_VIDEO_DEFAULT_4K2K_CODED_FRAME_MAXIMUM_SIZE;
        mPlayback->VideoCodedMemorySize = PLAYER_VIDEO_DEFAULT_4K2K_CODED_MEMORY_SIZE;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileHD720p)
    {
        mPlayback->VideoCodedFrameMaximumSize = PLAYER_VIDEO_DEFAULT_720P_CODED_FRAME_MAXIMUM_SIZE;
        mPlayback->VideoCodedMemorySize = PLAYER_VIDEO_DEFAULT_720P_CODED_MEMORY_SIZE;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileSD)
    {
        mPlayback->VideoCodedFrameMaximumSize = PLAYER_VIDEO_DEFAULT_SD_CODED_FRAME_MAXIMUM_SIZE;
        mPlayback->VideoCodedMemorySize = PLAYER_VIDEO_DEFAULT_SD_CODED_MEMORY_SIZE;
    }
    else
    {
        mPlayback->VideoCodedFrameMaximumSize = PLAYER_VIDEO_DEFAULT_HD_CODED_FRAME_MAXIMUM_SIZE;
        mPlayback->VideoCodedMemorySize = PLAYER_VIDEO_DEFAULT_HD_CODED_MEMORY_SIZE;
    }
    //
    // Get the coded frame parameters
    //
    switch (mStreamType)
    {
    case StreamTypeAudio:
        mCodedFrameBufferType   = mPlayer->BufferCodedFrameBufferType;
        CodedFrameCount         = mPlayback->AudioCodedFrameCount;
        CodedMemorySize         = mPlayback->AudioCodedMemorySize;
        CodedFrameMaximumSize   = mPlayback->AudioCodedFrameMaximumSize;
        memcpy(CodedMemoryPartitionName, mPlayback->AudioCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        break;

    case StreamTypeVideo:
        mCodedFrameBufferType   = mPlayer->BufferCodedFrameBufferType;
        CodedFrameCount         = mPlayback->VideoCodedFrameCount;
        CodedMemorySize         = mPlayback->VideoCodedMemorySize;
        CodedFrameMaximumSize   = mPlayback->VideoCodedFrameMaximumSize;
        memcpy(CodedMemoryPartitionName, mPlayback->VideoCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        break;

    case StreamTypeOther:
        mCodedFrameBufferType   = mPlayer->BufferCodedFrameBufferType;
        CodedFrameCount         = mPlayback->OtherCodedFrameCount;
        CodedMemorySize         = mPlayback->OtherCodedMemorySize;
        CodedFrameMaximumSize   = mPlayback->OtherCodedFrameMaximumSize;
        memcpy(CodedMemoryPartitionName, mPlayback->OtherCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE);
        break;

    default:
        break;
    }

    //
    // Register the player to the child classes, they will require it early,
    // these function do no work, so cannot return a fail status.
    //
    Collator->RegisterPlayer(mPlayer, mPlayback, this);
    FrameParser->RegisterPlayer(mPlayer, mPlayback, this);
    Codec->RegisterPlayer(mPlayer, mPlayback, this);
    OutputTimer->RegisterPlayer(mPlayer, mPlayback, this);
    mDecodeBufferManager->RegisterPlayer(mPlayer, mPlayback, this);
    ManifestationCoordinator->RegisterPlayer(mPlayer, mPlayback, this);

    //
    // Create the buffer pools, and attach the sequence numbers to them.
    //

    Status      = CreateCodedFrameBufferPool(BufferManager);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to create the coded buffer pool\n");
        return Status;
    }

    BufferStatus_t BufferStatus = mCodedFrameBufferPool->AttachMetaData(mPlayer->MetaDataSequenceNumberType);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach sequence numbers to the coded buffer pool\n");
        return PlayerError;
    }

    Status = mDecodeBufferManager->GetDecodeBufferPool(&DecodeBufferPool);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to get the decode buffer pool\n");
        return Status;
    }

    DecodeBufferPool->GetType(&DecodeBufferType);
    DecodeBufferPool->GetPoolUsage(&Count, NULL, NULL, NULL, NULL);

    if (Count == 0)
    {
        Count = DEFAULT_RING_SIZE;
    }

    mNumberOfDecodeBuffers = Count;
    if (mNumberOfDecodeBuffers > MAX_DECODE_BUFFERS)
    {
        SE_ERROR("Too many decode buffers - Implementation constant range error\n");
        return PlayerImplementationError;
    }

    //
    // Create the tasks that pass data between components,
    // and provide them with a context in which to operate.
    // NOTE Since we are unsure about the startup, we use a
    // simple delay to shut down the threads if startup is not
    // as expected.
    //
    OS_ResetEvent(&StartStopEvent);
    ProcessCreatedCount = 0;

    CollateToParseEdge = new CollateToParseEdge_c(ThreadDescIds[mStreamType][0], this,
                                                  CodedFrameCount + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS, FrameParser);
    if (CollateToParseEdge) { OSStatus = CollateToParseEdge->Start(); }
    else { OSStatus = OS_ERROR; }

    if (OSStatus == OS_NO_ERROR)
    {
        ProcessCreatedCount ++;

        ParseToDecodeEdge = new ParseToDecodeEdge_c(ThreadDescIds[mStreamType][1], this,
                                                    CodedFrameCount + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS, Codec);
        if (ParseToDecodeEdge) { OSStatus = ParseToDecodeEdge->Start(); }
        else { OSStatus = OS_ERROR; }
    }
    if (OSStatus == OS_NO_ERROR)
    {
        ProcessCreatedCount ++;

        DecodeToManifestEdge = new DecodeToManifestEdge_c(ThreadDescIds[mStreamType][2], this,
                                                          mNumberOfDecodeBuffers + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS,
                                                          ManifestationCoordinator, OutputTimer);
        if (DecodeToManifestEdge) { OSStatus = DecodeToManifestEdge->Start(); }
        else { OSStatus = OS_ERROR; }
    }
    if (OSStatus == OS_NO_ERROR)
    {
        ProcessCreatedCount ++;

        PostManifestEdge = new PostManifestEdge_c(ThreadDescIds[mStreamType][3], this,
                                                  mNumberOfDecodeBuffers + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS);
        if (PostManifestEdge) { OSStatus = PostManifestEdge->Start();}
        else { OSStatus = OS_ERROR; }
    }
    if (OSStatus == OS_NO_ERROR)
    {
        ProcessCreatedCount ++;
    }

    // Wait for all threads to be running
    CurrentProcessRunningCount = 0;
    while (CurrentProcessRunningCount != ProcessCreatedCount)
    {
        OS_Status_t WaitStatus = OS_WaitForEventAuto(&StartStopEvent, PLAYER_MAX_EVENT_WAIT);

        OS_LockMutex(&StartStopLock);
        OS_ResetEvent(&StartStopEvent);
        CurrentProcessRunningCount = ProcessRunningCount;
        OS_UnLockMutex(&StartStopLock);

        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for Stream processes to run. Player 0x%p ProcessRunningCount %d (expected %d) status:%d\n",
                       mPlayer, CurrentProcessRunningCount, ProcessCreatedCount, WaitStatus);
        }
    }

    // Terminate all threads if one of the creation had failed
    if (ProcessCreatedCount != PLAYER_STREAM_PROCESS_NB)
    {
        SE_ERROR("Failed to create all stream processes\n");
        return PlayerError;
    }

    //
    // Now exchange the appropriate information between the classes
    //

    Status      = GetEsProcessor()->Connect(CollateToParseEdge->GetInputPort());
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to connect CollateToParseEdge InputPort with EsProcessor\n");
        return Status;
    }

    Status      = Collator->Connect(static_cast<Port_c *>(GetEsProcessor()));
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to connect EsProcessor with Collator\n");
        return Status;
    }

    Status      = FrameParser->Connect(ParseToDecodeEdge->GetInputPort());
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to register BufferRing with Frame Parser\n");
        return Status;
    }

    Status      = Codec->Connect(DecodeToManifestEdge->GetInputPort());
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to connect with Codec\n");
        return Status;
    }

    Status      = OutputTimer->RegisterOutputCoordinator(mPlayback->OutputCoordinator);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to register Output Coordinator with Output Timer\n");
        return Status;
    }

    Status  = ManifestationCoordinator->Connect(PostManifestEdge->GetInputPort());
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to connect with Manifestor\n");
        return Status;
    }

    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p added\n", mPlayback, this);
    return PlayerNoError;
}

PlayerStream_c::~PlayerStream_c(void)
{
    unsigned int            CurrentProcessRunningCount;

    SE_DEBUG(group_player, "Playback 0x%p this 0x%p\n", GetPlayback(), this);

    //
    // Shut down the processes
    //
    if (PostManifestEdge)
    {
        PostManifestEdge->DiscardUntilMarkerFrame();
    }
    if (DecodeToManifestEdge)
    {
        DecodeToManifestEdge->DiscardUntilMarkerFrame();
    }
    if (ParseToDecodeEdge)
    {
        ParseToDecodeEdge->DiscardUntilMarkerFrame();
    }
    if (CollateToParseEdge)
    {
        CollateToParseEdge->DiscardUntilMarkerFrame();
    }
    if (GetCollator())
    {
        GetCollator()->DiscardAccumulatedData();
        GetCollator()->Halt();
    }
    if (GetFrameParser())
    {
        GetFrameParser()->Halt();
    }
    if (GetCodec())
    {
        GetCodec()->OutputPartialDecodeBuffers();
        GetCodec()->ReleaseReferenceFrame(CODEC_RELEASE_ALL);
        GetCodec()->Halt();
        // following is done to send NULL values (updated value) to respective codecs bug 57576
        // so that already deleted/removed Stream pointer is not dereferenced
        GetCodec()->RegisterPlayer(NULL, NULL, NULL);
    }
    if (GetOutputTimer())
    {
        GetOutputTimer()->Halt();
    }
    if (GetManifestationCoordinator())
    {
        GetManifestationCoordinator()->QueueNullManifestation();
        GetManifestationCoordinator()->Halt();
    }
    if (GetDecodeBufferManager())
    {
        GetDecodeBufferManager()->Halt();
    }

    // Ask threads to terminate
    OS_LockMutex(&StartStopLock);
    OS_ResetEvent(&StartStopEvent);
    CurrentProcessRunningCount = ProcessRunningCount;
    OS_UnLockMutex(&StartStopLock);

    // Write memory barrier: wmb_for_Stream_Terminating coupled with: rmb_for_Stream_Terminating
    OS_Smp_Mb();
    Terminate();

    if (CollateToParseEdge)
    {
        CollateToParseEdge->Stop();
    }
    if (ParseToDecodeEdge)
    {
        ParseToDecodeEdge->Stop();
    }
    if (DecodeToManifestEdge)
    {
        DecodeToManifestEdge->Stop();
    }
    if (PostManifestEdge)
    {
        PostManifestEdge->Stop();
    }

    // Wait for all threads termination
    while (CurrentProcessRunningCount != 0)
    {
        OS_Status_t WaitStatus = OS_WaitForEventAuto(&StartStopEvent, 2 * PLAYER_MAX_EVENT_WAIT);

        OS_LockMutex(&StartStopLock);
        OS_ResetEvent(&StartStopEvent);
        CurrentProcessRunningCount = ProcessRunningCount;
        OS_UnLockMutex(&StartStopLock);

        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for stream processes to terminate - mPlayer 0x%p ProcessRunningCount %d\n",
                       mPlayer, CurrentProcessRunningCount);
        }
    }

    GetPlayback()->UnregisterStream(this);

    //
    // Now reset the individual classes
    //
    if (GetOutputTimer())
    {
        GetOutputTimer()->Reset();
    }
    if (GetManifestationCoordinator())
    {
        GetManifestationCoordinator()->Reset();
    }
    if (GetDecodeBufferManager())
    {
        GetDecodeBufferManager()->Reset();
    }

    DestroyEsProcessor();

    if (CollateToParseEdge)
    {
        delete CollateToParseEdge;
        CollateToParseEdge = NULL;
    }

    if (ParseToDecodeEdge)
    {
        delete ParseToDecodeEdge;
        ParseToDecodeEdge = NULL;
    }

    if (DecodeToManifestEdge)
    {
        delete DecodeToManifestEdge;
        DecodeToManifestEdge = NULL;
    }

    if (PostManifestEdge)
    {
        delete PostManifestEdge;
        PostManifestEdge = NULL;
    }

    DestroyCodedFrameBufferPool();

    OS_TerminateMutex(&StartStopLock);
    OS_TerminateMutex(&NonDecodedBuffersLock);
    OS_TerminateRwLock(&mManifestorSharedLock);

    OS_TerminateEvent(&StartStopEvent);
    OS_TerminateEvent(&mDrained);
    OS_TerminateEvent(&SwitchStreamLastOneOutOfTheCodec);
    OS_TerminateEvent(&SingleStepMayHaveHappened);
    OS_TerminateEvent(&mLowPowerEnterEvent);
    OS_TerminateEvent(&mLowPowerExitEvent);

    OS_SemaphoreTerminate(&mSemaphoreStreamSwitchCollator);
    OS_SemaphoreTerminate(&mSemaphoreStreamSwitchFrameParser);
    OS_SemaphoreTerminate(&mSemaphoreStreamSwitchCodec);
    OS_SemaphoreTerminate(&mSemaphoreStreamSwitchOutputTimer);
    OS_SemaphoreTerminate(&mSemaphoreStreamSwitchComplete);
}

PlayerStatus_t PlayerStream_c::CreateCodedFrameBufferPool(BufferManager_t BufferManager)
{
    allocator_status_t AllocStatus = PartitionAllocatorOpen(&CodedFrameMemoryDevice,
                                                            CodedMemoryPartitionName, CodedMemorySize, MEMORY_DEFAULT_ACCESS);
    if (AllocStatus != allocator_ok)
    {
        SE_ERROR("Stream 0x%p Failed to allocate memory Status %d\n", this, AllocStatus);
        return PlayerInsufficientMemory;
    }

    // TODO(theryn): CreatePool() expects a 3-element array even though only two
    // slots are initialized.  For the short term, set the 3rd slot to a neutral value.
    // For the longer term, change CreatePool to expect a (more readable and
    // safer) struct.
    void *CodedFrameMemory[3];
    memset(CodedFrameMemory, 0, sizeof(CodedFrameMemory));
    CodedFrameMemory[CachedAddress]         = AllocatorUserAddress(CodedFrameMemoryDevice);
    CodedFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress(CodedFrameMemoryDevice);

    BufferStatus_t BufStatus  = BufferManager->CreatePool(&mCodedFrameBufferPool,
                                                          GetCodedFrameBufferType(), CodedFrameCount,
                                                          CodedMemorySize, CodedFrameMemory);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Stream 0x%p Failed to create the pool Status %d\n", this, BufStatus);
        DestroyCodedFrameBufferPool();
        return PlayerInsufficientMemory;
    }

    return PlayerNoError;
}

void PlayerStream_c::DestroyCodedFrameBufferPool(void)
{
    //
    // Delete the code frame buffer pool
    //
    if (mCodedFrameBufferPool != NULL)
    {
        BufferManager_t BufferManager;
        mPlayer->GetBufferManager(&BufferManager);
        BufferManager->DestroyPool(mCodedFrameBufferPool);
        mCodedFrameBufferPool  = NULL;
    }

    AllocatorClose(&CodedFrameMemoryDevice);
}

PlayerStatus_t PlayerStream_c::SetAlarm(stm_se_play_stream_alarm_t     alarm,
                                        bool  enable,
                                        void *value)
{
    PlayerStatus_t Status = PlayerNoError;
    switch (alarm)
    {
    case  STM_SE_PLAY_STREAM_ALARM_PARSED_PTS:
        mCollator->SetAlarmParsedPts();
        break;
    case  STM_SE_PLAY_STREAM_ALARM_PTS:
        stm_se_play_stream_pts_and_tolerance_t config;
        config = *(static_cast<stm_se_play_stream_pts_and_tolerance_t *>(value));
        Status = mEsProcessor->SetAlarm(enable, config);
        break;

    default:
        SE_ERROR("Unexpected alarm type received %d\n", alarm);
        return PlayerError;
    }

    return Status;

}

PlayerStatus_t PlayerStream_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    return mEsProcessor->SetDiscardTrigger(trigger);
}

PlayerStatus_t PlayerStream_c::ResetDiscardTrigger(void)
{
    return mEsProcessor->Reset();
}

FrameParser_t PlayerStream_c::GetFrameParser()
{
    if (CollateToParseEdge) { return CollateToParseEdge->GetFrameParser(); }
    else { return NULL; }
}

CollateTimeFrameParserInterface_c *PlayerStream_c::GetCollateTimeFrameParser()
{
    return static_cast<CollateTimeFrameParserInterface_c *>(GetFrameParser());
}

Codec_t PlayerStream_c::GetCodec()
{
    if (ParseToDecodeEdge) { return ParseToDecodeEdge->GetCodec(); }
    else { return NULL; }
}

ManifestationCoordinator_t PlayerStream_c::GetManifestationCoordinator()
{
    if (DecodeToManifestEdge) { return DecodeToManifestEdge->GetManifestationCoordinator(); }
    else { return NULL; }
}

OutputTimer_t PlayerStream_c::GetOutputTimer()
{
    if (DecodeToManifestEdge) { return DecodeToManifestEdge->GetOutputTimer(); }
    else { return NULL; }
}

BufferFormat_t PlayerStream_c::ConvertVibeBufferFormatToPlayerBufferFormat(
    stm_pixel_capture_format_t Format)
{
    switch (Format)
    {
    case STM_PIXEL_FORMAT_RGB_8B8B8B_SP:
        return FormatVideo888_RGB;

    case STM_PIXEL_FORMAT_YUV_8B8B8B_SP:
        return FormatVideo444_Raster;

    case STM_PIXEL_FORMAT_YUV_NV12:
        return FormatVideo420_Raster2B;

    case STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP:
        return FormatVideo422_Raster2B;

    case STM_PIXEL_FORMAT_YCbCr422R:
        return FormatVideo422_Raster;

    case STM_PIXEL_FORMAT_RGB565:
        return FormatVideo565_RGB;

    case STM_PIXEL_FORMAT_ARGB8888:
        return FormatVideo8888_ARGB;

    case STM_PIXEL_FORMAT_RGB_10B10B10B_SP:
    case STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP:
    case STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP:
        //
        // Defaulting to returning an error for 10-bit formats for
        // now until support is added
        // TODO Add support for 10-bit formats
        //
        SE_ERROR("Request for unsupported (10-bit) decode buffer format (%d)\n", Format);
        return FormatUnknown;

    default:
        SE_ERROR("Request for unsupported decode buffer format (%d)\n", Format);
        return FormatUnknown;
    }
}

BufferPool_t   PlayerStream_c::GetCodedFrameBufferPool(unsigned int         *MaximumCodedFrameSize)
{
    if (MaximumCodedFrameSize != NULL)
    {
        *MaximumCodedFrameSize    = CodedFrameMaximumSize;
    }

    return mCodedFrameBufferPool;
}

PlayerStatus_t PlayerStream_c::GetDecodeBuffer(stm_pixel_capture_format_t    Format,
                                               unsigned int                  Width,
                                               unsigned int                  Height,
                                               Buffer_t                      *Buffer,
                                               uint32_t                      *LumaAddress,
                                               uint32_t                      *ChromaOffset,
                                               unsigned int                  *Stride,
                                               bool                          NonBlockingInCaseOfFailure)
{
    DecodeBufferManagerStatus_t  Status;
    SE_VERBOSE(group_player, "\n");

    if ((Buffer == NULL) || (LumaAddress == NULL) ||
        (ChromaOffset == NULL) || (Stride == NULL))
    {
        SE_ERROR("Invalid data\n");
        return PlayerError;
    }

    BufferFormat_t BufferFormat = ConvertVibeBufferFormatToPlayerBufferFormat(Format);
    if (BufferFormat == FormatUnknown) { return PlayerError; }

    if (BufferFormat != mLastGetDecodeBufferFormat)
    {
        if (mLastGetDecodeBufferFormat != FormatUnknown)
        {
            mDecodeBufferManager->EnterStreamSwitch();
        }

        Status = mDecodeBufferManager->InitializeSimpleComponentList(BufferFormat);

        if (Status != DecodeBufferManagerNoError)
        {
            SE_ERROR("Unable to set buffer descriptor\n");
            return PlayerError;
        }

        mLastGetDecodeBufferFormat = BufferFormat;
    }

    // Request the buffer
    DecodeBufferRequest_t   BufferRequest;
    memset(&BufferRequest, 0, sizeof(DecodeBufferRequest_t));
    BufferRequest.DimensionCount  = 2;
    BufferRequest.Dimension[0]    = Width;
    BufferRequest.Dimension[1]    = Height;
    BufferRequest.NonBlockingInCaseOfFailure     = NonBlockingInCaseOfFailure;

    Status = mDecodeBufferManager->GetDecodeBuffer(&BufferRequest, Buffer);

    if (Status != DecodeBufferManagerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to get decode buffer\n", this);
        return PlayerError;
    }

    *LumaAddress      = (uint32_t)mDecodeBufferManager->ComponentBaseAddress(*Buffer, PrimaryManifestationComponent);
    *ChromaOffset     = mDecodeBufferManager->Chroma(*Buffer, PrimaryManifestationComponent) -
                        mDecodeBufferManager->Luma(*Buffer, PrimaryManifestationComponent);
    *Stride           = mDecodeBufferManager->ComponentStride(*Buffer, PrimaryManifestationComponent, 0, 0);

    // Change the ownership so that we can track the buffer through the system
    (*Buffer)->TransferOwnership(IdentifierExternal);

    return PlayerNoError;
}

PlayerStatus_t PlayerStream_c::ReturnDecodeBuffer(Buffer_t Buffer)
{
    BufferStatus_t      Status;

    Status = mDecodeBufferManager->ReleaseBuffer(Buffer, false);

    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p Failed to release buffer\n", this);
        return PlayerError;
    }

    return PlayerNoError;
}

PlayerStatus_t PlayerStream_c::CreateEsProcessor(void)
{
    PlayerStatus_t Status;

    if (mEsProcessor != NULL)
    {
        SE_ERROR("EsProcessor already created\n");
        return PlayerError;
    }

    mEsProcessor = new ES_Processor_Base_c();
    if (mEsProcessor == NULL)
    {
        SE_ERROR("no memory to create EsProcessor\n");
        return PlayerInsufficientMemory;
    }

    Status = mEsProcessor->FinalizeInit(this);
    if (Status != PlayerNoError)
    {
        SE_ERROR("EsProcessor initialization failed %d\n", Status);
        delete mEsProcessor;
        mEsProcessor = NULL;
    }

    return Status;
}

void PlayerStream_c::DestroyEsProcessor(void)
{
    delete mEsProcessor;
    mEsProcessor = NULL;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the collator
//

void    PlayerStream_c::SwitchCollator()
{
    SE_DEBUG(group_player, "Stream 0x%p\n", this);

    //
    // Halt the current collator;  will be destroyed during stream switch
    //
    mCollator->DiscardAccumulatedData();
    mCollator->Halt();

    //
    // Switch over to the new collator
    //
    SE_ASSERT((SwitchingToCollator != NULL) && (SwitchingToCollator != mCollator));
    mCollator = SwitchingToCollator;

    //
    // Initialize the collator
    //
    mCollator->RegisterPlayer(mPlayer, mPlayback, this);

    PlayerStatus_t Status = mCollator->Connect(static_cast<Port_c *>(GetEsProcessor()));
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to connect to EsProcessor\n", this);
    }

    OS_SemaphoreSignal(&this->mSemaphoreStreamSwitchCollator);

    SE_DEBUG(group_player, "Stream 0x%p completed\n", this);
}


PlayerStatus_t   PlayerStream_c::InsertMarkerFrame(markerType_t markerType, unsigned long long *sequenceNumber)
{
    //
    // Insert a marker frame into the processing ring
    //
    Buffer_t                  MarkerFrame;
    BufferStatus_t BufStatus = mCodedFrameBufferPool->GetBuffer(&MarkerFrame, IdentifierDrain, 0);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Unable to obtain a marker frame\n");
        return PlayerError;
    }

    CodedFrameParameters_t   *CodedFrameParameters;
    MarkerFrame->ObtainMetaDataReference(mPlayer->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    memset(CodedFrameParameters, 0, sizeof(CodedFrameParameters_t));

    PlayerSequenceNumber_t   *SequenceNumberStructure;
    MarkerFrame->ObtainMetaDataReference(mPlayer->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    SequenceNumberStructure->MarkerFrame = true;
    SequenceNumberStructure->Value       = CollateToParseEdge->GetMarkerFrameSequenceNumber();
    SequenceNumberStructure->MarkerType  = markerType;

    *sequenceNumber = SequenceNumberStructure->Value;

    //
    // Insert the marker into the flow
    //
    SE_DEBUG(group_player, "Stream 0x%p insert Marker Frame #%lld\n", this, SequenceNumberStructure->Value);
    CollateToParseEdge->Insert((uintptr_t)MarkerFrame);

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Insert a EOS frame marker
//

PlayerStatus_t   PlayerStream_c::EndOfStream()
{
    PlayerStatus_t            Status;
    stm_event_t               streamEvtMngt;
    unsigned long long        SequenceNumber;

    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p\n", mPlayback, this);

    Status = InsertMarkerFrame(EosMarker, &SequenceNumber);
    if (Status != PlayerNoError) { return Status; }

    //
    // Raise an event when end of stream
    //
    streamEvtMngt.event_id  = STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM;
    streamEvtMngt.object    = (stm_object_h) mHavanaStream;
    Status  = DecodeToManifestEdge->CallInSequence(SequenceTypeBeforeSequenceNumber, SequenceNumber, ManifestationCoordinatorFnEventMngrSignal, &streamEvtMngt);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to issue call to signal End of Stream\n");
        return Status;
    }

    DisplayDiscontinuity = true;

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

PlayerStatus_t   PlayerStream_c::Drain(
    bool                      NonBlocking,
    bool                      SignalEvent,
    void                     *EventUserData,
    PlayerPolicy_t            PlayoutPolicy,
    bool                      ParseAllFrames,
    unsigned long long       *pDrainSequenceNumber)
{
    PlayerStatus_t      Status;
    PlayerEventRecord_t Event;
    unsigned long long  PlayoutTime;
    unsigned long long  Delay;
    unsigned long long  DrainSequenceNumber;

    //
    // Read the appropriate policy
    //
    int PlayoutPolicyValue  = mPlayer->PolicyValue(mPlayback, this, PlayoutPolicy);
    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p NonBlocking %d PlayoutPolicyValue %d\n",
             mPlayback, this, NonBlocking, PlayoutPolicyValue);

    //
    // If we are to discard data in the drain, then perform the flushing
    //

    if (PlayoutPolicyValue == PolicyValueDiscard)
    {
        PostManifestEdge->DiscardUntilMarkerFrame();
        DecodeToManifestEdge->DiscardUntilMarkerFrame();
        ParseToDecodeEdge->DiscardUntilMarkerFrame();
        if (!ParseAllFrames) { CollateToParseEdge->DiscardUntilMarkerFrame(); }

        //ensure anyone waiting for buffers is unblocked
        DecodeToManifestEdge->ReleaseQueuedDecodeBuffers();
        Status  = GetCollator()->InputJump(true, false, false);
        ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnDiscardQueuedDecodes);
        ParseToDecodeEdge->CallInSequence(SequenceTypeImmediate, TIME_NOT_APPLICABLE, CodecFnOutputPartialDecodeBuffers);
        DecodeToManifestEdge->ReleaseQueuedDecodeBuffers();
        DisplayDiscontinuity = true;
    }
    else
    {
        Status  = GetCollator()->InputJump(false, false, false);   // This call will flush the already parsed input
    }

    //
    // Reset the event indicating draining and insert the marker into the flow
    //
    OS_ResetEvent(&mDrained);

    Status = InsertMarkerFrame(
                 (PlayoutPolicyValue == PolicyValueDiscard) ? DrainDiscardMarker : DrainPlayOutMarker, &DrainSequenceNumber);
    if (Status != PlayerNoError) { return Status; }

    if (pDrainSequenceNumber) { *pDrainSequenceNumber = DrainSequenceNumber; }


    //
    // Issue an in sequence synchronization reset
    //
    Status  = DecodeToManifestEdge->CallInSequence(SequenceTypeBeforeSequenceNumber, DrainSequenceNumber, OutputTimerFnResetTimeMapping, PlaybackContext);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to issue call to reset synchronization\n");
        return Status;
    }

    //
    // Do we want to raise a signal on drain completion
    //

    if (SignalEvent)
    {
        Event.Code              = EventStreamDrained;
        Event.Playback          = mPlayback;
        Event.Stream            = this;
        Event.PlaybackTime      = TIME_NOT_APPLICABLE;
        Event.UserData          = EventUserData;
        Status  = DecodeToManifestEdge->CallInSequence(SequenceTypeBeforeSequenceNumber, DrainSequenceNumber, ManifestationCoordinatorFnQueueEventSignal, &Event);

        if (Status != PlayerNoError)
        {
            SE_ERROR("Failed to issue call to signal drain completion\n");
            return Status;
        }
    }

    //
    // Queue the setting of the internal event, when the stream is drained
    // this allows us to commence multiple stream drains in a playback shutdown
    // and then block on completion of them all.
    //
    Status      = PostManifestEdge->CallInSequence(SequenceTypeAfterSequenceNumber, DrainSequenceNumber, OSFnSetEventOnPostManifestation, &mDrained);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to request OS_SetEvent\n");
        return Status;
    }

    //
    // Are we a blocking/non-blocking call
    //

    if (!NonBlocking)
    {
        Status  = WaitForDrainCompletion((PlayoutPolicyValue == PolicyValueDiscard));

        if (Status != PlayerNoError)
        {
            SE_ERROR("Playback 0x%p Stream 0x%p Failed to drain within allowed time (%d %d %d %d)\n",
                     mPlayback, this,
                     CollateToParseEdge->isDiscardingUntilMarkerFrame(), ParseToDecodeEdge->isDiscardingUntilMarkerFrame(),
                     DecodeToManifestEdge->isDiscardingUntilMarkerFrame(), PostManifestEdge->isDiscardingUntilMarkerFrame());
            return PlayerTimedOut;
        }

        //
        // If this was a playout drain, then check when the last
        // queued frame will complete and wait for it to do so.
        //

        if (PlayoutPolicyValue == PolicyValuePlayout)
        {
            //
            // The last frame will playout when the next frame could be displayed
            //
            Status  = GetManifestationCoordinator()->GetLastQueuedManifestationTime(&PlayoutTime);

            if ((Status == ManifestationCoordinatorNoError) && (mPlayback->mSpeed != 0))
            {
                Delay   = (PlayoutTime - OS_GetTimeInMicroSeconds()) / 1000;

                if (inrange(Delay, 1, (unsigned long long)Abs(RoundedLongLongIntegerPart(PLAYER_MAX_PLAYOUT_TIME / mPlayback->mSpeed))))
                {
                    SE_INFO(group_player, "Playback 0x%p Stream 0x%p Delay to manifest last frame is %lldms\n", mPlayback, this, Delay);
                    OS_SleepMilliSeconds((unsigned int)Delay);
                }
            }
        }
    }

    //
    SE_DEBUG(group_player, "Playback 0x%p Stream 0x%p completed\n", mPlayback, this);
    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

unsigned int   PlayerStream_c::WaitForDrainCompletion(bool Discard)
{
    unsigned int    IndividualWaitTime;
    OS_Status_t WaitStatus = OS_NO_ERROR;

    if (Discard)
    {
        WaitStatus = OS_WaitForEventAuto(&mDrained, PLAYER_MAX_DISCARD_DRAIN_TIME);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Stream 0x%p wait for drain completion timed out - skipping\n", this);
        }
    }
    else
    {
        if (mPlayback->mSpeed != 0)
        {
            IndividualWaitTime = Abs(RoundedIntegerPart(PLAYER_MAX_TIME_ON_DISPLAY / mPlayback->mSpeed));

            do
            {
                BuffersComingOutOfManifestation = false;
                WaitStatus = OS_WaitForEventAuto(&mDrained, IndividualWaitTime);
            }
            while ((WaitStatus == OS_TIMED_OUT) && BuffersComingOutOfManifestation);
        }
    }

    return (WaitStatus == OS_NO_ERROR) ? PlayerNoError : PlayerTimedOut;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch one stream in a playback for an alternative one
//

PlayerStatus_t   PlayerStream_c::Switch(Collator_t                Collator,
                                        FrameParser_t             FrameParser,
                                        Codec_t                   Codec,
                                        bool                      NonBlocking)
{
    SE_INFO(group_player, "Stream 0x%p NonBlocking %d\n", this, NonBlocking);
    SE_ASSERT(Collator && FrameParser && Codec);

    if (SwitchStreamInProgress)
    {
        SE_ERROR("Stream 0x%p Attempt to switch stream, when switch already in progress\n", this);
        return PlayerBusy;
    }

    // Initiate a non blocking stream drain, this generates and inserts a marker frame
    unsigned long long DrainSequenceNumber;
    PlayerStatus_t Status  = Drain(true, false, NULL, PolicyPlayoutOnSwitch, false, &DrainSequenceNumber);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to drain stream\n", this);
    }

    // Re-initialize clock recovery
    // We dont care about the time format used here as the clock recovery will be
    // re-initialized again when we get the first clock data point of the new stream
    // What we want here is to avoid a clock data point of the previous stream to be
    // used for the time mapping of the new stream
    mPlayer->ClockRecoveryInitialize(mPlayback, TimeFormatPts);

    // Insert the appropriate switch calls into the flow
    SwitchStreamInProgress  = true;
    SwitchingToCollator     = Collator;
    SwitchingToFrameParser  = FrameParser;
    SwitchingToCodec        = Codec;

    OS_ResetEvent(&SwitchStreamLastOneOutOfTheCodec);

    PostManifestEdge->CallInSequence(SequenceTypeBeforeSequenceNumber, (DrainSequenceNumber), PlayerFnSwitchComplete, this);

    ResetDiscardTrigger();

    SwitchCollator();

    CollateToParseEdge->CallInSequence(SequenceTypeAfterSequenceNumber, DrainSequenceNumber, PlayerFnSwitchFrameParser, this);
    ParseToDecodeEdge->CallInSequence(SequenceTypeAfterSequenceNumber, DrainSequenceNumber, PlayerFnSwitchCodec,       this);
    DecodeToManifestEdge->CallInSequence(SequenceTypeAfterSequenceNumber, DrainSequenceNumber, PlayerFnSwitchOutputTimer, this);

    // Reset mUnPlayable flag else we'll discard new data after the switch if previous stream was marked Unplayable
    mUnPlayable = false;

    // Are we blocking
    if (!NonBlocking)
    {
        OS_Status_t WaitStatus = OS_WaitForEventAuto(&mDrained, PLAYER_MAX_MARKER_TIME_THROUGH_CODEC);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_ERROR("Stream 0x%p Failed to drain old stream within allowed time (%d %d %d %d)\n", this,
                     CollateToParseEdge->isDiscardingUntilMarkerFrame(), ParseToDecodeEdge->isDiscardingUntilMarkerFrame(),
                     DecodeToManifestEdge->isDiscardingUntilMarkerFrame(), PostManifestEdge->isDiscardingUntilMarkerFrame());
            return PlayerTimedOut;
        }
    }

    SE_INFO(group_player, "Stream 0x%p completed\n", this);

    // Memorize that we just switch stream so that when
    // the first picture is send to display
    // we can inform VIBE that there is a temporal discontinuity and FVDP queue needs to be flushed.
    DisplayDiscontinuity = true;

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      A function to allow component to mark a stream as unplayable
//

PlayerStatus_t   PlayerStream_c::MarkUnPlayable()
{
    PlayerStatus_t        Status;
    PlayerEventRecord_t   Event;
    SE_DEBUG(group_player, "Stream 0x%p Ignoring call\n", this);
    //
    // First check if we should just ignore this call
    //
    int Ignore = mPlayer->PolicyValue(mPlayback, this, PolicyIgnoreStreamUnPlayableCalls);

    if (Ignore == PolicyValueApply)
    {
        SE_INFO(group_player, "Stream 0x%p Ignoring call\n", this);
        return PlayerNoError;
    }

    //
    // Mark the stream as unplayable
    //
    mUnPlayable  = true;

    //
    // raise the event to signal this to the user
    //
    Event.Code      = EventStreamUnPlayable;
    Event.Playback  = mPlayback;
    Event.Stream    = this;
    Event.PlaybackTime  = TIME_NOT_APPLICABLE;
    Event.UserData  = NULL;
    Event.Reason    = STM_SE_PLAY_STREAM_MSG_REASON_CODE_STREAM_UNKNOWN;
    Status  = SignalEvent(&Event);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to signal event EventStreamUnPlayable\n", this);
        return PlayerError;
    }

    return PlayerNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Step a specific stream in a playback.
//

PlayerStatus_t   PlayerStream_c::Step()
{
    SE_INFO(group_player, "Stream 0x%p Step\n", this);

    mStep    = true;
    OS_SetEvent(&SingleStepMayHaveHappened);

    return PlayerNoError;
}

PlayerStatus_t   PlayerStream_c::GetElementaryBufferLevel(
    stm_se_ctrl_play_stream_elementary_buffer_level_t    *ElementaryBufferLevel)
{
    unsigned int MemoryInPool, MemoryInUse;

    mCodedFrameBufferPool->GetPoolUsage(NULL, NULL, &MemoryInPool, NULL, &MemoryInUse, NULL);

    ElementaryBufferLevel->actual_size = MemoryInPool;
    ElementaryBufferLevel->effective_size = MemoryInPool - CodedFrameMaximumSize;
    ElementaryBufferLevel->bytes_used = MemoryInUse;
    ElementaryBufferLevel->maximum_nonblock_write = MemoryInPool - MemoryInUse;

    return PlayerNoError;
}

PlayerStatus_t   PlayerStream_c::InputJump(bool SurplusData, bool ContinuousReverseJump, bool FromDiscontinuityControl)
{
    PlayerStatus_t Status = mCollator->InputJump(SurplusData, ContinuousReverseJump, FromDiscontinuityControl);
    if (Status != PlayerNoError) { return Status; }

    DisplayDiscontinuity = true;

    return PlayerNoError;
}

// Helper function to fill in the Buffer Pool Usage levels
//
static void GetBufferPoolLevel(class BufferPool_c *Pool, BufferPoolLevel *Level)
{
    Pool->GetPoolUsage(&Level->BuffersInPool,
                       &Level->BuffersWithNonZeroReferenceCount,
                       &Level->MemoryInPool,
                       &Level->MemoryAllocated,
                       &Level->MemoryInUse,
                       &Level->LargestFreeMemoryBlock);
}


const PlayerStreamStatistics_t &PlayerStream_c::GetStatistics()
{
    //
    // The majority of our statistics are updated on an event basis
    // However, buffer levels, need to be polled before returning the structure
    //
    GetBufferPoolLevel(mCodedFrameBufferPool, &mStatistics.CodedFrameBufferPool);
    GetBufferPoolLevel(DecodeBufferPool,     &mStatistics.DecodeBufferPool);
    return mStatistics;
}

void PlayerStream_c::ResetStatistics()
{
    memset(&mStatistics, 0, sizeof(mStatistics));
}

const PlayerStreamAttributes_t &PlayerStream_c::GetAttributes()
{
    return mAttributes;
}

void PlayerStream_c::ResetAttributes()
{
    memset(&mAttributes, 0, sizeof(mAttributes));
}

//
//  Low power enter / exit functions at stream level
//  These methods are used to synchonize stop/restart of ParseToDecode threads in low power
//  and to terminate/init MME transformer used by codec in case of CPS mode.
//

PlayerStatus_t   PlayerStream_c::LowPowerEnter()
{
    SE_INFO(group_player, "Stream 0x%p\n", this);

    // Reset events used for putting ParseToDecode thread into "low power"
    OS_ResetEvent(&mLowPowerEnterEvent);
    OS_ResetEvent(&mLowPowerExitEvent);

    // Save low power state at stream level
    mIsLowPowerState = true;

    // Wait for ParseToDecode thread to be in safe state (no more MME commands issued)
    OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&mLowPowerEnterEvent, OS_INFINITE);
    if (WaitStatus == OS_INTERRUPTED)
    {
        SE_INFO(group_player, "wait for LP enter interrupted; event->Valid:%d\n", mLowPowerEnterEvent.Valid);
    }

    // Ask codec to enter low power state
    // For CPS mode, MME transformers will be terminated now...
    GetCodec()->LowPowerEnter();

    return PlayerNoError;
}

PlayerStatus_t   PlayerStream_c::LowPowerExit()
{
    SE_INFO(group_player, "Stream 0x%p\n", this);

    // Ask codec to exit low power state
    // For CPS mode, MME transformers will be re-initialized now...
    GetCodec()->LowPowerExit();

    // Reset low power state at stream level
    mIsLowPowerState = false;

    // Wake-up ParseToDecode thread
    OS_SetEventInterruptible(&mLowPowerExitEvent);

    return PlayerNoError;
}

PlayerStatus_t PlayerStream_c::CheckEvent(struct PlayerEventRecord_s *PlayerEvent, stm_se_play_stream_msg_t *streamMsg, stm_event_t *streamEvtMngt)
{
    SE_VERBOSE(group_player, "Stream 0x%p: Received (event %llx Playback 0x%p Stream 0x%p)\n", this, PlayerEvent->Code, PlayerEvent->Playback, PlayerEvent->Stream);

    // By default, set event_id and msg_id to invalid values
    streamEvtMngt->event_id          = STM_SE_PLAY_STREAM_EVENT_INVALID;
    streamMsg->msg_id                = STM_SE_PLAY_STREAM_MSG_INVALID;

    // Check if event is for us and we are interested.
    if ((PlayerEvent->Playback != GetPlayback()) ||
        (PlayerEvent->Stream != this))
    {
        SE_ERROR("Stream 0x%p: Playback 0x%p Invalid event info received (event %llx Event->Playback 0x%p Event->Stream 0x%p)\n",
                 this, GetPlayback(), PlayerEvent->Code, PlayerEvent->Playback, PlayerEvent->Stream);
        return PlayerError;
    }

    //{{{  Translate from an event record to the external play event record.
    switch (PlayerEvent->Code)
    {
    case EventSourceVideoParametersChangeManifest:
    {
        unsigned int    PictureWidth            = PlayerEvent->Value[0].UnsignedInt;
        unsigned int    PictureHeight           = PlayerEvent->Value[1].UnsignedInt;

        if (0 == PictureHeight)
        {
            SE_INFO(group_player, "Stream 0x%p: PictureHeight 0; forcing 1:1\n", this);
            PictureWidth  = 1;
            PictureHeight = 1;
        }

        Rational_t      PictureAspectRatio      = Rational_t(PictureWidth, PictureHeight) * PlayerEvent->Rational;
        // Invert scan type, as stm_se_scan_type_t states: progressive = 0, interlaced = 1
        stm_se_scan_type_t ScanType             = (stm_se_scan_type_t) !PlayerEvent->Value[2].UnsignedInt;
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED;
        streamMsg->u.video_parameters.width      = PictureWidth;
        streamMsg->u.video_parameters.height     = PictureHeight;
        streamMsg->u.video_parameters.scan_type  = ScanType;

        // less than 3:2 = 4:3, less than 2:1 = 16:9 otherwise 2.21:1
        if (PictureAspectRatio < Rational_t (3, 2))
        {
            streamMsg->u.video_parameters.aspect_ratio     = STM_SE_ASPECT_RATIO_4_BY_3;
        }
        else if (PictureAspectRatio < Rational_t (2, 1))
        {
            streamMsg->u.video_parameters.aspect_ratio     = STM_SE_ASPECT_RATIO_16_BY_9;
        }
        else
        {
            streamMsg->u.video_parameters.aspect_ratio     = STM_SE_ASPECT_RATIO_221_1;
        }

        // The manifestor inserts the pixel aspect ratio into the event rational.
        streamMsg->u.video_parameters.pixel_aspect_ratio_numerator           = (unsigned int)PlayerEvent->Rational.GetNumerator();
        streamMsg->u.video_parameters.pixel_aspect_ratio_denominator         = (unsigned int)PlayerEvent->Rational.GetDenominator();
        // Retreive the stereoscopic data
        streamMsg->u.video_parameters.format_3d         = (stm_se_format_3d_t) PlayerEvent->Value[3].UnsignedInt;
        streamMsg->u.video_parameters.left_right_format = PlayerEvent->Value[4].Bool;
        streamMsg->u.video_parameters.colorspace        = (stm_se_colorspace_t) PlayerEvent->Value[5].UnsignedInt;
        streamMsg->u.video_parameters.frame_rate        = (unsigned int)PlayerEvent->Value[6].UnsignedInt;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED: "
                "width %d height %d scan_type %d aspect_ratio %d format_3d %d "
                "left_right_format %d colorspace %d frame_rate %d\n",
                this, streamMsg->u.video_parameters.width, streamMsg->u.video_parameters.height,
                streamMsg->u.video_parameters.scan_type, streamMsg->u.video_parameters.aspect_ratio,
                streamMsg->u.video_parameters.format_3d, streamMsg->u.video_parameters.left_right_format,
                streamMsg->u.video_parameters.colorspace, streamMsg->u.video_parameters.frame_rate);
        break;
    }

    case EventSourceFrameRateChangeManifest:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED;
        streamMsg->u.frame_rate                  = IntegerPart(PlayerEvent->Rational * STM_SE_PLAY_FRAME_RATE_MULTIPLIER);
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED: frame_rate %d\n",
                this, streamMsg->u.frame_rate);
        break;

    case EventFirstFrameManifested:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY\n",
                this);
        break;

    case EventStreamUnPlayable:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE;
        streamMsg->u.reason                      = (stm_se_play_stream_reason_code_t)PlayerEvent->Reason;
        SE_WARNING("Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE: reason %d\n",
                   this, PlayerEvent->Reason);
        break;

    case EventFailedToQueueBufferToDisplay:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR;
        SE_ERROR("Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR: reason EventFailedToQueueBufferToDisplay\n",
                 this);
        break;

    case EventFailedToDecodeInTime:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE;
        SE_WARNING("Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE\n",
                   this);
        break;

    case EventFailedToDeliverDataInTime:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE;
        SE_WARNING("Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE\n",
                   this);
        break;

    case EventTrickModeDomainChange:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE;

        switch (PlayerEvent->Value[0].UnsignedInt)
        {
        case PolicyValueTrickModeAuto:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_AUTO;
            break;

        case PolicyValueTrickModeDecodeAll:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL;
            break;

        case PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES;
            break;

        case PolicyValueTrickModeStartDiscardingNonReferenceFrames:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES;
            break;

        case PolicyValueTrickModeDecodeReferenceFramesDegradeNonKeyFrames:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES;
            break;

        case PolicyValueTrickModeDecodeKeyFrames:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES;
            break;

        case PolicyValueTrickModeDiscontinuousKeyFrames:
            streamMsg->u.trick_mode_domain          = STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES;
            break;
        }

        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE: trick_mode_domain %d\n",
                this, streamMsg->u.trick_mode_domain);
        break;

    case EventVsyncOffsetMeasured:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_VSYNC_OFFSET_MEASURED;
        streamMsg->u.vsync_offset                = PlayerEvent->Value[0].LongLong;
        SE_DEBUG(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_VSYNC_OFFSET_MEASURED: trick_mode_domain %lld\n",
                 this, streamMsg->u.vsync_offset);
        break;

    case EventFatalHardwareFailure:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE;
        SE_ERROR("Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE\n",
                 this);
        break;

    case EventNewFrameDisplayed:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED;
        SE_VERBOSE(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED\n",
                   this);
        break;

    case EventNewFrameDecoded:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED;
        SE_VERBOSE(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED\n",
                   this);
        break;

    case EventStreamInSync:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC\n",
                this);
        break;

    case EventAlarmParsedPts:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS;
        streamMsg->u.alarm_parsed_pts.pts = (uint64_t) PlayerEvent->PlaybackTime;
        streamMsg->u.alarm_parsed_pts.size = PlayerEvent->Value[0].UnsignedInt;
        /* Retrieve marker_id0 and marker_id1, highest bits first */
        streamMsg->u.alarm_parsed_pts.data[0] = (PlayerEvent->Value[1].UnsignedInt >> 24) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[1] = (PlayerEvent->Value[1].UnsignedInt >> 16) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[2] = (PlayerEvent->Value[1].UnsignedInt >> 8) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[3] = PlayerEvent->Value[1].UnsignedInt & 0xff;
        streamMsg->u.alarm_parsed_pts.data[4] = (PlayerEvent->Value[2].UnsignedInt >> 24) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[5] = (PlayerEvent->Value[2].UnsignedInt >> 16) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[6] = (PlayerEvent->Value[2].UnsignedInt >> 8) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[7] = PlayerEvent->Value[2].UnsignedInt & 0xff;
        streamMsg->u.alarm_parsed_pts.data[8] = (PlayerEvent->Value[3].UnsignedInt >> 24) & 0xff;
        streamMsg->u.alarm_parsed_pts.data[9] = (PlayerEvent->Value[3].UnsignedInt >> 16) & 0xff;
        SE_DEBUG(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS: size %d"
                 "ControlUserData[1]= %02x%02x%02x%02x , "
                 "ControlUserData[2]=%02x%02x%02x%02x,"
                 "ControlUserData[3]= %02x%02x, PTS %llu\n",
                 this, streamMsg->u.alarm_parsed_pts.size,
                 streamMsg->u.alarm_parsed_pts.data[0],
                 streamMsg->u.alarm_parsed_pts.data[1],
                 streamMsg->u.alarm_parsed_pts.data[2],
                 streamMsg->u.alarm_parsed_pts.data[3],
                 streamMsg->u.alarm_parsed_pts.data[4],
                 streamMsg->u.alarm_parsed_pts.data[5],
                 streamMsg->u.alarm_parsed_pts.data[6],
                 streamMsg->u.alarm_parsed_pts.data[7],
                 streamMsg->u.alarm_parsed_pts.data[8],
                 streamMsg->u.alarm_parsed_pts.data[9],
                 streamMsg->u.alarm_parsed_pts.pts);
        break;

    case EventFrameStarvation:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION;
        SE_WARNING("Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION\n",
                   this);
        break;

    case EventFrameSupplied:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED;
        SE_VERBOSE(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED\n",
                   this);
        break;

    case EventSourceAudioParametersChange:
        streamEvtMngt->event_id                          = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                                = STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED;
        // We need to properly assign the Values
        // codec common parameters
        streamMsg->u.audio_parameters.audio_coding_type  = (stm_se_stream_encoding_t) PlayerEvent->Value[0].UnsignedInt;
        streamMsg->u.audio_parameters.sampling_freq      = (uint32_t) PlayerEvent->Value[1].UnsignedInt;
        streamMsg->u.audio_parameters.num_channels       = (uint8_t) PlayerEvent->Value[2].UnsignedInt;
        streamMsg->u.audio_parameters.dual_mono          = (bool) PlayerEvent->Value[3].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.pair0     = (unsigned int) PlayerEvent->Value[4].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.pair1     = (unsigned int) PlayerEvent->Value[5].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.pair2     = (unsigned int) PlayerEvent->Value[6].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.pair3     = (unsigned int) PlayerEvent->Value[7].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.pair4     = (unsigned int) PlayerEvent->Value[8].UnsignedInt;
        streamMsg->u.audio_parameters.channel_assignment.malleable = (unsigned int) PlayerEvent->Value[9].UnsignedInt;
        streamMsg->u.audio_parameters.bitrate            = (int) PlayerEvent->Value[10].UnsignedInt;
        streamMsg->u.audio_parameters.copyright          = (bool) PlayerEvent->Value[11].UnsignedInt;
        streamMsg->u.audio_parameters.emphasis           = (bool) PlayerEvent->Value[12].UnsignedInt;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED: "
                "common: AudioCodingType %d SamplingFreq %d NumChannels %d DualMono %d "
                "ChannelAssign[%d-%d-%d-%d-%d malleable(%d)] Bitrate %d Copyright %d Emphasis %d\n",
                this, streamMsg->u.audio_parameters.audio_coding_type,
                streamMsg->u.audio_parameters.sampling_freq,
                streamMsg->u.audio_parameters.num_channels,
                streamMsg->u.audio_parameters.dual_mono,
                streamMsg->u.audio_parameters.channel_assignment.pair0,
                streamMsg->u.audio_parameters.channel_assignment.pair1,
                streamMsg->u.audio_parameters.channel_assignment.pair2,
                streamMsg->u.audio_parameters.channel_assignment.pair3,
                streamMsg->u.audio_parameters.channel_assignment.pair4,
                streamMsg->u.audio_parameters.channel_assignment.malleable,
                streamMsg->u.audio_parameters.bitrate,
                streamMsg->u.audio_parameters.copyright,
                streamMsg->u.audio_parameters.emphasis);

        // codec specific parameters (if exists)
        switch (streamMsg->u.audio_parameters.audio_coding_type)
        {
        case STM_SE_STREAM_ENCODING_AUDIO_MPEG2:
        case STM_SE_STREAM_ENCODING_AUDIO_MP3:
            streamMsg->u.audio_parameters.codec_specific.mpeg_params.layer = (uint32_t) PlayerEvent->ExtraValue[0].UnsignedInt;
            SE_DEBUG(group_event, " specific: Layer=%d\n",
                     streamMsg->u.audio_parameters.codec_specific.mpeg_params.layer);
            break;

        default:
            // no specific parameters
            break;
        }

        break;

    case EventStreamSwitched:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_SWITCH_COMPLETED;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_EVENT_SWITCH_COMPLETED\n",
                this);
        break;

    case EventAlarmPts:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_ALARM_PTS;
        streamMsg->u.alarm_pts.pts = (uint64_t) PlayerEvent->PlaybackTime;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_ALARM_PTS pts %llu\n",
                this, streamMsg->u.alarm_pts.pts);
        break;

    case EventDiscarding:
        streamEvtMngt->event_id                  = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY;
        streamMsg->msg_id                        = STM_SE_PLAY_STREAM_MSG_DISCARDING;
        streamMsg->u.discard_trigger.type = (stm_se_play_stream_trigger_type_t)PlayerEvent->Value[0].UnsignedInt;
        streamMsg->u.discard_trigger.start_not_end = PlayerEvent->Value[1].Bool;
        SE_INFO(group_event, "Stream 0x%p: event STM_SE_PLAY_STREAM_MSG_DISCARDING type %d %s trigger\n",
                this, streamMsg->u.discard_trigger.type,
                (streamMsg->u.discard_trigger.start_not_end == true) ? "start" : "end");
        break;

    default:
        SE_ERROR("Stream 0x%p: Unexpected event %llx\n", this, PlayerEvent->Code);
        return PlayerError;
    }

    return PlayerNoError;
}

PlayerStatus_t PlayerStream_c::SignalEvent(struct PlayerEventRecord_s     *PlayerEvent)
{
    stm_se_play_stream_msg_t streamMsg;
    stm_event_t              streamEvtMngt;
    int                      err = 0;
    PlayerStatus_t           PlayerStatus = PlayerNoError;

    // TODO(vgi): review locking strategy for PlayerStream class
    OS_LockMutex(&mPlayer->Lock); // Make certain we cannot delete playback while checking the event

    if (CheckEvent(PlayerEvent, &streamMsg, &streamEvtMngt) == PlayerNoError)
    {
        // Signal Event via Event manager
        if (streamEvtMngt.event_id != STM_SE_PLAY_STREAM_EVENT_INVALID)
        {
            if (streamEvtMngt.event_id == STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY)
            {
                // Push message to subscriptions
                if (GetMessenger()->NewSignaledMessage(&streamMsg) != MessageNoError)
                {
                    SE_WARNING("Stream 0x%p: Message subscription not supported %d\n", this, streamMsg.msg_id);
                }
            }

            streamEvtMngt.object = (stm_object_h)GetHavanaStream();
            err = stm_event_signal(&streamEvtMngt);
            if (err)
            {
                if (err == -ECANCELED)
                {
                    SE_INFO(group_player, "Stream 0x%p: stm_event_signal returned ECANCELED\n", this);
                    // event suscriber was removed; not an error
                    PlayerStatus = PlayerNoError;
                }
                else
                {
                    PlayerStatus = PlayerError;
                    SE_FATAL("Stream 0x%p: stm_event_signal failed (%d)\n", this, err);
                }
            }
        }
    }
    else
    {
        SE_ERROR("Stream 0x%p: error during event check\n", this);
        PlayerStatus = PlayerError;
    }

    OS_UnLockMutex(&mPlayer->Lock);
    return PlayerStatus;
}

void  PlayerStream_c::SetSpeed(Rational_t Speed, PlayDirection_t Direction)
{
    bool ReTimeQueuedFrames = false;

    // if only key frames are decoded, or if speed is set to 0, retiming should not happen
    if (Speed != 0 && mSpeed != 0
        && mPlayer->PolicyValue(mPlayback, this, PolicyTrickModeDomain) != PolicyValueTrickModeDecodeKeyFrames
        && mPlayer->PolicyValue(mPlayback, this, PolicyStreamOnlyKeyFrames) != PolicyValueApply
        && mPlayer->PolicyValue(mPlayback, this, PolicyStreamDiscardFrames) != PolicyValueKeyFramesOnly
        && mDirection == Direction
        && mSpeed != Speed)
    {
        ReTimeQueuedFrames = true;
    }

    mSpeed      = Speed;
    mDirection  = Direction;

    if (ReTimeQueuedFrames && (mStreamType == StreamTypeVideo))
    {
        // Call TrickModeControl to force update of AdjustedSpeedAfterFrameDrop
        // so that the retimed frames will take the new speed into account.
        GetOutputTimer()->TrickModeControl();
        PostManifestEdge->PerformReTiming();
        DecodeToManifestEdge->ReleaseQueuedDecodeBuffers();
    }

    if (mStreamType == StreamTypeAudio)
    {
        GetCodec()->UpdatePlaybackSpeed();
        DecodeToManifestEdge->ReleaseQueuedDecodeBuffers();
    }

    // Reset frame parser RevPlaySmoothReverseFailureCount on speed request.
    // This is to avoid having backward remaining in I only even after doing a positive speed change (see bugzilla 19026).
    GetFrameParser()->ResetReverseFailureCounter();
    DecodeToManifestEdge->WakeUp();
}

void PlayerStream_c::ApplyDecoderConfig(unsigned int Config)
{
    if (ParseToDecodeEdge)
    {
        ParseToDecodeEdge->ApplyDecoderConfig(Config);
    }
}
