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

#include "manifestor_audio_sourceGrab.h"
#include "acc_mme.h"
#include "audio_conversions.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_AudioSrc_c"

// Definitions of constants for the FADE basic algorithm
#define FADE_NB_STEPS  128
#define FADE_0DB       0x8000
#define FADE_IN_START  0
#define FADE_OUT_START (FADE_0DB - 1) // -1 to avoid saturation in multiply.
#define FADE_STEP      0x8000 / FADE_NB_STEPS

///{{{ Constructor
/// \brief                      Initialise state
/// \return                     Success or fail
Manifestor_AudioSrc_c::Manifestor_AudioSrc_c(void)
    : Manifestor_Source_c()
    , mStatus(AudioGrabStopped)
    , mMute(false)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SE_VERBOSE(group_manifestor_audio_grab, "\n");

    SetGroupTrace(group_manifestor_audio_grab);

    // Fill in surface descriptor details with our assumed defaults
    SurfaceDescriptor.StreamType        = StreamTypeAudio;
    SurfaceDescriptor.BitsPerSample     = AUDIO_GRAB_DEFAULT_BITSPERSAMPLE;
    SurfaceDescriptor.ChannelCount      = AUDIO_GRAB_DEFAULT_CHANNELCOUNT;
    SurfaceDescriptor.SampleRateHz      = AUDIO_GRAB_DEFAULT_SAMPLERATEHZ;
    SurfaceDescriptor.SampleRateHz      = 0;
    SurfaceDescriptor.FrameRate         = 1; // rational
}
//}}}

///{{{ Destructor
/// \brief                      Release audio Source resources
/// \return                     Success or fail
Manifestor_AudioSrc_c::~Manifestor_AudioSrc_c(void)
{
    SE_VERBOSE(group_manifestor_audio_grab, "\n");
}
//}}}

///{{{ SetModuleParameters
/// \brief                      Configure the audio-grab
/// \param ParameterBlockSize   Sizeof the control structure
/// \param ParameterBlock       Control Parameters
/// \return                     Success or fail
ManifestorStatus_t      Manifestor_AudioSrc_c::SetModuleParameters(unsigned int   ParameterBlockSize,
                                                                   void          *ParameterBlock)
{
    SE_DEBUG(group_manifestor_audio_grab, ">\n");
    if (ParameterBlockSize == sizeof(ManifestorAudioParameterBlock_t))
    {
        ManifestorAudioParameterBlock_t *ManifestorParameters;
        ManifestorParameters = (ManifestorAudioParameterBlock_t *) ParameterBlock;

        if (ManifestorParameters->ParameterType == ManifestorAudioSetEmergencyMuteState)
        {
            mMute = ManifestorParameters->EmergencyMute;
            SE_DEBUG(group_manifestor_audio_grab, "Set Mute to %s\n", mMute == false ? "false" : "true");
        }
    }
    return ManifestorNoError;
}


