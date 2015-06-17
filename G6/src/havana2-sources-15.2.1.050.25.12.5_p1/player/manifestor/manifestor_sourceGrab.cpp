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

#include "player_threads.h"

#include "manifestor_sourceGrab.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Source_c"

/*{{{  End of stream Event handler*/
void SourceGrab_EndOfStreamEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    Manifestor_Source_c *thisManifestor = (Manifestor_Source_c *)eventsInfo->cookie;
    SE_INFO(thisManifestor->GetGroupTrace(), "\n");

    //
    // Currently this handler manages only one event "STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM"
    //

    if (!thisManifestor->Connected)
    {
        // Take care to be connected
        SE_ERROR(" EOS marker detected out of connection\n");
        return;
    }

    if (thisManifestor->waitListBufferInsert(thisManifestor->FakeEndOfStreamBuffer) == ManifestorError)
    {
        SE_ERROR("Too many queued buffers for grabbing : implemtation error\n");
        return;
    }

    //
    // Signal the pending pull read that a new buffer is available
    //
    OS_SetEvent(&thisManifestor->BufferReceived);
}

int pull_source_se(stm_object_h src_object,
                   struct stm_data_block *block_list,
                   uint32_t block_count,
                   uint32_t *filled_blocks)
{
    Manifestor_Source_c *thisManifestor = (Manifestor_Source_c *)src_object;
    int         retCode;
    uint32_t    copiedBytes = sizeof(stm_se_capture_buffer_t);
    //
    // The default returned value of copiedBytes is the size of the capture buffer
    // This setting is mandatory when pulling the fake EOS buffer without data
    //
    SE_DEBUG(thisManifestor->GetGroupTrace(), "checking for data block_list->len = %d\n", block_list->len);
    //
    // nothing read so far
    //
    *filled_blocks = 0;

    //
    // make sure provided buffer is large enough for captureBuffer
    //
    if (block_list->len < sizeof(stm_se_capture_buffer_t))
    {
        return -EINVAL;
    }

    //
    // Check if something to read and if connection still opened
    // After this call CurrentBuffer is set with next buffer to manifest
    //
    retCode = thisManifestor->PullFrameWait();

    if (retCode <= 0)
    {
        return retCode;
    }

    //
    // Specific process for End Of Stream
    //
    if (thisManifestor->GetEndOfStream((uint8_t *)block_list->data_addr) == false)
    {
        //
        // Read Frame from CurrentBuffer
        //
        copiedBytes = thisManifestor->PullFrameRead((uint8_t *)block_list->data_addr, block_list->len);
    }

    //
    // Release current frame buffer
    //
    thisManifestor->PullFramePostRead();
    block_list->len = copiedBytes;
    *filled_blocks = 1;
    // return total copied bytes
    return  copiedBytes;
}

int pull_test_for_source_se(stm_object_h src_object, uint32_t *size)
{
    Manifestor_Source_c *thisManifestor = (Manifestor_Source_c *)src_object;
    SE_DEBUG(thisManifestor->GetGroupTrace(), "checking for data\n");
    *size = thisManifestor->PullFrameAvailable();
    return 0;
}

struct stm_data_interface_pull_src se_manifestor_pull_interface =
{
    pull_source_se,
    pull_test_for_source_se
};

//{{{  Thread entry stub
static OS_TaskEntry(WaitListBufferThreadStub)
{
    Manifestor_Source_c *thisManifestor = (Manifestor_Source_c *)Parameter;
    thisManifestor->waitListBufferThread();
    OS_TerminateThread();
    return NULL;
}


