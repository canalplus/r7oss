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

#include "player_threads.h"

#include "coder_audio_mme.h"
#include "audio_conversions.h"
#include "timestamps.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Audio_Mme_c"

/* limit maximum number of output decoded buffers; W.A. for bug52072 and limited tsmux nb buffers handling */
#define CODER_MAX_NB_CODED_BUFFERS 64

typedef void (*MME_GenericCallback_t)(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData);

static void MMECallbackStub(MME_Event_t      Event,
                            MME_Command_t   *CallbackData,
                            void            *UserData)
{
    if (NULL != UserData)
    {
        Coder_Audio_Mme_c         *Self = (Coder_Audio_Mme_c *)UserData;
        Self->CallbackFromMME(Event, CallbackData);
    }
    else
    {
        SE_ERROR("UserData is NULL\n");
    }
}

Coder_Audio_Mme_c::Coder_Audio_Mme_c(const CoderAudioStaticConfiguration_t &staticconfig,
                                     const CoderBaseAudioMetadata_t &baseaudiometadata,
                                     const CoderAudioControls_t &basecontrols,
                                     const CoderAudioMmeStaticConfiguration_t &mmestaticconfig)
    : Coder_Audio_c(staticconfig, baseaudiometadata, basecontrols)
    , mMmeStaticConfiguration(mmestaticconfig)
    , mMmeDynamicContext()
    , mCurrentTransformContext(NULL)
    , mMmeCounters()
    , mMMECallbackPriorityBoosted(false)
    , mCoderContextBuffer(NULL)
    , mCoderContextBufferType(NULL)
    , mCoderContextBufferPool(NULL)
    , mInputPayload()
    , mPreviousMmeDynamicContext()
    , mParameterUpdateAnalysis()
    , mMmeAudioEncoderInitParams()
    , mMmeInitParams()
    , mTransformerHandle(0)
    , mIsLowPowerState(false)
    , mIsLowPowerMMEInitialized(false)
    , mOutputExtrapolation()
    , mDelayExtraction()
    , mAcquireAllMmeCommandContextsLock()
    , mNrtOutputSavedOffset(0)
{
    SE_ASSERT(!(mMmeStaticConfiguration.MmeTransformQueueDepth > CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH)
              || (0 == mMmeStaticConfiguration.MmeTransformQueueDepth));

    //Fill Codec Specific Static context: parameters that are not accessible through Controls/Metadata are Static
    //Form the Initial CodecDynamicContext (will be static unless codec later updates it based on context)
    mMmeDynamicContext.CodecDynamicContext.NrSamplePerCodedFrame = mStaticConfiguration.MinNbSamplesPerCodedFrame;
    mMmeDynamicContext.CodecDynamicContext.MaxCodedFrameSize     = mStaticConfiguration.MaxCodedFrameSize;

    mOutputExtrapolation.ResetExtrapolator();
    DelayExtractionReset();

    unsigned int framesize = GetMaxCodedBufferSize();
    unsigned int nbbuffers = GetNrCodedBuffersToAllocate();
    SetFrameMemory(framesize, nbbuffers);

    SE_DEBUG(group_encoder_audio_coder, "Set %u CodedBuffers of %u bytes ==> %u kB for allocation\n",
             nbbuffers, framesize,
             (nbbuffers * framesize) / 1024);

    OS_InitializeMutex(&mAcquireAllMmeCommandContextsLock);
}

Coder_Audio_Mme_c::~Coder_Audio_Mme_c()
{
    if (mCoderContextBufferPool != NULL)
    {
        BufferManager->DestroyPool(mCoderContextBufferPool);
    }

    OS_TerminateMutex(&mAcquireAllMmeCommandContextsLock);
}

