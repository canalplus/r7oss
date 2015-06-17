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

#include "preproc_audio_mme.h"
#include "audio_conversions.h"
#include "audio_conversions.h"

//! Enables re-init of MTPcmProcessing upon change to mono detection
#define B39714_WA_ENABLED 1

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Audio_Mme_c"

#define BUFFER_MME_AUDIO_CONTEXT       "PreprocMmeAudioContext"
#define BUFFER_MME_AUDIO_CONTEXT_TYPE  {BUFFER_MME_AUDIO_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(PreprocAudioMmeCommandContext_t)}

static BufferDataDescriptor_t PreprocMmeAudioContextDescriptor = BUFFER_MME_AUDIO_CONTEXT_TYPE;
//                             [    CMC                   DMIX                  CH-REMAP   ]
#define REQUIRED_PRE_PROCESSOR (1ULL<<(ACC_DMIX) |  1ULL<<(ACC_MIXING) | 1ULL<<(ACC_CHREMAP))
// Note : No capability return by the FW for Resamplex so not added in the above list

//! multicom callback function type
typedef void (*MME_GenericCallback_t)(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData);
/**
 * @brief C Wrapper function for multicom callback
 * requires the actual callback function to be public
 *
 * @param Event
 * @param CallbackData
 * @param UserData
 */
static void PreProcAudioMMECallbackStub(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData)
{
    if (NULL != UserData)
    {
        Preproc_Audio_Mme_c *Self = (Preproc_Audio_Mme_c *)UserData;
        Self->CallbackFromMME(Event, CallbackData);
    }
    else
    {
        SE_ERROR("UserData is NULL\n");
    }
}

/* @brief creates a Preproc_Audio_Mme_c, at ready */
Preproc_Audio_Mme_c::Preproc_Audio_Mme_c(void)
    : Preproc_Audio_c()
    , TransformerInitContext()
    , MaxPreprocBufferSize(0)
    , UpdatedControls(false)
    , InputExtrapolation()
    , AudioPreProcTransformerNames()
    , IsLowPowerState(false)
    , IsLowPowerMMEInitialized(false)
    , InputCurrentMmeCommandContext()
    , ApplyEOS(false)
    , PreprocDiscontinuityForwarded(false)
    , mAcmodOutput(ACC_MODE_ID)
    , LoopManagement()
    , AcquireAllMmeCommandContextsLock()
{
    //Use default extrapolation resolution (usec)
    InputExtrapolation.ResetExtrapolator();

    AudioPreProcTransformerNames[0] = PP_MT_NAME"_a0";
    AudioPreProcTransformerNames[1] = PP_MT_NAME"_a1";

    OS_InitializeMutex(&AcquireAllMmeCommandContextsLock);

    // base class updates
    unsigned int maxnrpreprocbuffers  = GetMaxNrPreprocBuffers();
    MaxPreprocBufferSize = GetMaxPreprocBufferSize();
    SetFrameMemory(MaxPreprocBufferSize, maxnrpreprocbuffers);

    SE_DEBUG(group_encoder_audio_preproc, "Set %u PreprocBuffers of %u bytes ==> %u kB for allocation\n",
             maxnrpreprocbuffers, MaxPreprocBufferSize,
             (maxnrpreprocbuffers * MaxPreprocBufferSize) / 1024);

    // BufferPoll for Transform
    Configuration.PreprocContextCount      = PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH;
    Configuration.PreprocContextDescriptor = &PreprocMmeAudioContextDescriptor;
}

/* @brief releases any allocation remaining after Halt */
Preproc_Audio_Mme_c::~Preproc_Audio_Mme_c(void)
{
    // TODO(pht) shall make sure all resources freed : dont rely on Halt being call beforehand

    OS_TerminateMutex(&AcquireAllMmeCommandContextsLock);
}

/* Stops the object and cleans in a waiting state */
PreprocStatus_t Preproc_Audio_Mme_c::Halt(void)
{
    PreprocStatus_t Status = PreprocNoError;
    SE_DEBUG(group_encoder_audio_preproc, "\n");

    // Call parent first
    if (PreprocNoError != Preproc_Audio_c::Halt())
    {
        SE_ERROR("Halt failed\n"); // continue
        Status = PreprocError;
    }

    // Then Specifics
    /* Wait for all commands to complete before termination */
    Buffer_t AllMmeContexts[PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH];
    bool GotAllMmeContexts;

    /* Do not give up if Not Running */
    Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts, false);
    if (PreprocNoError != Status)
    {
        SE_ERROR("AcquireAllMmeCommandContexts Failed: %08X\n", Status);
        // If we reach here with Error we are in trouble, yet we continue and attempt Terminate
    }

    Status = TerminateMmeTransformer();
    if (PreprocNoError != Status)
    {
        SE_ERROR("TerminateTransformer failed: %08X\n", Status); // continue
    }

    if (GotAllMmeContexts)
    {
        Status = ReleaseAllMmeCommandContexts(AllMmeContexts);

        if (PreprocNoError != Status)
        {
            SE_ERROR("ReleaseAllCodedContext Failed: %08X\n", Status);
        }
    }
    else
    {
        // If we failed to get all mme contexts and terminated ok, we still report error
        Status = PreprocError;
    }

    return Status;
}

void Preproc_Audio_Mme_c::InitializeLoopManagement(void)
{
    memset(&LoopManagement, 0, sizeof(LoopManagement));

    LoopManagement.RemainingIterations = 1;

    // these will not fail, as parameters have been checked prior
    GetNumberBytesPerSample(&CurrentInputBuffer, &LoopManagement.NrBytesPerSample);
    GetNumberSamples(&CurrentInputBuffer, &LoopManagement.TotalNrSamples);
    GetNumberAllocatedChannels(&CurrentInputBuffer, &LoopManagement.Channels);

    // increment the number of iterations till output number of samples always fits in PREPROC_AUDIO_OUT_MAX_NB_SAMPLES.
    // the last loop is NSample/Loops + NSample%Loops
    if (CurrentParameters.Controls.CoreFormat.sample_rate > CurrentParameters.InputMetadata->audio.core_format.sample_rate)
    {
        while ((((LoopManagement.TotalNrSamples / LoopManagement.RemainingIterations) +
                 (LoopManagement.TotalNrSamples % LoopManagement.RemainingIterations)) * CurrentParameters.Controls.CoreFormat.sample_rate)
               > ((PREPROC_AUDIO_OUT_MAX_NB_SAMPLES - PREPROC_AUDIO_MME_SFC_OUTPUT_BUFFER_SAFETY_SMP) * CurrentParameters.InputMetadata->audio.core_format.sample_rate))
        {
            LoopManagement.RemainingIterations++;
        }
    }

    // Set non 0 Iteration Parameters
    {
        LoopManagement.ThisIteration.NSamples = LoopManagement.TotalNrSamples / LoopManagement.RemainingIterations;
        LoopManagement.ThisIteration.SizeB    = LoopManagement.ThisIteration.NSamples * LoopManagement.NrBytesPerSample * LoopManagement.Channels;
    }
    ApplyEOS = (LoopManagement.ThisIteration.NSamples == 0);
    PreprocDiscontinuityForwarded = false;
}

PreprocStatus_t Preproc_Audio_Mme_c::Input(Buffer_t Buffer)
{
    PreprocStatus_t        Status = PreprocNoError;
    stm_se_discontinuity_t GeneratePreprocDiscontinuity;
    UpdatedControls = false;
    SE_DEBUG(group_encoder_audio_preproc, "\n");

    // Check if not in low power state
    if (IsLowPowerState)
    {
        SE_ERROR("SE device is in low power\n");
        return PreprocError;
    }

    // Call the parent implementation
    Status = Preproc_Audio_c::Input(Buffer);
    if (PreprocNoError != Status)
    {
        return PREPROC_STATUS_RUNNING(Status);
    }

    /* Refs: PreprocFrameBuffer{InputBuffer} */
    /* Use ReleaseMainPreprocFrameBuffer() to release PreprocFrameBuffer and attached */

    /* Check discontinuity applicable to audio */
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST)
        || (PreprocDiscontinuity & STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST))
    {
        SE_ERROR("Discontinuity GOP request not applicable to audio\n");
        ReleaseMainPreprocFrameBuffer();
        return EncoderNotSupported;
    }
    // Generate Buffer Discontinuity if Buffer Discontinuity detected except EOS, STM_SE_DISCONTINUITY_MUTE, STM_SE_DISCONTINUITY_FADEOUT and STM_SE_DISCONTINUITY_FADEIN.
    GeneratePreprocDiscontinuity = PreprocDiscontinuity;
    GeneratePreprocDiscontinuity = (stm_se_discontinuity_t)(GeneratePreprocDiscontinuity & ~STM_SE_DISCONTINUITY_MUTE);
    GeneratePreprocDiscontinuity = (stm_se_discontinuity_t)(GeneratePreprocDiscontinuity & ~STM_SE_DISCONTINUITY_FADEOUT);
    GeneratePreprocDiscontinuity = (stm_se_discontinuity_t)(GeneratePreprocDiscontinuity & ~STM_SE_DISCONTINUITY_FADEIN);
    GeneratePreprocDiscontinuity = (stm_se_discontinuity_t)(GeneratePreprocDiscontinuity & ~STM_SE_DISCONTINUITY_EOS);
    if (GeneratePreprocDiscontinuity)
    {
        // reset extrapolation
        InputExtrapolation.ResetExtrapolator();

        // Insert Buffer Discontinuity
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("GenerateBufferDiscontinuity Failed (%08X)\n", Status);
            ReleaseMainPreprocFrameBuffer();
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Silently discard frame for null buffer input
        if (InputBufferSize == 0)
        {
            ReleaseMainPreprocFrameBuffer();
            return PreprocNoError;
        }
    }

    // Initializes the transformer if required
    if (NULL == TransformerInitContext.hTransformer)
    {
        SE_DEBUG(group_encoder_audio_preproc, "initializing the transformer\n");

        Status = InitMmeTransformer();
        if (PreprocNoError != Status)
        {
            SE_ERROR("Failed to Init the transformer on the fly\n");
            ReleaseMainPreprocFrameBuffer();
            return PreprocError;
        }
    }

    /* Blocking: Compare Previous and Current setting and if necessary send GlobalParams update */
    Status = HandleChangeDetection();
    if (PreprocNoError != Status)
    {
        SE_ERROR("HandleChangeDetection Failed\n");
        /*HandleChangeDetection() takes care of clean-up*/
        return Status;
    }

    /* Actual Processing*/

    // check buffers
    if (NULL == CurrentInputBuffer.CachedAddress)
    {
        SE_ERROR("Input data buffer reference is NULL\n");
        ReleaseMainPreprocFrameBuffer();
        return PreprocError;
    }

    if (NULL == MainOutputBuffer.CachedAddress)
    {
        SE_ERROR("preproc buffer reference is NULL\n");
        ReleaseMainPreprocFrameBuffer();
        return PreprocError;
    }

    InitializeLoopManagement();

    //Loop to handle more that PREPROC_AUDIO_OUT_MAX_NB_SAMPLES samples
    while (0 < LoopManagement.RemainingIterations)
    {
        // Blocking : Get the next free transform command
        Status = GetNextFreeCommandContext();
        if (PreprocNoError != Status)
        {
            ReleaseMainPreprocFrameBuffer();
            return PreprocError;
        }
        /* Refs: MmeContext, PreprocFrameBuffer{InputBuffer} */


        /* Attach the PreprocOutputBuffer to the PreprocContextBuffer. It will be put in to the buffer ring during callback */
        InputCurrentMmeCommandContext->PreprocContextBuffer->AttachBuffer(MainOutputBuffer.Buffer);

        /* Refs: PreprocFrameBuffer{InputBuffer}, MmeContext{PreprocFrameBuffer{InputBuffer}} */

        //Fill transform Command Context
        FillTransformCommandContext();

        /* bz 225952 mme command modification */
        Status = Bz25952MmeCommandOverride();
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("Bz25952MmeCommandOverride Failed (%08X)\n", Status);
            ReleaseTransformContext(InputCurrentMmeCommandContext);
            InputCurrentMmeCommandContext = NULL;
            ReleaseMainPreprocFrameBuffer();
            return PREPROC_STATUS_RUNNING(Status);
        }
        /* Refs: PreprocFrameBuffer{InputBuffer}, MmeContext{PreprocFrameBuffer{InputBuffer}, ...} */

        //Send Command
        Status = SendCurrentCommand();
        if (PreprocNoError != Status)
        {
            /* Reminder Refs: PreprocFrameBuffer{InputBuffer}, MmeContext{PreprocFrameBuffer{InputBuffer}, ...} */
            ReleaseTransformContext(InputCurrentMmeCommandContext);
            InputCurrentMmeCommandContext = NULL;
            ReleaseMainPreprocFrameBuffer();
            return Status;
        }

        Status = HandleEndOfLoop(Buffer);
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("HandleEndOfLoop Failed (%08X)\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }
    } //End of loop to handle more than PREPROC_AUDIO_OUT_MAX_NB_SAMPLES samples at output

    // @note: from here all resources will be freed from Callback context

    // Wait for all MME command to complete before exit or discontinuity insertion
    Buffer_t AllMmeContexts[PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH];
    bool GotAllMmeContexts;
    Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);
    if (!GotAllMmeContexts)
    {
        SE_INFO(group_encoder_audio_preproc, "AcquireAllMmeCommandContexts did not get AllMmeContexts\n");
    }
    if (!GotAllMmeContexts)
    {
        if (PreprocNoError != Status)
        {
            SE_ERROR("AcquireAllMmeCommandContexts Failure\n");
            Status = PreprocError;
        }
        else
        {
            SE_INFO(group_encoder_audio_preproc, "AcquireAllMmeCommandContexts Aborted\n");
            Status = PreprocNoError; /* @todo: question: report error instead? */
        }

        return Status;
    }

    InputPost(Status);

    // Handling EOS generation post data processing
    // We need this because if we forward the EOS flag from callback for the last output but non zero then coder complain that discontinuity with non zero input.
    // So in that case we just put the last non zero buffer from callback without setting EOS flag. And in last we issue an empty buffer with EOS tag.
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (PreprocDiscontinuityForwarded == false))
    {
        // reset extrapolation
        InputExtrapolation.ResetExtrapolator();

        // Insert Buffer EOS
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("GenerateBufferDiscontinuity Failed (%08X)\n", Status);

            if (GotAllMmeContexts)  //For safety against code modifications
            {
                ReleaseAllMmeCommandContexts(AllMmeContexts);
            }
            return PREPROC_STATUS_RUNNING(Status);
        }
    }

    if (GotAllMmeContexts)  //For safety only
    {
        Status = ReleaseAllMmeCommandContexts(AllMmeContexts);
        if (PreprocNoError != Status)
        {
            SE_ERROR("ReleaseAllCodedContext Failed (%08X)\n", Status);
        }
    }

    return Status;
}

