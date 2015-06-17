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
#ifndef PREPROC_AUDIO_MME_H
#define PREPROC_AUDIO_MME_H

#include "preproc_audio.h"
#include "timestamps.h"

#include "acc_mme.h"
#include <ACC_Transformers/acc_mmedefines.h>
#include <ACC_Transformers/Pcm_PostProcessingTypes.h>
#include <ACC_Transformers/PcmProcessing_DecoderTypes.h>

/**
 * The number of commands we can issue in the pipeline; We limit
 * two for a ping-pong would be enough, but as in current
 * implementation Input() is blocking till all transforms are
 * complete we can set it to 1.
 */
#define PREPROC_AUDIO_MME_TRANSFORM_QUEUE_DEPTH 1

// The Transform Priority
#define PREPROC_AUDIO_MME_DEFAULT_MME_PRIORITY MME_PRIORITY_NORMAL

/**
 * This is to work around the fact that Input Buffer is not
 * always visible from companion. When defined as not 0, we
 * get an extra buffer from the ouput pool, copy the input data
 * to it and use it for the transform.
 */
#define PREPROC_AUDIO_MME_BUG_25952_INPUT_BUFFER_MAPPING_WORKAROUND 1

/**
 * Maximum number of channel in a buffer to be handled by the
 * preproc (to bound memory allocation)
 * */
#define PREPROC_AUDIO_MME_MAX_NUMBER_OF_CHANNEL_IN 8  // If you make this more than 8, take care of the corresponding code

/**
 * The width of the output buffer 6 is enough to contain 5.1 encoding.
 *
 * BUT in case the WA for bz 25952 is active, as we re-use an
 * output buffer to temporarily store the input data, and in
 * that case the number of channels at the output must be at
 * least the same as that at the input.
 *
 * To optimize time and avoid memory reallocations inside the
 * coders transformers, it is strongly recommended to set each
 * of the CODER_AUDIO_MME_MAX_NB_CHAN and of
 * each CODER_AUDIO_MME_xxxxx_DEFAULT_ENCODING_CHANNEL_ALLOC
 * to the same value as  PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT
 * (it will work without it, but it would slow down time from
 * creation to first byte out)
 **/
#if PREPROC_AUDIO_MME_BUG_25952_INPUT_BUFFER_MAPPING_WORKAROUND
#define PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT PREPROC_AUDIO_MME_MAX_NUMBER_OF_CHANNEL_IN
#else
#define PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT 6
#endif

/**
 * In case of sampling frequency conversion, a number of extra margin
 * samples we reserve when computing the output size requirement
 * Dependent on companion  implementation
 */
#define PREPROC_AUDIO_MME_SFC_OUTPUT_BUFFER_SAFETY_SMP 16

/**
 * @brief this Multicom structure is used to configure only a select list of processes in the MTTransformer
 *
 * Without this structure we would transfer parameters for all processes, including those we do not use
 */
typedef struct
{
    unsigned short                 StructSize;  //!< Size of this structure
    unsigned char                  DigSplit;    //!< for path management
    unsigned char                  AuxSplit;    //!< for path management
    MME_CMCGlobalParams_t          CMC;         //!< for downmix
    MME_DMixGlobalParams_t         DMix;        //!< for downmix
    MME_Resamplex2GlobalParams_t   Resamplex2;  //!< for resampling
    MME_ChannelReMapGlobalParams_t ChanReMap;  //!< for channel remapping
} PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t;

/**
 * @brief stores the difference between the size of the all the parameters and the size of the parameters we use
 *
 * this is used to offet the size of structures that would normally contain MME_PcmProcessorGlobalParams_t but instead actually
 * contain a PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t
 */
static const uint32_t PreprocAudioGlobalParamSubsetSizeAdjust = sizeof(MME_LxPcmProcessingGlobalParams_t) - sizeof(PreprocAudioMmeMulticomPcmProcessingGlobalParamsSubset_t);

//! The multicom side of the transform parameters
typedef struct PreprocAudioMmeTransformMulticomParams_s
{
    //Transform structures
    MME_DataBuffer_t                 *DataBuffers[2];       //!< Stores addresses of MMEInputBuffer and MMEOutputBuffer
    MME_ScatterPage_t                 MMEInputScatterPage;
    MME_DataBuffer_t                  MMEInputBuffer;
    MME_ScatterPage_t                 MMEOutputScatterPage;
    MME_DataBuffer_t                  MMEOutputBuffer;

    MME_PcmProcessingFrameStatus_t    PcmProcessingStatusParams;
    MME_PcmProcessingFrameParams_t    PcmProcessingFrameParams;
} PreprocAudioMmeTransformMulticomParams_t;