CoderStatus_t   Coder_Audio_Mme_c::Halt(void)
{
    CoderStatus_t Status = Coder_Audio_c::Halt();

    Buffer_t AllMmeContexts[CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH];
    bool GotAllMmeContexts;
    // Acquire all Mme Command Contexts, do not give up if NotRunning
    Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts, false);
    if (CoderNoError != Status)
    {
        SE_ERROR("AcquireAllMmeCommandContexts Failed\n");
    }

    if (GotAllMmeContexts)
    {
        if (CoderNoError != ReleaseAllMmeCommandContexts(AllMmeContexts))
        {
            SE_ERROR("ReleaseAllCodedContext Failed\n");
            Status = CoderError;
        }
    }

    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::InitializeCoder(void)
{
    mMmeStaticConfiguration.AudioEncoderTransformerNames[0] = AUDIOENCODER_MT_NAME"_a0";
    mMmeStaticConfiguration.AudioEncoderTransformerNames[1] = AUDIOENCODER_MT_NAME"_a1";

    CoderStatus_t Status = InitializeMmeTransformer();
    if (CoderNoError != Status)
    {
        //Enough Error Messages in InitializeMmeTransformer()
        return CoderResourceAllocationFailure;
    }

    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::TerminateCoder(void)
{
    CoderStatus_t Status = CoderNoError;

    //We should not be running here as can only be called after Halt().
    if (TestComponentState(ComponentRunning))
    {
        SE_ERROR("Called while component is running");
    }

    Buffer_t AllMmeContexts[CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH];
    bool GotAllMmeContexts;

    // Acquire all Mme Command Contexts, do not give up if NotRunning
    Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts, false);
    if (CoderNoError != Status)
    {
        SE_ERROR("AcquireAllMmeCommandContexts Failed\n");
        // If we reach here with Error we are in trouble, yet we will attempt Terminate
    }

    Status = TerminateMmeTransformer();

    if (CoderNoError != Status)
    {
        SE_ERROR("TerminateTransformer failed\n"); // continue
    }

    if (GotAllMmeContexts)
    {
        if (CoderNoError != ReleaseAllMmeCommandContexts(AllMmeContexts))
        {
            SE_ERROR("ReleaseAllCodedContext Failed\n");
        }
    }
    else
    {
        // If we failed to get all mme contexts and terminated ok, we still report error
        if (CoderNoError == Status)
        {
            Status = CoderError;
        }
    }

    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::CreateMmeContextPool()
{
    CoderStatus_t Status = CoderNoError;
    Status      = BufferManager->FindBufferDataType(mMmeStaticConfiguration.MmeContextDescriptor->TypeName, &mCoderContextBufferType);

    if (Status != BufferNoError)
    {
        // Create buffer data type if it does not exist
        Status = BufferManager->CreateBufferDataType(mMmeStaticConfiguration.MmeContextDescriptor, &mCoderContextBufferType);

        if (Status != BufferNoError)
        {
            SE_ERROR("Unable to create Mme context buffer type\n");
            return Status;
        }
    }

    Status = BufferManager->CreatePool(&mCoderContextBufferPool, mCoderContextBufferType, mMmeStaticConfiguration.MmeTransformQueueDepth);

    if (Status != BufferNoError)
    {
        SE_ERROR("Unable to create Mme context  context pool\n");
        return Status;
    }

    return Status;
}

void Coder_Audio_Mme_c::ReleaseAllCodedBufferInMmeTransformContext(CoderAudioMmeTransformContext_t *Context, const char *OptStr)
{
    for (uint32_t i = Context->NumberOfCodedBufferReferencesFreed; i < Context->NumberOfCodedBufferReferencesTaken; i++)
    {
        if (NULL == Context->CodedBuffers[i])
        {
            if (NULL != OptStr)
            {
                SE_ERROR("%s CodedBuffer[%d] is NULL\n", OptStr, i);
            }
        }
        else
        {
            ReleaseCodedBufferFromTransformContext(Context, i);
        }
    }
}

void Coder_Audio_Mme_c::ReleaseCodedBufferFromTransformContext(CoderAudioMmeTransformContext_t *Context, uint32_t Index)
{
    Context->CodedBuffers[Index]->DecrementReferenceCount();

    /* Even in case of failure increment count */
    Context->NumberOfCodedBufferReferencesFreed++;
}

void  Coder_Audio_Mme_c::ReleaseTransformContext(CoderAudioMmeTransformContext_t *Ctx, const char *OptStr)
{
    if ((NULL == Ctx) || (NULL == Ctx->CoderContextBuffer))
    {
        if (NULL != OptStr)
        {
            SE_INFO(group_encoder_audio_coder, "%s: Trying to release NULL Context (Ctx = %p)\n",
                    OptStr, Ctx);
        }
        return;
    }

    Ctx->CoderContextBuffer->DecrementReferenceCount(IdentifierEncoderCoder);
}

/* Blocking: Request N CodedBuffers as required by this transform */
CoderStatus_t Coder_Audio_Mme_c::GetRequiredNrCodedBuffers(Buffer_t InputBuffer)
{
    mCurrentTransformContext->CodedBuffers[0] = CodedFrameBuffer;
    mCurrentTransformContext->NumberOfCodedBufferReferencesTaken = 1;
    mCurrentTransformContext->NumberOfCodedBufferReferencesFreed = 0;
    SE_DEBUG(group_encoder_audio_coder, "Get required %d more CodedBuffers\n", mMmeDynamicContext.NrCodedBufferCurrentTransform - 1);

    for (uint32_t i = 1; i < mMmeDynamicContext.NrCodedBufferCurrentTransform; i++)
    {
        //Check State before going into a blocking call
        if (!TestComponentState(ComponentRunning))
        {
            SE_DEBUG(group_encoder_audio_coder, "ComponentNotRunning: abort\n");
            return CoderError;
        }

        CoderStatus_t Status = GetNewCodedBuffer(&mCurrentTransformContext->CodedBuffers[i], mMmeDynamicContext.MaxCodedBufferSizeBForThisInput);
        if ((CoderNoError != Status) || (NULL == mCurrentTransformContext->CodedBuffers[i]))
        {
            CODER_ERROR_RUNNING("GetNewCodedBuffer Failed : cannot get %d th coded frame buffer (returned: %08X, value: %p)\n",
                                i, Status, mCurrentTransformContext->CodedBuffers[i]);
            return CoderGenericBufferError;
        }

        //Check State after return from a blocking call
        if (!TestComponentState(ComponentRunning))
        {
            SE_DEBUG(group_encoder_audio_coder, "ComponentNotRunning: abort\n");
            mCurrentTransformContext->CodedBuffers[i]->DecrementReferenceCount();
            return CoderError;
        }

        mCurrentTransformContext->CodedBuffers[i]->AttachBuffer(InputBuffer);
        mCurrentTransformContext->NumberOfCodedBufferReferencesTaken++;
    }

    if (mCurrentTransformContext->NumberOfCodedBufferReferencesTaken != mMmeDynamicContext.NrCodedBufferCurrentTransform)
        SE_ERROR("bad count %d-%d\n", mCurrentTransformContext->NumberOfCodedBufferReferencesTaken,
                 mMmeDynamicContext.NrCodedBufferCurrentTransform);

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::RegisterOutputBufferRing(Ring_t      Ring)
{
    CoderStatus_t Status  = Coder_Base_c::RegisterOutputBufferRing(Ring);

    if (CoderNoError != Status)
    {
        SE_ERROR("Failed (%08X)\n", Status);
        return CoderGenericBufferError;
    }

    Status = CreateMmeContextPool();

    if (CoderNoError != Status)
    {
        SE_ERROR("Failed during context buffer pool creation(%08X)\n", Status);
        return CoderGenericBufferError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::Input(Buffer_t Buffer)
{
    SE_DEBUG(group_encoder_audio_coder, "\n");

    // Check if not in low power state
    if (mIsLowPowerState)
    {
        SE_ERROR("SE device is in low power\n");
        return CoderError;
    }

    const CoderAudioCurrentParameters_t  CurrentEncodingParameters  = GetConstRefCurEncodingParameters();

    /* Let base class do some work for us */
    CoderStatus_t Status = Coder_Audio_c::Input(Buffer);
    if (CoderNoError != Status)
    {
        CODER_ERROR_RUNNING("Input() returned error: %08X\n", Status);
        return CODER_STATUS_RUNNING(Status);
    }

    if ((NULL == CodedFrameBuffer) || (NULL == CurrentEncodingParameters.InputMetadata))
    {
        SE_ERROR("Bad configuration after Input(): CodedFrameBuffer:%p or CurrentEncodingParameters.InputMetadata:%p is NULL\n",
                 CodedFrameBuffer, CurrentEncodingParameters.InputMetadata);
        return CoderInvalidInputBufferReference;
    }

    /* Refs: CodedFrameBuffer{InputBuffer} */
    /* Use ReleaseMainCodedBuffer to release these references */

    /* Generate Buffer EOS if Buffer EOS detected and exit */
    if (CoderDiscontinuity)
    {
        SE_DEBUG(group_encoder_audio_coder, "CoderDiscontinuity %08X found\n", CoderDiscontinuity);

        // Handle separately case we need to drain or not
        // If data injected before then no drain is required
        if (0 == mMmeCounters.ContinousBytesSent)
        {
            SE_DEBUG(group_encoder_audio_coder, "No data before discontinuity\n");

            if (CoderDiscontinuity & STM_SE_DISCONTINUITY_EOS)
            {
                Buffer_t AllMmeContexts[CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH];
                bool GotAllMmeContexts;

                /* Refs: CodedFrameBuffer{InputBuffer} */
                Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);
                if (!GotAllMmeContexts || (CoderNoError != Status))
                {
                    /* This thread has low priority on AcquireAll: give up*/
                    CODER_ERROR_RUNNING("AcquireAllMmeCommandContexts failed, aborting EOS!\n");
                    ReleaseMainCodedBuffer();
                    return CODER_STATUS_RUNNING(Status);
                }

                /* Refs: AllMme, CodedFrameBuffer{InputBuffer} */

                CoderDiscontinuity = (stm_se_discontinuity_t)(CoderDiscontinuity & ~STM_SE_DISCONTINUITY_EOS);
                mOutputExtrapolation.ResetExtrapolator(mOutputExtrapolation.TimeFormat());
                mNrtOutputSavedOffset = 0;
                OS_LockMutex(&Lock);
                DelayExtractionReset();
                OS_UnLockMutex(&Lock);
                Status = GenerateBufferEOS(CodedFrameBuffer);
                /* Refs: AllMme (CodedFrameBuffer, InputBuffer released) */

                /* release mme contexts */
                if (GotAllMmeContexts)
                {
                    if (CoderNoError != ReleaseAllMmeCommandContexts(AllMmeContexts))
                    {
                        CODER_ERROR_RUNNING("ReleaseAllCodedContext failed\n");
                        Status = CoderError;
                    }
                }

                return CODER_STATUS_RUNNING(Status);
            }
            else
            {
                /* Refs: CodedFrameBuffer{InputBuffer} */
                SE_DEBUG(group_encoder_audio_coder, "Quietly discard the frame\n");
                mOutputExtrapolation.ResetExtrapolator(mOutputExtrapolation.TimeFormat());
                mNrtOutputSavedOffset = 0;
                OS_LockMutex(&Lock);
                DelayExtractionReset();
                OS_UnLockMutex(&Lock);
                // Silently discard the frame
                ReleaseMainCodedBuffer();
                return CoderNoError;
            }
        }
        else
        {
            // We send an MME command to drain previously injected data
            SE_DEBUG(group_encoder_audio_coder, "%d bytes before discontinuity\n", mMmeCounters.ContinousBytesSent);

            // Because Fake buffer does not have proper audio metadata we need to populate
            // Fake buffer timestamp needs to be be invalidated.
            UpdateFakeBufCurrentFromPreviousParameters();
        }
    }

    if (CurrentEncodingParameters.InputMetadata->discontinuity)
    {
        SE_DEBUG(group_encoder_audio_coder, "MetadataDiscontinuity %08X found\n",
                 CurrentEncodingParameters.InputMetadata->discontinuity);
    }

    /* Data Reference here since this class knows what kind of address to get */
    mInputPayload.Size_B = 0;
    mInputPayload.CachedAddress = NULL;
    Buffer->ObtainDataReference(NULL, &mInputPayload.Size_B, &mInputPayload.CachedAddress, CachedAddress);
    if (mInputPayload.CachedAddress == NULL)
    {
        SE_ERROR("Unable to obtain inputpayload address\n");
        ReleaseMainCodedBuffer();
        return CoderInvalidInputBufferReference;
    }

    /* Reminder Refs: CodedFrameBuffer{InputBuffer} */

    /* Check buffer and context compatibilities; Set internal context */
    Status = UpdateMmeDynamicContext();
    if (CoderNoError != Status)
    {
        SE_ERROR("Input Buffer or controls is not compatible with this Coder_Audio_Mme (%08X)\n", Status);
        ReleaseMainCodedBuffer();
        return CoderUnsupportedParameters;
    }

    /* Check we have an initialised transformer before going further */
    if (0 == mTransformerHandle)
    {
        SE_ERROR("mTransformerHandle is 0");
        ReleaseMainCodedBuffer();
        return CoderResourceAllocationFailure;
    }

    /* Blocking: Compare Previous and Current setting and action */
    Status = HandleChangeDetection(Buffer);
    if (CoderNoError != Status)
    {
        /*HandleChangeDetection() takes care of clean-up*/
        return Status;
    }

    /* Blocking: get an MME Transform context in mCurrentTransformContext*/
    Status = GetNextFreeTransformContext();
    if (CoderNoError != Status)
    {
        CODER_ERROR_RUNNING("GetNextFreeTransformContext Failed\n");
        ReleaseMainCodedBuffer();
        return CODER_STATUS_RUNNING(Status);
    }

    /* Refs: MmeContext, CodedFrameBuffer{InputBuffer} */

    /**
     * GetRequiredNrCodedBuffers() is Blocking: We try to get the
     * required number of output buffers and store references in
     * mCurrentTransformContext.
     *
     * Before the call we already have acquired MainCodedBuffer;
     * after the call, this reference is always transfered to the
     * TransformContext passed as a parameter (even if the call
     * returns an Error).
     *
     * So after the call CodedBuffer references must be released by
     * calling ReleaseAllCodedBufferInMmeTransformContext instead of
     * GetNextFreeTransformContext.
     */
    Status = GetRequiredNrCodedBuffers(Buffer);
    /* Refs: MmeContext, NumberOfCodedBufferReferencesTaken*CodedFrameBuffer{InputBuffer} */
    if (CoderNoError != Status)
    {
        CODER_ERROR_RUNNING("GetRequiredNrCodedBuffers Failed\n");
        ReleaseAllCodedBufferInMmeTransformContext(mCurrentTransformContext);
        ReleaseTransformContext(mCurrentTransformContext);
        mCurrentTransformContext = NULL;
        return CODER_STATUS_RUNNING(Status);
    }

    Status = FillCurrentTransformContext();
    if (CoderNoError != Status)
    {
        SE_ERROR("Could not fill Transform Context\n");
        ReleaseAllCodedBufferInMmeTransformContext(mCurrentTransformContext);
        ReleaseTransformContext(mCurrentTransformContext);
        mCurrentTransformContext = NULL;
        return Status;
    }

    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_VERBOSE(group_encoder_audio_coder, "CurrentTransformContext = %p\n", mCurrentTransformContext);
        SE_VERBOSE(group_encoder_audio_coder, "CurrentTransformContext Count = %d\n", mCurrentTransformContext->CommandCount);
        DumpMmeCmd(&mCurrentTransformContext->TransformCmd, "SendTransf");
    }

    if (CoderNoError != SendCommand(&mCurrentTransformContext->TransformCmd))
    {
        SE_ERROR("Could not send transform command\n");
        ReleaseAllCodedBufferInMmeTransformContext(mCurrentTransformContext);
        ReleaseTransformContext(mCurrentTransformContext);
        mCurrentTransformContext = NULL;
        return CoderResourceError;
    }

    /* Success! We have sent the Buffer for Encoding */
    mMmeCounters.TransformPrepared++;

    /* Refs: MmeContext, NumberOfCodedBufferReferencesTaken*CodedFrameBuffer{InputBuffer} will be freed from callback*/
    /* CurrentTransform Context reference does not belong to this thread anymore, will be cleared from callback */
    mCurrentTransformContext = NULL;
    SE_DEBUG(group_encoder_audio_coder, "Transformed Issued %d - GlobalParams Issued %d\n",
             mMmeCounters.TransformPrepared, mMmeCounters.SendParamsPrepared);

    InputPost();

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::HandleChangeDetection(Buffer_t Buffer)
{
    CoderStatus_t Status = CoderNoError;
    const CoderAudioEncodingParameters_t PreviousEncodingParameters = GetConstRefPrevEncodingParameters();
    const CoderAudioCurrentParameters_t  CurrentEncodingParameters  = GetConstRefCurEncodingParameters();
    mParameterUpdateAnalysis.Prev.InputMetadata = (stm_se_uncompressed_frame_metadata_t const *)&PreviousEncodingParameters.InputMetadata;
    mParameterUpdateAnalysis.Prev.Controls      = (CoderAudioControls_t                 const *)&PreviousEncodingParameters.Controls;
    mParameterUpdateAnalysis.Prev.MmeContext    = (CoderAudioMmeDynamicContext_t        const *)&mPreviousMmeDynamicContext;
    mParameterUpdateAnalysis.Cur.InputMetadata  = (stm_se_uncompressed_frame_metadata_t const *)CurrentEncodingParameters.InputMetadata;
    mParameterUpdateAnalysis.Cur.Controls       = (CoderAudioControls_t                 const *)&CurrentEncodingParameters.Controls;
    mParameterUpdateAnalysis.Cur.MmeContext     = (CoderAudioMmeDynamicContext_t        const *)&mMmeDynamicContext;
    mParameterUpdateAnalysis.Res.ChangeDetected     =  false;
    mParameterUpdateAnalysis.Res.RestartTransformer =  false;
    mParameterUpdateAnalysis.Res.InputConfig        =  false;
    mParameterUpdateAnalysis.Res.OutputConfig       =  false;
    mParameterUpdateAnalysis.Res.CodecConfig        =  false;

    SE_DEBUG(group_encoder_audio_coder, "\n");
    AnalyseCurrentVsPreviousParameters(&mParameterUpdateAnalysis);

    /* Reminder Refs: CodedFrameBuffer{InputBuffer} */

    if (mParameterUpdateAnalysis.Res.ChangeDetected)
    {
        /* Copy Dynamic Context to previous Dynamic parameters */
        mPreviousMmeDynamicContext = mMmeDynamicContext;

        if (mParameterUpdateAnalysis.Res.RestartTransformer)
        {
            SE_INFO(group_encoder_audio_coder, "New Parameters detected - requiring restart of audioFW\n");
            // Terminate: Here we wait for commands forever unless running state changes
            /* Refs: AllMme, CodedFrameBuffer{InputBuffer} */
            Buffer_t AllMmeContexts[CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH];
            bool GotAllMmeContexts;
            Status = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);
            if (!GotAllMmeContexts || (CoderNoError != Status))
            {
                CODER_ERROR_RUNNING("AcquireAllMmeCommandContexts failed, aborting current Input()\n");
                ReleaseMainCodedBuffer();
                return CoderError;
            }
            /* Refs: AllContexts, CodedFrameBuffer{InputBuffer} */

            Status = TerminateMmeTransformer();
            if (CoderNoError != Status)
            {
                CODER_ERROR_RUNNING("Could not terminate audioFW\n");
                if (GotAllMmeContexts)
                {
                    ReleaseAllMmeCommandContexts(AllMmeContexts);
                }
                ReleaseMainCodedBuffer();
                return Status;
            }

            Status = InitializeMmeTransformer();
            if (CoderNoError != Status)
            {
                SE_ERROR("Could not re-start audioFW\n");
                if (GotAllMmeContexts)
                {
                    ReleaseAllMmeCommandContexts(AllMmeContexts);
                }
                ReleaseMainCodedBuffer();
                return Status;
            }

            if (GotAllMmeContexts)
            {
                Status = ReleaseAllMmeCommandContexts(AllMmeContexts);
                if (CoderNoError != Status)
                {
                    ReleaseMainCodedBuffer();
                    return Status;
                }
            }
            /* Refs: CodedFrameBuffer{InputBuffer} */
        }
        else
        {
            SE_INFO(group_encoder_audio_coder, "New Parameters detected - dyn update of audioFW\n");

            /* Refs: CodedFrameBuffer{InputBuffer} */
            /* Blocking: get an MME Transform context in mCurrentTransformContext*/
            Status = GetNextFreeTransformContext();
            if (CoderNoError != Status)
            {
                CODER_ERROR_RUNNING("GetNextFreeTransformContext Failed\n");
                ReleaseMainCodedBuffer();
                return Status;
            }
            /* Refs: MmeContext, CodedFrameBuffer{InputBuffer} */

            FillCurrentTransformContextSendGlobal();

            /* Send GlobalParams Command */
            if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
            {
                SE_VERBOSE(group_encoder_audio_coder, "TrnsfrmCntxt = %p\n", mCurrentTransformContext);
                SE_VERBOSE(group_encoder_audio_coder, "TrnsfrmCntxt Count = %d\n", mCurrentTransformContext->CommandCount);
                SE_VERBOSE(group_encoder_audio_coder, "Trnsfrmer %08X\n", mTransformerHandle);
                DumpMmeCmd(&mCurrentTransformContext->TransformCmd, "SendParams");
            }

            if (CoderNoError != SendCommand(&mCurrentTransformContext->TransformCmd))
            {
                SE_ERROR("Could not send Global command\n");
                ReleaseTransformContext(mCurrentTransformContext);
                mCurrentTransformContext = NULL;
                ReleaseMainCodedBuffer();
                return CoderResourceError;
            }

            /* Success! We have sent the Global Params */
            mMmeCounters.SendParamsPrepared++;
            SE_DEBUG(group_encoder_audio_coder, "SendParamsPrepared %u\n", mMmeCounters.SendParamsPrepared);
            /* Refs: CodedFrameBuffer{InputBuffer}, Context will be freed in Calback */
            mCurrentTransformContext = NULL;
        }
        SE_DEBUG(group_encoder_audio_coder, "handled detected change ok\n");
    }

    return CoderNoError;
}

void Coder_Audio_Mme_c::SetCurrentTransformContextSizesAndPointers()
{
    //These sizes are now known from Coder_Audio_Mme
    mCurrentTransformContext->StructSize                        = mMmeStaticConfiguration.SizeOfMmeTransformContext;
    mCurrentTransformContext->NumberOfAllocatedBuffert          = mMmeStaticConfiguration.NrAllocatedCodedBuffersPerTransformContext;
    mCurrentTransformContext->NumberOfAllocatedMMEBuffers       = mMmeStaticConfiguration.NrAllocatedMMEBuffersPerTransformContext;
    mCurrentTransformContext->NumberOfAllocatedMMEScatterPages  = mMmeStaticConfiguration.NrAllocatedScatterPagessPerTransformContext;
    //Others are only adddressable in Coder_Audio_Mme_Codecs
}

void Coder_Audio_Mme_c::FillMmeGlobalParamsFromDynamicContext(MME_AudioEncoderGlobalParams_t *GlobalParams)
{
    SE_DEBUG(group_encoder_audio_coder, "\n");
    GlobalParams->StructSize = sizeof(uint32_t) + sizeof(MME_AudEncInConfig_t) + sizeof(MME_AudEncPreProcessingConfig_t) + sizeof(MME_AudEncOutConfig_t);
    //Disables preproc as no Preprocessing in SDK2 Encoder
    {
        GlobalParams->PcmParams.Id                     = ACC_ENCODER_PCMPREPROCESSOR_ID;
        GlobalParams->PcmParams.StructSize             = sizeof(MME_AudEncPreProcessingConfig_t);
        GlobalParams->PcmParams.Sfc.Id                 = (eAccEncoderPcmId)PCMPROCESS_SET_ID(ACC_PCM_SFC_ID, ACC_MIX_MAIN);
        GlobalParams->PcmParams.Sfc.StructSize         = sizeof(MME_SfcGlobalParams_t);
        GlobalParams->PcmParams.Sfc.Apply              = ACC_MME_DISABLED;
        GlobalParams->PcmParams.Sfc.Config[SFC_MODE]          = AENC_SFC_AUTO_NOUT_MODE;
        GlobalParams->PcmParams.Sfc.Config[SFC_CONST_RAT_OUT] = 1;
        GlobalParams->PcmParams.Resamplex2.Id                 = (eAccEncoderPcmId)PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, ACC_MIX_MAIN);
        GlobalParams->PcmParams.Resamplex2.StructSize         = sizeof(MME_Resamplex2GlobalParams_t);
        GlobalParams->PcmParams.Resamplex2.Apply              = ACC_MME_DISABLED;
    }
    /* Set input config from current settings */
    {
        uAudioEncoderChannelConfig channel_config;
        channel_config.u32                 = 0;// Reset the channel_config to 0
        channel_config.Bits.Attenuation    = 0;
        channel_config.Bits.AudioMode      = mMmeDynamicContext.AcMode;
        channel_config.Bits.ChannelSwap    = 0;
        channel_config.Bits.DialogNorm     = 1;
        channel_config.Bits.NbChannelsSent = mMmeDynamicContext.NbChannelsSent;
        GlobalParams->InConfig.ChannelConfig = channel_config.u32;
        GlobalParams->InConfig.Id            = ACC_ENCODER_INPUT_CONFIG_ID;
        GlobalParams->InConfig.StructSize    = sizeof(MME_AudEncInConfig_t);
        GlobalParams->InConfig.SamplingFreq  = mMmeDynamicContext.FsCod;
        GlobalParams->InConfig.WordSize      = mMmeDynamicContext.WsCod;
    }
    /*Set Output config from current settings */
    {
        GlobalParams->OutConfig.Id            = mMmeStaticConfiguration.EncoderId;
        GlobalParams->OutConfig.StructSize    = sizeof(MME_AudEncOutConfig_t);
        GlobalParams->OutConfig.SamplingFreq  = mMmeDynamicContext.FsCod;
        GlobalParams->OutConfig.outputBitrate = mMmeDynamicContext.BitRate;
    }
    /* Restores Codec Specific Config to get the static part of it*/
    memcpy(GlobalParams->OutConfig.Config, mMmeDynamicContext.CodecDynamicContext.CodecSpecificConfig, sizeof(mMmeDynamicContext.CodecDynamicContext.CodecSpecificConfig));
}

CoderStatus_t Coder_Audio_Mme_c::FillMmeInitParams()
{
    CoderStatus_t Status;
    /* Get up-to-date mme context */
    Status = UpdateMmeDynamicContext();

    if (CoderNoError != Status)
    {
        SE_ERROR("Current context is not compatible\n");
        return CoderUnsupportedParameters;
    }

    memset(&mMmeAudioEncoderInitParams, 0, sizeof(MME_AudioEncoderInitParams_t));
    FillMmeGlobalParamsFromDynamicContext(&mMmeAudioEncoderInitParams.GlobalParams);
    mMmeAudioEncoderInitParams.BytesToSkipBeginScatterPage = 0;
    mMmeAudioEncoderInitParams.maxNumberSamplesPerChannel  = mStaticConfiguration.MaxNbSamplesPerInput;
    mMmeAudioEncoderInitParams.OptionFlags1                = 0;
    {
        tAudioEncoderOptionFlags1 *pFlags1 = (tAudioEncoderOptionFlags1 *)&mMmeAudioEncoderInitParams.OptionFlags1;
        pFlags1->FrameBased  = 1;
        pFlags1->DeliverASAP = 1; //1:One MME Buffer per es frame; 0: one mme_buffer per encoder
        pFlags1->Swap1       = mMmeStaticConfiguration.Swap1 ? 1 : 0;
        pFlags1->Swap2       = mMmeStaticConfiguration.Swap2 ? 1 : 0;
        pFlags1->Swap3       = mMmeStaticConfiguration.Swap3 ? 1 : 0;
    }
    mMmeAudioEncoderInitParams.StructSize = sizeof(MME_AudioEncoderInitParams_t);
    mMmeInitParams.StructSize                        = sizeof(MME_TransformerInitParams_t);
    mMmeInitParams.Priority                          = MME_PRIORITY_NORMAL;
    mMmeInitParams.Callback                          = MMECallbackStub;
    mMmeInitParams.CallbackUserData                  = (void *)this;
    mMmeInitParams.TransformerInitParamsSize         = mMmeAudioEncoderInitParams.StructSize ;
    mMmeInitParams.TransformerInitParams_p           = (MME_GenericParams_t *)&mMmeAudioEncoderInitParams;
    /* Copy Dynamic Context to previous Dynamic parameters */
    mPreviousMmeDynamicContext = mMmeDynamicContext;
    return CoderNoError;
}


bool  Coder_Audio_Mme_c::AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *CurrentParams)
{
    // number of channels sent vs. max
    if (CurrentParams->InputMetadata->audio.core_format.channel_placement.channel_count > mStaticConfiguration.MaxNbChannels)
    {
        SE_ERROR("Input Buffer has %d channels, only %d supported.",
                 CurrentParams->InputMetadata->audio.core_format.channel_placement.channel_count,
                 mStaticConfiguration.MaxNbChannels);
        return false;
    }

    // number of channels sent vs. 0
    if (0 == CurrentParams->InputMetadata->audio.core_format.channel_placement.channel_count)
    {
        SE_ERROR("Input Buffer has 0 channels");
        return false;
    }

    // sample format: only 1 allowed, with 4 bytes per sample
    if (STM_SE_AUDIO_PCM_FMT_S32LE != CurrentParams->InputMetadata->audio.sample_format)
    {
        SE_ERROR("Only STM_SE_AUDIO_PCM_FMT_S32LE is supported in Coder_Audio_Mme_c (%08X)\n",
                 CurrentParams->InputMetadata->audio.sample_format);
        return false;
    }

    uint32_t bytes_per_smp = 4; // TODO(pht) move

    // sampling frequency vs. 0
    if (0 == CurrentParams->InputMetadata->audio.core_format.sample_rate)
    {
        SE_ERROR("Input Buffer has a sampling frequency of 0 Hz\n");
        return false;
    }

    // maximum number of samples per Input: (buffer_size / (channels * bytes_per_smp)) >? Max
    if ((mStaticConfiguration.MaxNbSamplesPerInput
         * CurrentParams->InputMetadata->audio.core_format.channel_placement.channel_count
         * bytes_per_smp) <  mInputPayload.Size_B)
    {
        // (already checked and rejected potential divide by 0 above)
        SE_ERROR("Samples %u are more than maximum allowed per Input %u\n",
                 mInputPayload.Size_B / (CurrentParams->InputMetadata->audio.core_format.channel_placement.channel_count * bytes_per_smp),
                 mStaticConfiguration.MaxNbSamplesPerInput);
        return false;
    }

    return true;
}

