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

#ifndef H_CODEC_MME_BASE
#define H_CODEC_MME_BASE

#include "player.h"
#include "acc_mme.h"
#include "allocinline.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeBase_c"
// /////////////////////////////////////////////////////////////////////////
//
// Frequently used macros
//


#define CODEC_MAX_WAIT_FOR_MME_COMMAND_COMPLETION       100     /* Ms */
#define CODEC_MAX_TRANSFORMERS                          4


typedef struct CodecConfiguration_s
{
    const char                   *CodecName;

    DecodeBufferComponentDescriptor_t   ListOfDecodeBufferComponents[NUMBER_OF_COMPONENTS + 1];

    char                          TranscodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    char                          AncillaryMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned int                  StreamParameterContextCount;
    BufferDataDescriptor_t       *StreamParameterContextDescriptor;

    unsigned int                  DecodeContextCount;
    BufferDataDescriptor_t       *DecodeContextDescriptor;

    unsigned int                  MaxDecodeIndicesPerBuffer;

    BufferType_t                  AudioVideoDataParsedParametersType;
    void                        **AudioVideoDataParsedParametersPointer;
    unsigned int                  SizeOfAudioVideoDataParsedParameters;

    const char                   *TransformName[CODEC_MAX_TRANSFORMERS];
    unsigned int                  AvailableTransformers;
    unsigned int                  SizeOfTransformCapabilityStructure;
    void                         *TransformCapabilityStructurePointer;

    AddressType_t                 AddressingMode;

    bool                          ShrinkCodedDataBuffersAfterDecode;
    bool                          IgnoreFindCodedDataBuffer;

    CodecTrickModeParameters_t    TrickModeParameters;

    //
    // Video only flags
    //

    bool                          SliceDecodePermitted;
    bool                          DecimatedDecodePermitted;

    void                         *DeltaTopCapabilityStructurePointer;

    //
    // Audio only flags
    //

    unsigned int                  MaximumSampleCount;

    unsigned int                  TranscodedFrameMaxSize;
    unsigned int                  CompressedFrameMaxSize;

    CodecConfiguration_s(void)
        : CodecName("not-specified")
        , ListOfDecodeBufferComponents()
        , TranscodedMemoryPartitionName()
        , AncillaryMemoryPartitionName()
        , StreamParameterContextCount(0)
        , StreamParameterContextDescriptor(NULL)
        , DecodeContextCount(0)
        , DecodeContextDescriptor(NULL)
        , MaxDecodeIndicesPerBuffer(0)
        , AudioVideoDataParsedParametersType(0)
        , AudioVideoDataParsedParametersPointer(NULL)
        , SizeOfAudioVideoDataParsedParameters(0)
        , TransformName()
        , AvailableTransformers(0)
        , SizeOfTransformCapabilityStructure(0)
        , TransformCapabilityStructurePointer(NULL)
        , AddressingMode(CachedAddress)
        , ShrinkCodedDataBuffersAfterDecode(false)
        , IgnoreFindCodedDataBuffer(false)
        , TrickModeParameters()
        , SliceDecodePermitted(false)
        , DecimatedDecodePermitted(false)
        , DeltaTopCapabilityStructurePointer(NULL)
        , MaximumSampleCount(0)
        , TranscodedFrameMaxSize(0)
        , CompressedFrameMaxSize(0)
    {}
} CodecConfiguration_t;

typedef enum
{
    CodecSelectTransformer              = BASE_CODEC,
    CodecSpecifyDRC,
    CodecSpecifyDownmix,
} CodecParameterBlockType_t;

typedef struct _DRCParams_s
{
    unsigned int                          DRC_Enable;
    unsigned int                          DRC_Type;
    unsigned int                          DRC_HDR;
    unsigned int                          DRC_LDR;
} DRCParams_t;

typedef struct _DownmixParams_s
{
    bool StreamDrivenDownmix;
} DownmixParams_t;

typedef struct CodecParameterBlock_s
{
    CodecParameterBlockType_t      ParameterType;

    union
    {
        unsigned int               Transformer;
        char                       PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
        PlayerChannelSelect_t      Channel;
        DRCParams_t                DRC;
        DownmixParams_t            Downmix;
    };
} CodecParameterBlock_t;