PreprocStatus_t Preproc_Audio_Mme_c::HandleEndOfLoop(Buffer_t Buffer)
{
    PreprocStatus_t Status = PreprocNoError;
    /*  End of Loop invariant: if more  iteration required need to get new output
    buffer, attach the input buffer to it, etc */
    LoopManagement.SamplesSentPrior += LoopManagement.ThisIteration.NSamples;
    LoopManagement.RemainingIterations--;

    // For EOS add one more loop with zero input buffer so that FW flush its delay line
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (LoopManagement.RemainingIterations == 0) && (ApplyEOS == false))
    {
        ApplyEOS = true;
        LoopManagement.RemainingIterations = 1;
    }

    if (0 != LoopManagement.RemainingIterations)
    {
        /* 1/ Whatever was done by Preproc_Base_c::Input() for the PreprocFrameBuffer */
        // Get a New 'PreprocFrameBuffer'
        // TODO(pht) check call to ReleaseMainPreprocFrameBuffer was done
        Status = GetNewBuffer(&PreprocFrameBuffer);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("LoopEnd - Unable to get new buffer\n");
            return Status;
        }

        // Retrieve preproc metadata
        PreprocFrameBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreprocFullMetaDataDescriptor));
        SE_ASSERT(PreprocFullMetaDataDescriptor != NULL);

        // Initialize preproc meta data
        PreprocMetaDataDescriptor = &PreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
        memcpy(PreprocMetaDataDescriptor, InputMetaDataDescriptor, sizeof(stm_se_uncompressed_frame_metadata_t));
        memcpy(&PreprocFullMetaDataDescriptor->encode_coordinator_metadata, EncodeCoordinatorMetaDataDescriptor, sizeof(__stm_se_encode_coordinator_metadata_t));

        // Attach the Input buffer to the PreprocFrameBuffer so that it is kept until Preprocessing is complete.
        PreprocFrameBuffer->AttachBuffer(Buffer);

        /* 2/ What was done by Preproc_Audio_c::Input() for the PreprocFrameBuffer */
        //Parse PreprocFrameBuffer
        Status = FillPreprocAudioBufferFromPreProcBuffer(&MainOutputBuffer, PreprocFrameBuffer);
        if (PreprocNoError != Status)
        {
            SE_ERROR("PreprocFrameBuffer Not a valid buffer\n");
            ReleaseMainPreprocFrameBuffer();
            return Status;
        }

        //Update next Loop Iteration Parameters
        if (ApplyEOS == false)
        {
            LoopManagement.ThisIteration.NSamples = LoopManagement.TotalNrSamples - LoopManagement.SamplesSentPrior;
            LoopManagement.ThisIteration.NSamples = LoopManagement.ThisIteration.NSamples / LoopManagement.RemainingIterations;
            LoopManagement.ThisIteration.OffsetB  = LoopManagement.SamplesSentPrior * LoopManagement.NrBytesPerSample * LoopManagement.Channels;
            LoopManagement.ThisIteration.SizeB    = LoopManagement.ThisIteration.NSamples * LoopManagement.NrBytesPerSample * LoopManagement.Channels;
        }
        else
        {
            LoopManagement.ThisIteration.NSamples = 0;
            LoopManagement.ThisIteration.OffsetB  = 0;
            LoopManagement.ThisIteration.SizeB    = 0;
        }
    }

    return Status;
}

PreprocStatus_t Preproc_Audio_Mme_c::InitMmeTransformer(void)
{
    PreprocStatus_t Status = PreprocNoError;
    bool CompanionIsCapable;
    uint32_t SelectedCpu;

    if (NULL != TransformerInitContext.hTransformer)
    {
        SE_ERROR("Transformer Handle is not NULL\n");
        return PreprocError;
    }

    //Check State before
    if (!TestComponentState(ComponentRunning))
    {
        SE_DEBUG(group_encoder_audio_preproc, "ComponentNotRunning: abort\n");
        return PreprocError;
    }

    // Update Multicom Context from Current Parameters
    Status = FillInitParamsFromCurrentParameters();
    if (PreprocNoError != Status)
    {
        SE_ERROR("Current Parameters are not compatible\n");
        return PreprocError;
    }

    //Initialize the transformer
    MME_ERROR MStatus = MME_SUCCESS;
    CompanionIsCapable = IsPreProcSupportedByCompanion(SelectedCpu);
    if (CompanionIsCapable == false)
    {
        SE_ERROR("Loaded AudioFW binary on all CPU does not support some preproc\n");
        return PreprocError;
    }
    else
    {
        SE_INFO(group_encoder_audio_preproc, "Initiating preproc on CPU:%d\n", SelectedCpu);
    }

    MStatus = MME_InitTransformer(AudioPreProcTransformerNames[SelectedCpu], &TransformerInitContext.InitParams, &TransformerInitContext.hTransformer);
    if (MStatus != MME_SUCCESS)
    {
        SE_ERROR("MME_InitTransformer failed with %08X\n", MStatus);
        TransformerInitContext.hTransformer = NULL;
        return PreprocError;
    }

    return Status;
}

PreprocStatus_t Preproc_Audio_Mme_c::TerminateMmeTransformer()
{
    PreprocStatus_t Status = PreprocNoError;

    if (NULL == TransformerInitContext.hTransformer)
    {
        SE_DEBUG(group_encoder_audio_preproc, "Transformer is not started\n");
        return PreprocNoError;
    }

    SE_DEBUG(group_encoder_audio_preproc, "Terminating Transformer\n");
    MME_ERROR MStatus = MME_SUCCESS;
    MStatus = MME_TermTransformer(TransformerInitContext.hTransformer);
    if (MStatus != MME_SUCCESS)
    {
        SE_ERROR("MME_TermTransformer failed with %08X\n", MStatus);
        Status = PreprocError;
        //Fall-Through
    }

    TransformerInitContext.hTransformer = NULL;
    SE_DEBUG(group_encoder_audio_preproc, "Terminated Transformer\n");
    return Status;
}

/**
 * @brief Checks whether CurrentInputBuffer and CurrentParameters can be processed
 *
 *
 * @return boolean true if they are supported by the object
 */
bool Preproc_Audio_Mme_c::AreCurrentControlsAndMetadataSupported()
{
    // First call parent
    if (false == Preproc_Audio_c::AreCurrentControlsAndMetadataSupported())
    {
        SE_ERROR("Failed\n");
        return false;
    }

    //Then MME Implementation Specific
    // Check the Input PCM Format
    // For the moment not all are supported
    enum eAccLpcmWs LpcmWs;

    if (0 != StmSeAudioGetLpcmWsFromLPcmFormat(&LpcmWs, CurrentParameters.InputMetadata->audio.sample_format))
    {
        SE_ERROR("Input PCM format is not supported\n");
        return false;
    }

    //Check Channel Mappings
    enum eAccAcMode aAcModeIn, aAcModeOut;
    {
        //Check Input Channel configuration
        StmSeAudioChannelPlacementAnalysis_t Analysis;
        stm_se_audio_channel_placement_t SortedPlacement;
        bool AudioModeIsPhysical;

        if ((0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcModeIn, &AudioModeIsPhysical,
                                                                    &SortedPlacement, &Analysis,
                                                                    &CurrentParameters.InputMetadata->audio.core_format.channel_placement))
            || (ACC_MODE_ID == aAcModeIn))
        {
            SE_ERROR("Intput metadata channel routing is not supported\n");
            return false;
        }

        // Check the Input buffer width is not more than the max
        if (PREPROC_AUDIO_MME_MAX_NUMBER_OF_CHANNEL_IN <  CurrentParameters.InputMetadata->audio.core_format.channel_placement.channel_count)
        {
            SE_ERROR("Input buffer width (%d) is more than the max (%d)\n",
                     CurrentParameters.InputMetadata->audio.core_format.channel_placement.channel_count,
                     PREPROC_AUDIO_MME_MAX_NUMBER_OF_CHANNEL_IN);
            return false;
        }

        //Check Output Channel configuration
        if ((0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcModeOut, &AudioModeIsPhysical,
                                                                    &SortedPlacement, &Analysis,
                                                                    &CurrentParameters.Controls.CoreFormat.channel_placement))
            || (ACC_MODE_ID == aAcModeOut))
        {
            SE_ERROR("Requested output control channel routing is not supported\n");
            return false;
        }

        // Checks output width is not more than allocated
        if (PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT < Analysis.ActiveChannelCount)
        {
            SE_ERROR("Requested Output channel requires buffer width (%d) more than allocated (%d)\n",
                     Analysis.ActiveChannelCount, PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT);
            return false;
        }

        /// Checking forced dual-mono process on non stereo stream
        if (((STM_SE_CTRL_VALUE_USER_ENFORCED_DUALMONO == CurrentParameters.Controls.DualMono.StreamDriven)
             && ((STM_SE_DUAL_LEFT_OUT == CurrentParameters.Controls.DualMono.DualMode)
                 || (STM_SE_DUAL_RIGHT_OUT == CurrentParameters.Controls.DualMono.DualMode)))
            && ((ACC_MODE_1p1 != aAcModeIn) && (ACC_MODE20 != aAcModeIn) && (ACC_MODE20t != aAcModeIn) && (ACC_HDMI_MODE20 != aAcModeIn)))

        {
            SE_ERROR("DualLeft or DualRight can't be forced on non stereo stream AcMode[%d]\n", aAcModeIn);
            return false;
        }

    }
    // Check Frequencies
    enum eAccFsCode aAccFreqCode;

    if (0 != StmSeTranslateIsoSamplingFrequencyToDiscrete(CurrentParameters.InputMetadata->audio.core_format.sample_rate, aAccFreqCode))
    {
        SE_ERROR("Input Sampling Frequency %u is not supported\n", CurrentParameters.InputMetadata->audio.core_format.sample_rate);
        return false;
    }

    if (0 != StmSeTranslateIsoSamplingFrequencyToDiscrete(CurrentParameters.Controls.CoreFormat.sample_rate, aAccFreqCode))
    {
        SE_ERROR("Output Sampling Frequency %u is not supported\n", CurrentParameters.Controls.CoreFormat.sample_rate);
        return false;
    }

    return true;
}