//{{{  Constructor
//{{{  doxynote
/// \brief                      Initail state
/// \return                     Success or fail
Manifestor_Source_c::Manifestor_Source_c(void)
    :  Manifestor_Source_Base_c()
    , SurfaceDescriptor()
    , Connected(false)
    , Interrupted(false)
    , PullSinkInterface()
    , EventSubscription()
    , BufferLock()
    , WaitListThreadRunning(false)
    , WaitListThreadTerminated()
    , BufferReceived()
    , FakeEndOfStreamBuffer(NULL)
    , CurrentBuffer(NULL)
    , SourceStreamBuffer()
    , waitListBuffer()
    , waitListLimit(MAX_DECODE_BUFFERS + 1)
    , waitListNextExtract(0)
    , waitListNextInsert(0)
    , FramePhysicalAddress(NULL)
    , FrameVirtualAddress(NULL)
    , FrameSize(0)
    , SinkHandle(NULL)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SE_DEBUG(GetGroupTrace(), "\n");

    Configuration.Capabilities = MANIFESTOR_CAPABILITY_SOURCE;

    // need a fake buffer_c address for EOS buffer detection
    FakeEndOfStreamBuffer = (Buffer_t)&Connected;

    OS_InitializeMutex(&BufferLock);
    OS_InitializeEvent(&WaitListThreadTerminated);
    OS_InitializeEvent(&BufferReceived);
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Try to disconnect if not done
/// \return                     Success or fail
Manifestor_Source_c::~Manifestor_Source_c(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    Disconnect(SinkHandle);

    OS_TerminateMutex(&BufferLock);
    OS_TerminateEvent(&BufferReceived);
    OS_TerminateEvent(&WaitListThreadTerminated);
}
//}}}


//{{{  QueueDecodeBuffer
//{{{  doxynote
/// \brief                      Actually first decode decoded buffer
/// \param Buffer               buffer
/// \return                     Success or fail
//}}}
ManifestorStatus_t  Manifestor_Source_c::QueueDecodeBuffer(Buffer_t Buffer, ManifestationOutputTiming_t **TimingArray, unsigned int *NumTimes)
{
    ManifestorStatus_t                  Status = ManifestorNoError;
    uint32_t                            bufferIndex;
    int                                 retval;
    AssertComponentState(ComponentRunning);

    //
    // Check availibility of queues
    //
    if (!mOutputPort)
    {
        SE_ERROR("mOutputPort not available\n");
        return ManifestorError;
    }

    //
    // No action if not connected yet
    //
    if (!Connected)
    {
        // Buffer can be released immediately
        mOutputPort->Insert((unsigned int) Buffer);
        return ManifestorNoError;
    }

    //
    // Set SourceStreamBuffer of the CurrentBuffer thanks to its index
    //
    Buffer->GetIndex(&bufferIndex);

    //
    // Make sure Index is correct
    //
    if (bufferIndex >= MAX_DECODE_BUFFERS)
    {
        // Buffer rejected manifestor. No need to insert buffer into ouput port
        // because error is returned to manifestion_coordinator
        SE_ERROR("Wrong Buffer Index: %x\n", bufferIndex);
        return ManifestorError;
    }

    if (*NumTimes > 1)
    {
        SE_INFO(GetGroupTrace(), "Implementation Error can only handle one output timing\n");
    }

    //
    // Save extra info for this buffer
    //
    SourceStreamBuffer[bufferIndex].OutputTiming = *TimingArray;
    //
    // Queue buffer for buffer received thread
    //
    SE_DEBUG(GetGroupTrace(), "Buffer [%d] Queued, CurrentQ = %d\n", bufferIndex, waitListBufferCount());

    if (waitListBufferInsert(Buffer) == ManifestorError)
    {
        // Buffer rejected manifestor. No need to insert buffer into output port
        // because error is returned to manifestion_coordinator
        SE_ERROR("Too namy queued buffer for grabbing\n");
        return ManifestorError;
    }

    //
    // Signal memsink that a new frame is available
    //
    if (PullSinkInterface.notify)
    {
        retval = PullSinkInterface.notify(SinkHandle,                 // SINK
                                          STM_MEMSINK_EVENT_DATA_AVAILABLE);

        if (retval != 0)
        {
            // Error can happen in case of the connection is closed.
            // Here we already tested that connection was opened but could be closed meantime
            SE_ERROR("Error in PullSinkInterface.notify(%p) ret = %d\n", SinkHandle, retval);
            return ManifestorError;
        }
    }
    else
    {
        // Notification interface not available.
        // Should not be possible but prevents null pointer occurence
        SE_ERROR("PullSinkInterface.notify(%p) not available\n", SinkHandle);
    }

    //
    // Signal new buffer to ReleaseBuffer thread
    //
    OS_SetEvent(&BufferReceived);
    return Status;
}