CoderStatus_t Coder_Audio_Mme_c::InitializeMmeTransformer(void)
{
    MME_ERROR       MmeResult = MME_SUCCESS;
    CoderStatus_t   Status;
    bool CompanionIsCodedCapable;
    uint32_t SelectedCpu;

    //Check State before
    if (!TestComponentState(ComponentRunning))
    {
        SE_DEBUG(group_encoder_audio_coder, "ComponentNotRunning: abort\n");
        return CoderError;
    }

    SE_DEBUG(group_encoder_audio_coder, "\n");
    Status = FillMmeInitParams();

    if (CoderNoError != Status)
    {
        SE_ERROR("Current Context is not compatible with MME Initialisation");
        return Status;
    }

    DumpMmeInitParams(&mMmeInitParams);
    CompanionIsCodedCapable = IsCodecSupportedByCompanion(SelectedCpu);

    if (CompanionIsCodedCapable == false)
    {
        SE_ERROR("Loaded AudioFW binary on all CPU does not support this codec\n");
        return CoderUnsupportedParameters;
    }
    else
    {
        SE_INFO(group_encoder_audio_coder, "Initiating coder on CPU:%d\n", SelectedCpu);
    }

    MmeResult = MME_InitTransformer(mMmeStaticConfiguration.AudioEncoderTransformerNames[SelectedCpu], &mMmeInitParams, &mTransformerHandle);
    if (MmeResult != MME_SUCCESS)
    {
        SE_ERROR("MME_InitTransformer failed with %08X\n", MmeResult);
        return CoderResourceAllocationFailure;
    }
    SE_DEBUG(group_encoder_audio_coder, "MME_InitTransformer done\n");

    mMMECallbackPriorityBoosted      = false;

    //The Offset is zero (can be recomputed): at init or after a drain
    DelayExtractionReset();
    mOutputExtrapolation.ResetExtrapolator();

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::TerminateMmeTransformer()
{
    MME_ERROR      MmeStatus;
    CoderStatus_t  Status = CoderNoError;

    if (0 == mTransformerHandle)
    {
        SE_DEBUG(group_encoder_audio_coder, "Transformer is not started\n");
        return CoderNoError;
    }

    SE_DEBUG(group_encoder_audio_coder, "Terminating Transformer\n");
    MmeStatus = MME_TermTransformer(mTransformerHandle);

    if (MmeStatus != MME_SUCCESS)
    {
        SE_ERROR("MME_TermTransformer failed with %08X\n", MmeStatus);
        Status = CoderError;
    }

    mTransformerHandle = 0;
    SE_DEBUG(group_encoder_audio_coder, "Terminated Transformer\n");
    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::GetNextFreeTransformContext()
{
    BufferStatus_t BufStatus = BufferNoError;

    if (NULL != mCurrentTransformContext)
    {
        SE_DEBUG(group_encoder_audio_coder, "Warning CurrentTranformerContext is non NULL\n");
    }

    //Check State before
    if (!TestComponentState(ComponentRunning))
    {
        //Do not go on with command;
        SE_WARNING("Component is not running: abort\n");
        return CoderError;
    }

    mCoderContextBuffer = NULL;
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint64_t EntryTime            = OS_GetTimeInMicroSeconds();
    uint32_t GetBufferTimeOutMs   = 100;
    do
    {
        BufStatus = mCoderContextBufferPool->GetBuffer(&mCoderContextBuffer, IdentifierEncoderCoder,
                                                       UNSPECIFIED_SIZE, false, false,
                                                       GetBufferTimeOutMs);

        // Warning message every TimeBetweenMessageUs us
        if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
        {
            EntryTime = OS_GetTimeInMicroSeconds();
            SE_WARNING("%p->%p->GetBuffer still NoFreeBufferAvailable with %s\n",
                       this, mCoderContextBufferPool,
                       TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning");
        }
    }
    while ((BufferNoFreeBufferAvailable == BufStatus) && TestComponentState(ComponentRunning));

    if (BufStatus != BufferNoError || mCoderContextBuffer == NULL)
    {
        CODER_ERROR_RUNNING("Unable to get coder context buffer\n");
        mCoderContextBuffer = NULL;
        mCurrentTransformContext = NULL;
        return CoderError;
    }

    mCoderContextBuffer->SetUsedDataSize(sizeof(CoderAudioMmeTransformContext_t));

    // Obtain MmeCommand context reference
    mCoderContextBuffer->ObtainDataReference(NULL, NULL, (void **)(&mCurrentTransformContext));
    SE_ASSERT(mCurrentTransformContext != NULL); // can not be empty

    //Success
    //Check State after
    if (!TestComponentState(ComponentRunning))
    {
        //Do not go on with command;
        SE_WARNING("Component is not running: abort\n");
        // Release ContextBuffer
        mCoderContextBuffer->DecrementReferenceCount(IdentifierEncoderCoder);
        mCoderContextBuffer = NULL;
        mCurrentTransformContext = NULL;
        return CoderError;
    }

    SetCurrentTransformContextSizesAndPointers();
    mCurrentTransformContext->CoderContextBuffer = mCoderContextBuffer;
    mCurrentTransformContext->CommandCount = NrIssuedCommands();
    SE_DEBUG(group_encoder_audio_coder, "CurrentTransformContext %p count:%d\n",
             mCurrentTransformContext, mCurrentTransformContext->CommandCount);

    if (mCurrentTransformContext->NumberOfAllocatedBuffert < mMmeDynamicContext.NrCodedBufferCurrentTransform)
    {
        SE_ERROR("Tranform requires %d buffers, can only store %d in transform context\n",
                 mMmeDynamicContext.NrCodedBufferCurrentTransform, mCurrentTransformContext->NumberOfAllocatedBuffert);
        mCurrentTransformContext = NULL;
        return CoderAssertLevelError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::ResetCurrentTransformReturnParams()
{
    memset(&mCurrentTransformContext->EncoderStatusParams_p->CommonParams.Status, 0,
           sizeof(CoderAudioMmeAccessorAudioEncoderStatusCommonStatus_t));
    memset(&mCurrentTransformContext->EncoderStatusParams_p->SpecificEncoderStatusBArray, 0,
           mCurrentTransformContext->EncoderStatusParams_p->CommonParams.SpecificEncoderStatusBArraySize);
    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::UpdateMmeDynamicContext()
{
    CoderStatus_t Status = CoderNoError;
    const CoderAudioCurrentParameters_t CurrentEncodingParameters = GetConstRefCurEncodingParameters();

    /* Check current from codec */
    if (false == AreCurrentControlsAndMetadataSupported(&CurrentEncodingParameters))
    {
        SE_ERROR("Current Configuration is not supported for encoding\n");
        return CoderUnsupportedParameters;
    }

    /* Have codec fill its specifics */
    Status = UpdateDynamicCodecContext(&mMmeDynamicContext.CodecDynamicContext, &CurrentEncodingParameters);
    if (CoderNoError != Status)
    {
        SE_ERROR("Current encoding parameters are incompatible with codec (%08X)", Status);
        return CoderUnsupportedParameters;
    }

    /* From Codec Config to generic config in case we want some override e.g. margin in FrameSize */
    mMmeDynamicContext.MaxEsFrameSizeBForThisInput  = mMmeDynamicContext.CodecDynamicContext.MaxCodedFrameSize;
    mMmeDynamicContext.SamplesEncodedPerFrame       = mMmeDynamicContext.CodecDynamicContext.NrSamplePerCodedFrame;

    /* Convert Parameters to Acc variables */
    if (0 != StmSeTranslateIsoSamplingFrequencyToDiscrete(CurrentEncodingParameters.InputMetadata->audio.core_format.sample_rate,
                                                          mMmeDynamicContext.FsCod))
    {
        SE_ERROR("Frequency %d is not valid\n", CurrentEncodingParameters.InputMetadata->audio.core_format.sample_rate);
        return CoderUnsupportedParameters;
    }

    if ((STM_SE_AUDIO_PCM_FMT_S32LE != CurrentEncodingParameters.InputMetadata->audio.sample_format)
        || (0 != StmSeAudioGetWordsizeCodeFromLPcmFormat(&mMmeDynamicContext.WsCod, CurrentEncodingParameters.InputMetadata->audio.sample_format))
        || (ACC_WS32 != mMmeDynamicContext.WsCod))
    {
        SE_ERROR("Only STM_SE_AUDIO_PCM_FMT_S32LE is supported\n");
        return CoderUnsupportedParameters;
    }

    {
        StmSeAudioChannelPlacementAnalysis_t Analysis;
        stm_se_audio_channel_placement_t SortedPlacement;
        bool AudioModeIsPhysical;

        if ((0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&mMmeDynamicContext.AcMode, &AudioModeIsPhysical,
                                                                    &SortedPlacement, &Analysis,
                                                                    &CurrentEncodingParameters.InputMetadata->audio.core_format.channel_placement))
            || (ACC_MODE_ID == mMmeDynamicContext.AcMode))
        {
            SE_ERROR("Channel Assignement is invalid\n");
            return CoderUnsupportedParameters;
        }
    }

    if (0 == (mMmeDynamicContext.BytesPerSample = StmSeAudioGetNrBytesFromLpcmFormat(CurrentEncodingParameters.InputMetadata->audio.sample_format)))
    {
        SE_ERROR("LPCM Format is invalid\n");
        return CoderUnsupportedParameters;
    }

    mMmeDynamicContext.NbChannelsSent = CurrentEncodingParameters.InputMetadata->audio.core_format.channel_placement.channel_count;

    if (!CurrentEncodingParameters.Controls.BitRateCtrl.is_vbr)
    {
        mMmeDynamicContext.BitRate = CurrentEncodingParameters.Controls.BitRateCtrl.bitrate / CODER_AUDIO_BPS_PER_KBPS;
    }

    mMmeDynamicContext.NumberOfChannelsInBuffer = mMmeDynamicContext.NbChannelsSent;
    mMmeDynamicContext.Sample_Size_B            = mMmeDynamicContext.BytesPerSample;
    mMmeDynamicContext.NumberOfSamplesInBuffer  = mInputPayload.Size_B / (mMmeDynamicContext.NumberOfChannelsInBuffer * mMmeDynamicContext.Sample_Size_B);

    if (mMmeDynamicContext.NumberOfSamplesInBuffer * mMmeDynamicContext.NumberOfChannelsInBuffer * mMmeDynamicContext.Sample_Size_B != mInputPayload.Size_B)
    {
        SE_ERROR("Buffer Size %u not coherent with format\n", mInputPayload.Size_B);
        return CoderUnsupportedParameters;
    }

    if (mStaticConfiguration.MaxNbSamplesPerInput < mMmeDynamicContext.NumberOfSamplesInBuffer)
    {
        SE_ERROR("Number of samples in input buffer is larger than the current maximum %d > %d; I take care of it\n",
                 mMmeDynamicContext.NumberOfSamplesInBuffer, mStaticConfiguration.MaxNbSamplesPerInput);
    }

    mMmeCounters.MaxNbSamplesPerTransform = max(mMmeCounters.MaxNbSamplesPerTransform, mMmeDynamicContext.NumberOfSamplesInBuffer);

    if (mMmeDynamicContext.NumberOfSamplesInBuffer != 0)
    {
        mMmeDynamicContext.MaxNumberOfEncodedFrames  = (mMmeDynamicContext.NumberOfSamplesInBuffer / mMmeDynamicContext.SamplesEncodedPerFrame)
                                                       + ((mMmeDynamicContext.NumberOfSamplesInBuffer % mMmeDynamicContext.SamplesEncodedPerFrame == 0) ? 0 : 1)
                                                       + mMmeStaticConfiguration.MaxNbBufferedFrames;
    }
    else
    {
        //We need to send 1 extra buffer for the remaining samples from the last command.
        mMmeDynamicContext.MaxNumberOfEncodedFrames  =  1 + mMmeStaticConfiguration.MaxNbBufferedFrames;
    }

    mMmeDynamicContext.NrCodedBufferCurrentTransform    = mMmeDynamicContext.MaxNumberOfEncodedFrames;
    mMmeDynamicContext.MaxCodedBufferSizeBForThisInput  = mMmeDynamicContext.MaxEsFrameSizeBForThisInput;
    mMmeDynamicContext.MmeBuffersPerCodedBuffer         = 1;//Fixed in current version
    SE_DEBUG(group_encoder_audio_coder, "nb_samples:%d nb_ch:%d bitrate:%d\n",
             mMmeDynamicContext.NumberOfSamplesInBuffer, mMmeDynamicContext.NumberOfChannelsInBuffer, mMmeDynamicContext.BitRate);
    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::FillCurrentTransformContext()
{
    const CoderAudioCurrentParameters_t CurrentEncodingParameters = GetConstRefCurEncodingParameters();

    // Copy current encoding controls to transform context for later retrieval
    mCurrentTransformContext->Controls         = CurrentEncodingParameters.Controls;
    mCurrentTransformContext->ParameterUpdated = mParameterUpdateAnalysis.Res.ChangeDetected;

    // Fill Multicom Structure
    memset(&mCurrentTransformContext->TransformCmd, 0, sizeof(MME_Command_t));
    mCurrentTransformContext->TransformCmd.NumberInputBuffers  = 1;
    mCurrentTransformContext->TransformCmd.NumberOutputBuffers = mMmeDynamicContext.MmeBuffersPerCodedBuffer *
                                                                 mMmeDynamicContext.NrCodedBufferCurrentTransform;

    if (mCurrentTransformContext->NumberOfAllocatedMMEBuffers < (mCurrentTransformContext->TransformCmd.NumberOutputBuffers + 1))
    {
        SE_ERROR("Required number of coded buffers is larger than the maximum %d > %d\n",
                 mCurrentTransformContext->TransformCmd.NumberOutputBuffers, mCurrentTransformContext->NumberOfAllocatedMMEBuffers);
        return CoderAssertLevelError;
    }

    /* Input Buffer */
    memset(&mCurrentTransformContext->MMEScatterPages[0], 0, sizeof(MME_ScatterPage_t));
    mCurrentTransformContext->MMEScatterPages[0].Page_p          = mInputPayload.CachedAddress;
    mCurrentTransformContext->MMEScatterPages[0].Size            = mInputPayload.Size_B;

    memset(&mCurrentTransformContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    mCurrentTransformContext->MMEBuffers[0].NumberOfScatterPages = 1;
    mCurrentTransformContext->MMEBuffers[0].ScatterPages_p       = &mCurrentTransformContext->MMEScatterPages[0];
    mCurrentTransformContext->MMEBuffers[0].StructSize           = sizeof(MME_DataBuffer_t);
    mCurrentTransformContext->MMEBuffers[0].TotalSize            = mInputPayload.Size_B;
    mCurrentTransformContext->MMEBuffers[0].UserData_p           = (void *)this;

    /* Output Buffers */
    for (uint32_t idxbuf = 0; idxbuf < mMmeDynamicContext.NrCodedBufferCurrentTransform; idxbuf++)
    {
        void *Coded_Buffer_MME_Transformer_Suitable_Address;
        mCurrentTransformContext->CodedBuffers[idxbuf]->ObtainDataReference(NULL,
                                                                            NULL,
                                                                            &Coded_Buffer_MME_Transformer_Suitable_Address,
                                                                            CachedAddress);
        if (Coded_Buffer_MME_Transformer_Suitable_Address == NULL)
        {
            SE_ERROR("CodedBuffers[%d] Obtain Data Reference failed\n", idxbuf);
            // TODO(pht) or continue ?
            return CoderGenericBufferError;
        }

        for (uint32_t frame = 0; frame < mMmeDynamicContext.MmeBuffersPerCodedBuffer; frame++)
        {
            uint32_t   index    = 1 + mMmeDynamicContext.MmeBuffersPerCodedBuffer * idxbuf + frame;
            unsigned char *buffer_p = (unsigned char *)Coded_Buffer_MME_Transformer_Suitable_Address + frame * mMmeDynamicContext.MaxEsFrameSizeBForThisInput;
            memset(&mCurrentTransformContext->MMEScatterPages[index], 0, sizeof(MME_ScatterPage_t));
            mCurrentTransformContext->MMEScatterPages[index].Size            = mMmeDynamicContext.MaxEsFrameSizeBForThisInput;
            mCurrentTransformContext->MMEScatterPages[index].Page_p          = (void *)buffer_p;
            memset(&mCurrentTransformContext->MMEBuffers[index], 0, sizeof(MME_DataBuffer_t));
            mCurrentTransformContext->MMEBuffers[index].NumberOfScatterPages = 1;
            mCurrentTransformContext->MMEBuffers[index].ScatterPages_p       = &(mCurrentTransformContext->MMEScatterPages[index]);
            mCurrentTransformContext->MMEBuffers[index].StructSize           = sizeof(MME_DataBuffer_t);
            mCurrentTransformContext->MMEBuffers[index].TotalSize            = mMmeDynamicContext.MaxEsFrameSizeBForThisInput;
            mCurrentTransformContext->MMEBuffers[index].UserData_p           = (void *)this;
        }

        mCurrentTransformContext->MmeBuffersPerCodedBuffer                = mMmeDynamicContext.MmeBuffersPerCodedBuffer;
        mCurrentTransformContext->NumberOfCodedBufferForCurrentTransform  = mMmeDynamicContext.NrCodedBufferCurrentTransform;
    }

    /* Attach Buffers to command */
    mCurrentTransformContext->TransformCmd.DataBuffers_p  = mCurrentTransformContext->MMEDataBuffers_p;

    /* Input Buffers */
    mCurrentTransformContext->TransformCmd.DataBuffers_p[0] = &mCurrentTransformContext->MMEBuffers[0];
    /* Output buffers */
    for (uint32_t i = 0; i < mCurrentTransformContext->TransformCmd.NumberOutputBuffers; i++)
    {
        mCurrentTransformContext->TransformCmd.DataBuffers_p[1 + i] = &mCurrentTransformContext->MMEBuffers[1 + i];
        mCurrentTransformContext->TransformCmd.DataBuffers_p[1 + i]->ScatterPages_p->BytesUsed = 0;
    }

    /* Attach Return Status to command */
    ResetCurrentTransformReturnParams();
    /* Attach return params to command */
    mCurrentTransformContext->TransformCmd.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t *)mCurrentTransformContext->EncoderStatusParams_p;
    mCurrentTransformContext->TransformCmd.CmdStatus.AdditionalInfoSize = mCurrentTransformContext->EncoderStatusParams_p->CommonParams.StructSize;

    /* Set command parameters */
    memset(&mCurrentTransformContext->CoderTransformParams, 0, sizeof(MME_AudioEncoderTransformParams_t));
    mCurrentTransformContext->CoderTransformParams.AudioMode = mMmeDynamicContext.AcMode;

    /// EOS Management: pass EOS as EOF to the companion for drain
    stm_se_discontinuity_t Discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    if (STM_SE_DISCONTINUITY_CONTINUOUS != CoderDiscontinuity)
    {
        Discontinuity = CoderDiscontinuity;
        SE_DEBUG(group_encoder_audio_coder, "Coder discontinuity:%08X\n", Discontinuity);
        {
            CoderDiscontinuity = (stm_se_discontinuity_t)(CoderDiscontinuity & ~STM_SE_DISCONTINUITY_EOS);
            CoderDiscontinuity = (stm_se_discontinuity_t)(CoderDiscontinuity & ~STM_SE_DISCONTINUITY_DISCONTINUOUS);
        }
    }
    else if (STM_SE_DISCONTINUITY_CONTINUOUS != CurrentEncodingParameters.InputMetadata->discontinuity)
    {
        Discontinuity = CurrentEncodingParameters.InputMetadata->discontinuity;
        SE_DEBUG(group_encoder_audio_coder, "InputMetadata discontinuity:%08X\n", Discontinuity);
    }

    if (STM_SE_DISCONTINUITY_CONTINUOUS != Discontinuity)
    {
        mCurrentTransformContext->Discontinuity = Discontinuity; // Stores discontinuity type for callback

        if ((Discontinuity & STM_SE_DISCONTINUITY_EOS) || (Discontinuity & STM_SE_DISCONTINUITY_DISCONTINUOUS))
        {
            mCurrentTransformContext->CoderTransformParams.EndOfFile = 1; // Drains
        }

        mMmeCounters.ContinousBytesSent = 0;
    }
    else
    {
        mMmeCounters.ContinousBytesSent += mInputPayload.Size_B;
    }

    StmSeAudioAccPtsFromTimeStamp((uMME_BufferFlags *)&mCurrentTransformContext->CoderTransformParams.PtsInfo.PtsFlags,
                                  &mCurrentTransformContext->CoderTransformParams.PtsInfo.PTS,
                                  TimeStamp_c(CurrentEncodingParameters.InputMetadata->native_time,
                                              CurrentEncodingParameters.InputMetadata->native_time_format));

    mCurrentTransformContext->CoderTransformParams.numberOutputSamples  = mMmeDynamicContext.NumberOfSamplesInBuffer;
    mCurrentTransformContext->CoderTransformParams.UpdateFormat         = 0; //!< @todo
    mCurrentTransformContext->TransformCmd.Param_p                      = (MME_GenericParams_t *)&mCurrentTransformContext->CoderTransformParams;
    mCurrentTransformContext->TransformCmd.ParamSize                    = sizeof(MME_AudioEncoderTransformParams_t);

    // Fill Coded Buffer(s) metadata:
    // Most can be filled directly from here
    // Most of the metata is the same as Input Buffer (e.g. sampling frequency, channels)
    for (uint32_t idxbuf = 0; idxbuf < mMmeDynamicContext.NrCodedBufferCurrentTransform; idxbuf++)
    {
        __stm_se_frame_metadata_t *CoderFullFrameMetadata;
        mCurrentTransformContext->CodedBuffers[idxbuf]->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)&CoderFullFrameMetadata);
        SE_ASSERT(CoderFullFrameMetadata != NULL);

        stm_se_compressed_frame_metadata_t *CodedFrameMetadata = &CoderFullFrameMetadata->compressed_frame_metadata;
        memset(CodedFrameMetadata, 0, sizeof(*CodedFrameMetadata));
        CodedFrameMetadata->encoding            = mStaticConfiguration.Encoding;
        CodedFrameMetadata->native_time         = CurrentEncodingParameters.InputMetadata->native_time;
        CodedFrameMetadata->native_time_format  = CurrentEncodingParameters.InputMetadata->native_time_format;
        CodedFrameMetadata->encoded_time        = CurrentEncodingParameters.EncodeCoordinatorMetadata->encoded_time;
        CodedFrameMetadata->encoded_time_format = CurrentEncodingParameters.EncodeCoordinatorMetadata->encoded_time_format;
        CodedFrameMetadata->audio.core_format   = CurrentEncodingParameters.InputMetadata->audio.core_format;
    }

    //Encoder Delay management: take care of input PTS
    OS_LockMutex(&Lock);
    DelayExtractionInputPts(TimeStamp_c(CurrentEncodingParameters.InputMetadata->native_time,
                                        CurrentEncodingParameters.InputMetadata->native_time_format));
    OS_UnLockMutex(&Lock);
    // For EOS (and time extrapolator) pass samples per ES frame to the callback context
    mCurrentTransformContext->SamplesPerESFrame = mMmeDynamicContext.SamplesEncodedPerFrame;
    mCurrentTransformContext->TransformCmd.StructSize     = sizeof(MME_Command_t);
    mCurrentTransformContext->TransformCmd.CmdCode        = MME_TRANSFORM;
    mCurrentTransformContext->TransformCmd.CmdEnd         = MME_COMMAND_END_RETURN_NOTIFY;

    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        TimeStamp_c InputTimeStamp = TimeStamp_c(CurrentEncodingParameters.InputMetadata->native_time,
                                                 CurrentEncodingParameters.InputMetadata->native_time_format);
        uMME_BufferFlags *BufferFlags = (uMME_BufferFlags *)&mCurrentTransformContext->CoderTransformParams.PtsInfo.PtsFlags;
        SE_VERBOSE(group_encoder_audio_coder, "to send: %u nbsmpls, %u bytes, PTS:%llX (%llu ms)\n",
                   mMmeDynamicContext.NumberOfSamplesInBuffer,
                   mCurrentTransformContext->MMEScatterPages[0].Size,
                   (0 == (BufferFlags->Bits.PTS_DTS_FLAG & ACC_PTS_PRESENT)) ? 0 : mCurrentTransformContext->CoderTransformParams.PtsInfo.PTS,
                   InputTimeStamp.mSecValue());
    }

    return CoderNoError;
}

void Coder_Audio_Mme_c::FillCurrentTransformContextSendGlobal()
{
    memset(&mCurrentTransformContext->TransformCmd, 0, sizeof(mCurrentTransformContext->TransformCmd));
    FillMmeGlobalParamsFromDynamicContext(&mCurrentTransformContext->GlobalParams);
    mCurrentTransformContext->TransformCmd.StructSize                   = sizeof(MME_Command_t);
    mCurrentTransformContext->TransformCmd.NumberInputBuffers           = 0;
    mCurrentTransformContext->TransformCmd.NumberOutputBuffers          = 0;
    mCurrentTransformContext->TransformCmd.DataBuffers_p                = NULL;
    mCurrentTransformContext->TransformCmd.CmdCode                      = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    mCurrentTransformContext->TransformCmd.CmdEnd                       = MME_COMMAND_END_RETURN_NOTIFY;
    mCurrentTransformContext->TransformCmd.CmdStatus.AdditionalInfo_p   = NULL;
    mCurrentTransformContext->TransformCmd.CmdStatus.AdditionalInfoSize = 0;
    mCurrentTransformContext->TransformCmd.Param_p                  = (MME_GenericParams_t *)&mCurrentTransformContext->GlobalParams;
    mCurrentTransformContext->TransformCmd.ParamSize                = mCurrentTransformContext->GlobalParams.StructSize;

    if (!mParameterUpdateAnalysis.Res.InputConfig)
    {
        // Tell companion to not parse input config if no input parameter change
        // This also works as a work-around for bug 27753
        mCurrentTransformContext->GlobalParams.InConfig.Id = ACC_ENCODER_LAST_ID;
    }

    if (!mParameterUpdateAnalysis.Res.OutputConfig)
    {
        // Tell companion to not parse output config if no output parameter change
        mCurrentTransformContext->GlobalParams.OutConfig.Id = ACC_ENCODER_LAST_ID;
    }
}

CoderStatus_t Coder_Audio_Mme_c::SendCommand(MME_Command_t *MmeCommand)
{
    MME_ERROR status = MME_SUCCESS;
    SE_DEBUG(group_encoder_audio_coder, "\n");
    status = MME_SendCommand(mTransformerHandle, MmeCommand);

    if (MME_SUCCESS != status)
    {
        SE_ERROR("MME_SendCommand failed:%08X\n", status);
        return CoderResourceError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Audio_Mme_c::ParseMmeTransformCallback(CoderAudioMmeTransformContext_t                 *TrnsfrmCntxt
                                                           , stm_se_uncompressed_frame_metadata_t         **InputMetadata_p
                                                           , MME_AudioEncoderFrameBufferStatus_t          **MmmeAudioEncoderBufferFrameStatus_p)
{
    *InputMetadata_p = NULL;
    *MmmeAudioEncoderBufferFrameStatus_p = NULL;
    MME_AudioEncoderStatusParams_t              *MmeEncoderStatus_p;

    if (MME_SUCCESS != TrnsfrmCntxt->TransformCmd.CmdStatus.Error)
    {
        SE_ERROR("Transform CmdStatus.Error:%08X\n",  TrnsfrmCntxt->TransformCmd.CmdStatus.Error);
        return CoderAssertLevelError;
    }

    if (MME_COMMAND_COMPLETED != TrnsfrmCntxt->TransformCmd.CmdStatus.State)
    {
        SE_ERROR("Transform CmdStatus.State:%08X\n", TrnsfrmCntxt->TransformCmd.CmdStatus.State);
        return CoderAssertLevelError;
    }

    if (0 == TrnsfrmCntxt->NumberOfCodedBufferForCurrentTransform)
    {
        SE_ERROR("This Transforms has no coded buffers to process\n");
        return CoderAssertLevelError;
    }

    MmeEncoderStatus_p = (MME_AudioEncoderStatusParams_t *)TrnsfrmCntxt->TransformCmd.CmdStatus.AdditionalInfo_p;

    if (NULL == MmeEncoderStatus_p)
    {
        SE_ERROR("MmeEncoderStatus_p is NULL\n");
    }
    else
    {
        *MmmeAudioEncoderBufferFrameStatus_p = (MME_AudioEncoderFrameBufferStatus_t *)MmeEncoderStatus_p->SpecificEncoderStatusBArray;
        if (NULL == *MmmeAudioEncoderBufferFrameStatus_p)
        {
            SE_ERROR("MmmeAudioEncoderBufferFrameStatus_p is NULL\n");
        }
    }

    if ((NULL == MmeEncoderStatus_p) || (NULL == *MmmeAudioEncoderBufferFrameStatus_p))
    {
        return CoderAssertLevelError;
    }

    if (NULL == TrnsfrmCntxt->EncoderStatusParams_p)
    {
        SE_ERROR("TrnsfrmCntxt->EncoderStatusParams_p is NULL\n");
        return CoderAssertLevelError;
    }

    if (ACC_ENCODER_NO_ERROR != TrnsfrmCntxt->EncoderStatusParams_p->CommonParams.Status.EncoderStatus)
    {
        SE_ERROR("Transform CommonParams.Status.EncoderStatus = %08X\n", TrnsfrmCntxt->EncoderStatusParams_p->CommonParams.Status.EncoderStatus);
        return CoderError;
    }

    /* Check that all coded buffers are valid with metadata */
    /* any error here is a fatal error */
    if (TrnsfrmCntxt->NumberOfCodedBufferForCurrentTransform != TrnsfrmCntxt->NumberOfCodedBufferReferencesTaken)
    {
        SE_ERROR("NumberOfCodedBufferForCurrentTransform (%d) != NumberOfCodedBufferReferencesTaken (%d)\n",
                 TrnsfrmCntxt->NumberOfCodedBufferForCurrentTransform, TrnsfrmCntxt->NumberOfCodedBufferReferencesTaken);
        return CoderAssertLevelError;
    }

    for (uint32_t i = 0 ; i < TrnsfrmCntxt->NumberOfCodedBufferForCurrentTransform; i++)
    {
        if (NULL == TrnsfrmCntxt->CodedBuffers[i])
        {
            SE_ERROR("TrnsfrmCntxt->CodedBuffers[%d] is NULL\n", i);
            return CoderAssertLevelError;
        }

        __stm_se_frame_metadata_t *CodedFrameMetadata;
        TrnsfrmCntxt->CodedBuffers[i]->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)&CodedFrameMetadata);
        SE_ASSERT(CodedFrameMetadata != NULL);
    }

    /* Get input buffer only to get its metadata */
    __stm_se_frame_metadata_t *PreProcMetaDataDescriptor;
    Buffer_t InputBuffer;
    TrnsfrmCntxt->CodedBuffers[0]->ObtainAttachedBufferReference(InputBufferType, &InputBuffer);
    SE_ASSERT(InputBuffer != NULL);

    /* Get Metadata reference */
    InputBuffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&PreProcMetaDataDescriptor));
    SE_ASSERT(PreProcMetaDataDescriptor != NULL);

    *InputMetadata_p = &PreProcMetaDataDescriptor->uncompressed_frame_metadata;

    return CoderNoError;
}

uint32_t Coder_Audio_Mme_c::GetNrOfBytesForThisCodedBufferInMmeTransformCallback(
    CoderAudioMmeTransformContext_t *TrnsfrmCntxt,
    uint32_t coded_buffer_ix,
    MME_AudioEncoderFrameBufferStatus_t **MmmeAudioEncoderBufferFrameStatus_p)
{
    uint32_t CodedBufferByteUsed = 0;
    uint32_t NMmeBufPerCodedBuf  = TrnsfrmCntxt->MmeBuffersPerCodedBuffer;

    for (uint32_t mme_buf_ix = 0; mme_buf_ix < NMmeBufPerCodedBuf; mme_buf_ix++)
    {
        uint32_t Size = TrnsfrmCntxt->MMEDataBuffers_p[1 + coded_buffer_ix * NMmeBufPerCodedBuf + mme_buf_ix]->ScatterPages_p->BytesUsed;
        CodedBufferByteUsed += Size;
        (*MmmeAudioEncoderBufferFrameStatus_p)++;
    }

    return CodedBufferByteUsed;
}

void Coder_Audio_Mme_c::FillCodedBufferProcessMetadata(stm_se_encode_process_metadata_t *ProcessMetadata,
                                                       const CoderAudioControls_t *TrsfrmControls,
                                                       bool Update)
{
    if ((NULL == ProcessMetadata) || (NULL == TrsfrmControls))
    {
        return;
    }

    ProcessMetadata->updated_metadata = Update || ProcessMetadata->updated_metadata; //Could have been set to true by preproc
    ProcessMetadata->audio.bitrate = TrsfrmControls->BitRateCtrl;

    if (!TrsfrmControls->StreamIsCopyrighted)
    {
        ProcessMetadata->audio.scms = STM_SE_AUDIO_NO_COPYRIGHT;
        //We treat (false, false) as valid, STM_SE_AUDIO_NO_COPYRIGHT
    }
    else
    {
        if (TrsfrmControls->OneCopyIsAuthorized)
        {
            ProcessMetadata->audio.scms = STM_SE_AUDIO_ONE_MORE_COPY_AUTHORISED;
        }
        else
        {
            ProcessMetadata->audio.scms = STM_SE_AUDIO_NO_FUTHER_COPY_AUTHORISED;
        }
    }

    ProcessMetadata->audio.crc_on   = TrsfrmControls->CrcOn;
    ProcessMetadata->audio.loudness = TrsfrmControls->TargetLoudness;
    // ProcessMetadata->audio.core_format       // From preproc
    // ProcessMetadata->audio.dual_mono_forced; // From preproc
    // ProcessMetadata->audio.dual_mode;        // From preroc
}

CoderStatus_t Coder_Audio_Mme_c::FillCodedBufferMetadata(CoderAudioMmeTransformContext_t *TrnsfrmCntxt,
                                                         uint32_t coded_buffer_ix,
                                                         stm_se_uncompressed_frame_metadata_t *InputMetadata_p,
                                                         MME_AudioEncoderFrameBufferStatus_t *AudioEncoderBufferFrameStatus_p,
                                                         bool LastBufferInTransform)
{
    Buffer_t CodedBuffer = TrnsfrmCntxt->CodedBuffers[coded_buffer_ix];

    __stm_se_frame_metadata_t *CoderFullFrameMetadata;
    CodedBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)&CoderFullFrameMetadata);
    SE_ASSERT(CoderFullFrameMetadata != NULL);

    stm_se_encode_process_metadata_t   *ProcessMetadata;
    stm_se_compressed_frame_metadata_t *CodedFrameMetadata;
    CoderAudioControls_t               *TrsfrmControls;
    CodedFrameMetadata = &CoderFullFrameMetadata->compressed_frame_metadata;
    ProcessMetadata    = &CoderFullFrameMetadata->encode_process_metadata;   //Cannot be memset as carries info from Preproc
    TrsfrmControls     = &TrnsfrmCntxt->Controls;
    //First Process Metadata
    FillCodedBufferProcessMetadata(ProcessMetadata, TrsfrmControls, TrnsfrmCntxt->ParameterUpdated);
    TrnsfrmCntxt->ParameterUpdated = false;
    // Fill Compressed Metata where required
    CodedFrameMetadata->encoding                  = mStaticConfiguration.Encoding;
    CodedFrameMetadata->audio.core_format         = InputMetadata_p->audio.core_format;
    CodedFrameMetadata->audio.drc_factor          = 0;     //@todo
    /* Time Stamp Handling: take care of time transforms and wrapping */
    TimeStamp_c InputTimeStamp = TimeStamp_c(CodedFrameMetadata->native_time, CodedFrameMetadata->native_time_format);
    TimeStamp_c InputNrtTimeStamp = TimeStamp_c(CodedFrameMetadata->encoded_time, CodedFrameMetadata->encoded_time_format);
    TimeStamp_c TransformInputTimeStamp =
        StmSeAudioTimeStampFromAccPts((const uMME_BufferFlags * const)&TrnsfrmCntxt->CoderTransformParams.PtsInfo.PtsFlags,
                                      (const uint64_t *const)&TrnsfrmCntxt->CoderTransformParams.PtsInfo.PTS);
    TimeStamp_c TransformOutputTimeStamp =
        StmSeAudioTimeStampFromAccPts((const uMME_BufferFlags * const)&AudioEncoderBufferFrameStatus_p->PtsInfo.PtsFlags,
                                      (const uint64_t *const)&AudioEncoderBufferFrameStatus_p->PtsInfo.PTS);
    // We have to decorrelate Input and output time stamps.
    // We could extrapolate the output timestamp from the first valid input timestamp but would result in accumulated error.
    // Or
    // Better: we use the timestamp from the companion
    TimeStamp_c OutputTimeStamp; //Invalid
    bool IsDelay;
    OS_LockMutex(&Lock);
    TimeStamp_c Offset = DelayExtractionGetDelay(IsDelay, TransformOutputTimeStamp);
    OS_UnLockMutex(&Lock);
    /// For the moment we do not do anything with the delay, we output TransformOutputTimeStamp,
    /// but in the future we will offset the delay from TransformOutputTimeStamp, and move the
    /// delay to a separate field.
    OutputTimeStamp = TransformOutputTimeStamp;
    //Get Extrapolated OutputTimeStamp (in case OutputTimeStamp is invalid, like is the case for EOS)
    OutputTimeStamp = mOutputExtrapolation.GetTimeStamp(OutputTimeStamp,
                                                        TrnsfrmCntxt->SamplesPerESFrame,
                                                        CodedFrameMetadata->audio.core_format.sample_rate);

    //@todo activate this once we have a field for the delay in metadata
    if ((0 == 1)
        && Offset.IsValid()
        && (0 != Offset.Value()))
    {
        if (IsDelay)
        {
            OutputTimeStamp = TransformOutputTimeStamp + Offset;
            //Add Offset to delay from preproc in process medadata
        }
    }

    // Adjust the output time stamp
    // We also have to take care of reporting this delay on encoded_time
    // Here we assume encoded_time is always expressed in the same unit than native_time, as set by the EncodeCoordinator
    if (InputTimeStamp.IsValid() && InputNrtTimeStamp.IsValid())
    {
        /**
         * mNrtOutputSavedOffset is 0 at initial value (vs. invalid): so
         *      if native or encoded time stamps are invalid and this is
         *      the first time this code sets encoded_time to
         *      native_time. Review code if this is not desired outcome
         *      (and Invalid is desired, or something else).
         *
         *  What about
         * (InputTimeStamp.IsValid() != InputNrtTimeStamp.IsValid())
         *      Is it possible to have this? in that case what should
         * be the behaviours ?
         */
        mNrtOutputSavedOffset = CodedFrameMetadata->encoded_time - CodedFrameMetadata->native_time;
    }
    CodedFrameMetadata->native_time = OutputTimeStamp.Value(CodedFrameMetadata->native_time_format);
    CodedFrameMetadata->encoded_time = CodedFrameMetadata->native_time + mNrtOutputSavedOffset;
    SE_DEBUG(group_encoder_audio_coder, "Frame received PTS:%llu ms\n", OutputTimeStamp.mSecValue());

    /* Discontinuity Management */
    CodedFrameMetadata->discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS; //Default value

    if (LastBufferInTransform)
    {
        CodedFrameMetadata->discontinuity = TrnsfrmCntxt->Discontinuity;

        if (STM_SE_DISCONTINUITY_CONTINUOUS != CodedFrameMetadata->discontinuity)
        {
            // Discontinuity: reset the extrapolation
            mOutputExtrapolation.ResetExtrapolator(mOutputExtrapolation.TimeFormat());
            OS_LockMutex(&Lock);
            DelayExtractionReset();
            OS_UnLockMutex(&Lock);
        }

        //Print we we have EOS
        if ((STM_SE_DISCONTINUITY_EOS == (CodedFrameMetadata->discontinuity & STM_SE_DISCONTINUITY_EOS)))
        {
            SE_DEBUG(group_encoder_audio_coder, "discontinuity:STM_SE_DISCONTINUITY_EOS\n");
        }
        else
        {
            SE_DEBUG(group_encoder_audio_coder, "discontinuity:%08X\n", CodedFrameMetadata->discontinuity);
        }
    }

    return CoderNoError;
}