/**
 * @brief Checks Capability of the AudioFW on the selected CPU.
 *
 *
 * @return Supported == true if CPU has the capability on SelectedCpu.
 */
bool Preproc_Audio_Mme_c::CheckCompanionCapability(uint32_t SelectedCpu)
{
    MME_LxPcmProcessingInfo_t   PcmProcInfo;
    MME_TransformerCapability_t MMECapability;
    uint64_t                    CapabilityMask = 0;
    uint64_t                    FwCapability   = 0;
    bool Supported = false;
    MMECapability.StructSize          = sizeof(MME_TransformerCapability_t);
    MMECapability.TransformerInfoSize = sizeof(MME_LxPcmProcessingInfo_t);
    MMECapability.TransformerInfo_p   = &PcmProcInfo;
    CapabilityMask = REQUIRED_PRE_PROCESSOR;

    if (MME_SUCCESS != MME_GetTransformerCapability(AudioPreProcTransformerNames[SelectedCpu], &MMECapability))
    {
        SE_DEBUG(group_encoder_audio_preproc, "Failed to get audioFW preprocessor capability on CPU:%d", SelectedCpu);
    }
    else
    {
        FwCapability = uint64_t(PcmProcInfo.PcmProcessorCapabilityFlags[0] | ((uint64_t)PcmProcInfo.PcmProcessorCapabilityFlags[1] << 32));
        FwCapability = FwCapability & CapabilityMask;
        Supported    = (FwCapability == CapabilityMask);
    }

    return Supported;
}

/**
 * @brief Checks Capability of the AudioFW on the selected CPU.
 *        If capability not found on the selected CPU then check all other CPUs
 *
 * @return Supported == true if any of the CPU has the capability on SelectedCpu.
 */
bool Preproc_Audio_Mme_c::IsPreProcSupportedByCompanion(uint32_t &SelectedCpu)
{
    bool Supported = false;
    SelectedCpu =  Encoder.EncodeStream->AudioEncodeNo;
    /* First check the selected CPU for the capability */
    Supported = CheckCompanionCapability(SelectedCpu);

    /* If capability not found on the selected CPU then check other CPUs for the capability */
    if (Supported == false)
    {
        SelectedCpu = 0;

        while (SelectedCpu < ENCODER_STREAM_AUDIO_MAX_CPU)
        {
            if (SelectedCpu != Encoder.EncodeStream->AudioEncodeNo) // It is already checked
            {
                Supported = CheckCompanionCapability(SelectedCpu);
            }

            if (Supported == true)
            {
                break;
            }

            SelectedCpu++;
        }
    }

    return Supported;
}

PreprocStatus_t Preproc_Audio_Mme_c::FillInitParamsFromCurrentParameters()
{
    memset(&TransformerInitContext.InitParams, 0, sizeof(MME_TransformerInitParams_t));

    if (PreprocNoError != FillGlobalParamsFromCurrentParameters(&TransformerInitContext.PcmProcessingInitParams.GlobalParams))
    {
        return PreprocError;
    }

    TransformerInitContext.PcmProcessingInitParams.StructSize = sizeof(MME_LxPcmProcessingInitParams_t); // Generic Size
    //Adaptation to subset params size
    TransformerInitContext.PcmProcessingInitParams.StructSize -= PreprocAudioGlobalParamSubsetSizeAdjust;
    TransformerInitContext.PcmProcessingInitParams.CacheFlush  = ACC_MME_ENABLED;
    TransformerInitContext.PcmProcessingInitParams.BlockWise   = 0;
    TransformerInitContext.PcmProcessingInitParams.SfreqRange  = ACC_FSRANGE_48k;
    TransformerInitContext.PcmProcessingInitParams.NChans[ACC_MIX_MAIN] = PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT;
    TransformerInitContext.PcmProcessingInitParams.NChans[ACC_MIX_AUX]  = 0;
    TransformerInitContext.InitParams.StructSize  = sizeof(MME_TransformerInitParams_t);
    TransformerInitContext.InitParams.Priority  = PREPROC_AUDIO_MME_DEFAULT_MME_PRIORITY;
    TransformerInitContext.InitParams.Callback = PreProcAudioMMECallbackStub;
    TransformerInitContext.InitParams.CallbackUserData = (void *)this;
    TransformerInitContext.InitParams.TransformerInitParamsSize = TransformerInitContext.PcmProcessingInitParams.StructSize;
    TransformerInitContext.InitParams.TransformerInitParams_p   = (MME_GenericParams_t *)&TransformerInitContext.PcmProcessingInitParams;
    DumpInitParams(&TransformerInitContext.InitParams);

    return PreprocNoError;
}


PreprocStatus_t Preproc_Audio_Mme_c::ModifyInputRouteForDualMono(stm_se_audio_channel_placement_t &InChannelPlacementCopy,
                                                                 bool &AudioModeIsPhysical,
                                                                 enum eAccAcMode &AcmodInput,
                                                                 enum eAccAcMode AcmodOutput)
{
    /// If Output is mono and dual-mono process
    if (((ACC_MODE10 == AcmodOutput)
         && ((!CurrentParameters.Controls.DualMono.StreamDriven) || (ACC_MODE_1p1 == AcmodInput)))
       )
    {
        SE_DEBUG(group_encoder_audio_preproc, "DualMono: Overwrite channel Map\n");
        bool ForceRightAsMonoC = false;
        bool ForceLeftAsMonoC = false;

        if (ACC_MODE10 == AcmodOutput)
        {
            if (STM_SE_DUAL_RIGHT_OUT == CurrentParameters.Controls.DualMono.DualMode)
            {
                ForceRightAsMonoC = true;
            }

            if (STM_SE_DUAL_LEFT_OUT == CurrentParameters.Controls.DualMono.DualMode)
            {
                ForceLeftAsMonoC = true;
            }
        }

        for (uint32_t chan = 0; chan < InChannelPlacementCopy.channel_count; chan++)
        {
            stm_se_audio_channel_id_t ChannelId = (stm_se_audio_channel_id_t)(InChannelPlacementCopy.chan[chan]);

            switch (ChannelId)
            {
            case STM_SE_AUDIO_CHAN_L          : //falthrough
            case STM_SE_AUDIO_CHAN_L_DUALMONO :
                if (ForceRightAsMonoC)
                {
                    InChannelPlacementCopy.chan[chan] = STM_SE_AUDIO_CHAN_STUFFING;
                }

                if (ForceLeftAsMonoC)
                {
                    InChannelPlacementCopy.chan[chan] = STM_SE_AUDIO_CHAN_C;
                }

                break;

            case STM_SE_AUDIO_CHAN_R          : //falltrough;
            case STM_SE_AUDIO_CHAN_R_DUALMONO :
                if (ForceLeftAsMonoC)
                {
                    InChannelPlacementCopy.chan[chan] = STM_SE_AUDIO_CHAN_STUFFING;
                }

                if (ForceRightAsMonoC)
                {
                    InChannelPlacementCopy.chan[chan] = STM_SE_AUDIO_CHAN_C;
                }

                break;

            default: InChannelPlacementCopy.chan[chan] = STM_SE_AUDIO_CHAN_STUFFING;
            }
        }

        /* Revaluate the mapping for physicality */
        StmSeAudioChannelPlacementAnalysis_t Analysis;
        stm_se_audio_channel_placement_t SortedPlacement;
        StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&AcmodInput, &AudioModeIsPhysical,
                                                          &SortedPlacement, &Analysis,
                                                          &InChannelPlacementCopy);
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Audio_Mme_c::FillGlobalParamsFromCurrentParameters(MME_PcmProcessorGlobalParams_t *GlobalParams)
{
    PreprocStatus_t Status = PreprocNoError;
    bool AudioModeIsPhysical;

    if (false == AreCurrentControlsAndMetadataSupported())
    {
        SE_ERROR("Current Parameters are not supported\n");
        return PreprocError;
    }

    //@note From here We can make assumptions about calls not returning error as parameters have been  checked
    memset(GlobalParams, 0, sizeof(MME_PcmProcessorGlobalParams_t));
    //Parse the CurrentParameters
    enum eAccFsCode FsCodeInput, FsCodeOutput;
    StmSeTranslateIsoSamplingFrequencyToDiscrete(CurrentParameters.InputMetadata->audio.core_format.sample_rate, FsCodeInput);
    StmSeTranslateIsoSamplingFrequencyToDiscrete(CurrentParameters.Controls.CoreFormat.sample_rate, FsCodeOutput);
    /* Handle Channel Routing */
    enum eAccAcMode AcmodInput, AcmodOutput;
    stm_se_audio_channel_placement_t InChannelPlacementCopy = CurrentParameters.InputMetadata->audio.core_format.channel_placement;
    {
        StmSeAudioChannelPlacementAnalysis_t Analysis;
        stm_se_audio_channel_placement_t SortedPlacement;

        StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&AcmodOutput, &AudioModeIsPhysical,
                                                          &SortedPlacement, &Analysis,
                                                          &CurrentParameters.Controls.CoreFormat.channel_placement);
        //At output we do not care about physicality
        StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&AcmodInput, &AudioModeIsPhysical,
                                                          &SortedPlacement, &Analysis,
                                                          &InChannelPlacementCopy);

