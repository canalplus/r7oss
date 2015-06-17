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

#ifndef H_CODEC_MME_VIDEO_H264
#define H_CODEC_MME_VIDEO_H264

#include "codec_mme_video.h"
#include "h264ppinline.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoH264_c"

typedef enum
{
    ActionNull                                  = 1,
    ActionCallOutputPartialDecodeBuffers,
    ActionCallDiscardQueuedDecodes,
    ActionCallReleaseReferenceFrame,
    ActionPassOnFrame,
    ActionPassOnPreProcessedFrame,
} FramesInPreprocessorChainAction_t;

typedef struct FramesInPreprocessorChain_s
{
    bool                                 Used;
    FramesInPreprocessorChainAction_t    Action;
    bool                                 AbortedFrame;
    Buffer_t                             CodedBuffer;
    Buffer_t                             PreProcessorBuffer;
    ParsedFrameParameters_t             *ParsedFrameParameters;
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

class Codec_MmeVideoH264_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoH264_c(void);
    CodecStatus_t FinalizeInit(void);
    ~Codec_MmeVideoH264_c(void);

    CodecStatus_t   Halt(void);

    //
    // Superclass functions
    //

    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   OutputPartialDecodeBuffers(void);
    CodecStatus_t   DiscardQueuedDecodes(void);
    CodecStatus_t   ReleaseReferenceFrame(unsigned int ReferenceFrameDecodeIndex);
    CodecStatus_t   CheckReferenceFrameList(unsigned int NumberOfReferenceFrameLists,
                                            ReferenceFrameList_t ReferenceFrameList[]);
    CodecStatus_t   Input(Buffer_t CodedBuffer);

    void IntermediateProcess(void);

protected:
    H264_TransformerCapability_fmw_t      H264TransformCapability;
    H264_InitTransformerParam_fmw_t       H264InitializationParameters;

    bool                                  Terminating;
    OS_Event_t                            StartStopEvent;

    unsigned int                          SD_MaxMBStructureSize;            // Data items relating to macroblock structure buffers
    unsigned int                          HD_MaxMBStructureSize;

    bool                                  DiscardFramesInPreprocessorChain;
    FramesInPreprocessorChain_t           FramesInPreprocessorChain[H264_CODED_FRAME_COUNT];
    OS_Mutex_t                            H264Lock;

    Ring_t                                FramesInPreprocessorChainRing;
    BufferType_t                          mPreProcessorBufferType;
    BufferType_t                          mPreProcessorBufferMaxSize;
    BufferDataDescriptor_t                mH264PreProcessorBufferDescriptor;
    BufferPool_t                          PreProcessorBufferPool;
    h264pp_device_t                       PreProcessorDevice;

    bool                                  ReferenceFrameSlotUsed[H264_MAX_REFERENCE_FRAMES];    // A usage array for reference frame slots in the transform data
    H264_HostData_t                       RecordedHostData[MAX_DECODE_BUFFERS];                 // A record of hostdata for each reference frame
    unsigned int                          OutstandingSlotAllocationRequest;

    unsigned int                          NumberOfUsedDescriptors;                              // Map of used descriptors when constructing a reference list
    unsigned char                         DescriptorIndices[3 * H264_MAX_REFERENCE_FRAMES];

    ReferenceFrameList_t                  LocalReferenceFrameList[H264_NUM_REF_FRAME_LISTS];    // A reference frame list used for local processing, that the 2.4 compiler
    // discovered is a little large for the stack frame.

    bool                                  RasterOutput;

    // Internal process functions called via C

    CodecStatus_t   FillOutDecodeCommandHostData(H264SliceHeader_t     *SliceHeader);
    CodecStatus_t   FillOutDecodeCommandRefPicList(H264SliceHeader_t   *SliceHeader);
    CodecStatus_t   PrepareDecodeCommand(H264SliceHeader_t  *SliceHeader,
                                         Buffer_t            PreProcessorBuffer,
                                         bool                ReferenceFrame);
    unsigned int    FillOutNewDescriptor(unsigned int        ReferenceId,
                                         unsigned int        BufferIndex,
                                         ReferenceDetails_t *Details);
    virtual CodecStatus_t   H264ReleaseReferenceFrame(unsigned int       ReferenceFrameDecodeIndex);

    CodecStatus_t   ReferenceFrameSlotAllocate(unsigned int BufferIndex);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t       PrepareStreamParametersCommand(void    *Context,
                                                       H264SequenceParameterSetHeader_t   *SPS,
                                                       H264PictureParameterSetHeader_t    *PPS);
    virtual CodecStatus_t   FillOutSetStreamParametersCommand(void);
    virtual CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void *Parameters);
    CodecStatus_t   DumpDecodeParameters(void    *Parameters);

    CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request);

    virtual void    UnuseCodedBuffer(Buffer_t CodedBuffer, FramesInPreprocessorChain_t *PPentry);

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideoH264_c);
};

#endif