void Coder_Audio_Mme_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    if (NULL == CallbackData)
    {
        SE_ERROR("Fatal: No CallbackData from MME!\n");
        return;
    }

    CoderAudioMmeTransformContext_t *TrnsfrmCntxt = (CoderAudioMmeTransformContext_t *)CallbackData;

    /* Refs: at least MmeContext */
    if (NULL == TrnsfrmCntxt->CoderContextBuffer)
    {
        SE_ERROR("Fatal: CoderContextBuffer is NULL\n");
        return;
    }

    switch (CallbackData->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
    {
        /* Refs: MmeContext */
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            mMmeCounters.SendParamsCompleted++;
            SE_DEBUG(group_encoder_audio_coder, "MME_SET_GLOBAL_TRANSFORM_PARAMS completed (%d)\n",
                     mMmeCounters.SendParamsCompleted);

            // Test for returned status
            if ((MME_SUCCESS != TrnsfrmCntxt->TransformCmd.CmdStatus.Error)
                || (MME_COMMAND_COMPLETED != TrnsfrmCntxt->TransformCmd.CmdStatus.State))
            {
                SE_ERROR("GlobalParams execution reported error:\n");
                SE_ERROR("  TransformCmd.CmdStatus.Error = %08X\n",  TrnsfrmCntxt->TransformCmd.CmdStatus.Error);
                SE_ERROR("  TransformCmd.CmdStatus.State = %08X\n", TrnsfrmCntxt->TransformCmd.CmdStatus.State);

                if (SE_IS_DEBUG_ON(group_encoder_audio_coder))
                {
                    SE_DEBUG(group_encoder_audio_coder, "  TrnsfrmCntxt = %p\n", TrnsfrmCntxt);
                    SE_DEBUG(group_encoder_audio_coder, "  TrnsfrmCntxt Count = %d\n", TrnsfrmCntxt->CommandCount);
                    DumpMmeCmd(&TrnsfrmCntxt->TransformCmd, "ComplParams");
                }
            }
        }
        else
        {
            mMmeCounters.SendParamsAborted++;
            SE_ERROR("MME_SET_GLOBAL_TRANSFORM_PARAMS aborted (%d)\n",
                     mMmeCounters.SendParamsAborted);

            if (SE_IS_DEBUG_ON(group_encoder_audio_coder))
            {
                SE_DEBUG(group_encoder_audio_coder, "TrnsfrmCntxt = %p\n", TrnsfrmCntxt);
                SE_DEBUG(group_encoder_audio_coder, "TrnsfrmCntxt Count = %d\n", TrnsfrmCntxt->CommandCount);
                DumpMmeCmd(&TrnsfrmCntxt->TransformCmd, "FailedParams");
            }
        }

        /* Refs: MmeContext */
    }
    break;

    case MME_TRANSFORM:
    {
        /* Refs: MmeContext, NumberOfCodedBufferReferencesTaken*CodedFrameBuffer{InputBuffer} */
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            mMmeCounters.TransformCompleted++;
            SE_VERBOSE(group_encoder_audio_coder, "MME_TRANSFORM completed (%d)\n",
                       mMmeCounters.TransformCompleted);

            uint32_t NrCodedBuffers             = TrnsfrmCntxt->NumberOfCodedBufferForCurrentTransform;
            uint32_t TotalCodedBytesInTransform = 0;
            uint32_t CodedBytesInTranformSoFar  = 0;

            MME_AudioEncoderFrameBufferStatus_t   *MmmeAudioEncoderBufferFrameStatus_p;
            stm_se_uncompressed_frame_metadata_t  *InputMetadata_p;
            CoderStatus_t Status = ParseMmeTransformCallback(TrnsfrmCntxt, &InputMetadata_p, &MmmeAudioEncoderBufferFrameStatus_p);
            if (CoderNoError != Status)
            {
                if (CoderAssertLevelError == Status)
                {
                    SE_ERROR("ParseMmeTransformCallback has status CoderAssertLevelError\n");
                }

                if (SE_IS_DEBUG_ON(group_encoder_audio_coder))
                {
                    SE_DEBUG(group_encoder_audio_coder, "TrnsfrmCntxt = %p\n", TrnsfrmCntxt);
                    SE_DEBUG(group_encoder_audio_coder, "TrnsfrmCntxt Count = %d\n", TrnsfrmCntxt->CommandCount);
                    DumpMmeCmd(&TrnsfrmCntxt->TransformCmd, "FailedTransf");
                }
            }

            // Get total number of bytes in this transform
            if (CoderNoError == Status)
            {
                for (uint32_t coded_buffer_ix = 0; coded_buffer_ix < NrCodedBuffers ; coded_buffer_ix++)
                {
                    MME_AudioEncoderFrameBufferStatus_t *TmpFrameStatus = MmmeAudioEncoderBufferFrameStatus_p;
                    TotalCodedBytesInTransform += GetNrOfBytesForThisCodedBufferInMmeTransformCallback(TrnsfrmCntxt,
                                                                                                       coded_buffer_ix,
                                                                                                       &TmpFrameStatus);
                }
            }

            for (uint32_t coded_buffer_ix = 0; ((coded_buffer_ix < NrCodedBuffers) && (CoderNoError == Status)); coded_buffer_ix++)
            {
                MME_AudioEncoderFrameBufferStatus_t *AudioEncoderBufferFrameStatusToUse_p = MmmeAudioEncoderBufferFrameStatus_p;
                uint32_t CodedBufferByteUsed = 0;
                CodedBufferByteUsed = GetNrOfBytesForThisCodedBufferInMmeTransformCallback(
                                          TrnsfrmCntxt, coded_buffer_ix, &MmmeAudioEncoderBufferFrameStatus_p);
                CodedBytesInTranformSoFar += CodedBufferByteUsed;

                /**
                 * If we wanted to get the EOF from companion (but we don't need to and this is
                 * good because we need to update companion to support this feature):<p>
                 * if(0 != AudioEncoderBufferFrameStatusToUse_p->Flags &
                 * ACC_ENCODER_FRAME_STATUS_EOF)<p>
                 *  ThisBufferHasEofMarkerFromCompanion = true;
                 */

                // Release buffer with no data to pool
                // except if no data for transform and first coded buffer and discontinuity,
                // in which case we output an empty buffer with discontinuity flag
                if ((0 == CodedBufferByteUsed)
                    && !((0 == TotalCodedBytesInTransform)
                         && (0 == coded_buffer_ix)
                         && (TrnsfrmCntxt->Discontinuity)
                        )
                   )
                {
                    /* Return to pool */
                    SE_VERBOSE(group_encoder_audio_coder, "Releasing empty CodedBuffer[%d]\n", coded_buffer_ix);

                    ReleaseCodedBufferFromTransformContext(TrnsfrmCntxt, coded_buffer_ix);
                    /* Refs: MmeContext, (NumberOfCodedBufferReferencesTaken-(coded_buffer_ix+1))*CodedFrameBuffer{InputBuffer} */
                }
                else
                {
                    /* Send buffer out */
                    /* Fill Metadata */
                    if (CoderNoError != FillCodedBufferMetadata(TrnsfrmCntxt,
                                                                coded_buffer_ix,
                                                                InputMetadata_p,
                                                                AudioEncoderBufferFrameStatusToUse_p,
                                                                CodedBytesInTranformSoFar == TotalCodedBytesInTransform))
                    {
                        SE_ERROR("Unable to Fill Output Metadata\n");
                        Status = CoderAssertLevelError;
                    }
                    else
                    {
                        /* Put buffer in Ring */
                        SE_DEBUG(group_encoder_audio_coder, "Sending CodedBuffer[%d] with %d bytes to ring\n", coded_buffer_ix, CodedBufferByteUsed);

                        if (CoderNoError == SendCodedBufferOut(TrnsfrmCntxt->CodedBuffers[coded_buffer_ix], CodedBufferByteUsed))
                        {
                            TrnsfrmCntxt->NumberOfCodedBufferReferencesFreed++;
                            /* Refs: MmeContext, (NumberOfCodedBufferReferencesTaken-(coded_buffer_ix+1))*CodedFrameBuffer{InputBuffer} */
                            /* One InputBufferDeatched, One CodedFrameBuffer passed to Ring */
                        }
                        else
                        {
                            SE_ERROR("SendCodedBufferOut failed\n");
                            Status = CoderAssertLevelError;
                        }
                    }
                }
            } //End on loop on each coded buffer

            /* In case of any error, we need to free all remaining CodedBuffers*/
            if ((CoderNoError == Status)
                && (TrnsfrmCntxt->NumberOfCodedBufferReferencesFreed != TrnsfrmCntxt->NumberOfCodedBufferReferencesTaken))
            {
                /* This should be impossible: no error but some CodedBuffers no freed */
                SE_ERROR("!! Status is CoderNoError but only released %d out of %d !!\n",
                         TrnsfrmCntxt->NumberOfCodedBufferReferencesFreed,
                         TrnsfrmCntxt->NumberOfCodedBufferReferencesTaken);
                Status = CoderError;
            }

            /* Release all remaining buffers */
            if (CoderNoError != Status)
            {
                ReleaseAllCodedBufferInMmeTransformContext(TrnsfrmCntxt, "MME Callback Error cleanup");
            }
            /* Refs: MmeContext */
        }
        else
        {
            mMmeCounters.TransformAborted++;
            SE_ERROR("MME_TRANSFORM did not complete returned with %08X (%d)\n", Event,
                     mMmeCounters.TransformAborted);

            ReleaseAllCodedBufferInMmeTransformContext(TrnsfrmCntxt);
            /* Refs: MmeContext */
        }
    }
    break;

    default:
        SE_ERROR("Unknown CmdCode 0x%08X\n", CallbackData->CmdCode);
        break;
    }

    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_VERBOSE(group_encoder_audio_coder, "SendParamsAborted %d - SendParamsCompleted %d\n"
                   , mMmeCounters.SendParamsAborted, mMmeCounters.SendParamsCompleted);
        SE_VERBOSE(group_encoder_audio_coder, "TransformAborted %d - TransformCompleted %d\n"
                   , mMmeCounters.TransformAborted, mMmeCounters.TransformCompleted);
        SE_VERBOSE(group_encoder_audio_coder, "CodedBufferOut %d - CodedBytesOut %d\n"
                   , mCoderAudioStatistics.OutputFrames, mCoderAudioStatistics.OutputBytes);
    }

    // boost the callback priority to be the same as the CoderToOutput process
    // coder audio might not have global params so done in global case
    // (as opposed to what is done in other such threads in se)
    if (!mMMECallbackPriorityBoosted)
    {
        OS_SetPriority(&player_tasks_desc[SE_TASK_ENCOD_AUDCTOO]);
        mMMECallbackPriorityBoosted = true;
    }

    // Release command context buffer
    ReleaseTransformContext(TrnsfrmCntxt, "MME Callback");
}