        // Do another analysis (and if necessary override) step to handle dual-mono specials
        if (PreprocNoError != ModifyInputRouteForDualMono(InChannelPlacementCopy, AudioModeIsPhysical, AcmodInput, AcmodOutput))
        {
            SE_ERROR("ModifyInputRouteForDualMono failed\n");
            return PreprocError;
        }
    }
    /* Get the wordsize */
    enum eAccLpcmWs LpcmWordSizeIn;
    StmSeAudioGetLpcmWsFromLPcmFormat(&LpcmWordSizeIn, CurrentParameters.InputMetadata->audio.sample_format);
    uint32_t NbChanInput = CurrentParameters.InputMetadata->audio.core_format.channel_placement.channel_count;
    // Fill the Multicom Structures
    //First Fill the General PcmProcessing Framework
    GlobalParams->StructSize = sizeof(MME_PcmProcessorGlobalParams_t);
    GlobalParams->StructSize -= PreprocAudioGlobalParamSubsetSizeAdjust;
    GlobalParams->PcmConfig.DecoderId = ACC_PCM_ID;
    GlobalParams->PcmConfig.StructSize = sizeof(MME_PcmInputConfig_t);
    GlobalParams->PcmConfig.NbChannels = NbChanInput;
    GlobalParams->PcmConfig.HeadRoom = 0;
    GlobalParams->PcmConfig.Reserved = 0;

    if (LpcmWordSizeIn != ACC_LPCM_WS32)
    {
        //Use full LCPM decoder
        GlobalParams->PcmConfig.Lpcm.Definition = 1; // Inform that WordSize is LPCM type
        //in c++ enums are checked, so first downgrade to int. companion definition should be changed
        GlobalParams->PcmConfig.Lpcm.WordSize = (enum eAccWordSizeCode)(int)LpcmWordSizeIn;
    }
    else
    {
        //Bypass Lpcm decoder
        //hypothesis is that ACC_WS32 does not 'set' Lpcm.Definition bit
        GlobalParams->PcmConfig.WordSize = ACC_WS32;
    }

    GlobalParams->PcmConfig.AudioMode = AcmodInput;
    GlobalParams->PcmConfig.SamplingFreq = FsCodeInput;
    GlobalParams->PcmConfig.Request = 0;
    GlobalParams->PcmConfig.Config = 0;
    //Then the Specific Processess
    PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t *SubsetGlobalParams;
    SubsetGlobalParams = (PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t *)&GlobalParams->PcmParams;
    SubsetGlobalParams->StructSize = sizeof(PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t);
    SubsetGlobalParams->DigSplit   = ACC_SPLIT_AUTO;
    SubsetGlobalParams->AuxSplit   = ACC_SPLIT_AUTO;
    SubsetGlobalParams->CMC.Id         = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, ACC_MIX_MAIN);
    SubsetGlobalParams->CMC.StructSize = sizeof(MME_CMCGlobalParams_t);
    SubsetGlobalParams->CMC.Config[CMC_OUTMODE_MAIN]    = AcmodOutput;
    SubsetGlobalParams->CMC.Config[CMC_OUTMODE_AUX]     = ACC_MODE_ID; // no stereo downmix
    //Dual Mono settings
    //Default: LR to LR
    SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_LR;

    // Do not Apply Dual-mono out if OutMode is 1p1
    if (ACC_MODE_1p1 != AcmodOutput)
    {
        switch (CurrentParameters.Controls.DualMono.DualMode)
        {
        case STM_SE_STEREO_OUT:
            SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_LR;
            SE_DEBUG(group_encoder_audio_preproc, "DualMono: ACC_DUAL_LR\n");
            break;

        case STM_SE_DUAL_LEFT_OUT:
            SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_LEFT_MONO;
            SE_DEBUG(group_encoder_audio_preproc, "DualMono: ACC_DUAL_LEFT_MONO\n");
            break;

        case STM_SE_DUAL_RIGHT_OUT:
            SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_RIGHT_MONO;
            SE_DEBUG(group_encoder_audio_preproc, "DualMono: ACC_DUAL_RIGHT_MONO\n");
            break;

        case STM_SE_MONO_OUT:
            SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] = ACC_DUAL_MIX_LR_MONO;
            SE_DEBUG(group_encoder_audio_preproc, "DualMono: ACC_DUAL_MIX_LR_MONO\n");
            break;

        default:
            SE_ERROR("Invalid Dual Mode from Audio_Player %d\n", CurrentParameters.Controls.DualMono.DualMode);
            return PreprocError;
        }
    }

    if (CurrentParameters.Controls.DualMono.StreamDriven)
    {
        SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE] |= ACC_DUAL_1p1_ONLY;
    }

    // [End of of dual-mono settings]
    SubsetGlobalParams->CMC.Config[CMC_PCM_DOWN_SCALED] = ACC_MME_TRUE;
    SubsetGlobalParams->CMC.CenterMixCoeff              = ACC_M3DB;
    SubsetGlobalParams->CMC.SurroundMixCoeff            = ACC_M3DB;
    SubsetGlobalParams->CMC.ExtendedParams.MixCoeffs.GlobalGainCoeff   = ACC_UNITY;
    SubsetGlobalParams->CMC.ExtendedParams.MixCoeffs.LfeMixCoeff       = ACC_UNITY;
    SubsetGlobalParams->DMix.Id         = PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, ACC_MIX_MAIN);
    SubsetGlobalParams->DMix.StructSize = sizeof(MME_DMixGlobalParams_t);
    SubsetGlobalParams->DMix.Apply      = ACC_MME_AUTO;
    SubsetGlobalParams->DMix.Config[DMIX_USER_DEFINED]   = ACC_MME_FALSE;
    SubsetGlobalParams->DMix.Config[DMIX_STEREO_UPMIX]   = ACC_MME_FALSE;
    SubsetGlobalParams->DMix.Config[DMIX_MONO_UPMIX]     = ACC_MME_FALSE;
    SubsetGlobalParams->DMix.Config[DMIX_MEAN_SURROUND]  = ACC_MME_FALSE;
    SubsetGlobalParams->DMix.Config[DMIX_SECOND_STEREO]  = ACC_MME_TRUE;
    SubsetGlobalParams->DMix.Config[DMIX_NORMALIZE]      = ACC_MME_TRUE;
    SubsetGlobalParams->DMix.Config[DMIX_NORM_IDX]       = 0;
    SubsetGlobalParams->DMix.Config[DMIX_DIALOG_ENHANCE] = ACC_MME_FALSE;
    // Checks whether the sampling frequency are in the same 2^ domain (if not ressample2x)
    // and whether they are different when brought back to the same 2^ domain (if not use SFC)
    const uint32_t Number_Of_Fs_Per_Fs_Range = (ACC_FS_reserved_48K - ACC_FS48k) + 1; //Size of one Frequency Range
    SubsetGlobalParams->Resamplex2.Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, ACC_MIX_MAIN);
    SubsetGlobalParams->Resamplex2.StructSize = sizeof(MME_Resamplex2GlobalParams_t);

    if (FsCodeOutput == FsCodeInput)
    {
        SubsetGlobalParams->Resamplex2.Apply = ACC_MME_DISABLED;
    }
    else
    {
        SubsetGlobalParams->Resamplex2.Apply = ACC_MME_AUTO;
    }

    SubsetGlobalParams->Resamplex2.Range = FsCodeOutput / Number_Of_Fs_Per_Fs_Range; // Gets the eFsRange
    SubsetGlobalParams->Resamplex2.OutFs = FsCodeOutput & ACC_FS_reserved_48K; // Gets the modulo in the range
    SubsetGlobalParams->Resamplex2.SfcEnable = 0;

    if (SubsetGlobalParams->Resamplex2.OutFs != (FsCodeInput & ACC_FS_reserved_48K)) //Compares the modulos
    {
        SubsetGlobalParams->Resamplex2.SfcEnable = 1; // Enable the fractional SFC if the modulos are different
    }

    SubsetGlobalParams->Resamplex2.SfcFilterSelect = 0;
    SubsetGlobalParams->Resamplex2.reserved = 0;
    // If the mapping is not normal companion order, need to setup remapping process
    // This covers the dual-left or dual-right into mono ?
    SubsetGlobalParams->ChanReMap.Id = PCMPROCESS_SET_ID(ACC_PCM_CHREMAPIN_ID, ACC_MIX_MAIN);
    SubsetGlobalParams->ChanReMap.StructSize      = sizeof(MME_ChannelReMapGlobalParams_t);

    if (AudioModeIsPhysical)
    {
        SubsetGlobalParams->ChanReMap.Apply           = ACC_MME_DISABLED;
    }
    else
    {
        SubsetGlobalParams->ChanReMap.Apply           = ACC_MME_ENABLED;
    }

    for (int i = 0; i < CurrentParameters.InputMetadata->audio.core_format.channel_placement.channel_count; i++)
    {
        SubsetGlobalParams->ChanReMap.ChannelID[i]    = InChannelPlacementCopy.chan[i];
    }

    for (int i = CurrentParameters.InputMetadata->audio.core_format.channel_placement.channel_count; i < PREPROC_AUDIO_DEFAULT_INPUT_CHANNEL_ALLOC; i++)
    {
        SubsetGlobalParams->ChanReMap.ChannelID[i]    = STM_SE_AUDIO_CHAN_STUFFING;
    }

    // update Acmode used
    mAcmodOutput = AcmodOutput;

    return Status;
}

/**
 * @brief handles all callback from transformers, sends valid buffers to output
 *
 *
 * @param Event
 * @param CallbackData
 */
void Preproc_Audio_Mme_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    PreprocAudioMmeCommandContext_t *TrnsfrmCntxt = (PreprocAudioMmeCommandContext_t *)CallbackData;

    if (NULL == CallbackData)
    {
        SE_ERROR("No CallbackData from MME!\n");
        return;
    }

    switch (CallbackData->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
    {
        /* Reminder Refs: MmeContext */
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            SE_DEBUG(group_encoder_audio_preproc, "MME_SET_GLOBAL_TRANSFORM_PARAMS completed\n");

            // Test for returned status
            if ((MME_SUCCESS != TrnsfrmCntxt->TransformCmd.CmdStatus.Error)
                || (MME_COMMAND_COMPLETED != TrnsfrmCntxt->TransformCmd.CmdStatus.State))
            {
                SE_ERROR("GlobalParams execution reported error:\n");
                SE_ERROR("  TransformCmd.CmdStatus.Error = %08X\n",  TrnsfrmCntxt->TransformCmd.CmdStatus.Error);
                SE_ERROR("  TransformCmd.CmdStatus.State = %08X\n", TrnsfrmCntxt->TransformCmd.CmdStatus.State);
            }
        }
        else
        {
            SE_ERROR("MME_SET_GLOBAL_TRANSFORM_PARAMS aborted\n");
        }
    }
    break;

    case MME_TRANSFORM:
    {
        /* Reminder Refs: PreprocOutputBuffer{InputBuffer}, MmeContext{PreprocOutputBuffer{InputBuffer}, ...} */
        if (NULL == TrnsfrmCntxt->PreprocContextBuffer)
        {
            SE_ERROR("%p->PreprocContextBuffer is NULL\n", TrnsfrmCntxt);
            return;
        }

        Buffer_t PreprocOutputBuffer;
        TrnsfrmCntxt->PreprocContextBuffer->ObtainAttachedBufferReference(PreprocFrameBufferType, &PreprocOutputBuffer);
        SE_ASSERT(PreprocOutputBuffer != NULL);

        PreprocStatus_t Status = PreprocNoError;
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            SE_VERBOSE(group_encoder_audio_preproc, "MME_TRANSFORM completed\n");

            PreprocAudioBuffer_t   OutputAudioBuffer; //!< Output buffer storage
            Status = FillPreprocAudioBufferFromPreProcBuffer(&OutputAudioBuffer, PreprocOutputBuffer);
            if (PreprocNoError != Status)
            {
                SE_ERROR("PreprocOutputBuffer is not a valid buffer\n");
                PreprocOutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                /* Refs: MmeContext{PreprocOutputBuffer{InputBuffer}, ...} */
                break;
            }

            //Fill ouptput Metadata and Size
            Status = FinalizeOutputAudioBuffer(TrnsfrmCntxt, &OutputAudioBuffer);
            if (PreprocNoError != Status)
            {
                SE_ERROR("Failed to finalize output AudioBuffer\n");
                PreprocOutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                /* Refs: MmeContext{PreprocOutputBuffer{InputBuffer}, ...} */
                break;
            }

            //Send the Output
            Status = SendBufferToOutput(&OutputAudioBuffer);
            if (PreprocNoError != Status)
            {
                SE_ERROR("Failed to send buffer to output ring\n");
                PreprocOutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                /* Refs: MmeContext{PreprocOutputBuffer{InputBuffer}, ...} */
            }
            /* Refs: MmeContext{PreprocOutputBuffer, ...} */
            /* Passed to Ring: PreprocOutputBuffer, Release InputBuffer */
        }
        else
        {
            SE_ERROR("MME_TRANSFORM aborted\n");
            PreprocOutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            /* Refs: MmeContext{PreprocOutputBuffer{InputBuffer}, ...} */
        }
    }
    break;

    default:
        SE_ERROR("Unknown CmdCode %08X\n", CallbackData->CmdCode);
    }

    // Release command context buffer
    ReleaseTransformContext(TrnsfrmCntxt, "MMe Callback");
}


