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
#ifndef CODER_AUDIO_MME_H
#define CODER_AUDIO_MME_H

#include "coder_audio.h"
#include "timestamps.h"
#include "preproc_audio_mme.h" // for PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT

#include "acc_mme.h"
#include <ACC_Transformers/acc_mmedefines.h>
#include <ACC_Transformers/Audio_EncoderTypes.h>

//! Max number of channels supported by companion
#define CODER_AUDIO_MME_MAX_NB_CHAN PREPROC_AUDIO_MME_NUMBER_OF_CHANNEL_OUT
//! Max number of samples per input buffer
#define CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER (STM_SE_AUDIO_ENCODE_MAX_SAMPLES_PER_FRAME)

/**
 * In order to help the threading of client we allocate extra
 * buffers so that a maximum of N Injections with the maximum of
 * of samples at the maximum re-sampling ratio can be consummed
 * by the encoder (preproc+coder) without blocking, even if
 * there not coded buffers being consummed.
 *
 * For memory allocation saving purposes the absorbtion of this
 *  data must be done in coded buffers rather than PCM buffers,
 *  hence the definition in this class.
 */

//! Maximum of calls to Input() will complete without blocking even if no coded buffer is pulled (0...N)
#define CODER_AUDIO_MME_MAX_NR_INJECTION_TO_ABSORB_WITHOUT_BLOCKING     2
//! Maximum up-sampling-ratio to consider in the memory allocation computation for this purpose (1...N)
#define CODER_AUDIO_MME_MAX_UPSAMPLING_RATIO_TO_ABSORB_WITHOUT_BLOCKING 6 //8->48kHZ

/** Default number of commands we can queue in multicom, 2
 *  should be enough for most codecs */
#define CODER_AUDIO_MME_DEF_TRANSFORM_CONTEXT_POOL_DEPTH   2

/** (Arbitrary) Maximum number of transform context for
 *  memory allocation purpose: increase if some codec
 *  requires it */
#define CODER_AUDIO_MME_MAX_TRANSFORM_CONTEXT_POOL_DEPTH   2

// AUDIO_ENCODER_HELPER_TYPES
/* @note Types in this block will in the end come from firmware Audio_EncoderTypes.h */
//! To set the output (compressed) configuration
typedef struct
{
    uint32_t                    Id;               //!< ==> use @ref ACC_GENERATE_ENCODERID(*eAccEncoderId*) macro
    uint32_t                    StructSize;       //!< Size of this structure
    uint16_t                    outputBitrate;    //!< Encoding bitrate in kbps
    enum eAccFsCode             SamplingFreq;     //!< Sampling frequency of the audio to encode (after SRC)
} CoderAudioMmeAccessorAudEncOutConfigCommon_t;

typedef struct
{
    CoderAudioMmeAccessorAudEncOutConfigCommon_t        Common;
    U8                                                  Config[NB_ENCODER_CONFIG_ELEMENT];  //!< Codec Specific encoder configuration
} CoderAudioMmeAccessorAudEncOutConfigGeneric_t;

typedef struct
{
    uint32_t numberInputBytesLeft;
    uint32_t numberOutputBytes;
    uint32_t byteStartAddress;


    enum eAccEncStatusError EncoderStatus;
    uint32_t BitRate;
    uint32_t PreProElapsedTime;                 //!< Elapsed time (us) in SFC
    uint32_t EncodeElapsedTime;                 //!< Elapsed time (us) in codec
    uint32_t TransfElapsedTime;                 //!< Elapsed time (us) in Transform
    uint32_t RdBuffElapsedTime;                 //!< Elapsed time (us) reading data
    uint32_t WrBuffElapsedTime;                 //!< Elapsed time (us) writing data
    uint32_t WtBuffElapsedTime;                 //!< Elapsed time (us) waiting for available buffer
    uint32_t ElapsedTime[AENC_NB_TIMERID];      //!< Reserved for SBAG Satistics
} CoderAudioMmeAccessorAudioEncoderStatusCommonStatus_t;

typedef struct
{
    uint32_t StructSize;
    CoderAudioMmeAccessorAudioEncoderStatusCommonStatus_t Status;
    uint32_t SpecificEncoderStatusBArraySize;
} CoderAudioMmeAccessorAudioEncoderStatusCommonParams_t;

typedef struct
{
    CoderAudioMmeAccessorAudioEncoderStatusCommonParams_t CommonParams;
    unsigned char SpecificEncoderStatusBArray[MAX_NB_ENCODER_TRANSFORM_RETURN_ELEMENT];
} CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t;

