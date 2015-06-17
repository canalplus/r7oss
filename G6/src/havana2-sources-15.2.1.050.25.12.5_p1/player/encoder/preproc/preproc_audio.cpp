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

#include "preproc_audio.h"
#include "audio_conversions.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Audio_c"

Preproc_Audio_c::Preproc_Audio_c(void)
    : Preproc_Base_c()
    , CurrentInputBuffer()
    , CurrentParameters()
    , MainOutputBuffer()
    , DefaultParameters()
    , PreviousParameters()
    , mRelayfsIndex(0)
    , AudioStatistics()
    , Controls()
{
    SetGroupTrace(group_encoder_audio_preproc);
    mRelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT);

    /// Default channel placement is MODE20
    DefaultParameters.Controls.CoreFormat.channel_placement.channel_count = PREPROC_AUDIO_DEFAULT_OUTPUT_CHANNEL_COUNT;
    DefaultParameters.Controls.CoreFormat.channel_placement.chan[0] = PREPROC_AUDIO_DEFAULT_OUTPUT_CHAN_0;
    DefaultParameters.Controls.CoreFormat.channel_placement.chan[1] = PREPROC_AUDIO_DEFAULT_OUTPUT_CHAN_1;

    /// Default sample rate is 48kHz
    DefaultParameters.Controls.CoreFormat.sample_rate = PREPROC_AUDIO_DEFAULT_OUTPUT_SAMPLING_FREQ;

    /// Default DualMono: Stream Driven and stereo  (left, right)
    DefaultParameters.Controls.DualMono.StreamDriven = true;
    DefaultParameters.Controls.DualMono.DualMode = STM_SE_STEREO_OUT;

    Controls = DefaultParameters.Controls;

    /// metadata defaults
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.channel_count = PREPROC_AUDIO_DEFAULT_INPUT_CHANNEL_ALLOC;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[0] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_0;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[1] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_1;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[2] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_2;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[3] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_3;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[4] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_4;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[5] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_5;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[6] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_6;
    DefaultParameters.InputMetadata.audio.core_format.channel_placement.chan[7] = (uint8_t)PREPROC_AUDIO_DEFAULT_INPUT_CHAN_7;
    DefaultParameters.InputMetadata.audio.core_format.sample_rate = PREPROC_AUDIO_DEFAULT_INPUT_SAMPLING_FREQ;
    DefaultParameters.InputMetadata.audio.emphasis = PREPROC_AUDIO_DEFAULT_INPUT_EMPHASIS;

    CurrentParameters.InputMetadata = &DefaultParameters.InputMetadata;
}

Preproc_Audio_c::~Preproc_Audio_c(void)
{
    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT, mRelayfsIndex);
}

PreprocStatus_t Preproc_Audio_c::Halt(void)
{
    // Call the Base Class implementation
    return Preproc_Base_c::Halt();
}

/* @brief Gets basic info from an input buffer object and fills a PreprocAudioBuffer_t structure
 * PreprocAudioBuffer_t are preferable to Buffer_c because one the information is stored in a PreprocAudioBuffer_t structure
 * for most processes cases there is no need to call @ref Buffer_c methods anymore */
PreprocStatus_t Preproc_Audio_c::FillPreprocAudioBufferFromInputBuffer(PreprocAudioBuffer_t *AudioBuffer, Buffer_t Buffer)
{
    memset(AudioBuffer, 0, sizeof(PreprocAudioBuffer_t));
    AudioBuffer->Buffer = Buffer;

    /* Considers size = 0 and address = 0 as valid (for future discontinuity markers) */
    Buffer->ObtainDataReference(NULL, &AudioBuffer->Size, (void **)(&AudioBuffer->PhysicalAddress), PhysicalAddress);
    if (AudioBuffer->PhysicalAddress == NULL)
    {
        SE_ERROR("Could not get a valid Phys AudioBuffer\n");
        return PreprocError;
    }

    Buffer->ObtainDataReference(NULL, &AudioBuffer->Size, (void **)(&AudioBuffer->CachedAddress), CachedAddress);
    if (AudioBuffer->CachedAddress == NULL)
    {
        SE_ERROR("Could not get a valid AudioBuffer\n");
        return PreprocError;
    }

    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&AudioBuffer->Metadata));
    SE_ASSERT(AudioBuffer->Metadata != NULL);

    Buffer->ObtainMetaDataReference(EncodeCoordinatorMetaDataBufferType, (void **)(&AudioBuffer->EncodeCoordinatorMetadata));
    SE_ASSERT(AudioBuffer->EncodeCoordinatorMetadata != NULL);

    return PreprocNoError;
}

