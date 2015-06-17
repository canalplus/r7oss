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

#include "encoder_generic.h"

// Provide Class definitions for construction
#include "encode_coordinator.h"
// Preprocessors
#include "preproc_null.h"
#include "preproc_video_generic.h"
#include "preproc_audio_null.h"
#include "preproc_audio_mme.h"
// Coders
#include "coder_null.h"
#include "coder_video_mme_h264.h"
#include "coder_audio_mme_ddce51.h"
#include "coder_audio_mme_aace.h"
#include "coder_audio_mme_mp3e.h"
// Transporters
// Only the Null Transporter is constructed at initial stream creation
// Alternative Transporters are constructed when attached to the output
#include "transporter_null.h"

#undef TRACE_TAG
#define TRACE_TAG "Encoder_Generic_c"

static BufferDataDescriptor_t     BufferInputBuffer                 = BUFFER_INPUT_TYPE;
static BufferDataDescriptor_t     BufferInputMetaData               = METADATA_ENCODE_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t     BufferEncodeCoordinatorMetaData   = METADATA_ENCODE_COORDINATOR_FRAME_PARAMETERS_TYPE;

static BufferDataDescriptor_t     BufferPreprocFrame                = BUFFER_PREPROC_FRAME_TYPE;
static BufferDataDescriptor_t     BufferPreprocAllocation           = BUFFER_PREPROC_ALLOCATION_TYPE;
static BufferDataDescriptor_t     BufferCodedFrame                  = BUFFER_CODED_FRAME_TYPE;

static BufferDataDescriptor_t     BufferInternalMetaData            = METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS_TYPE;
static BufferDataDescriptor_t     BufferMetaDataSequenceNumber      = ENCODE_METADATA_SEQUENCE_NUMBER_TYPE;

Encoder_Generic_c::Encoder_Generic_c(void)
    : Encoder_c()
    , InitializationStatus(EncoderNoError)
    // Set AudioEncodeNo to 0 so that all even audio encodes run on companion1
    // and all odd audio encodes run on companion0
    , AudioEncodeNo(0)
    , Lock()
    , ListOfEncodes(NULL)
    , BufferManager(NULL)
    , InputBufferPool(NULL)
    , BufferTypes()
    , LowPowerMode(STM_SE_LOW_POWER_MODE_HPS)
{
    SE_VERBOSE(group_encoder_stream, "\n");
    OS_InitializeMutex(&Lock);
}

Encoder_Generic_c::~Encoder_Generic_c(void)
{
    SE_VERBOSE(group_encoder_stream, "\n");
    if (InputBufferPool != NULL)
    {
        InputBufferPool->DetachMetaData(BufferTypes.InputMetaDataBufferType);
        InputBufferPool->DetachMetaData(BufferTypes.EncodeCoordinatorMetaDataBufferType);

        BufferManager->DestroyPool(InputBufferPool);
        InputBufferPool = NULL;
    }

    OS_TerminateMutex(&Lock);
}

//
// Managing the Encoder
//