void Coder_Audio_Mme_c::AnalyseCurrentVsPreviousParameters(CoderAudioMmeParameterUpdateAnalysis_t *Analysis)
{
    Analysis->Res.ChangeDetected     =  false;
    Analysis->Res.RestartTransformer =  false;
    Analysis->Res.InputConfig        =  false;
    Analysis->Res.OutputConfig       =  false;
    Analysis->Res.CodecConfig        =  false;

    if (Analysis->Prev.MmeContext->AcMode != Analysis->Cur.MmeContext->AcMode)
    {
        SE_INFO(group_encoder_audio_coder, "Input AcMode change detected %08X -> %08X\n",
                Analysis->Prev.MmeContext->AcMode, Analysis->Cur.MmeContext->AcMode);
        Analysis->Res.InputConfig  = true;
    }

    if (Analysis->Prev.MmeContext->NbChannelsSent != Analysis->Cur.MmeContext->NbChannelsSent)
    {
        SE_INFO(group_encoder_audio_coder, "Input Buffer Interleaving change detected %d -> %d\n",
                Analysis->Prev.MmeContext->NbChannelsSent, Analysis->Cur.MmeContext->NbChannelsSent);
        Analysis->Res.InputConfig  = true;
        Analysis->Res.RestartTransformer = true;
    }

    if (Analysis->Prev.MmeContext->WsCod != Analysis->Cur.MmeContext->WsCod)
    {
        SE_INFO(group_encoder_audio_coder, "Input WsCode change detected %08X -> %08X\n",
                (uint32_t)Analysis->Prev.MmeContext->WsCod, (uint32_t)Analysis->Cur.MmeContext->WsCod);
        Analysis->Res.InputConfig  = true;
        Analysis->Res.RestartTransformer = true;
    }

    if (Analysis->Prev.MmeContext->FsCod != Analysis->Cur.MmeContext->FsCod)
    {
        SE_INFO(group_encoder_audio_coder, "Input Sampling Frequency change detected %d -> %d\n",
                Analysis->Prev.MmeContext->FsCod, Analysis->Cur.MmeContext->FsCod);
        Analysis->Res.RestartTransformer = true;
        Analysis->Res.InputConfig  = true;
        Analysis->Res.OutputConfig = true;
    }

    if ((Analysis->Prev.Controls->BitRateCtrl.is_vbr != Analysis->Cur.Controls->BitRateCtrl.is_vbr))
    {
        SE_INFO(group_encoder_audio_coder, "Output bitrate vbr on control change detected %01X -> %01X\n",
                Analysis->Prev.Controls->BitRateCtrl.is_vbr, Analysis->Cur.Controls->BitRateCtrl.is_vbr);
        Analysis->Res.OutputConfig = true;
    }
    else
    {
        //Let codecs that support it decide whether to check Controls->BitRateCtrl.bitrate_cap or not
        if (!Analysis->Cur.Controls->BitRateCtrl.is_vbr)
        {
            if (Analysis->Prev.Controls->BitRateCtrl.bitrate != Analysis->Cur.Controls->BitRateCtrl.bitrate)
            {
                SE_INFO(group_encoder_audio_coder, "Output bitrate change detected %d -> %d\n",
                        Analysis->Prev.Controls->BitRateCtrl.bitrate, Analysis->Cur.Controls->BitRateCtrl.bitrate);
                Analysis->Res.OutputConfig = true;
            }
        }
        else
        {
            if (Analysis->Prev.Controls->BitRateCtrl.vbr_quality_factor != Analysis->Cur.Controls->BitRateCtrl.vbr_quality_factor)
            {
                SE_INFO(group_encoder_audio_coder, "Output VBR Q change detected %d -> %d\n",
                        Analysis->Prev.Controls->BitRateCtrl.vbr_quality_factor, Analysis->Cur.Controls->BitRateCtrl.vbr_quality_factor);
                Analysis->Res.OutputConfig = true;
            }
        }
    }

    if (Analysis->Prev.Controls->TargetLoudness != Analysis->Cur.Controls->TargetLoudness)
    {
        SE_INFO(group_encoder_audio_coder, "Output loudness control change detected %d -> %d\n",
                Analysis->Prev.Controls->TargetLoudness, Analysis->Cur.Controls->TargetLoudness);
        Analysis->Res.OutputConfig = true;
    }

    if ((Analysis->Res.InputConfig) && (!(Analysis->Res.RestartTransformer)))
    {
        /* @note Workaround to bug 27789. @todo re-evaluate  when bug 27789 is fixed */
        SE_INFO(group_encoder_audio_coder, "Input Configuration change: restart transformer\n");
        Analysis->Res.RestartTransformer = true;
    }

    Analysis->Res.ChangeDetected = Analysis->Res.RestartTransformer
                                   || Analysis->Res.InputConfig
                                   || Analysis->Res.OutputConfig
                                   || Analysis->Res.CodecConfig;
}