//}}}
//{{{  GetBufferId
//{{{  doxynote
/// \brief      Retrieve buffer index of last buffer queued. If no buffers
///             yet queued returns ANY_BUFFER_ID
/// \return     Buffer index of last buffer sent for display
//}}}
unsigned int    Manifestor_Source_c::GetBufferId(void)
{
    return ANY_BUFFER_ID;
}


//}}}
//{{{  GetNextQueuedManifestationTime
//{{{  doxynote
/// \brief      Get the earliest System time at which the next frame to Be queued will be grabbed.
///             For more details, please have a look at the doxygen definition of
///             ManifestorStatus_t Manifestor_c::GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes)
/// \param Buffer  Time : estimated worse case time at which the next frame will be grabbed.
/// \return     Alway no error
//}}}
ManifestorStatus_t      Manifestor_Source_c::GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes)
{
    uint32_t estimatedDelay;
    uint32_t currentQueuedBuffers;
    SE_DEBUG(GetGroupTrace(), "\n");
    // Estimate memsink pull will grab in quarter of a second.
    *Time     = OS_GetTimeInMicroSeconds() + 250000;
    *NumTimes = 1;
    //
    // Take into account the number of queued buffers to be grabbed
    // Cumulated time at estimated 40ms per frame
    //
    currentQueuedBuffers = waitListBufferCount();
    estimatedDelay = currentQueuedBuffers * 40000;
    *Time += estimatedDelay;
    SE_DEBUG(GetGroupTrace(), "estimatedDelay = %d  for %d buffers\n", estimatedDelay, currentQueuedBuffers);
    return ManifestorNoError;
}


//{{{  FlushDisplayQueue
//{{{  doxynote
/// \brief      Flushes the display queue so buffers not yet manifested are returned
ManifestorStatus_t      Manifestor_Source_c::FlushDisplayQueue(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");

    //
    // check if currently connected
    //
    if (!Connected)
    {
        return ManifestorNoError;
    }

    //
    // purge waiting list
    //
    do
    {
        Buffer_t     bufferToRelease;
        waitListBufferExtract(&bufferToRelease , false);

        // release current buffer
        if (bufferToRelease == NULL)
        {
            break;
        }

        if (bufferToRelease != FakeEndOfStreamBuffer)
        {
            mOutputPort->Insert((unsigned int) bufferToRelease);
        }
    }
    while (true);

    return ManifestorNoError;
}
//}}}