///{{{ PullFrameRead
/// \brief                      Prepare captureBuffer to be returned to memsink_pull
/// \param captureBufferAddr    Kernel Address of buffer provided by memsink
/// \param captureBufferLen     Length of buffer provided by memsink
/// \return                     Number of bytes copied
uint32_t        Manifestor_AudioSrc_c::PullFrameRead(uint8_t *captureBufferAddr, uint32_t captureBufferLen)
{
    struct ParsedAudioParameters_s  *AudioParameters;
    struct ParsedFrameParameters_s  *FrameParameters;
    struct ParsedFrameParameters_s  *ParsedFrameParameters;
    uint32_t                         bufferIndex;
    struct SourceStreamBuffer_s     *StreamBuff      = NULL;
    stm_se_capture_buffer_t         *CaptureBuffer   = (stm_se_capture_buffer_t *)captureBufferAddr;
    AssertComponentState(ComponentRunning);
    //
    // Get StreamBuffer of the CurrentBuffer thanks to its index
    //
    CurrentBuffer->GetIndex(&bufferIndex);
    if (bufferIndex >= MAX_DECODE_BUFFERS)
    {
        SE_ERROR("invalid buffer index %d\n", bufferIndex);
        return 0;
    }

    //
    // retrieve StreamBuffer via its index
    //
    StreamBuff   = &SourceStreamBuffer[bufferIndex];
    //
    // Get Audio parameters from Current buffer metadata
    //
    CurrentBuffer->ObtainMetaDataReference(Player->MetaDataParsedAudioParametersType, (void **) &AudioParameters);
    SE_ASSERT(AudioParameters != NULL);

    CurrentBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **) &ParsedFrameParameters);
    SE_ASSERT(ParsedFrameParameters != NULL);

    //
    // Get Audio parameters from Current buffer metadata
    //
    CurrentBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **) &FrameParameters);
    SE_ASSERT(FrameParameters != NULL);

    //
    // CaptureBuffer structure provide the size, physical and virtual address of the destination buffer
    //
    FramePhysicalAddress  = (void *)  Stream->GetDecodeBufferManager()->ComponentBaseAddress(CurrentBuffer, PrimaryManifestationComponent, PhysicalAddress);
    FrameVirtualAddress   = (void *)  Stream->GetDecodeBufferManager()->ComponentBaseAddress(CurrentBuffer, PrimaryManifestationComponent, CachedAddress);
    FrameSize             =    AudioParameters->SampleCount
                               *  AudioParameters->Source.ChannelCount
                               * (AudioParameters->Source.BitsPerSample / 8);
    //
    // Update Surface descriptor
    //
    SurfaceDescriptor.BitsPerSample = AudioParameters->Source.BitsPerSample;
    SurfaceDescriptor.ChannelCount = AudioParameters->Source.ChannelCount;
    SurfaceDescriptor.SampleRateHz = AudioParameters->Source.SampleRateHz;
    CaptureBuffer->u.uncompressed.system_time              = StreamBuff->OutputTiming->SystemPlaybackTime;
    CaptureBuffer->u.uncompressed.native_time_format       = TIME_FORMAT_US;
    CaptureBuffer->u.uncompressed.native_time              = FrameParameters->NormalizedPlaybackTime;
    CaptureBuffer->u.uncompressed.user_data_size           = 0;
    CaptureBuffer->u.uncompressed.user_data_buffer_address = NULL;
    CaptureBuffer->u.uncompressed.discontinuity            = STM_SE_DISCONTINUITY_CONTINUOUS;
    CaptureBuffer->u.uncompressed.media                    = STM_SE_ENCODE_STREAM_MEDIA_AUDIO;
    // Audio specific Meta Data
    // Sample Rate [Hz]
    CaptureBuffer->u.uncompressed.audio.core_format.sample_rate = AudioParameters->Source.SampleRateHz;
    // Program level in millibel (typ. -2000, -2300, -3100) ">=0" is treated as a default/unknow value of -2000mb
    CaptureBuffer->u.uncompressed.audio.program_level      = AudioParameters->StreamMetadata.DialogNorm;
    // Indicates whether the input as any kind of audio (pre)emphasis. -- stm_se_emphasis_type_t
    CaptureBuffer->u.uncompressed.audio.emphasis           = (AudioParameters->Emphasis ? STM_SE_EMPH_50_15us : STM_SE_NO_EMPHASIS);
    // Only STM_SE_AUDIO_PCM_FMT_S32LE is supported in Coder_Audio_Mme_c
    CaptureBuffer->u.uncompressed.audio.sample_format      = STM_SE_AUDIO_PCM_FMT_S32LE;
    StmSeAudioChannelPlacementAnalysis_t Analysis;

    // Convert to encoder metadata
    if ((0 != StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(
             &CaptureBuffer->u.uncompressed.audio.core_format.channel_placement,
             &Analysis,
             (enum eAccAcMode)AudioParameters->Organisation,
             AudioParameters->Source.ChannelCount))
        || (AudioParameters->Source.ChannelCount != CaptureBuffer->u.uncompressed.audio.core_format.channel_placement.channel_count))
    {
        SE_ERROR("Invalid or not supported Organisation parameter (0x%02X)\n", AudioParameters->Organisation);
        return 0;
    }

    //
    // Copy Audio buffer into provided buffer
    // First check buffer large enough
    //
    if (CaptureBuffer->buffer_length < FrameSize)
    {
        SE_ERROR("Audio Capture buffer too small: excepted %d got %d bytes\n", FrameSize, CaptureBuffer->buffer_length);
        return 0;
    }

    // Perform a software copy (no mandataory HW accelerator here)
    uint32_t Offset = 0;

    if (mMute == false)
    {
        if (mStatus == AudioGrabMuting)
        {
            // fade in the signal
            Offset = Fade32((int32_t *) FrameVirtualAddress, (int32_t *) CaptureBuffer->virtual_address,
                            AudioParameters, FADE_IN_START, +FADE_STEP);
        }

        memcpy((void *) & (((char *)CaptureBuffer->virtual_address)[Offset]),
               (void *) & (((char *)FrameVirtualAddress)[Offset]), FrameSize - Offset);

        mStatus = AudioGrabPlaying;
    }
    else // mMute == true
    {
        if (mStatus == AudioGrabPlaying)
        {
            // Fade Out the audio signal
            Offset = Fade32((int32_t *) FrameVirtualAddress, (int32_t *)(CaptureBuffer->virtual_address),
                            AudioParameters, FADE_OUT_START, -FADE_STEP);
        }

        memset((void *) & (((char *)CaptureBuffer->virtual_address)[Offset]),
               0, FrameSize - Offset);

        mStatus = AudioGrabMuting;
    }

    // Update the real copied frame size
    CaptureBuffer->payload_length = FrameSize;
    return (CaptureBuffer->payload_length);
}
//}}}