void Preproc_Audio_Mme_c::ReleaseTransformContext(PreprocAudioMmeCommandContext_t *Ctx, const char *OptStr)
{
    if ((NULL == Ctx) || (NULL == Ctx->PreprocContextBuffer))
    {
        if (NULL != OptStr)
        {
            SE_INFO(group_encoder_audio_preproc, "%s Trying to release NULL Context (Ctx %p)\n",
                    OptStr, Ctx);
        }
    }
    else
    {
        Ctx->PreprocContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
    }
}

PreprocStatus_t Preproc_Audio_Mme_c::GetNextFreeCommandContext(void)
{
    if (NULL != InputCurrentMmeCommandContext)
    {
        SE_DEBUG(group_encoder_audio_preproc, "Warning CurrentInputCommand is non NULL\n");
        InputCurrentMmeCommandContext = NULL;
    }

    //Check State before
    if (!TestComponentState(ComponentRunning))
    {
        //Do not go on with command;
        SE_WARNING("Component is not running: abort\n");
        return PreprocError;
    }

    // Initialize buffer
    PreprocStatus_t Status = Preproc_Base_c::GetNewContext(&PreprocContextBuffer);
    if (PreprocNoError != Status)
    {
        return PreprocError;
    }

    PreprocContextBuffer->SetUsedDataSize(sizeof(PreprocAudioMmeCommandContext_t));

    // Obtain MmeCommand context reference
    PreprocContextBuffer->ObtainDataReference(NULL, NULL, (void **)(&InputCurrentMmeCommandContext));
    SE_ASSERT(InputCurrentMmeCommandContext != NULL);  // not supposed to be empty

    //Success

    //Check State after
    if (!TestComponentState(ComponentRunning))
    {
        //Do not go on with command;
        SE_WARNING("Component is not running: abort\n");
        // Release command context buffer
        PreprocContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        InputCurrentMmeCommandContext = NULL;
        return PreprocError;
    }

    //All OK
    // Initialize MmeCommand context
    memset((void *)InputCurrentMmeCommandContext, 0, sizeof(PreprocAudioMmeCommandContext_t));

    // Store the PreprocContextBuffer
    InputCurrentMmeCommandContext->PreprocContextBuffer = PreprocContextBuffer;

    return PreprocNoError;
}

void Preproc_Audio_Mme_c::FillSendGlobalCommandContext()
{
    if (NULL == InputCurrentMmeCommandContext)
    {
        SE_ERROR("InputCurrentMmeCommandContext is NULL\n");
        return;
    }

    PreprocAudioMmeCommandContext_t *ctx = InputCurrentMmeCommandContext; //shorter alias
    memset(&ctx->TransformCmd, 0, sizeof(ctx->TransformCmd));
    ctx->TransformCmd.StructSize = sizeof(MME_Command_t);
    ctx->TransformCmd.CmdCode = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    ctx->TransformCmd.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    ctx->TransformCmd.NumberInputBuffers = 0;
    ctx->TransformCmd.NumberOutputBuffers = 0;
    ctx->TransformCmd.DataBuffers_p = NULL;
    ctx->TransformCmd.CmdStatus.AdditionalInfoSize = 0;
    ctx->TransformCmd.CmdStatus.AdditionalInfo_p = NULL;
    ctx->TransformCmd.ParamSize = ctx->GlobalParams.StructSize;
    ctx->TransformCmd.Param_p = (MME_GenericParams_t *)&ctx->GlobalParams;
    //Done
}