/* @brief Gets basic info from an preprocessed frame buffer object and fills a PreprocAudioBuffer_t structure
 * PreprocAudioBuffer_t are preferable to Buffer_c because one the information is stored in a PreprocAudioBuffer_t structure
 * for most processes cases there is no need to call @ref Buffer_c methods anymore */
PreprocStatus_t Preproc_Audio_c::FillPreprocAudioBufferFromPreProcBuffer(PreprocAudioBuffer_t *AudioBuffer, Buffer_t Buffer)
{
    __stm_se_frame_metadata_t *PreProcMetaDataDescriptor;
    memset(AudioBuffer, 0, sizeof(PreprocAudioBuffer_t));
    AudioBuffer->Buffer = Buffer;

    /* Considers size = 0 and address = 0 as valid (for future discontinuity markers) */
    Buffer->ObtainDataReference(NULL, &AudioBuffer->Size, (void **)(&AudioBuffer->PhysicalAddress), PhysicalAddress);
    if (AudioBuffer->PhysicalAddress == NULL)
    {
        SE_ERROR("Could not get a valid Phys AudioBuffer\n");
        return PreprocError;
    }

    SE_VERBOSE(group_encoder_audio_preproc, "Phy Address:%p", AudioBuffer->PhysicalAddress);

    Buffer->ObtainDataReference(NULL, &AudioBuffer->Size, (void **)(&AudioBuffer->CachedAddress), CachedAddress);
    if (AudioBuffer->CachedAddress == NULL)
    {
        SE_ERROR("Could not get a valid AudioBuffer\n");
        return PreprocError;
    }

    SE_VERBOSE(group_encoder_audio_preproc, "Cached Address:%p", AudioBuffer->CachedAddress);

    Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreProcMetaDataDescriptor));
    SE_ASSERT(PreProcMetaDataDescriptor != NULL);

    AudioBuffer->Metadata        = &PreProcMetaDataDescriptor->uncompressed_frame_metadata;
    AudioBuffer->ProcessMetadata = &PreProcMetaDataDescriptor->encode_process_metadata;
    AudioBuffer->EncodeCoordinatorMetadata = &PreProcMetaDataDescriptor->encode_coordinator_metadata;

    return PreprocNoError;
}

/* @brief Gets the number of channels in a @ref PreprocAudioBuffer_t @var AudioBuffer and writes in @var Channels
 * The number of channels is the "physical" number of channels, not the number of active channels
 * e.g. an 8 channel buffer with only stereo content will will return 8 Channels */
PreprocStatus_t   Preproc_Audio_c::GetNumberAllocatedChannels(PreprocAudioBuffer_t *AudioBuffer, uint32_t *Channels)
{
    *Channels = 0;

    if (NULL == AudioBuffer->Metadata)
    {
        SE_ERROR("AudioBuffer->Metadata is NULL\n");
        return PreprocError;
    }

    if (0 != AudioBuffer->Size)
    {
        *Channels = AudioBuffer->Metadata->audio.core_format.channel_placement.channel_count;
    }

    return PreprocNoError;
}

/* @brief Gets the number of bytes per sample in a @ref PreprocAudioBuffer_t @var AudioBuffer and write in @var BytesPerSample
 */
PreprocStatus_t   Preproc_Audio_c::GetNumberBytesPerSample(PreprocAudioBuffer_t *AudioBuffer, uint32_t *BytesPerSample)
{
    *BytesPerSample = 0;

    if (NULL == AudioBuffer->Metadata)
    {
        SE_ERROR("AudioBuffer->Metadata is NULL\n");
        return PreprocError;
    }

    if (0 != AudioBuffer->Size)
    {
        if (0 == (*BytesPerSample = StmSeAudioGetNrBytesFromLpcmFormat(AudioBuffer->Metadata->audio.sample_format)))
        {
            return PreprocError;
        }
    }

    return PreprocNoError;
}

/* @brief Gets the number of samples (per channel) in a @ref PreprocAudioBuffer_t @var AudioBuffer and write in @var Samples
 */