//{{{  Connect
//{{{  doxynote
/// \brief                      Connect to memory Sink port
/// \param                      SrcHandle  : HavanaStream object
/// \param Buffer               SinkHandle : memory sink object to connnect to
/// \return                     Success or fail
ManifestorStatus_t Manifestor_Source_c::Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    int           retval;
    stm_object_h  sinkType;
    char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       returnedSize;
    OS_Thread_t   Thread;

    if (Connected)
    {
        SE_ERROR("Try to open Manisfetor twice\n");
        return ManifestorError;
    }

    //
    // Start up Wait List thread
    //
    WaitListThreadRunning = true;

    if (OS_CreateThread(&Thread, WaitListBufferThreadStub, this, &player_tasks_desc[SE_TASK_MANIF_BRSRCGRAB]) != OS_NO_ERROR)
    {
        SE_ERROR("Unable to create WaitList thread\n");
        WaitListThreadRunning = false;
        return ManifestorError;
    }

    //
    // Check sink object support STM_DATA_INTERFACE_PULL interface
    //
    retval = stm_registry_get_object_type(SinkHandle, &sinkType);
    if (retval)
    {
        SE_ERROR("error in stm_registry_get_object_type(%p, &%p) (%d)\n", SinkHandle, sinkType, retval);
        goto memsink_error;
    }

    retval = stm_registry_get_attribute(SinkHandle,
                                        STM_DATA_INTERFACE_PULL,
                                        tagTypeName,
                                        sizeof(PullSinkInterface),
                                        &PullSinkInterface,
                                        (int *)&returnedSize);
    if ((retval) || (returnedSize != sizeof(PullSinkInterface)))
    {
        SE_DEBUG(GetGroupTrace(), "error in stm_registry_get_attribute(...) (%d)\n", retval);
        goto memsink_error;
    }

    SE_DEBUG(GetGroupTrace(), "\n Getting tag type %s\n", tagTypeName);
    //
    // call the sink interface's connect handler to connect the consumer */
    //
    SE_DEBUG(GetGroupTrace(), "ManifestorSource tries to connect to memorySink\n");
    retval = PullSinkInterface.connect((stm_object_h)this       , // SRC
                                       SinkHandle,                // SINK
                                       (struct stm_data_interface_pull_src *) &se_manifestor_pull_interface);

    if (retval)
    {
        // Connection fails : Free OS objects
        SE_ERROR("error in PullSinkInterface.connect(%p) ret = %d\n", SinkHandle, retval);
        goto memsink_error;
    }

    //
    // Need to subscribe PlayerStream event 'STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM'
    //
    {
        //
        // Subscribe to End of stream event and set handler event
        //
        stm_event_subscription_entry_t event_entry;
        event_entry.object = (stm_object_h) SrcHandle;
        event_entry.event_mask = STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM;
        event_entry.cookie = this;
        // request only one subscription at a time
        retval = stm_event_subscription_create(&event_entry, 1, &EventSubscription);
        if (retval < 0)
        {
            EventSubscription = NULL;
            SE_ERROR("Failed to Create Event Subscription for EOS event (%d)\n", retval);
            goto memsink_error;
        }
        else
        {
            // set the call back function for subscribed events
            retval = stm_event_set_handler(EventSubscription, SourceGrab_EndOfStreamEvent);
            if (retval < 0)
            {
                EventSubscription = NULL;
                SE_ERROR("Failed to Create Event handler for EOS event (%d)\n", retval);
                goto memsink_error;
            }
        }
    }
    //
    // Save sink object for disconnection
    //
    this->SinkHandle = SinkHandle;
    //
    // Initialize connection
    //
    initialiseConnection();
    Connected = true;
    SetComponentState(ComponentRunning);
    return ManifestorNoError;
    //
    // Cannot connect to memsink: release rereources
    //
memsink_error:
    // terminate WaitList thread
    OS_ResetEvent(&WaitListThreadTerminated);
    OS_Smp_Mb(); // Write memory barrier: wmb_for_WaitList_Terminating coupled with: rmb_for_WaitList_Terminating
    WaitListThreadRunning = false;
    OS_SetEvent(&BufferReceived);
    OS_WaitForEventAuto(&WaitListThreadTerminated, OS_INFINITE);
    return ManifestorError;
}
//}}}

