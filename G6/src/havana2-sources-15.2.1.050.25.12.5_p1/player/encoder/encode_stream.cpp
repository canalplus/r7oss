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

#include "ring_generic.h"
#include "encode_stream.h"
#include "transporter_tsmux.h"
#include "transporter_memsrc.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeStream_c"

#define MEDIA_STR(x)   (((x) == STM_SE_ENCODE_STREAM_MEDIA_VIDEO) ? "video" : ((x) == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) ? "audio" : "any")

EncodeStream_c::EncodeStream_c(stm_se_encode_stream_media_t media)
    : AudioEncodeNo()
    , mMedia(media)
    , Lock()
    , SyncProcess()
    , Next(NULL)
    , BufferManager(NULL)
    , mInputRing(NULL)
    , PreprocOutputRing(NULL)
    , CoderOutputRing(NULL)
    , Statistics()
    , VideoEncodeMemoryProfile()
    , VideoEncodeInputColorFormatForecasted()
    , CodedMemorySize(0)
    , CodedFrameMaximumSize(0)
    , CodedMemoryPartitionName()
    , Terminating(false)
    , ProcessRunningCount(0)
    , StartStopLock()
    , StartStopEvent()
    , UnEncodable(false)
    , PreprocFrameBufferType(0)
    , PreprocFrameAllocType(0)
    , CodedFrameBufferType(0)
    , BufferInputBufferType(0)
    , InputMetaDataBufferType(0)
    , EncodeCoordinatorMetaDataBufferType(0)
    , MetaDataSequenceNumberType(0)
    , BufferEncoderControlStructureType(0)
    , MarkerInPreprocFrameIndex(INVALID_INDEX)
    , MarkerInCodedFrameIndex(INVALID_INDEX)
    , NextBufferSequenceNumber(0)
    , DiscardingUntilMarkerFramePtoC(false)
    , DiscardingUntilMarkerFrameCtoO(false)
    , mPortConnected(false)
    , mReleaseBufferCallBack(NULL)
    , IsLowPowerState(false)
    , LowPowerEnterEvent()
    , LowPowerExitEvent()
    , AccumulatedBeforePtoCControlMessages()
    , AccumulatedAfterPtoCControlMessages()
    , AccumulatedBeforeCtoOControlMessages()
    , AccumulatedAfterCtoOControlMessages()
{
    SE_VERBOSE(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    OS_InitializeMutex(&Lock);
    OS_InitializeMutex(&SyncProcess);
    OS_InitializeMutex(&StartStopLock);

    OS_InitializeEvent(&StartStopEvent);
    OS_InitializeEvent(&LowPowerEnterEvent);
    OS_InitializeEvent(&LowPowerExitEvent);
}

EncodeStream_c::~EncodeStream_c()
{
    SE_VERBOSE(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // StopStream() already performed previously in TerminateEncodeStream()
    // and in case of error conditions in AddStream()
    // so no need to perform a StopStream() here (would lead to Bug23237)
    // Terminates are called in the reverse order from Initialize for Symmetry.
    OS_TerminateEvent(&LowPowerExitEvent);
    OS_TerminateEvent(&LowPowerEnterEvent);
    OS_TerminateEvent(&StartStopEvent);

    OS_TerminateMutex(&StartStopLock);
    OS_TerminateMutex(&SyncProcess);
    OS_TerminateMutex(&Lock);
}

EncoderStatus_t EncodeStream_c::GetEncoder(Encoder_t *Encoder)
{
    *Encoder = this->Encoder.Encoder;
    return EncoderNoError;
}

EncoderStatus_t   EncodeStream_c::Input(Buffer_t    Buffer)
{
    SE_VERBOSE(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    // We pass this to the PreProc stage ...
    OS_LockMutex(&SyncProcess);
    EncoderStatus_t Status;
    Status = Encoder.Preproc->Input(Buffer);
    if (Status != EncoderNoError)
    {
        SE_ERROR("Failed to Input data on the stream, Status = %u\n", Status);
    }
    OS_UnLockMutex(&SyncProcess);

    SE_VERBOSE(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return Status;
}

EncoderStatus_t   EncodeStream_c::Flush(void)
{
    EncoderStatus_t Status = EncoderNoError;

    SE_INFO(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Nothing more to do if no ring
    if (mInputRing == NULL)
    {
        SE_INFO(group_encoder_stream, "%s 0x%p: Nothing to do, ring does not exist\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    unsigned int FramesPurged = 0;
    while (mInputRing->NonEmpty())
    {
        Buffer_t  Buffer;
        mInputRing->Extract((uintptr_t *) &Buffer);

        if (Buffer != NULL)
        {
            ReleaseInputBuffer(Buffer);
            FramesPurged++;
        }
    }
    SE_DEBUG(group_encoder_stream, "%s 0x%p: Purged %u Frames\n", MEDIA_STR(mMedia), this, FramesPurged);

    // In NRT mode, we also need to release remaining buffers still stored in EncodeCoordinator
    if (Encoder.Encode->IsModeNRT() == true)
    {
        Status = Encoder.EncodeCoordinator->Flush(this);
        if (Status != EncoderNoError)
        {
            SE_ERROR("%s 0x%p: Failed to flush EncodeCoordinator\n", MEDIA_STR(mMedia), this);
        }
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::GetStatistics(encode_stream_statistics_t *ExposedStatistics)
{
    SE_VERBOSE(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);
    SE_ASSERT(sizeof(encode_stream_statistics_t) == sizeof(EncodeStreamStatistics_t));

    // Return a *copy* of the statistics.
    *(EncodeStreamStatistics_t *)ExposedStatistics = Statistics;
    return EncoderNoError;
}

EncoderStatus_t   EncodeStream_c::ResetStatistics(void)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);
    memset(&(Statistics), 0, sizeof(EncodeStreamStatistics_t));
    return EncoderNoError;
}

//
// Stream Lifetime Support
//

EncoderStatus_t   EncodeStream_c::StartStream(void)
{
    EncoderStatus_t Status;
    OS_Thread_t     Thread;
    unsigned int    CurrentProcessRunningCount;
    unsigned int    ProcessCreatedCount;

    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    int ThreadIdItoP;
    int ThreadIdPtoC;
    int ThreadIdCtoO;

    switch (mMedia)
    {
    case STM_SE_ENCODE_STREAM_MEDIA_AUDIO:
        ThreadIdItoP = SE_TASK_ENCOD_AUDITOP;
        ThreadIdPtoC = SE_TASK_ENCOD_AUDPTOC;
        ThreadIdCtoO = SE_TASK_ENCOD_AUDCTOO;
        break;
    case STM_SE_ENCODE_STREAM_MEDIA_VIDEO:
        ThreadIdItoP = SE_TASK_ENCOD_VIDITOP;
        ThreadIdPtoC = SE_TASK_ENCOD_VIDPTOC;
        ThreadIdCtoO = SE_TASK_ENCOD_VIDCTOO;
        break;
    case STM_SE_ENCODE_STREAM_MEDIA_ANY:
    default:
        SE_ERROR("0x%p: invalid / non supported media type %d (%s)\n", this, mMedia, MEDIA_STR(mMedia));
        return EncoderError;
    }

    mInputRing = new RingGeneric_c(ENCODER_MAX_INPUT_BUFFERS, "EncodeStream_c::InputRing");
    if ((mInputRing == NULL) || (mInputRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("%s 0x%p: Unable to create input ring\n", MEDIA_STR(mMedia), this);
        StopStream();
        return EncoderNoMemory;
    }

    PreprocOutputRing = new RingGeneric_c(ENCODER_STREAM_MAX_PREPROC_BUFFERS + ENCODER_MAX_CONTROL_STRUCTURE_BUFFERS,
                                          "EncodeStream_c::PreprocOutputRing");
    if ((PreprocOutputRing == NULL) || (PreprocOutputRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("%s 0x%p: Unable to create preprocessed frame ring\n", MEDIA_STR(mMedia), this);
        StopStream();
        return EncoderNoMemory;
    }

    Encoder.Preproc->RegisterOutputBufferRing(PreprocOutputRing);
    CoderOutputRing   = new RingGeneric_c(ENCODER_STREAM_MAX_CODED_BUFFERS + ENCODER_MAX_CONTROL_STRUCTURE_BUFFERS,
                                          "EncodeStream_c::CoderOutputRing");
    if ((CoderOutputRing == NULL) || (CoderOutputRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("%s 0x%p: Unable to create coded frame ring\n", MEDIA_STR(mMedia), this);
        StopStream();
        return EncoderNoMemory;
    }

    Encoder.Coder->RegisterOutputBufferRing(CoderOutputRing);
    Status = Encoder.Coder->InitializeCoder();
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Unable to initialize the encoder hardware\n", MEDIA_STR(mMedia), this);
        StopStream();
        return Status;
    }

    // Create threads
    OS_ResetEvent(&StartStopEvent);
    ProcessCreatedCount = 0;

    if (OS_CreateThread(&Thread, EncoderProcessInputToPreprocessor, Encoder.EncodeStream, &player_tasks_desc[ThreadIdItoP]) == OS_NO_ERROR)
    {
        ProcessCreatedCount ++;
        if (OS_CreateThread(&Thread, EncoderProcessPreprocessorToCoder, Encoder.EncodeStream, &player_tasks_desc[ThreadIdPtoC]) == OS_NO_ERROR)
        {
            ProcessCreatedCount ++;
            if (OS_CreateThread(&Thread, EncoderProcessCoderToOutput, Encoder.EncodeStream, &player_tasks_desc[ThreadIdCtoO]) == OS_NO_ERROR)
            {
                ProcessCreatedCount ++;
            }
        }
    }

    // Wait for all threads to be running
    CurrentProcessRunningCount = 0;
    while (CurrentProcessRunningCount != ProcessCreatedCount)
    {
        OS_Status_t WaitStatus = OS_WaitForEventAuto(&StartStopEvent, ENCODE_STREAM_MAX_EVENT_WAIT);

        OS_LockMutex(&StartStopLock);
        OS_ResetEvent(&StartStopEvent);
        CurrentProcessRunningCount = ProcessRunningCount;
        OS_UnLockMutex(&StartStopLock);

        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("%s 0x%p: Still waiting for Stream processes to run - ProcessRunningCount %d (expected %d)\n",
                       MEDIA_STR(mMedia), this, CurrentProcessRunningCount, ProcessCreatedCount);
        }
    }

    // Terminate all threads if one of the creation had failed
    if (ProcessCreatedCount != ENCODE_STREAM_PROCESS_NB)
    {
        SE_ERROR("%s 0x%p: Failed to create all stream processes\n", MEDIA_STR(mMedia), this);
        StopStream();
        return EncoderError;
    }

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return EncoderNoError;
}

EncoderStatus_t   EncodeStream_c::StopStream(void)
{
    unsigned int            OwnerIdentifier;
    Buffer_t                Buffer;
    unsigned int            CurrentProcessRunningCount;

    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Set Discard Flags
    DiscardingUntilMarkerFramePtoC = true;
    DiscardingUntilMarkerFrameCtoO = true;

    // Stop Components
    if (Encoder.Preproc)
    {
        Encoder.Preproc->Halt();
    }

    if (Encoder.Coder)
    {
        Encoder.Coder->Halt();
        Encoder.Coder->TerminateCoder();
    }

    if (Encoder.Transporter)
    {
        Encoder.Transporter->Halt();
    }

    // Ask threads to terminate
    OS_LockMutex(&StartStopLock);
    OS_ResetEvent(&StartStopEvent);
    CurrentProcessRunningCount = ProcessRunningCount;
    OS_UnLockMutex(&StartStopLock);

    // Write memory barrier: wmb_for_EncodeStream_Terminating coupled with: rmb_for_EncodeStream_Terminating
    OS_Smp_Mb();
    Terminating = true;

    if (PreprocOutputRing != NULL)
    {
        PreprocOutputRing->Insert((uintptr_t)NULL);
    }

    if (CoderOutputRing != NULL)
    {
        CoderOutputRing->Insert((uintptr_t)NULL);
    }

    // Wait for all threads termination
    while (CurrentProcessRunningCount != 0)
    {
        OS_Status_t WaitStatus = OS_WaitForEventAuto(&StartStopEvent, 2 * ENCODE_STREAM_MAX_EVENT_WAIT);

        OS_LockMutex(&StartStopLock);
        OS_ResetEvent(&StartStopEvent);
        CurrentProcessRunningCount = ProcessRunningCount;
        OS_UnLockMutex(&StartStopLock);

        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("%s 0x%p: Still waiting for stream processes to terminate - ProcessRunningCount %d\n",
                       MEDIA_STR(mMedia), this, CurrentProcessRunningCount);
        }
    }

    //
    // Strip the rings - NOTE we assume we are the first owner on any buffer
    //
    if (mInputRing != NULL)
    {
        mInputRing->Flush();
        delete mInputRing;
        mInputRing = NULL;
    }

    if (PreprocOutputRing != NULL)
    {
        while (PreprocOutputRing->NonEmpty())
        {
            PreprocOutputRing->Extract((uintptr_t *)(&Buffer));

            if (Buffer != NULL)
            {
                Buffer->GetOwnerList(1, &OwnerIdentifier);
                Buffer->DecrementReferenceCount(OwnerIdentifier);
            }
        }

        delete PreprocOutputRing;
        PreprocOutputRing = NULL;
    }

    if (CoderOutputRing != NULL)
    {
        while (CoderOutputRing->NonEmpty())
        {
            CoderOutputRing->Extract((uintptr_t *)(&Buffer));

            if (Buffer != NULL)
            {
                Buffer->GetOwnerList(1, &OwnerIdentifier);
                Buffer->DecrementReferenceCount(OwnerIdentifier);
            }
        }

        delete CoderOutputRing;
        CoderOutputRing = NULL;
    }

    // Extract from Encode list
    // TODO: Manage the Encode List Lifetimes.

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return EncoderNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      A function to allow components to mark a stream as UnEncodable
//

EncoderStatus_t   EncodeStream_c::MarkStreamUnEncodable(void)
{
    SE_INFO(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);
    //
    // TODO: We don't yet have support for policies in the Encoder
    // Check to see if a policy has been set to ignore this call.
    //
    //
    // Mark the stream as UnEncodable
    //
    UnEncodable     = true;
    //
    // Raise the event to signal this to the user
    //
    SignalEvent(STM_SE_ENCODE_STREAM_EVENT_FATAL_ERROR);
    //
    // Report the Error
    //
    SE_ERROR("%s Stream 0x%p marked as un-encodable\n", MEDIA_STR(mMedia), this);
    // But the call has completed successfully
    return EncoderNoError;
}

void   EncodeStream_c::GetClassList(Encode_t         *Encode,
                                    Preproc_t        *Preproc,
                                    Coder_t          *Coder,
                                    Transporter_t        *Transporter,
                                    EncodeCoordinator_t  *EncodeCoordinator)
{
    if (Encode)
    {
        *Encode = Encoder.Encode;
    }

    if (Preproc)
    {
        *Preproc = Encoder.Preproc;
    }

    if (Coder)
    {
        *Coder = Encoder.Coder;
    }

    if (Transporter)
    {
        *Transporter = Encoder.Transporter;
    }

    if (EncodeCoordinator)
    {
        *EncodeCoordinator = Encoder.EncodeCoordinator;
    }
}

EncoderStatus_t   EncodeStream_c::ManageMemoryProfile()
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    EncoderStatus_t Status = Encoder.Preproc->ManageMemoryProfile();
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Manage Memory Profile for the Encode Preprocessor\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    Status = Encoder.Coder->ManageMemoryProfile();
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Manage Memory Profile for the Encode Coder\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::RegisterBufferManager(BufferManager_t BufferManager)
{
    EncoderStatus_t Status = EncoderNoError;
    const EncoderBufferTypes *BufferTypes = Encoder.Encoder->GetBufferTypes();
    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    if (this->BufferManager != NULL)
    {
        SE_ERROR("%s 0x%p: Buffer manager already registered\n", MEDIA_STR(mMedia), this);
        return EncoderError;
    }
    this->BufferManager = BufferManager;

    // Register the Buffer Types locally
    PreprocFrameBufferType              = BufferTypes->PreprocFrameBufferType;
    PreprocFrameAllocType               = BufferTypes->PreprocFrameAllocType;
    CodedFrameBufferType                = BufferTypes->CodedFrameBufferType;
    BufferInputBufferType               = BufferTypes->BufferInputBufferType;
    InputMetaDataBufferType             = BufferTypes->InputMetaDataBufferType;
    EncodeCoordinatorMetaDataBufferType = BufferTypes->EncodeCoordinatorMetaDataBufferType;
    MetaDataSequenceNumberType          = BufferTypes->MetaDataSequenceNumberType;

    // Initialize the working classes
    Status = Encoder.Preproc->RegisterBufferManager(BufferManager);
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Register Buffer Manager with the Preprocessor\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    Status = Encoder.Coder->RegisterBufferManager(BufferManager);
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Register Buffer Manager with the Coder\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    Status = Encoder.Transporter->RegisterBufferManager(BufferManager);
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Register Buffer Manager with the Transporter\n", MEDIA_STR(mMedia), this);
        return Status;
    }

    return Status;
}

Transporter_t EncodeStream_c::FindTransporter(stm_object_h  Sink)
{
    EncoderStatus_t       Status;
    Transporter_t         Transporter = NULL;
    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Probe each transporter to find one that can support this Sink Object
    // First match wins...

    // The Transport Engine TS Mux Device
    Status = Transporter_TSMux_c::Probe(Sink);
    if (Status == EncoderNoError)
    {
        SE_INFO(group_encoder_stream, "%s 0x%p: Constructing a Transporter_TSMux_c for Sink 0x%p\n", MEDIA_STR(mMedia), this, Sink);
        Transporter = new Transporter_TSMux_c;
        if ((Transporter == NULL) || (Transporter->InitializationStatus != TransporterNoError))
        {
            SE_ERROR("%s 0x%p: Transporter_TSMux_c failed to initialize for Sink 0x%p\n", MEDIA_STR(mMedia), this, Sink);
            delete Transporter;
            Transporter = NULL;
        }
        return Transporter;
    }

    // (The Source to) A MemSink Pull interface
    Status = Transporter_MemSrc_c::Probe(Sink);
    if (Status == EncoderNoError)
    {
        SE_INFO(group_encoder_stream, "%s 0x%p: Constructing a Transporter_MemSrc_c for Sink 0x%p\n", MEDIA_STR(mMedia), this, Sink);
        Transporter = new Transporter_MemSrc_c;
        if ((Transporter == NULL) || (Transporter->InitializationStatus != TransporterNoError))
        {
            SE_ERROR("%s 0x%p: Transporter_MemSrc_c failed to initialize for Sink 0x%p\n", MEDIA_STR(mMedia), this, Sink);
            delete Transporter;
            Transporter = NULL;
        }
        return Transporter;
    }

    SE_ERROR("%s 0x%p: No transporter found for Sink 0x%p\n", MEDIA_STR(mMedia), this, Sink);
    return NULL;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Try to establish connection to sink port
//
EncoderStatus_t   EncodeStream_c::AddTransport(stm_object_h  sink)
{
    EncoderStatus_t       Status;
    Transporter_t         Transporter;
    Transporter_t         OldTransporter;

    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // We need to find a Transporter capable of communicating with this object
    Transporter = FindTransporter(sink);
    if (Transporter == NULL)
    {
        return EncoderError;    // Failed to find/create a new transporter
    }

    // The New Transporter needs all of its registrations early
    Transporter->RegisterEncoder(Encoder.Encoder, Encoder.Encode, this, Encoder.Preproc, Encoder.Coder, Transporter);

    // Bring up the new Transporter
    Status = Transporter->RegisterBufferManager(BufferManager);
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to Register Buffer Manager with the Transporter\n", MEDIA_STR(mMedia), this);
        delete Transporter; // We failed to register the Buffer Manager - so we won't use this Transporter
        return Status;
    }

    // Tear down the OldTransporter
    OldTransporter = Encoder.Transporter;
    OldTransporter->Halt();

    // Register the new Transporter with all of the Stream Components.
    // The Encoder and Encode do not have a specific Transporter so we do not register with them.
    this /* Stream */->RegisterEncoder(NULL, NULL, NULL, NULL, NULL, Transporter);
    Encoder.Preproc  ->RegisterEncoder(NULL, NULL, NULL, NULL, NULL, Transporter);
    Encoder.Coder    ->RegisterEncoder(NULL, NULL, NULL, NULL, NULL, Transporter);
    delete OldTransporter;

    // Finally - connect the New Transporter to the Sink object
    return Encoder.Transporter->CreateConnection(sink);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//    Try to remove connection with specified sink port
//
EncoderStatus_t   EncodeStream_c::RemoveTransport(stm_object_h  sink)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // We do not need to replace the current transporter with a NULL Transporter
    // as upon RemoveConnection it will only discard output.
    // If we are re-connected to a new output - a New transporter will be
    // created by the Add Transport call
    return Encoder.Transporter->RemoveConnection(sink);
}

//
// Managing Controls
//

EncoderStatus_t   EncodeStream_c::DelegateGetControl(stm_se_ctrl_t   Control,
                                                     void           *Data)
{
    EncoderStatus_t Status;

    // Try control on all components.
    // If return value is EncoderControlNotMatch, try next one.
    // If return value is EncoderUnsupportedControl, return error.
    Status = Encoder.Preproc->GetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Coder->GetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Transporter->GetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    // Control is not known by these components.
    SE_ERROR("%s 0x%p: Not match control %d (%s)\n", MEDIA_STR(mMedia), this, Control, StringifyControl(Control));
    return EncoderControlNotMatch;
}

EncoderStatus_t   EncodeStream_c::DelegateGetCompoundControl(stm_se_ctrl_t   Control,
                                                             void           *Data)
{
    EncoderStatus_t Status;

    // Try compound control on all components.
    // If return value is EncoderControlNotMatch, try next one.
    // If return value is EncoderUnsupportedControl, return error.
    Status = Encoder.Preproc->GetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Coder->GetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Transporter->GetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    // Compound control is not known by these components.
    SE_ERROR("%s 0x%p: Not match compound control %d (%s)\n", MEDIA_STR(mMedia), this, Control, StringifyControl(Control));
    return EncoderControlNotMatch;
}

EncoderStatus_t   EncodeStream_c::DelegateSetControl(stm_se_ctrl_t   Control,
                                                     const void     *Data)
{
    EncoderStatus_t Status;

    // Try control on all components.
    // If return value is EncoderControlNotMatch, try next one.
    // If return value is EncoderUnsupportedControl, return error.
    Status = Encoder.Preproc->SetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Coder->SetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Transporter->SetControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    // Control is not known by these components.
    SE_ERROR("%s 0x%p: Not match control %d (%s)\n", MEDIA_STR(mMedia), this, Control, StringifyControl(Control));
    return EncoderControlNotMatch;
}

EncoderStatus_t   EncodeStream_c::DelegateSetCompoundControl(stm_se_ctrl_t   Control,
                                                             const void     *Data)
{
    EncoderStatus_t Status;

    // Try compound control on all components.
    // If return value is EncoderControlNotMatch, try next one.
    // If return value is EncoderUnsupportedControl, return error.
    Status = Encoder.Preproc->SetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Coder->SetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    Status = Encoder.Transporter->SetCompoundControl(Control, Data);

    if ((Status == EncoderNoError) || (Status == EncoderUnsupportedControl))
    {
        return Status;
    }

    // Compound control is not known by these components.
    SE_ERROR("%s 0x%p: Not match compound control %d (%s)\n", MEDIA_STR(mMedia), this, Control, StringifyControl(Control));
    return EncoderControlNotMatch;
}

EncoderStatus_t   EncodeStream_c::GetControl(stm_se_ctrl_t   Control,
                                             void           *Data)
{
    EncoderStatus_t Status = EncoderNoError;

    switch (Control)
    {
    // Handle any Stream Specific controls
    default:
        // Working Classes also have the opportunity to provide control data.
        Status = DelegateGetControl(Control, Data);
        break;
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::GetCompoundControl(stm_se_ctrl_t   Control,
                                                     void           *Data)
{
    EncoderStatus_t Status = EncoderNoError;

    switch (Control)
    {
    // Handle any Stream Specific controls
    default:
        // Working Classes also have the opportunity to provide control data.
        Status = DelegateGetCompoundControl(Control, Data);
        break;
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::SetControl(stm_se_ctrl_t   Control,
                                             const void     *Data)
{
    EncoderStatus_t Status = EncoderNoError;

    switch (Control)
    {
    // Handle any Stream Specific controls
    default:
        // Working Classes also have the opportunity to set control data.
        Status = DelegateSetControl(Control, Data);
        break;
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::SetCompoundControl(stm_se_ctrl_t   Control,
                                                     const void     *Data)
{
    EncoderStatus_t Status = EncoderNoError;

    switch (Control)
    {
    // Handle any Stream Specific controls
    default:
        // Working Classes also have the opportunity to set control data.
        Status = DelegateSetCompoundControl(Control, Data);
        break;
    }

    return Status;
}

EncoderStatus_t   EncodeStream_c::InjectDiscontinuity(stm_se_discontinuity_t Discontinuity)
{
    EncoderStatus_t Status;

    SE_INFO(group_encoder_stream, "%s 0x%p\n", MEDIA_STR(mMedia), this);

    // We pass this to the PreProc stage
    OS_LockMutex(&SyncProcess);
    Status = Encoder.Preproc->InjectDiscontinuity(Discontinuity);
    if (Status != EncoderNoError)
    {
        SE_ERROR("%s 0x%p: Failed to inject discontinuity 0x%x (status %d)\n", MEDIA_STR(mMedia), this, Discontinuity, Status);
    }
    OS_UnLockMutex(&SyncProcess);

    return Status;
}

EncoderStatus_t   EncodeStream_c::SignalEvent(stm_se_encode_stream_event_t   Event)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p: Event %d\n", MEDIA_STR(mMedia), this, Event);

    if (Event == STM_SE_ENCODE_STREAM_EVENT_INVALID)
    {
        SE_ERROR("%s 0x%p: Invalid event\n", MEDIA_STR(mMedia), this);
        return EncoderError;
    }

    int32_t err;
    stm_event_t StreamEvent; // can this be a local variable?
    StreamEvent.event_id = (uint32_t)Event;
    StreamEvent.object   = (stm_object_h)Encoder.EncodeStream;

    OS_LockMutex(&Lock);
    err = stm_event_signal(&StreamEvent);
    if (err != 0)
    {
        OS_UnLockMutex(&Lock);
        SE_ERROR("%s 0x%p: Failed to Signal Event %d (%d)\n", MEDIA_STR(mMedia), this, Event, err);
        return EncoderError;
    }
    OS_UnLockMutex(&Lock);

    return EncoderNoError;
}

//
// Encode streams linked list management
//

EncodeStream_t    EncodeStream_c::GetNext(void)
{
    return Next;
}

void    EncodeStream_c::SetNext(EncodeStream_t Stream)
{
    Next = Stream;
}

VideoEncodeMemoryProfile_t    EncodeStream_c::GetVideoEncodeMemoryProfile(void)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p: %u\n", MEDIA_STR(mMedia), this, VideoEncodeMemoryProfile);
    return VideoEncodeMemoryProfile;
}

void    EncodeStream_c::SetVideoEncodeMemoryProfile(VideoEncodeMemoryProfile_t MemoryProfile)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p: %u\n", MEDIA_STR(mMedia), this, MemoryProfile);
    VideoEncodeMemoryProfile = MemoryProfile;
}

surface_format_t    EncodeStream_c::GetVideoInputColorFormatForecasted(void)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p: %u\n", MEDIA_STR(mMedia), this, VideoEncodeInputColorFormatForecasted);
    return VideoEncodeInputColorFormatForecasted;
}

void    EncodeStream_c::SetVideoInputColorFormatForecasted(surface_format_t ForecastedFormat)
{
    SE_DEBUG(group_encoder_stream, "%s 0x%p: %u\n", MEDIA_STR(mMedia), this, ForecastedFormat);
    VideoEncodeInputColorFormatForecasted = ForecastedFormat;
}

//
// Low power functions
//

EncoderStatus_t   EncodeStream_c::LowPowerEnter(void)
{
    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Reset events used for putting PreprocessorToCoder thread into "low power"
    OS_ResetEvent(&LowPowerEnterEvent);
    OS_ResetEvent(&LowPowerExitEvent);
    // Save low power state
    IsLowPowerState = true;

    // Wait for PreprocessorToCoder thread to be in safe state (no more MME commands issued)
    OS_Status_t WaitStatus = OS_WaitForEventInterruptible(&LowPowerEnterEvent, OS_INFINITE);
    if (WaitStatus == OS_INTERRUPTED)
    {
        SE_INFO(group_encoder_stream, "wait for LP enter interrupted; LowPowerEnterEvent:%d\n", LowPowerEnterEvent.Valid);
    }

    // Ask preproc and coder to enter low power state
    // For CPS mode, MME transformers will be terminated now...
    if (Encoder.Preproc != NULL)
    {
        Encoder.Preproc->LowPowerEnter();
    }

    if (Encoder.Coder != NULL)
    {
        Encoder.Coder->LowPowerEnter();
    }

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return EncoderNoError;
}

EncoderStatus_t   EncodeStream_c::LowPowerExit(void)
{
    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Ask preproc and coder to exit low power state
    // For CPS mode, MME transformers will be re-initialized now...
    if (Encoder.Coder != NULL)
    {
        Encoder.Coder->LowPowerExit();
    }

    if (Encoder.Preproc != NULL)
    {
        Encoder.Preproc->LowPowerExit();
    }

    // Reset low power state
    IsLowPowerState = false;
    // Wake-up PreprocessorToCoder thread
    OS_SetEventInterruptible(&LowPowerExitEvent);

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return EncoderNoError;
}

//{{{  EncodeStream_c::Disconnect
//{{{  doxynote
/// \brief                      Allow to disconnect EncodeStrean from source in tunneled mode.
/// \return                     EncoderError   : EncodeStream not connected in tunneled mode
///                             EncoderNoError : EncodeStream is disconnected from source in tunneled mode
//}}}
EncoderStatus_t     EncodeStream_c::Disconnect()
{
    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    // Calling Preproc->Halt() and Coder->Halt() is a WA that simply aborts any calls to prevent us being blocked.
    // It must be done before locking the SyncProcess mutex that may be already locked the Input() method.

    // Calling Halt() effectively stops the Coder and Preproc from
    // being locked in requesting new *buffers* and prevents them from
    // requesting new ones, preventing further deadlocks.
    // Calling Halt() does not prevent the threads from waiting for *Contexts*,
    // for HW/st231 deferred commands to complete before existing:
    // this is required to exit clean.

    // R.G.: Current WA might have some issues with video due to different behavior
    // Audio and video stream have not the same behaviour at closure:
    // - audio waits for commands to complete in detach (inside Halt())
    // - video waits for commands to complete in delete (inside TerminateCoder())
    // That means that after detach() call, we might still get some command callbacks on video stream.

    PreprocStatus_t PStatus;
    PStatus = Encoder.Preproc->Halt();
    if (PreprocNoError != PStatus)
    {
        SE_ERROR("%s 0x%p: Preproc->Halt() error %08X\n", MEDIA_STR(mMedia), this, PStatus);
        //continue
    }

    CoderStatus_t   CStatus;
    CStatus = Encoder.Coder->Halt();
    if (CoderNoError != CStatus)
    {
        SE_ERROR("%s 0x%p: Coder->Halt() error %08X\n", MEDIA_STR(mMedia), this, CStatus);
        //continue
    }

    // make sure this encode stream is connected
    OS_LockMutex(&SyncProcess);

    if (mPortConnected == false)
    {
        SE_ERROR("%s 0x%p: Connection doesn't exist for this EncodeStream\n", MEDIA_STR(mMedia), this);
        OS_UnLockMutex(&SyncProcess);
        return EncoderError;
    }

    // Flush remaining buffers
    Flush();

    mPortConnected         = false;
    mReleaseBufferCallBack = NULL;

    OS_UnLockMutex(&SyncProcess);

    // In NRT mode, disconnect EncodeCoordinator
    if (Encoder.Encode->IsModeNRT() == true)
    {
        if (Encoder.EncodeCoordinator != NULL)
        {
            Encoder.EncodeCoordinator->Disconnect(this);
        }
    }

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return EncoderNoError;
}
//}}}

//{{{  EncodeStream_c::Connect
//{{{  doxynote
/// \brief                      Allow to connect a source to this EncodeStrean in tunneled mode and to set the Release callback.
/// \param                      src_object        : Source to be connected with. This handle should be provided
///                                                 when inserted EncodeInputBuffer will be released
/// \param                      release_callback  : Callback to execute to released all EncodeInputBuffer
/// \param                      inputRing         : EncodeStream input port
/// \return                     EncoderError   : EncodeStream already connected to a source in tunneled mode
///                             EncoderNoError : EncodeStream is connected to a source in tunneled mode
//}}}
EncoderStatus_t     EncodeStream_c::Connect(ReleaseBufferInterface_c *ReleaseBufferItf, Port_c **InputPort)
{
    SE_INFO(group_encoder_stream, ">%s 0x%p\n", MEDIA_STR(mMedia), this);

    //
    // make sure this encode stream is not yet connected
    //
    OS_LockMutex(&SyncProcess);

    if (mPortConnected == true)
    {
        SE_ERROR("%s 0x%p: Connection already exists for this EncodeStream\n", MEDIA_STR(mMedia), this);
        OS_UnLockMutex(&SyncProcess);
        return EncoderError;
    }

    // In NRT mode, we substitute EncodeStream connection by EncodeCoordinator connection
    // The input port and the release buffer interfaces are those from EncodeCoordinator
    if (Encoder.Encode->IsModeNRT() == true)
    {
        // NRT mode: connect to EncodeCoordinator
        EncoderStatus_t Status;
        ReleaseBufferInterface_c *EncodeCoordinatorReleaseBufferItf;
        Port_c                   *EncodeCoordinatorInputPort;

        Status = Encoder.EncodeCoordinator->Connect(this, ReleaseBufferItf, &EncodeCoordinatorReleaseBufferItf, &EncodeCoordinatorInputPort);
        if (Status != EncoderNoError)
        {
            SE_ERROR("%s 0x%p: Failed to connect to EncodeCoordinator\n", MEDIA_STR(mMedia), this);
            OS_UnLockMutex(&SyncProcess);
            return EncoderError;
        }

        mReleaseBufferCallBack = EncodeCoordinatorReleaseBufferItf;
        *InputPort             = EncodeCoordinatorInputPort;
    }
    else
    {
        // Real-time mode (legacy)
        // We connect directly to the EncodeStream input port, EncodeCoordinator is not used
        mReleaseBufferCallBack = ReleaseBufferItf;
        *InputPort             = mInputRing;
    }

    mPortConnected = true;
    OS_UnLockMutex(&SyncProcess);

    SE_DEBUG(group_encoder_stream, "<%s 0x%p\n", MEDIA_STR(mMedia), this);
    return  EncoderNoError;
}
//}}}

//{{{  EncodeStream_c::ReleaseInputBuffer
//{{{  doxynote
/// \brief                      Allow to release a previously pushed EncodeInputBuffer.
/// \param                      Buffer         : Encode input buffer to release
/// \return                     EncoderError   : no call back available or release refused
///                             EncoderNoError : buffer is released
//}}}
EncoderStatus_t     EncodeStream_c::ReleaseInputBuffer(Buffer_t  Buffer)
{
    SE_VERBOSE(group_encoder_stream, "%s 0x%p - Buffer 0x%p\n", MEDIA_STR(mMedia), this, Buffer);

    if (mReleaseBufferCallBack)
    {
        if (mReleaseBufferCallBack->ReleaseBuffer(Buffer) != PlayerNoError)
        {
            SE_ERROR("%s 0x%p: Release buffer callback failure - Buffer 0x%p\n", MEDIA_STR(mMedia), this, Buffer);
            return EncoderError;
        }
    }
    else
    {
        SE_ERROR("%s 0x%p: No release buffer callback available - Buffer 0x%p\n", MEDIA_STR(mMedia), this, Buffer);
        return EncoderError;
    }

    return EncoderNoError;
}
//}}}