PreprocStatus_t   Preproc_Audio_c::GetNumberSamples(PreprocAudioBuffer_t *AudioBuffer, uint32_t *Samples)
{
    uint32_t BytesPerSample, Channels;
    *Samples = 0;

    if (0 != AudioBuffer->Size)
    {
        if ((PreprocNoError != GetNumberAllocatedChannels(AudioBuffer, &Channels))
            || (PreprocNoError != GetNumberBytesPerSample(AudioBuffer, &BytesPerSample))
            || (0 == BytesPerSample)
            || (0 == Channels))
        {
            SE_ERROR("Could not get valid audio metadata data from Buffer\n");
            return PreprocError;
        }

        *Samples = AudioBuffer->Size / (Channels * BytesPerSample);
    }

    return PreprocNoError;
}

/* @brief Function to update the Preproc_Audio_c::Statistics at the beginning of Preproc_Audio_c::Input from a PreprocAudioBuffer_t */
PreprocStatus_t   Preproc_Audio_c::InputUpdateStatistics(PreprocAudioBuffer_t *AudioBuffer)
{
    PreprocStatus_t Status = PreprocNoError;
    uint32_t NbSamples;
    AudioStatistics.In.Frames++; //Increment even if fails
    SE_DEBUG(group_encoder_audio_preproc, "[%lld]\n", AudioStatistics.In.Frames);

    if (0 != AudioBuffer->Size)
    {
        if (PreprocNoError != GetNumberSamples(AudioBuffer, &NbSamples))
        {
            SE_ERROR("Could not get valid audio metadata data from Buffer\n");
            return PreprocError;
        }

        AudioStatistics.In.Bytes   += AudioBuffer->Size;
        AudioStatistics.In.Samples += NbSamples;
    }

    return Status;
}

/* @brief Function to update the Preproc_Audio_c::Statistics when a buffer is to be pushed to the Ring */
PreprocStatus_t   Preproc_Audio_c::OutputUpdateStatistics(PreprocAudioBuffer_t *AudioBuffer)
{
    PreprocStatus_t Status = PreprocNoError;
    uint32_t NbSamples;
    AudioStatistics.Out.Frames++;

    if (0 != AudioBuffer->Size)
    {
        if (PreprocNoError != GetNumberSamples(AudioBuffer, &NbSamples))
        {
            SE_ERROR("Could not get valid audio metadata data from Buffer\n");
            return PreprocError;
        }

        AudioStatistics.Out.Bytes   += AudioBuffer->Size;
        AudioStatistics.Out.Samples += NbSamples;
    }

    return Status;
}

/* @brief This function does basic setup should be called by children classes at the beginning of Input
 * It manages internal statistics manages controls and metadata */
PreprocStatus_t Preproc_Audio_c::Input(Buffer_t Buffer)
{
    PreprocStatus_t Status;
    AudioStatistics.In.Inputs++;
    SE_DEBUG(group_encoder_audio_preproc, "[%lld]", AudioStatistics.In.Inputs);

    if (NULL == Buffer)
    {
        SE_ERROR("Buffer is NULL\n");
        return  PreprocError;
    }

    // Dump preproc input buffer via st_relay
    Buffer->DumpViaRelay(ST_RELAY_TYPE_ENCODER_AUDIO_PREPROC_INPUT + mRelayfsIndex, ST_RELAY_SOURCE_SE);

    // Call function from parent class
    Status = Preproc_Base_c::Input(Buffer);
    if (PreprocNoError != Status)
    {
        PREPROC_ERROR_RUNNING("Failed\n");
        return Status;
    }

    /* @note from here he have a PreprocFrameBuffer reference, with attached InputBuffer */
    //Parse InputBuffer
    Status = FillPreprocAudioBufferFromInputBuffer(&CurrentInputBuffer, Buffer);
    if (PreprocNoError != Status)
    {
        SE_ERROR("Not a valid buffer\n");
        ReleaseMainPreprocFrameBuffer();
        return Status;
    }

    //Parse PreprocFrameBuffer
    Status = FillPreprocAudioBufferFromPreProcBuffer(&MainOutputBuffer, PreprocFrameBuffer);
    if (PreprocNoError != Status)
    {
        SE_ERROR("PreprocFrameBuffer Not a valid buffer\n");
        ReleaseMainPreprocFrameBuffer();
        return Status;
    }

    //Copy input metadata ptr to the current context structure
    CurrentParameters.InputMetadata = CurrentInputBuffer.Metadata;
    //Latch Current Controls
    CurrentParameters.Controls = Controls;
    //If some Controls are latched 'bypass' values, we set them to values from input instead
    CheckCurrentControlsForBypassValues();

    Status = InputUpdateStatistics(&CurrentInputBuffer);
    if (PreprocNoError != Status)
    {
        SE_ERROR("Not a valid buffer\n");
        ReleaseMainPreprocFrameBuffer();
        return Status;
    }

    return Status;
}