//{{{  Disconnect
//{{{  doxynote
/// \brief                      Disconnect from memory Sink port (if connected)
///                             Release pending pull call if any
/// \return                     Success or fail
ManifestorStatus_t Manifestor_Source_c::Disconnect(stm_object_h  SinkHandle)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    int retval;

    //
    // Check that Sink object correspond the connected one
    //
    if (this->SinkHandle != SinkHandle)
    {
        return ManifestorError;
    }

    /* Remove event subscription */
    if (EventSubscription)
    {
        retval = stm_event_subscription_delete(EventSubscription);
        if (retval < 0)
        {
            SE_ERROR("Failed to delete Event Subscription for EOS event (%d)\n", retval);
        }

        EventSubscription = NULL;
    }

    if (Connected == true)
    {
        //
        // Close connection for memsink calls
        // and shut down buffer release thread
        //
        Connected = false;

        // ask WaitList thread to terminate
        OS_ResetEvent(&WaitListThreadTerminated);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_WaitList_Terminating coupled with: rmb_for_WaitList_Terminating
        WaitListThreadRunning = false;

        // reset old sink object
        this->SinkHandle = NULL;

        // Wait for the thread to terminate
        OS_Status_t WaitStatus;
        do
        {
            // signal event for the thread termination
            OS_SetEvent(&BufferReceived);
            // Wait for thread to exit
            WaitStatus = OS_WaitForEventAuto(&WaitListThreadTerminated, 10);
        }
        while (WaitStatus == OS_TIMED_OUT);

        //
        // Signal event to release pending memsink pull call
        //
        OS_SetEvent(&BufferReceived);
        uint32_t  NbTries = 0;

        //
        // call the sink interface's disconnect handler to disconnect the consumer
        //
        do
        {
            SE_DEBUG(GetGroupTrace(), "ManifestorSource tries to disconnect to memorySink\n");
            retval = PullSinkInterface.disconnect((stm_object_h)this, SinkHandle);

            if (retval)
            {
                SE_ERROR("error in PullSinkInterface.disconnect(%p) tries #%d\n", SinkHandle, NbTries);
                //
                // memsink can be busy because of a pending pull
                // Wait a while until it ends
                //
                OS_SleepMilliSeconds(10);
                NbTries++;
            }
        }
        while (retval);

        //
        // Release pending pull call, if any
        //
        terminateConnection();
    }

    return ManifestorNoError;
}
//}}}


///{{{ PullFrameAvailable
/// \brief                      Check if a frame is available
/// \return                     Success or fail
int Manifestor_Source_c::PullFrameAvailable()
{
    int sizeAvailable = 0;
    OS_LockMutex(&BufferLock);

    if (waitListNextExtract != waitListNextInsert)
    {
        sizeAvailable = sizeof(stm_se_capture_buffer_t);
    }

    OS_UnLockMutex(&BufferLock);
    return (sizeAvailable);
}
//}}}


///{{{ PullFrameWait
/// \brief                      Check if current buffer is a fake one for End Of Stream notificatio
///                             If yes set up Capture buffer with size = 0 and return true
/// \param captureBufferAddr    Kernel Address of buffer provided by memsink
/// \return                     true if EOS
bool     Manifestor_Source_c::GetEndOfStream(uint8_t *captureBufferAddr)
{
    stm_se_capture_buffer_t    *CaptureBuffersForEOS   = (stm_se_capture_buffer_t *)captureBufferAddr;
    uint64_t   CurrentSystemTime;
    if (CurrentBuffer != FakeEndOfStreamBuffer)
    {
        return false;
    }

    // Set copied frame size to zero to notify EOS
    CaptureBuffersForEOS->payload_length = 0;
    // set discontinuity End of Stream
    CaptureBuffersForEOS->u.uncompressed.discontinuity = STM_SE_DISCONTINUITY_EOS;
    // General metadata setting
    CurrentSystemTime                                               = OS_GetTimeInMicroSeconds();
    CaptureBuffersForEOS->u.uncompressed.system_time                = CurrentSystemTime;
    // native_time = system_time for this EOS marked fake frame and so format is microsec
    CaptureBuffersForEOS->u.uncompressed.native_time                = CurrentSystemTime;
    CaptureBuffersForEOS->u.uncompressed.native_time_format         = TIME_FORMAT_US;

    CaptureBuffersForEOS->u.uncompressed.user_data_size             = 0;
    CaptureBuffersForEOS->u.uncompressed.user_data_buffer_address   = NULL;
    CaptureBuffersForEOS->u.uncompressed.media                      = GetMediaEncode();
    return true;
}