void Preproc_Audio_Mme_c::FillTransformCommandContext()
{
    if (NULL == InputCurrentMmeCommandContext)
    {
        SE_ERROR("InputCurrentMmeCommandContext is NULL\n");
        return;
    }

    PreprocAudioMmeCommandContext_t *ctx = InputCurrentMmeCommandContext; //shorter alias
    // Propagate and reset UpdatedControls flag
    ctx->TransformParams.UpdatedControls = UpdatedControls;
    UpdatedControls = false;
    memset(&ctx->TransformCmd, 0, sizeof(ctx->TransformCmd));
    /* Copy Current Input references to Tranform Context */
    ctx->TransformParams.Controls = CurrentParameters.Controls;
    // Update maximum time/
    // We already know the sampling frequency and nsamples have been checked, don't check errors
    uint32_t NSamples = LoopManagement.ThisIteration.NSamples;
    /* Input / Output  buffer */
    ctx->TransformCmd.NumberInputBuffers = 1;
    ctx->TransformCmd.NumberOutputBuffers = 1;
    ctx->TransformCmd.DataBuffers_p = &ctx->TransformParams.MulticomParams.DataBuffers[0];
    ctx->TransformCmd.DataBuffers_p[0] = &ctx->TransformParams.MulticomParams.MMEInputBuffer;
    ctx->TransformCmd.DataBuffers_p[1] = &ctx->TransformParams.MulticomParams.MMEOutputBuffer;
    ctx->TransformCmd.StructSize = sizeof(MME_Command_t);
    ctx->TransformCmd.CmdCode = MME_TRANSFORM;
    ctx->TransformCmd.CmdEnd = MME_COMMAND_END_RETURN_NOTIFY;
    /* Input buffer */
    ctx->TransformCmd.DataBuffers_p[0]->StartOffset = 0;
    ctx->TransformCmd.DataBuffers_p[0]->Flags = 0;
    ctx->TransformCmd.DataBuffers_p[0]->StructSize = sizeof(MME_DataBuffer_t);
    ctx->TransformCmd.DataBuffers_p[0]->UserData_p = (void *)this;
    ctx->TransformCmd.DataBuffers_p[0]->TotalSize = LoopManagement.ThisIteration.SizeB;
    ctx->TransformCmd.DataBuffers_p[0]->NumberOfScatterPages = 1;
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p = &ctx->TransformParams.MulticomParams.MMEInputScatterPage;
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Page_p = (void *)((uint8_t *)(CurrentInputBuffer.CachedAddress)
                                                                            + LoopManagement.ThisIteration.OffsetB);
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Size = ctx->TransformCmd.DataBuffers_p[0]->TotalSize;
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].BytesUsed = 0;
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].FlagsIn = 0;
    ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].FlagsOut = 0;
    /* Output Buffer */
    ctx->TransformCmd.DataBuffers_p[1]->StartOffset = 0;
    ctx->TransformCmd.DataBuffers_p[1]->Flags = 0;
    ctx->TransformCmd.DataBuffers_p[1]->StructSize = sizeof(MME_DataBuffer_t);
    ctx->TransformCmd.DataBuffers_p[1]->UserData_p = (void *)this;
    ctx->TransformCmd.DataBuffers_p[1]->TotalSize = MaxPreprocBufferSize;
    ctx->TransformCmd.DataBuffers_p[1]->NumberOfScatterPages = 1;
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p = &ctx->TransformParams.MulticomParams.MMEOutputScatterPage;
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p[0].Page_p = MainOutputBuffer.CachedAddress;
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p[0].Size = MaxPreprocBufferSize;
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p[0].BytesUsed = 0; //Will be overwritten by tranform
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p[0].FlagsIn = 0;
    ctx->TransformCmd.DataBuffers_p[1]->ScatterPages_p[0].FlagsOut = 0;

    /* Return params and attach to command */
    memset(&ctx->TransformParams.MulticomParams.PcmProcessingStatusParams, 0, sizeof(MME_PcmProcessingFrameStatus_t));
    ctx->TransformCmd.CmdStatus.AdditionalInfoSize = sizeof(MME_PcmProcessingFrameStatus_t);
    ctx->TransformCmd.CmdStatus.AdditionalInfo_p = &ctx->TransformParams.MulticomParams.PcmProcessingStatusParams;
    /* Frame Params  and attach to command */
    memset(&ctx->TransformParams.MulticomParams.PcmProcessingFrameParams, 0, sizeof(MME_PcmProcessingFrameParams_t));
    TimeStamp_c InputTimeStamp; // set to Invalid

    // only first iteration use real time stamp and get extrapolated if needed
    // else  other iterations uses invalid and always get extrapolated

    if (0 == LoopManagement.SamplesSentPrior)
    {
        InputTimeStamp = TimeStamp_c(CurrentInputBuffer.EncodeCoordinatorMetadata->encoded_time, CurrentInputBuffer.EncodeCoordinatorMetadata->encoded_time_format);
        LoopManagement.DeltaEncodeNativeTime = CurrentInputBuffer.EncodeCoordinatorMetadata->encoded_time - CurrentInputBuffer.Metadata->native_time;
        SE_DEBUG(group_encoder_audio_preproc, "Input TS %llu (ms)\n", InputTimeStamp.mSecValue());
    }

    //Get Extrapolated timestamp if necessary (takes care of loop offest automatically)
    InputTimeStamp = InputExtrapolation.GetTimeStamp(InputTimeStamp,
                                                     LoopManagement.ThisIteration.NSamples,
                                                     CurrentInputBuffer.Metadata->audio.core_format.sample_rate);
    //Set the output buffer time stamp with Offset
    MainOutputBuffer.EncodeCoordinatorMetadata->encoded_time = InputTimeStamp.Value();
    // Compute native time,  keeping orginal delta between the encoded_time and the native_time
    MainOutputBuffer.Metadata->native_time = InputTimeStamp.Value() - LoopManagement.DeltaEncodeNativeTime;

    SE_DEBUG(group_encoder_audio_preproc, "Input TS + LoopOffset %llu (ms)\n", InputTimeStamp.mSecValue());
    StmSeAudioAccPtsFromTimeStamp(&ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PtsFlags,
                                  &ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PTS,
                                  InputTimeStamp);
    // For the EOS apply Eof tag to the FW input Buffer.
    ctx->TransformParams.ApplyEOS = ApplyEOS;

    if (ctx->TransformParams.ApplyEOS)
    {
        ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PtsFlags.Bits.FrameType    = STREAMING_DEC_EOF;
    }
    ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.Cmd = ACC_CMD_PLAY;
    /*  AudioFW handles FadeOut, Mute and FadeIn as following

    1:- Upon reception of the CMD_MUTE does the Fade-out on that input before muting the frame.

    2:- Upon reception of next CMD_MUTE does the full mute on that frame.

    3:- Upon reception of CMD_PLAY after CMD_MUTE does the Fade-In on that input.
    So we need to set PcmProcessingFrameParams.Cmd to the CMD_MUTE whenever we receive DISCONTINUITY_FADEOUT or DISCONTINUITY_MUTE.
    Else set PcmProcessingFrameParams.Cmd = CMD_PLAY.
    No need of any setting for DISCONTINUITY_FADEIN. Transition from CMD_MUTE to CMD_PLAY will handle it.*/
    if ((STM_SE_DISCONTINUITY_FADEOUT == PreprocDiscontinuity)
        || (STM_SE_DISCONTINUITY_MUTE == PreprocDiscontinuity))
    {
        ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.Cmd = ACC_CMD_MUTE;
    }
    ctx->TransformCmd.ParamSize = sizeof(MME_PcmProcessingFrameParams_t);
    ctx->TransformCmd.Param_p = (MME_GenericParams_t *)&ctx->TransformParams.MulticomParams.PcmProcessingFrameParams;
    //Ready to send transform
    SE_DEBUG(group_encoder_audio_preproc, "to send: %u nbsmpls, %u bytes, PTS:%llX (%llu ms)\n",
             NSamples,
             ctx->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Size,
             (0 == (ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PtsFlags.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ?
             0 : ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PTS                     ,
             (0 == (ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PtsFlags.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ?
             0 : ctx->TransformParams.MulticomParams.PcmProcessingFrameParams.PTS / 90
            );
    DumpDecoderFrameParams(&ctx->TransformParams.MulticomParams.PcmProcessingFrameParams);
}

PreprocStatus_t Preproc_Audio_Mme_c::SendCurrentCommand()
{
    SE_DEBUG(group_encoder_audio_preproc, "\n");

    MME_ERROR MStatus = MME_SendCommand(TransformerInitContext.hTransformer,
                                        &InputCurrentMmeCommandContext->TransformCmd);
    if (MME_SUCCESS != MStatus)
    {
        SE_ERROR("MME_SendCommand failed: %08X\n", MStatus);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Audio_Mme_c::FinalizeOutputAudioBuffer(PreprocAudioMmeCommandContext_t *TrnsfrmCntxt, PreprocAudioBuffer_t *OutputAudioBuffer)
{
    /* AssertMeThis: */
    PreprocStatus_t Status = PreprocNoError;

    if ((NULL == OutputAudioBuffer->ProcessMetadata) ||
        (NULL == OutputAudioBuffer->Metadata))
    {
        return Status;
    }

    PreprocAudioMmeTransformContext_t *TrnsfrmParams = &TrnsfrmCntxt->TransformParams;
    //Reminder: Preproc_Base_c already copied the Metadata from input to output.
    //We only need to update with controls, size, pts
    PreprocAudioControls_t            *TrnsfrmControls = &TrnsfrmParams->Controls;
    // Fill the ProcessMetadata
    {
        stm_se_encode_process_metadata_t *ProcessMetadata = OutputAudioBuffer->ProcessMetadata;
        ProcessMetadata->updated_metadata = TrnsfrmParams->UpdatedControls;
        TrnsfrmParams->UpdatedControls = false;
        ProcessMetadata->audio.dual_mode   = TrnsfrmControls->DualMono.DualMode;
        ProcessMetadata->audio.dual_mono_forced = TrnsfrmControls->DualMono.StreamDriven;
    }
    /* Output Metadata that may have been changed from input */
    MME_PcmProcessingFrameParams_t    *TrnsfrmPPParams = &TrnsfrmParams->MulticomParams.PcmProcessingFrameParams;
    MME_PcmProcessingFrameStatus_t    *TrnsfrmPPStatusParams = &TrnsfrmParams->MulticomParams.PcmProcessingStatusParams;
    stm_se_uncompressed_frame_metadata_t *Metadata = OutputAudioBuffer->Metadata;
    __stm_se_encode_coordinator_metadata_t *EncodeCoordinatorMetadata = OutputAudioBuffer->EncodeCoordinatorMetadata;
    // Core Format
    Metadata->audio.core_format       = TrnsfrmControls->CoreFormat;
    //Convert from companion ac_mode to get exact mapping
    {
        StmSeAudioChannelPlacementAnalysis_t Analysis;

        if (0) // @todo fixme: Despite setting AcMode for upmix is reported as same as input (see bug 28230)
        {
            StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(&Metadata->audio.core_format.channel_placement,
                                                               &Analysis,
                                                               TrnsfrmPPStatusParams->AudioMode,
                                                               PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT);

            if (SE_IS_DEBUG_ON(group_encoder_audio_preproc))
            {
                SE_DEBUG(group_encoder_audio_preproc, "AcMode is %s\n", StmSeAudioAcModeGetName(TrnsfrmPPStatusParams->AudioMode));

                for (int i = 0; i < Metadata->audio.core_format.channel_placement.channel_count; i++)
                {
                    SE_DEBUG(group_encoder_audio_preproc, "chan[%d] = %s\n", i,
                             StmSeAudioChannelIdGetName((enum stm_se_audio_channel_id_t)Metadata->audio.core_format.channel_placement.chan[i]));
                }
            }
        }
        else
        {
            enum eAccAcMode AudioMode;
            bool IsPhysical;
            stm_se_audio_channel_placement_t SortedPlacement;
            StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&AudioMode, &IsPhysical, &SortedPlacement, &Analysis,
                                                              &TrnsfrmControls->CoreFormat.channel_placement);
            StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(&Metadata->audio.core_format.channel_placement,
                                                               &Analysis,
                                                               AudioMode,
                                                               PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT);

            if (SE_IS_DEBUG_ON(group_encoder_audio_preproc))
            {
                SE_DEBUG(group_encoder_audio_preproc, "AcMode is %s\n", StmSeAudioAcModeGetName(AudioMode));

                for (int i = 0; i < Metadata->audio.core_format.channel_placement.channel_count; i++)
                {
                    SE_DEBUG(group_encoder_audio_preproc, "chan[%d] = %s\n", i,
                             StmSeAudioChannelIdGetName((enum stm_se_audio_channel_id_t)Metadata->audio.core_format.channel_placement.chan[i]));
                }
            }
        }
    }
    // Pts
    //Time Stamp: take care of transform delay and wrapping
    bool IsDelay = true;
    TimeStamp_c TransformOffset;
    //Input Timestamp (with offset) is inside the preproc buffer metadata
    TimeStamp_c InputTimeStamp = TimeStamp_c(EncodeCoordinatorMetadata->encoded_time,
                                             EncodeCoordinatorMetadata->encoded_time_format);
    TimeStamp_c TransformInputTimeStamp  = StmSeAudioTimeStampFromAccPts((const uMME_BufferFlags * const)&TrnsfrmPPParams->PtsFlags,
                                                                         (const uint64_t *const)&TrnsfrmPPParams->PTS);
    TimeStamp_c TransformOutputTimeStamp = StmSeAudioTimeStampFromAccPts((const uMME_BufferFlags * const)&TrnsfrmPPStatusParams->PTSflag,
                                                                         (const uint64_t *const)&TrnsfrmPPStatusParams->PTS);
    TimeStamp_c OutputTimeStamp = StmSeAudioGetApplyOffsetFrom33BitPtsTimeStamps(TransformOffset,
                                                                                 IsDelay,
                                                                                 InputTimeStamp,
                                                                                 TransformInputTimeStamp,
                                                                                 TransformOutputTimeStamp);
    // For the moment we apply the offset to the buffer timestamp
    // but we maybe moving to a separate field
    EncodeCoordinatorMetadata->encoded_time = OutputTimeStamp.Value(InputTimeStamp.TimeFormat());
    // Size
    OutputAudioBuffer->Size = TrnsfrmParams->MulticomParams.MMEOutputScatterPage.BytesUsed;

    if ((TrnsfrmParams->MulticomParams.MMEOutputScatterPage.BytesUsed == 0) && (TrnsfrmParams->ApplyEOS == true))
    {
        OutputAudioBuffer->Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreprocFullMetaDataDescriptor));
        SE_ASSERT(PreprocFullMetaDataDescriptor != NULL);

        // reset extrapolation
        InputExtrapolation.ResetExtrapolator();
        PreprocFullMetaDataDescriptor->uncompressed_frame_metadata.discontinuity = PreprocDiscontinuity;
        PreprocDiscontinuityForwarded = true;
    }
    if ((STM_SE_DISCONTINUITY_MUTE == PreprocDiscontinuity)
        || (STM_SE_DISCONTINUITY_FADEOUT == PreprocDiscontinuity)
        || (STM_SE_DISCONTINUITY_FADEIN == PreprocDiscontinuity))
    {
        PreprocFullMetaDataDescriptor->uncompressed_frame_metadata.discontinuity = PreprocDiscontinuity;
    }
    // PCM Format: Constant at the output of preproc
    Metadata->audio.sample_format     = STM_SE_AUDIO_PCM_FMT_S32LE;

    if (SE_IS_DEBUG_ON(group_encoder_audio_preproc))
    {
        uint32_t NSamples;
        GetNumberSamples(OutputAudioBuffer, &NSamples);
        SE_DEBUG(group_encoder_audio_preproc, "nb_samples:%u buf size:%u, Pts:%llu (ms)\n",
                 NSamples,
                 OutputAudioBuffer->Size,
                 OutputTimeStamp.mSecValue());
    }

    DumpPcmProcessingStatus(&TrnsfrmParams->MulticomParams.PcmProcessingStatusParams);
    return Status;
}

bool Preproc_Audio_Mme_c::ChangeDetectedRequiresTransformReinit(void)
{
    bool ReInitFlag = false;

    /* WA for b39714: Input Params Change From something to Mono */
    if (B39714_WA_ENABLED)
    {
        /* Different Mappings */
        if (0 != memcmp(&PreviousParameters.InputMetadata.audio.core_format.channel_placement,
                        &CurrentParameters.InputMetadata->audio.core_format.channel_placement,
                        sizeof(PreviousParameters.Controls.CoreFormat.channel_placement)))
        {
            /* New Mapping analysis */
            StmSeAudioChannelPlacementAnalysis_t Analysis;
            stm_se_audio_channel_placement_t SortedPlacement;
            bool AudioModeIsPhysical;
            enum eAccAcMode AcmodInput;
            StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&AcmodInput, &AudioModeIsPhysical,
                                                              &SortedPlacement, &Analysis,
                                                              &CurrentParameters.InputMetadata->audio.core_format.channel_placement);
            ReInitFlag = ReInitFlag || (ACC_MODE10 == AcmodInput)  || (ACC_MODE10_LFE == AcmodInput);
        }
    }

    return ReInitFlag;
}

PreprocStatus_t Preproc_Audio_Mme_c::HandleChangeDetection(void)
{
    PreprocStatus_t Status = PreprocNoError;
    /* Quick solution: Create a local Global Param and Fill it with current context;
     * if it is not identical to stored one, then we need to send global params command */
    MME_PcmProcessorGlobalParams_t NewGlobalParams;


    /* Reminder Refs: PreprocFrameBuffer{InputBuffer} */

    if (PreprocNoError != FillGlobalParamsFromCurrentParameters(&NewGlobalParams))
    {
        SE_ERROR("FillGlobalParamsFromCurrentParameters failed\n");
        ReleaseMainPreprocFrameBuffer();
        return PreprocError;
    }

    if (0 != memcmp(&TransformerInitContext.PcmProcessingInitParams.GlobalParams, &NewGlobalParams,
                    sizeof(MME_PcmProcessorGlobalParams_t)))
    {
        if (ChangeDetectedRequiresTransformReinit())
        {
            SE_INFO(group_encoder_audio_preproc, "Parameter change requires reinit of MTPcmProcessing\n");
            /* Blocking */
            Buffer_t AllMmeContexts[PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH];
            bool GotAllMmeContexts;
            Status =  AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);

            if (!GotAllMmeContexts)
            {
                if (PreprocNoError != Status)
                {
                    SE_ERROR("AcquireAllMmeCommandContexts Failure %08X\n", Status);
                }
                else
                {
                    SE_INFO(group_encoder_audio_preproc, "AcquireAllMmeCommandContexts aborted\n");
                }

                ReleaseMainPreprocFrameBuffer();
                return Status;
            }

            if (PreprocNoError != TerminateMmeTransformer())
            {
                SE_ERROR("TerminateMmeTransformer failed\n");

                if (GotAllMmeContexts)
                {
                    ReleaseAllMmeCommandContexts(AllMmeContexts);
                }

                ReleaseMainPreprocFrameBuffer();
                return PreprocError;
            }

            if (PreprocNoError != InitMmeTransformer())
            {
                SE_ERROR("InitMmeTransformer failed\n");

                if (GotAllMmeContexts)
                {
                    ReleaseAllMmeCommandContexts(AllMmeContexts);
                }

                ReleaseMainPreprocFrameBuffer();
                return PreprocError;
            }

            if (GotAllMmeContexts)
            {
                Status = ReleaseAllMmeCommandContexts(AllMmeContexts);
                if (Status != PreprocNoError)
                {
                    ReleaseMainPreprocFrameBuffer();
                    return Status;
                }
            }
        }
        else
        {
            SE_INFO(group_encoder_audio_preproc, "Change detected send global params required\n");
            // Blocking
            Status = GetNextFreeCommandContext();
            if (PreprocNoError != Status)
            {
                PREPROC_ERROR_RUNNING("Could not get a free command to send global params\n");
                ReleaseMainPreprocFrameBuffer();
                return Status;
            }
            /* Refs: MmeContext, PreprocFrameBuffer{InputBuffer} */

            /* Copy the New global params to command and to reference */
            memcpy(&TransformerInitContext.PcmProcessingInitParams.GlobalParams, &NewGlobalParams,
                   sizeof(MME_PcmProcessorGlobalParams_t));
            memcpy(&InputCurrentMmeCommandContext->GlobalParams, &NewGlobalParams,
                   sizeof(MME_PcmProcessorGlobalParams_t));

            /* Fill the Multicom command */
            FillSendGlobalCommandContext();
            if (!TestComponentState(ComponentRunning))
            {
                SE_WARNING("Component is not running: abort\n");
                ReleaseTransformContext(InputCurrentMmeCommandContext);
                InputCurrentMmeCommandContext = NULL;
                ReleaseMainPreprocFrameBuffer();
                return PreprocError;
            }

            /* Send the command */
            Status = SendCurrentCommand();
            if (PreprocNoError != Status)
            {
                SE_ERROR("Sending GlobalParams Command failed\n");
                ReleaseTransformContext(InputCurrentMmeCommandContext);
                InputCurrentMmeCommandContext = NULL;
                ReleaseMainPreprocFrameBuffer();
                return PreprocError;
            }
        }

        UpdatedControls = true;
    }

    return Status;
}

