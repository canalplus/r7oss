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

#include <stm_event.h>

#include "player_threads.h"
#include "st_relayfs_se.h"

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_video.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Video_c"

#define TUNEABLE_NAME_MANIFESTOR_VIDEO_ENABLE_SW_CRC    "video_manifestor_enable_sw_crc"

#define FOUR_BY_THREE                           Rational_t(4,3)
#define SIXTEEN_BY_NINE                         Rational_t(16,9)

#define MAX_SUPPORTED_VIDEO_CONTENT_WIDTH       2048
#define MAX_SUPPORTED_VIDEO_CONTENT_HEIGHT      1536
#define MAX_LATENCY_MICROSEC                    200000

static OS_TaskEntry(DisplaySignalThreadStub)
{
    Manifestor_Video_c *VideoManifestor = (Manifestor_Video_c *)Parameter;
    VideoManifestor->DisplaySignalThread();
    OS_TerminateThread();
    return NULL;
}

unsigned int volatile Manifestor_Video_c::EnableSoftwareCRC = 0;

Manifestor_Video_c::Manifestor_Video_c(void)
    : Manifestor_Base_c()
    , RelayfsIndex(0)
    , Visible(false)
    , DisplayUpdatePending(false)
    , SurfaceDescriptor()
    , StreamDisplayParameters()
    , MaxBufferCount(0)
    , StreamBuffer()
    , BufferOnDisplay(INVALID_BUFFER_ID)
    , PtsOnDisplay(INVALID_TIME)
    , LastQueuedBuffer(ANY_BUFFER_ID)
    , BufferLock()
    , NumberOfTimings(1)
    , DisplaySignalThreadTerminated()
    , DisplaySignalThreadRunning(false)
    , BufferDisplayed()
    , mFirstFrameOnDisplay(true)
    , IsHalting(false)
    , mNextQueuedManifestationTime(0)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Initialization status not valid - aborting init\n", Stream, this);
        return;
    }

    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);

    Configuration.ManifestorName        = "Video";
    Configuration.StreamType            = StreamTypeVideo;

    // Initialise the FrameRate in the Surface Descriptor
    for (int i = 0; i < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; i++)
    {
        SurfaceDescriptor[i].FrameRate    = 1;    // rational
    }

    OS_InitializeMutex(&BufferLock);

    RelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_DECODED_VIDEO_BUFFER);

    // Register the SW CRC option global
    OS_RegisterTuneable(TUNEABLE_NAME_MANIFESTOR_VIDEO_ENABLE_SW_CRC, (unsigned int *)&EnableSoftwareCRC);
    InitCRC32Table();

    OS_SemaphoreInitialize(&BufferDisplayed, 0);
    OS_InitializeEvent(&DisplaySignalThreadTerminated);
}

Manifestor_Video_c::~Manifestor_Video_c(void)
{
    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);

    Manifestor_Video_c::Halt();

    OS_UnregisterTuneable(TUNEABLE_NAME_MANIFESTOR_VIDEO_ENABLE_SW_CRC);

    OS_TerminateMutex(&BufferLock);

    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_DECODED_VIDEO_BUFFER, RelayfsIndex);

    OS_SemaphoreTerminate(&BufferDisplayed);
    OS_TerminateEvent(&DisplaySignalThreadTerminated);
}

//  Halt
//  doxynote
/// \brief              Shutdown, stop presenting and retrieving frames
///                     don't return until complete
//
ManifestorStatus_t      Manifestor_Video_c::Halt(void)
{
    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);
    IsHalting = true;
    OS_Smp_Mb();

    if (DisplaySignalThreadRunning)
    {
        // Ask display signal thread to terminate
        OS_ResetEvent(&DisplaySignalThreadTerminated);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_DisplaySignal_Terminating coupled with: rmb_for_DisplaySignal_Terminating
        DisplaySignalThreadRunning = false;

        OS_SemaphoreSignal(&BufferDisplayed);

        // Wait for thread to terminate
        OS_WaitForEventAuto(&DisplaySignalThreadTerminated, OS_INFINITE);
    }

    return Manifestor_Base_c::Halt();
}