///{{{ PullFrameWait
/// \brief                      Actually put buffer on display
/// \param BufferIndex          Index into array of stream buffers
/// \return                     Success or fail
OS_Status_t     Manifestor_Source_c::PullFrameWait()
{
    SE_DEBUG(GetGroupTrace(), "\n");

    //
    // check if currently connected
    //
    if (!Connected)
    {
        return -EPERM;
    }

    //
    // extract Next Buffer
    //
    do
    {
        bool Blocking = true;
        //
        // check if user asks for waiting mode
        //
        if ((PullSinkInterface.mode & STM_IOMODE_NON_BLOCKING_IO) == STM_IOMODE_NON_BLOCKING_IO)
        {
            //
            // in case of no buffer available return immediately
            //
            if (waitListNextInsert == waitListNextExtract)
            {
                return 0;
            }
            // We do not want to block the Extract if a flush is in progress at this point
            Blocking = false;
        }


        waitListBufferExtract(&CurrentBuffer, Blocking);

        //
        // Check if connection was closed or interrupted while waiting for frame
        //
        if (!Connected || Interrupted)
        {
            return -ECONNRESET;
        }

        //
        // Check if the CurrentBuffer is NULL.
        // This scan happen if the waitListBufferThread has removed
        // the current buffer due to the time out presentataion time
        //
    }
    while (CurrentBuffer == NULL);

    //
    // signal receive buffer event if wait list not empty
    //
    if (waitListNextExtract != waitListNextInsert)
    {
        // to process this buffer now
        OS_SetEvent(&BufferReceived);
    }

    return (sizeof(stm_se_capture_buffer_t));
}
//}}

///{{{ PullFramePostRead
/// \brief                      Release Frame Buffer as soon as possible
/// \return                     Nothing
void  Manifestor_Source_c::PullFramePostRead(void)
{
    //
    // release previous CurrentBuffer if exists
    //
    if (CurrentBuffer && (CurrentBuffer != FakeEndOfStreamBuffer))
    {
        mOutputPort->Insert((unsigned int) CurrentBuffer);
    }

    CurrentBuffer = NULL;
}
//}}}

///{{{ initialiseConnection
/// \brief                      Reset CurrentBuffer and Waiting List
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_Source_c::initialiseConnection(void)
{
    //
    // perform general initialisation
    //
    OS_LockMutex(&BufferLock);
    waitListNextExtract     = 0;
    waitListNextInsert      = 0;
    CurrentBuffer = NULL;
    Interrupted = false;
    OS_UnLockMutex(&BufferLock);
    return ManifestorNoError;
}
//}}}

///{{{ terminateConnection
/// \brief                      Purge Waiting List into mOutputPort
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_Source_c::terminateConnection(void)
{
    //
    // perform general termination
    //
    if (CurrentBuffer)
    {
        // release current buffer
        mOutputPort->Insert((unsigned int) CurrentBuffer);
        CurrentBuffer = NULL;
    }

    //
    // purge waiting ring
    //
    do
    {
        Buffer_t     bufferToRelease;
        waitListBufferExtract(&bufferToRelease, false);

        // release current buffer
        if (bufferToRelease == NULL)
        {
            break;
        }

        if (bufferToRelease != FakeEndOfStreamBuffer)
        {
            mOutputPort->Insert((unsigned int) bufferToRelease);
        }
    }
    while (true);

    return ManifestorNoError;
}
//}}}


///{{{ waitListBufferinsert
/// \brief                      Insert received buffer into wait list
/// \return                     ManifestorError : buffer not inserted, no more free entry
ManifestorStatus_t  Manifestor_Source_c::waitListBufferInsert(Buffer_t receivedBuffer)
{
    uint32_t waitListOldNextInsert;
    OS_LockMutex(&BufferLock);
    waitListOldNextInsert       = waitListNextInsert;
    waitListNextInsert++;

    if (waitListNextInsert == waitListLimit)
    {
        waitListNextInsert = 0;
    }

    if (waitListNextInsert == waitListNextExtract)
    {
        // buffer not inserted
        waitListNextInsert      = waitListOldNextInsert;
        OS_UnLockMutex(&BufferLock);
        return ManifestorError;
    }

    waitListBuffer[waitListOldNextInsert] = receivedBuffer;
    OS_UnLockMutex(&BufferLock);
    return ManifestorNoError;
}
//}}}


