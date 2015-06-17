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

#include "transporter_memsrc.h"

#undef TRACE_TAG
#define TRACE_TAG "Transporter_MemSrc_c"


int pull_encode_source_se(stm_object_h src_object,
                          struct stm_data_block *block_list,
                          uint32_t block_count,
                          uint32_t *filled_blocks)
{
    Transporter_MemSrc_c *Transporter_MemSrc = (Transporter_MemSrc_c *)src_object;

    // nothing read so far
    *filled_blocks = 0;

    SE_VERBOSE(group_encoder_transporter, "input len: %d\n", block_list->len);

    // make sure provided buffer is large enough for captureBuffer
    if (block_list->len < sizeof(stm_se_capture_buffer_t))
    {
        SE_ERROR("bad input len: %d\n", block_list->len);
        return -EINVAL;
    }

    //
    // Check if something to read and if connection still opened
    // In this case retCode is >= 0
    // if nothing to read or internal error retcode is < 0
    // After this call CurrentBuffer is set with next buffer to manifest
    //
    int32_t retCode = Transporter_MemSrc->PullFrameWait();
    if (retCode < 0)
    {
        SE_DEBUG(group_encoder_transporter, "return after wait - retcode:%d\n", retCode);
        return retCode;
    }

    // Read Frame from CurrentBuffer
    int32_t copiedBytes = Transporter_MemSrc->PullFrameRead((uint8_t *)block_list->data_addr, block_list->len);

    // Release current frame buffer
    Transporter_MemSrc->PullFramePostRead();

    // We fill in the block_list based on the success of PullFrameRead
    if (copiedBytes >= 0)
    {
        block_list->len = copiedBytes;
        *filled_blocks = 1;
    }

    // return total copied bytes, or error status of PullFrameRead
    return copiedBytes;
}

int pull_encode_test_for_source_se(stm_object_h src_object, uint32_t *size)
{
    SE_VERBOSE(group_encoder_transporter, "checking for data\n");

    Transporter_MemSrc_c *Transporter_MemSrc = (Transporter_MemSrc_c *)src_object;
    // PullFrameAvailable will return the frame size which can be zero (skip frame)
    // if nothing to read or internal error retcode is < 0
    int32_t data_size = Transporter_MemSrc->PullFrameAvailable();
    *size = data_size;
    return (data_size >= 0 ? 0 : data_size);
}

struct stm_data_interface_pull_src se_manifestor_pull_encode_interface =
{
    pull_encode_source_se,
    pull_encode_test_for_source_se
};