typedef struct CodecIntensityCompensation_s
{
    unsigned int                  Top1;
    unsigned int                  Bottom1;
    unsigned int                  Top2;
    unsigned int                  Bottom2;
} CodecIntensityCompensation_t;


typedef struct CodecBufferState_s
{
    Buffer_t              Buffer;

    bool                  OutputOnDecodesComplete;      // Copy to output ring when decodes in progress reaches zero
    unsigned int          DecodesInProgress;            // For video fields/slices may be several

    unsigned int          ReferenceFrameCount;          // Number of reference frames in this buffer for
    // video field/slice decodes this could be 2 or more

    bool                  FieldDecode;          // false by default, used in calculating frame decode time

    unsigned int          BufferQualityWeight;      // Number of decodes contributing to buffer quality
    Rational_t            BufferQuality;        // Store the quality of the decoded buffer.

    //
    // Stream specific union
    //

    union
    {
        void                     *AudioVideoDataParsedParameters;
        ParsedVideoParameters_t  *ParsedVideoParameters;
        ParsedAudioParameters_t  *ParsedAudioParameters;
    };

    //
    // Video specifics
    //

    unsigned int                  PictureSyntax;                        // Used by VC1
    CodecIntensityCompensation_t  AppliedIntensityCompensation;

    bool                          DecodedAsFrames;                      // Flag used by H264
    bool                          DecodedWithMbaff;                     // Flag used by H264
    unsigned int                  ReferenceFrameSlot;                   // Value used by H264

    CodecBufferState_s(void)
        : Buffer(NULL)
        , OutputOnDecodesComplete(false)
        , DecodesInProgress(0)
        , ReferenceFrameCount(0)
        , FieldDecode(false)
        , BufferQualityWeight(0)
        , BufferQuality()
        , PictureSyntax(0)
        , AppliedIntensityCompensation()
        , DecodedAsFrames(false)
        , DecodedWithMbaff(false)
        , ReferenceFrameSlot(0)
    {}
} CodecBufferState_t;

typedef struct CodecIndexBufferMap_s
{
    unsigned int        DecodeIndex;
    unsigned int        BufferIndex;
} CodecIndexBufferMap_t;

typedef struct CodecBaseStreamParameterContext_s
{
    MME_Command_t      MMECommand;                     // Must be first element

    Buffer_t           StreamParameterContextBuffer;   // The buffer class that I am part of
} CodecBaseStreamParameterContext_t;

// HEVC like H264 could have large no of reference frames. In SW-HEVC,
// all reference frames/buffers are passed using databuffers hence
// following defines have increased value

#define MME_MAX_BUFFERS_PER_DECODE 12 // previously value was 7
#define MME_MAX_PAGES_PER_BUFFER   2  // previously value was 1

typedef struct CodecBaseDecodeContext_s
{
    MME_Command_t          MMECommand;           // Must be first element

    Buffer_t               DecodeContextBuffer;  // The buffer class that I am part of

    MME_DataBuffer_t      *MMEBufferList[MME_MAX_BUFFERS_PER_DECODE];
    MME_DataBuffer_t       MMEBuffers[MME_MAX_BUFFERS_PER_DECODE];
    MME_ScatterPage_t      MMEPages[MME_MAX_BUFFERS_PER_DECODE *MME_MAX_PAGES_PER_BUFFER];

    bool                   DecodeInProgress;
    unsigned int           BufferIndex;

    bool                   UsedReferenceFrameSubstitution;
    bool                   IndependentFrame;
    unsigned int           NumberOfReferenceFrameLists;
    ReferenceFrameList_t   ReferenceFrameList[MAX_REFERENCE_FRAME_LISTS];

    // Store the quality of the decode (may be only field, and does not encompass the reference frame list).
    Rational_t             DecodeQuality;

    unsigned long long     DecodeCommenceTime;
} CodecBaseDecodeContext_t;

/// Provides a buffer and MME context management framework to assist codec implementation.
class Codec_MmeBase_c : public Codec_c
{
public:
    Codec_MmeBase_c(void);
    ~Codec_MmeBase_c(void);

    CodecStatus_t   Halt(void);

    CodecStatus_t   SetModuleParameters(unsigned int      ParameterBlockSize,
                                        void             *ParameterBlock);

    void            UpdateConfig(unsigned int Update) {}
    //
    // Codec class functions
    //

    CodecStatus_t   GetTrickModeParameters(CodecTrickModeParameters_t      *TrickModeParameters);

