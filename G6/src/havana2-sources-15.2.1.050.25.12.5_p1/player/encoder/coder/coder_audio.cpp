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

#include "coder_audio.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Audio_c"

static const CoderAudioDefaultBitRate_t gCoderAudioDefaultBitRateLut[] =
{
    {48000, ACC_MODE10,     128000},
    {48000, ACC_MODE_1p1,   192000},
    {48000, ACC_MODE20,     192000},
    {48000, ACC_MODE32,     448000},
    {48000, ACC_MODE32_LFE, 448000},
    {44100, ACC_MODE10,     128000},
    {44100, ACC_MODE20,     192000},
    {32000, ACC_MODE10,     128000},
    {32000, ACC_MODE20,     192000},
    {24000, ACC_MODE10,      64000},
    {24000, ACC_MODE20,      96000},
    {22050, ACC_MODE10,      64000},
    {22050, ACC_MODE20,      96000},
    {16000, ACC_MODE10,      64000},
    {16000, ACC_MODE20,      96000},
    {12000, ACC_MODE10,      32000},
    {12000, ACC_MODE20,      48000},
    {11025, ACC_MODE10,      32000},
    {11025, ACC_MODE20,      48000},
    { 8000, ACC_MODE10,      32000},
    { 8000, ACC_MODE20,      48000},
    {    0, ACC_MODE_ID,         0}

};

Coder_Audio_c::Coder_Audio_c(const CoderAudioStaticConfiguration_t &staticconfig,
                             const CoderBaseAudioMetadata_t &baseaudiometadata,
                             const CoderAudioControls_t &basecontrols)
    : Coder_Base_c()
    , mStaticConfiguration(staticconfig)
    , mCoderAudioStatistics()
    , mRelayfsIndex(0)
    , mPreviousEncodingParameters(baseaudiometadata, basecontrols)
    , mCurrentEncodingParameters(basecontrols)
    , mNextControls(basecontrols)
{
    SetGroupTrace(group_encoder_audio_coder);
    mRelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT);

    memset(&mCoderAudioStatistics, 0, sizeof(mCoderAudioStatistics));
    mCurrentEncodingParameters.InputMetadata = &mPreviousEncodingParameters.InputMetadata;
    mCurrentEncodingParameters.EncodeCoordinatorMetadata = &mPreviousEncodingParameters.EncodeCoordinatorMetadata;

    // set base class frame max size
    CodedFrameMaximumSize = mStaticConfiguration.MaxCodedFrameSize;
}

Coder_Audio_c::~Coder_Audio_c(void)
{
    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT, mRelayfsIndex);
}

CoderStatus_t   Coder_Audio_c::Halt(void)
{
    SE_DEBUG(group_encoder_audio_coder, "\n");

    CoderStatus_t Status = Coder_Base_c::Halt();

    // @todo check that Halt synchronizes the cores in base class
    OS_Smp_Mb();
    return Status;
}

void Coder_Audio_c::ReleaseMainCodedBuffer(void)
{
    if (NULL != CodedFrameBuffer) //Can't be null from where we call, but...
    {
        CodedFrameBuffer->DecrementReferenceCount();
    }
}

/**
 * @brief Does some basic setup from Buffer_t, does not encode, does not block
 */
CoderStatus_t Coder_Audio_c::Input(Buffer_t Buffer)
{
    __stm_se_frame_metadata_t *InternalMetaDataDescriptor;

    mCoderAudioStatistics.InputFrames++;
    SE_DEBUG(group_encoder_audio_coder, "[%d]\n", mCoderAudioStatistics.InputFrames);

    // Dump coder input buffer via st_relay
    Buffer->DumpViaRelay(ST_RELAY_TYPE_ENCODER_AUDIO_CODER_INPUT + mRelayfsIndex, ST_RELAY_SOURCE_SE);

    //First call the base class
    CoderStatus_t Status = Coder_Base_c::Input(Buffer);
    if (CoderNoError != Status)
    {
        CODER_ERROR_RUNNING("base input failed (%08X)\n", Status);
        return Status;
    }

    /* @note From this point on we have a CodedFrameBuffer with an attached InputBuffer from Coder_Base */
    /* Get a data reference only to fill statistics InputBytes */
    unsigned int  Size_B = 0;       //!< Size of paylaod
    void         *Address = NULL;      //!< Pointer to payload

    Buffer->ObtainDataReference(NULL, &Size_B, &Address, CachedAddress);
    if (Address == NULL)
    {
        SE_ERROR("Unable to obtain valid data reference\n");
        ReleaseMainCodedBuffer();
        return CoderInvalidInputBufferReference;
    }

    mCoderAudioStatistics.InputBytes += Size_B;

    /* Get Audio Frame Information */
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&InternalMetaDataDescriptor));
    SE_ASSERT(InternalMetaDataDescriptor != NULL);

    /* update CurrentEncodingParameters: metadata from buffer and user controls */
    mCurrentEncodingParameters.InputMetadata = &InternalMetaDataDescriptor->uncompressed_frame_metadata;
    mCurrentEncodingParameters.EncodeCoordinatorMetadata = &InternalMetaDataDescriptor->encode_coordinator_metadata;

    if ((0 == mNextControls.BitRateCtrl.bitrate) || (false == mNextControls.BitRateUpdated))
    {
        FillDefaultBitRate();
    }

    /* Latch the controls */
    mCurrentEncodingParameters.Controls = mNextControls;

    return CoderNoError;
}