//{{{  Constructor
//{{{  doxynote
/// \brief                      Set Initial state if success
Transporter_MemSrc_c::Transporter_MemSrc_c(void)
    : Transporter_Base_c()
    , CurrentBuffer(NULL)
    , CompressedBufferRing(NULL)
    , FramePhysicalAddress(NULL)
    , FrameVirtualAddress(NULL)
    , FrameSize(0)
    , Connected(false)
    , PullSinkInterface()
    , SinkHandle(NULL)
{
    // TODO(pht) move to a FinalizeInit method
    CompressedBufferRing = new RingGeneric_c(TRANSPORTER_STREAM_MAX_CODED_BUFFERS,
                                             "Transporter_MemSrc_c::CompressedBufferRing");
    if (CompressedBufferRing == NULL)
    {
        SE_ERROR("CompressedBufferRing alloc failed\n");
        InitializationStatus = TransporterError;
        return;
    }

    SetComponentState(ComponentRunning);
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Try to disconnect if not done
Transporter_MemSrc_c::~Transporter_MemSrc_c(void)
{
    Halt();
}
//}}}

//{{{
TransporterStatus_t Transporter_MemSrc_c::Halt(void)
{
    Transporter_Base_c::Halt();

    RemoveConnection(SinkHandle);

    delete CompressedBufferRing;
    CompressedBufferRing = NULL;

    return TransporterNoError;
}
//}}}

//{{{  Probe
//{{{  doxynote
/// \brief                      Determine if this Class can connect to the Sink Object
/// \param Sink                 SinkHandle : The object to connect to
/// \return                     Success or fail
TransporterStatus_t Transporter_MemSrc_c::Probe(stm_object_h  Sink)
{
    int           Status;
    stm_object_h  SinkType;
    char          TagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       Size;
    struct stm_data_interface_pull_sink           Interface;
    SE_DEBUG(group_encoder_transporter, "\n");
    //
    // Check if the sink object supports the STM_TE_ASYNC_DATA_INTERFACE interface
    //
    Status = stm_registry_get_object_type(Sink, &SinkType);
    if (Status != 0)
    {
        SE_ERROR("stm_registry_get_object_type(%p, &%p) failed (%d)\n", Sink, SinkType, Status);
        return TransporterError;
    }

    Status = stm_registry_get_attribute(Sink,
                                        STM_DATA_INTERFACE_PULL,
                                        TagTypeName,
                                        sizeof(Interface),
                                        &Interface,
                                        (int *)&Size);
    if ((Status) || (Size != sizeof(Interface)))
    {
        // no SE_ERROR since function used as test-check feature
        SE_DEBUG(group_encoder_transporter, "stm_registry_get_attribute() failed (%d)\n", Status);
        return TransporterError;
    }

    return TransporterNoError; // We can connect to this object
}
//}}}

//{{{  Input
//{{{  doxynote
/// \brief                      Queue new encoded buffer to be provided to the sink port
/// \param Buffer               buffer
/// \return                     Success or fail
//}}}
TransporterStatus_t  Transporter_MemSrc_c::Input(Buffer_t Buffer)
{
    AssertComponentState(ComponentRunning);

    // No action if not connected yet
    if (!Connected)
    {
        // Buffer can be returned immediately
        SE_DEBUG(group_encoder_transporter, "No connection established\n");
        return TransporterNoError;
    }

    SE_VERBOSE(group_encoder_transporter, "\n");

    // Call the Base Class implementation for common processing
    TransporterStatus_t TransporterStatus = Transporter_Base_c::Input(Buffer);
    if (TransporterStatus != TransporterNoError)
    {
        SE_ERROR("Error returned by base class Input method\n");
        return TransporterStatus;
    }

    // Queue buffer for buffer memsink pull
    Buffer->IncrementReferenceCount(IdentifierEncoderTransporter);

    RingStatus_t RingStatus = CompressedBufferRing->Insert((unsigned int) Buffer);
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Too many queued buffers rs:%d\n", RingStatus);
        Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
        return TransporterError;
    }
    TraceRing("Input: INSERT", Buffer);

    // Signal memsink that a new frame is available
    if (PullSinkInterface.notify)
    {
        int retval = PullSinkInterface.notify(SinkHandle,
                                              STM_MEMSINK_EVENT_DATA_AVAILABLE);
        if (retval != 0)
        {
            // Error can happen in case of the connection is closed.
            // Here we already tested that connection was opened but could be closed meantime
            SE_ERROR("Error in PullSinkInterface.notify(%p) ret = %d\n", SinkHandle, retval);
            Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
            return TransporterError;
        }
    }
    else
    {
        // Notification interface not available (shall not happen)
        SE_ERROR("PullSinkInterface.notify(%p) not available\n", SinkHandle);
        Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
        return TransporterError;
    }

    return TransporterNoError;
}

//{{{  CreateConnection
//{{{  doxynote
/// \brief                      Connect to pull interface (memsink only) or push interface
/// \param Buffer               SinkHandle : sink object to connnect to
/// \return                     Success or fail
TransporterStatus_t Transporter_MemSrc_c::CreateConnection(stm_object_h  SinkHandle)
{
    SE_INFO(group_encoder_transporter, "\n");
    int           retval;
    stm_object_h  sinkType;
    char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       returnedSize;

    if (Connected)
    {
        return TransporterError;
    }

    //
    // Check sink object support STM_DATA_INTERFACE_PULL interface
    //
    retval = stm_registry_get_object_type(SinkHandle, &sinkType);
    if (retval)
    {
        SE_ERROR("stm_registry_get_object_type(%p) failed (%d)\n", SinkHandle, retval);
        return TransporterError;
    }

    retval = stm_registry_get_attribute(SinkHandle,
                                        STM_DATA_INTERFACE_PULL,
                                        tagTypeName,
                                        sizeof(PullSinkInterface),
                                        &PullSinkInterface,
                                        (int *)&returnedSize);
    if ((retval) || (returnedSize != sizeof(PullSinkInterface)))
    {
        SE_ERROR("stm_registry_get_attribute() failed (%d)\n", retval);
        return TransporterError;
    }

    //
    // call the sink interface's connect handler to connect the consumer */
    //
    retval = PullSinkInterface.connect((stm_object_h)this       , // SRC
                                       SinkHandle,                // SINK
                                       (struct stm_data_interface_pull_src *) &se_manifestor_pull_encode_interface);
    if (retval)
    {
        // Connection fails : Free OS objects
        SE_ERROR("PullSinkInterface.connect(%p) failed; status:%d\n", SinkHandle, retval);
        return TransporterError;
    }

    SE_DEBUG(group_encoder_transporter, "Successful connection to memsink\n");
    //
    // Save sink object for disconnection
    //
    this->SinkHandle = SinkHandle;
    //
    // Initialize connection
    //
    InitialiseConnection();
    Connected = true;
    return TransporterNoError;
}
//}}}