//AUDIO_ENCODER_HELPER_TYPES

/**
 * @brief Static Parameters set by the codec for CoderAudioMmme to function
 *
 * @var CoderAudioMmeStaticConfiguration_t::SpecificEncoderStatusBArraySize
 * @brief The size required to store status for all the codec buffers in one single transform
 *  the size must be enough to contain:
 *    max_n_frame_per_transform @ref MME_AudioEncoderFrameBufferStatus_t
 *    1 uint32_t to store SpecificStatusSize
 *    1 codec specific status e.g. 1 @ref MME_Ddce51Status_t
 *  in the codec sub-class we use a util-type e.g.  @ref CoderAudioMmeDdce51SpecificEncoderBArray_t
 *
 * @var  CoderAudioMmeStaticConfiguration_t::SpecificEncoderStatusStructSize
 * @brief The size of the transform status
 * Must be enough to contain
 *  1 @ref CoderAudioMmeAccessorAudioEncoderStatusCommonParams_t
 *  and
 *   @ref unsigned chars
 *   or
 *   @ref CoderAudioMmeStaticConfiguration_t::SpecificEncoderStatusBArraySize unsigned chars
 * in the codec subclasess we use s util type e.g @ref CoderAudioMmeDdce51StatusParams_t
 * */
typedef struct CoderAudioMmeStaticConfiguration_s
{
    uint32_t     EncoderId;  //!< Codec's ENCODERID see @ref MME_AudEncOutConfig_t::Id and use @ref ACC_GENERATE_ENCODERID
    uint32_t     CodecSpecificStatusSize;                     //!< Size of Codec Status, see CODECXYZ_EncoderTypes.h
    uint32_t     CodecSpecificConfigSize;                     //!< Size of Codec Config, see CODECXYZ_EncoderTypes.h
    uint32_t     MaxNbBufferedFrames;                         //!< Codec's maximum group delay expressed in frames
    uint32_t     MmeTransformQueueDepth;                      //!< How many MmeTransformContext are allocated?
    uint32_t     SizeOfMmeTransformContext;                   //!< Size of each Codec's TransformContext
    uint32_t     NrAllocatedCodedBuffersPerTransformContext;  //!< Number of CodedBuffers in each TransformContext
    uint32_t     NrAllocatedMMEBuffersPerTransformContext;    //!< Number of MMEBuffers in each TransformContext
    uint32_t     NrAllocatedScatterPagessPerTransformContext; //!< Number of ScatterPages in each TransformContext
    uint32_t     SpecificEncoderStatusBArraySize;             //!< Size of the codec specific status array
    uint32_t     SpecificEncoderStatusStructSize;             //!< Size of the Encoder status structure
    bool         Swap1;                                       //!< Corresponding the @ref tAudioEncoderOptionFlags1::Swap1
    bool         Swap2;                                       //!< Corresponding the @ref tAudioEncoderOptionFlags1::Swap2
    bool         Swap3;                                       //!< Corresponding the @ref tAudioEncoderOptionFlags1::Swap3

    BufferDataDescriptor_t                                    *MmeContextDescriptor;

    const char *AudioEncoderTransformerNames[ENCODER_STREAM_AUDIO_MAX_CPU];  //!< AudioEncoder transformer names for each running CPU
} CoderAudioMmeStaticConfiguration_t;

/**
 * @brief This structure is to store information about the codec for current Input */
typedef struct CoderAudioMmeCodecDynamicContext_s
{
    uint32_t NrSamplePerCodedFrame;  //!< Frame duration in current context (nchan, bitrate, etc)
    uint32_t MaxCodedFrameSize;      //!< Max frame size in current context (nchan, bitrate, etc)

    /*! CodecSpecific Settings: array of bytes for this class, filled in sub-class */
    /*  See Audio_EncoderTypes.h is in .../target/usr/include/ACC_Transformers */
    unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT];
} CoderAudioMmeCodecDynamicContext_t;

