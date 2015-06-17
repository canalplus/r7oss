/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : codec_mme_base.h
Author :           Nick

Definition of the codec base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-Jan-07   Created                                         Nick
11-Sep-07   Added Event/Count for stream output codecs      Adam

************************************************************************/

#ifndef H_CODEC_MME_BASE
#define H_CODEC_MME_BASE

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "player.h"
#include "acc_mme.h"
#include "allocinline.h"

// /////////////////////////////////////////////////////////////////////////
//
// Frequently used macros
//

#ifndef ENABLE_CODEC_DEBUG
#define ENABLE_CODEC_DEBUG 0
#endif

#ifndef CODEC_TAG
#define CODEC_TAG      "Codec"
#endif

#define CODEC_FUNCTION (ENABLE_CODEC_DEBUG ? __PRETTY_FUNCTION__ : CODEC_TAG)

/* Output debug information (which may be on the critical path) but is usually turned off */
#define CODEC_DEBUG(fmt, args...) ((void)(ENABLE_CODEC_DEBUG && \
					  (report(severity_note, "%s: " fmt, CODEC_FUNCTION, ##args), 0)))

/* Output trace information off the critical path */
#define CODEC_TRACE(fmt, args...) (report(severity_note, "%s: " fmt, CODEC_FUNCTION, ##args))
/* Output errors, should never be output in 'normal' operation */
#define CODEC_ERROR(fmt, args...) (report(severity_error, "%s: " fmt, CODEC_FUNCTION, ##args))

#define CODEC_ASSERT(x) do if(!(x)) report(severity_error, "%s: Assertion '%s' failed at %s:%d\n", \
					       CODEC_FUNCTION, #x, __FILE__, __LINE__); while(0)

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#ifdef CONFIG_32BIT
#define CODEC_MAX_DECODE_BUFFERS                        64
#else
#define CODEC_MAX_DECODE_BUFFERS                        32
#endif

#define CODEC_MAX_WAIT_FOR_MME_COMMAND_COMPLETION       100     /* Ms */
#define CODEC_MAX_TRANSFORMERS                          4

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct CodecConfiguration_s
{
    const char                   *CodecName;

    char                          TranscodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    char                          AncillaryMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    BufferFormat_t                DecodeOutputFormat;

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

    //
    // Audio only flags
    //

    unsigned int                  MaximumSampleCount;

} CodecConfiguration_t;

//

typedef enum
{
    CodecSelectTransformer              = BASE_CODEC,
    CodecSpecifyTranscodedMemoryPartition,
    CodecSpecifyAncillaryMemoryPartition,
    CodecSelectChannel,
    CodecSpecifyTransformerPostFix,
    CodecSpecifyDRC,
    CodecSpecifyDownmix,
} CodecParameterBlockType_t;

typedef struct _DRCParams_s
{
    unsigned int                          Enable;
    unsigned int                          Type;
    unsigned int                          HDR;
    unsigned int                          LDR;
} DRCParams_t;

typedef struct _DownmixParams_s
{
    int OutmodeMain;
} DownmixParams_t;

//

typedef struct CodecParameterBlock_s
{
    CodecParameterBlockType_t      ParameterType;

    union
    {
	unsigned int               Transformer;
	char                       PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
	PlayerChannelSelect_t      Channel;
	struct
	{
	    unsigned int           Transformer;
	    char                   PostFix;
	}                          TransformerPostFix;
	DRCParams_t                DRC;
	DownmixParams_t            Downmix;
    };
} CodecParameterBlock_t;

//

typedef struct CodecIntensityCompensation_s
{
    unsigned int                  Top1;
    unsigned int                  Bottom1;
    unsigned int                  Top2;
    unsigned int                  Bottom2;
} CodecIntensityCompensation_t;


typedef struct CodecBufferState_s
{
    Buffer_t                      Buffer;
    unsigned char                *BufferPointer;
    unsigned int                  BufferLength;

    bool                          OutputOnDecodesComplete;      // Copy to output ring when decodes in progress reaches zero
    unsigned int                  DecodesInProgress;            // For video fields/slices may be several 

    unsigned int                  ReferenceFrameCount;          // Number of reference frames in this buffer for 
								// video field/slice decodes this could be 2 or more

    BufferStructure_t            *BufferStructure;

    bool			  FieldDecode;			// false by default, used in calculating frame decode time

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

    unsigned char                *BufferLumaPointer;
    unsigned char                *BufferChromaPointer;
    unsigned char                *BufferRasterPointer;

    bool			  MacroblockStructurePresent;    	// Only for some types of video, currently only h264 uses the first 2 fields, 
    Buffer_t			  BufferMacroblockStructure;		// because only h264 allocates a buffer separate from the decode buffer.
    unsigned char                *BufferMacroblockStructurePointer;
    unsigned int                  PictureSyntax;                        // Used by VC1
    CodecIntensityCompensation_t  AppliedIntensityCompensation;

    bool                          DecodedAsFrames;                      // Flag used by H264
    bool                          DecodedWithMbaff;                     // Flag used by H264
    unsigned int                  ReferenceFrameSlot;                   // Value used by H264

    unsigned char                *DecimatedLumaPointer;
    unsigned char                *DecimatedChromaPointer;

} CodecBufferState_t;

//

typedef struct CodecIndexBufferMap_s
{
    unsigned int        DecodeIndex;
    unsigned int        BufferIndex;
} CodecIndexBufferMap_t;

//

typedef struct CodecBaseStreamParameterContext_s
{
    MME_Command_t               MMECommand;                     // Must be first element

    Buffer_t                    StreamParameterContextBuffer;   // The buffer class that I am part of
} CodecBaseStreamParameterContext_t;

//

#define MME_MAX_BUFFERS_PER_DECODE 6 // cjt divx seems to use loads (was 2)
#define MME_MAX_PAGES_PER_BUFFER   1

typedef struct CodecBaseDecodeContext_s
{
    MME_Command_t               MMECommand;                     // Must be first element

    Buffer_t                    DecodeContextBuffer;            // The buffer class that I am part of

    MME_DataBuffer_t            *MMEBufferList[MME_MAX_BUFFERS_PER_DECODE];
    MME_DataBuffer_t            MMEBuffers[MME_MAX_BUFFERS_PER_DECODE];
    MME_ScatterPage_t           MMEPages[MME_MAX_BUFFERS_PER_DECODE * MME_MAX_PAGES_PER_BUFFER];

    bool                        DecodeInProgress;
    unsigned int                BufferIndex;

    unsigned int                NumberOfReferenceFrameLists;
    ReferenceFrameList_t        ReferenceFrameList[MAX_REFERENCE_FRAME_LISTS];

    unsigned long long          DecodeCommenceTime;
} CodecBaseDecodeContext_t;


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Provides a buffer and MME context management framework to assist codec implementation.
class Codec_MmeBase_c : public Codec_c
{
protected:

    // Data

    OS_Mutex_t                            Lock;
    CodecConfiguration_t                  Configuration;
    unsigned int                          SelectedTransformer;
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
    Ring_t                                OutputRing;

    BufferPool_t                          PostProcessControlBufferPool;

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
    ParsedFrameParameters_t              *ParsedFrameParameters;

    CodecBufferState_t                    BufferState[CODEC_MAX_DECODE_BUFFERS];

    unsigned int                          CurrentDecodeBufferIndex;
    Buffer_t                              CurrentDecodeBuffer;
    unsigned int                          CurrentDecodeIndex;
    CodecBaseStreamParameterContext_t    *StreamParameterContext;
    CodecBaseDecodeContext_t             *DecodeContext;

    Buffer_t                              MarkerBuffer;
    unsigned int                          PassOnMarkerBufferAt;

    unsigned int                          DiscardDecodesUntil;

    unsigned int			  DecodeTimeShortIntegrationPeriod;
    unsigned int			  DecodeTimeLongIntegrationPeriod;
    unsigned int			  NextDecodeTime;
    unsigned long long			  LastDecodeCompletionTime;
    unsigned long long			  DecodeTimes[16 * CODEC_MAX_DECODE_BUFFERS];
    unsigned long long			  ShortTotalDecodeTime;
    unsigned long long			  LongTotalDecodeTime;

    // Functions

    CodecStatus_t   InitializeDataType(         BufferDataDescriptor_t   *InitialDescriptor,
						BufferType_t             *Type,
						BufferDataDescriptor_t  **ManagedDescriptor );

    CodecStatus_t   DecrementReferenceCount(    unsigned int              BufferIndex );
    CodecStatus_t   CalculateMaximumFrameRate(	CodecBaseDecodeContext_t *DecodeContext );

    CodecStatus_t           VerifyMMECapabilities( unsigned int ActualTransformer );
    CodecStatus_t           VerifyMMECapabilities() { return VerifyMMECapabilities( SelectedTransformer ); }
    CodecStatus_t           GloballyVerifyMMECapabilities( void );
    virtual CodecStatus_t   InitializeMMETransformer(   void );
    virtual CodecStatus_t   TerminateMMETransformer(    void ); // Required by StreamBase audio decoder classes to flush buffer queues
    Buffer_t                TakeMarkerBuffer(           void );


    void   DumpMMECommand(                      MME_Command_t *CmdInfo_p );

public:
    virtual void   CallbackFromMME(             MME_Event_t               Event,
						MME_Command_t            *Command );

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeBase_c(            void );
    ~Codec_MmeBase_c(           void );

    //
    // Overrides for component base class functions
    //

    CodecStatus_t   Halt(       void );

    CodecStatus_t   Reset(      void );

    CodecStatus_t   SetModuleParameters(        unsigned int      ParameterBlockSize,
						void             *ParameterBlock );

    //
    // Codec class functions
    //

    CodecStatus_t   GetTrickModeParameters(     CodecTrickModeParameters_t      *TrickModeParameters );

    CodecStatus_t   RegisterOutputBufferRing(   Ring_t                    Ring );

    CodecStatus_t   OutputPartialDecodeBuffers( void );

    CodecStatus_t   DiscardQueuedDecodes(       void );

    CodecStatus_t   ReleaseReferenceFrame(      unsigned int              ReferenceFrameDecodeIndex );

    CodecStatus_t   CheckReferenceFrameList(    unsigned int              NumberOfReferenceFrameLists,
						ReferenceFrameList_t      ReferenceFrameList[] );

    CodecStatus_t   ReleaseDecodeBuffer(        Buffer_t                  Buffer );

    CodecStatus_t   Input(                      Buffer_t                  CodedBuffer );

    //
    // Extensions to the class to support my inheritors
    //

protected:

    CodecStatus_t   MapBufferToDecodeIndex(             unsigned int              DecodeIndex,
							unsigned int              BufferIndex );

    CodecStatus_t   UnMapBufferIndex(                   unsigned int              BufferIndex );

    CodecStatus_t   TranslateDecodeIndex(               unsigned int              DecodeIndex,
							unsigned int             *BufferIndex );

    CodecStatus_t   GetDecodeBuffer(                    void );

    CodecStatus_t   ReleaseDecodeContext(               CodecBaseDecodeContext_t *Context );

    CodecStatus_t   TranslateReferenceFrameLists(       bool                      IncrementUseCountForReferenceFrame );

    CodecStatus_t   SetOutputOnDecodesComplete(         unsigned int              BufferIndex,
							bool                      TestForImmediateOutput );

    virtual CodecStatus_t   SendMMEStreamParameters(            void ); // DivX needs its own version
    virtual CodecStatus_t   SendMMEDecodeCommand(               void ); // WMA/OGG need to enhance

    //
    // Virtual functions that can be extended by my inheritors
    //

    virtual CodecStatus_t   InitializeDataTypes(        void );
    virtual CodecStatus_t   FillOutSendBufferCommand(   void ); // This function may only implemented for stream base inheritors.
    virtual CodecStatus_t   TestMarkerFramePassOn(      void );
    virtual CodecStatus_t   CheckCodecReturnParameters(       CodecBaseDecodeContext_t *Context ){ return CodecNoError; }

    //
    // Virtual functions that should be implemented by my inheritors
    //

    virtual CodecStatus_t   HandleCapabilities(         void ) = 0;
    virtual CodecStatus_t   FillOutDecodeBufferRequest( BufferStructure_t       *Request ) = 0;
    virtual CodecStatus_t   FillOutTransformerInitializationParameters( void ) = 0;
    virtual CodecStatus_t   FillOutSetStreamParametersCommand( void ) = 0;
    virtual CodecStatus_t   FillOutDecodeCommand(       void ) = 0;
    virtual CodecStatus_t   ValidateDecodeContext( CodecBaseDecodeContext_t *Context ) = 0;

    virtual CodecStatus_t   DumpSetStreamParameters(    void    *Parameters ) = 0;
    virtual CodecStatus_t   DumpDecodeParameters(       void    *Parameters ) = 0;
};
#endif