///{{{ initialiseConnection
/// \brief                      Perform after connection actions
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_AudioSrc_c::initialiseConnection(void)
{
    //
    // do specific Audio initialisation
    //
    //
    // perform general initialisation
    //
    return Manifestor_Source_c::initialiseConnection();
}
//}}}
///{{{ terminateConnection
/// \brief                      Perform before connection actions
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_AudioSrc_c::terminateConnection(void)
{
    //
    // do specific Audio initialisation
    //
    //
    // perform general termination
    //
    return Manifestor_Source_c::terminateConnection();
}
//}}}

///{{{ GetSurfaceParameters
/// \brief                      Return Audio surface descriptor
/// \return                     Success or fail
ManifestorStatus_t      Manifestor_AudioSrc_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    SE_VERBOSE(group_manifestor_audio_grab, "\n");
    *SurfaceParameters  = &SurfaceDescriptor;
    *NumSurfaces        = 1;
    return ManifestorNoError;
}
//}}}

ManifestorStatus_t  Manifestor_AudioSrc_c::GetChannelConfiguration(enum eAccAcMode *AcMode)
{
    *AcMode = ACC_MODE_ID;
    return ManifestorNoError;
}

///{{{ Fade32
/// \brief                      Fade out / in decoded buffer carrying 32 bits samples
/// \return                     Number of bytes faded in/out
uint32_t Manifestor_AudioSrc_c::Fade32(int32_t *frame_p, int32_t *capture_p, struct ParsedAudioParameters_s *AudioParameters, int32_t coeff, uint32_t step)
{
    int32_t  s      = 0;
    int32_t  nchan  = AudioParameters->Source.ChannelCount;

    // Fade In the audio signal
    while ((s < AudioParameters->SampleCount) && (s < FADE_NB_STEPS))
    {
        int32_t c;

        for (c = 0; c < nchan; c++)
        {
            capture_p[c] = (frame_p[c] >> 16) * coeff; // basic Q15 * Q16 = Q31 multiplication
        }

        // apply linear slope
        coeff += step;

        // Check overflow boundaries
        if (coeff >= FADE_0DB)
        {
            break;
        }

        if (coeff < 0)
        {
            break;
        }

        // next sample
        frame_p   = & frame_p[c];
        capture_p = & capture_p[c];
        s++;
    }

    return s * nchan * (AudioParameters->Source.BitsPerSample / 8);
}
//}}}
