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

#include "transporter_tsmux.h"

#undef TRACE_TAG
#define TRACE_TAG "Transporter_TSMux_c"


int transporter_tsmux_release_buffer(stm_object_h         src_object,
                                     struct stm_data_block *block_list,
                                     uint32_t               block_count)
{
    Transporter_TSMux_c *Transporter_TSMux = (Transporter_TSMux_c *)src_object;
    return Transporter_TSMux->ReleaseBuffer(block_list, block_count);
}

stm_te_async_data_interface_src_t TSMuxAsyncDataInterface =
{
    transporter_tsmux_release_buffer,
};

//{{{  Constructor
//{{{  doxynote
/// \brief                      Set Initial state if success
Transporter_TSMux_c::Transporter_TSMux_c(void)
    : Transporter_Base_c()
    , TSMuxHandle(NULL)
    , TSMuxBufferRing()
    , TSMuxInterface()
{
    // TODO(pht) move to a FinalizeInit method
    TSMuxBufferRing = new RingGeneric_c(TRANSPORTER_STREAM_MAX_CODED_BUFFERS,
                                        "Transporter_TSMux_c::TSMuxBufferRing");
    if (TSMuxBufferRing == NULL)
    {
        SE_ERROR("TSMuxBufferRing alloc failed\n");
        InitializationStatus = TransporterError;
        return;
    }

    SetComponentState(ComponentRunning);
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Try to disconnect if not done
Transporter_TSMux_c::~Transporter_TSMux_c(void)
{
    Halt();
}
//}}}

TransporterStatus_t Transporter_TSMux_c::Halt(void)
{
    Transporter_Base_c::Halt();

    RemoveConnection(TSMuxHandle);

    //
    // Purge anything left on the Ring
    //
    if (TSMuxBufferRing)
    {
        PurgeBufferRing();
        delete TSMuxBufferRing;
        TSMuxBufferRing = NULL;
    }

    return TransporterNoError;
}

//{{{  Probe
//{{{  doxynote
/// \brief                      Determine if this Class can connect to the Sink Object
/// \param Sink                 SinkHandle : The object to connect to
/// \return                     Success or fail
TransporterStatus_t Transporter_TSMux_c::Probe(stm_object_h  Sink)
{
    int           Status;
    stm_object_h  SinkType;
    char          TagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       Size;
    stm_te_async_data_interface_sink_t            Interface;

    SE_DEBUG(group_encoder_transporter, "\n");

    // Check if the sink object supports the STM_TE_ASYNC_DATA_INTERFACE interface
    Status = stm_registry_get_object_type(Sink, &SinkType);
    if (Status != 0)
    {
        SE_ERROR("stm_registry_get_object_type(%p, &%p) failed (%d)\n", Sink, SinkType, Status);
        return TransporterError;
    }

    Status = stm_registry_get_attribute(SinkType,
                                        STM_TE_ASYNC_DATA_INTERFACE,
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

//{{{  CreateConnection
//{{{  doxynote
/// \brief                      Connect to a TSMux Object for direct tunnelled Multiplexing
/// \param Sink                 Handle : sink object to connect to
/// \return                     Success or fail
TransporterStatus_t Transporter_TSMux_c::CreateConnection(stm_object_h  Sink)
{
    AssertComponentState(ComponentRunning);
    SE_INFO(group_encoder_transporter, "\n");
    int           Status;
    stm_object_h  SinkType;
    char          TagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       Size;

    if (TSMuxHandle)
    {
        SE_ERROR("TSMux already connected on %p, refusing connection on %p\n", TSMuxHandle, Sink);
        return TransporterError;
    }

    // Check sink object support STM_TE_ASYNC_DATA_INTERFACE interface
    Status = stm_registry_get_object_type(Sink, &SinkType);
    if (Status != 0)
    {
        SE_ERROR("stm_registry_get_object_type(%p, &%p) failed (%d)\n", Sink, SinkType, Status);
        return TransporterError;
    }

    Status = stm_registry_get_attribute(SinkType,
                                        STM_TE_ASYNC_DATA_INTERFACE,
                                        TagTypeName,
                                        sizeof(TSMuxInterface),
                                        &TSMuxInterface,
                                        (int *)&Size);
    if ((Status) || (Size != sizeof(TSMuxInterface)))
    {
        // no SE_ERROR since function used as test-check feature
        SE_DEBUG(group_encoder_transporter, "stm_registry_get_attribute() failed (%d)\n", Status);
        return TransporterError;
    }

    // call the sink interface's connect handler to connect the consumer */
    SE_DEBUG(group_encoder_transporter, "Connecting to the TSMuxInterface\n");
    Status = TSMuxInterface.connect((stm_object_h)this,    // This class is the handle for the Source
                                    Sink,
                                    &TSMuxAsyncDataInterface);
    if (Status)
    {
        // Connection fails : Free OS objects
        SE_ERROR("TSMuxInterface.connect(%p) failed; status:%d\n", Sink, Status);
        return TransporterError;
    }

    SE_DEBUG(group_encoder_transporter, "Successful connection to TSMuxInterface\n");

    // Save sink object for communication with the TSMux
    TSMuxHandle = Sink;
    return TransporterNoError;
}
//}}}

//{{{  RemoveConnection
//{{{  doxynote
/// \brief                      Disconnect from TSMux Sink port (if connected)
/// \return                     Success or fail
TransporterStatus_t Transporter_TSMux_c::RemoveConnection(stm_object_h  Sink)
{
    SE_DEBUG(group_encoder_transporter, "\n");

    // Check that Sink object correspond the connected one
    if (TSMuxHandle != Sink)
    {
        SE_ERROR("Sink (%p) does not match TSMuxHandle (%p)\n", Sink, TSMuxHandle);
        return TransporterError;
    }

    if (TSMuxHandle != 0)
    {
        // Reset our Sink Handle to prevent it being re-used
        TSMuxHandle = NULL;

        // call the sink interface's disconnect handler to disconnect the consumer
        SE_DEBUG(group_encoder_transporter, "Disconnecting from TSMux Sink\n");
        int Status = TSMuxInterface.disconnect((stm_object_h)this, Sink);
        if (Status)
        {
            SE_ERROR("TSMuxInterface.disconnect(%p) failed\n", Sink);
            // We have removed our local handle - and we are actively in the process of shutdown.
            // We simply continue at this point (and quite likely purge our BufferRing)
        }
        else
        {
            SE_DEBUG(group_encoder_transporter, "Successfully disconnected from TSMux Sink\n");
        }

        // On Disconnect - TSMux will release all of its buffers.
        // If anything is left, there is an error and it needs to be purged.
        PurgeBufferRing();
    }

    return TransporterNoError;
}
//}}}

//{{{  Input
//{{{  doxynote
/// \brief                      Transfer a buffer into the Mux
/// \return                     Success or fail
TransporterStatus_t Transporter_TSMux_c::Input(Buffer_t   Buffer)
{
    TransporterStatus_t                 TransporterStatus;
    // Retrieving Metadata
    __stm_se_frame_metadata_t          *OutputMetaDataDescriptor;
    stm_se_compressed_frame_metadata_t *CompressedFrameMetaData;
    // Retrieving Buffer data
    void                               *FrameVirtualAddress;
    uint32_t                            FrameSize;
    struct stm_data_block               BlockList;
    uint32_t                            DataBlocks;

    AssertComponentState(ComponentRunning);

    if (!TSMuxHandle)
    {
        // Buffer can be returned immediately
        SE_DEBUG(group_encoder_transporter, "No connection established to TSMux\n");
        return TransporterNoError;
    }

    SE_VERBOSE(group_encoder_transporter, "\n");

    // Call the Base Class implementation for common processing
    TransporterStatus = Transporter_Base_c::Input(Buffer);
    if (TransporterStatus != TransporterNoError)
    {
        SE_ERROR("Error returned by base class Input method\n");
        return TransporterStatus;
    }

    // Get the MetaData reference needed by TSMux interface
    Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&OutputMetaDataDescriptor));
    SE_ASSERT(OutputMetaDataDescriptor != NULL);

    CompressedFrameMetaData = &OutputMetaDataDescriptor->compressed_frame_metadata;

    // Fill in the Block List
    Buffer->ObtainDataReference(NULL, &FrameSize, &FrameVirtualAddress,  CachedAddress);
    if (FrameVirtualAddress == NULL)
    {
        SE_ERROR("Unable to retrieve buffer address\n");
        return TransporterError;
    }

    // Hold a reference to this buffer while it is being used by the TSMux.
    // We will decrement this value when the TXMux calls ReleaseBuffer
    Buffer->IncrementReferenceCount(IdentifierEncoderTransporter);

    RingStatus_t RingStatus = TSMuxBufferRing->Insert((uintptr_t) Buffer);
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Failed to insert the buffer onto the local ring rs:%d", RingStatus);
        // Abandon the reference that we were going to use to keep the Buffer on the Ring.
        Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
        return TransporterError;
    }
    TraceRing("Input: INSERT", Buffer);

    SE_VERBOSE(group_encoder_transporter, "buffer:%p framesize:%d\n", Buffer, FrameSize);
    BlockList.data_addr = FrameVirtualAddress;
    BlockList.len       = FrameSize;
    BlockList.next      = NULL;
    int Status = TSMuxInterface.queue_buffer(
                     TSMuxHandle,
                     CompressedFrameMetaData,
                     &BlockList,
                     1,
                     &DataBlocks);
    if (Status)
    {
        Buffer_t BufferOut;
        RingStatus = TSMuxBufferRing->ExtractLastInserted((uintptr_t *)(&BufferOut), RING_NONE_BLOCKING);
        if (RingStatus == RingNoError)
        {
            TraceRing("Input: EXTRACT LAST", BufferOut);
        }

        SE_ERROR("Failed to queue buffer:%p with TSMux:%p (status:%d - %d)\n", Buffer, TSMuxHandle, Status, RingStatus);
        Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
        Encoder.EncodeStream->EncodeStreamStatistics().TsMuxQueueError++;
        return TransporterError;
    }

    Encoder.EncodeStream->EncodeStreamStatistics().BufferCountFromTransporter++;
    if (FrameSize == 0)
    {
        Encoder.EncodeStream->EncodeStreamStatistics().NullSizeBufferCountFromTransporter++;
    }

    return TransporterNoError;
}
//}}}

