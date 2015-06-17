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

#ifndef H_CODEC_MME_VIDEO_HEVC
#define H_CODEC_MME_VIDEO_HEVC

#include "codec_mme_video.h"
#include "codec_mme_video_hevc_utils.h"
#include "hevc.h"
#include "hevcppinline.h"
#include "hevc_hard_host_transformer.h"

// Manages Hades peculiarity: needs a PPB buffer even for non-ref images
#define WA_PPB_OUT_NON_REF

// TODO: Move to common HEVC header file (hevc.h ?)
#define MAX_NUM_SLICES 200
#define MAX_SCALING_SIZEID      4 //!< Maximum number of SizeID signifying different transformation size for scaling.
#define MAX_SCALING_MATRIXID    6 //!< Maximum number of MatrixID for scaling.
#define SLICE_TABLE_ENTRY_SIZE  8
#define MAX_SLICE_HEADER_SIZE   89  // = 2 (slice header area) +  1 (slice_header_next_1) + 17 (slice_ref_pic_set) +  2 (slice_header_next_2) + 4 (ref_pic_list_mod)  + 63 (pred_weight_table)
#define MAX_CTB_TABLE_ENTRIES   1100
#define CTB_TABLE_ENTRY_SIZE    4
#define MAX_CTB_32x32_COMMAND_SIZE 73
#define MAX_PIC_SIZE_IN_CTB_32x32     9600
#define MAX_RESIDUAL_IN_CTB_32x32      400


typedef struct HevcCurrentParameterSet_s
{
    // TODO: Only keep elements that are actually required for decode
    HevcSequenceParameterSet_t SPS;
    HevcPictureParameterSet_t PPS;
} HevcCurrentParameterSet_t;

// /////////////////////////////////////////////////////////////////////////
//
// The C task entry stubs
//

extern "C" {
    OS_TaskEntry(Codec_MmeVideoHevc_IntermediateProcess);
}

class Codec_MmeVideoHevc_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoHevc_c(void);
    CodecStatus_t FinalizeInit(void);
    ~Codec_MmeVideoHevc_c(void);

    CodecStatus_t   Halt(void);

    //
    // Superclass functions
    //

    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   OutputPartialDecodeBuffers(void);
    CodecStatus_t   DiscardQueuedDecodes(void);
    CodecStatus_t   ReleaseReferenceFrame(unsigned int              ReferenceFrameDecodeIndex);
    CodecStatus_t   CheckReferenceFrameList(unsigned int              NumberOfReferenceFrameLists,
                                            ReferenceFrameList_t      ReferenceFrameList[]);
    CodecStatus_t   Input(Buffer_t                  CodedBuffer);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);

    void IntermediateProcess(void);

private:
    Hevc_InitTransformerParam_fmw_t      InitTransformerParam;

// TODO: Structures are still commented out in hevc_video_transformer_types.h. Remove comment
// and update FillOutTransformerInitializationParameters() accordingly if this ever changes
// in the future.
//    hevcdecpix_transformer_capability_t           HevcTransformCapability;
//    hevcdecpix_init_transformer_param_t           HevcInitializationParameters;

    bool                                  DiscardFramesInPreprocessorChain;
    ppFramesRing_c                       *mPpFramesRing;

    BufferType_t                          mPreProcessorBufferType[HEVC_NUM_INTERMEDIATE_BUFFERS];
    allocator_device_t                    mPreProcessorBufferAllocator[HEVC_NUM_INTERMEDIATE_BUFFERS];
    unsigned int                          mPreprocessorBufferSize[HEVC_NUM_INTERMEDIATE_BUFFERS]; // maximum size of IB according to stream level
    BufferPool_t                          mPreProcessorBufferPool[HEVC_NUM_INTERMEDIATE_BUFFERS];
    HevcppDevice_t                        *mPreProcessorDevice;

    unsigned int                          mSliceTableEntries;
    uint32_t                              mLastLevelIdc; // used to cache PreprocessorBufferSize[] when level does not change between pictures

#ifdef WA_PPB_OUT_NON_REF
    allocator_device_t                    mFakePPBAllocator;
    unsigned char                        *mFakePPBAddress;
#endif

// TODO: Uncomment member attributes as they become necessary
    //bool                                  ReferenceFrameSlotUsed[HEVC_MAX_REFERENCE_FRAMES];    // A usage array for reference frame slots in the transform data
//    H264_HostData_t                       RecordedHostData[MAX_DECODE_BUFFERS];                 // A record of hostdata for each reference frame
    //unsigned int                          OutstandingSlotAllocationRequest;
//
//    unsigned int                          NumberOfUsedDescriptors;                              // Map of used descriptors when constructing a reference list
//    unsigned char                         DescriptorIndices[3 * H264_MAX_REFERENCE_FRAMES];
//
    ReferenceFrameList_t                  LocalReferenceFrameList[HEVC_NUM_REF_FRAME_LISTS];    // A reference frame list used for local processing, that the 2.4 compiler
    // discovered is a little large for the stack frame.
//
//    bool                                  RasterOutput;

    // Functions

    // Internal process functions called via C

// TODO: Uncomment as implementation progresses
//    CodecStatus_t   FillOutDecodeCommandHostData(       void );
//    CodecStatus_t   FillOutDecodeCommandRefPicList(     void );
//    unsigned int    FillOutNewDescriptor(               unsigned int             ReferenceId,
//                            unsigned int             BufferIndex,
//                            ReferenceDetails_t  *Details );
//    CodecStatus_t   HevcReleaseReferenceFrame(      unsigned int         ReferenceFrameDecodeIndex );
//
//    CodecStatus_t   ReferenceFrameSlotAllocate(     unsigned int         BufferIndex );

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   AllocatePreProcBufferPool(HevcFrameParameters_t *FrameParameters);
    void            DeallocatePreProcBufferPool(void);
    int             UpdatePreprocessorBufferSize(HevcFrameParameters_t *FrameParameters);
    CodecStatus_t   GetIntermediateBufferSize(int level_index,  uint32_t pic_width_in_luma_samples, uint32_t pic_height_in_luma_samples, unsigned int *IBSize);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   DumpSetStreamParameters(void    *Parameters);
    CodecStatus_t   DumpDecodeParameters(void *Parameters);
    CodecStatus_t   DumpDecodeParameters(void *Parameters, unsigned int DecodeFrameIndex);

    CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request);

    void            FillOutPreprocessorCommand(hevcdecpreproc_transform_param_t     *cmd,
                                               HevcSequenceParameterSet_t *sps, HevcPictureParameterSet_t *pps, uint32_t num_poc_total_curr, uint32_t DecodeFrameIndex);

    static uint32_t scaling_list_command_init(uint32_t *pt, uint8_t use_default, scaling_list_t *scaling_list);

    void TrimCodedBuffer(uint8_t *buffer, unsigned int DataOffset, unsigned int *CodedSlicesSize);

    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideoHevc_c);
};

#endif // H_CODEC_MME_VIDEO_HEVC