///{{{ waitListBufferExtract
/// \brief                      Extract next buffer or wait for new received
/// \return                     ManifestorError : buffer not extracted
ManifestorStatus_t  Manifestor_Source_c::waitListBufferExtract(Buffer_t *extractedBuffer, bool waitExtract)
{
    OS_ResetEvent(&BufferReceived);
    // This code is inspired from RingGeneric_c::Extract where OS_Smp_Mb()
    // is called  after OS_ResetEvent(). I do agree that the memory barrier could be removed
    // but this will be done only if RingGeneric code analysis concludes to remove it
    OS_Smp_Mb();

    //
    // Wait extract event if wait list is empty
    //
    if ((waitListNextExtract == waitListNextInsert) && (waitExtract == true))
    {
        if (OS_WaitForEventInterruptible(&BufferReceived, OS_INFINITE) == OS_INTERRUPTED)
        {
            Interrupted = true;
            return ManifestorNoError;
        }
    }

    OS_LockMutex(&BufferLock);
    *extractedBuffer = NULL;

    if (waitListNextExtract != waitListNextInsert)
    {
        *extractedBuffer = waitListBuffer[waitListNextExtract];
        waitListNextExtract++;

        if (waitListNextExtract == waitListLimit)
        {
            waitListNextExtract = 0;
        }

        OS_UnLockMutex(&BufferLock);
        return ManifestorNoError;
    }

    OS_UnLockMutex(&BufferLock);
    return ManifestorNoError;
}
//}}}


///{{{ waitListBufferExtract
/// \brief                      Return the number of frame ready to grabbed in the waiting list
/// \return                     Number of current queued buffer
uint32_t  Manifestor_Source_c::waitListBufferCount(void)
{
    uint32_t  nbBufWaiting = 0;
    OS_LockMutex(&BufferLock);

    if (waitListNextInsert >= waitListNextExtract)
    {
        nbBufWaiting = waitListNextInsert - waitListNextExtract;
    }
    else
    {
        nbBufWaiting = waitListLimit - waitListNextExtract + waitListNextInsert ;
    }

    OS_UnLockMutex(&BufferLock);
    return nbBufWaiting;
}