void Coder_Audio_Mme_c::InputPost()
{
    /* Let parent class do some post process */
    Coder_Audio_c::InputPost();
}

bool Coder_Audio_Mme_c::IsCodecSupportedByCompanion(uint32_t &SelectedCpu)
{
    bool Supported = false;

    if (0 == GetCodecCapabilityMask())
    {
        SE_ERROR("Could not get a valid CapabilityMask\n");
        return CoderError;
    }

    SelectedCpu = Encoder.EncodeStream->AudioEncodeNo;
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

/**
 * @brief Checks Capability of the AudioFW on the selected CPU.
 *
 *
 * @return Supported == true if the CPU has the capability on SelectedCpu.
 */
bool Coder_Audio_Mme_c::CheckCompanionCapability(uint32_t SelectedCpu)
{
    MME_AudioEncoderInfo_t      infoEncoder;
    MME_TransformerCapability_t MMECapability;
    uint32_t                    CapabilityMask = 0;
    bool Supported = false;
    MMECapability.StructSize          = sizeof(MME_TransformerCapability_t);
    MMECapability.TransformerInfoSize = sizeof(MME_AudioEncoderInfo_t);
    MMECapability.TransformerInfo_p   = &infoEncoder;
    CapabilityMask = GetCodecCapabilityMask();

    if (MME_SUCCESS != MME_GetTransformerCapability(mMmeStaticConfiguration.AudioEncoderTransformerNames[SelectedCpu], &MMECapability))
    {
        SE_INFO(group_encoder_audio_coder, "Failed to get audioFW encoder capability on CPU:%d", SelectedCpu);
    }
    else
    {
        Supported = (0 != (infoEncoder.EncoderCapabilityFlags & (1 << CapabilityMask)));
    }

    return Supported;
}

unsigned int Coder_Audio_Mme_c::GetMaxCodedBufferSize(void)
{
    return (unsigned int)mStaticConfiguration.MaxCodedFrameSize;
}

unsigned int Coder_Audio_Mme_c::GetMaxNrCodedBuffersPerTransform(void)
{
    int MaxNbFramesProduced;

    if (0 == mStaticConfiguration.MinNbSamplesPerCodedFrame)
    {
        SE_FATAL("mStaticConfiguration.MinNbSamplesPerCodedFrame is 0\n");
        return 0; // in case fatal not lethal
    }

    /* At any point of time (excluding intrisic delays & bit buffers), the max nb of frames a single input can produce due to input samples */
    MaxNbFramesProduced = (mStaticConfiguration.MaxNbSamplesPerInput / mStaticConfiguration.MinNbSamplesPerCodedFrame) + 1;

    /* To that we add the internal delays that can be flushed anytime (mp3 bit buffer) or upon drain command */
    MaxNbFramesProduced += mMmeStaticConfiguration.MaxNbBufferedFrames;

    return (unsigned int)MaxNbFramesProduced;
}

unsigned int Coder_Audio_Mme_c::GetNrCodedBuffersToAllocate(void)
{
    int NumberOfCodedBuffersToAllocate;

    int MaximumUpsamplingRatio = CODER_AUDIO_MME_MAX_UPSAMPLING_RATIO_TO_ABSORB_WITHOUT_BLOCKING;
    int MaximumNrOfInjectionWithoutPull = CODER_AUDIO_MME_MAX_NR_INJECTION_TO_ABSORB_WITHOUT_BLOCKING;

    NumberOfCodedBuffersToAllocate = GetMaxNrCodedBuffersPerTransform();

    if (0 < MaximumNrOfInjectionWithoutPull)
    {
        // sanity check limit ratio for allocation purpose between 1 and 6
        MaximumUpsamplingRatio = max(1, MaximumUpsamplingRatio);
        MaximumUpsamplingRatio = min(6, MaximumUpsamplingRatio);

        NumberOfCodedBuffersToAllocate = NumberOfCodedBuffersToAllocate * (MaximumNrOfInjectionWithoutPull * MaximumUpsamplingRatio);
    }

    NumberOfCodedBuffersToAllocate = min(NumberOfCodedBuffersToAllocate, CODER_MAX_NB_CODED_BUFFERS);

    return (unsigned int)NumberOfCodedBuffersToAllocate;
}

/**
 * Prints out the MME_AudioEncoderGlobalParams_t
 *
 *
 * @param GlobalParams
 */
void Coder_Audio_Mme_c::DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(GlobalParams != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "StructSize = %d\n", GlobalParams->StructSize);
        /* PreProc config */
        SE_VERBOSE(group_encoder_audio_coder, "PreProcConfig\n");
        SE_VERBOSE(group_encoder_audio_coder, "  Id             = %08X\n", GlobalParams->PcmParams.Id);
        SE_VERBOSE(group_encoder_audio_coder, "  StructSize     = %d\n"  , GlobalParams->PcmParams.StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "PreProcConfig.Sfc\n");
        SE_VERBOSE(group_encoder_audio_coder, "  Id             = %08X\n", GlobalParams->PcmParams.Sfc.Id);
        SE_VERBOSE(group_encoder_audio_coder, "  StructSize     = %d\n"  , GlobalParams->PcmParams.Sfc.StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "  Apply          = %s\n"  , GlobalParams->PcmParams.Sfc.Apply == ACC_MME_DISABLED  ? " disabled" : "enabled");
        SE_VERBOSE(group_encoder_audio_coder, "PreProcConfig.Resample2x\n");
        SE_VERBOSE(group_encoder_audio_coder, "  Id             = %08X\n", GlobalParams->PcmParams.Resamplex2.Id);
        SE_VERBOSE(group_encoder_audio_coder, "  StructSize     = %d\n"  , GlobalParams->PcmParams.Resamplex2.StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "  Apply          = %s\n"  , GlobalParams->PcmParams.Resamplex2.Apply == ACC_MME_DISABLED  ? " disabled" : "enabled");
        /* input config */
        {
            uAudioEncoderChannelConfig channel_config;
            channel_config.u32 = GlobalParams->InConfig.ChannelConfig;
            SE_VERBOSE(group_encoder_audio_coder, "Input Config\n");
            SE_VERBOSE(group_encoder_audio_coder, "  Id             = %08X\n", GlobalParams->InConfig.Id);
            SE_VERBOSE(group_encoder_audio_coder, "  SamplingFreq   = %d\n"  , GlobalParams->InConfig.SamplingFreq);
            SE_VERBOSE(group_encoder_audio_coder, "  WordSize       = %08X\n", GlobalParams->InConfig.WordSize);
            SE_VERBOSE(group_encoder_audio_coder, "  StructSize     = %d\n"  , GlobalParams->InConfig.StructSize);
            SE_VERBOSE(group_encoder_audio_coder, "  Attenuation    = %d\n"  , channel_config.Bits.Attenuation);
            SE_VERBOSE(group_encoder_audio_coder, "  AudioMode      = %08X\n", channel_config.Bits.AudioMode);
            SE_VERBOSE(group_encoder_audio_coder, "  ChannelSwap    = %d\n"  , channel_config.Bits.ChannelSwap);
            SE_VERBOSE(group_encoder_audio_coder, "  DialogNorm     = %d\n"  , channel_config.Bits.DialogNorm);
            SE_VERBOSE(group_encoder_audio_coder, "  NbChannelsSent = %d\n"  , channel_config.Bits.NbChannelsSent);
        }
        /* Output config  */
        {
            SE_VERBOSE(group_encoder_audio_coder, "Output Config\n");
            SE_VERBOSE(group_encoder_audio_coder, "  Id            %08X\n", GlobalParams->OutConfig.Id);
            SE_VERBOSE(group_encoder_audio_coder, "  StructSize    %d\n"  , GlobalParams->OutConfig.StructSize);
            SE_VERBOSE(group_encoder_audio_coder, "  SamplingFreq  %d\n"  , GlobalParams->OutConfig.SamplingFreq);
            SE_VERBOSE(group_encoder_audio_coder, "  outputBitrate %d\n"  , GlobalParams->OutConfig.outputBitrate);
        }
    }
}