/* @brief A utility function to release a Buffer reference from the Output context*/
void   Preproc_Audio_c::ReleasePreprocOutFrameBuffer(PreprocAudioBuffer_t *AudioBuffer)
{
    if (NULL == AudioBuffer)
    {
        SE_ERROR("AudioBuffer is NULL\n");
        return;
    }

    if (NULL != AudioBuffer->Buffer)
    {
        SE_DEBUG(group_encoder_audio_preproc, "buffer %p DecrementReferenceCount()\n", AudioBuffer->Buffer);
        AudioBuffer->Buffer->DecrementReferenceCount();
        AudioBuffer->Buffer = NULL;
    }
}

/* @brief A utility function to release the Buffer references got from calling Preproc_Base_c::Input()
 * Should be used by children classes  to release the Buffer references got from calling Preproc_Audio_c::Input()*/
void   Preproc_Audio_c::ReleaseMainPreprocFrameBuffer()
{
    if (NULL != PreprocFrameBuffer)
    {
        PreprocFrameBuffer->DecrementReferenceCount();
        PreprocFrameBuffer = NULL;
    }
}

/* @brief This function should be used to send a processed buffer to the ring */
PreprocStatus_t Preproc_Audio_c::SendBufferToOutput(PreprocAudioBuffer_t *AudioBuffer)
{
    if ((NULL == AudioBuffer) || (NULL == AudioBuffer->Buffer) || (NULL == AudioBuffer->Metadata))
    {
        SE_ERROR("not a valid AudioBuffer %p to send out\n", AudioBuffer);
        return  PreprocError;
    }

    AudioBuffer->Buffer->SetUsedDataSize(AudioBuffer->Size);

    Buffer_t ContentBuffer;
    AudioBuffer->Buffer->ObtainAttachedBufferReference(PreprocFrameAllocType, &ContentBuffer);
    SE_ASSERT(ContentBuffer != NULL);

    ContentBuffer->SetUsedDataSize(AudioBuffer->Size) ;
    BufferStatus_t BufferStatus = ContentBuffer->ShrinkBuffer(AudioBuffer->Size);
    if (BufferNoError != BufferStatus)
    {
        SE_ERROR("ShrinkBuffer(%d) resulted in %08X\n", AudioBuffer->Size, BufferStatus);
        ReleasePreprocOutFrameBuffer(AudioBuffer);
        return PreprocError;
    }

    //Preproc_Base_c::Output() will release the references
    //so We have to update the statistics before Preproc_Base_c::Output()
    //backup output statistics in case Preproc_Base_c::Output fails
    PreprocAudioStatistics_t TmpOutputStats = AudioStatistics;
    if (PreprocNoError != OutputUpdateStatistics(AudioBuffer))
    {
        SE_ERROR("OutputUpdateStatistics Failed\n");
        ReleasePreprocOutFrameBuffer(AudioBuffer);
        return PreprocError;
    }

    // Let parent class deliver to Ring and detach InputBuffer
    PreprocStatus_t Status = Preproc_Base_c::Output(AudioBuffer->Buffer);
    if (PreprocNoError != Status)
    {
        //No need to call ReleasePreprocOutFrameBuffer(AudioBuffer) base class does it
        SE_ERROR("Output status %08X\n", Status);
        //restore *output* statistics
        AudioStatistics.Out = TmpOutputStats.Out;
        return Status;
    }

    SE_DEBUG(group_encoder_audio_preproc, "Sent to Ring: %llu frames, %llu bytes, %llu samples\n"
             , AudioStatistics.Out.Frames, AudioStatistics.Out.Bytes, AudioStatistics.Out.Samples);
    return PreprocNoError;
}

/* This function is to be called at the end Input() to update context
 * @var Status : the ending status of Input() */
void Preproc_Audio_c::InputPost(PreprocStatus_t Status)
{
    if (PreprocNoError == Status)
    {
        AudioStatistics.In.FramesOk++;
    }

    SaveCurrentParametersToPrevious();
}

