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

Source file name : codec_mme_video_h264.h
Author :           Nick

Definition of the stream specific codec implementation for H264 video in player 2


Date        Modification                                    Name
----        ------------                                    --------
25-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_CODEC_MME_VIDEO_H264
#define H_CODEC_MME_VIDEO_H264

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video.h"
#include "h264ppinline.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define H264_DEFAULT_MACROBLOCK_STRUCTURE_BUFFER_SIZE   (64 * (1920/16) * (1088/16))

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef enum
{
    ActionNull                                  = 1,
    ActionCallOutputPartialDecodeBuffers,
    ActionCallDiscardQueuedDecodes,
    ActionCallReleaseReferenceFrame,
    ActionPassOnFrame,
    ActionPassOnPreProcessedFrame,
} FramesInPreprocessorChainAction_t;

//

typedef struct FramesInPreprocessorChain_s
{
    bool                                 Used;
    FramesInPreprocessorChainAction_t    Action;
    bool                                 AbortedFrame;
    Buffer_t                             CodedBuffer;
    Buffer_t                             PreProcessorBuffer;
    ParsedFrameParameters_t		*ParsedFrameParameters;
    unsigned int                         DecodeFrameIndex;
    void                                *InputBufferCachedAddress;
    void                                *InputBufferPhysicalAddress;
    void                                *OutputBufferCachedAddress;
    void                                *OutputBufferPhysicalAddress;
} FramesInPreprocessorChain_t;


// /////////////////////////////////////////////////////////////////////////
//
// The C task entry stubs
//

extern "C" {
OS_TaskEntry(Codec_MmeVideoH264_IntermediateProcess);
}

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Codec_MmeVideoH264_c : public Codec_MmeVideo_c
{
protected:

    // Data

    H264_TransformerCapability_fmw_t      H264TransformCapability;
    H264_InitTransformerParam_fmw_t       H264InitializationParameters;

    bool                                  Terminating;
    unsigned int                          ProcessRunningCount;
    OS_Event_t                            StartStopEvent;

    unsigned int                          SD_MaxMBStructureSize;	        // Data items relating to macroblock structure buffers
    unsigned int			  HD_MaxMBStructureSize;
    allocator_device_t			  MacroBlockMemoryDevice;
    void				 *MacroBlockMemory[3];
    BufferType_t                          MacroBlockStructureType;
    BufferPool_t                          MacroBlockStructurePool;

    bool                                  DiscardFramesInPreprocessorChain;
    FramesInPreprocessorChain_t           FramesInPreprocessorChain[H264_CODED_FRAME_COUNT];
    OS_Mutex_t                            H264Lock;
    Ring_t                                FramesInPreprocessorChainRing;
    BufferType_t                          PreProcessorBufferType;
    BufferPool_t                          PreProcessorBufferPool;
    h264pp_device_t                       PreProcessorDevice;

    bool				  ReferenceFrameSlotUsed[H264_MAX_REFERENCE_FRAMES];	// A usage array for reference frame slots in the transform data
    H264_HostData_t                       RecordedHostData[CODEC_MAX_DECODE_BUFFERS];           // A record of hostdata for each reference frame
    unsigned int			  OutstandingSlotAllocationRequest;

    unsigned int                          NumberOfUsedDescriptors;                              // Map of used descriptors when constructing a reference list 
    unsigned char                         DescriptorIndices[3 * H264_MAX_REFERENCE_FRAMES];

    // Functions

    // Internal process functions called via C

    CodecStatus_t   FillOutDecodeCommandHostData(       void );
    CodecStatus_t   FillOutDecodeCommandRefPicList(     void );
    unsigned int    FillOutNewDescriptor(               unsigned int             ReferenceId,
							unsigned int             BufferIndex,
							H264ReferenceDetails_t  *Details );
    CodecStatus_t   H264ReleaseReferenceFrame(		unsigned int		 ReferenceFrameDecodeIndex );

    CodecStatus_t   ReferenceFrameSlotAllocate(		unsigned int		 BufferIndex );

public:

    void IntermediateProcess( void );

public:

    //
    // Constructor/Destructor methods
    //

    Codec_MmeVideoH264_c(               void );
    ~Codec_MmeVideoH264_c(              void );

    //
    // Overrides for component base class functions
    //

    CodecStatus_t   Halt(               void );
    CodecStatus_t   Reset(              void );

    //
    // Superclass functions
    //

    CodecStatus_t   RegisterOutputBufferRing(           Ring_t                    Ring );
    CodecStatus_t   OutputPartialDecodeBuffers(         void );
    CodecStatus_t   DiscardQueuedDecodes(               void );
    CodecStatus_t   ReleaseReferenceFrame(              unsigned int              ReferenceFrameDecodeIndex );
    CodecStatus_t   CheckReferenceFrameList(            unsigned int              NumberOfReferenceFrameLists,
							ReferenceFrameList_t      ReferenceFrameList[] );
    CodecStatus_t   Input(                              Buffer_t                  CodedBuffer );

    //
    // Stream specific functions
    //

protected:

    CodecStatus_t   HandleCapabilities( void );

    CodecStatus_t   FillOutTransformerInitializationParameters( void );
    CodecStatus_t   FillOutSetStreamParametersCommand(          void );
    CodecStatus_t   FillOutDecodeCommand(                       void );
    CodecStatus_t   ValidateDecodeContext(                      CodecBaseDecodeContext_t *Context );
    CodecStatus_t   DumpSetStreamParameters(                    void    *Parameters );
    CodecStatus_t   DumpDecodeParameters(                       void    *Parameters );
};
#endif