EncoderStatus_t   Encoder_Generic_c::CreateEncode(Encode_t       *Encode)
{
    // Instantiate new Encode object
    Encode_t NewEncode = new class Encode_c;
    if (NewEncode == NULL)
    {
        SE_ERROR("Unable to create a new encode object\n");
        return EncoderNoMemory;
    }

    // Also instantiate a new EncodeCoordinator object (required for NRT mode)
    EncodeCoordinator_t NewEncodeCoordinator = new class EncodeCoordinator_c;
    if (NewEncodeCoordinator == NULL)
    {
        SE_ERROR("Unable to create a new EncodeCoordinator object\n");
        delete NewEncode;
        return EncoderNoMemory;
    }

    EncoderStatus_t Status = NewEncodeCoordinator->FinalizeInit();
    if (Status != EncoderNoError)
    {
        SE_ERROR("Unable to initialize EncodeCoordinator\n");
        delete NewEncodeCoordinator;
        delete NewEncode;
        return Status;
    }

    // Add new encode instance to linked list
    OS_LockMutex(&Lock);
    NewEncode->SetNext(ListOfEncodes);
    ListOfEncodes = NewEncode;
    OS_UnLockMutex(&Lock);

    // Register this Encoder Instance with the newly created Encode
    NewEncode->RegisterEncoder(this, NewEncode, NULL, NULL, NULL, NULL, NewEncodeCoordinator);

    // Prepare the Encode with a handle to the BufferManager to pass to its Streams
    NewEncode->RegisterBufferManager(BufferManager);

    SE_INFO(group_encoder_stream, "Encode 0x%p\n", Encode);
    *Encode = NewEncode;

    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::TerminateEncode(Encode_t         Encode)
{
    Encode_t         TempEncode;

    SE_VERBOSE(group_encoder_stream, ">Encode 0x%p\n", Encode);

    // Remove encode instance from linked list
    OS_LockMutex(&Lock);
    if (ListOfEncodes == Encode)
    {
        ListOfEncodes = Encode->GetNext();
    }
    else if (ListOfEncodes != NULL)
    {
        TempEncode = ListOfEncodes;

        while (TempEncode->GetNext() != NULL)
        {
            if (TempEncode->GetNext() == Encode)
            {
                TempEncode->SetNext(Encode->GetNext());
                break;
            }

            TempEncode = TempEncode->GetNext();
        }
    }
    OS_UnLockMutex(&Lock);

    // Get EncodeCoordinator before calling Encode Halt
    EncodeCoordinator_t EncodeCoordinator = Encode->GetEncodeCoordinator();

    // Stop and clean the encode objects before destruction
    Encode->Halt();
    EncodeCoordinator->Halt();

    SE_INFO(group_encoder_stream, "<Encode 0x%p\n", Encode);
    delete Encode;
    delete EncodeCoordinator;

    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::CreateEncodeStream(EncodeStream_t           *EncodeStream,
                                                        Encode_t                  Encode,
                                                        stm_se_encode_stream_media_t      Media,
                                                        stm_se_encode_stream_encoding_t   Encoding)
{
    EncoderStatus_t          Status      = EncoderNoError;
    EncodeStream_t           Stream      = NULL;
    Preproc_t                Preproc     = NULL;
    Coder_t                  Coder       = NULL;
    Transporter_t            Transporter = NULL;
    const char              *preprocstr = "";
    const char              *coderstr = "";

    Stream = new class EncodeStream_c(Media);
    if (Stream == NULL)
    {
        SE_ERROR("Unable to create new stream object for Encode 0x%p\n", Encode);
        return EncoderNoMemory;
    }

    if (Stream->InitializationStatus != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Stream failed to Initialise\n", Encode);
        Status = Stream->InitializationStatus;
        delete Stream;
        return Status;
    }

    // Construct the required classes, then Add them to the Encode object.
    Stream->AudioEncodeNo = 0;
    switch (Media)
    {
    case STM_SE_ENCODE_STREAM_MEDIA_AUDIO:
        Preproc = new class Preproc_Audio_Mme_c;
        OS_LockMutex(&Lock);
        AudioEncodeNo = (AudioEncodeNo + 1) % ENCODER_STREAM_AUDIO_MAX_CPU;
        Stream->AudioEncodeNo = AudioEncodeNo;
        OS_UnLockMutex(&Lock);
        preprocstr = "audio";
        break;

    case STM_SE_ENCODE_STREAM_MEDIA_VIDEO:
        Preproc = new class Preproc_Video_Generic_c;
        preprocstr = "video";
        break;

    case STM_SE_ENCODE_STREAM_MEDIA_ANY:
    default:
        Preproc = new class Preproc_Null_c;
        break;
    }

    // An initial NULL Transporter is used until we are connected to an output
    Transporter = new class Transporter_Null_c;

    //

    switch (Encoding)
    {
    // Audio Encoding Types Construction
    case STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3:
        Coder = new class Coder_Audio_Mme_Ddce51_c;
        coderstr = "DDCe51";
        break;

    case STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC:
        Coder = new class Coder_Audio_Mme_Aace_c;
        coderstr = "AACe";
        break;

    case STM_SE_ENCODE_STREAM_ENCODING_AUDIO_MP3:
        Coder = new class Coder_Audio_Mme_Mp3e_c;
        coderstr = "MP3e";
        break;

    case STM_SE_ENCODE_STREAM_ENCODING_AUDIO_DTS:
    case STM_SE_ENCODE_STREAM_ENCODING_AUDIO_NULL:
        Coder = new class Coder_Null_c;
        break;

    // Video Encoding Types
    case STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264:
        Coder = new class Coder_Video_Mme_H264_c;
        coderstr = "h264";
        break;

    case STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL:
        Coder = new class Coder_Null_c;
        break;

    default:
        SE_ERROR("Encode 0x%p: Failed to construct suitable Coder class for type %d\n", Encode, Encoding);
        break;
    }

    SE_INFO(group_encoder_stream, "Encode 0x%p Stream 0x%p %s-%s-%d\n", Encode, Stream, preprocstr, coderstr, Stream->AudioEncodeNo);

    // Calling AddStream will start and commence the stream objects
    // If any of the objects failed to create, AddStream will return failure.
    Status = Encode->AddStream(Stream, Preproc, Coder, Transporter);
    if (EncoderNoError != Status)
    {
        // On Error creating and starting the stream, we are responsible for cleaning up
        delete Stream;
        delete Preproc;
        delete Coder;
        delete Transporter;
        return Status;
    }

    // Successfully created and attached the stream, so we can return its handle.
    *EncodeStream = Stream;
    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::TerminateEncodeStream(Encode_t                  Encode,
                                                           EncodeStream_t            EncodeStream)
{
    EncoderStatus_t          Status      = EncoderNoError;
    Preproc_t                Preproc     = NULL;
    Coder_t                  Coder       = NULL;
    Transporter_t            Transporter = NULL;

    EncodeStream->GetClassList(NULL, &Preproc, &Coder, &Transporter);

    OS_LockMutex(&Lock);

    // ensure that next added stream will take the place of this deleted stream
    if (EncodeStream->AudioEncodeNo != 0)
    {
        AudioEncodeNo  = EncodeStream->AudioEncodeNo - 1;
    }
    else
    {
        AudioEncodeNo  = ENCODER_STREAM_AUDIO_MAX_CPU - 1;
    }

    OS_UnLockMutex(&Lock);

    SE_INFO(group_encoder_stream, "Encode 0x%p Stream 0x%p\n", Encode, EncodeStream);

    // First Remove the stream (which stops the threads and components)
    Status = Encode->RemoveStream(EncodeStream);
    if (Status != EncoderNoError)
    {
        SE_ERROR("Encode 0x%p: Failure in removing Stream 0x%p\n", Encode, EncodeStream);
    }

    // Then remove its classes of which we have created
    delete EncodeStream;
    delete Preproc;
    delete Coder;
    delete Transporter;
    return Status;
}

EncoderStatus_t   Encoder_Generic_c::CallInSequence(EncodeStream_t        Stream,
                                                    EncodeSequenceType_t      Type,
                                                    EncodeSequenceValue_t     Value,
                                                    EncodeComponentFunction_t Fn,
                                                    ...)
{
    return EncoderNoError;
}

//
// Mechanisms for registering global items
//

EncoderStatus_t   Encoder_Generic_c::RegisterBufferManager(BufferManager_t        BufferManager)
{
    EncoderStatus_t          Status      = EncoderNoError;

    SE_VERBOSE(group_encoder_stream, "\n");

    if (this->BufferManager != NULL)
    {
        SE_ERROR("Buffer manager already registered\n");
        return EncoderError;
    }
    this->BufferManager = BufferManager;

    // Create Input buffer pool
    Status  = BufferManager->CreateBufferDataType(&BufferInputBuffer, &BufferTypes.BufferInputBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create input buffer type\n");
        return Status;
    }

    Status  = BufferManager->CreatePool(&InputBufferPool, BufferTypes.BufferInputBufferType, ENCODER_MAX_INPUT_BUFFERS);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to create input buffer pool\n");
        return Status;
    }

    // Attach related metadata buffer types
    Status = BufferManager->CreateBufferDataType(&BufferInputMetaData, &BufferTypes.InputMetaDataBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create input metadata buffer type\n");
        return Status;
    }

    Status = InputBufferPool->AttachMetaData(BufferTypes.InputMetaDataBufferType,
                                             sizeof(stm_se_uncompressed_frame_metadata_t));
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach input metadata to input buffer pool\n");
        return Status;
    }

    Status = BufferManager->CreateBufferDataType(&BufferEncodeCoordinatorMetaData,
                                                 &BufferTypes.EncodeCoordinatorMetaDataBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create encode coordinator buffer type\n");
        return Status;
    }

    Status = InputBufferPool->AttachMetaData(BufferTypes.EncodeCoordinatorMetaDataBufferType,
                                             sizeof(__stm_se_encode_coordinator_metadata_t));
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach encode coordinator metadata to input buffer pool\n");
        return Status;
    }

    // Encode Stream Buffer Type Creation.
    Status = BufferManager->CreateBufferDataType(&BufferPreprocFrame, &BufferTypes.PreprocFrameBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create preproc frame buffer type\n");
        return Status;
    }

    Status = BufferManager->CreateBufferDataType(&BufferPreprocAllocation, &BufferTypes.PreprocFrameAllocType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create preproc allocation buffer type\n");
        return Status;
    }

    Status = BufferManager->CreateBufferDataType(&BufferCodedFrame, &BufferTypes.CodedFrameBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create coded frame buffer type\n");
        return Status;
    }

    // MetaData Buffer types
    Status = BufferManager->CreateBufferDataType(&BufferInternalMetaData, &BufferTypes.InternalMetaDataBufferType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create internal metadata buffer type\n");
        return Status;
    }

    Status = BufferManager->CreateBufferDataType(&BufferMetaDataSequenceNumber, &BufferTypes.MetaDataSequenceNumberType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create sequence number metadata buffer type\n");
        return Status;
    }

    return Status;
}

