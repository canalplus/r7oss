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

#include "manifestor_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#define BUFFER_OVERLAP_MAX_TRIES        16

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor :
//      Action  : Allocate and initialise all necessary structures
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_Base_c::Manifestor_Base_c    (void)
{
    InitializationStatus        = ManifestorError;

    //MANIFESTOR_DEBUG ("\n");

    memset (&Configuration, 0x00, sizeof(ManifestorConfiguration_t));
    Configuration.ManifestorName                = "Noname";
    Configuration.StreamType                    = StreamTypeNone;
    Configuration.DecodeBufferDescriptor        = NULL;
    Configuration.PostProcessControlBufferCount = 0;
    Configuration.OutputRateSmoothingFramesBetweenReCalculate = 8;

    DecodeBufferPool                            = NULL;
    DecodeMemoryDevice                          = ALLOCATOR_INVALID_DEVICE;
    LastDecodeBufferSize                        = 1;

    PostProcessControlBufferPool                = NULL;

    OutputRateSmoothingFramesSinceLastCalculation = 0;
    OutputRateSmoothingIndex                    = 0;
    OutputRateSmoothingLastRate                 = 0;
    OutputRateSmoothingSubPPMPart               = 0;
    OutputRateSmoothingBaseValue                = 1000000;
    OutputRateSmoothingLastValue                = 1000000;
    OutputRateMovingTo				= false;

    if (OS_InitializeMutex (&EventLock) != OS_NO_ERROR)
    {
        MANIFESTOR_ERROR ("Failed to init Event mutex\n");
        EventLockValid = false; // suppress destruction
        return;
    }
    EventLockValid = true;

    Manifestor_Base_c::Reset ();

    InitializationStatus        = ManifestorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor :
//      Action  : Give up switch off the lights and go home
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_Base_c::~Manifestor_Base_c   (void)
{
    if( EventLockValid )
    {
        OS_TerminateMutex (&EventLock);
        EventLockValid  = false;
    }

    //MANIFESTOR_DEBUG ("\n");
    Manifestor_Base_c::Halt  ();
    Manifestor_Base_c::Reset ();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Halt :
//      Action  : Terminate access to any registered resources
//      Input   :
//      Output  :
//      Result  :
//

ManifestorStatus_t      Manifestor_Base_c::Halt (void)
{
    //MANIFESTOR_DEBUG ("Base\n");

    return BaseComponentClass_c::Halt ();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Reset function
//      Action  : Release any resources, and reset all variables
//      Input   :
//      Output  :
//      Result  :
//

ManifestorStatus_t      Manifestor_Base_c::Reset        (void)
{
    int i;

    //MANIFESTOR_DEBUG ("Base\n");

    if (TestComponentState (ComponentRunning))
        Halt ();

    memset (&BufferConfiguration, 0x00, sizeof(ManifestorBufferConfiguration_t));

    if (DecodeBufferPool != NULL)
    {
        BufferManager->DestroyPool (DecodeBufferPool);
        DecodeBufferPool        = NULL;
    }

    if (PostProcessControlBufferPool != NULL)
    {
        BufferManager->DestroyPool (PostProcessControlBufferPool);
        PostProcessControlBufferPool    = NULL;
    }

    if( DecodeMemoryDevice != ALLOCATOR_INVALID_DEVICE )
    {
        AllocatorClose( DecodeMemoryDevice );
        DecodeMemoryDevice      = ALLOCATOR_INVALID_DEVICE;
    }

    EventPending        = false;
    NextEvent           = 0;
    LastEvent           = 0;
    for (i = 0; i < MAXIMUM_WAITING_EVENTS; i++)
    {
        EventList[i].Id         = INVALID_BUFFER_ID;
    }
    return BaseComponentClass_c::Reset ();
}


// /////////////////////////////////////////////////////////////////////////
//
//      SetModuleParameters function
//      Action  : Allows external user to set up important environmental parameters
//      Input   :
//      Output  :
//      Result  :
//

CodecStatus_t   Manifestor_Base_c::SetModuleParameters(
                                                unsigned int   ParameterBlockSize,
                                                void          *ParameterBlock )
{
ManifestorParameterBlock_t      *ManifestorParameterBlock = (ManifestorParameterBlock_t *)ParameterBlock;

//

    if (ParameterBlockSize != sizeof(ManifestorParameterBlock_t))
    {
        report( severity_error, "Manifestor_Base_c::SetModuleParameters: Invalid parameter block.\n");
        return ManifestorError;
    }

//

    switch (ManifestorParameterBlock->ParameterType)
    {
        case ManifestorBufferConfiguration:
            memcpy( &BufferConfiguration, &ManifestorParameterBlock->BufferConfiguration, sizeof(ManifestorBufferConfiguration_t) );
            report( severity_info, "Manifestor_Base_c::SetModuleParameters - Setting buffer configuration\n" );
            break;

        default:
            report( severity_error, "Manifestor_Base_c::SetModuleParameters: Unrecognised parameter block (%d).\n", ManifestorParameterBlock->ParameterType);
            return ManifestorError;
    }

    return  ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      GetDecodedBufferPool :
//      Action  : Retrieve details of decode buffers
//      Input   : Pointer to location for buffer pool pointer
//      Output  : Details of the buffer pool holding the decode buffers.
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetDecodeBufferPool  (BufferPool_t*          Pool)
{
PlayerStatus_t          Status;
allocator_status_t      AStatus;

//

    MANIFESTOR_DEBUG ("\n");

    //
    // Nick modified buffer pool construction to run here
    //

    //
    // Have we been configured.
    //

    if( BufferConfiguration.TotalBufferMemory == 0 )
    {
        MANIFESTOR_ERROR("(%s) - BufferConfiguration has not been specified.\n", Configuration.ManifestorName );
        return ManifestorError;
    }

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if( DecodeBufferPool == NULL )
    {

        //
        // Obtain the class list
        //

        Player->GetBufferManager( &BufferManager );

        //
        // Initialize the data type we use.
        //

        Status          = BufferManager->FindBufferDataType( Configuration.DecodeBufferDescriptor->TypeName, &DecodeBufferType );
        if( Status != BufferNoError )
        {
            Status      = BufferManager->CreateBufferDataType( Configuration.DecodeBufferDescriptor, &DecodeBufferType );
            if( Status != BufferNoError )
            {
                report( severity_error, "Manifestor_Base_c::GetDecodeBufferPool(%s) - Failed to create the decode buffer type.\n", Configuration.ManifestorName );
                return ManifestorError;
            }
        }

        BufferManager->GetDescriptor( DecodeBufferType, BufferDataTypeBase, &DecodeBufferDescriptor );

        //
        // Get the memory and Create the pool with it
        //

        AStatus = PartitionAllocatorOpen( &DecodeMemoryDevice, BufferConfiguration.PartitionName, BufferConfiguration.TotalBufferMemory, true );
        if( AStatus != allocator_ok )
        {
            report( severity_error, "Manifestor_Base_c::GetDecodeBufferPool(%s) - Failed to allocate memory\n", Configuration.ManifestorName );
            return PlayerInsufficientMemory;
        }

        DecodeBufferMemory[CachedAddress]         = AllocatorUserAddress( DecodeMemoryDevice );
        DecodeBufferMemory[UnCachedAddress]       = AllocatorUncachedUserAddress( DecodeMemoryDevice );
        DecodeBufferMemory[PhysicalAddress]       = AllocatorPhysicalAddress( DecodeMemoryDevice );

//

        Status  = BufferManager->CreatePool( &DecodeBufferPool, DecodeBufferType, BufferConfiguration.MaxBufferCount, BufferConfiguration.TotalBufferMemory, DecodeBufferMemory );
        if( Status != BufferNoError )
        {
            report( severity_error, "Manifestor_Base_c::GetDecodeBufferPool(%s) - Failed to create the pool.\n", Configuration.ManifestorName );
            return PlayerInsufficientMemory;
        }

        //
        // Attach the structure data
        //

        Status  = DecodeBufferPool->AttachMetaData( Player->MetaDataBufferStructureType );
        if( Status != BufferNoError )
        {
            report( severity_error, "Manifestor_Base_c::GetDecodeBufferPool(%s) - Failed to attach buffer structure data to the pool.\n", Configuration.ManifestorName );
            return PlayerInsufficientMemory;
        }
    }

    //
    // Setup the parameters and return
    //

    *Pool                       = DecodeBufferPool;

//

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      PostProcessControlBufferPool :
//      Action  : create a buffer pool for postprocessing
//      Input   : Pointer to location for buffer pool pointer
//      Output  : Details of the buffer pool holding the post processing control buffers.
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetPostProcessControlBufferPool  (BufferPool_t*          Pool)
{
PlayerStatus_t          Status;
BufferType_t            Type;

//

    MANIFESTOR_DEBUG ("\n");

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if( PostProcessControlBufferPool == NULL )
    {

        //
        // Obtain the buffer manager, just in case no-one else has
        //

        Player->GetBufferManager( &BufferManager );

        //
        // Create the pool
        //

        Type    = (Configuration.StreamType == StreamTypeVideo) ? Player->BufferVideoPostProcessingControlType :
                                                                  Player->BufferAudioPostProcessingControlType;

        Status  = BufferManager->CreatePool( &PostProcessControlBufferPool, Type, Configuration.PostProcessControlBufferCount );
        if( Status != BufferNoError )
        {
            report( severity_error, "Manifestor_Base_c::GetPostProcessControlBufferPool(%s) - Failed to create the pool.\n", Configuration.ManifestorName );
            return PlayerInsufficientMemory;
        }
    }

    //
    // Setup the parameters and return
    //

    *Pool                       = PostProcessControlBufferPool;

//

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      RegisterOutputBufferRing :
//      Action  : Save details of ring on which to place manifested buffers
//      Input   : Pointer to ring to use for finished decode frames
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::RegisterOutputBufferRing     (Ring_t         Ring)
{
BufferPool_t            LocalDecodeBufferPool;
ManifestorStatus_t      Status;

//

    MANIFESTOR_DEBUG ("\n");

    //
    // On registration of the output ring we force construction of the decode buffer pool
    //

    Status      = Manifestor_Base_c::GetDecodeBufferPool( &LocalDecodeBufferPool );
    if( Status != ManifestorNoError )
        return Status;

//

    OutputRing  = Ring;

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      GetSurfaceParameters :
//      Action  : Fill in private structure with details about display surface
//      Input   : Opaque pointer to structure to complete
//      Output  : Filled in structure
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetSurfaceParameters (void** SurfaceParameters )
{
    MANIFESTOR_DEBUG ("\n");

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The GetNextQueuedManifestTime function :
//      Action  : Return the earliest system time at which the next frame to be queued will be manifested
//      Input   : Pointer to 64-bit system time variable
//      Output  : Estimated time
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetNextQueuedManifestTime    (unsigned long long*    Time)
{
    MANIFESTOR_DEBUG("\n");

    return ManifestorNoError;
}


//{{{  ReleaseQueuedDecodeBuffers
//{{{  doxynote
/// \brief      Passes onto the output ring any decode buffers that are currently queued,
///             but not in the process of being manifested.
/// \return     Buffer index of last buffer sent for display
//}}}  
ManifestorStatus_t      Manifestor_Base_c::ReleaseQueuedDecodeBuffers   (void)
{
    int i;

    MANIFESTOR_DEBUG ("\n");

    FlushDisplayQueue ();

    OS_LockMutex (&EventLock);

    NextEvent   = 0;
    LastEvent   = 0;
    for (i = 0; i < MAXIMUM_WAITING_EVENTS; i++)
        EventList[i].Id         = INVALID_BUFFER_ID;

    OS_UnLockMutex (&EventLock);

    return ManifestorNoError;
}
//}}}  


// /////////////////////////////////////////////////////////////////////////
//
//      QueueNullManifestation :
//      Action  : Insert null frame into display sequence
//      Input   :
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::QueueNullManifestation       (void)
{
    MANIFESTOR_DEBUG ("\n");

    return ManifestorNoError;
}

//{{{  QueueEventSignal
//{{{  doxynote
/// \brief      Copy event record to be signalled when last queued buffer is displayed
/// \param      Event Pointer to a player 2 event record to be signalled
/// \return     Success if saved, failure if event queue full
//}}}  
ManifestorStatus_t      Manifestor_Base_c::QueueEventSignal    (PlayerEventRecord_t*   Event)
{
    MANIFESTOR_DEBUG ("\n");

    OS_LockMutex (&EventLock);
    if (((LastEvent + 1) % MAXIMUM_WAITING_EVENTS) == NextEvent)
    {
        //MANIFESTOR_ERROR("Event queue overflow\n");
        NextEvent++;
        if (NextEvent == MAXIMUM_WAITING_EVENTS)
            NextEvent   = 0;
        //OS_UnLockMutex (&EventLock);
        //return ManifestorError;
    }
    EventList[LastEvent].Id     = GetBufferId ();
    EventList[LastEvent].Event  = *Event;
    EventPending                = true;

    LastEvent++;
    if (LastEvent == MAXIMUM_WAITING_EVENTS)
        LastEvent       = 0;
    OS_UnLockMutex (&EventLock);

    return ManifestorNoError;
}
//}}}  

//{{{  ServiceEventQueue
//{{{  doxynote
/// \brief      Signal all events associated with buffer just manifested
///             We need to travel down the event queue signalling all events
///             including the one associated with the chosen buffer.
/// \param Id   Index of buffer
/// \return
//}}}  
ManifestorStatus_t      Manifestor_Base_c::ServiceEventQueue   (unsigned int    Id)
{
    bool        EventsToQueue;
    bool        FoundId         = false;
    MANIFESTOR_DEBUG ("\n");

    OS_LockMutex (&EventLock);
    EventsToQueue       = (LastEvent != NextEvent);
    while (EventsToQueue)
    {
        if (EventList[NextEvent].Id == Id)
            FoundId     = true;

        EventList[NextEvent].Id = INVALID_BUFFER_ID;
        if (EventList[NextEvent].Event.Code != EventIllegalIdentifier)
            Player->SignalEvent (&EventList[NextEvent].Event);

        NextEvent++;
        if (NextEvent == MAXIMUM_WAITING_EVENTS)
            NextEvent   = 0;

        if ((NextEvent == LastEvent) || (FoundId && (EventList[NextEvent].Id != ANY_BUFFER_ID) && (EventList[NextEvent].Id != Id)))
            EventsToQueue       = false;
    }
    OS_UnLockMutex (&EventLock);

    return ManifestorNoError;
}
//}}}  

// /////////////////////////////////////////////////////////////////////
//
//      The get decode buffer function

ManifestorStatus_t   Manifestor_Base_c::GetDecodeBuffer(
                                                BufferStructure_t        *RequestedStructure,
                                                Buffer_t                 *Buffer )
{
ManifestorStatus_t       Status;
BufferStructure_t       *AttachedRequestStructure;
unsigned char           *Data;
Buffer_t                 BufferList[BUFFER_OVERLAP_MAX_TRIES];
unsigned int             i;
unsigned int             BufferCount;
//

     AssertComponentState ("Manifestor_Base_c::GetDecodeBuffer", ComponentRunning);

    //
    // First flesh out the request structure
    //

    if( RequestedStructure->Format == FormatMarkerFrame )
    {
        RequestedStructure->Size        = 0;
    }
    else
    {
        Status  = FillOutBufferStructure( RequestedStructure );
        if( Status != ManifestorNoError )
        {
            MANIFESTOR_ERROR( "(%s) - Failed to flesh out request structure.\n", Configuration.ManifestorName );
            return Status;
        }

        LastDecodeBufferSize    = RequestedStructure->Size;
    }

    BufferCount                 = 0;
    while( BufferCount < BUFFER_OVERLAP_MAX_TRIES )
    {
        //
        // Obtain a buffer
        //

        Status                  = DecodeBufferPool->GetBuffer( Buffer, IdentifierManifestor, RequestedStructure->Size );
        if( Status != ManifestorNoError )
        {
            MANIFESTOR_ERROR( "(%s) - Failed to obtain a buffer.\n", Configuration.ManifestorName );
            return Status;
        }

        if (RequestedStructure->Format == FormatMarkerFrame)
            break;

        Status                  = (*Buffer)->ObtainDataReference (NULL, NULL, (void**)(&Data), PhysicalAddress);
        if( Status != ManifestorNoError )
        {
            MANIFESTOR_ERROR( "(%s) - Failed to obtain buffer address.\n", Configuration.ManifestorName );
            return Status;
        }

        if (BufferAvailable (Data, RequestedStructure->Size))
            break;

        BufferList[BufferCount++]       = *Buffer;

    }

    if (BufferCount == BUFFER_OVERLAP_MAX_TRIES)
        BufferCount--;                                          // Accept this last buffer ragardless

    for (i=0; i<BufferCount; i++)
        BufferList[i]->DecrementReferenceCount();               // put unwanted buffers back;

    //
    // Copy the request structure into the attached meta data
    //

    Status      = (*Buffer)->ObtainMetaDataReference(   Player->MetaDataBufferStructureType,
                                                        (void **)(&AttachedRequestStructure) );
    if( Status != ManifestorNoError )
    {
        report( severity_error, "Manifestor_Base_c::GetDecodeBuffer(%s) - Failed to obtain a reference to the structure meta data.\n", Configuration.ManifestorName );
        return Status;
    }

    memcpy( AttachedRequestStructure, RequestedStructure, sizeof(BufferStructure_t) );

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////
//
//      The get decode buffer count function

ManifestorStatus_t   Manifestor_Base_c::GetDecodeBufferCount( unsigned int       *Count )
{
unsigned int            MinimumBufferSize;

//

     AssertComponentState ("Manifestor_Base_c::GetDecodeBufferCount", ComponentRunning);

    //
    // Calculate the maximum number of buffers that can be allocated
    //

    MinimumBufferSize   = max( DecodeBufferDescriptor->RequiredAllignment, DecodeBufferDescriptor->AllocationUnitSize );
    MinimumBufferSize   = max( LastDecodeBufferSize, MinimumBufferSize );

    *Count      = min( (BufferConfiguration.TotalBufferMemory / MinimumBufferSize), BufferConfiguration.MaxBufferCount );
    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////
//
//      The default synchronize output function

ManifestorStatus_t   Manifestor_Base_c::SynchronizeOutput(              void )
{
    return PlayerNotSupported;
}


// /////////////////////////////////////////////////////////////////////
//
//      The get decode buffer count function

ManifestorStatus_t   Manifestor_Base_c::DerivePPMValueFromOutputRateAdjustment(
                                                        Rational_t               OutputRateAdjustment,
                                                        int                     *PPMValue )
{
Rational_t      SubPartValue0;
Rational_t      SubPartValue1;

    //
    // Reset on changed rate
    //

    if( OutputRateAdjustment != OutputRateSmoothingLastRate )
    {
#if 0
if( Configuration.StreamType == StreamTypeAudio ) report( severity_info, "Output Clock Change - %3d (%d)\n", OutputRateSmoothingBaseValue - IntegerPart( OutputRateAdjustment * 1000000 ), IntegerPart( OutputRateAdjustment * 1000000 ) );
#endif

        OutputRateSmoothingFramesSinceLastCalculation   = 0;
        OutputRateSmoothingIndex                        = 0;
        OutputRateSmoothingLastRate                     = OutputRateAdjustment;
        OutputRateSmoothingSubPPMPart                   = Remainder(   OutputRateAdjustment * 1000000 );
        OutputRateSmoothingBaseValue                    = IntegerPart( OutputRateAdjustment * 1000000 );
	OutputRateMovingTo				= true;
    }

    //
    // If we have changed rate, and are moving to the new rate, we only allow 1 ppm change per frame
    //

    if( OutputRateMovingTo && (OutputRateSmoothingLastValue != OutputRateSmoothingBaseValue) )
    {
	OutputRateSmoothingLastValue			= OutputRateSmoothingLastValue + 
								((OutputRateSmoothingLastValue > OutputRateSmoothingBaseValue) ? -1 : 1);
        *PPMValue               = OutputRateSmoothingLastValue;
        return ManifestorNoError;
    }

    OutputRateMovingTo		= false;

    //
    // Set default value, and check if we should just return
    //

    if( OutputRateSmoothingFramesSinceLastCalculation < Configuration.OutputRateSmoothingFramesBetweenReCalculate )
    {
        OutputRateSmoothingFramesSinceLastCalculation++;
        *PPMValue               = OutputRateSmoothingLastValue;
        return ManifestorNoError;
    }

    //
    // Calculate current and next subpart values
    //

    SubPartValue0                       = OutputRateSmoothingIndex * OutputRateSmoothingSubPPMPart;
    SubPartValue1                       = SubPartValue0 + OutputRateSmoothingSubPPMPart;

    OutputRateSmoothingIndex++;

    OutputRateSmoothingLastValue        = (SubPartValue0.IntegerPart() == SubPartValue1.IntegerPart()) ?
                                                OutputRateSmoothingBaseValue :
                                                OutputRateSmoothingBaseValue + 1;

//

    OutputRateSmoothingFramesSinceLastCalculation = 0;
    *PPMValue                           = OutputRateSmoothingLastValue;

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////
//
//      The validate decode buffer address function

ManifestorStatus_t   Manifestor_Base_c::ValidatePhysicalDecodeBufferAddress( unsigned int	Address )
{
    if( !inrange( (unsigned int)Address, (unsigned int)DecodeBufferMemory[PhysicalAddress], 
					((unsigned int)DecodeBufferMemory[PhysicalAddress] + BufferConfiguration.TotalBufferMemory - 1)) )
    {
	report( severity_fatal, "Manifestor_Base_c::ValidateDecodeBufferAddress - Invalid address (%08x not in %08x %08x)\n",
				(unsigned int)Address, 
				(unsigned int)DecodeBufferMemory[PhysicalAddress], 
				((unsigned int)DecodeBufferMemory[PhysicalAddress] + BufferConfiguration.TotalBufferMemory - 1) );
    }
    return ManifestorNoError;
}