//{{{  ReleaseBuffer
//{{{  doxynote
/// \brief                      The callback from the TSMux to indicate a buffer is free to be reused
/// \return                     0 Success or 1 fail (stm_te status compatible)
int Transporter_TSMux_c::ReleaseBuffer(struct stm_data_block *block_list,
                                       uint32_t               block_count)
{
    Buffer_t                           Buffer;
    void                              *FrameVirtualAddress;
    uint32_t                           FrameSize;
    RingStatus_t RingStatus = TSMuxBufferRing->Extract((uintptr_t *)(&Buffer), RING_NONE_BLOCKING);
    if (RingStatus == RingNoError)
    {
        TraceRing("ReleaseBuffer: EXTRACT", Buffer);
        if (Buffer != NULL)
        {
            // Verify that we are releasing the correct buffer...
            Buffer->ObtainDataReference(NULL, &FrameSize, &FrameVirtualAddress,  CachedAddress);
            if (FrameVirtualAddress == NULL)
            {
                SE_ERROR("Unable to retrieve buffer address\n");
                Encoder.EncodeStream->EncodeStreamStatistics().TsMuxTransporterBufferAddressError++;
                return 1;
            }

            if ((FrameVirtualAddress != block_list->data_addr) || (FrameSize != block_list->len))
            {
                SE_ERROR("buffer for release does not match one requested\n"
                         " Callback provided: %p (%d)\n"
                         " Our Ring provided: %p (%d)\n",
                         block_list->data_addr, block_list->len,
                         FrameVirtualAddress, FrameSize);
                Encoder.EncodeStream->EncodeStreamStatistics().TsMuxTransporterUnexpectedReleasedBufferError++;
            }
            SE_VERBOSE(group_encoder_transporter, "buffer:%p framesize:%d\n", Buffer, FrameSize);

            // Regardless of whether this is the correct buffer - we must now release...
            // As it has been removed from the Ring
            Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
            Encoder.EncodeStream->EncodeStreamStatistics().ReleaseBufferCountFromTsMuxTransporter++;
            return 0;
        }
    }

    SE_ERROR("Failed to match a buffer for release rs:%d\n", RingStatus);
    Encoder.EncodeStream->EncodeStreamStatistics().TsMuxTransporterRingExtractError++;
    return 1;
}
//}}}

