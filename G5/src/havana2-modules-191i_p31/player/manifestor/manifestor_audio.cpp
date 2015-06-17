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

Source file name : manifestor_audio.cpp
Author :           Daniel

Implementation of the base manifestor class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
14-May-07   Created (from manifestor_video.cpp)             Daniel

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "manifestor_audio.h"
#include "st_relay.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#undef  MANIFESTOR_TAG
#define MANIFESTOR_TAG          "Manifestor_Audio_c::"

#define BUFFER_DECODE_BUFFER			"AudioDecodeBuffer"
#define BUFFER_DECODE_BUFFER_TYPE		{BUFFER_DECODE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 32, 32, false, false, 0}
static BufferDataDescriptor_t			InitialDecodeBufferDescriptor = BUFFER_DECODE_BUFFER_TYPE;

////////////////////////////////////////////////////////////////////////////
///
///
///
Manifestor_Audio_c::Manifestor_Audio_c  (void)
    : DestroySyncrhonizationPrimatives(false)
{
    MANIFESTOR_DEBUG (">><<\n");

    Configuration.ManifestorName	= "Audio";
    Configuration.StreamType		= StreamTypeAudio;
    Configuration.DecodeBufferDescriptor= &InitialDecodeBufferDescriptor;
    Configuration.PostProcessControlBufferCount	= 16;

    if (InitializationStatus != ManifestorNoError)
    {
	MANIFESTOR_ERROR ("Initialization status not valid - aborting init\n");
	return;
    }

    if (OS_InitializeMutex (&BufferQueueLock) != OS_NO_ERROR)
    {
	MANIFESTOR_ERROR ("Unable to create the buffer queue lock\n");
	InitializationStatus = ManifestorError;
	return;
    }

    if (OS_InitializeEvent (&BufferQueueUpdated) != OS_NO_ERROR)
    {
	MANIFESTOR_ERROR("Unable to create the buffer queue update event\n");
	OS_TerminateMutex( &BufferQueueLock );
	InitializationStatus = ManifestorError;
	return;
    }

    DestroySyncrhonizationPrimatives = true;

    RelayfsIndex = st_relayfs_getindex(ST_RELAY_SOURCE_AUDIO_MANIFESTOR);

    Manifestor_Audio_c::Reset ();
}