/**
 * @brief Displays InitParams
 *
 */
void Preproc_Audio_Mme_c::DumpInitParams(MME_TransformerInitParams_t *InitParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_preproc))
    {
        SE_ASSERT(InitParams != NULL);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.StructSize                = %08X\n", InitParams->StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.Priority                  = %08X\n", InitParams->Priority);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.TransformerInitParamsSize = %08X\n", InitParams->TransformerInitParamsSize);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.Callback                  = %p\n"  , InitParams->Callback);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.CallbackUserData          = %p\n"  , InitParams->CallbackUserData);
        SE_VERBOSE(group_encoder_audio_preproc, "InitParams.TransformerInitParams_p   = %p\n"  , InitParams->TransformerInitParams_p);
        MME_LxPcmProcessingInitParams_t *PcmProcessingInitParams = (MME_LxPcmProcessingInitParams_t *)InitParams->TransformerInitParams_p ;

        if (NULL == PcmProcessingInitParams)
        {
            SE_ERROR("InitParams.TransformerInitParams_p is NULL\n");
            return;
        }

        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.StructSize           = %08X\n", PcmProcessingInitParams->StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.CacheFlush           = %08X\n", PcmProcessingInitParams->CacheFlush);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.BlockWise            = %08X\n", PcmProcessingInitParams->BlockWise);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.SfreqRange           = %08X\n", PcmProcessingInitParams->SfreqRange);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.NChans[ACC_MIX_MAIN] = %08X\n", PcmProcessingInitParams->NChans[ACC_MIX_MAIN]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingInitParams.NChans[ACC_MIX_AUX]  = %08X\n", PcmProcessingInitParams->NChans[ACC_MIX_AUX]);
        DumpGlobalParams(&PcmProcessingInitParams->GlobalParams);
    }
}

/**
 * @brief Displays the content pointed by GlobalParams
 *
 *
 * @param GlobalParams
 *
 */
void Preproc_Audio_Mme_c::DumpGlobalParams(MME_PcmProcessorGlobalParams_t *GlobalParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_preproc))
    {
        SE_ASSERT(GlobalParams != NULL);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.StructSize = %d\n", GlobalParams->StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.DecoderId       = %08X\n", GlobalParams->PcmConfig.DecoderId);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.StructSize      = %08X\n", GlobalParams->PcmConfig.StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.NbChannels      = %08X\n", GlobalParams->PcmConfig.NbChannels);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.HeadRoom        = %08X\n", GlobalParams->PcmConfig.HeadRoom);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.Reserved        = %08X\n", GlobalParams->PcmConfig.Reserved);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.Lpcm.Definition = %08X\n", GlobalParams->PcmConfig.Lpcm.Definition);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.Lpcm.WordSize   = %08X\n", GlobalParams->PcmConfig.Lpcm.WordSize);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.AudioMode       = %08X\n", GlobalParams->PcmConfig.AudioMode);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.SamplingFreq    = %08X\n", GlobalParams->PcmConfig.SamplingFreq);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.Request         = %08X\n", GlobalParams->PcmConfig.Request);
        SE_VERBOSE(group_encoder_audio_preproc, "GlobalParams.PcmConfig.Config          = %08X\n", GlobalParams->PcmConfig.Config);
        PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t *SubsetGlobalParams;
        SubsetGlobalParams = (PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t *)&GlobalParams->PcmParams;
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.StructSize                       = %08X\n", SubsetGlobalParams->StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DigSplit                         = %08X\n", SubsetGlobalParams->DigSplit);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.AuxSplit                         = %08X\n", SubsetGlobalParams->AuxSplit);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.Id                           = %08X\n", SubsetGlobalParams->CMC.Id);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.StructSize                   = %08X\n", SubsetGlobalParams->CMC.StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.Config[CMC_OUTMODE_MAIN]     = %08X\n", SubsetGlobalParams->CMC.Config[CMC_OUTMODE_MAIN]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.Config[CMC_OUTMODE_AUX]      = %08X\n", SubsetGlobalParams->CMC.Config[CMC_OUTMODE_AUX]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.Config[CMC_DUAL_MODE]        = %08X\n", SubsetGlobalParams->CMC.Config[CMC_DUAL_MODE]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.Config[CMC_PCM_DOWN_SCALED]  = %08X\n", SubsetGlobalParams->CMC.Config[CMC_PCM_DOWN_SCALED]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.CenterMixCoeff               = %08X\n", SubsetGlobalParams->CMC.CenterMixCoeff);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.CMC.SurroundMixCoeff             = %08X\n", SubsetGlobalParams->CMC.SurroundMixCoeff);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Id                          = %08X\n", SubsetGlobalParams->DMix.Id);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.StructSize                  = %08X\n", SubsetGlobalParams->DMix.StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Apply                       = %08X\n", SubsetGlobalParams->DMix.Apply);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_USER_DEFINED]   = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_USER_DEFINED]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_STEREO_UPMIX]   = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_STEREO_UPMIX]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_MONO_UPMIX]     = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_MONO_UPMIX]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_MEAN_SURROUND]  = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_MEAN_SURROUND]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_SECOND_STEREO]  = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_SECOND_STEREO]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_NORMALIZE]      = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_NORMALIZE]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_NORM_IDX]       = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_NORM_IDX]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.DMix.Config[DMIX_DIALOG_ENHANCE] = %08X\n", SubsetGlobalParams->DMix.Config[DMIX_DIALOG_ENHANCE]);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.Id                    = %08X\n", SubsetGlobalParams->Resamplex2.Id);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.StructSize            = %08X\n", SubsetGlobalParams->Resamplex2.StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.Apply                 = %08X\n", SubsetGlobalParams->Resamplex2.Apply);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.Range                 = %08X\n", SubsetGlobalParams->Resamplex2.Range);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.OutFs                 = %08X\n", SubsetGlobalParams->Resamplex2.OutFs);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.SfcEnable             = %08X\n", SubsetGlobalParams->Resamplex2.SfcEnable);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmParams.Resamplex2.SfcFilterSelect       = %08X\n", SubsetGlobalParams->Resamplex2.SfcFilterSelect);
    }
}