//{{{  PurgeBufferRing
//{{{  doxynote
/// \brief                      This private internal function is used to ensure that
///                             our TSMuxBufferRing is completely emptied.
/// \return                     Success or fail
TransporterStatus_t Transporter_TSMux_c::PurgeBufferRing(void)
{
    Buffer_t Buffer;

    while (TSMuxBufferRing->NonEmpty())
    {
        RingStatus_t RingStatus = TSMuxBufferRing->Extract((uintptr_t *)(&Buffer), RING_NONE_BLOCKING);

        if (RingStatus == RingNoError)
        {
            TraceRing("PurgeBufferRing: EXTRACT", Buffer);
            if (Buffer != NULL)
            {
                // release buffer
                SE_WARNING("purging remaining buffer: %p\n", Buffer);
                Buffer->Dump(DumpBufferStates);
                Buffer->DecrementReferenceCount(IdentifierEncoderTransporter);
            }
        }
    }

    return TransporterNoError;
}
//}}}

// Debug traces helper function
void Transporter_TSMux_c::TraceRing(const char *ActionStr, Buffer_t Buffer)
{
    if (Buffer == NULL)
    {
        SE_DEBUG(group_encoder_transporter, "%s NULL => count %u\n", ActionStr, TSMuxBufferRing->NbOfEntries());
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
                     ActionStr, Buffer, BufferSize, MetaDataDescriptor->compressed_frame_metadata.discontinuity, TSMuxBufferRing->NbOfEntries());
        }
        else
        {
            SE_VERBOSE(group_encoder_transporter, "%s 0x%p size %u discont 0x%x => count %u\n",
                       ActionStr, Buffer, BufferSize, MetaDataDescriptor->compressed_frame_metadata.discontinuity, TSMuxBufferRing->NbOfEntries());
        }
    }
}