//{{{  RemoveConnection
//{{{  doxynote
/// \brief                      Disconnect from memory Sink port (if connected)
///                             Release pending pull call if any before disconnection
/// \return                     Success or fail
TransporterStatus_t Transporter_MemSrc_c::RemoveConnection(stm_object_h  SinkHandle)
{
    SE_DEBUG(group_encoder_transporter, "\n");

    //
    // Check that Sink object correspond the connected one
    //
    if (this->SinkHandle != SinkHandle)
    {
        SE_ERROR("Sink handle %p != %p does not match\n", this->SinkHandle, SinkHandle);
        return TransporterError;
    }

    if (Connected == true)
    {
        // close connection for user
        Connected = false;
        // Memory Barrier following Flag Setting.
        OS_Smp_Mb();
        //
        // Insert dummy buffer to unlock pending pull call
        //
        (void)CompressedBufferRing->Insert(NULL);
        TraceRing("RemoveConnection: INSERT", NULL);
        //
        // Release pending pull call, if any
        //
        TerminateConnection();
        // reset old sink object
        this->SinkHandle = NULL;
        //
        // call the sink interface's disconnect handler to disconnect the consumer
        //
        SE_DEBUG(group_encoder_transporter, "disconnect from memorySink\n");
        int retval = PullSinkInterface.disconnect((stm_object_h)this, SinkHandle);

        if (retval)
        {
            SE_ERROR("PullSinkInterface.disconnect(%p) failed\n", SinkHandle);
        }
        else
        {
            SE_DEBUG(group_encoder_transporter, "Successful disconnection from memsink\n");
        }
    }

    return TransporterNoError;
}
//}}}

///{{{ PrepareEncodeBuffer
/// \brief                     Prepare EncodeBuffer to be returned to memsink_pull
/// \param encodeMetadata      Address where to copy output metadata
/// \return                    Success or fail

TransporterStatus_t Transporter_MemSrc_c::PrepareEncodeBuffer(stm_se_compressed_frame_metadata_t *encodeMetadata)
{
    if (OutputMetaDataBufferType == NULL)
    {
        SE_ERROR("OutputMetaDataBufferType not assigned yet\n");
        return TransporterError;
    }

    // Get Output Metatdata
    __stm_se_frame_metadata_t *OutputMetaDataDescriptor;
    CurrentBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&OutputMetaDataDescriptor));
    SE_ASSERT(OutputMetaDataDescriptor != NULL);

    *encodeMetadata = OutputMetaDataDescriptor->compressed_frame_metadata;

    // get buffer addresses and length
    CurrentBuffer->ObtainDataReference(NULL, NULL, &FramePhysicalAddress, PhysicalAddress);
    if (FramePhysicalAddress == NULL)
    {
        SE_ERROR("Cannot retrieve buffer phys address\n");
        return TransporterError;
    }

    CurrentBuffer->ObtainDataReference(NULL, &FrameSize, &FrameVirtualAddress,  CachedAddress);
    if (FrameVirtualAddress == NULL)
    {
        SE_ERROR("Cannot retrieve buffer address\n");
        return TransporterError;
    }

    return TransporterNoError;
}
//}}}


