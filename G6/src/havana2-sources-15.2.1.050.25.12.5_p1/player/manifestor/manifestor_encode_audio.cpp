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

#include "acc_mme.h"
#include "audio_conversions.h"
#include <ACC_Transformers/acc_mmedefines.h>
#include "manifestor_encode_audio.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_EncodeAudio_c"

#define AUDIO_ENCODE_DEFAULT_BITSPERSAMPLE          32
#define AUDIO_ENCODE_DEFAULT_CHANNELCOUNT            2
#define AUDIO_ENCODE_DEFAULT_SAMPLERATEHZ        48000

//{{{  Constructor
//{{{  doxynote
/// \brief                      Initialise and fill the Configuration of this manifestor
/// \return                     No return value
//}}}
Manifestor_EncodeAudio_c::Manifestor_EncodeAudio_c(void)
    : Enabled(false)
    , mAcMode(ACC_MODE_ID)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SetGroupTrace(group_manifestor_audio_encode);

    Configuration.Capabilities                  = MANIFESTOR_CAPABILITY_ENCODE;
    Configuration.ManifestorName                = "EncodAudio";

    SurfaceDescriptor.StreamType                = StreamTypeAudio;
    SurfaceDescriptor.BitsPerSample             = AUDIO_ENCODE_DEFAULT_BITSPERSAMPLE;
    SurfaceDescriptor.ChannelCount              = AUDIO_ENCODE_DEFAULT_CHANNELCOUNT;
    SurfaceDescriptor.SampleRateHz              = AUDIO_ENCODE_DEFAULT_SAMPLERATEHZ;
    SurfaceDescriptor.FrameRate                 = 1; // rational
    // But request that we set output to match input rates
    SurfaceDescriptor.InheritRateAndTypeFromSource = true;
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Give up switch off the lights and go home
/// \return                     No return value
//}}}
Manifestor_EncodeAudio_c::~Manifestor_EncodeAudio_c(void)
{
    Halt();
}
//}}}

ManifestorStatus_t  Manifestor_EncodeAudio_c::GetChannelConfiguration(enum eAccAcMode *AcMode)
{
    *AcMode = mAcMode;
    return ManifestorNoError;
}

//{{{  PrepareEncodeMetaData
//{{{  doxynote
/// \brief                      Actually prepare the uncompressed metadata of a decode buffer encoder
/// \param Buffer               A Raw Decode Buffer to wrap
/// \param EncodeBuffer         Returned Encode input buffer reference to be sent Encoder Subsystem
/// \return                     Success or failure depending upon Encode Input Buffer was created or not.
//}}}
ManifestorStatus_t Manifestor_EncodeAudio_c::PrepareEncodeMetaData(Buffer_t  Buffer, Buffer_t  *EncodeBuffer)
{
    stm_se_uncompressed_frame_metadata_t     *Meta;
    void                     *InputBufferAddr[3];
    Buffer_t                  InputBuffer = NULL;
    unsigned int              DataSize = 0;
    // Input Buffer MetaData.
    ParsedFrameParameters_t          *FrameParameters;
    ParsedAudioParameters_t          *AudioParameters;

    SE_DEBUG(group_manifestor_audio_encode, "\n");

    // Obtain InputBuffer MetaData
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)&FrameParameters);
    SE_ASSERT(FrameParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedAudioParametersType, (void **)&AudioParameters);
    SE_ASSERT(AudioParameters != NULL);

    // Check Input Parameters
    if (AudioParameters->Source.SampleRateHz == 0)
    {
        SE_ERROR("Can't encode source with SampleRateHz = 0 !\n");
        return ManifestorError;
    }

    SE_ASSERT(AudioParameters->Source.ChannelCount);
    SE_ASSERT(AudioParameters->Source.BitsPerSample);

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
    DataSize                         =    AudioParameters->SampleCount *  AudioParameters->Source.ChannelCount
                                          * (AudioParameters->Source.BitsPerSample / 8);
    InputBuffer->RegisterDataReference(DataSize, InputBufferAddr);
    InputBuffer->SetUsedDataSize(DataSize);

    // Get user data buffer (attached to input buffer) to be filled
    InputBuffer->ObtainMetaDataReference(Encoder_BufferTypes->InputMetaDataBufferType, (void **)(&Meta));
    SE_ASSERT(Meta != NULL);

    // General Meta Data
    Meta->system_time           = OS_GetTimeInMicroSeconds(); //AudioOutputTiming->SystemPlaybackTime
    Meta->native_time_format    = TIME_FORMAT_PTS;
    Meta->native_time           = FrameParameters->NativePlaybackTime; // NormalizedPlaybackTime
    Meta->user_data_size        = 0;
    Meta->user_data_buffer_address  = NULL;
    Meta->media                 = STM_SE_ENCODE_STREAM_MEDIA_AUDIO;
    Meta->discontinuity         = STM_SE_DISCONTINUITY_CONTINUOUS;

    // Audio specific Meta Data
    Meta->audio.core_format.sample_rate     = AudioParameters->Source.SampleRateHz; // Sample Rate [Hz]
    Meta->audio.program_level       = 0; // Program level in millibel (typ. -2000, -2300, -3100) ">=0" is treated as a default/unknow value of -2000mb
    Meta->audio.emphasis            = STM_SE_NO_EMPHASIS; // Indicates whether the input as any kind of audio (pre)emphasis. -- stm_se_emphasis_type_t
    Meta->audio.sample_format       = STM_SE_AUDIO_PCM_FMT_S32LE; // Only STM_SE_AUDIO_PCM_FMT_S32LE is supported in Coder_Audio_Mme_c

    // Convert to encoder metadata
    StmSeAudioChannelPlacementAnalysis_t Analysis;
    if ((0 != StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(&Meta->audio.core_format.channel_placement,
                                                                 &Analysis,
                                                                 (enum eAccAcMode)AudioParameters->Organisation,
                                                                 AudioParameters->Source.ChannelCount))
        || (AudioParameters->Source.ChannelCount != Meta->audio.core_format.channel_placement.channel_count))
    {
        SE_ERROR("Invalid or not supported Organisation parameter (0x%02X)\n", AudioParameters->Organisation);
        InputBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        return ManifestorError;
    }

    // request Preproc for current AcMode update
    if (EncodeStream != NULL)
    {
        Preproc_t  Preproc = NULL;
        EncodeStream->GetClassList(NULL, &Preproc, NULL, NULL, NULL);
        if (Preproc != NULL)
        {
            Preproc->GetChannelConfiguration((int64_t *)&mAcMode);
            SE_VERBOSE(GetGroupTrace(), "preproc channel configuration:%s\n", StmSeAudioAcModeGetName(mAcMode));
        }
    }

    // Set the returned encode buffer
    *EncodeBuffer = InputBuffer;

    return ManifestorNoError;
}
//}}}