/* Purely MME implementation related */
typedef struct CoderAudioMmeDynamicContext_s
{
    enum eAccFsCode        FsCod;               //!< sampling frequency code
    enum eAccAcMode        AcMode;              //!< channel configuration code
    enum eAccWordSizeCode  WsCod;               //!< input pcm word size code
    uint32_t               BitRate;             //!< encoding bitrate in kbps
    uint32_t               BytesPerSample;      //!< input pcm number of bytes per sample
    uint32_t               NbChannelsSent;      //!< width of the input buffer sent to MTEncoder

    /* Input Buffer */
    uint32_t NumberOfChannelsInBuffer; //!< NbChanInBuffer * NbSamplesInBuffer * Sample_Size_B = mInputPayload.CurrentInputSize_B
    uint32_t NumberOfSamplesInBuffer;  //!< NbChanInBuffer * NbSamplesInBuffer * Sample_Size_B = mInputPayload.CurrentInputSize_B
    uint32_t Sample_Size_B;            //!< NbChanInBuffer * NbSamplesInBuffer * Sample_Size_B = mInputPayload.CurrentInputSize_B

    /* Output (these are set by SetCurentEncodingRequirements) */
    uint32_t MaxEsFrameSizeBForThisInput;     //!< Max number of bytes in an ES Frame for this Input
    uint32_t MaxCodedBufferSizeBForThisInput; //!< Max number of bytes in an Output Buffer_t for this Input
    uint32_t MaxNumberOfEncodedFrames;        //!< Max number of ES frames that can be produced for this context
    uint32_t SamplesEncodedPerFrame;          //!< Numberof samples per Frame
    uint32_t NrCodedBufferCurrentTransform;   //!< Number of Output Buffer_t to get for this Input(): in fine should be equal to MaxNumberOfEncodedFrames/NumberOfTransformPerInput
    uint32_t MmeBuffersPerCodedBuffer;        //!< Number of ES Frames that could go in each Output Buffer_t. In fine should always be 1.

    CoderAudioMmeCodecDynamicContext_t CodecDynamicContext; //!< Parameters to be updated by children codec

} CoderAudioMmeDynamicContext_t;

/**
 * @brief Contains common parts for what is required for processing an mme_encoder
 * Must be included in a codec specific transform context.
 */
typedef struct
{
    /*! @note TransformCmd has to be first in structure so that TransformContext can be retrieved from cast of MME_Command_t *CallbackData */
    MME_Command_t       TransformCmd;
    uint32_t  CommandCount;                      //!< Current Count for debug

    //Members filled by children classes
    uint32_t  StructSize;                        //!< The actual structure size
    uint32_t  NumberOfAllocatedBuffert;          //!< Number of Buffer_t in @var CoderAudioMmeTransformContext_t::CoderBuffers
    uint32_t  NumberOfAllocatedMMEBuffers;       //!< Number of MMeBuffer in @var CoderAudioMmeTransformContext_t::MMEBuffers
    uint32_t  NumberOfAllocatedMMEScatterPages;  //!< Number of ScatterPage in @var CoderAudioMmeTransformContext_t::MMEScatterPages

    //Control and Update managemement
    bool                                        ParameterUpdated;       //!< To notify parameter change
    CoderAudioControls_t                        Controls;               //!< Copy of controls for this transform
    MME_CommandId_t                             TransformCmdId;         //!< Would be used to Abort already sent command
    uint32_t                                    SamplesPerESFrame;      //!< For timestamp extrapolator
    stm_se_discontinuity_t                      Discontinuity;          //<! Discontinuity Associated (only EOS or not today)

    // Current Transform Command parameters
    uint32_t  MmeBuffersPerCodedBuffer;                  //!< Whether we have several es frames per coded buffer
    uint32_t  NumberOfCodedBufferForCurrentTransform;    //!< Nummber of CodedBuffers required for current transform

    // A transform command can be a "transform" xor a "send_global"
    union
    {
        MME_AudioEncoderGlobalParams_t          GlobalParams;
        MME_AudioEncoderTransformParams_t       CoderTransformParams;
    };

    // Buffer Reference Counters
    uint32_t   NumberOfCodedBufferReferencesTaken; //Counts number of CodedBuffer Attached to this context
    uint32_t   NumberOfCodedBufferReferencesFreed; //Counts number of CodedBuffer released from this context

    /* Actual allocated structures pointed by pointers below are in codec specific structures  */
    /* These value can only be set by Codec */
    CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t      *EncoderStatusParams_p;
    MME_DataBuffer_t                                            **MMEDataBuffers_p;
    MME_DataBuffer_t                                            *MMEBuffers;
    MME_ScatterPage_t                                           *MMEScatterPages;
    Buffer_t                                                    *CodedBuffers;
    Buffer_t                                                     CoderContextBuffer;
} CoderAudioMmeTransformContext_t;