void Coder_Audio_c::FillDefaultBitRate()
{
    enum eAccAcMode aAcMode;
    StmSeAudioChannelPlacementAnalysis_t Analysis;
    stm_se_audio_channel_placement_t SortedPlacement;
    const stm_se_audio_channel_placement_t *channel_placement = &mCurrentEncodingParameters.InputMetadata->audio.core_format.channel_placement;
    bool AudioModeIsPhysical;
    unsigned int aFrequency = mCurrentEncodingParameters.InputMetadata->audio.core_format.sample_rate;

    StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcMode, &AudioModeIsPhysical, &SortedPlacement, &Analysis, channel_placement);

    /* Loop till end of LUT, marked by 0 == Lut[ModeIdx].SampleRate */
    for (unsigned int ModeIdx = 0; 0 != gCoderAudioDefaultBitRateLut[ModeIdx].SampleRate; ModeIdx++)
    {
        if (aFrequency == gCoderAudioDefaultBitRateLut[ModeIdx].SampleRate)
        {
            if (aAcMode == gCoderAudioDefaultBitRateLut[ModeIdx].AudioCodingMode)
            {
                mNextControls.BitRateCtrl.bitrate =  gCoderAudioDefaultBitRateLut[ModeIdx].DefaultBitRate;
                mNextControls.BitRateUpdated = true;
                SE_DEBUG(group_encoder_audio_coder, "Setting default BitRate[%d] for SampleRate[%d] ChannelPlacement[%d]\n",
                         mNextControls.BitRateCtrl.bitrate, aFrequency, aAcMode);
                break;
            }
        }
    }

    if (false == mNextControls.BitRateUpdated)
    {
        SE_WARNING("Unknown SampleRate[%d] or ChannelPlacement[%d] keeping default bitrate to %d\n",
                   aFrequency, aAcMode, mNextControls.BitRateCtrl.bitrate);
    }
}

CoderStatus_t Coder_Audio_c::SendCodedBufferOut(Buffer_t Buffer, unsigned int Size)
{
    Buffer->SetUsedDataSize(Size);
    BufferStatus_t BufStatus = Buffer->ShrinkBuffer(Size);
    if (BufStatus != BufferNoError)
    {
        SE_DEBUG(group_encoder_audio_coder, "base buffer shrink failed\n");
        // keep going
    }

    CoderStatus_t CoderStatus = Coder_Base_c::Output(Buffer);
    if (CoderNoError == CoderStatus)
    {
        mCoderAudioStatistics.OutputBytes += Size;
        mCoderAudioStatistics.OutputFrames++;
    }
    else
    {
        SE_ERROR("base output failed\n");
        CoderStatus = CoderAssertLevelError;
    }

    return CoderStatus;
}

void Coder_Audio_c::InputPost()
{
    mPreviousEncodingParameters.Controls      = mCurrentEncodingParameters.Controls;
    mPreviousEncodingParameters.InputMetadata = *mCurrentEncodingParameters.InputMetadata;
    mPreviousEncodingParameters.EncodeCoordinatorMetadata = *mCurrentEncodingParameters.EncodeCoordinatorMetadata;

    mCoderAudioStatistics.InputFramesOk++;
    SE_DEBUG(group_encoder_audio_coder, "[%d]\n", mCoderAudioStatistics.InputFramesOk);
}