//  Reset
/// \brief              Reset all state to intial values
ManifestorStatus_t Manifestor_Video_c::Reset(void)
{
    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    IsHalting                           = false;
    OS_Smp_Mb();
    BufferOnDisplay                     = INVALID_BUFFER_ID;
    LastQueuedBuffer                    = ANY_BUFFER_ID;
    DisplayUpdatePending                = false;
    mNextQueuedManifestationTime        = 0;
    mFirstFrameOnDisplay                = true;

    return Manifestor_Base_c::Reset();
}

//  Connect
//  doxynote
/// \brief Create and initialize the buffer release threads, then register the output port
/// \param Port         Output port.
/// \return             Succes or failure
//
ManifestorStatus_t      Manifestor_Video_c::Connect(Port_c *Port)
{
    unsigned int                        i, j;
    ManifestorStatus_t                  Status;
    BufferPool_t                        Pool;
    OS_Thread_t                         Thread;

    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);

    //  Get the number of buffers in the decode buffer pool.
    Status = Stream->GetDecodeBufferManager()->GetDecodeBufferPool(&Pool);

    if (Status != PlayerNoError)
    {
        return Status;
    }

    Pool->GetPoolUsage(&MaxBufferCount);

    //  Create display signal thread
    if (!DisplaySignalThreadRunning)
    {
        DisplaySignalThreadRunning = true;

        if (OS_CreateThread(&Thread, DisplaySignalThreadStub, this, &player_tasks_desc[SE_TASK_MANIF_DSVIDEO]) != OS_NO_ERROR)
        {
            SE_ERROR("Stream 0x%p this 0x%p Unable to create Display Signal thread\n", Stream, this);
            DisplaySignalThreadRunning  = false;
            return ManifestorError;
        }
    }

    for (i = 0; i < MaxBufferCount; i++)
    {
        StreamBuffer[i].BufferIndex                 = i;
        StreamBuffer[i].QueueCount                  = 0;
        StreamBuffer[i].BufferState                 = BufferStateAvailable;
        StreamBuffer[i].BufferClass                 = NULL;

        for (j = 0; j < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; j++)
        {
            StreamBuffer[i].OutputTiming[j]           = NULL;
        }

        StreamBuffer[i].NumberOfTimings             = 0;
    }

    BufferOnDisplay                     = INVALID_BUFFER_ID;
    LastQueuedBuffer                    = ANY_BUFFER_ID;

    memset((void *)&StreamDisplayParameters, 0, sizeof(StreamDisplayParameters)); // TODO(pht) change
    StreamDisplayParameters.PixelAspectRatio    = 0; // rational
    StreamDisplayParameters.FrameRate           = 0; // rational

    // Connect the port
    Status = Manifestor_Base_c::Connect(Port);

    if (Status != ManifestorNoError)
    {
        return Status;
    }

    // Let outside world know we are up and running
    SetComponentState(ComponentRunning);

    return ManifestorNoError;
}

//  GetSurfaceParameters
//  doxynote
/// \brief      Fill in private structure with timing details of display surface
/// \param      SurfaceParameters pointer to structure to complete
//
ManifestorStatus_t      Manifestor_Video_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    unsigned int i;
    UpdateOutputSurfaceDescriptor();

    for (i = 0; i < NumberOfTimings; i++)
    {
        SurfaceParameters[i] = &SurfaceDescriptor[i];
        SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p SurfaceDescriptor[%d] 0x%p\n", Stream, this, i, &SurfaceDescriptor[i]);
    }

    *NumSurfaces = NumberOfTimings;
    SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p NumSurfaces %d\n", Stream, this, *NumSurfaces);
    return ManifestorNoError;
}



//  GetNextQueuedManifestationTime
//  doxynote
/// \brief              Return the earliest system time at which the next frame to be queued will be manifested
/// \param Time         Pointer to 64-bit system time variable
/// \result             Success or fail if no time can be inferred
//
ManifestorStatus_t      Manifestor_Video_c::GetNextQueuedManifestationTime(unsigned long long *ArrayOfTimes, unsigned int *NumTimes)
{
    *NumTimes = 1;

    if (mNextQueuedManifestationTime == 0)
    {
        ArrayOfTimes[SOURCE_INDEX] = OS_GetTimeInMicroSeconds() + GetPipelineLatency();
    }
    else
    {
        ArrayOfTimes[SOURCE_INDEX] = mNextQueuedManifestationTime;
    }

    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p Estimate %llu\n", Stream, this, ArrayOfTimes[SOURCE_INDEX]);

    return ManifestorNoError;
}