typedef struct PreprocAudioMmeTransformContext_s
{
    bool                                        UpdatedControls;   //!< Whether controls have been updated
    PreprocAudioControls_t                      Controls;          //!< Copy of controls for this transform
    bool                                        ApplyEOS;          //!< True when EOS discontinuity

    PreprocAudioMmeTransformMulticomParams_t    MulticomParams; //!< The multicom  transform parameters
} PreprocAudioMmeTransformContext_t;

/**
 * @brief The full context required to execute a transform and process its result
 */
typedef struct PreprocAudioMmeCommandContext_s
{
    /*! @note TransformCmd has to be first in structure so that TransformContext can be retrieved from cast of MME_Command_t *CallbackData */
    MME_Command_t               TransformCmd;
    /* To store current PreprocContextBuffer  */
    Buffer_t                    PreprocContextBuffer;
    /* A transform command can be a "full_transform" xor a "send_global" */
    union
    {
        PreprocAudioMmeTransformContext_t TransformParams;  //Transform Params
        MME_PcmProcessorGlobalParams_t    GlobalParams;     //Global Params
    };
} PreprocAudioMmeCommandContext_t;

/**
 * @brief a structure to store all the parameters related to a transformer init
 */
typedef struct PreprocAudioMmeMulticomTransformerInit_s
{
    MME_TransformerHandle_t                hTransformer;  //!< the transformer handle
    MME_TransformerInitParams_t            InitParams;    //!< generic transform init params
    MME_LxPcmProcessingInitParams_t        PcmProcessingInitParams; //!< preproc transform init params
} PreprocAudioMmeMulticomTransformerInit_t;

class Preproc_Audio_Mme_c : public Preproc_Audio_c
{
public:
    Preproc_Audio_Mme_c(void);
    ~Preproc_Audio_Mme_c(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t Discontinuity);
    // This function prepares parses the buffer and send commands to Multicom
    // then multicom results will be callected from a callback function
    PreprocStatus_t   Input(Buffer_t Buffer);

    // Low power methods
    PreprocStatus_t   LowPowerEnter(void);
    PreprocStatus_t   LowPowerExit(void);

    //MME Callback
    void CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData);

    virtual void              GetChannelConfiguration(int64_t *AcMode)
    {
        if (AcMode != NULL) { *AcMode = (int64_t)mAcmodOutput; }
    }

protected:
    bool AreCurrentControlsAndMetadataSupported(void); // Check CurrentInputBuffer and CurrentParameters