/* Pointer Null has been checked by parent class */
CoderStatus_t Coder_Audio_c::SetControl(stm_se_ctrl_t Control, const void *Data)
{
    CoderStatus_t Status = CoderNoError;

    if (NULL == Data)
    {
        return CoderError;  // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL:
        SE_DEBUG(group_encoder_audio_coder, "prog level:%d\n", *(uint32_t *)Data);
        // not implemented yet - TODO
        SE_INFO(group_encoder_audio_coder, "prog level not yet supported\n");
        break;

    case STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE:
        switch (*(uint32_t *)Data)
        {
        case STM_SE_CTRL_VALUE_DISAPPLY:
            SE_DEBUG(group_encoder_audio_coder, "crc off\n");
            mNextControls.CrcOn = false;
            break;
        case STM_SE_CTRL_VALUE_APPLY:
            SE_DEBUG(group_encoder_audio_coder, "crc on\n");
            mNextControls.CrcOn = true;
            break;
        default:
            SE_ERROR("Undefined crc enable value:%d\n", *(uint32_t *)Data);
            Status = CoderError;
            break;
        }
        break;

    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS:
    {
        stm_se_audio_scms_t *ScmsInfo = (stm_se_audio_scms_t *)Data;
        switch (*ScmsInfo)
        {
        case STM_SE_AUDIO_NO_COPYRIGHT:
        {
            mNextControls.StreamIsCopyrighted = false;
            mNextControls.OneCopyIsAuthorized = true;
            SE_DEBUG(group_encoder_audio_coder, "scms no copyright\n");
        }
        break;

        case STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED:
        {
            mNextControls.StreamIsCopyrighted = true;
            mNextControls.OneCopyIsAuthorized = true;
            SE_DEBUG(group_encoder_audio_coder, "scms one more copy\n");
        }
        break;

        case STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED:
        {
            mNextControls.StreamIsCopyrighted = true;
            mNextControls.OneCopyIsAuthorized = false;
            SE_DEBUG(group_encoder_audio_coder, "scms no further copy\n");
        }
        break;

        default:
            SE_ERROR("Undefined scms value:%d\n", *(uint32_t *)Data);
            Status = CoderError;
            break;
        }
    }
    break;

    default :
        Status = CoderError;
        break;
    }

    return Status;
}

/* Pointer Null has been checked by parent class */
CoderStatus_t Coder_Audio_c::SetCompoundControl(stm_se_ctrl_t Control, const void *Data)
{
    CoderStatus_t Status = CoderNoError;

    if (NULL == Data)
    {
        return CoderError;  // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL:
        mNextControls.BitRateCtrl = *(stm_se_audio_bitrate_control_t *)Data;
        if (mNextControls.BitRateCtrl.is_vbr)
        {
            SE_DEBUG(group_encoder_audio_coder, "VBR mode, quality factor:%d, cap:%d\n",
                     mNextControls.BitRateCtrl.vbr_quality_factor, mNextControls.BitRateCtrl.bitrate_cap);
        }
        else
        {
            SE_DEBUG(group_encoder_audio_coder, "CBR/ABR mode, target bitrate:%d, cap:%d\n",
                     mNextControls.BitRateCtrl.bitrate, mNextControls.BitRateCtrl.bitrate_cap);
        }
        mNextControls.BitRateUpdated = true;
        break;

    default :
        Status = CoderError;
        break;
    }

    return Status;
}

/* Pointer Null has been checked by parrent class */
CoderStatus_t   Coder_Audio_c::GetControl(stm_se_ctrl_t Control, void   *Data)
{
    CoderStatus_t Status = CoderNoError;

    if (NULL == Data)
    {
        return CoderError; // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_PROGRAM_LEVEL:
        // not implemented yet
        break;

    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS:
    {
        stm_se_audio_scms_t *ScmsInfo = (stm_se_audio_scms_t *)Data;

        if (!mNextControls.StreamIsCopyrighted)
        {
            *ScmsInfo = STM_SE_AUDIO_NO_COPYRIGHT;
            //We treat (false, false) as valid, STM_SE_AUDIO_NO_COPYRIGHT
        }
        else
        {
            if (mNextControls.OneCopyIsAuthorized)
            {
                *ScmsInfo = STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED;
            }
            else
            {
                *ScmsInfo = STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED;
            }
        }
    }
    break;

    case STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE:
    {
        *(uint32_t *)Data = mNextControls.CrcOn ? STM_SE_CTRL_VALUE_APPLY : STM_SE_CTRL_VALUE_DISAPPLY;
    }
    break;

    default : Status = CoderError;
    }

    return Status;
}

CoderStatus_t   Coder_Audio_c::GetCompoundControl(stm_se_ctrl_t Control, void   *Data)
{
    CoderStatus_t Status = CoderNoError;

    if (NULL == Data)
    {
        return CoderError; // no error warning: due to delegate model
    }

    switch (Control)
    {
    case STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL:
        *(stm_se_audio_bitrate_control_t *)Data = mNextControls.BitRateCtrl;
        break;

    default : Status = CoderError;
    }

    return Status;
}