void Manifestor_Video_c::UpdateNextQueuedManifestationTime(ManifestationOutputTiming_t  **VideoOutputTimingArray)
{
    if (ValidTime(VideoOutputTimingArray[SOURCE_INDEX]->SystemPlaybackTime))
    {
        mNextQueuedManifestationTime = VideoOutputTimingArray[SOURCE_INDEX]->SystemPlaybackTime;
    }
    mNextQueuedManifestationTime += VideoOutputTimingArray[SOURCE_INDEX]->ExpectedDurationTime;
}

ManifestorStatus_t      Manifestor_Video_c::ReleaseQueuedDecodeBuffers(void)
{
    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p\n", Stream, this);
    mNextQueuedManifestationTime = 0;
    return Manifestor_Base_c::ReleaseQueuedDecodeBuffers();
}

ManifestorStatus_t      Manifestor_Video_c::ObtainBufferMetaData(class Buffer_c                         *Buffer,
                                                                 struct ParsedFrameParameters_s        **FrameParameters,
                                                                 struct ParsedVideoParameters_s        **VideoParameters)
{
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)FrameParameters);
    SE_ASSERT(FrameParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    return ManifestorNoError;
}

/// \brief              Receive a decode buffer for display and send it with appropraite
///                     flags to the driver
/// \param              Buffer containing decoded frame witha attached display meta data
/// \result             Success if frame displayed, fail if not displayed
//
//  _QueueDecodeBuffer
ManifestorStatus_t      Manifestor_Video_c::_QueueDecodeBuffer(class Buffer_c        *Buffer,
                                                               ManifestationOutputTiming_t  **VideoOutputTimingArray, unsigned int *NumTimes)
{
    struct StreamBuffer_s              *StreamBuff;
    struct ParsedFrameParameters_s     *FrameParameters;
    struct ParsedVideoParameters_s     *VideoParameters;
    unsigned int                        BufferIndex, i;
    ManifestorStatus_t                  Status;

    SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p Buffer 0x%p VideoOutputTimingArray 0x%p NumTimes %d\n",
               Stream, this, Buffer, VideoOutputTimingArray[SOURCE_INDEX], *NumTimes);

    Buffer->GetIndex(&BufferIndex);
    if (BufferIndex >= MAX_DECODE_BUFFERS)
    {
        SE_ERROR("Stream 0x%p this 0x%p invalid buffer index %d\n", Stream, this, BufferIndex);
        return ManifestorError;
    }

    StreamBuff = &StreamBuffer[BufferIndex];

    SE_ASSERT(StreamBuff->BufferState == BufferStateAvailable);

    Status = ObtainBufferMetaData(Buffer, &FrameParameters, &VideoParameters);
    SE_ASSERT(Status == ManifestorNoError);

    StreamBuff->BufferClass     = Buffer;

    for (i = 0; i < *NumTimes; i++)
    {
        StreamBuff->OutputTiming[i]    = VideoOutputTimingArray[i];
        SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p StreamBuff->OutputTiming[%d] 0x%p, VideoOutputTimingArray[i] 0x%p\n",
                   Stream, this, i, StreamBuff->OutputTiming[i], VideoOutputTimingArray[i]);
    }

    StreamBuff->NumberOfTimings = *NumTimes;

    StreamBuff->NativePlaybackTime  = FrameParameters->NativePlaybackTime;
    StreamBuff->NativeTimeFormat    = FrameParameters->NativeTimeFormat;

    // Allow to capture queued frame buffer with ST_RELAY
    {
        unsigned char *Data = (unsigned char *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent, CachedAddress);
        unsigned int   Len  = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, PrimaryManifestationComponent);

        st_relayfs_write_se(ST_RELAY_TYPE_DECODED_VIDEO_BUFFER + RelayfsIndex, ST_RELAY_SOURCE_SE, Data, Len, 0);
    }

    StreamBuff->BufferState     = BufferStateNotQueued;
    Status = QueueBuffer(BufferIndex, FrameParameters, VideoParameters, VideoOutputTimingArray, Buffer);

    if (Status != ManifestorNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Failed to queue buffer %d to display\n", Stream, this, BufferIndex);
        StreamBuff->BufferState     = BufferStateAvailable;
        mOutputPort->Insert((uintptr_t)Buffer);
    }
    else
    {
        if (StreamBuff->BufferState == BufferStateQueued)
        {
            SE_EXTRAVERB(GetGroupTrace(), "Stream 0x%p this 0x%p Buffer #%d has been queued\n",
                         Stream, this, BufferIndex);
            UpdateNextQueuedManifestationTime(VideoOutputTimingArray);
            LastQueuedBuffer            = BufferIndex;
        }
        else if (StreamBuff->BufferState == BufferStateNotQueued)
        {
            SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p Buffer #%d has not been queued\n",
                       Stream, this, BufferIndex);
            StreamBuff->BufferState     = BufferStateAvailable;
            mOutputPort->Insert((uintptr_t)Buffer);
        }
    }

    return ManifestorNoError;
}