///{{{ PullFrameAvailable
/// \brief                      Check if a frame is available and return the size of the data
///                             available for the next pull
/// \return                     Size of data available (buffer payload)
///                              -EPERM if not connected
///                              -EAGAIN if no frame available
///                              -EINVAL if internal error
int32_t Transporter_MemSrc_c::PullFrameAvailable()
{
    BufferStatus_t   BufferStatus;
    stm_se_compressed_frame_metadata_t compressed_frame_metadata;

    if (!Connected)
    {
        return -EPERM;
    }

    // Check if the current encode buffer have been already extracted
    if (CurrentBuffer == NULL)
    {
        if (CompressedBufferRing->NonEmpty() == false)
        {
            return -EAGAIN;
        }

        // We can extract the current encode buffer from the ring
        // as the ring is not empty
        RingStatus_t RingStatus = CompressedBufferRing->Extract((uintptr_t *)(&CurrentBuffer), PLAYER_MAX_EVENT_WAIT);
        if (RingStatus != RingNoError)
        {
            SE_ERROR("Unexpected empty ring\n");
            return -EINVAL;
        }
        TraceRing("PullFrameAvailable: EXTRACT", CurrentBuffer);
    }

    // Null pointer can append only after connect state move to false
    if (CurrentBuffer == NULL)
    {
        if (Connected == true)
        {
            SE_ERROR("Unexpected invalid compressed buffer ref in not connected state\n");
        }

        return -EINVAL;
    }

    // Retrieve actual payload
    BufferStatus = PrepareEncodeBuffer((stm_se_compressed_frame_metadata_t *)&compressed_frame_metadata);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to prepare encoded buffer\n");
        return -EINVAL;
    }

    // returned number of bytes available to pull
    SE_VERBOSE(group_encoder_transporter, "FrameSize:%d\n", FrameSize);
    return FrameSize;
}
//}}}


///{{{ PullFrameWait
/// \brief                      Try to get a frame. Depending connection mode
///                             will wait for a new frame or disconnection.
///                             The framesize will be returned in metadata via provided buffer
/// \return                     if a frame is available  : Size of the compressed buffer provided
///                             if no frame is available : -EAGAIN
///                             Linux kernel error otherwise (negative value)
int32_t     Transporter_MemSrc_c::PullFrameWait()
{
    RingStatus_t RingStatus;

    if (!Connected)
    {
        return -EPERM;
    }

    if (CurrentBuffer != NULL)
    {
        // Buffer previously extracted by PullFrameAvailable()
        return FrameSize;
    }

    // check if user asks for waiting mode
    if ((PullSinkInterface.mode & STM_IOMODE_NON_BLOCKING_IO) == STM_IOMODE_NON_BLOCKING_IO)
    {
        // in case of no buffer available return immediately
        if (CompressedBufferRing->NonEmpty() == false)
        {
            return -EAGAIN;
        }
    }

    int count = 0;
    do
    {
        SE_EXTRAVERB(group_encoder_transporter, "%d\n", count++);
        RingStatus = CompressedBufferRing->Extract((uintptr_t *)(&CurrentBuffer), PLAYER_MAX_EVENT_WAIT);
        if (RingStatus == RingNoError)
        {
            TraceRing("PullFrameWait: EXTRACT", CurrentBuffer);
        }

        // Check if connection was closed while waiting for frame
        if (!Connected)
        {
            return -EPERM;
        }

        // The nature of this call means that userspace can be blocked here
        // waiting for an action. This loop must have an escape route for
        // signals such as when ctrl-c (SIGINT) / kill -9 (SIGKILL) are sent.
        //
        if (OS_SignalPending())
        {
            return -EAGAIN;
        }
    }
    while (RingStatus == RingNothingToGet);

    // Null pointer can append only after connect state move to false
    if (CurrentBuffer == NULL)
    {
        SE_ERROR("Unexpected invalid compressed buffer ref in not connected state\n");
        return -ECONNRESET;
    }

    return sizeof(stm_se_capture_buffer_t);
}
//}}

///{{{ PullFramePostRead
/// \brief                      Release Frame Buffer as provided to memsink
/// \return                     Nothing
void  Transporter_MemSrc_c::PullFramePostRead(void)
{
    // release previous CurrentBuffer if exists
    if (CurrentBuffer)
    {
        CurrentBuffer->DecrementReferenceCount(IdentifierEncoderTransporter);
    }

    CurrentBuffer = NULL;
}
//}}}

///{{{ PullFrameRead
/// \brief                      Set memsink structure fields and copy the compressed frame into payload buffer
/// \param MemsinkBufferAddr    Kernel Address of buffer provided by memsink
/// \param MemsinkBufferLen     length of buffer provided by memsink
/// \return                     Number of bytes copied, (positive or null value)
///                             Linux kernel error otherwise (negative value)