const EncoderBufferTypes *Encoder_Generic_c::GetBufferTypes(void)
{
    return &BufferTypes;
}

EncoderStatus_t   Encoder_Generic_c::RegisterScaler(Scaler_t          Scaler)
{
    SE_VERBOSE(group_encoder_stream, "\n");
    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::RegisterBlitter(Blitter_t        Blitter)
{
    SE_VERBOSE(group_encoder_stream, "\n");
    return EncoderNoError;
}

//
// Support functions for the child classes
//

EncoderStatus_t   Encoder_Generic_c::GetBufferManager(BufferManager_t        *BufferManager)
{
    *BufferManager = this->BufferManager;
    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::GetClassList(EncodeStream_t          Stream,
                                                  Encode_t         *Encode,
                                                  Preproc_t        *Preproc,
                                                  Coder_t          *Coder,
                                                  Transporter_t        *Transporter,
                                                  EncodeCoordinator_t  *EncodeCoordinator)
{
    Stream->GetClassList(Encode, Preproc, Coder, Transporter, EncodeCoordinator);
    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::GetInputBuffer(Buffer_t *Buffer, bool NonBlocking)
{
    *Buffer = NULL;
    BufferStatus_t BufferStatus = InputBufferPool->GetBuffer(Buffer, IdentifierGetInjectBuffer, UNSPECIFIED_SIZE, NonBlocking);
    return (BufferStatus == BufferNoError) ? EncoderNoError : EncoderError;
}

EncoderStatus_t   Encoder_Generic_c::InputData(EncodeStream_t        Stream,
                                               const void           *DataVirtual,
                                               unsigned long         DataPhysical,
                                               unsigned int          DataSize,
                                               const stm_se_uncompressed_frame_metadata_t *Descriptor)
{
    EncoderStatus_t     Status = EncoderNoError;

    SE_VERBOSE(group_encoder_stream, "Stream 0x%p\n", Stream);

    if ((DataSize != 0) && ((DataVirtual == NULL) || (DataPhysical == 0)))
    {
        SE_ERROR("Stream 0x%p: Bad argument (DataSize %u, DataVirtual 0x%p, DataPhysical 0x%lx)\n", Stream, DataSize, DataVirtual, DataPhysical);
        return EncoderError;
    }

    // First check that we are not in NRT mode
    // This mode is currently not supported in non-tunneled mode
    Encode_t Encode = NULL;
    Stream->GetClassList(&Encode);
    if (Encode->IsModeNRT() == true)
    {
        SE_ERROR("Stream 0x%p: NRT mode is not supported in non-tunneled data path\n", Stream);
        return EncoderError;
    }

    // Obtain a buffer from the input pool to store these data pointers
    Buffer_t Buffer = NULL;
    BufferStatus_t BufferStatus = InputBufferPool->GetBuffer(&Buffer, IdentifierGetInjectBuffer);
    if ((BufferStatus != BufferNoError) || (Buffer == NULL))
    {
        SE_ERROR("Failed to get new encoder input buffer for Stream 0x%p\n", Stream);
        return EncoderError;
    }

    // Register our Pointers into the Buffer_t
    void *InputBufferAddr[3];
    InputBufferAddr[0] = (void *)DataVirtual;
    InputBufferAddr[1] = (void *)DataPhysical;
    InputBufferAddr[2] = NULL;
    Buffer->RegisterDataReference(max(DataSize, 1), InputBufferAddr);
    Buffer->SetUsedDataSize(DataSize);

    // Attach meta data to the coded buffer is done at RegisterBufferManager at the pool level
    stm_se_uncompressed_frame_metadata_t *InputMetaDataDescriptor;
    Buffer->ObtainMetaDataReference(BufferTypes.InputMetaDataBufferType, (void **)(&InputMetaDataDescriptor));
    SE_ASSERT(InputMetaDataDescriptor != NULL);

    // Copy the uncompressed metadata info
    memcpy(InputMetaDataDescriptor, Descriptor, sizeof(stm_se_uncompressed_frame_metadata_t));

    // Send the Buffer Pointers into our Stream
    Status = Stream->Input(Buffer);

    // Once the Stream Input has done its work, we can remove our reference.
    // If the Stream needs to keep this buffer - it can take its own reference.
    Buffer->DecrementReferenceCount(IdentifierGetInjectBuffer);

    return Status;
}

//
// Low power functions
//

EncoderStatus_t   Encoder_Generic_c::LowPowerEnter(__stm_se_low_power_mode_t low_power_mode)
{
    SE_VERBOSE(group_encoder_stream, ">\n");

    // Save the requested low power mode
    LowPowerMode = low_power_mode;

    // Call Encodes method
    OS_LockMutex(&Lock);
    for (Encode_t Encode = ListOfEncodes; Encode != NULL; Encode = Encode->GetNext())
    {
        Encode->LowPowerEnter();
    }
    OS_UnLockMutex(&Lock);

    SE_VERBOSE(group_encoder_stream, "<\n");
    return EncoderNoError;
}

EncoderStatus_t   Encoder_Generic_c::LowPowerExit(void)
{
    SE_VERBOSE(group_encoder_stream, ">\n");

    // Call Encodes method
    OS_LockMutex(&Lock);
    for (Encode_t Encode = ListOfEncodes; Encode != NULL; Encode = Encode->GetNext())
    {
        Encode->LowPowerExit();
    }
    OS_UnLockMutex(&Lock);

    SE_VERBOSE(group_encoder_stream, "<\n");
    return EncoderNoError;
}

__stm_se_low_power_mode_t Encoder_Generic_c::GetLowPowerMode(void)
{
    return LowPowerMode;
}