////////////////////////////////////////////////////////////////////////////
///
///
///
Manifestor_Audio_c::~Manifestor_Audio_c   (void)
{
    MANIFESTOR_DEBUG (">><<\n");

    Manifestor_Audio_c::Halt ();

    st_relayfs_freeindex(ST_RELAY_SOURCE_AUDIO_MANIFESTOR,RelayfsIndex);

    if( DestroySyncrhonizationPrimatives )
    {
	OS_TerminateMutex(&BufferQueueLock);
	OS_TerminateEvent(&BufferQueueUpdated);
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::Halt (void)
{
    MANIFESTOR_DEBUG (">><<\n");

    //
    // It is unlikely that we have any decode buffers queued at this point but if we do we
    // deperately need to jettison them (otherwise the reference these buffers hold to the
    // coded data can never be undone).
    //

    ReleaseQueuedDecodeBuffers();
    
    MANIFESTOR_ASSERT(0 == QueuedBufferCount);
    MANIFESTOR_ASSERT(0 == NotQueuedBufferCount);

    return Manifestor_Base_c::Halt ();
}


////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t Manifestor_Audio_c::Reset (void)
{
    MANIFESTOR_DEBUG (">><<\n");

    if (TestComponentState (ComponentRunning))
	Halt ();

    BufferQueueHead             = INVALID_BUFFER_ID;
    BufferQueueTail             = ANY_BUFFER_ID;
    QueuedBufferCount           = 0;
    NotQueuedBufferCount        = 0;
    DisplayUpdatePending        = false;
    ForcedUnblock               = false;

    memset(&LastSeenAudioParameters, 0, sizeof(LastSeenAudioParameters));

    return Manifestor_Base_c::Reset ();
}

////////////////////////////////////////////////////////////////////////////
///
/// Allocate and initialize the decode buffer if required, and pass them to the caller.
/// 
/// \param Pool         Pointer to location for buffer pool pointer to hold
///                     details of the buffer pool holding the decode buffers.
/// \return             Succes or failure
///
ManifestorStatus_t      Manifestor_Audio_c::GetDecodeBufferPool         (class BufferPool_c**   Pool)
{
    unsigned int                        i;
    ManifestorStatus_t			Status;

    MANIFESTOR_DEBUG (">><<\n");

    // Only create the pool if it doesn't exist and buffers have been created
    if (DecodeBufferPool != NULL)
    {
	*Pool   = DecodeBufferPool;
	return ManifestorNoError;
    }

    Status      = Manifestor_Base_c::GetDecodeBufferPool( Pool );
    if( Status != ManifestorNoError )
    {
	MANIFESTOR_ERROR ("Failed to create a pool of decode buffers.\n");
	DecodeBufferPool        = NULL;
	return ManifestorError;
    }

    // Fill in surface descriptor details with our assumed defaults
    SurfaceDescriptor.BitsPerSample = 32;
    SurfaceDescriptor.ChannelCount = 8;
    SurfaceDescriptor.SampleRateHz = 0;

    for (i = 0; i < BufferConfiguration.MaxBufferCount; i++)
    {
	StreamBuffer[i].BufferIndex                     = i;
	StreamBuffer[i].QueueCount                      = 0;
	StreamBuffer[i].TimeOfGoingOnDisplay            = 0;
	StreamBuffer[i].BufferState                     = AudioBufferStateAvailable;
    }

    BufferQueueHead                     = INVALID_BUFFER_ID;
    BufferQueueTail                     = ANY_BUFFER_ID;
    QueuedBufferCount                   = 0;
    NotQueuedBufferCount                = 0;

    // Let outside world know about the created pool
    *Pool   = DecodeBufferPool;
    SetComponentState (ComponentRunning);

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// Fill in private structure with timing details of display surface.
///
/// \param      SurfaceParameters pointer to structure to complete
/// 
ManifestorStatus_t      Manifestor_Audio_c::GetSurfaceParameters (void** SurfaceParameters )
{
    MANIFESTOR_DEBUG (">><<\n");
    *SurfaceParameters  = (void*)&SurfaceDescriptor;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::GetNextQueuedManifestationTime   (unsigned long long*    Time)
{
    MANIFESTOR_DEBUG(">><<\n");

    // this is not an appropriate implementation... it just assumes that audio latency is a quarter of
    // a second.
    *Time     = OS_GetTimeInMicroSeconds() + 250000;

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release any buffers queued within the StreamBuffer structure.
///
/// Pass any buffer held on the Manifestor_Audio_c::StreamBuffer structure
/// onto the output ring.
///
/// The super-class will tidy up an queued events.
///
ManifestorStatus_t      Manifestor_Audio_c::ReleaseQueuedDecodeBuffers   ()
{
    ManifestorStatus_t Status;
    MANIFESTOR_DEBUG(">><<\n");

    OS_LockMutex(&BufferQueueLock);

    //
    // Emit all the queued buffers
    //

    for (unsigned int i=BufferQueueHead; i!=INVALID_BUFFER_ID; i=StreamBuffer[i].NextIndex)
    {
	Status = ReleaseBuffer(i);
	if( Status != ManifestorNoError )
	{
	    MANIFESTOR_ERROR("Sub-class got cross when we tried to release a buffer - ignoring them\n");
	}
        QueuedBufferCount--;
	if( StreamBuffer[i].EventPending )
		ServiceEventQueue( i );
	OutputRing->Insert( (unsigned int) StreamBuffer[i].Buffer );
    }

    BufferQueueHead = INVALID_BUFFER_ID;
    BufferQueueTail = ANY_BUFFER_ID;

    OS_UnLockMutex(&BufferQueueLock);

    return Manifestor_Base_c::ReleaseQueuedDecodeBuffers();
}


////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::InitialFrame       (class Buffer_c*         Buffer)
{
    MANIFESTOR_DEBUG (">><<\n");
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::QueueDecodeBuffer   (class Buffer_c*        Buffer)
{
    ManifestorStatus_t                  Status;
    BufferStatus_t                      BufferStatus;
    unsigned int                        BufferIndex;

    //MANIFESTOR_DEBUG(">><<\n");
    AssertComponentState ("Manifestor_Audio_c::QueueDecodeBuffer", ComponentRunning);

    //
    // Obtain the index for the buffer and populate the parameter data.
    //

    BufferStatus = Buffer->GetIndex (&BufferIndex);
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR ("Unable to lookup buffer index %x.\n", BufferStatus);
	return ManifestorError;
    }

    StreamBuffer[BufferIndex].Buffer = Buffer;
    StreamBuffer[BufferIndex].EventPending = EventPending;
    EventPending = false;

    BufferStatus = Buffer->ObtainMetaDataReference (Player->MetaDataParsedFrameParametersReferenceType,
						    (void**) &StreamBuffer[BufferIndex].FrameParameters);
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR ("Unable to access buffer parsed frame parameters %x.\n", BufferStatus);
	return ManifestorError;
    }

    BufferStatus = Buffer->ObtainMetaDataReference (Player->MetaDataParsedAudioParametersType,
						    (void**) &StreamBuffer[BufferIndex].AudioParameters);
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR ("Unable to access buffer parsed audio parameters %x.\n", BufferStatus);
	return ManifestorError;
    }

    Buffer->DumpToRelayFS(ST_RELAY_TYPE_DECODED_AUDIO_BUFFER, ST_RELAY_SOURCE_AUDIO_MANIFESTOR + RelayfsIndex, (void*)Player);

    BufferStatus = Buffer->ObtainMetaDataReference (Player->MetaDataAudioOutputTimingType,
						    (void**) &StreamBuffer[BufferIndex].AudioOutputTiming);
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR ("Unable to access buffer audio output timing parameters %x.\n", BufferStatus);
	return ManifestorError;
    }

    BufferStatus = Buffer->ObtainDataReference( NULL, NULL,
						(void**)(&StreamBuffer[BufferIndex].Data), UnCachedAddress );
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR ("Unable to obtain buffer's data reference %x.\n", BufferStatus);
	return ManifestorError;

    }

    StreamBuffer[BufferIndex].QueueAsCodedData = true;

    //
    // Check if there are new audio parameters (i.e. change of sample rate etc.) and note this
    //

    if( 0 == memcmp( &LastSeenAudioParameters, StreamBuffer[BufferIndex].AudioParameters,
		     sizeof(LastSeenAudioParameters ) ) )
    {
	StreamBuffer[BufferIndex].UpdateAudioParameters = false;
    }
    else
    {
	StreamBuffer[BufferIndex].UpdateAudioParameters = true;
	memcpy( &LastSeenAudioParameters, StreamBuffer[BufferIndex].AudioParameters,
		sizeof(LastSeenAudioParameters ) );
    }

    //
    // Allow the sub-class to have a peek at the buffer before we queue it for display
    //

    Status = QueueBuffer (BufferIndex);
    if (Status != ManifestorNoError)
    {
        MANIFESTOR_ERROR("Unable to queue buffer %x.\n", Status);
        return Status;
    }

    //
    // Enqueue the buffer for display within the playback thread
    //

    OS_LockMutex( &BufferQueueLock );
    
    QueuedBufferCount++;

    StreamBuffer[BufferIndex].NextIndex = INVALID_BUFFER_ID; // end marker    

    if( BufferQueueHead == INVALID_BUFFER_ID )
    {
	BufferQueueHead = BufferIndex;
    }
    else
    {
	StreamBuffer[BufferQueueTail].NextIndex = BufferIndex;
    }

    BufferQueueTail = BufferIndex;

    OS_UnLockMutex( &BufferQueueLock );

    OS_SetEvent( &BufferQueueUpdated );

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Retrieve buffer index of last buffer queued.
///
/// If no buffers yet queued returns ANY_BUFFER_ID
///
/// \return     Buffer index of last buffer sent for display
///
unsigned int    Manifestor_Audio_c::GetBufferId    (void)
{
    return BufferQueueTail;
}

////////////////////////////////////////////////////////////////////////////
/// 
/// Get original PTS of currently visible buffer
///
/// \param Pts          Pointer to Pts variable
///
/// \return             Success if Pts value is available
///
ManifestorStatus_t Manifestor_Audio_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long* Time)
{
    BufferStatus_t                      BufferStatus;
    struct ParsedFrameParameters_s*     FrameParameters;

    MANIFESTOR_DEBUG ("\n");

    *Time      = INVALID_TIME;
    
    OS_AutoLockMutex AutoLock( &BufferQueueLock );

    if (BufferQueueHead == INVALID_BUFFER_ID)
    {
	MANIFESTOR_DEBUG( "No buffer on display.\n" );
	return ManifestorError;
    }
    
    BufferStatus = StreamBuffer[BufferQueueHead].Buffer->ObtainMetaDataReference (
		    Player->MetaDataParsedFrameParametersReferenceType, (void**)&FrameParameters);
    if (BufferStatus != BufferNoError)
    {
	MANIFESTOR_ERROR( "Unable to access buffer parsed frame parameters %x.\n", BufferStatus );
	return ManifestorError;
    }

    if( !ValidTime( FrameParameters->NativePlaybackTime ) )
    {
	MANIFESTOR_ERROR( "Buffer on display does not have a valid native playback time\n" );
	return ManifestorError;
    }
    
    MANIFESTOR_DEBUG("%lld\n", FrameParameters->NativePlaybackTime);
    *Time       = FrameParameters->NativePlaybackTime;

    return ManifestorNoError;
}


/////////////////////////////////////////////////////////////////////////////
///
/// \brief Get number of frames appeared on the display
/// \param Framecount   Pointer to FrameCount variable
/// \return             Success
///
ManifestorStatus_t Manifestor_Audio_c::GetFrameCount (unsigned long long* FrameCount)
{
    //MANIFESTOR_DEBUG ("\n");

    *FrameCount         = 0ull;

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Dequeue a buffer from the queue of buffers ready for display.
///
/// Must be called only from the mixer playback thread.
///
/// \param BufferIndexPtr Pointer to store the index of the dequeued buffer.
/// \param NonBlock If true, return an error rather than blocking.
/// \return Manifestor status code, ManifestorNoError indicates success.
///
ManifestorStatus_t Manifestor_Audio_c::DequeueBuffer(unsigned int *BufferIndexPtr, bool NonBlock)
{
unsigned int BufferIndex;

    while( !ForcedUnblock )
    {
	//
	// Dequeue the buffer
	//

	OS_LockMutex(&BufferQueueLock);
	if( BufferQueueHead != INVALID_BUFFER_ID )
	{
	    BufferIndex = BufferQueueHead;
	    BufferQueueHead = StreamBuffer[BufferIndex].NextIndex;
            
            QueuedBufferCount--;
            NotQueuedBufferCount++;
            
	    OS_UnLockMutex(&BufferQueueLock);

	    *BufferIndexPtr = BufferIndex;
	    return ManifestorNoError;
	}
	OS_UnLockMutex(&BufferQueueLock);

	//
	// Block if no buffer is available.
	//

	if( NonBlock )
	    return ManifestorWouldBlock;

	OS_WaitForEvent(&BufferQueueUpdated, OS_INFINITE);
	OS_ResetEvent(&BufferQueueUpdated);     
    }

    return ManifestorError;
}


////////////////////////////////////////////////////////////////////////////
/// 
/// Push a previously dequeued buffer back to the head of the buffer queue.
///
/// Must be called only from the mixer playback thread.
///
/// \param BufferIndex Index of the previously dequeued buffer.
///
void Manifestor_Audio_c::PushBackBuffer(unsigned int BufferIndex)
{
    OS_LockMutex(&BufferQueueLock);
    StreamBuffer[BufferIndex].NextIndex = BufferQueueHead;
    if( BufferQueueHead == INVALID_BUFFER_ID )
	BufferQueueTail = BufferIndex;
    BufferQueueHead = BufferIndex;
    QueuedBufferCount++;
    NotQueuedBufferCount--;
    OS_UnLockMutex(&BufferQueueLock);
}


// /////////////////////////////////////////////////////////////////////
//
//      Function to flesh out a buffer request structure

ManifestorStatus_t   Manifestor_Audio_c::FillOutBufferStructure( BufferStructure_t	 *RequestedStructure )
{
    //
    // Check that the format type is compatible
    //

    if( RequestedStructure->Format != FormatAudio )
    {
	report( severity_error, "Manifestor_Audio_c::FillOutBufferStructure - Unsupported buffer format (%d)\n", RequestedStructure->Format );
	return ManifestorError;
    }

    //
    // Calculate the appropriate fields
    //

    RequestedStructure->ComponentCount		= 1;
    RequestedStructure->ComponentOffset[0]	= 0;

    RequestedStructure->Strides[0][0]		= RequestedStructure->Dimension[0] / 8;
    RequestedStructure->Strides[1][0]		= RequestedStructure->Dimension[1] * RequestedStructure->Strides[0][0];

    RequestedStructure->Size			= (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * RequestedStructure->Dimension[2]) / 8;

    return ManifestorNoError;
}