//{{{  BufferReleaseThread
/// \brief
void  Manifestor_Source_c::waitListBufferThread(void)
{
    SE_DEBUG(GetGroupTrace(), "Starting\n");

    while (WaitListThreadRunning)
    {
        int  SleepTime = 0;
        bool bIgnoreEndOfStream = false;

        //
        // Wait until new received buffer or presentation time of next buffer
        //
        OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&BufferReceived, OS_INFINITE);
        if (WaitStatus == OS_INTERRUPTED)
        {
            SE_INFO(GetGroupTrace(), "wait for buffer received interrupted\n");
        }

        OS_ResetEvent(&BufferReceived);

        // This code is inspired from RingGeneric_c::Extract where OS_Smp_Mb()
        // is called  after OS_ResetEvent(). I do agree that the memory barrier could be removed
        // but this will be done only if RingGenieric code analysis concludes to remove it
        OS_Smp_Mb();

        //
        // This thread treats only one buffer during a loop and then wait a new one.
        // If several buffers are presents in the wait list, the signal will
        // wake up this thread to treat it immediately.
        //
        if (waitListNextExtract != waitListNextInsert)
        {
            // to process this buffer now
            OS_SetEvent(&BufferReceived);
        }

        SleepTime = 0;

        //
        // Thread needs to stop
        //
        if (!WaitListThreadRunning)
        {
            break;
        }

        //
        // Discard fake buffer used to notify end of stream
        //
        OS_LockMutex(&BufferLock);

        if ((waitListNextExtract != waitListNextInsert)
            && (waitListBuffer[waitListNextExtract] == FakeEndOfStreamBuffer))
        {
            bIgnoreEndOfStream = true;
        }

        OS_UnLockMutex(&BufferLock);

        if (bIgnoreEndOfStream)
        {
            // Sleep for a while to allow memsink pull to get the event
            //
            OS_SleepMilliSeconds(10);
            //
            // Nothing more to do
            //
            continue;
        }

        //
        // Release buffer with presentation time > current time
        // and if AVSync is not disabled
        //
        bool AVDSyncOff;
        AVDSyncOff = (Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueDisapply);

        if (AVDSyncOff == true)
        {
            //
            // Sleep for a while to allow memsink pull to get the event
            //
            OS_SleepMilliSeconds(10);
        }
        else
        {
            Buffer_t  bufferToRelease = NULL;
            OS_LockMutex(&BufferLock);

            //
            // Ignore case of empty wait list (released by thread or consumed by pull func)
            //
            if (waitListNextExtract != waitListNextInsert)
            {
                unsigned long long          presentationTimeMs;
                uint32_t                    bufferIndex;
                bufferToRelease = waitListBuffer[waitListNextExtract];
                //
                // Need presentation time from SystemPlaybackTime
                //
                bufferToRelease->GetIndex(&bufferIndex);
                SE_DEBUG(GetGroupTrace(), "Buffer [%d] presentation time check", bufferIndex);

                //
                // Check if the presentation time of the next buffer to extract is in the past
                //
                unsigned long long      timeNowMs;
                timeNowMs               = OS_GetTimeInMicroSeconds();
                presentationTimeMs      = SourceStreamBuffer[bufferIndex].OutputTiming->SystemPlaybackTime;

                //
                // Buffer is released if invalid presentation time
                if (ValidTime(presentationTimeMs))
                {
                    if (presentationTimeMs > timeNowMs)
                    {
                        SleepTime       = (presentationTimeMs - timeNowMs) / 1000;
                        SE_DEBUG(GetGroupTrace(), "Buffer [%d] Presentation time %llu, Now %llu, Sleep %d\n",
                                 bufferIndex, presentationTimeMs, timeNowMs,  SleepTime);
                        // Buffer can stay a little in the wait list
                        bufferToRelease = NULL;
                    }
                    else
                    {
                        SE_DEBUG(GetGroupTrace(), "Buffer [%d] late : Presentation time %llu, Now %llu, overdue %llu\n",
                                 bufferIndex, presentationTimeMs, timeNowMs, (timeNowMs - presentationTimeMs) / 1000);
                    }
                }
                else
                {
                    SE_DEBUG(GetGroupTrace(), "Buffer [%d]  Invalid presentation time %llu, 0x%llx\n", bufferIndex, presentationTimeMs, presentationTimeMs);
                }

                //
                // Release the buffer if needed and remove it from wait list
                //
                if (bufferToRelease)
                {
                    SE_DEBUG(GetGroupTrace(), "Buffer [%d] Released %p\n", bufferIndex, bufferToRelease);
                    mOutputPort->Insert((unsigned int) bufferToRelease);
                    bufferToRelease = NULL;
                    // Remove buffer from wait list
                    waitListNextExtract++;

                    if (waitListNextExtract == waitListLimit)
                    {
                        waitListNextExtract = 0;
                    }

                    // Other received buffers to proceed ?
                    if (waitListNextExtract != waitListNextInsert)
                    {
                        OS_SetEvent(&BufferReceived);
                    }
                }
            } // End wait list not empty

            OS_UnLockMutex(&BufferLock);
        }

        //
        // Wait until presentation time of next buffer
        //
        if (SleepTime > 0)
        {
            OS_SleepMilliSeconds(SleepTime);
        }
    } // while (WaitListThreadRunning)

    OS_Smp_Mb(); // Read memory barrier: rmb_for_WaitList_Terminating coupled with: wmb_for_WaitList_Terminating
    OS_SetEvent(&WaitListThreadTerminated);

    SE_DEBUG(GetGroupTrace(), "Terminating\n");
}
//}}}