    CodecStatus_t   Connect(Port_c *Port);

    CodecStatus_t   OutputPartialDecodeBuffers(void);

    CodecStatus_t   DiscardQueuedDecodes(void);

    CodecStatus_t   ReleaseReferenceFrame(unsigned int              ReferenceFrameDecodeIndex);

    CodecStatus_t   CheckReferenceFrameList(unsigned int              NumberOfReferenceFrameLists,
                                            ReferenceFrameList_t      ReferenceFrameList[]);

    CodecStatus_t   ReleaseDecodeBuffer(Buffer_t                  Buffer);

    CodecStatus_t   Input(Buffer_t                  CodedBuffer);
    CodecStatus_t   UpdatePlaybackSpeed(void);

    // Low power methods
    CodecStatus_t   LowPowerEnter(void);
    CodecStatus_t   LowPowerExit(void);

    virtual void GlobalParamsCommandCompleted(CodecBaseStreamParameterContext_t *StreamParameterContext);

    //
    virtual void   CallbackFromMME(MME_Event_t               Event,
                                   MME_Command_t            *Command);

protected:
    OS_Mutex_t                            Lock;
    CodecConfiguration_t                  Configuration;
    unsigned int                          SelectedTransformer;  // Which CPU is selected after checking the capability.
    bool                                  ForceStreamParameterReload;

    BufferManager_t                       BufferManager;
    bool                                  DataTypesInitialized;

    bool                                  MMEInitialized;
    MME_TransformerHandle_t               MMEHandle;
    MME_TransformerInitParams_t           MMEInitializationParameters;
    unsigned int                          MMECommandPreparedCount;              // Mme command counts
    unsigned int                          MMECommandAbortedCount;
    unsigned int                          MMECommandCompletedCount;
    bool                                  MMECallbackPriorityBoosted;

    BufferPool_t                          CodedFrameBufferPool;
    BufferType_t                          CodedFrameBufferType;

    BufferPool_t                          DecodeBufferPool;
    unsigned int                          DecodeBufferCount;
    Port_c                               *mOutputPort;

    BufferPool_t                          StreamParameterContextPool;
    BufferDataDescriptor_t               *StreamParameterContextDescriptor;
    BufferType_t                          StreamParameterContextType;
    Buffer_t                              StreamParameterContextBuffer;

    BufferPool_t                          DecodeContextPool;
    BufferDataDescriptor_t               *DecodeContextDescriptor;
    BufferType_t                          DecodeContextType;
    Buffer_t                              DecodeContextBuffer;

    unsigned int                          IndexBufferMapSize;
    CodecIndexBufferMap_t                *IndexBufferMap;

    Buffer_t                              CodedFrameBuffer;
    unsigned int                          CodedDataLength;
    unsigned char                        *CodedData;
    unsigned char                        *CodedData_Cp;
    ParsedFrameParameters_t              *ParsedFrameParameters;

    CodecBufferState_t                    BufferState[MAX_DECODE_BUFFERS];

    unsigned int                          CurrentDecodeBufferIndex;
    Buffer_t                              CurrentDecodeBuffer;
    unsigned int                          CurrentDecodeIndex;
    CodecBaseStreamParameterContext_t    *StreamParameterContext;
    CodecBaseDecodeContext_t             *DecodeContext;

    Buffer_t                              MarkerBuffer;
    unsigned int                          PassOnMarkerBufferAt;

    unsigned int                          DiscardDecodesUntil;

    unsigned int                          DecodeTimeShortIntegrationPeriod;
    unsigned int                          DecodeTimeLongIntegrationPeriod;
    unsigned int                          NextDecodeTime;
    // Index for the table DecodeTimesForIOnly to store the decode values
    unsigned int                          NextDecodeTimeForIOnly;
    unsigned long long                    LastDecodeCompletionTime;
    unsigned long long                    DecodeTimes[16 * MAX_DECODE_BUFFERS];
    // Table storing the decode values from the point where the frame enters the decode process to when it is decoded
    unsigned long long                    DecodeTimesForIOnly[16 * MAX_DECODE_BUFFERS];
    unsigned long long                    ShortTotalDecodeTime;
    unsigned long long                    LongTotalDecodeTime;
    // Decode values averaged on a short and longer period.
    // The integrated value is based on the values in DecodeTimesForIOnly and used to set the I only decode mode threshold
    unsigned long long                    ShortTotalDecodeTimeForIOnly;
    unsigned long long                    LongTotalDecodeTimeForIOnly;