private:
    //! Stores the data required to initialize a transformer
    PreprocAudioMmeMulticomTransformerInit_t TransformerInitContext;

    unsigned int                             MaxPreprocBufferSize;
    bool                                     UpdatedControls; //!< true if new controls have been detected for this input

    TimeStampExtrapolator_c                  InputExtrapolation; //! Manages input timestamp
    const char                              *AudioPreProcTransformerNames[ENCODER_STREAM_AUDIO_MAX_CPU]; //!< AudioPreProc transformer names for each running CPU

    //! Low power data
    bool                                     IsLowPowerState;          //!< HPS/CPS: indicates when SE is in low power state
    bool                                     IsLowPowerMMEInitialized; //!< CPS: indicates whether MME transformer was initialized when entering low power state
    PreprocAudioMmeCommandContext_t         *InputCurrentMmeCommandContext;  //!< Pointer owned by Input thread, filled by GetNextFreeCommandContext;

    bool                                     ApplyEOS; //!< When EOS discontinuity, it is set by HandleEndOfLoop (or InitializeLoopManagement for zero input) to trigger the FW tagging of EOS.
    bool                                     PreprocDiscontinuityForwarded; //!< It is set by FinalizeOutputAudioBuffer when no output for EOS input.

    enum eAccAcMode                          mAcmodOutput;

    /**
     * A structure to handle more than PREPROC_AUDIO_OUT_MAX_NB_SAMPLES samples at
     * output: we iterate N transforms: we store some overall pre-parsed information
     * in this structure, plus information about the current/past iteration.
     */
    struct
    {
        uint32_t TotalNrSamples;      //!< Total Number of samples in the current Input()
        uint32_t Channels;            //!< Parsed number of channels in current Input
        uint32_t NrBytesPerSample;    //!< Number of bytes per PCM sample
        int32_t  RemainingIterations; //!< Number of remaining iteration(s) including this
        int32_t  SamplesSentPrior;    //!< Samples already sent in prior loops iterations
        int64_t DeltaEncodeNativeTime; //!<offset between encoded and native time
        struct
        {
            uint32_t NSamples;        //!< Number of samples to send in this loop
            uint32_t OffsetB;         //!< Offset in input buffer in this loop
            uint32_t SizeB;           //!< Number of bytes sent in this loop
        } ThisIteration;
    } LoopManagement;

    /**
     * @brief Initilializes the Pcm Preprocessor Transformer based on current parameters
     *
     * Today there is no requirement to re-start transformer in preproc (vs. coder)
     * so Init/Terminate assume a start from scratch/terminate to remove
     *
     * @return PreprocStatus_t PreprocError if could not succeed
     */
    PreprocStatus_t InitMmeTransformer(void);      // Initializes companion RPC

    /**
     * @brief Checks the companion capabilities vs. supported PreProc
     *
     *
     * @param SelectedCpu is the Audio CPU on which capability found.
     *
     * @return Return true if we find the preproc on the companion, false otherwise.
     *
     */
    bool   IsPreProcSupportedByCompanion(uint32_t &SelectedCpu);

    /**
     * @brief Get the companion capabilities on the selected cpu
     *
     *
     * @param SelectedCpu is the Audio CPU on which capability found.
     *
     * @return Return true if we find the preproc on the companion, false otherwise.
     *
     */
    bool  CheckCompanionCapability(uint32_t SelectedCpu);

    /**
     * Terminates the transformer:
     *
     * Caller is to take precautions that there is no pending
     * non-returned MME command  prior to calling this.
     */
    PreprocStatus_t TerminateMmeTransformer(void);  // Terminates  companion RPC

    /**
     * (blocking) Acquires all Mmme Command Contexts; doing so
     * ensures that all previously issues mme commands have
     * completed.
     * When Success is true all Contexts are owned by caller, stored
     * in ContextBufferArray, and must be freed later by
     * calling @ref Preproc_Audio_Mme_c::ReleaseAllMmeCommandContexts
     *
     * When Success is false, no Context is owned by caller on return.
     *
     * When Success is false, it is the responsibility of the caller
     * to decide whether to retry or not.
     *
     * @param ContextBufferArray
     *                An array of Buffer_t to store all obtained
     *                resources
     * @param Success returns set to true if all context were
     *                acquired, to false if no context was acquired
     * @param AbortOnNonRunning
     *                If true (default), the function can exit
     *                without Success if the component is not in
     *                Running state
     *
     * @return PreprocStatus_t PreprocNoError in case of no failure;
     *         PreprocNoError != Success
     */
    PreprocStatus_t AcquireAllMmeCommandContexts(Buffer_t ContextBufferArray[],
                                                 bool *Success,
                                                 bool AbortOnNonRunning = true);
    OS_Mutex_t    AcquireAllMmeCommandContextsLock; /*!< Protects critical resources against concurrent calls*/

    /**
     * Releases all Contexts obtained through @ref
     * Preproc_Audio_Mme_c::AcquireAllMmeCommandContexts
     *
     * @param ContextBufferArray
     *               An array of Buffer_t to contained all obtained resources
     *
     * @return PreprocStatus_t PreprocNoError if no error
     */
    PreprocStatus_t ReleaseAllMmeCommandContexts(Buffer_t ContextBufferArray[]);


    /**
     * Determines if a re-init of the MTPcmProcessing is required to
     * handle change of parameters
     *
     *
     * @return bool true if a Terminate/Init of the transformer is
     *         required
     */
    bool ChangeDetectedRequiresTransformReinit(void);

    /**
     * @brief Blocking: Detects changes in context, issues new parameters to companion if necessary
     *
     * This function clears all buffer references if it fails. No need for caller to do so if returns error code
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t HandleChangeDetection(void);   // Detects changes in context, issue parameters to companion if necessary

    /**
     * @brief Blocking: gets the next free Multicom Command Context, saves ptr in
     *        InputCurrentMmeCommandContext
     *
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t GetNextFreeCommandContext(void);

    /**
     * Releases the transform context Buffer_t to the pool<p>
     * (caller must release any data in the context prior)
     *
     * @param TransforContext_p
     *
     * @param OptStr An optional string to have messages in case of
     *               errors/warnings.
     *
     * @return CoderStatus_t
     */
    void   ReleaseTransformContext(PreprocAudioMmeCommandContext_t *Ctx, const char *OptStr = NULL);

    /**
     * @brief Fills InputCurrentMmeCommandContext for a SendTransform
     *
     */
    void FillTransformCommandContext(void);

    /**
     * @brief Fills InputCurrentMmeCommandContext for a
     *        SendGlobalParams
     *
     */
    void FillSendGlobalCommandContext(void);

    /**
     * @brief A generic function that can be used for sending Tranform or GlobalParams from
     *        InputCurrentMmeCommandContext->TransformCmd
     *
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t SendCurrentCommand(void);

    // Functions to manage Loop for more than PREPROC_AUDIO_OUT_MAX_NB_SAMPLES at output
    /**
     * decides correct number of iterations and
     * initiliazes the loop context to handle buffers
     * larger than PREPROC_AUDIO_OUT_MAX_NB_SAMPLES at
     * the output
     *
     */
    void InitializeLoopManagement(void);

    /**
     * Prepare the next loop iteration if necessary.
     * (Blocking) Because it may need extra
     * buffers/commands, it is a blocking call.
     *
     * @param Buffer The Input() Buffer: needs to be passed
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t HandleEndOfLoop(Buffer_t Buffer);

    /**
     * @brief Fill all parameters required to initiliase transformer based on current context
     *
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t FillInitParamsFromCurrentParameters(void);

    /**
     * @brief Fill a Pcm Preprocessing Transformer GlobalParams according to current context
     * Can be used for InitTransformer or SendGlobal.
     *
     * @param GlobalParams
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t FillGlobalParamsFromCurrentParameters(MME_PcmProcessorGlobalParams_t *);


    /**
     * @brief Fill TransformContext's OutputAudioBuffer's structure that were not filled by Preproc_Base_c::Input
     *
     * Preproc_Base_c::Input copied the metadata from InputBuffer, parameters that are not the same are filled by this method
     *
     * @param TrnsfrmCntxt
     * @param OutputAudioBuffer
     *
     * @return PreprocStatus_t
     */
    PreprocStatus_t            FinalizeOutputAudioBuffer(PreprocAudioMmeCommandContext_t *, PreprocAudioBuffer_t    *OutputAudioBuffer);

    /**
     *  Dual mono requires overriding the input map in some cases:<p>
     *  1/ For forced-dual mono we need to cancel any non L/R
     *  channel<p>
     *  2/ Mono out and dual mono is not handled well by companion
     *  esp if only L or only R, but we may need it for encoding: in
     *  case of mono out + dual mono forced/stream +single L or R
     *  req, we simply mark the requested channel as the centre
     *  channel and cancel the others. Even this worakaround requires
     *  companion update: needs to support channel positions
     *
     * @param InChannelPlacementCopy
     *                   the channel map we may override
     * @param AudioModeIsPhysical
     *                   whether the channel routing is native to audio companion
     * @param AcmodInput Acmod parsed from actual map input buffer
     * @param AcmodOutput
     *                   Acmod parsed from control
     *
     * @return PreprocNoError if no error
     */
    PreprocStatus_t ModifyInputRouteForDualMono(stm_se_audio_channel_placement_t &InChannelPlacementCopy,
                                                bool &AudioModeIsPhysical,
                                                enum eAccAcMode &AcmodInput,
                                                enum eAccAcMode AcmodOutput);

    /**
     * Handles bz 25952 getting resource and transform command
     * modification; must be called after
     * FillTransformCommandContext, before
     * SendCurrentCommand
     *
     * The transformer requires that the input buffer send to the
     * transformer be mapped to the companion memory; so far we have
     * not found a process that maps on-the-fly without problem;
     * thus we use an output buffer (already mapped pool) and copy
     * the input buffer to this buffer (this requires that the
     * output buffers be at least as large as the input buffers)
     *
     * This funtions gets extra resource required to handle the bug,
     * and sets the transformer to use the resource instead of the
     * initial resource.
     *
     * At the funtion return on success, the resource is attached to
     * the transform: there is not new handling expected of the
     * caller: in case of future failure the transform context is to
     * be released as usual, the extra resource will be released
     * with it.
     *
     * @return PreprocStatus_t PreprocNoError if OK
     */
    PreprocStatus_t Bz25952MmeCommandOverride(void);

    unsigned int      GetMaxPreprocBufferSize(void);
    unsigned int      GetMaxNrPreprocBuffers(void);

    //Debugging
    void            DumpGlobalParams(MME_PcmProcessorGlobalParams_t *GlobalParams);
    void            DumpInitParams(MME_TransformerInitParams_t *InitParams);
    void            DumpPcmProcessingStatus(MME_PcmProcessingFrameStatus_t *PcmProcessingStatusParams);
    void            DumpDecoderFrameParams(MME_LxAudioDecoderFrameParams_t *FrameParams);

    DISALLOW_COPY_AND_ASSIGN(Preproc_Audio_Mme_c);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Audio_Mme_c
\brief Responsible for preparing samples before passing to the audio encoder.

It may be more practical to set metadata for processing, and defer to the encoder.<br>
For example, Sample Rate Conversion might be better handled by the encoder capabilities.

Future optimisations here might perform analysis for the encoder, so that multiple encodings
have a reduced workload and can for example share frequency analysis at a common stage.

*/

#endif /* PREPROC_AUDIO_MME_H */