/**
 * Prints out all the  InitParams structure
 *
 *
 * @param InitParams
 */
void Coder_Audio_Mme_c::DumpMmeInitParams(MME_TransformerInitParams_t *InitParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(InitParams != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "StructSize                = %08X\n", InitParams->StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "Priority                  = %08X\n", InitParams->Priority);
        SE_VERBOSE(group_encoder_audio_coder, "Callback                  = %p\n"  , InitParams->Callback);
        SE_VERBOSE(group_encoder_audio_coder, "CallbackUserData          = %p\n"  , InitParams->CallbackUserData);
        SE_VERBOSE(group_encoder_audio_coder, "TransformerInitParamsSize = %08X\n", InitParams->TransformerInitParamsSize);
        SE_VERBOSE(group_encoder_audio_coder, "TransformerInitParams_p   = %p\n"  , InitParams->TransformerInitParams_p);

        if (NULL == InitParams->TransformerInitParams_p)
        {
            SE_ERROR("InitParams->TransformerInitParams_p is NULL\n");
            return;
        }

        MME_AudioEncoderInitParams_t *MmeAudioEncoderInitParams_p = (MME_AudioEncoderInitParams_t *)InitParams->TransformerInitParams_p;
        SE_VERBOSE(group_encoder_audio_coder, "EncoderInitParams_p->StructSize = %08X\n"                 , MmeAudioEncoderInitParams_p->StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "EncoderInitParams_p->BytesToSkipBeginScatterPage = %08X\n", MmeAudioEncoderInitParams_p->BytesToSkipBeginScatterPage);
        SE_VERBOSE(group_encoder_audio_coder, "EncoderInitParams_p->maxNumberSamplesPerChannel  = %08X\n", MmeAudioEncoderInitParams_p->maxNumberSamplesPerChannel);
        SE_VERBOSE(group_encoder_audio_coder, "EncoderInitParams_p->OptionFlags1                = %08X\n", MmeAudioEncoderInitParams_p->OptionFlags1);
        {
            tAudioEncoderOptionFlags1 *pFlags1 = (tAudioEncoderOptionFlags1 *)&MmeAudioEncoderInitParams_p->OptionFlags1;
            SE_VERBOSE(group_encoder_audio_coder, "pFlags1->FrameBased  = %08X\n", pFlags1->FrameBased);
            SE_VERBOSE(group_encoder_audio_coder, "pFlags1->DeliverASAP = %08X\n", pFlags1->DeliverASAP);
            SE_VERBOSE(group_encoder_audio_coder, "pFlags1->Swap1       = %08X\n", pFlags1->Swap1);
            SE_VERBOSE(group_encoder_audio_coder, "pFlags1->Swap2       = %08X\n", pFlags1->Swap2);
            SE_VERBOSE(group_encoder_audio_coder, "pFlags1->Swap3       = %08X\n", pFlags1->Swap3);
        }
        DumpMmeGlobalParams(&MmeAudioEncoderInitParams_p->GlobalParams);
    }
}

/**
 *
 * @param Scat pointer to the scatter page to print
 * @param Input true if called from Input() thread, false if called from Output/Callback thread()
 */
void Coder_Audio_Mme_c::DumpMmeScatterPage(MME_ScatterPage_t *Scat, const char *MessageTag)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(Scat != NULL);
        SE_ASSERT(MessageTag != NULL);
        //SE_VERBOSE(group_encoder_audio_coder, "%s Page_p      %p\n", Input ?"In":"Out",Scat->Page_p    );
        SE_VERBOSE(group_encoder_audio_coder, "%s Size      %08X\n", MessageTag, Scat->Size);
        SE_VERBOSE(group_encoder_audio_coder, "%s BytesUsed %08X\n", MessageTag, Scat->BytesUsed);
        SE_VERBOSE(group_encoder_audio_coder, "%s FlagsIn   %08X\n", MessageTag, Scat->FlagsIn);
        SE_VERBOSE(group_encoder_audio_coder, "%s FlagsOut  %08X\n", MessageTag, Scat->FlagsOut);
    }
}

/**
 *
 * @param MmeBuf_p pointer to the structure to to print
 * @param Input true if called from Input() thread, false if called from Output/Callback thread()
 */
void Coder_Audio_Mme_c::DumpMmeBuffer(MME_DataBuffer_t    *MmeBuf_p, const char *MessageTag)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        unsigned int i;
        SE_ASSERT(MmeBuf_p != NULL);
        SE_ASSERT(MessageTag != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "%s StructSize           = %08X\n", MessageTag, MmeBuf_p->StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s UserData_p           = %p\n",   MessageTag, MmeBuf_p->UserData_p);
        SE_VERBOSE(group_encoder_audio_coder, "%s Flags                = %08X\n", MessageTag, MmeBuf_p->Flags);
        SE_VERBOSE(group_encoder_audio_coder, "%s StreamNumber         = %08X\n", MessageTag, MmeBuf_p->StreamNumber);
        SE_VERBOSE(group_encoder_audio_coder, "%s NumberOfScatterPages = %08X\n", MessageTag, MmeBuf_p->NumberOfScatterPages);
        SE_VERBOSE(group_encoder_audio_coder, "%s ScatterPages_p       = %p\n",   MessageTag, MmeBuf_p->ScatterPages_p);
        SE_VERBOSE(group_encoder_audio_coder, "%s TotalSize            = %08X\n", MessageTag, MmeBuf_p->TotalSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s StartOffset          = %08X\n", MessageTag, MmeBuf_p->StartOffset);

        for (i = 0; i < MmeBuf_p->NumberOfScatterPages; i++)
        {
            DumpMmeScatterPage(MmeBuf_p->ScatterPages_p, MessageTag);
        }
    }
}