/* @note Warning Conceptually it is possible to momentarily have NbPrepared < (NbAborted + NbCompleted)
   @note Warning Take care of unsigned potential issues */
typedef struct CoderAudioMmeCounters_s
{
    uint32_t  TransformPrepared;        //!< Nr of send transf commands, controlled from coder thread
    uint32_t  TransformAborted;         //!< Nr of aborted commands, controlled from multicom thread
    uint32_t  TransformCompleted;       //!< Nr of completed transf commands controlled from multicom thread
    uint32_t  SendParamsPrepared;       //!< Nr of sent param commands, controlled from coder thread
    uint32_t  SendParamsAborted;        //!< Nr of aborted  params commands controlled from multicom thread
    uint32_t  SendParamsCompleted;      //!< Nr of completed params commands controlled from multicom thread
    uint32_t  MaxNbSamplesPerTransform; //!< Maximum NumberOfSamples sent by the host is single transform
    uint32_t  ContinousBytesSent;       //!< Required to handle properly handle EOS with companion
} CoderAudioMmeCounters_t;

//! Store Input Buffer Info
typedef struct CoderAudioMmmeInputPayload_s
{
    uint32_t   Size_B;                  //!< Size of paylaod
    void      *CachedAddress;           //!< Pointer to payload
} CoderAudioMmmeInputPayload_t;

/**
 * @brief A Structure to pass pointers to all information required/useful to analyze change of parameters and pass results back
 * */
typedef struct CoderAudioMmeParameterUpdateAnalysis_s
{
    struct
    {
        stm_se_uncompressed_frame_metadata_t const *InputMetadata; //!< Pointer to previous InputMetadata
        CoderAudioControls_t                 const *Controls;      //!< Pointer to previous Controls
        CoderAudioMmeDynamicContext_t        const *MmeContext;    //!< Access to SE -> Multicom converted values
    } Prev;

    struct
    {
        stm_se_uncompressed_frame_metadata_t const *InputMetadata;  //!< Pointer to current InputMetadata
        CoderAudioControls_t                 const *Controls;       //!< Pointer to current Controls
        CoderAudioMmeDynamicContext_t        const *MmeContext;     //!< Access to SE -> Multicom converted values
    } Cur;

    struct
    {
        bool ChangeDetected;      //!< true if any parameter changed
        bool RestartTransformer;  //!< true if Codec config changed
        bool InputConfig;         //!< true if Input config changed
        bool OutputConfig;        //!< true if Output config changed
        bool CodecConfig;         //!< true if Codec config changed
    } Res;
} CoderAudioMmeParameterUpdateAnalysis_t;

/**
 * @brief Top MME Audio Encoder Class: all common mme encoder related code is implemented in this class.
 * For mme encoder most code is common to all codecs; to avoid code duplication MME calls are implemneted in this class,
 * while only codec specific code is instantiated in children codec classes.
 */
class Coder_Audio_Mme_c : public Coder_Audio_c
{
public:
    Coder_Audio_Mme_c(const CoderAudioStaticConfiguration_t &staticconfig,
                      const CoderBaseAudioMetadata_t &baseaudiometadata,
                      const CoderAudioControls_t &basecontrols,
                      const CoderAudioMmeStaticConfiguration_t &mmestaticconfig);
    ~Coder_Audio_Mme_c(void);

    CoderStatus_t          Halt(void);

    CoderStatus_t          LowPowerEnter(void);
    CoderStatus_t          LowPowerExit(void);

    virtual void           CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData);