ManifestorStatus_t      Manifestor_Video_c::QueueDecodeBuffer(class Buffer_c        *Buffer,
                                                              ManifestationOutputTiming_t  **TimingPointerArray, unsigned int *NumTimes)
{
    ManifestorStatus_t  Status;

    AssertComponentState(ComponentRunning);

    OS_LockMutex(&BufferLock);

    Status = _QueueDecodeBuffer(Buffer, TimingPointerArray, NumTimes);

    OS_UnLockMutex(&BufferLock);

    return Status;
}

//  GetBufferId
//  doxynote
/// \brief      Retrieve buffer index of last buffer queued. If no buffers
///             yet queued returns ANY_BUFFER_ID
/// \return     Buffer index of last buffer sent for display
//
unsigned int    Manifestor_Video_c::GetBufferId(void)
{
    return LastQueuedBuffer;
}

//  GetNativeTimeOfCurrentlyManifestedFrame
//  doxynote
/// \brief Get original PTS of currently visible buffer
/// \param Pts          Pointer to Pts variable
/// \return             Success if Pts value is available
//
ManifestorStatus_t Manifestor_Video_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
{
    SE_VERBOSE(GetGroupTrace(), "Stream 0x%p this 0x%p PtsOnDisplay %llu\n", Stream, this, PtsOnDisplay);
    *Time = PtsOnDisplay;

    if (NotValidTime(PtsOnDisplay))
    {
        SE_ERROR("Stream 0x%p this 0x%p No buffer on display\n", Stream, this);
        return ManifestorError;
    }

    return ManifestorNoError;
}

//  DisplaySignalThread
void  Manifestor_Video_c::DisplaySignalThread(void)
{
    //struct StreamBuffer_s*      Buffer;
    stm_event_t              streamEvtMngt;
    int                      err = 0;
    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p>\n", Stream, this);

    while (DisplaySignalThreadRunning)
    {
        // If the kernel debug option CONFIG_DETECT_HUNG_TASK is enabled,
        // we must not block for more than the period it defines
        while (OS_TIMED_OUT == OS_SemaphoreWaitTimeout(&BufferDisplayed, MAX_THREAD_BLOCK_MS))
            ; // No Op - We've timed out - continue to wait

        if (DisplaySignalThreadRunning)
        {
            //
            // Check if we need to signal first frame event
            //
            CheckFirstFrameOnDisplay();
            //
            // Signal the frame rendered event for each displayed frame
            //
            streamEvtMngt.event_id          = STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED;
            streamEvtMngt.object = (stm_object_h)Stream->GetHavanaStream();
            // TODO:vgi any reason why it is not using normal SignalEvent path ?

            err = stm_event_signal(&streamEvtMngt);
            if (err)
            {
                SE_ERROR("Stream 0x%p this 0x%p stm_event_signal failed (%d)\n", Stream, this, err);
            }

            ServiceEventQueue(BufferOnDisplay);
        }
    }
    OS_Smp_Mb(); // Read memory barrier: rmb_for_DisplaySignal_Terminating coupled with: wmb_for_DisplaySignal_Terminating

    SE_DEBUG(GetGroupTrace(), "Stream 0x%p this 0x%p<\n", Stream, this);
    OS_SetEvent(&DisplaySignalThreadTerminated);
}