    bool                                  ReportedMissingReferenceFrame;
    bool                                  ReportedUsingReferenceFrameSubstitution;

    // Low power data
    // CPS: indicates whether MME transformer was initialized when entering low power state
    bool                                  IsLowPowerMMEInitialized;

    CodecStatus_t   InitializeDataType(BufferDataDescriptor_t   *InitialDescriptor,
                                       BufferType_t             *Type,
                                       BufferDataDescriptor_t  **ManagedDescriptor);

    CodecStatus_t   CalculateMaximumFrameRate(CodecBaseDecodeContext_t *DecodeContext);

    virtual CodecStatus_t   VerifyMMECapabilities(unsigned int ActualTransformer);
    virtual CodecStatus_t   VerifyMMECapabilities() { return VerifyMMECapabilities(SelectedTransformer); }

    CodecStatus_t           GloballyVerifyMMECapabilities(void);
    virtual CodecStatus_t   InitializeMMETransformer(void);
    virtual CodecStatus_t   TerminateMMETransformer(void);      // Required by StreamBase audio decoder classes to flush buffer queues
    Buffer_t                TakeMarkerBuffer(void);

    virtual CodecStatus_t   MapBufferToDecodeIndex(unsigned int        DecodeIndex,
                                                   unsigned int        BufferIndex);

    virtual CodecStatus_t   UnMapBufferIndex(unsigned int              BufferIndex);

    virtual CodecStatus_t   TranslateDecodeIndex(unsigned int          DecodeIndex,
                                                 unsigned int         *BufferIndex);

    CodecStatus_t   GetDecodeBuffer(void);

    virtual CodecStatus_t   ReleaseDecodeContext(CodecBaseDecodeContext_t *Context);

    CodecStatus_t           ReleaseDecodeBufferLocked(Buffer_t Buffer);

    CodecStatus_t   SetOutputOnDecodesComplete(unsigned int            BufferIndex,
                                               bool                    TestForImmediateOutput);

    void   UpdateBufferQuality(CodecBaseDecodeContext_t *Context);

    void   DumpMMECommand(MME_Command_t *CmdInfo_p);

    virtual CodecStatus_t   TranslateReferenceFrameLists(bool IncrementUseCountForReferenceFrame);
    CodecStatus_t   TranslateSetOfReferenceFrameLists(bool                  IncrementUseCountForReferenceFrame,
                                                      bool                  ParsedReferenceFrame,
                                                      ReferenceFrameList_t  ParsedReferenceFrameList[MAX_REFERENCE_FRAME_LISTS]);

    virtual CodecStatus_t   SendMMEStreamParameters(void);              // DivX needs its own version
    virtual CodecStatus_t   SendMMEDecodeCommand(void);                 // WMA/OGG need to enhance

    virtual CodecStatus_t   HandleCapabilities(void) = 0;
    virtual CodecStatus_t   HandleCapabilities(unsigned int ActualTransformer) { return HandleCapabilities(); };
    virtual CodecStatus_t   ParseCapabilities(unsigned int ActualTransformer) { return CodecNoError; };

    //
    // Virtual functions that can be extended by my inheritors
    //

    virtual CodecStatus_t   InitializeDataTypes(void);
    virtual CodecStatus_t   FillOutSendBufferCommand(void);     // This function may only implemented for stream base inheritors.
    virtual CodecStatus_t   TestMarkerFramePassOn(void);
    virtual CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context) { return CodecNoError; }

    //
    // Virtual functions that should be implemented by my inheritors
    //
    virtual CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t *Request) = 0;
    virtual CodecStatus_t   FillOutTransformerInitializationParameters(void) = 0;
    virtual CodecStatus_t   FillOutSetStreamParametersCommand(void) = 0;
    virtual CodecStatus_t   FillOutDecodeCommand(void) = 0;
    virtual CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context) = 0;

    virtual CodecStatus_t   DumpSetStreamParameters(void *Parameters) = 0;
    virtual CodecStatus_t   DumpDecodeParameters(void    *Parameters) = 0;

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_MmeBase_c);

    CodecStatus_t   DecrementAccessCount(unsigned int  BufferIndex,
                                         bool          ForReference);
};

#endif