/* @brief Saves supported controls values in the object */
PreprocStatus_t   Preproc_Audio_c::SetControl(stm_se_ctrl_t Control, const void *Data)
{
    if (NULL == Data)
    {
        return PreprocError;  // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
        switch (*(uint32_t *)Data)
        {
        case STM_SE_CTRL_VALUE_STREAM_DRIVEN_DUALMONO:
            Controls.DualMono.StreamDriven = true;
            SE_DEBUG(group_encoder_audio_preproc, "stream driven dualmono\n");
            break;
        case STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO:
            Controls.DualMono.StreamDriven = false;
            SE_DEBUG(group_encoder_audio_preproc, "user enforced dualmono\n");
            break;

        default: SE_ERROR("Undefined stream driven dualmono value:%d\n", *(uint32_t *)Data);
            return PreprocError;
        }

        break;

    case STM_SE_CTRL_DUALMONO:
        switch (*(stm_se_dual_mode_t *)Data)
        {
        case STM_SE_STEREO_OUT     : //fall-through
        case STM_SE_DUAL_LEFT_OUT  : //fall-through
        case STM_SE_DUAL_RIGHT_OUT : //fall-through
        case STM_SE_MONO_OUT       :
            Controls.DualMono.DualMode = *(stm_se_dual_mode_t *)Data;
            break;

        default: SE_ERROR("Undefined ctrl dualmono value:%d\n", *(uint32_t *)Data);
            return PreprocError;
        }

        SE_DEBUG(group_encoder_audio_preproc, "ctrl dualmono:%d\n", *(uint32_t *)Data);
        break;

    default :
        return PreprocError;
    }

    return PreprocNoError;
}

/* @brief Saves supported controls values in the object */
PreprocStatus_t   Preproc_Audio_c::SetCompoundControl(stm_se_ctrl_t Control, const void *Data)
{
    if (NULL == Data)
    {
        return PreprocError;  // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT:
        Controls.CoreFormat.sample_rate = ((stm_se_audio_core_format_t *)Data)->sample_rate;
        Controls.CoreFormat.channel_placement = ((stm_se_audio_core_format_t *)Data)->channel_placement;
        SE_DEBUG(group_encoder_audio_preproc, "stream core_format rate:%d\n",
                 Controls.CoreFormat.sample_rate);
        SE_DEBUG(group_encoder_audio_preproc, "stream core_format ch_count:%d\n",
                 Controls.CoreFormat.channel_placement.channel_count);

        if (Controls.CoreFormat.channel_placement.channel_count >= lengthof(Controls.CoreFormat.channel_placement.chan))
        {
            SE_ERROR("invalid channel count:%d\n", Controls.CoreFormat.channel_placement.channel_count);
            Controls.CoreFormat.channel_placement.channel_count = 0;
            return PreprocError;
        }

        for (int i = 0; i < Controls.CoreFormat.channel_placement.channel_count; i++)
        {
            // dont print stuffing channels
            if (STM_SE_AUDIO_CHAN_STUFFING == Controls.CoreFormat.channel_placement.chan[i]) { continue; }
            SE_DEBUG(group_encoder_audio_preproc, "stream core_format ch[%d]:%s (%d)\n",
                     i, StmSeAudioChannelIdGetName((stm_se_audio_channel_id_t)Controls.CoreFormat.channel_placement.chan[i]),
                     (int)Controls.CoreFormat.channel_placement.chan[i]);
        }
        break;

    default :
        return PreprocError;
    }

    return PreprocNoError;
}

