/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : manifestor_base.cpp
Author :           Julian

Implementation of the base manifestor class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
11-Jan-07   Created                                         Julian

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "manifestor_video.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define FOUR_BY_THREE                           Rational_t(4,3)
#define SIXTEEN_BY_NINE                         Rational_t(16,9)

#define MAX_SUPPORTED_VIDEO_CONTENT_WIDTH       2048
#define MAX_SUPPORTED_VIDEO_CONTENT_HEIGHT      1536

#define BUFFER_DECODE_BUFFER                    "VideoDecodeBuffer"
#define BUFFER_DECODE_BUFFER_TYPE               {BUFFER_DECODE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 1024, 1024, false, false, 0}
static BufferDataDescriptor_t                   InitialDecodeBufferDescriptor = BUFFER_DECODE_BUFFER_TYPE;

// /////////////////////////////////////////////////////////////////////////

//{{{  Thread entry stubs
static OS_TaskEntry (BufferReleaseThreadStub)
{
    Manifestor_Video_c* VideoManifestor = (Manifestor_Video_c*)Parameter;

    VideoManifestor->BufferReleaseThread ();
    OS_TerminateThread ();
    return NULL;
}

static OS_TaskEntry (DisplaySignalThreadStub)
{
    Manifestor_Video_c* VideoManifestor = (Manifestor_Video_c*)Parameter;

    VideoManifestor->DisplaySignalThread ();
    OS_TerminateThread ();
    return NULL;
}
//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor :
//      Action  : Initialise state
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_Video_c::Manifestor_Video_c  (void)
{
    //MANIFESTOR_DEBUG ("\n");
    if (InitializationStatus != ManifestorNoError)
    {
        MANIFESTOR_ERROR ("Initialization status not valid - aborting init\n");
        return;
    }

    InitializationStatus                = ManifestorError;

    Configuration.ManifestorName        = "Video";
    Configuration.StreamType            = StreamTypeVideo;
    Configuration.DecodeBufferDescriptor= &InitialDecodeBufferDescriptor;
    Configuration.PostProcessControlBufferCount = 16;

    DisplayAspectRatioPolicyValue       = 0xffffffff;
    DisplayFormatPolicyValue            = 0xffffffff;
    PixelAspectRatioCorrectionPolicyValue = PolicyValuePixelAspectRatioCorrectionDisabled;

    SurfaceWindow.X                     = 0;
    SurfaceWindow.Y                     = 0;
    SurfaceWindow.Width                 = 0;
    SurfaceWindow.Height                = 0;

    OldSurfaceWindow.X                  = 0;
    OldSurfaceWindow.Y                  = 0;
    OldSurfaceWindow.Width              = 0;
    OldSurfaceWindow.Height             = 0;

    InputCrop.X                         = 0;
    InputCrop.Y                         = 0;
    InputCrop.Width                     = 0;
    InputCrop.Height                    = 0;

    BufferReleaseThreadId               = OS_INVALID_THREAD;
    DisplaySignalThreadId               = OS_INVALID_THREAD;

    PtsOnDisplay                        = INVALID_TIME;

    OS_InitializeMutex (&BufferLock);

    RelayfsIndex = st_relayfs_getindex(ST_RELAY_SOURCE_VIDEO_MANIFESTOR);

    Manifestor_Video_c::Reset ();

    InitializationStatus                = ManifestorNoError;
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor :
//      Action  : Give up switch off the lights and go home
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_Video_c::~Manifestor_Video_c   (void)
{
    //MANIFESTOR_DEBUG ("\n");
    Manifestor_Video_c::Halt ();

    OS_TerminateMutex (&BufferLock);

    st_relayfs_freeindex(ST_RELAY_SOURCE_VIDEO_MANIFESTOR,RelayfsIndex);

}
//}}}
//{{{  Halt
//{{{  doxynote
/// \brief              Shutdown, stop presenting and retrieving frames
///                     don't return until complete
//}}}
ManifestorStatus_t      Manifestor_Video_c::Halt (void)
{
    MANIFESTOR_DEBUG ("Video\n");

    if (DisplaySignalThreadId != OS_INVALID_THREAD)
    {
        DisplaySignalThreadRunning              = false;
        OS_SemaphoreSignal    (&InitialFrameDisplayed);
        OS_SemaphoreSignal    (&BufferDisplayed);
        OS_WaitForEvent       (&DisplaySignalThreadTerminated, OS_INFINITE);    // Wait for display signal to exit
        OS_TerminateEvent     (&DisplaySignalThreadTerminated);
        OS_SemaphoreTerminate (&InitialFrameDisplayed);
        OS_SemaphoreTerminate (&BufferDisplayed);
        DisplaySignalThreadId                   = OS_INVALID_THREAD;
    }

    if (BufferReleaseThreadId != OS_INVALID_THREAD)
    {
        BufferReleaseThreadRunning              = false;
        OS_WaitForEvent       (&BufferReleaseThreadTerminated, OS_INFINITE);    // Wait for buffer release to exit
        OS_TerminateEvent     (&BufferReleaseThreadTerminated);
        OS_TerminateMutex     (&InitialFrameLock);
        BufferReleaseThreadId                   = OS_INVALID_THREAD;
    }

    return Manifestor_Base_c::Halt ();
}
//}}}
//{{{  Reset
/// \brief              Reset all state to intial values
ManifestorStatus_t Manifestor_Video_c::Reset (void)
{
    MANIFESTOR_DEBUG("\n");
    if (TestComponentState (ComponentRunning))
        Halt ();

    BufferOnDisplay                     = INVALID_BUFFER_ID;
    LastQueuedBuffer                    = ANY_BUFFER_ID;
    QueuedBufferCount                   = 0;
    NotQueuedBufferCount                = 0;
    DisplayUpdatePending                = false;
    DecimateIfAvailable                 = false;

    DequeueIn                           = 0;
    DequeueOut                          = 0;

    DisplayEvent.Code                   = EventIllegalIdentifier;
    DisplayEvent.Playback               = Playback;
    DisplayEvent.Stream                 = Stream;
    DisplayEventRequested               = 0;

    InitialFrameState                   = InitialFramePossible;
    NextTimeSlot                        = 0ull;
    TimeSlotOnDisplay                   = 0ull;
    FrameCount                          = 0ull;

    FatalHardwareError                  = false;
    FatalHardwareErrorSignalled         = false;

    Step                                = 0;
    Steps                               = 1;
    Stepping                            = false;

    return Manifestor_Base_c::Reset ();
}
//}}}
//{{{  GetDecodeBufferPool
//{{{  doxynote
/// \brief Allocate decode buffers, initialise them and hand them over to caller
/// \param Pool         Pointer to location for buffer pool pointer to hold
///                     details of the buffer pool holding the decode buffers.
/// \return             Succes or failure
//}}}
ManifestorStatus_t      Manifestor_Video_c::GetDecodeBufferPool         (class BufferPool_c**   Pool)
{
    unsigned int                        i;
    ManifestorStatus_t                  Status;

    MANIFESTOR_DEBUG ("\n");

    // Only create the pool if it doesn't exist and buffers have been created
    if (DecodeBufferPool != NULL)
    {
        *Pool   = DecodeBufferPool;
        return ManifestorNoError;
    }

    //{{{  Create buffer release thread
    if (BufferReleaseThreadId == OS_INVALID_THREAD)
    {
        if (OS_InitializeMutex (&InitialFrameLock) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR ("Failed to initialize InitialFrameLock mutex\n");
            return ManifestorError;
        }
        if (OS_InitializeEvent (&BufferReleaseThreadTerminated) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR ("Failed to initialize BufferReleaseThreadTerminated mutex\n");
            OS_TerminateMutex (&InitialFrameLock);
            return ManifestorError;
        }
        BufferReleaseThreadRunning      = true;
        if (OS_CreateThread (&BufferReleaseThreadId, BufferReleaseThreadStub, this, "Manifestor Buffer Release Thread", OS_MID_PRIORITY+16) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR("Unable to create Buffer Release thread\n");
            BufferReleaseThreadId       = OS_INVALID_THREAD;
            BufferReleaseThreadRunning  = false;
            OS_TerminateEvent (&BufferReleaseThreadTerminated);
            OS_TerminateMutex (&InitialFrameLock);
            return ManifestorError;
        }
    }
    //}}}
    //{{{  Create display signal thread
    if (DisplaySignalThreadId == OS_INVALID_THREAD)
    {
        if (OS_SemaphoreInitialize (&InitialFrameDisplayed, 0) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR ("Failed to initialize InitialFrameDisplayed semaphore\n");
            return ManifestorError;
        }
        if (OS_SemaphoreInitialize (&BufferDisplayed, 0) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR ("Failed to initialize BufferDisplayed semaphore\n");
            OS_SemaphoreTerminate (&InitialFrameDisplayed);
            return ManifestorError;
        }
        if (OS_InitializeEvent (&DisplaySignalThreadTerminated) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR ("Failed to initialize DisplaySignalThreadTerminated mutex\n");
            OS_SemaphoreTerminate (&InitialFrameDisplayed);
            OS_SemaphoreTerminate (&BufferDisplayed);
            return ManifestorError;
        }
        DisplaySignalThreadRunning      = true;
        if (OS_CreateThread (&DisplaySignalThreadId, DisplaySignalThreadStub, this, "Manifestor Display Signal Thread", (OS_HIGHEST_PRIORITY+OS_MID_PRIORITY)/2) != OS_NO_ERROR)
        {
            MANIFESTOR_ERROR("Unable to create Display Signal thread\n");
            DisplaySignalThreadId       = OS_INVALID_THREAD;
            DisplaySignalThreadRunning  = false;
            OS_TerminateEvent     (&DisplaySignalThreadTerminated);
            OS_SemaphoreTerminate (&InitialFrameDisplayed);
            OS_SemaphoreTerminate (&BufferDisplayed);
            return ManifestorError;
        }
    }
    //}}}

    for (i = 0; i < BufferConfiguration.MaxBufferCount; i++)
    {
        StreamBuffer[i].BufferIndex                 = i;
        StreamBuffer[i].QueueCount                  = 0;
        StreamBuffer[i].BufferState                 = BufferStateAvailable;
        StreamBuffer[i].BufferClass                 = NULL;
        StreamBuffer[i].OutputTiming                = NULL;
        StreamBuffer[i].EventPending                = false;
        StreamBuffer[i].Data                        = NULL;
    }

    Status      = Manifestor_Base_c::GetDecodeBufferPool( Pool );
    if( Status != ManifestorNoError )
    {
        MANIFESTOR_ERROR ("Failed to create a pool of decode buffers.\n");
        DecodeBufferPool        = NULL;
        return ManifestorError;
    }

    BufferOnDisplay                     = INVALID_BUFFER_ID;
    LastQueuedBuffer                    = ANY_BUFFER_ID;
    QueuedBufferCount                   = 0;
    NotQueuedBufferCount                = 0;

    // Initialise details of surface
    if ((SurfaceWindow.Width == 0) || (SurfaceWindow.Height == 0))
    {
        SurfaceWindow.X                 = 0;
        SurfaceWindow.Y                 = 0;
        SurfaceWindow.Width             = SurfaceDescriptor.DisplayWidth;
        SurfaceWindow.Height            = SurfaceDescriptor.DisplayHeight;
        OldSurfaceWindow.X              = 0;
        OldSurfaceWindow.Y              = 0;
        OldSurfaceWindow.Width          = SurfaceDescriptor.DisplayWidth;
        OldSurfaceWindow.Height         = SurfaceDescriptor.DisplayHeight;
    }
    MANIFESTOR_DEBUG("Surface X %d, Y %d, Width %d, Height %d\n", SurfaceWindow.X, SurfaceWindow.Y, SurfaceWindow.Width, SurfaceWindow.Height);

    // Setup input window - portion of incoming content to copy to output window
    InputWindow.X                       = 0;
    InputWindow.Y                       = 0;
    InputWindow.Width                   = SurfaceDescriptor.DisplayWidth;
    InputWindow.Height                  = SurfaceDescriptor.DisplayHeight;

    // Setup output window - portion of surface content is to be rendered on
    OutputWindow.X                      = 0;
    OutputWindow.Y                      = 0;
    OutputWindow.Width                  = SurfaceDescriptor.DisplayWidth;
    OutputWindow.Height                 = SurfaceDescriptor.DisplayHeight;

    // Setup input crop - portion of incoming content user is interested in
    InputCrop.X                          = 0;
    InputCrop.Y                          = 0;
    InputCrop.Width                      = 0;
    InputCrop.Height                     = 0;

    memset ((void*)&StreamDisplayParameters, 0, sizeof (struct VideoDisplayParameters_s));
    StreamDisplayParameters.PixelAspectRatio    = 0;
    StreamDisplayParameters.FrameRate           = 0;


    // Let outside world know about the created pool

    SetComponentState (ComponentRunning);

    return ManifestorNoError;
}
//}}}
//{{{  GetSurfaceParameters
//{{{  doxynote
/// \brief      Fill in private structure with timing details of display surface
/// \param      SurfaceParameters pointer to structure to complete
//}}}
ManifestorStatus_t      Manifestor_Video_c::GetSurfaceParameters (void** SurfaceParameters )
{
    MANIFESTOR_DEBUG ("\n");

    UpdateOutputSurfaceDescriptor();

    *SurfaceParameters  = (void*)&SurfaceDescriptor;
    return ManifestorNoError;
}


//}}}
//{{{  GetNextQueuedManifestationTime
//{{{  doxynote
/// \brief              Return the earliest system time at which the next frame to be queued will be manifested
///                     If this is initial frame the time is guessed as two frame periods on from vsync
///                     when the initial frame goes on the display
/// \param Time         Pointer to 64-bit system time variable
/// \result             Success or fail if no time can be inferred
//}}}
ManifestorStatus_t      Manifestor_Video_c::GetNextQueuedManifestationTime   (unsigned long long*    Time)
{
    unsigned long long          Now             = OS_GetTimeInMicroSeconds ();
    Rational_t                  Period          = 1000000 / SurfaceDescriptor.FrameRate;
    Rational_t                  Behind;


    // No buffer on display wait for initial frame if pending
    OS_LockMutex (&InitialFrameLock);
    if ((BufferOnDisplay == INVALID_BUFFER_ID) && (InitialFrameState == InitialFrameQueued))
        OS_SemaphoreWait (&InitialFrameDisplayed);
    OS_UnLockMutex (&InitialFrameLock);

    // No buffer on display and initial frame not available (or aborted) - return now plus 4 field periods at 50Hz
    if (BufferOnDisplay == INVALID_BUFFER_ID)
    {
        *Time           = OS_GetTimeInMicroSeconds () + 80000;
        MANIFESTOR_DEBUG ("Estimate %llu\n", *Time);
        return ManifestorNoError;
    }

    // The initial frame is still on display and no other frames have been queued
    if ((InitialFrameState == InitialFrameQueued) && (NextTimeSlot == 0ull))
    {
        unsigned long long      Vsync           = StreamBuffer[BufferOnDisplay].TimeOnDisplay;

        Now             = OS_GetTimeInMicroSeconds ();
        if (Vsync == 0)
            Vsync       = Now;
        Behind          = (Now - Vsync) / Period;
        Period          = (Behind.IntegerPart() + 5) * Period;          // we want to be at least 4 vsyncs ahead
        *Time           = Vsync + Period.RoundedLongLongIntegerPart();
        NextTimeSlot    = *Time;                                        // Initialize timeslot.
        MANIFESTOR_TRACE ("0 - Estimate %llu, Time from now %u, Time since Vsync %u, FrameRate %d.%06d\n", *Time,
                          (unsigned int)(*Time - Now), (unsigned int)(Now - Vsync),
                          SurfaceDescriptor.FrameRate.IntegerPart(), SurfaceDescriptor.FrameRate.RemainderDecimal());
	Behind.Print();
	Period.Print();
        return ManifestorNoError;
    }

    // Check to see if we have queued some buffers but they have not yet reached the display
    if ((NextTimeSlot != 0ull) && (TimeSlotOnDisplay == 0ull))
    {
        *Time           = NextTimeSlot;
        MANIFESTOR_TRACE ("1 - Estimate %llu, Time from now %u\n", *Time, (unsigned int)(*Time - Now));
	Period.Print();
        return ManifestorNoError;
    }

    // Latest display time is the next available time slot adjusted by the difference between
    // time current buffer was put on display and time it was supposed to go on display
    Now         = OS_GetTimeInMicroSeconds ();
    *Time       = NextTimeSlot + (StreamBuffer[BufferOnDisplay].TimeOnDisplay - TimeSlotOnDisplay);
    if (*Time < (Now + RoundedLongLongIntegerPart(4 * Period)))
    {
        Behind          = (Now - *Time) / Period;
        Period          = (Behind.IntegerPart() + 5) * Period;        // we want to be at least 4 vsyncs ahead
        *Time          += Period.RoundedLongLongIntegerPart();
    }
    MANIFESTOR_TRACE ("2 - NextTimeSlot %llu, TimeSlotOnDisplay %llu Buff.TimeOnDisplay %llu  Estimate %llu, Now %llu diff %d\n",
                      NextTimeSlot, TimeSlotOnDisplay, StreamBuffer[BufferOnDisplay].TimeOnDisplay,
                      *Time, Now, *Time - Now);
    Behind.Print();
    Period.Print();

    return ManifestorNoError;
}
//{{{  ReleaseQueuedDecodeBuffers
ManifestorStatus_t      Manifestor_Video_c::ReleaseQueuedDecodeBuffers(void)
{
    NextTimeSlot	= TimeSlotOnDisplay;
    return Manifestor_Base_c::ReleaseQueuedDecodeBuffers();
}
//}}}
//{{{  InitialFrame
//{{{  doxynote
/// \brief              Queue the first field of the first picture to the display
/// \param              Buffer containing frame to be displayed
/// \result             Success if frame displayed, fail if not displayed or a frame has already been displayed
//}}}
ManifestorStatus_t      Manifestor_Video_c::InitialFrame       (class Buffer_c*         Buffer)
{
    MANIFESTOR_DEBUG("%d\n", __LINE__);
    if (InitialFrameState == InitialFramePossible)
    {
        struct StreamBuffer_s*              StreamBuff;
        BufferStatus_t                      BufferStatus;
        struct ParsedFrameParameters_s*     FrameParameters;
        struct ParsedVideoParameters_s*     VideoParameters;
        struct BufferStructure_s*           BufferStructure;
        unsigned int                        BufferIndex;
        ManifestorStatus_t                  Status;

        AssertComponentState ("Manifestor_Video_c::InitialFrame", ComponentRunning);

        OS_LockMutex (&BufferLock);

        BufferStatus                    = Buffer->GetIndex (&BufferIndex);
        if (BufferStatus != BufferNoError)
        {
            MANIFESTOR_ERROR ("Buffer not accessible %x.\n", BufferStatus);
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        // Check buffer state
        StreamBuff                      = &StreamBuffer[BufferIndex];
        if (StreamBuff->BufferState != BufferStateAvailable)
        {
            MANIFESTOR_ERROR ("Buffer already being manifested %d %d.\n", BufferIndex, StreamBuff->BufferState);
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        BufferStatus                    = Buffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersReferenceType, (void**)&FrameParameters);
        if (BufferStatus != BufferNoError)
        {
            MANIFESTOR_ERROR ("Unable to access buffer parsed frame parameters %x.\n", BufferStatus);
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        BufferStatus                    = Buffer->ObtainMetaDataReference (Player->MetaDataParsedVideoParametersType, (void**)&VideoParameters);
        if (BufferStatus != BufferNoError)
        {
            MANIFESTOR_ERROR ("Unable to access buffer parsed video parameters %x.\n", BufferStatus);
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        BufferStatus                = Buffer->ObtainMetaDataReference (Player->MetaDataBufferStructureType, (void**)&BufferStructure);
        if (BufferStatus != BufferNoError)
        {
            MANIFESTOR_ERROR ("Unable to access buffer structure parameters %x.\n", BufferStatus);
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        BufferStatus                    = Buffer->ObtainDataReference (NULL, NULL, (void**)(&StreamBuffer[BufferIndex].Data), PhysicalAddress);
        StreamBuff->BufferClass         = Buffer;
        StreamBuff->OutputTiming        = NULL;
        StreamBuff->EventPending        = false;

        // Queue the buffer onto the display
        Status  = SetDisplayWindows (&VideoParameters->Content);
        if (Status != ManifestorNoError)
        {
            MANIFESTOR_ERROR ("Unable to configure display for stream.\n");
            OS_UnLockMutex (&BufferLock);
            if (Status == ManifestorUnplayable)
                Player->MarkStreamUnPlayable (Stream);
            return Status;
        }
        StreamBuff->DecimateIfAvailable = DecimateIfAvailable;

        if (QueueInitialFrame (BufferIndex, VideoParameters, BufferStructure) != ManifestorNoError)
        {
            MANIFESTOR_ERROR ("Unable to queue initial buffer to display.\n");
            OS_UnLockMutex (&BufferLock);
            return ManifestorError;
        }

        memset ((void*)&StreamDisplayParameters, 0, sizeof (struct VideoDisplayParameters_s));
        StreamDisplayParameters.PixelAspectRatio    = 0;
        StreamDisplayParameters.FrameRate           = 0;

        OS_UnLockMutex (&BufferLock);
        return ManifestorNoError;
    }

    return ManifestorError;
}
//}}}
//{{{  _QueueDecodeBuffer
//{{{  doxynote
/// \brief              Receive a decode buffer for display and send it with appropraite
///                     flags to the driver
/// \param              Buffer containing decoded frame witha attached display meta data
/// \result             Success if frame displayed, fail if not displayed
//}}}
ManifestorStatus_t      Manifestor_Video_c::_QueueDecodeBuffer   (class Buffer_c*        Buffer)
{
    struct StreamBuffer_s*              StreamBuff;
    BufferStatus_t                      BufferStatus;
    struct ParsedFrameParameters_s*     FrameParameters;
    struct ParsedVideoParameters_s*     VideoParameters;
    struct BufferStructure_s*           BufferStructure;
    struct VideoOutputTiming_s*         VideoOutputTiming       = NULL;
    unsigned int                        BufferIndex;

    //MANIFESTOR_DEBUG("\n");

    BufferStatus                = Buffer->GetIndex (&BufferIndex);
    if (BufferStatus != BufferNoError)
    {
        MANIFESTOR_ERROR ("Buffer not accessible %x.\n", BufferStatus);
        return ManifestorError;
    }

    // Check buffer state
    StreamBuff                  = &StreamBuffer[BufferIndex];
    if ((StreamBuff->BufferState != BufferStateAvailable) && (StreamBuff->BufferState != BufferStateMultiQueue))
    {
        MANIFESTOR_ERROR ("Buffer already being manifested %d %d.\n", BufferIndex, StreamBuff->BufferState);
        return ManifestorError;
    }

    BufferStatus                = Buffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersReferenceType, (void**)&FrameParameters);
    if (BufferStatus != BufferNoError)
    {
        MANIFESTOR_ERROR ("Unable to access buffer parsed frame parameters %x.\n", BufferStatus);
        return ManifestorError;
    }
    BufferStatus                = Buffer->ObtainMetaDataReference (Player->MetaDataParsedVideoParametersType, (void**)&VideoParameters);
    if (BufferStatus != BufferNoError)
    {
        MANIFESTOR_ERROR ("Unable to access buffer parsed video parameters %x.\n", BufferStatus);
        return ManifestorError;
    }

    Buffer->DumpToRelayFS(ST_RELAY_TYPE_DECODED_VIDEO_BUFFER, ST_RELAY_SOURCE_VIDEO_MANIFESTOR + RelayfsIndex, (void*)Player);

    BufferStatus                = Buffer->ObtainMetaDataReference (Player->MetaDataVideoOutputTimingType, (void**)&VideoOutputTiming);
    if (BufferStatus != BufferNoError)
    {
        MANIFESTOR_ERROR ("Unable to access buffer video output timing parameters %x.\n", BufferStatus);
        return ManifestorError;
    }

    BufferStatus                = Buffer->ObtainMetaDataReference (Player->MetaDataBufferStructureType, (void**)&BufferStructure);
    if (BufferStatus != BufferNoError)
    {
        MANIFESTOR_ERROR ("Unable to access buffer structure parameters %x.\n", BufferStatus);
        return ManifestorError;
    }

    BufferStatus                = Buffer->ObtainDataReference (NULL, NULL, (void**)(&StreamBuffer[BufferIndex].Data), PhysicalAddress);

    StreamBuff->BufferClass     = Buffer;
    StreamBuff->OutputTiming    = VideoOutputTiming;
    StreamBuff->EventPending    = EventPending;
    EventPending                = false;
    if (StreamBuff->BufferState == BufferStateAvailable)
        StreamBuff->BufferState = BufferStateNotQueued;

    // Queue the buffer onto the display
    if ((VideoOutputTiming->DisplayCount[0] > 0) || (VideoOutputTiming->DisplayCount[1] > 0))
    {
        ManifestorStatus_t      Status;

        // Some events may have been queued so we need to insert a dummy event into the event queue
        DisplayEventRequested           = 0;
        Status                          = SetDisplayWindows (&VideoParameters->Content);
        if (Status != ManifestorNoError)
        {
            MANIFESTOR_ERROR ("Unable to configure display for stream.\n");
            if (StreamBuff->BufferState == BufferStateNotQueued)
                StreamBuff->BufferState = BufferStateAvailable;
            if (Status == ManifestorUnplayable)
                Player->MarkStreamUnPlayable (Stream);
            return Status;
        }
        StreamBuff->DecimateIfAvailable = DecimateIfAvailable;

        //memcpy ((void*)&StreamPanScan, (void*)&VideoParameters->PanScan, sizeof(struct PanScan_s));
        StreamBuff->TimeSlot            = ValidTime    (VideoOutputTiming->SystemPlaybackTime) ?
                                                                VideoOutputTiming->SystemPlaybackTime :
                                                                NextTimeSlot;
        StreamBuff->NativePlaybackTime  = FrameParameters->NativePlaybackTime;

        Status                          = QueueBuffer  (BufferIndex,
                                                        FrameParameters,
                                                        VideoParameters,
                                                        VideoOutputTiming,
                                                        BufferStructure);
        if (Status != ManifestorNoError)
        {
            NotQueuedBufferCount++;
            MANIFESTOR_ERROR ("Failed to queue buffer %d to display.\n", BufferIndex);
            DisplayEventRequested       = 0;
            EventPending                = false;
            //return ManifestorNoError;
        }
        else if (StreamBuff->BufferState == BufferStateNotQueued)
        {
            StreamBuff->BufferState     = BufferStateQueued;
            QueuedBufferCount++;
            LastQueuedBuffer            = BufferIndex;
        }
        NextTimeSlot                    = StreamBuff->TimeSlot + VideoOutputTiming->ExpectedDurationTime;
    }
    else
    {
        MANIFESTOR_DEBUG("NOT Q %d.\n", BufferIndex);
        NotQueuedBufferCount++;
    }

    return ManifestorNoError;
}
//}}}
//{{{  QueueDecodeBuffer
//{{{  doxynote
/// \brief              Receive a decode buffer for display and send it with appropraite
///                     flags to the driver
/// \param              Buffer containing decoded frame witha attached display meta data
/// \result             Success if frame displayed, fail if not displayed
//}}}
ManifestorStatus_t      Manifestor_Video_c::QueueDecodeBuffer   (class Buffer_c*        Buffer)
{
    ManifestorStatus_t  Status;

    AssertComponentState ("Manifestor_Video_c::QueueDecodeBuffer", ComponentRunning);

    OS_LockMutex (&BufferLock);
    Status      = _QueueDecodeBuffer (Buffer);
    OS_UnLockMutex (&BufferLock);

    return Status;
}
//}}}
//{{{  GetBufferId
//{{{  doxynote
/// \brief      Retrieve buffer index of last buffer queued. If no buffers
///             yet queued returns ANY_BUFFER_ID
/// \return     Buffer index of last buffer sent for display
//}}}
unsigned int    Manifestor_Video_c::GetBufferId    (void)
{
    return LastQueuedBuffer;
}
//}}}
//{{{  GetNativeTimeOfCurrentlyManifestedFrame
//{{{  doxynote
/// \brief Get original PTS of currently visible buffer
/// \param Pts          Pointer to Pts variable
/// \return             Success if Pts value is available
//}}}
ManifestorStatus_t Manifestor_Video_c::GetNativeTimeOfCurrentlyManifestedFrame (unsigned long long* Time)
{
    MANIFESTOR_DEBUG ("\n");

    *Time       = PtsOnDisplay;
    if (PtsOnDisplay == INVALID_TIME)
    {
        MANIFESTOR_ERROR ("No buffer on display.\n");
        return ManifestorError;
    }

    return ManifestorNoError;
}
//}}}
//{{{  SetOutputWindow
//{{{  doxynote
/// \brief              Specify output window
/// \param              The coordinates and size of desired window
//}}}
ManifestorStatus_t   Manifestor_Video_c::SetOutputWindow       (unsigned int            X,
                                                                unsigned int            Y,
                                                                unsigned int            Width,
                                                                unsigned int            Height)
{
    OS_LockMutex (&BufferLock);

    if( (X == 0) && (Y == 0) && (Width == 0) && (Height == 0) )
    {
        SurfaceWindow.X         = 0;
        SurfaceWindow.Y         = 0;
        SurfaceWindow.Width     = SurfaceDescriptor.DisplayWidth;
        SurfaceWindow.Height    = SurfaceDescriptor.DisplayHeight;
    }
    else
    {
        if (X > SurfaceDescriptor.DisplayWidth)
            X                   = 0;
        if (Y > SurfaceDescriptor.DisplayHeight)
            Y                   = 0;
        if ((X + Width) > SurfaceDescriptor.DisplayWidth)
            Width               = SurfaceDescriptor.DisplayWidth - X;
        if ((Y + Height) > SurfaceDescriptor.DisplayHeight)
            Height              = SurfaceDescriptor.DisplayHeight - Y;

        SurfaceWindow.X         = X;
        SurfaceWindow.Y         = Y;
        SurfaceWindow.Width     = Width;
        SurfaceWindow.Height    = Height;
    }

    Step                        = 0;
    Steps                       = Player->PolicyValue (Playback, Stream, PolicyVideoOutputWindowResizeSteps);
    if (Steps > MAX_RESIZE_STEPS)
        Steps                   = MAX_RESIZE_STEPS;
    Stepping                    = Steps > 1;

    memset ((void*)&StreamDisplayParameters, 0, sizeof (struct VideoDisplayParameters_s));
    StreamDisplayParameters.PixelAspectRatio    = 0;
    StreamDisplayParameters.FrameRate           = 0;

    RequeueBufferOnDisplayIfNecessary ();

    OS_UnLockMutex (&BufferLock);
    MANIFESTOR_DEBUG("%dx%d at %d,%d (Steps(%d) = %d)\n", Width, Height, X, Y, Stepping, Steps);

    return ManifestorNoError;
}
//}}}
//{{{  GetOutputWindow
//{{{  doxynote
/// \brief              Retrieve output window
/// \param              Places to save coordinates and size of desired window
//}}}
ManifestorStatus_t   Manifestor_Video_c::GetOutputWindow       (unsigned int*           X,
                                                                unsigned int*           Y,
                                                                unsigned int*           Width,
                                                                unsigned int*           Height)
{

    *X          = SurfaceWindow.X;
    *Y          = SurfaceWindow.Y;
    *Width      = SurfaceWindow.Width;
    *Height     = SurfaceWindow.Height;

    MANIFESTOR_DEBUG("%dx%d at %d,%d\n", *Width, *Height, *X, *Y);

    return ManifestorNoError;
}
//}}}
//{{{  SetInputWindow
//{{{  doxynote
/// \brief              Specify input crop
/// \param              The coordinates and size of desired window
//}}}
ManifestorStatus_t   Manifestor_Video_c::SetInputWindow        (unsigned int            X,
                                                                unsigned int            Y,
                                                                unsigned int            Width,
                                                                unsigned int            Height)
{
    ManifestorStatus_t  Status;

    MANIFESTOR_DEBUG("%dx%d at %d,%d\n", Width, Height, X, Y);

    Status      = CheckInputDimensions (Width, Height);
    if (Status != ManifestorNoError)
    {
        MANIFESTOR_ERROR("Unsupported window dimensions %dx%d\n", Width, Height);
        return Status;
    }

    InputCrop.X         = X;
    InputCrop.Y         = Y;
    InputCrop.Width     = Width;
    InputCrop.Height    = Height;


    memset ((void*)&StreamDisplayParameters, 0, sizeof (struct VideoDisplayParameters_s));
    StreamDisplayParameters.PixelAspectRatio    = 0;
    StreamDisplayParameters.FrameRate           = 0;

    return ManifestorNoError;
}
//}}}
//{{{  GetFrameCount
//{{{  doxynote
/// \brief Get number of frames appeared on the display
/// \param Framecount   Pointer to FrameCount variable
/// \return             Success
//}}}
ManifestorStatus_t Manifestor_Video_c::GetFrameCount (unsigned long long* FrameCount)
{
    //MANIFESTOR_DEBUG ("\n");

    *FrameCount         = this->FrameCount;

    return ManifestorNoError;
}
//}}}

//{{{  RequeueBufferOnDisplayIfNecessary
//{{{  doxynote
/// \brief              Requeue visible buffer if size has changed while paused
//}}}
ManifestorStatus_t   Manifestor_Video_c::RequeueBufferOnDisplayIfNecessary (void)
{
    Rational_t                          Speed;
    PlayDirection_t                     Direction;
    unsigned long long                  TimeSlot                        = 0ull;
    unsigned int                        BufferIndex;
    ManifestorStatus_t                  Status                  = ManifestorNoError;

    //OS_LockMutex (&BufferLock);

    MANIFESTOR_DEBUG("LastQueuedBuffer = %d\n", LastQueuedBuffer);
    BufferIndex                          = INVALID_INDEX;
    Player->GetPlaybackSpeed (Playback, &Speed, &Direction);
    if (Speed != 0)
        return ManifestorNoError;

    for (unsigned int i = 0; i < BufferConfiguration.MaxBufferCount; i++)
    {
        if ((StreamBuffer[i].BufferState == BufferStateQueued) || (StreamBuffer[i].BufferState == BufferStateMultiQueue))
        {
            MANIFESTOR_DEBUG("Buffer %d queued state = %x QueueCount = %d Timeslot = %llu\n",
                StreamBuffer[i].BufferIndex, StreamBuffer[i].BufferState, StreamBuffer[i].QueueCount, StreamBuffer[i].TimeSlot);
            if (StreamBuffer[i].TimeSlot > TimeSlot)
            {
                TimeSlot                = StreamBuffer[i].TimeSlot;
                BufferIndex             = i;
            }
        }
    }

    if (BufferIndex != INVALID_INDEX)
    {
        AssertComponentState ("Manifestor_Video_c::RequeueBufferOnDisplayIfNecessary", ComponentRunning);

        MANIFESTOR_DEBUG("Stream Buffer %d selected for requeue, queuecount = %d\n", BufferIndex, StreamBuffer[BufferIndex].QueueCount);
        //Stepping                                        = false;
        //Steps                                           = 1;
        StreamBuffer[BufferIndex].BufferState           = BufferStateMultiQueue;
        Status                                          = _QueueDecodeBuffer   (StreamBuffer[BufferIndex].BufferClass);
        RequeuedBufferIndex                             = BufferIndex;
    }

    //OS_UnLockMutex (&BufferLock);

    return ManifestorNoError;
}
//}}}
//{{{  SetDisplayWindows
//{{{  doxynote
/// \brief              Calculate correct input and output windows given source size, aspect ratio and
///                     surface size and aspect ratio and output window size and aspect ratio.  Shap is
///                     determined by fullscreen, centre cut out or letter box display policy.
/// \param              Pointer to video parameters parsed fromstream
//}}}
//{{{  doxynote Aspect ratios
/// \note       If the picture is the same  shape as the display the input window will be the whole picture
///             and the output window will be the desired target area
///
///             PanScan              For pan and scan we use the frame_centre_horizontal_offset and
///                              frame_centre_vertical_offset information associated with the frame.
///                              determine the input and output windows.
///
///             Letterbox            For letterbox the whole picture should be displayed so the input window
///                              is the whole frame.
///                                  For a widescreen picture on a 4:3 TV the output width is the display width
///                              and the output height is derived from the frame aspect ratio.
///                                  For a 4x3 picture on a widescreen TV the output height is the display height
///                              and the output width is derived from the frame aspect ratio.
///                                  The areas not covered by the output window should be displayed black.
///
///             CentreCutOut         For center cut out the picture should be trimmed to fit while maintaining the
///             .                correct aspect ratio
///                                  For a widescreen picture on a 4:3 TV the output height is the display height
///                              and the output width is 4 thirds of the height.
///                                  For a 4x3 picture on a widescreen TV the output width is the display width
///                              and the output height is 3 quarters of the width.
///           Examples
///             CentreCutOut
///                              tv is 4:3       content is 16:9
///
///                                  Source width conversion from 720
///                                      720 * 3 / 4     = 540   * 16/ 9     = 960
///                                  Result: cut off 120 from left and right
///                                          540x480 NTSC, 540x576 PAL
///
///                              tv is 16:9      content is 4:3
///
///                                  Source height conversion from 480
///                                      480 * 3 / 4     = 360   * 9 / 16    = 640
///                                  Result: cut off 80 top and bottom
///                                          720x320 NTSC
///
///                                  Source height conversion from 576
///                                      576 * 3 / 4     = 432   * 16/ 9     = 768
///                                  Result: cut off 96 top and bottom
///                                          720x384 PAL
///
///             Letterbox        tv is 4:3       content is 16:9
///
///                                  Destination height conversion from 480
///                                      480 * 4 / 3     = 640   * 9 / 16    = 360
///                                  Result: black bars for 60 top and bottom
///
///                                  Destination height conversion from 576
///                                      576 * 4 / 3     = 768   * 9 / 16    = 432
///                                  Result: black bars for 72 top and bottom
///
///                              tv is 16:9      content is 4:3
///
///                                  Destination width conversion from 720
///                                      720 * 4 / 3     = 960   * 9 / 16    = 540
///                                  Result: black bars for 90 at sides
//}}}
ManifestorStatus_t Manifestor_Video_c::SetDisplayWindows (struct VideoDisplayParameters_s*   VideoParameters)
{
    int                         SourceX,                SourceY;
    int                         SourceWidth,            SourceHeight;
    int                         DestX,                  DestY;
    int                         DestWidth,              DestHeight;

    Rational_t                  PictureAspectRatio;
    Rational_t                  WindowAspectRatio;

    //MANIFESTOR_DEBUG("\n");

    // Before checking if the display parameters have changed, we copy over 
    // the content frame rate to avoid excess message generation.
    if (VideoParameters->FrameRate != StreamDisplayParameters.FrameRate)
        DisplayEventRequested          |= EventSourceFrameRateChangeManifest;

    StreamDisplayParameters.FrameRate   = VideoParameters->FrameRate;

    if (!Stepping &&
        (Player->PolicyValue (Playback, Stream, PolicyDisplayAspectRatio)         == DisplayAspectRatioPolicyValue)  &&
        (Player->PolicyValue (Playback, Stream, PolicyDisplayFormat)              == DisplayFormatPolicyValue)       &&
        (Player->PolicyValue (Playback, Stream, PolicyPixelAspectRatioCorrection) == PixelAspectRatioCorrectionPolicyValue) &&
        (memcmp ((void*)&StreamDisplayParameters, (void*)VideoParameters, sizeof(struct VideoDisplayParameters_s)) == 0))
        return ManifestorNoError;

    DisplayAspectRatioPolicyValue       = Player->PolicyValue (Playback, Stream, PolicyDisplayAspectRatio);
    DisplayFormatPolicyValue            = Player->PolicyValue (Playback, Stream, PolicyDisplayFormat);
    PixelAspectRatioCorrectionPolicyValue = Player->PolicyValue (Playback, Stream, PolicyPixelAspectRatioCorrection);

    //MANIFESTOR_DEBUG("Size %dx%d, Display %dx%d, Prog %d, Scan %d, Rate %d.%06d, PAR %d.%06d\n",
    //        StreamDisplayParameters.Width, StreamDisplayParameters.Height,
    //        StreamDisplayParameters.DisplayWidth, StreamDisplayParameters.DisplayHeight,
    //        StreamDisplayParameters.Progressive, StreamDisplayParameters.OverscanAppropriate,
    //        StreamDisplayParameters.FrameRate.IntegerPart(), StreamDisplayParameters.FrameRate.RemainderDecimal(),
    //        StreamDisplayParameters.PixelAspectRatio.IntegerPart(), StreamDisplayParameters.PixelAspectRatio.RemainderDecimal() );

    // Init default settings to display whole picture full screen or
    // if an input crop window has been specified that overrides the default settings
    // The window aspect ratio is the shape of the television - not the wxh pixels
    // Check whether the display width/height overrides aspect ratio info (for 4x3 display only)
    // Aspect ratio information is completely worked out by the frame parser so is no longer used here
    if (0 && ((VideoParameters->DisplayWidth != 0) || (VideoParameters->DisplayHeight != 0)) &&
       (DisplayAspectRatioPolicyValue == PolicyValue4x3))
    {
        SourceWidth             = (int)VideoParameters->DisplayWidth;
        SourceHeight            = (int)VideoParameters->DisplayHeight;
        SourceX                 = ((int)VideoParameters->Width - SourceWidth) / 2;
        SourceY                 = ((int)VideoParameters->Height - SourceHeight) / 2;
    }
    else if ((InputCrop.Width != 0) && (InputCrop.Height != 0))
    {
        if (InputCrop.X > VideoParameters->Width)
            InputCrop.X         = 0;
        if (InputCrop.Y > VideoParameters->Height)
            InputCrop.Y         = VideoParameters->Height;

        if ((InputCrop.X + InputCrop.Width) > VideoParameters->Width)
            InputCrop.Width     = VideoParameters->Width - InputCrop.X;
        if ((InputCrop.Y + InputCrop.Height) > VideoParameters->Height)
            InputCrop.Height    = VideoParameters->Height - InputCrop.Y;

        SourceX                 = InputCrop.X;
        SourceY                 = InputCrop.Y;
        SourceWidth             = InputCrop.Width;
        SourceHeight            = InputCrop.Height;
    }
    else
    {
        SourceX                 = 0;
        SourceY                 = 0;
        SourceWidth             = (int)VideoParameters->Width;
        SourceHeight            = (int)VideoParameters->Height;
    }
    //{{{  work out initial output window guess
    if (!Stepping || (++Step == Steps))
    {
        Stepping                = false;

        OldSurfaceWindow.X      = SurfaceWindow.X;
        OldSurfaceWindow.Y      = SurfaceWindow.Y;
        OldSurfaceWindow.Width  = SurfaceWindow.Width;
        OldSurfaceWindow.Height = SurfaceWindow.Height;

        DestX                   = SurfaceWindow.X;
        DestY                   = SurfaceWindow.Y;
        DestWidth               = SurfaceWindow.Width;
        DestHeight              = SurfaceWindow.Height;
    }
    else
    {
        DestX                   = ((OldSurfaceWindow.X      * (Steps - Step)) + (SurfaceWindow.X      * Step)) / Steps;
        DestY                   = ((OldSurfaceWindow.Y      * (Steps - Step)) + (SurfaceWindow.Y      * Step)) / Steps;
        DestWidth               = ((((OldSurfaceWindow.Width  * (Steps - Step)) + (SurfaceWindow.Width  * Step)) / Steps) + 1) & 0xfffffffe;  // Force width to even number of pixels
        DestHeight              = ((OldSurfaceWindow.Height * (Steps - Step)) + (SurfaceWindow.Height * Step)) / Steps;
    }
    //}}}

    PictureAspectRatio          = Rational_t(SourceWidth, SourceHeight) * VideoParameters->PixelAspectRatio;
    WindowAspectRatio           = (DisplayAspectRatioPolicyValue == PolicyValue4x3) ? FOUR_BY_THREE : SIXTEEN_BY_NINE;      // Display aspect ratio

    if (!Stepping)
    {
        // Prints have been changed to reduce time taken to do them and lock interrupts for shorter periods as 
        // on occasion this has effected our AV sync calculations on startup.
      
        report(severity_info,"Incoming Source %dx%d @ %d,%d\n",
	    VideoParameters->Width, VideoParameters->Height,
	     SourceX, SourceY);

	report(severity_info,"Display Size %dx%d @ %d,%d\n",
	     DestWidth, DestHeight,
	     DestX, DestY);

        report(severity_info,"%s Content, FrameRate %d.%02d, PixelAspectRatio %d.%02d\n",
            VideoParameters->Progressive ? "Progressive" : "Interlaced",
            VideoParameters->FrameRate.IntegerPart(), VideoParameters->FrameRate.RemainderDecimal(),
	    VideoParameters->PixelAspectRatio.IntegerPart(), VideoParameters->PixelAspectRatio.RemainderDecimal());
	/*
        MANIFESTOR_DEBUG("Incoming Source %dx%d (%dx%d), at %d,%d, Dest %dx%d at %d,%d\n",
            VideoParameters->Width, VideoParameters->Height,
            VideoParameters->DisplayWidth, VideoParameters->DisplayHeight,
            SourceX, SourceY,
            DestWidth, DestHeight, DestX, DestY);
        MANIFESTOR_DEBUG("Content is %s with FrameRate %d.%06d, PixelAspectRatio %d.%06d, VideoFullRange = %d, MatrixCoefficients = %d\n",
            VideoParameters->Progressive ? "progressive" : "interlaced",
            VideoParameters->FrameRate.IntegerPart(), VideoParameters->FrameRate.RemainderDecimal(),
            VideoParameters->PixelAspectRatio.IntegerPart(), VideoParameters->PixelAspectRatio.RemainderDecimal(),
            VideoParameters->VideoFullRange, VideoParameters->ColourMatrixCoefficients );
	*/
    }
    if ((SourceWidth > MAX_SUPPORTED_VIDEO_CONTENT_WIDTH)  | (SourceHeight > MAX_SUPPORTED_VIDEO_CONTENT_HEIGHT))
    {
        MANIFESTOR_ERROR ("Infeasible source dimensions %dx%d\n", SourceWidth, SourceHeight);
        return ManifestorUnplayable;
    }


    if ((DestWidth != (int)SurfaceDescriptor.DisplayWidth) || (DestHeight != (int)SurfaceDescriptor.DisplayHeight))              // See if full screen
    {
        Rational_t ScreenAspectRatio          = Rational_t(SurfaceDescriptor.DisplayWidth, SurfaceDescriptor.DisplayHeight);
        Rational_t DisplayPixelAspectRatio    = ScreenAspectRatio / WindowAspectRatio;

        WindowAspectRatio       = (DestWidth * DisplayPixelAspectRatio) / DestHeight;
    }

    //if ((EventMask & EventSourceSizeChangeManifest) != 0)       // Create an event record indicating that size/shape has changed
#warning "Not checking EventMask in SetDisplayWindows"
    if ((VideoParameters->Width != StreamDisplayParameters.Width) || (VideoParameters->Height != StreamDisplayParameters.Height) ||
        (VideoParameters->DisplayWidth != StreamDisplayParameters.DisplayWidth) || (VideoParameters->DisplayHeight != StreamDisplayParameters.DisplayHeight) ||
        (VideoParameters->PixelAspectRatio != StreamDisplayParameters.PixelAspectRatio))
    {
        DisplayEventRequested              |= EventSourceSizeChangeManifest;
        DisplayEvent.Value[0].UnsignedInt   = SourceWidth;
        DisplayEvent.Value[1].UnsignedInt   = SourceHeight;
        DisplayEvent.Rational               = VideoParameters->PixelAspectRatio;
    }
    memcpy ((void*)&StreamDisplayParameters, (void*)VideoParameters, sizeof (struct VideoDisplayParameters_s));

    if (DisplayFormatPolicyValue != PolicyValueFullScreen)
    {
        if (PictureAspectRatio != WindowAspectRatio)
        {
            if (DisplayFormatPolicyValue == PolicyValueLetterBox)
            {
               if (PictureAspectRatio > WindowAspectRatio)
               {
                   // Picture is wider than display surface so must shrink height
                   Rational_t   NewHeight       = (DestHeight * WindowAspectRatio) / PictureAspectRatio;
                   DestHeight                   = NewHeight.IntegerPart();
                   DestY                        = DestY + ((SurfaceWindow.Height - DestHeight) >> 1);
               }
               else
               {
                   // Picture is taller than display surface so must shrink width
                   Rational_t   NewWidth        = (DestWidth * PictureAspectRatio) / WindowAspectRatio;
                   DestWidth                    = NewWidth.IntegerPart();
                   DestX                        = DestX + ((SurfaceWindow.Width - DestWidth) >> 1);
               }
            }
            else if (DisplayFormatPolicyValue == PolicyValueCentreCutOut ||
                     DisplayFormatPolicyValue == PolicyValuePanScan)
            {
               if (PictureAspectRatio > WindowAspectRatio)
               {
                   // Picture is wider than display surface so must chop off edges
                   int          OldWidth        = SourceWidth;
                   Rational_t   NewWidth        = (SourceWidth * WindowAspectRatio) / PictureAspectRatio;
                   SourceWidth                  = NewWidth.IntegerPart();
                   SourceX                      = SourceX + ((OldWidth - SourceWidth) >> 1);
               }
               else
               {
                   // Picture is taller than display surface so must chop off top and bottom
                   int          OldHeight       = SourceHeight;
                   Rational_t   NewHeight       = (SourceHeight * PictureAspectRatio) / WindowAspectRatio;
                   SourceHeight                 = NewHeight.IntegerPart();
                   SourceY                      = SourceY + ((OldHeight - SourceHeight) >> 1);
               }
            }
            else if (DisplayFormatPolicyValue == PolicyValueZoom_4_3)
            {
                if (PictureAspectRatio > WindowAspectRatio)
                {
                    int OldWidth         = SourceWidth;
                    int OldHeight        = SourceHeight;
                    Rational_t NewWidth  = (SourceWidth * WindowAspectRatio) / PictureAspectRatio;
                    Rational_t NewHeight = (SourceHeight * WindowAspectRatio) / PictureAspectRatio;
                    SourceWidth          = NewWidth.IntegerPart();
                    SourceHeight         = NewHeight.IntegerPart();
                    SourceX             += ((OldWidth - SourceWidth) >> 1);
                    SourceY             += ((OldHeight - SourceHeight) >> 1);
                }
            }
        }
    }

#if defined (CROP_TOP_FEW_LINES)
    if ((SourceHeight > DestHeight) && ((SourceHeight - DestHeight) < 16))
    {
        // if we have ended up with the source being very slightly larger than the destination we should
        // crop rather than scale.  An arbitrary difference of 16 is chosen as the boundary - this should
        // presumably be some proportion of the height.  At the moment this is only done for height and only
        // when source is larger as it is assumed the destination position is not negotiable.
        SourceY         += (SourceHeight - DestHeight) / 2;
        SourceHeight     = DestHeight;
    }
    else if ((SourceHeight == (int)VideoParameters->Height) && (DestHeight != (int)SurfaceDescriptor.DisplayHeight))
    {
        // If we are scaling video from the full image into a window, crop the top and bottom few lines.
        // This removes visual artifacts caused by broadcasters not encoding good data into lines normally
        // hidden by overscanning TVs. It also removes artifacts caused by the vertical filters repeating
        // the first and last lines to complete all the filter taps, giving those lines too much weight
        // in the filter calculation.
        SourceY         += 4;
        SourceHeight    -= 8;
    }
#endif

    if (SourceX < 0)
    {
        int  Shift, Width;

        Shift           = -SourceX;
        Width           = DestWidth - ((Shift * DestWidth) / SourceWidth);      // Shrink dest width by similar ratio
        DestX          += (DestWidth - Width) / 2;
        DestWidth       = Width;
        SourceX         = 0;
        SourceWidth    -= (Shift + Shift);
    }
    if (SourceY < 0)
    {
        int  Shift, Height;

        Shift           = -SourceY;
        Height          = DestHeight - ((Shift * DestHeight) / SourceHeight);   // Shrink dest height by similar ratio
        DestY          += (DestHeight - Height) / 2;
        DestHeight      = Height;
        SourceY         = 0;
        SourceHeight   -= (Shift + Shift);
    }

    //
    // Nick added signalling of changes in the output window
    //

    if ((OutputWindow.X      != (unsigned int)DestX) || 
        (OutputWindow.Y      != (unsigned int)DestY) || 
        (OutputWindow.Width  != (unsigned int)DestWidth) || 
        (OutputWindow.Height != (unsigned int)DestHeight))
    {
        DisplayEventRequested              |= EventOutputSizeChangeManifest;
    }

//

    OutputWindow.X                      = DestX;
    OutputWindow.Y                      = DestY;
    OutputWindow.Width                  = DestWidth;
    OutputWindow.Height                 = DestHeight;

    InputWindow.X                       = SourceX * INPUT_WINDOW_SCALE_FACTOR;
    InputWindow.Y                       = SourceY * INPUT_WINDOW_SCALE_FACTOR;
    InputWindow.Width                   = SourceWidth;
    InputWindow.Height                  = SourceHeight;

    // Decide whether the display requires scaling/cropping or not
    DecimateIfAvailable                 = false;
#if defined (CROP_INPUT_WHEN_DECIMATION_NEEDED_BUT_NOT_AVAILABLE)
    if ((Player->PolicyValue (Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled) &&
       ((SourceWidth > (DestWidth * MAX_SCALING_FACTOR)) || (SourceHeight > (DestHeight * MAX_SCALING_FACTOR))))
    {
        // Decimation is necessary but may not be available. Crop out a section from the middle of
        // the source with the same aspect ration as previously determined.
        Rational_t          WidthRatio  = Rational_t (SourceWidth, DestWidth * MAX_SCALING_FACTOR);
        Rational_t          HeightRatio = Rational_t (SourceHeight, DestHeight * MAX_SCALING_FACTOR);
        if (WidthRatio > HeightRatio)
        {
            unsigned int NewHeight      = (SourceHeight * (DestWidth * MAX_SCALING_FACTOR)) / SourceWidth;
            SourceY                     = SourceY + ((SourceHeight - NewHeight) / 2);
            SourceHeight                = NewHeight;
            SourceX                     = SourceX + ((SourceWidth - (DestWidth * MAX_SCALING_FACTOR)) / 2);
            SourceWidth                 = DestWidth * MAX_SCALING_FACTOR;
        }
        else
        {
            unsigned int NewWidth       = (SourceWidth * (DestHeight * MAX_SCALING_FACTOR)) / SourceHeight;
            SourceX                     = SourceX + ((SourceWidth - NewWidth) / 2);
            SourceWidth                 = NewWidth;
            SourceY                     = SourceY + ((SourceHeight - (DestHeight * MAX_SCALING_FACTOR)) / 2);
            SourceHeight                = DestHeight * MAX_SCALING_FACTOR;
        }

        DecimateIfAvailable             = true;
    }
#endif

    CroppedWindow.X                     = SourceX * INPUT_WINDOW_SCALE_FACTOR;
    CroppedWindow.Y                     = SourceY * INPUT_WINDOW_SCALE_FACTOR;
    CroppedWindow.Width                 = SourceWidth;
    CroppedWindow.Height                = SourceHeight;

    if (!Stepping)
        MANIFESTOR_DEBUG ("Outgoing Source %dx%d, at %d,%d(16ths), Dest %dx%d at %d,%d\n",
                       InputWindow.Width,  InputWindow.Height,  InputWindow.X,  InputWindow.Y,
                       OutputWindow.Width, OutputWindow.Height, OutputWindow.X, OutputWindow.Y);

    return UpdateDisplayWindows ();
}
//}}}
//{{{  BufferReleaseThread
void  Manifestor_Video_c::BufferReleaseThread (void)
{
    unsigned int                i;

    MANIFESTOR_DEBUG ("Starting\n");

    while (BufferReleaseThreadRunning)
    {
        OS_LockMutex (&BufferLock);

        // Check if any buffers were not manifested so can be released immediately
        if (NotQueuedBufferCount != 0)
        {
            for (i = 0; i < BufferConfiguration.MaxBufferCount; i++)
                if (StreamBuffer[i].BufferState == BufferStateNotQueued)
                {
                    StreamBuffer[i].BufferState = BufferStateAvailable;
                    NotQueuedBufferCount--;

                    OutputRing->Insert ((unsigned int)StreamBuffer[i].BufferClass);
                    InitialFrameState           = InitialFrameNotPossible;
                }

            if (NotQueuedBufferCount != 0)
            {
                MANIFESTOR_ERROR("Internal error, NotQueuedBufferCount non-zero but no buffers found.\n");
                NotQueuedBufferCount    = 0;
            }
        }

        if (DequeueOut != DequeueIn)
        {
            struct StreamBuffer_s*      DequeuedStreamBuffer;
            RingStatus_t                Status;
            PlayerStatus_t              PlayerStatus;
            PlayerEventRecord_t         Event;

            if( FatalHardwareError && !FatalHardwareErrorSignalled )
            {
                Event.Code                      = EventFatalHardwareFailure;
                Event.Playback                  = Playback;
                Event.Stream                    = Stream;
                Event.PlaybackTime              = TIME_NOT_APPLICABLE;
                Event.UserData                  = NULL;

                PlayerStatus                    = Player->SignalEvent( &Event );
                if( PlayerStatus != PlayerNoError )
                    MANIFESTOR_ERROR("Failed to signal fatal hardware failure event.\n" );

                FatalHardwareErrorSignalled     = true;
            }

            DequeuedStreamBuffer                = DequeuedStreamBuffers[DequeueOut];

            DequeueOut++;
            if (DequeueOut == MAX_DEQUEUE_BUFFERS)
                DequeueOut                      = 0;

            //if (DequeuedStreamBuffer->BufferIndex == RequeuedBufferIndex)
            //    MANIFESTOR_DEBUG("DQ (%d,%d) Buffer %d state = %x, QueueCount = %d Class = %p, QueuedBufferCount %d\n", DequeueIn, DequeueOut,
            //                DequeuedStreamBuffer->BufferIndex, DequeuedStreamBuffer->BufferState, DequeuedStreamBuffer->QueueCount, DequeuedStreamBuffer->BufferClass,
            //                QueuedBufferCount);
            if (--(DequeuedStreamBuffer->QueueCount) == 0)
            {
                if ((DequeuedStreamBuffer->BufferState != BufferStateQueued) && (DequeuedStreamBuffer->BufferState != BufferStateMultiQueue))
                    MANIFESTOR_ERROR("Buffer  %d state = %x - could go on ring twice\n", DequeuedStreamBuffer->BufferIndex, DequeuedStreamBuffer->BufferState);
                //if (DequeuedStreamBuffer->BufferIndex == RequeuedBufferIndex)
                //    MANIFESTOR_DEBUG("Buffer  %d (%x) has been requeued\n", DequeuedStreamBuffer->BufferIndex, DequeuedStreamBuffer->BufferState);

                DequeuedStreamBuffer->BufferState   = BufferStateAvailable;
                //MANIFESTOR_DEBUG ("DQ %d (%d, %d)\n", DQCount++, StreamBuffer->BufferIndex, QueuedBufferCount);

                QueuedBufferCount--;
                Status                              = OutputRing->Insert ((unsigned int)DequeuedStreamBuffer->BufferClass);
                InitialFrameState                   = InitialFrameNotPossible;
            }

            OS_UnLockMutex (&BufferLock);
        }
        else
        {
            if (Stepping)
                RequeueBufferOnDisplayIfNecessary ();
            OS_UnLockMutex (&BufferLock);
            OS_SleepMilliSeconds (20);
        }
    }

    OS_LockMutex (&BufferLock);
    // Give back to the ring all buffers currently within our ambit
    // ignoring the dequeued buffer list
    for (i = 0; i < BufferConfiguration.MaxBufferCount; i++)
    {
        if (StreamBuffer[i].BufferState != BufferStateAvailable)
        {
            MANIFESTOR_DEBUG ("Buffer %d state = %x\n", i, StreamBuffer[i].BufferState);
            StreamBuffer[i].BufferState         = BufferStateAvailable;
            if (StreamBuffer[i].BufferClass != NULL)
                OutputRing->Insert ((unsigned int)StreamBuffer[i].BufferClass);
            StreamBuffer[i].BufferClass         = NULL;
        }
    }
    OS_UnLockMutex (&BufferLock);

    OS_SetEvent (&BufferReleaseThreadTerminated);
    MANIFESTOR_DEBUG ("Terminating\n");
}
//}}}
//{{{  DisplaySignalThread
void  Manifestor_Video_c::DisplaySignalThread (void)
{
    //struct StreamBuffer_s*      Buffer;

    MANIFESTOR_DEBUG ("Starting\n");

    while (DisplaySignalThreadRunning)
    {
        OS_SemaphoreWait (&BufferDisplayed);

        if (DisplaySignalThreadRunning)
            ServiceEventQueue (BufferOnDisplay);
    }

    MANIFESTOR_DEBUG ("Terminating\n");
    OS_SetEvent (&DisplaySignalThreadTerminated);
}
//}}}

//{{{  FillOutBufferStructure
// /////////////////////////////////////////////////////////////////////
//
//      Function to flesh out a buffer request structure

ManifestorStatus_t   Manifestor_Video_c::FillOutBufferStructure( BufferStructure_t       *RequestedStructure )
{
    //
    // Do a switch depending on the buffer type
    //

   unsigned int    DecimationPolicyValue   = Player->PolicyValue (Playback, Stream, PolicyDecimateDecoderOutput);
   unsigned int    DecimationValue         = (DecimationPolicyValue == PolicyValueDecimateDecoderOutputHalf) ? 2 : 4;


    switch( RequestedStructure->Format )
    {
        case FormatVideo420_PairedMacroBlock:
            //
            // Round up dimensions to paired macroblock size and fall through,
            // NOTE some codecs that do not require paired macroblock widths,
            // do require paired macroblock heights, H264 in particular.

            RequestedStructure->Dimension[0]            = ((RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0);

        case FormatVideo420_MacroBlock:
        {
            RequestedStructure->Dimension[0]            = ((RequestedStructure->Dimension[0] + 0x0f) & 0xfffffff0);
            RequestedStructure->Dimension[1]            = ((RequestedStructure->Dimension[1] + 0x1f) & 0xffffffe0);

            RequestedStructure->ComponentOffset[0]      = 0;
            RequestedStructure->ComponentOffset[1]      = RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

            // Force a 1k alignment, to support the 7109 hardware mpeg2 decoder
            RequestedStructure->ComponentOffset[1]      = (RequestedStructure->ComponentOffset[1] + 0x3ff) & 0xfffffc00;

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0];
            RequestedStructure->Strides[0][1]           = RequestedStructure->Dimension[0];

            RequestedStructure->Size                    = RequestedStructure->ComponentOffset[1] +
                                                              ((RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1])/2);

            RequestedStructure->ComponentCount          = 2;


            if (RequestedStructure->DecimationRequired)
            {

                RequestedStructure->ComponentCount      = 4;

                RequestedStructure->Dimension[2]        = RequestedStructure->Dimension[0] / DecimationValue;
                RequestedStructure->Dimension[3]        = RequestedStructure->Dimension[1] / DecimationValue;
                RequestedStructure->Dimension[2]        = ((RequestedStructure->Dimension[2] + 0x0f) & 0xfffffff0);
                RequestedStructure->Dimension[3]        = ((RequestedStructure->Dimension[3] + 0x1f) & 0xffffffe0);

                RequestedStructure->Strides[0][2]       = RequestedStructure->Dimension[2];
                RequestedStructure->Strides[0][3]       = RequestedStructure->Dimension[2];

                RequestedStructure->ComponentOffset[2]  = RequestedStructure->ComponentOffset[1] +
                                                              ((RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1]) / 2);

                RequestedStructure->ComponentOffset[3]  = RequestedStructure->ComponentOffset[2] +
                                                              (RequestedStructure->Dimension[2] * RequestedStructure->Dimension[3]);

                // Force a 1k alignment, to support the 7109 hardware mpeg2 decoder
                RequestedStructure->ComponentOffset[3]      = (RequestedStructure->ComponentOffset[3] + 0x3ff) & 0xfffffc00;


                RequestedStructure->DecimatedSize      += (RequestedStructure->Dimension[2] * RequestedStructure->Dimension[3]) +
                                                              ((RequestedStructure->Dimension[2] * RequestedStructure->Dimension[3])/2);

                RequestedStructure->Size               += RequestedStructure->DecimatedSize;

            }
            break;
        }
        case FormatVideo422_Raster:

            //
            // Round up dimesion 0 to 32, these buffers are used for dvp capture,
            // the capture hardware needs strides on a 16 byte boundary and
            // the display hardware needs strides on a 64 byte (32 pixel) boundary.
            //

            RequestedStructure->Dimension[0]            = ((RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0);

            RequestedStructure->ComponentCount          = 1;
            RequestedStructure->ComponentOffset[0]      = 0;

            RequestedStructure->Strides[0][0]           = 2 * RequestedStructure->Dimension[0];

            RequestedStructure->Size                    = (2 * RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1]);

            // Not possible for now so untested!
            if (RequestedStructure->DecimationRequired)
            {
               RequestedStructure->Dimension[2]         = RequestedStructure->Dimension[0] / DecimationValue;
               RequestedStructure->Dimension[3]         = RequestedStructure->Dimension[1] / DecimationValue;

               RequestedStructure->ComponentCount       = 4;
               RequestedStructure->ComponentOffset[2]   = (2 * RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1]);

               RequestedStructure->Strides[0][2]        = 2 * RequestedStructure->Dimension[2];
               RequestedStructure->Strides[0][3]        = 2 * RequestedStructure->Dimension[3];

               RequestedStructure->DecimatedSize        = (2 * RequestedStructure->Dimension[2] * RequestedStructure->Dimension[3]);

               RequestedStructure->Size                += RequestedStructure->DecimatedSize;

            }       
            break;

        case FormatVideo420_Planar:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + RequestedStructure->ComponentBorder[0]*2 + 0x0f) & 0xfffffff0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + RequestedStructure->ComponentBorder[1]*2 + 0x0f) & 0xfffffff0;

            RequestedStructure->ComponentCount          = 2;
            RequestedStructure->ComponentOffset[0]      = 0;
            RequestedStructure->ComponentOffset[1]      = RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0];
            RequestedStructure->Strides[0][1]           = RequestedStructure->Dimension[0] / 2;

            RequestedStructure->Size                    = (RequestedStructure->ComponentOffset[1] * 3) / 2;
            break;

        case FormatVideo420_PlanarAligned:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + RequestedStructure->ComponentBorder[0]*2 + 0x0f) & 0xfffffff0;
            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + 0x3f) & 0xffffffc0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + RequestedStructure->ComponentBorder[1]*2 + 0x0f) & 0xfffffff0;

            RequestedStructure->ComponentCount          = 2;
            RequestedStructure->ComponentOffset[0]      = 0;
            RequestedStructure->ComponentOffset[1]      = RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0];
            RequestedStructure->Strides[0][1]           = RequestedStructure->Dimension[0] / 2;

            RequestedStructure->Size                    = (RequestedStructure->ComponentOffset[1] * 3) / 2;
            break;

        case FormatVideo422_Planar:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + RequestedStructure->ComponentBorder[0]*2 + 0x0f) & 0xfffffff0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + RequestedStructure->ComponentBorder[1]*2 + 0x0f) & 0xfffffff0;

            RequestedStructure->ComponentCount          = 2;
            RequestedStructure->ComponentOffset[0]      = 0;
            RequestedStructure->ComponentOffset[1]      = RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0];
            RequestedStructure->Strides[0][1]           = RequestedStructure->Dimension[0];

            RequestedStructure->Size                    = (RequestedStructure->ComponentOffset[1] * 2);
            break;

        case FormatVideo8888_ARGB:
            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + 0x1f) & 0xffffffe0;

            RequestedStructure->ComponentCount          = 1;
            RequestedStructure->ComponentOffset[0]      = 0;

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0] * 4;

            RequestedStructure->Size                    = (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 4);
            break;


        case FormatVideo888_RGB:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + 0x1f) & 0xffffffe0;

            RequestedStructure->ComponentCount          = 1;
            RequestedStructure->ComponentOffset[0]      = 0;

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0] * 3;

            RequestedStructure->Size                    = (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 3);
            break;

        case FormatVideo565_RGB:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + 0x1f) & 0xffffffe0;

            RequestedStructure->ComponentCount          = 1;
            RequestedStructure->ComponentOffset[0]      = 0;

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0] * 2;

            RequestedStructure->Size                    = (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 2);
            break;       

        case FormatVideo422_YUYV:

            RequestedStructure->Dimension[0]            = (RequestedStructure->Dimension[0] + 0x1f) & 0xffffffe0;
            RequestedStructure->Dimension[1]            = (RequestedStructure->Dimension[1] + 0x1f) & 0xffffffe0;

            RequestedStructure->ComponentCount          = 1;
            RequestedStructure->ComponentOffset[0]      = 0;

            RequestedStructure->Strides[0][0]           = RequestedStructure->Dimension[0] * 2;

            RequestedStructure->Size                    = (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 2);
            break;       

        default: MANIFESTOR_ERROR("Unsupported buffer format (%d)\n", RequestedStructure->Format );
    }

    return ManifestorNoError;
}
//}}}