/**
 *
 * @param Status pointer to the structure to print
 * @param Input true if called from Input() thread, false if called from Output/Callback thread()
 */
void Coder_Audio_Mme_c::DumpMmeStatusGenericParams(CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *Status, const char *MessageTag)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(Status != NULL);
        SE_ASSERT(MessageTag != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.StructSize                      = %03i\n", MessageTag, Status->CommonParams.StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.SpecificEncoderStatusBArraySize = %03i\n", MessageTag, Status->CommonParams.SpecificEncoderStatusBArraySize);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.Status.numberInputBytesLeft     = %08X\n", MessageTag, Status->CommonParams.Status.numberInputBytesLeft);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.Status.numberOutputBytes        = %08X\n", MessageTag, Status->CommonParams.Status.numberOutputBytes);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.Status.byteStartAddress         = %08X\n", MessageTag, Status->CommonParams.Status.byteStartAddress);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.Status.EncoderStatus            = %08X\n", MessageTag, Status->CommonParams.Status.EncoderStatus);
        SE_VERBOSE(group_encoder_audio_coder, "%s Status.Common.Status.BitRate                  = %08X\n", MessageTag, Status->CommonParams.Status.BitRate);
    }
}

/**
 *
 * @param CoderTransformParams pointer to the structure to print
 * @param Input true if called from Input() thread, false if called from Output/Callback thread()
 */
void Coder_Audio_Mme_c::DumpMmeCoderTransformParams(MME_AudioEncoderTransformParams_t *CoderTransformParams, const char *MessageTag)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(CoderTransformParams != NULL);
        SE_ASSERT(MessageTag != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.numberOutputSamples = %08X\n" , MessageTag, CoderTransformParams->numberOutputSamples);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.EndOfFile           = %08X\n" , MessageTag, CoderTransformParams->EndOfFile);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.UpdateFormat        = %08X\n" , MessageTag, CoderTransformParams->UpdateFormat);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.AudioMode           = %08X\n" , MessageTag, CoderTransformParams->AudioMode);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.PtsInfo.PTS         = %llX\n" , MessageTag, CoderTransformParams->PtsInfo.PTS);
        SE_VERBOSE(group_encoder_audio_coder, "%s TransformParam.PtsInfo.PtsFlags    = %08X\n" , MessageTag, CoderTransformParams->PtsInfo.PtsFlags.U32);
    }
}

/**
 *
 * @param MmeCmd pointer to the structure to print
 * @param Input true if called from Input() thread, false if called from Output/Callback thread()
 */
void Coder_Audio_Mme_c::DumpMmeCmd(MME_Command_t       *TransformCmd, const char *MessageTag)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        unsigned int i;
        SE_ASSERT(TransformCmd != NULL);
        SE_ASSERT(MessageTag != NULL);
        SE_VERBOSE(group_encoder_audio_coder, "%s StructSize                   = %08X\n", MessageTag, TransformCmd->StructSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdCode                      = %08X\n", MessageTag, TransformCmd->CmdCode);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdEnd                       = %08X\n", MessageTag, TransformCmd->CmdEnd);
        SE_VERBOSE(group_encoder_audio_coder, "%s DueTime                      = %08X\n", MessageTag, TransformCmd->DueTime);
        SE_VERBOSE(group_encoder_audio_coder, "%s NumberInputBuffers           = %08X\n", MessageTag, TransformCmd->NumberInputBuffers);
        SE_VERBOSE(group_encoder_audio_coder, "%s NumberOutputBuffers          = %08X\n", MessageTag, TransformCmd->NumberOutputBuffers);
        SE_VERBOSE(group_encoder_audio_coder, "%s DataBuffers_p                = %p\n",   MessageTag, TransformCmd->DataBuffers_p);
        SE_VERBOSE(group_encoder_audio_coder, "%s ParamSize                    = %08X\n", MessageTag, TransformCmd->ParamSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s Param_p                      = %p\n",   MessageTag, TransformCmd->Param_p);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.CmdId              = %08X\n", MessageTag, TransformCmd->CmdStatus.CmdId);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.State              = %08X\n", MessageTag, TransformCmd->CmdStatus.State);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.ProcessedTime      = %08X\n", MessageTag, TransformCmd->CmdStatus.ProcessedTime);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.Error              = %08X\n", MessageTag, TransformCmd->CmdStatus.Error);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.AdditionalInfoSize = %08X\n", MessageTag, TransformCmd->CmdStatus.AdditionalInfoSize);
        SE_VERBOSE(group_encoder_audio_coder, "%s CmdStatus.AdditionalInfo_p   = %p\n",   MessageTag, TransformCmd->CmdStatus.AdditionalInfo_p);

        switch (TransformCmd->CmdCode)
        {
        case MME_TRANSFORM:
        {
            SE_DEBUG(group_encoder_audio_coder, "CmdCode MME_TRANSFORM\n");
            if (NULL == TransformCmd->DataBuffers_p)
            {
                SE_ERROR("TransformCmd->DataBuffers_p is NULL\n");
                return;
            }

            for (i = 0; i < TransformCmd->NumberInputBuffers + TransformCmd->NumberOutputBuffers; i++)
            {
                SE_VERBOSE(group_encoder_audio_coder, "%s TransformCmd->DataBuffers_p[%u]\n", MessageTag, i);
                DumpMmeBuffer(TransformCmd->DataBuffers_p[i], MessageTag);
            }

            DumpMmeStatusGenericParams((CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *)TransformCmd->CmdStatus.AdditionalInfo_p, MessageTag);
            DumpMmeCoderTransformParams((MME_AudioEncoderTransformParams_t *)TransformCmd->Param_p, MessageTag);
        }
        break;

        case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        {
            SE_DEBUG(group_encoder_audio_coder, "CmdCode MME_SET_GLOBAL_TRANSFORM_PARAMS\n");
            if (0 != (TransformCmd->NumberInputBuffers + TransformCmd->NumberOutputBuffers))
            {
                SE_ERROR("There are %d buffers with this GLOBAL PARAMS command should be 0\n",
                         TransformCmd->NumberInputBuffers + TransformCmd->NumberOutputBuffers);
            }

            for (i = 0; i < (TransformCmd->NumberInputBuffers + TransformCmd->NumberOutputBuffers); i++)
            {
                SE_VERBOSE(group_encoder_audio_coder, "%s TransformCmd->DataBuffers_p[%u]\n", MessageTag, i);
                DumpMmeBuffer(TransformCmd->DataBuffers_p[i], MessageTag);
            }

            DumpMmeGlobalParams((MME_AudioEncoderGlobalParams_t *)TransformCmd->Param_p);
        }
        break;

        default:
            SE_ERROR("Unsupported TransformCmd->CmdCode %08x\n", TransformCmd->CmdCode);
        }
    }
}

inline uint32_t Coder_Audio_Mme_c::NrIssuedCommands(void)
{
    return (mMmeCounters.SendParamsPrepared + mMmeCounters.TransformPrepared);
}

inline uint32_t Coder_Audio_Mme_c::NrDoneCommands(void)
{
    return (mMmeCounters.SendParamsAborted
            + mMmeCounters.SendParamsCompleted
            + mMmeCounters.TransformAborted
            + mMmeCounters.TransformCompleted);
}

CoderStatus_t Coder_Audio_Mme_c::AcquireAllMmeCommandContexts(Buffer_t ContextBufferArray[], bool *Success,
                                                              bool AbortOnNonRunning)
{
    /* Difference between (true==(!*Success)) and false=(CoderNoError==AcquireAllMmeCommandContexts()):
       !*Success can happen on the normal path
       the other is an Error status: it should not happen */
    int32_t          ContextBufferDepth = (int32_t)mMmeStaticConfiguration.MmeTransformQueueDepth;
    int32_t          i;
    uint64_t         EntryTime;
    CoderStatus_t    Status = CoderNoError;
    BufferStatus_t   BufStatus = BufferNoError;

    *Success         = true;

    if (mCoderContextBufferPool == NULL)
    {
        *Success         = false;
        return CoderError;
    }

    for (i = 0;  i < ContextBufferDepth ; i++)
    {
        ContextBufferArray[i] = NULL;
    }

    /* Prevent concurrency on AcquireAllMmeCommandContexts */
    OS_LockMutex(&mAcquireAllMmeCommandContextsLock);

    // Acquire all context buffers.
    // Buffer manager will block if any context buffer is locked by FW, other process
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint32_t GetBufferTimeOutMs   = 100;
    for (i = 0;  i < ContextBufferDepth ; i++)
    {
        EntryTime = OS_GetTimeInMicroSeconds();
        do
        {
            BufStatus = mCoderContextBufferPool->GetBuffer(&ContextBufferArray[i], IdentifierEncoderCoder,
                                                           UNSPECIFIED_SIZE, false, false,
                                                           GetBufferTimeOutMs);
            // Warning message every TimeBetweenMessageUs us
            if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
            {
                SE_WARNING("%p->GetBuffer still returns BufferNoFreeBufferAvailable,  %s and %s\n",
                           mCoderContextBufferPool,
                           TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning",
                           AbortOnNonRunning ? "AbortOnNonRunning" : "!AbortOnNonRunning");
                EntryTime = OS_GetTimeInMicroSeconds();
            }
        }
        while ((BufferNoFreeBufferAvailable == BufStatus)
               && (TestComponentState(ComponentRunning) || (!AbortOnNonRunning)));

        if ((BufStatus != BufferNoError) || (NULL == ContextBufferArray[i]))
        {
            *Success = false;
            ContextBufferArray[i] = NULL;
            if (BufferBlockingCallAborted == BufStatus)
            {
                SE_INFO(group_encoder_audio_coder, "GetBufferAborted\n");  // TODO(pht) remove
            }
            else
            {
                SE_ERROR("GetBufferFailure %08X\n", BufStatus);
                Status = CoderError;
            }

            // We did not succeed to get all Buffers
            break;
        }
    }

    //In case of non-sucess to get all mme buffers, release all mme buffers obtained here
    if (!*Success)
    {
        CoderStatus_t ReleaseStatus = ReleaseAllMmeCommandContexts(ContextBufferArray);
        if (CoderNoError != ReleaseStatus)
        {
            Status = ReleaseStatus;
        }
    }

    // We can safely unlock the mutex
    OS_UnLockMutex(&mAcquireAllMmeCommandContextsLock);

    return Status;
}

CoderStatus_t Coder_Audio_Mme_c::ReleaseAllMmeCommandContexts(Buffer_t ContextBufferArray[])
{
    int32_t          i;
    CoderStatus_t    Status = CoderNoError;
    uint32_t         ContextBufferDepth = mMmeStaticConfiguration.MmeTransformQueueDepth;

    if (mCoderContextBufferPool == NULL)
    {
        return CoderNoError;
    }

    for (i = 0; i < (int32_t)ContextBufferDepth ; i++)
    {
        if (NULL == ContextBufferArray[i])
        {
            /* This is not an error path as ReleaseAllMmeCommandContexts is called from AcquireAllMmeCommandContexts */
            SE_DEBUG(group_encoder_audio_coder, "ContextBufferArray[%d] is NULL\n", i);
            continue;
        }

        ContextBufferArray[i]->DecrementReferenceCount(IdentifierEncoderCoder);

        ContextBufferArray[i] = NULL;
    }

    return Status;
}

//
// Low power enter method
// For CPS mode, all MME transformers must be terminated
//
CoderStatus_t Coder_Audio_Mme_c::LowPowerEnter(void)
{
    CoderStatus_t CoderStatus = CoderNoError;
    // Save low power state
    mIsLowPowerState = true;
    // Terminate MME transformer if needed
    mIsLowPowerMMEInitialized = (mTransformerHandle != 0);

    if (mIsLowPowerMMEInitialized)
    {
        SE_INFO(group_encoder_audio_coder, "terminate mmeT\n");

        Buffer_t AllMmeContexts[CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH];
        bool GotAllMmeContexts;

        /**
         * @todo : LowPower State(s) to be added to base class and test
         *        against them in Input threads, so that Input threads
         *        can be unlocked if LP is required.
         */

        /* Gives up if component is not Running */
        CoderStatus = AcquireAllMmeCommandContexts(AllMmeContexts, &GotAllMmeContexts);
        if (CoderNoError != CoderStatus)
        {
            SE_ERROR("Error in AcquireAllMmeCommandContexts\n");
            return CoderError;
        }

        if (!GotAllMmeContexts)
        {
            SE_INFO(group_encoder_audio_coder, "Coder Halted while trying to enter LowPower\n");
            return CoderNoError; /* @todo: CoderNoError or CoderError ? */
        }

        CoderStatus = TerminateMmeTransformer();
        if (GotAllMmeContexts)
        {
            CoderStatus = ReleaseAllMmeCommandContexts(AllMmeContexts);
        }
    }

    return CoderStatus;
}

//
// Low power exit method
// For CPS mode, all MME transformers must be re-initialized
//
CoderStatus_t Coder_Audio_Mme_c::LowPowerExit(void)
{
    CoderStatus_t CoderStatus = CoderNoError;

    // Re-initialize MME transformer if needed
    if (mIsLowPowerMMEInitialized)
    {
        SE_INFO(group_encoder_audio_coder, "reinit MmeT\n");
        CoderStatus = InitializeMmeTransformer();
    }

    // Reset low power state
    mIsLowPowerState = false;
    return CoderStatus;
}

TimeStamp_c Coder_Audio_Mme_c::DelayExtractionGetDelay(bool &IsDelay, TimeStamp_c OutputPts)
{
    if (!(mDelayExtraction.TransformOffset.IsValid()))
    {
        if (!(mDelayExtraction.FirstReceivedPts.IsValid()))
        {
            mDelayExtraction.FirstReceivedPts = OutputPts.Pts();
        }

        if ((mDelayExtraction.FirstInjectedPts.IsValid())
            && (mDelayExtraction.FirstReceivedPts.IsValid()))
        {
            mDelayExtraction.TransformOffset = StmSeAudioGetOffsetFrom33BitPtsTimeStamps(mDelayExtraction.IsDelay,
                                                                                         mDelayExtraction.FirstInjectedPts,
                                                                                         mDelayExtraction.FirstReceivedPts);
        }
    }

    IsDelay = mDelayExtraction.IsDelay;
    return mDelayExtraction.TransformOffset;
}