/* @brief Gets supported controls values from the object */
PreprocStatus_t   Preproc_Audio_c::GetControl(stm_se_ctrl_t     Control, void   *Data)
{
    if (NULL == Data)
    {
        return PreprocError; // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
        if (Controls.DualMono.StreamDriven)
        {
            *(uint32_t *)Data = STM_SE_CTRL_VALUE_STREAM_DRIVEN_DUALMONO;
        }
        else
        {
            *(uint32_t *)Data = STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO;
        }

        break;

    case STM_SE_CTRL_DUALMONO:
        *(stm_se_dual_mode_t *)Data = Controls.DualMono.DualMode;
        break;

    default :
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t   Preproc_Audio_c::GetCompoundControl(stm_se_ctrl_t Control, void *Data)
{
    if (NULL == Data)
    {
        return PreprocError; // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT:
        ((stm_se_audio_core_format_t *)Data)->sample_rate       = Controls.CoreFormat.sample_rate;
        ((stm_se_audio_core_format_t *)Data)->channel_placement = Controls.CoreFormat.channel_placement;
        break;

    default :
        return PreprocError;
    }

    return PreprocNoError;
}

/* @brief saves the current parameters */
void Preproc_Audio_c::SaveCurrentParametersToPrevious()
{
    PreviousParameters.Controls      =  CurrentParameters.Controls;
    PreviousParameters.InputMetadata = *CurrentParameters.InputMetadata;
}

/**
 * @brief Checks whether CurrentInputBuffer and CurrentParameters can be processed
 *
 *
 * @return boolean true if they are supported by the object
 */
bool Preproc_Audio_c::AreCurrentControlsAndMetadataSupported()
{
    PreprocStatus_t Status = PreprocNoError;

    //Maximum number of samples at input
    uint32_t NumberOfSamplesInInputBuffer;
    Status = GetNumberSamples(&CurrentInputBuffer, &NumberOfSamplesInInputBuffer);
    if (PreprocNoError != Status)
    {
        SE_ERROR("Could not parse CurrentInputBuffer\n");
        return false;
    }

    if ((0 == NumberOfSamplesInInputBuffer) && !(PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS))
    {
        SE_INFO(group_encoder_audio_preproc, "Number of samples in this buffer is 0\n");
        return false;
    }

    if (PREPROC_AUDIO_IN_MAX_NB_SAMPLES < NumberOfSamplesInInputBuffer)
    {
        SE_ERROR("Number of samples in this buffer (%u) is larger than max supported (%u)",
                 NumberOfSamplesInInputBuffer, PREPROC_AUDIO_IN_MAX_NB_SAMPLES);
        return false;
    }

    //TimeStamps
    if ((TIME_FORMAT_27MHz != CurrentParameters.InputMetadata->native_time_format)
        && (TIME_FORMAT_PTS != CurrentParameters.InputMetadata->native_time_format)
        && (TIME_FORMAT_US   != CurrentParameters.InputMetadata->native_time_format))
    {
        SE_ERROR("Unknown Time Format %08X\n", CurrentParameters.InputMetadata->native_time_format);
        return false;
    }

    // SamplingFrequency Checks
    if (0 == CurrentParameters.InputMetadata->audio.core_format.sample_rate)
    {
        SE_ERROR("Input Sampling Frequency is 0\n");
        return false;
    }

    if (0 == CurrentParameters.Controls.CoreFormat.sample_rate)
    {
        SE_ERROR("Output Sampling Frequency is 0\n");
        return false;
    }

    if (CurrentParameters.Controls.CoreFormat.sample_rate
        > (CurrentParameters.InputMetadata->audio.core_format.sample_rate * PREPROC_AUDIO_MAX_UPSAMPLING_RATIO))
    {
        SE_ERROR("Upsampling ratio %u/%u is higher than max (%u)\n"
                 , CurrentParameters.InputMetadata->audio.core_format.sample_rate, CurrentParameters.Controls.CoreFormat.sample_rate
                 , PREPROC_AUDIO_MAX_UPSAMPLING_RATIO);
        return false;
    }

    // Additional check for discontinuity non-applicable
    // Open gop is already checked in base class, in case of new support for open gop, this check will be valid for audio
    if ((CurrentParameters.InputMetadata->discontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST)
        || (CurrentParameters.InputMetadata->discontinuity & STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST))
    {
        SE_ERROR("Discontinuity GOP request not applicable to audio\n");
        return false;
    }

    return true;
}

/**
 * @brief checks current controls, if they are set 'bypass' values, copy from current input metadata
 *
 */
void Preproc_Audio_c::CheckCurrentControlsForBypassValues()
{
    if (0 == CurrentParameters.Controls.CoreFormat.sample_rate)
    {
        SE_DEBUG(group_encoder_audio_preproc, "Using input sampling frequency %u for output\n",
                 CurrentParameters.InputMetadata->audio.core_format.sample_rate);
        CurrentParameters.Controls.CoreFormat.sample_rate  =  CurrentParameters.InputMetadata->audio.core_format.sample_rate;
    }

    if (0 == CurrentParameters.Controls.CoreFormat.channel_placement.channel_count)
    {
        CurrentParameters.Controls.CoreFormat.channel_placement = CurrentParameters.InputMetadata->audio.core_format.channel_placement;
    }
}
