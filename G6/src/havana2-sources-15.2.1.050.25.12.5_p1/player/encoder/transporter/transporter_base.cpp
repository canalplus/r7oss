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

#include "transporter_base.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Transporter_Base_c"

Transporter_Base_c::Transporter_Base_c(void)
    : BufferManager(NULL)
    , OutputMetaDataBufferType(0)
    , mRelayInitialized(false)
    , mRelayType(ST_RELAY_OFFSET_SE_START)
    , mRelayIndex(0)
{
}

Transporter_Base_c::~Transporter_Base_c(void)
{
    Halt();
}

TransporterStatus_t Transporter_Base_c::Halt(void)
{
    BaseComponentClass_c::Halt();

    if (mRelayInitialized)
    {
        TerminateRelay();
    }

    return TransporterNoError;
}

TransporterStatus_t Transporter_Base_c::RegisterBufferManager(BufferManager_t       BufferManager)
{
    const EncoderBufferTypes *BufferTypes = Encoder.Encoder->GetBufferTypes();

    if (this->BufferManager != NULL)
    {
        SE_ERROR("Attempt to change buffer manager, not permitted\n");
        return TransporterError;
    }
    this->BufferManager = BufferManager;

    // Output metadata buffer of transporter is a coder metadata output buffer of type
    // METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS_TYPE (__stm_se_frame_metadata_t)
    OutputMetaDataBufferType = BufferTypes->InternalMetaDataBufferType;

    return TransporterNoError;
}

/// This Function should only deal with common aspects regarding Input of a buffer
/// The implementation classes must deal with the actual input.
TransporterStatus_t Transporter_Base_c::Input(Buffer_t    Buffer)
{
    // Get the frame metadata reference
    __stm_se_frame_metadata_t *OutputMetaDataDescriptor;
    Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&OutputMetaDataDescriptor));
    SE_ASSERT(OutputMetaDataDescriptor != NULL);

    Encoder.EncodeStream->EncodeStreamStatistics().BufferCountToTransporter++;

    // Initialize st_relay index at first Input call
    // This cannot be done in the constructor since media type is not known yet
    if (!mRelayInitialized)
    {
        InitRelay();
    }

    // Dump transporter input buffer via st_relay
    DumpViaRelay(Buffer);

    return TransporterNoError;
}

//{{{  CreateConnection
//{{{  doxynote
/// \brief              Try to establish connection to the sink port according its type
//}}}
TransporterStatus_t Transporter_Base_c::CreateConnection(stm_object_h  sink)
{
    SE_ERROR("dynamic connections not supported\n");
    return TransporterError;
}
//}}}

//{{{  RemoveConnection
//{{{  doxynote
/// \brief            Try to remove connection with sink port
//}}}
TransporterStatus_t Transporter_Base_c::RemoveConnection(stm_object_h  sink)
{
    SE_ERROR("dynamic connections not supported\n");
    return TransporterError;
}
//}}}

TransporterStatus_t Transporter_Base_c::GetControl(stm_se_ctrl_t    Control,
                                                   void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_transporter, "Not match transporter control %u\n", Control);
        return TransporterControlNotMatch;
    }
}

TransporterStatus_t Transporter_Base_c::GetCompoundControl(stm_se_ctrl_t    Control,
                                                           void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_transporter, "Not match transporter control %u\n", Control);
        return TransporterControlNotMatch;
    }
}

TransporterStatus_t Transporter_Base_c::SetControl(stm_se_ctrl_t    Control,
                                                   const void      *Data)
{
    switch (Control)
    {
    default:
        SE_DEBUG(group_encoder_transporter, "Not match transporter control %u\n", Control);
        return TransporterControlNotMatch;
    }
}

TransporterStatus_t Transporter_Base_c::SetCompoundControl(stm_se_ctrl_t    Control,
                                                           const void      *Data)
{
    switch (Control)
    {
    default:
        SE_DEBUG(group_encoder_transporter, "Not match transporter control %u\n", Control);
        return TransporterControlNotMatch;
    }
}

void Transporter_Base_c::InitRelay(void)
{
    // Get media type from EncodeStream and then select the st_relay type entry
    SE_ASSERT(Encoder.EncodeStream != NULL);
    stm_se_encode_stream_media_t Media = Encoder.EncodeStream->GetMedia();

    if (Media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO)
    {
        mRelayType = ST_RELAY_TYPE_ENCODER_AUDIO_TRANSPORTER_INPUT;
    }
    else if (Media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO)
    {
        mRelayType = ST_RELAY_TYPE_ENCODER_VIDEO_TRANSPORTER_INPUT;
    }
    else
    {
        // st_relay capture is supported for audio and video streams only
        return;
    }

    // Get new st_relay index
    mRelayIndex = st_relayfs_getindex_fortype_se(mRelayType);
    mRelayInitialized = true;
}

void Transporter_Base_c::TerminateRelay(void)
{
    // Free st_relay index
    st_relayfs_freeindex_fortype_se(mRelayType, mRelayIndex);
    mRelayInitialized = false;
}

void Transporter_Base_c::DumpViaRelay(Buffer_t Buffer)
{
    if (mRelayInitialized)
    {
        // Dump transporter input buffer via st_relay
        Buffer->DumpViaRelay(mRelayType + mRelayIndex, ST_RELAY_SOURCE_SE);
    }
}