protected:
    // Inherited Methods further polymorphed
    CoderStatus_t          Input(Buffer_t Buffer);
    CoderStatus_t          RegisterOutputBufferRing(Ring_t Ring);
    CoderStatus_t          InitializeCoder(void);
    CoderStatus_t          TerminateCoder(void);
    void                   InputPost(void);

    /*
     * Companion <> SE converstion methods These should be moved
     * to common code in audio_conversions.
     */

    /*
     * Methods that should be polymorphed by children classes
     * These should be used by children classes as:
     * "call parent first, then my specifics"
     */

    /**
     * @brief Analyses Previous Vs Current Context
     * @param Analysis A type specifically created for this function see @ref CoderAudioMmeParameterUpdateAnalysis_t
     * */
    virtual void           AnalyseCurrentVsPreviousParameters(CoderAudioMmeParameterUpdateAnalysis_t *);

    /**
     * Resets the current transform resturn status.
     *
     * In this class resets the common return params, children class
     * implementation must clear its specific return params
     *
     *
     * @return CoderStatus_t CoderNoError if ok
     */
    virtual CoderStatus_t  ResetCurrentTransformReturnParams(void);

    /**
     * Checks Current Controls and Metadata for compatibility issues
     *
     * @return bool true if compatible
     */
    virtual bool           AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *);

    /**
     * Fills the default values for the MTEncoder specific parames
     *
     * This is a pure virtual function as only the the children class knows what
     * they are.
     *
     * @param CodecSpecificConfig a pointer to an array of NB_ENCODER_CONFIG_ELEMENT
     *                bytes, representing the Specific Config
     */
    virtual void  FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT]) = 0 ;

    /**
     * Fills the MTEncoder GlobalParams based on current context (Input Metadata,
     * Controls, State)
     *
     * @param GlobalParams
     */
    virtual void           FillMmeGlobalParamsFromDynamicContext(MME_AudioEncoderGlobalParams_t *GlobalParams);

    /**
     * Makes the CurrentTransformContext parseable by Coder_Audio_Mme_c
     *
     * The Transform Context pool belongs to the children classes. In order for
     * Coser_Audio_Mme_c to be able to use the context some members need to be
     * filled by children class. see @ref CoderAudioMmeTransformContext_t
     *
     * @todo To improve readability maybe the members to fill need to be passed
     *       as params
     */
    virtual void           SetCurrentTransformContextSizesAndPointers(void);

    /*
     * Pure Virtual MME Specific functions to be implemented in children codec
     * classes
     */

    /**
     * Fills some dynamic information based on current Input and Controls
     *
     *
     * @param MmeDynamicContext The context structure to be filled
     * @param CurrentParams  The current Input and Controls
     *
     * @return CoderStatus_t CoderNoError if success
     */
    virtual CoderStatus_t  UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *MmeDynamicContext,
                                                     const CoderAudioCurrentParameters_t *CurrentParams) = 0;

    /**
     * returns the MTEncoder Audio Companion Capability mask
     *
     * This is order to be able to query the companion for its encoding capability,
     * vs. the requested codec
     *
     * @return uint32_t the codec mask
     */
    virtual uint32_t       GetCodecCapabilityMask(void) = 0;

    /* Debug Print Functions */
    virtual void           DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams);
    void                   DumpMmeInitParams(MME_TransformerInitParams_t *InitParams);
    void                   DumpMmeScatterPage(MME_ScatterPage_t *Scat, const char *MessageTag);
    void                   DumpMmeBuffer(MME_DataBuffer_t    *MmeBuf_p, const char *MessageTag);
    void                   DumpMmeStatusGenericParams(CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *Status, const char *MessageTag);
    void                   DumpMmeCmd(MME_Command_t *TransformCmd, const char *MessageTag);
    void                   DumpMmeCoderTransformParams(MME_AudioEncoderTransformParams_t *CoderTransformParams, const char *MessageTag);

    CoderAudioMmeStaticConfiguration_t          mMmeStaticConfiguration;
    CoderAudioMmeDynamicContext_t               mMmeDynamicContext;
    CoderAudioMmeTransformContext_t            *mCurrentTransformContext;        //!< For current Input()