//  InitCRC32table
/// \brief                      Initializes CRC coefficients table
void Manifestor_Video_c::InitCRC32Table(void)
{
    unsigned int i;
    unsigned int j;
    unsigned int crc_precalc;
    unsigned int POLYNOM = 0x04c11db7;

    for (i = 0; i < 256; i++)
    {
        crc_precalc = i << 24;

        for (j = 0; j < 8; j++)
        {
            if ((crc_precalc & 0x80000000) == 0)
            {
                crc_precalc = (crc_precalc << 1);
            }
            else
            {
                crc_precalc = (crc_precalc << 1) ^  POLYNOM;
            }
        }

        CRC32Table[i] = crc_precalc;
    }
}


//  CalcSWCRC
/// \brief                      Calculate CRC on a given picture
ManifestorStatus_t  Manifestor_Video_c::CalcSWCRC(BufferFormat_t BufferFormat, unsigned int Width, unsigned int Height,
                                                  PictureStructure_t PictureStructure, unsigned char *LumaAddress,
                                                  unsigned char *CbAddress, unsigned char *CrAddress ,
                                                  unsigned int *LumaCRC, unsigned  int *ChromaCRC)
{
    unsigned int CRC_Y;
    unsigned int CRC_C;
    unsigned int i;
    unsigned int j;
    unsigned int skip_part = 0;
    // TopField == 0; BottomField == 1
    bool field_select = (PictureStructure == StructureBottomField);

    if (BufferFormat == FormatVideo420_Raster2B)
    {
        skip_part = (((Width + 31) >> 5) << 5) - Width;
    }

    SE_DEBUG(GetGroupTrace(), "%u %u %p %p %p\n", Width, Height, LumaAddress , CbAddress, CrAddress);
    /*init crc*/
    CRC_Y = 0xffffffff;
    CRC_C = 0xffffffff;
    {
        /* Now handles Fields and Frames in one code block */
        for (i = 0; i < Height; i++)
        {
            if ((i % 2 == field_select) || (PictureStructure == StructureFrame))
                for (j = 0; j < Width; j++)
                {
                    CRC_Y = (CRC_Y << 8) ^ CRC32Table[(CRC_Y >> 24) ^ *LumaAddress++];
                }
            else
            {
                LumaAddress += Width;    // Progress on by one line
            }

            LumaAddress += skip_part;
        }

        for (i = 0; i < Height / 2; i++)
        {
            if ((i % 2 == field_select) || (PictureStructure == StructureFrame))
            {
                for (j = 0; j < Width / 2; j++)
                {
                    CRC_C = (CRC_C << 8) ^ CRC32Table[(CRC_C >> 24) ^ *CbAddress++];

                    if (BufferFormat == FormatVideo420_Raster2B)
                    {
                        CRC_C = (CRC_C << 8) ^ CRC32Table[(CRC_C >> 24) ^ *CbAddress++];
                    }
                    else
                    {
                        CRC_C = (CRC_C << 8) ^ CRC32Table[(CRC_C >> 24) ^ *CrAddress++];
                    }
                }
            }
            else
            {
                CbAddress += Width;

                if (BufferFormat == FormatVideo420_Raster2B)
                {
                    CbAddress += Width;
                }
                else
                {
                    CrAddress += Width;
                }
            }

            CbAddress += skip_part;
            // Should this increment CrAddress also ? CrAddress+=skip_part;
        }
    }
    *LumaCRC  = CRC_Y;
    *ChromaCRC = CRC_C;
    return ManifestorNoError;
}

long long Manifestor_Video_c::GetPipelineLatency()
{
    return 80000; // 2 Vsyncs at 25fps
}



//
//  CheckFirstFrameOnDisplay
/// \brief                      Detect if the first frame on display is the first after a stream create
///                             or a stream switch. If yes, waits DISPLAY time.
void Manifestor_Video_c::CheckFirstFrameOnDisplay(void)
{
    if (mFirstFrameOnDisplay)
    {
        // Signal the First frame rendered event

        stm_event_t              streamEvtMngt;
        streamEvtMngt.event_id  = STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY;
        streamEvtMngt.object    = (stm_object_h)Stream->GetHavanaStream();
        // TODO:vgi any reason why it is not using normal SignalEvent path ?

        int err = stm_event_signal(&streamEvtMngt);
        if (err) { SE_ERROR("Stream 0x%p this 0x%p stm_event_signal failed (%d)\n", Stream, this, err); }

        SE_INFO(GetGroupTrace(), "Stream 0x%p this 0x%p First frame on display event\n" , Stream, this);

        mFirstFrameOnDisplay = false;
    }
}