void Preproc_Audio_Mme_c::DumpPcmProcessingStatus(MME_PcmProcessingFrameStatus_t *PcmProcessingStatusParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_preproc))
    {
        SE_ASSERT(PcmProcessingStatusParams != NULL);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->StructSize         %08X\n", PcmProcessingStatusParams->StructSize);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->DecStatus          %08X\n", PcmProcessingStatusParams->DecStatus);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->DecRemainingBlocks %08X\n", PcmProcessingStatusParams->DecRemainingBlocks);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->DecAudioMode       %08X\n", PcmProcessingStatusParams->DecAudioMode);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->DecSamplingFreq    %08X\n", PcmProcessingStatusParams->DecSamplingFreq);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->NbOutSamples       %08X\n", PcmProcessingStatusParams->NbOutSamples);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->AudioMode          %08X\n", PcmProcessingStatusParams->AudioMode);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->SamplingFreq       %08X\n", PcmProcessingStatusParams->SamplingFreq);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->Emphasis           %08X\n", PcmProcessingStatusParams->Emphasis);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->ElapsedTime        %08X\n", PcmProcessingStatusParams->ElapsedTime);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->PTSflag            %08X\n", PcmProcessingStatusParams->PTSflag.U32);
        SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->PTS                %llX\n", PcmProcessingStatusParams->PTS);

        for (int i = 0; i < ACC_MAX_DEC_FRAME_STATUS; i++)
        {
            SE_VERBOSE(group_encoder_audio_preproc, "PcmProcessingStatusParams->FrameStatus[i] %04X\n", PcmProcessingStatusParams->FrameStatus[i]);
        }

        SE_VERBOSE(group_encoder_audio_preproc, "Pts = %llX (%llu ms)\n",
                   ((0 == (PcmProcessingStatusParams->PTSflag.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ? 0 : PcmProcessingStatusParams->PTS),
                   ((0 == (PcmProcessingStatusParams->PTSflag.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ? 0 : PcmProcessingStatusParams->PTS / 90)
                  );
    }
}

void Preproc_Audio_Mme_c::DumpDecoderFrameParams(MME_LxAudioDecoderFrameParams_t *FrameParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_preproc))
    {
        SE_ASSERT(FrameParams != NULL);
        SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->Restart       %08X\n", FrameParams->Restart);
        SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->Cmd           %08X\n", FrameParams->Cmd);
        SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->PauseDuration %08X\n", FrameParams->PauseDuration);
        SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->PtsFlags      %08X\n", FrameParams->PtsFlags.U32);
        SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->PTS           %llX\n", FrameParams->PTS);

        for (int i = 0; i < ACC_MAX_DEC_FRAME_PARAMS; i++)
        {
            SE_VERBOSE(group_encoder_audio_preproc, "FrameParams->FrameParams[%d] %04X\n", i, FrameParams->FrameParams[i]);
        }

        SE_VERBOSE(group_encoder_audio_preproc, "Pts = %llX (%llu ms)\n",
                   ((0 == (FrameParams->PtsFlags.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ? 0 : FrameParams->PTS),
                   ((0 == (FrameParams->PtsFlags.Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ? 0 : FrameParams->PTS / 90)
                  );
    }
}

PreprocStatus_t Preproc_Audio_Mme_c::InjectDiscontinuity(stm_se_discontinuity_t Discontinuity)
{
    PreprocStatus_t Status = PreprocNoError;

    Status = Preproc_Base_c::InjectDiscontinuity(Discontinuity);
    if (PreprocNoError != Status)
    {
        PREPROC_ERROR_RUNNING("Unable to inject discontinuity!\n");
        return PREPROC_STATUS_RUNNING(Status);
    }

    // Additional check for discontinuity non-applicable
    // Open gop is already checked in base class, in case of new support for open gop, this check will be valid for audio
    if ((Discontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST)
        || (Discontinuity & STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST))
    {
        SE_ERROR("Discontinuity GOP request not applicable to audio\n");
        return EncoderNotSupported;
    }

    if (Discontinuity)
    {
        // Wait for commands to complete
        Buffer_t AllMmeContexts[PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH];
        bool GotAllMmeContexts;
        Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);

        if (!GotAllMmeContexts)
        {
            if (PreprocNoError != Status)
            {
                SE_ERROR("AcquireAllMmeCommandContexts Failure %08X\n", Status);
            }
            else
            {
                SE_INFO(group_encoder_audio_preproc, "AcquireAllMmeCommandContexts Aborted\n");
            }

            return Status;
        }

        // Generate Discontinuity Buffer
        Status = GenerateBufferDiscontinuity(Discontinuity);
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("GenerateBufferDiscontinuity Failed %08X\n", Status);

            if (GotAllMmeContexts)
            {
                ReleaseAllMmeCommandContexts(AllMmeContexts);
            }
            return PREPROC_STATUS_RUNNING(Status);
        }

        if (GotAllMmeContexts)
        {
            Status = ReleaseAllMmeCommandContexts(AllMmeContexts);
            if (PreprocNoError != Status)
            {
                SE_ERROR("ReleaseAllMmeContexts Failed %08X\n", Status);
            }
        }
    }

    return Status;
}

PreprocStatus_t Preproc_Audio_Mme_c::AcquireAllMmeCommandContexts(Buffer_t ContextBufferArray[], bool *Success,
                                                                  bool AbortOnNonRunning)
{
    /* Difference between (true==(!*Success)) and false=(CoderNoError==AcquireAllMmeCommandContexts()):
       !*Success can happen on the normal path
       the other is an Error status: it should not happen */
    *Success = true;

    if (PreprocContextBufferPool == NULL)
    {
        *Success = false;
        return PreprocError;
    }

    int32_t  ContextBufferDepth = (int32_t)PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH;

    /* Prevent concurrency on AcquireAllMmeCommandContexts */
    OS_LockMutex(&AcquireAllMmeCommandContextsLock);
    int i;
    for (i = 0;  i < ContextBufferDepth ; i++)
    {
        ContextBufferArray[i] = NULL;
    }

    PreprocStatus_t Status = PreprocNoError;
    BufferStatus_t  BufStatus = BufferNoError;
    // Acquire all context buffers.
    // Buffer manager will block if any context buffer is locked by FW, other process
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint32_t GetBufferTimeOutMs   = 100;
    for (i = 0;  i < ContextBufferDepth ; i++)
    {
        uint64_t EntryTime = OS_GetTimeInMicroSeconds();
        do
        {
            BufStatus = PreprocContextBufferPool->GetBuffer(&ContextBufferArray[i], IdentifierEncoderPreprocessor,
                                                            UNSPECIFIED_SIZE, false, false,
                                                            GetBufferTimeOutMs);

            // Warning message every TimeBetweenMessageUs us
            if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
            {
                SE_WARNING("%p->%p->GetBuffer still returns BufferNoFreeBufferAvailable,  %s and %s\n",
                           this, PreprocContextBufferPool,
                           TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning",
                           AbortOnNonRunning ? "AbortOnNonRunning" : "!AbortOnNonRunning");
                EntryTime = OS_GetTimeInMicroSeconds();
            }
        }
        while ((BufferNoFreeBufferAvailable == BufStatus)
               && (TestComponentState(ComponentRunning)
                   || (!AbortOnNonRunning)));

        if ((BufStatus != BufferNoError) || (NULL == ContextBufferArray[i]))
        {
            ContextBufferArray[i] = NULL;
            *Success = false;
            if (BufferBlockingCallAborted == BufStatus)
            {
                SE_WARNING("GetBufferAborted\n"); // TODO(pht) remove
            }
            else
            {
                SE_ERROR("GetBufferFailure %08X\n", BufStatus);
                Status = PreprocError;
            }
            break;
        }
    }

    //In case of non-sucess to get all mme buffers, release all mme buffers obtained here
    if (!*Success)
    {
        PreprocStatus_t ReleaseStatus = ReleaseAllMmeCommandContexts(ContextBufferArray);
        if (PreprocNoError != ReleaseStatus)
        {
            Status = ReleaseStatus;
        }
    }

    // We can safely unlock the mutex
    OS_UnLockMutex(&AcquireAllMmeCommandContextsLock);

    return Status;
}

PreprocStatus_t Preproc_Audio_Mme_c::ReleaseAllMmeCommandContexts(Buffer_t ContextBufferArray[])
{
    int32_t ContextBufferDepth = (int32_t)(PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH);

    if (PreprocContextBufferPool == NULL)
    {
        return PreprocNoError;
    }

    int i;
    for (i = 0; i < ContextBufferDepth ; i++)
    {
        if (NULL == ContextBufferArray[i])
        {
            /* This is not an error path as ReleaseAllMmeCommandContexts is called from AcquireAllMmeCommandContexts */
            SE_DEBUG(group_encoder_audio_preproc, "ContextBufferArray[%d] is NULL\n", i);
            continue;
        }

        ContextBufferArray[i]->DecrementReferenceCount(IdentifierEncoderPreprocessor);

        ContextBufferArray[i] = NULL;
    }

    return PreprocNoError;
}

//
// Low power enter method
// For CPS mode, all MME transformers must be terminated
//
PreprocStatus_t Preproc_Audio_Mme_c::LowPowerEnter(void)
{
    PreprocStatus_t PreprocStatus = PreprocNoError;
    // Save low power state
    IsLowPowerState = true;
    // Terminate MME transformer if needed
    IsLowPowerMMEInitialized = (TransformerInitContext.hTransformer != NULL);

    if (IsLowPowerMMEInitialized)
    {
        Buffer_t AllMmeContexts[PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH];
        bool GotAllMmeContexts;

        /**
         * @todo : LowPower State(s) to be added to base class and test
         *        against them in Input threads, so that Input threads
         *        can be unlocked if LP is required.
         */

        /* Gives up if component is not Running */
        PreprocStatus = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);

        if (!GotAllMmeContexts)
        {
            if (PreprocNoError != PreprocStatus)
            {
                SE_ERROR("AcquireAllMmeCommandContexts Failed %08X\n", PreprocStatus);
            }
            else
            {
                SE_INFO(group_encoder_audio_preproc, "Preproc Halted while trying to enter LowPower\n");
            }

            return PreprocStatus;
        }

        PreprocStatus = TerminateMmeTransformer();

        if (PreprocNoError != PreprocStatus)
        {
            SE_ERROR("TerminateMmeTransformer Failed %08X\n", PreprocStatus);

            if (GotAllMmeContexts)
            {
                ReleaseAllMmeCommandContexts(AllMmeContexts);
            }
        }

        if (GotAllMmeContexts)
        {
            PreprocStatus = ReleaseAllMmeCommandContexts(AllMmeContexts);

            if (PreprocNoError != PreprocStatus)
            {
                SE_ERROR("ReleaseAllMmeContexts Failed %08X\n", PreprocStatus);
            }
        }
    }

    return PreprocStatus;
}

//
// Low power exit method
// For CPS mode, all MME transformers must be re-initialized
//
PreprocStatus_t Preproc_Audio_Mme_c::LowPowerExit(void)
{
    PreprocStatus_t PreprocStatus = PreprocNoError;

    // Re-initialize MME transformer if needed
    if (IsLowPowerMMEInitialized)
    {
        PreprocStatus = InitMmeTransformer();
    }

    // reset low power state
    IsLowPowerState = false;
    return PreprocStatus;
}

PreprocStatus_t Preproc_Audio_Mme_c::Bz25952MmeCommandOverride(void)
{
    if (0 != PREPROC_AUDIO_MME_BUG_25952_INPUT_BUFFER_MAPPING_WORKAROUND)
    {
        PreprocStatus_t  Status;
        Buffer_t Bz25952Buffer = NULL;

        // Get an extra PreprocFrameBuffer
        Status = GetNewBuffer(&Bz25952Buffer);
        if ((NULL == Bz25952Buffer) || (PreprocNoError != Status))
        {
            PREPROC_ERROR_RUNNING("Failed to get a new preproc buffer to store input\n");
            return PreprocError;
        }
        /* Refs: bz225952, PreprocFrameBuffer{InputBuffer}, MmeContext{PreprocFrameBuffer{InputBuffer}} */

        /* Attach the bz225952 buffer to the PreprocContextBuffer. No use of it after the callback */
        InputCurrentMmeCommandContext->PreprocContextBuffer->AttachBuffer(Bz25952Buffer);
        /* Refs: bz225952, PreprocFrameBuffer{InputBuffer}, MmeContext{bz225952, PreprocFrameBuffer{InputBuffer}} */

        // DecrementReferenceCount of bz225952 buffer so that it releases automatically on PreprocContextBuffer release
        Bz25952Buffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);

        /* Refs: PreprocFrameBuffer{InputBuffer}, MmeContext{bz225952, PreprocFrameBuffer{InputBuffer}} */
        void  *BufferCachedAddress = NULL;
        Bz25952Buffer->ObtainDataReference(NULL, NULL, &BufferCachedAddress, CachedAddress);
        if (NULL == BufferCachedAddress)
        {
            // TODO(pht) check if assert would fit
            SE_ERROR("Bz25952Buffer->ObtainDataReference failure\n");
            return PreprocError;
        }

        /* Copy the data from Input to the Bz25952Buffer */
        memcpy(BufferCachedAddress,
               InputCurrentMmeCommandContext->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Page_p,
               InputCurrentMmeCommandContext->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Size);
        /* Set Bz25952Buffer as source of the transform  */
        InputCurrentMmeCommandContext->TransformCmd.DataBuffers_p[0]->ScatterPages_p[0].Page_p = BufferCachedAddress;
    }

    return PreprocNoError;
}

unsigned int Preproc_Audio_Mme_c::GetMaxPreprocBufferSize(void)
{
    const int BPerSample = 4; //fixed constant 32bit per smp

    return (unsigned int)(PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT * PREPROC_AUDIO_OUT_MAX_NB_SAMPLES * BPerSample);
}

unsigned int Preproc_Audio_Mme_c::GetMaxNrPreprocBuffers(void)
{
    int NumberOfBuffersRequired = 0;

    const int NBeingEncoded = 1;
    const int NBeingPreprocessed = 1;

    NumberOfBuffersRequired = NBeingPreprocessed + NBeingEncoded ;

    if (0 != PREPROC_AUDIO_MME_BUG_25952_INPUT_BUFFER_MAPPING_WORKAROUND)
    {
        /* 1 extra output buffer to copy input data to */
        NumberOfBuffersRequired++;
    }

    /**
     * Why we do not need to consider the upsampling ratio here: we
     * split the InputBuffer in as many smaller process units as
     * required, each processed sequentially.
     *
     * However to help the SE client manage its buffers, we will
     * consider the input ratio in the allocation of coded buffers:
     * we will consider how many coded buffers can
     * be producedin the *time* correponding to
     * PREPROC_AUDIO_IN_MAX_NB_SAMPLES
     */

    return (unsigned int)(NumberOfBuffersRequired);
}
