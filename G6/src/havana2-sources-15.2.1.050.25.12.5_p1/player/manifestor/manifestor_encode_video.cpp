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

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_encode_video.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_EncodeVideo_c"

// These defaults are not used. They are defined to establish sane defaults in the construction.
// Actual rates are defined by the parameters given by the stream
#define FRAME_ENCODER_DEFAULT_DISPLAY_WIDTH                1920
#define FRAME_ENCODER_DEFAULT_DISPLAY_HEIGHT               1080
#define FRAME_ENCODER_DEFAULT_FRAME_RATE                   50

//{{{  Constructor
//{{{  doxynote
/// \brief                      Initialise and fill the Configuration of this manifestor
/// \return                     No return value
//}}}
Manifestor_EncodeVideo_c::Manifestor_EncodeVideo_c(void)
    : VideoMetadataHelper()
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SetGroupTrace(group_manifestor_video_encode);

    Configuration.Capabilities                  = MANIFESTOR_CAPABILITY_ENCODE;
    Configuration.ManifestorName                = "EncodVideo";

    SurfaceDescriptor.StreamType                = StreamTypeVideo;
    SurfaceDescriptor.ClockPullingAvailable     = false;
    SurfaceDescriptor.DisplayWidth              = FRAME_ENCODER_DEFAULT_DISPLAY_WIDTH;
    SurfaceDescriptor.DisplayHeight             = FRAME_ENCODER_DEFAULT_DISPLAY_HEIGHT;
    SurfaceDescriptor.Progressive               = false;
    SurfaceDescriptor.FrameRate                 = Rational_t(FRAME_ENCODER_DEFAULT_FRAME_RATE, 1);
    SurfaceDescriptor.PercussiveCapable         = true;
    // But request that we set output to match input rates
    SurfaceDescriptor.InheritRateAndTypeFromSource = true;
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Give up switch off the lights and go home
/// \return                     No return value
//}}}
Manifestor_EncodeVideo_c::~Manifestor_EncodeVideo_c(void)
{
    Halt();
}
//}}}

//{{{  PrepareEncodeMetaData
//{{{  doxynote
/// \brief                      ctually prepare the uncompressed metadata of a decode buffer encoder
/// \param Buffer               A Raw Decode Buffer to wrap
/// \param EncodeBuffer         Returned Encode input buffer reference to be sent Encoder Subsystem
/// \return                     Success or failure depending upon Encode Input Buffer was created or not.
//}}}
ManifestorStatus_t Manifestor_EncodeVideo_c::PrepareEncodeMetaData(Buffer_t  Buffer, Buffer_t  *EncodeBuffer)
{
    stm_se_uncompressed_frame_metadata_t     *Meta;
    void                     *InputBufferAddr[3];
    Buffer_t                  InputBuffer = NULL;
    unsigned int              DataSize = 0;
    ParsedFrameParameters_t  *FrameParameters;
    ParsedVideoParameters_t  *VideoParameters;

    SE_DEBUG(group_manifestor_video_encode, "\n");

    // Obtain InputBuffer MetaData
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)&FrameParameters);
    SE_ASSERT(FrameParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    // Get Encoder buffer
    EncoderStatus_t EncoderStatus = Encoder->GetInputBuffer(&InputBuffer);
    if ((EncoderStatus != EncoderNoError) || (InputBuffer == NULL))
    {
        SE_ERROR("Failed to get Encoder Input Buffer\n");
        return ManifestorError;
    }

    // Register our pointers into the Buffer_t
    InputBufferAddr[CachedAddress]   = Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent, CachedAddress);
    InputBufferAddr[PhysicalAddress] = Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, PrimaryManifestationComponent, PhysicalAddress);
    InputBufferAddr[2]               = NULL;
    DataSize                         = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, PrimaryManifestationComponent);

    InputBuffer->RegisterDataReference(DataSize, InputBufferAddr);
    InputBuffer->SetUsedDataSize(DataSize);

    // Get metadata buffer (attached to input buffer) to be filled
    InputBuffer->ObtainMetaDataReference(Encoder_BufferTypes->InputMetaDataBufferType, (void **)(&Meta));
    SE_ASSERT(Meta != NULL);

    // Meta Data setup
    Meta->system_time           = OS_GetTimeInMicroSeconds();
    Meta->native_time_format    = TIME_FORMAT_PTS;
    Meta->native_time           = FrameParameters->NativePlaybackTime;
    setUncompressedMetadata(Buffer, Meta, VideoParameters);

    // Set the returned encode buffer
    *EncodeBuffer = InputBuffer;

    return ManifestorNoError;
}
//}}}