int32_t Transporter_MemSrc_c::PullFrameRead(uint8_t *MemsinkBufferAddr, uint32_t MemsinkBufferLen)
{
    stm_se_capture_buffer_t *memsinkBuffer = (stm_se_capture_buffer_t *)MemsinkBufferAddr;

    // perform audio/video preparation
    BufferStatus_t BufferStatus = PrepareEncodeBuffer((stm_se_compressed_frame_metadata_t *)&memsinkBuffer->u);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to prepare encoded buffer\n");
        return -EINVAL;
    }

    // Compressed frames into provided buffer
    // First check buffer large enough
    if (memsinkBuffer->buffer_length < FrameSize)
    {
        SE_ERROR("Memsink compressed buffer too small: expected at least %d got %d bytes (%d)\n",
                 FrameSize, memsinkBuffer->buffer_length, MemsinkBufferLen);
        return -EINVAL;
    }

    SE_VERBOSE(group_encoder_transporter, "FrameSize:%d\n", FrameSize);
    // Perform a software copy (no mandatory HW accelerator here)
    memcpy((void *)memsinkBuffer->virtual_address,
           (void *)FrameVirtualAddress,
           FrameSize);

    // Update the real copied frame size
    memsinkBuffer->payload_length = FrameSize;

    //Statistics update
    Encoder.EncodeStream->EncodeStreamStatistics().BufferCountFromTransporter++;
    if (FrameSize == 0)
    {
        Encoder.EncodeStream->EncodeStreamStatistics().NullSizeBufferCountFromTransporter++;
    }

    return FrameSize;
}
//}}}

///{{{ InitialiseConnection
/// \brief                      Reset CurrentBuffer and Waiting List (Ring)
/// \return                     Always Success (for now)
TransporterStatus_t  Transporter_MemSrc_c::InitialiseConnection(void)
{
    // perform general initialisation
    SE_DEBUG(group_encoder_transporter, "FLUSH => %u\n", CompressedBufferRing->NbOfEntries());
    CompressedBufferRing->Flush();
    return TransporterNoError;
}
//}}}

///{{{ TerminateConnection
/// \brief                      Release all frames of the waiting list
/// \return                     Success or fail
TransporterStatus_t  Transporter_MemSrc_c::TerminateConnection(void)
{
    // perform general termination
    if (CurrentBuffer != NULL)
    {
        // release current buffer
        CurrentBuffer->DecrementReferenceCount(IdentifierEncoderTransporter);
        CurrentBuffer = NULL;
    }

    // purge waiting ring
    while (CompressedBufferRing->NonEmpty())
    {
        RingStatus_t RingStatus = CompressedBufferRing->Extract((uintptr_t *)(&CurrentBuffer), RING_NONE_BLOCKING);
        if (RingStatus == RingNoError)
        {
            TraceRing("TerminateConnection: EXTRACT", CurrentBuffer);
            if (CurrentBuffer != NULL)
            {
                // release buffer
                SE_WARNING("purging remaining buffer: %p\n", CurrentBuffer);
                CurrentBuffer->DecrementReferenceCount(IdentifierEncoderTransporter);
                CurrentBuffer = NULL;
            }
        }
    }

    return TransporterNoError;
}
//}}}

// Debug traces helper function
void Transporter_MemSrc_c::TraceRing(const char *ActionStr, Buffer_t Buffer)
{
    if (Buffer == NULL)
    {
        SE_DEBUG(group_encoder_transporter, "%s NULL => count %u\n", ActionStr, CompressedBufferRing->NbOfEntries());
    }
    else
    {
        __stm_se_frame_metadata_t *MetaDataDescriptor;
        Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&MetaDataDescriptor));
        SE_ASSERT(MetaDataDescriptor != NULL);

        unsigned int BufferSize;
        Buffer->ObtainDataReference(NULL, &BufferSize, NULL);

        if ((BufferSize == 0) || (MetaDataDescriptor->compressed_frame_metadata.discontinuity & STM_SE_DISCONTINUITY_EOS))
        {
            SE_DEBUG(group_encoder_transporter, "%s EOS 0x%p size %u discont 0x%x => count %u\n",
                     ActionStr, Buffer, BufferSize, MetaDataDescriptor->compressed_frame_metadata.discontinuity, CompressedBufferRing->NbOfEntries());
        }
        else
        {
            SE_VERBOSE(group_encoder_transporter, "%s 0x%p size %u discont 0x%x => count %u\n",
                       ActionStr, Buffer, BufferSize, MetaDataDescriptor->compressed_frame_metadata.discontinuity, CompressedBufferRing->NbOfEntries());
        }
    }
}