private:
    CoderAudioMmeCounters_t                     mMmeCounters;
    bool                                        mMMECallbackPriorityBoosted;

    //Context
    Buffer_t                                    mCoderContextBuffer;
    BufferType_t                                mCoderContextBufferType;
    BufferPool_t                                mCoderContextBufferPool;

    // Current State
    CoderAudioMmmeInputPayload_t                mInputPayload;

    // Previous State
    CoderAudioMmeDynamicContext_t               mPreviousMmeDynamicContext;
    CoderAudioMmeParameterUpdateAnalysis_t      mParameterUpdateAnalysis;

    // MME/Multicom Structures
    MME_AudioEncoderInitParams_t                mMmeAudioEncoderInitParams;
    MME_TransformerInitParams_t                 mMmeInitParams;
    MME_TransformerHandle_t                     mTransformerHandle;              //!< Non 0 if transform init

    // Low power data
    bool                                        mIsLowPowerState;                //!< HPS/CPS: indicates when SE is in low power state
    bool                                        mIsLowPowerMMEInitialized;       //!< CPS: indicates whether MME transformer was initialized when entering low power state

    /// Output Extrapolator: required at least for EOS and last frame.
    TimeStampExtrapolator_c                     mOutputExtrapolation;

    /**
     * The coder output timestamps at callback are completely decorrelated from the
     * current transform input timestamps.<p>
     *  e.g. for mp3e: inject 1152 times 1 sample, obviously the timestamp at first
     * output is not related to the timestamp of the last Input(), but related to
     * that of the first input.<p>
     *  => we use the timestamps from companion, since it already maintains the
     *  buffered PTSs internally.<p>
     * <p>
     *  When a frame is delayed by the encoder, it means the first X samples are
     * from previous injected frames (X samples at 0 in the case of the first
     * frame). This means that the PTS of the first sample of the coded frame is the
     * PTS of the corresponding injected data minus an unsigned quantity.
     */
    struct DelayExtraction_s
    {
        TimeStamp_c                             FirstInjectedPts;
        TimeStamp_c                             FirstReceivedPts;
        TimeStamp_c                             TransformOffset;      //!< Measured Time offset introduced by transformer
        bool                                    IsDelay;              //!< if true "output time stamp" is "input time stamp - offset"
        DelayExtraction_s(void)
            : FirstInjectedPts(), FirstReceivedPts(), TransformOffset(), IsDelay(false) {}
    } mDelayExtraction;

    OS_Mutex_t    mAcquireAllMmeCommandContextsLock; /*!< Protects critical resources against concurrent calls*/

    /// Previous Nrt To Native offset, in case current one is invalid
    int64_t       mNrtOutputSavedOffset;

    /**
     * Fills @var Coder_Audio_Mme_c::mMmeInitParams Multicom Init command structure
     * based on current settings, sub classes to fill the specific config, in order
     * to initialize the transformer.
     *
     * @return CoderStatus_t CoderNoError if success
     */
    CoderStatus_t   FillMmeInitParams(void);

    /**
     * Instanciates the MME Tranformer for requested codec
     *
     *
     * @return CoderStatus_t CoderNoError if tranformer could be initiated
     */
    CoderStatus_t   InitializeMmeTransformer(void);

    /**
     * @brief Checks the companion capabilities vs. the current codec
     *
     *
     * @param SelectedCpu is the Audio CPU on which capability found.
     *
     * @return Return true if we find the codec on the companion, false otherwise.
     */
    bool   IsCodecSupportedByCompanion(uint32_t &SelectedCpu);

    /**
     * @brief Checks the get the companion capabilities on the selected cpu
     *
     *
     * @param SelectedCpu is the Audio CPU on which capability is requested.
     *
     * @return Return true if we find the codec on the companion, false otherwise.
     */
    bool  CheckCompanionCapability(uint32_t SelectedCpu);

    /**
     *
     * @return unsigned int Maximum coded buffer size (i.e. minimum
     *         to allocate per CodedBuffer)
     */
    unsigned int GetMaxCodedBufferSize(void);

    /**
     *
     * @return unsigned int Maximum nr frames returned in a single
     *         transform (i.e. minimum to allocate for a transform
     *         to succeed)
     */
    unsigned int GetMaxNrCodedBuffersPerTransform(void);

    /**
     * Recipe to compute the number of coded buffers to allocate
     *
     * @return unsigned int
     */
    unsigned int GetNrCodedBuffersToAllocate(void);

    /**
     * Waits for an available transform context, sets @var
     * Coder_Audio_Mme_c::CurrentTransformContext from the pool
     * of transform context
     *
     * Also fills the pointers and sizes for awareness of actual
     * allocated data.
     *
     *
     * @return CoderStatus_t CoderNoError if sucess
     */
    CoderStatus_t   GetNextFreeTransformContext(void);

    /**
     * Updates @var Coder_Audio_Mme_c::mMmeDynamicContext based on current context
     * such as Current Controls, Current Input
     *
     *
     * @return CoderStatus_t
     */
    CoderStatus_t   UpdateMmeDynamicContext(void);

    /**
     * Detects and handles changes in Input and/or Controls (blocking)
     *
     * Can issue send commands, can terminate/init the transformer if required.
     *
     * In case of errors it cleans-up after itself: no need to release buffers if
     * this function fails, just return from Input() with Error status.
     *
     * @param InputBuffer the current input Buffer_t
     *
     * @return CoderStatus_t CoderNoError if success
     */
    CoderStatus_t   HandleChangeDetection(Buffer_t InputBuffer);

    /**
     * Gets the required number of CodedBuffers from pool for the current transform
     * (blocking)
     *
     *
     * @param InputBuffer the current input Buffer_t
     *
     * @return CoderStatus_t CoderNoError if success
     */
    CoderStatus_t   GetRequiredNrCodedBuffers(Buffer_t InputBuffer);

    /**
     * Fill @var
     * Coder_Audio_Mme_c::CurrentTransformContext for sending a Transform Command
     *
     *
     * @return CoderStatus_t CoderNoError if success
     */
    CoderStatus_t   FillCurrentTransformContext(void);

    /**
     * Sends an MmeCommand, whether a Transform or GlobalParams
     *
     *
     * @param MmeCommand pointer to the command structure
     *
     * @return CoderStatus_t CoderNoError if success
     */
    CoderStatus_t   SendCommand(MME_Command_t *MmeCommand);

    /**
     * Terminates the transformer:
     *
     * Caller is to take precautions that there is no pending
     * non-returned MME command  prior to calling this.
     *
     * @return CoderStatus_t CoderNoError all commands completeted and term
     *         successful
     */
    CoderStatus_t   TerminateMmeTransformer(void);                                   //!< @note To be called in Halt(void) ?

    /**
     * (blocking) Acquires all Mmme Command Contexts; doing so
     * ensures that all previously issues mme commands have
     * completed.
     * When Success is true all Contexts are owned by caller, stored
     * in ContextBufferArray, and must be freed later by
     * calling @ref Coder_Audio_Mme_c::ReleaseAllMmeCommandContexts
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
     * @return CoderStatus_t CoderNoError in case of no failure;
     *         CoderNoError != Success
     */
    CoderStatus_t AcquireAllMmeCommandContexts(Buffer_t ContextBufferArray[], bool *Success,
                                               bool AbortOnNonRunning = true);


    /**
     * Releases all Mme Context buffers obtained through
     * @ref Coder_Audio_Mme_c::AcquireAllMmeCommandContexts
     *
     * @param ContextBufferArray
     *               An array of Buffer_t to contained all obtained resources
     *
     * @return CoderStatus_t CoderNoError if no error
     */
    CoderStatus_t ReleaseAllMmeCommandContexts(Buffer_t ContextBufferArray[]);

    /**
     * Releases the indexed CodedBuffer reference from a transform context
     *
     *
     * @param Context the transform context
     * @param index the index of the coded buffer we need released
     *
     * @return void
     */
    void ReleaseCodedBufferFromTransformContext(CoderAudioMmeTransformContext_t *Context, uint32_t index);

    /**
     * Releases all the CodedBuffer refrences attached to a transform context
     *
     *
     * @param Context the transform context
     *
     * @param OptStr An optional string to have messages in case of
     *               errors/warnings.
     */
    void ReleaseAllCodedBufferInMmeTransformContext(CoderAudioMmeTransformContext_t *Context, const char *OptStr = NULL);

    /**
     * Releases the transform Context to the pool <br> (Any
     * data in the transform context should have been relased prior)
     *
     * @param Ctx   the transform Context
     * @param OptStr An optional string to have messages in case of
     *               errors/warnings.
     */
    void ReleaseTransformContext(CoderAudioMmeTransformContext_t *Ctx, const char *OptStr = NULL);

    /**
     * Fills @var Coder_Audio_Mme_c::CurrentTransformContext to send new
     * GlobalParams command
     *
     * Based on current context, forms a SendGlobalParam command
     *
     */
    void            FillCurrentTransformContextSendGlobal(void);

    /**
     * Parses the multicom callback context for detection of any issue, and
     * getting information for further processing
     *
     * @param TrnsfrmCntxt The current transform context
     * @param InputMetadata_p to return a pointer to the InputMetatadata used with
     *                this transform
     * @param MmmeAudioEncoderBufferFrameStatus_p to return a pointer to the
     *                        first Encode status
     *                        associated with this
     *                        transform
     *
     *
     * @return CoderStatus_t CoderNoError if parsing found no error
     */
    CoderStatus_t   ParseMmeTransformCallback(CoderAudioMmeTransformContext_t *TrnsfrmCntxt,
                                              stm_se_uncompressed_frame_metadata_t **InputMetadata_p,
                                              MME_AudioEncoderFrameBufferStatus_t **MmmeAudioEncoderBufferFrameStatus_p);

    /**
     * Gets the payload information for indexed CodedBuffer
     *
     *
     * @param TrnsfrmCntxt current transform context
     * @param coded_buffer_ix index of the CodedBuffer
     * @param MmmeAudioEncoderBufferFrameStatus_p input: pointer to the indexed
     *                        CodedBuffer's status. output:
     *                        incremented to point to the next
     *                        CodedBuffer status
     *
     * @return uint32_t the number of bytes for the indexed CodedBuffer payload
     */
    uint32_t GetNrOfBytesForThisCodedBufferInMmeTransformCallback(CoderAudioMmeTransformContext_t *TrnsfrmCntxt,
                                                                  uint32_t coded_buffer_ix,
                                                                  MME_AudioEncoderFrameBufferStatus_t **MmmeAudioEncoderBufferFrameStatus_p);

    /**
     * Fills output metadata for indexed CodedBuffer<p>
     *
     * @param TrnsfrmCntxt
     *               current transform context
     * @param coded_buf_ix
     *               index of the CodedBuffer in the array
     * @param InputMetadata_p
     *               the corresponding input metadata
     * @param AudioEncoderBufferFrameStatus_p
     *               the indexed CodedBuffer encoder status
     * @param LastBufferInTransform
     *               true if there are no more bytes after this buffer
     *
     * @return CoderStatus_t
     */
    CoderStatus_t   FillCodedBufferMetadata(CoderAudioMmeTransformContext_t *TrnsfrmCntxt,
                                            uint32_t coded_buf_ix,
                                            stm_se_uncompressed_frame_metadata_t *InputMetadata_p,
                                            MME_AudioEncoderFrameBufferStatus_t *AudioEncoderBufferFrameStatus_p,
                                            bool LastBufferInTransform);
    /**
     * Fills the CodedBuffer Process Metadata
     *
     *
     * @param ProcessMetadata structure to fill
     * @param TrsfrmControls Controls associated with the current CodedBuffer
     * @param Update true if the controls were updated for this CodedBuffer
     */
    void            FillCodedBufferProcessMetadata(stm_se_encode_process_metadata_t *ProcessMetadata,
                                                   const CoderAudioControls_t *TrsfrmControls,
                                                   bool Update);

    /* Inline helper functions */

    /**
     * Returns the total number of Multicom command issued.
     *
     * While transformer is running, accurate only from command issuing thread
     *
     * @return uint32_t
     */
    uint32_t        NrIssuedCommands(void);

    /**
     * Returns the total number of Muticom Command callbacks.
     *
     * While transformer is running, accurate only from a callback
     *
     * @return uint32_t
     */
    uint32_t        NrDoneCommands(void);

    /**
     * Resets the state of the Delay Extraction logic
     *
     */
    void DelayExtractionReset(void)
    {
        TimeStamp_c InvalidTime;
        mDelayExtraction.FirstInjectedPts = InvalidTime;
        mDelayExtraction.FirstReceivedPts = InvalidTime;
        mDelayExtraction.TransformOffset  = InvalidTime;
    }

    /**
     * Feeds the transform input PTS to the Delay Extraction logic
     *
     *
     * @param InputPts
     */
    void DelayExtractionInputPts(TimeStamp_c InputPts)
    {
        if (!(mDelayExtraction.FirstInjectedPts.IsValid()))
        {
            mDelayExtraction.FirstInjectedPts = InputPts.Pts();
        }
    }

    /**
     * Feeds the transform output PTS to the Delay Extraction logic and extracts
     * the encoder delay information from the context and the transformer
     * output PTS <p>
     * <p>
     *  If the delay has not been determined yet, it tries to be.<p>
     *  In order to get the delay it is necessary to have the first valid injected
     *  timestamps and the first valid output timestamp.<p>
     *  Then we use the utility function StmSeAudioGetOffsetFrom33BitPtsTimeStamps
     *  that compares the timestamps to extract the delay.
     *
     * @param IsDelay true if FirstReceivedPts is analyzed to be smaller than
     *            FirstReceivedPts
     *
     * @param OutputPts the output Timestamp from the transform
     *
     * @return TimeStamp_c the computed offset (can be Invalid if not enough data
     *         received yet)
     */
    TimeStamp_c DelayExtractionGetDelay(bool &IsDelay, TimeStamp_c OutputPts);

    /**
     * Creates the MmeContext Pool bases on static confiration
     *
     *
     * @return CoderStatus_t
     */
    CoderStatus_t CreateMmeContextPool(void);

    DISALLOW_COPY_AND_ASSIGN(Coder_Audio_Mme_c);
};

#endif /* CODER_AUDIO_MME_H */
