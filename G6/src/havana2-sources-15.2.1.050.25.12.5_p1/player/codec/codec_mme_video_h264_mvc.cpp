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

#include "codec_mme_video_h264_mvc.h"
#include "h264.h"
#include "mvc.h"
#include "ring_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoH264_MVC_c"

typedef struct MVCCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
    H264_SetGlobalParam_t               StreamParameters;
} MVCCodecStreamParameterContext_t;

#define BUFFER_MVC_CODEC_STREAM_PARAMETER_CONTEXT             "MVCCodecStreamParameterContext"
#define BUFFER_MVC_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_MVC_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MVCCodecStreamParameterContext_t)}

static BufferDataDescriptor_t  MVCCodecStreamParameterContextDescriptor = BUFFER_MVC_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

typedef struct MVCCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    H264_TransformParam_fmw_t           DecodeParameters;
    H264_CommandStatus_fmw_t            DecodeStatus;

    bool                isBaseView;
} MVCCodecDecodeContext_t;

#define BUFFER_MVC_CODEC_DECODE_CONTEXT       "MVCCodecDecodeContext"
#define BUFFER_MVC_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_MVC_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MVCCodecDecodeContext_t)}

static BufferDataDescriptor_t            MVCCodecDecodeContextDescriptor = BUFFER_MVC_CODEC_DECODE_CONTEXT_TYPE;

// Display base view (0), dependent view (1) or both (2)
static unsigned int MVC_stereo = 1;

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor function, fills in the codec specific parameter values
//

Codec_MmeVideoH264_MVC_c::Codec_MmeVideoH264_MVC_c(void)
    : Codec_MmeVideoH264_c()
    , isBaseView(false)
    , IndexDepBufferMapSize(0)
    , IndexDepBufferMap(NULL)
    , LastDecodeFrameIndex(INVALID_INDEX)
    , LastBaseDecodeBufferIndex(INVALID_INDEX)
    , LastDepDecodeBufferIndex(INVALID_INDEX)
{
    Configuration.CodecName                             = "MVC video";
    Configuration.StreamParameterContextCount           = 8;                    // twice H264 for MVC
    Configuration.StreamParameterContextDescriptor      = &MVCCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 8;                    // twice H264 for MVC
    Configuration.DecodeContextDescriptor               = &MVCCodecDecodeContextDescriptor;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset
//      are executed for all levels of the class.
//

Codec_MmeVideoH264_MVC_c::~Codec_MmeVideoH264_MVC_c(void)
{
    Halt();

    // Free the indexing map
    OS_LockMutex(&Lock);

    if (IndexDepBufferMap != NULL)
    {
        delete[] IndexDepBufferMap;
    }

    OS_UnLockMutex(&Lock);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Halt function
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::Halt(void)
{
    return Codec_MmeVideoH264_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port, we take this opportunity to
//      create the index map for the dependent view decode buffers
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::Connect(Port_c *Port)
{
    CodecStatus_t Status = Codec_MmeVideoH264_c::Connect(Port);
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // Create the mapping between decode indices and decode buffers
    //
    OS_LockMutex(&Lock);
    IndexDepBufferMapSize  = IndexBufferMapSize;
    IndexDepBufferMap      = new CodecIndexBufferMap_t[IndexDepBufferMapSize];

    if (IndexDepBufferMap == NULL)
    {
        SE_ERROR("(%s) - Failed to allocate DecodeIndex <=> Dependent Buffer map\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        SetComponentState(ComponentInError);
        return PlayerInsufficientMemory;
    }

    memset(IndexDepBufferMap, 0xff, IndexDepBufferMapSize * sizeof(CodecIndexBufferMap_t));
    OS_UnLockMutex(&Lock);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an MVC mme transformer.
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::FillOutSetStreamParametersCommand(void)
{
    MVCStreamParameters_t                   *Parsed         = (MVCStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    MVCCodecStreamParameterContext_t        *Context        = (MVCCodecStreamParameterContext_t *)StreamParameterContext;
    H264PictureParameterSetHeader_t         *PPS;
    H264SequenceParameterSetHeader_t        *SPS;
    MVCSubSequenceParameterSetHeader_t      *SUB_SPS;
    CodecStatus_t                            Status;
    // Prepare stream parameters for the current view
    SPS         = Parsed->SequenceParameterSet;
    SUB_SPS     = Parsed->SubsetSequenceParameterSet;

    if (isBaseView)
    {
        PPS    = Parsed->PictureParameterSet;
        Status = PrepareStreamParametersCommand((void *)Context, SPS, PPS);
    }
    else
    {
        PPS    = Parsed->DepPictureParameterSet;
        Status = PrepareStreamParametersCommand((void *)Context, &(SUB_SPS->h264_header), PPS);
    }

    if (Status != CodecNoError)
    {
        return Status;
    }

//
    /* Default values programmed to decode H264 or MVC stream using the ASMVC firmware */
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.adaptive_tcoeff_level_prediction_flag              = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.extended_spatial_scalability = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.inter_layer_deblocking_filter_control_present_flag = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.slice_header_restriction_flag                      = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.chroma_phase_x_plus1_flag                          = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.chroma_phase_y_plus1                               = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_ref_layer_chroma_phase_x_plus1_flag            = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_ref_layer_chroma_phase_y_plus1                 = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_scaled_ref_layer_right_offset                  = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_scaled_ref_layer_top_offset                    = 0;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_scaled_ref_layer_bottom_offset                 = 0;

    // For Stereo MVC, the interview reference list contains only 1 element: the base view
    // Fill out the interview ref arrays accordingly.
    if (!isBaseView)
    {
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l0 = 1;
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l1 = 1;
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.anchor_non_anchor_ref_l0[0]   = SUB_SPS->view_id[0];
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.anchor_non_anchor_ref_l1[0]   = SUB_SPS->view_id[0];
    }
    else
    {
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l0 = 0;
        Context->StreamParameters.MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l1 = 0;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to translate the dep view decode frame indexes
//
//      This function must be mutex-locked by caller
//
CodecStatus_t Codec_MmeVideoH264_MVC_c::TranslateReferenceFrameLists(bool IncrementUseCountForReferenceFrame)
{
    CodecStatus_t Status;
    MVCFrameParameters_t *Parsed = (MVCFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    OS_AssertMutexHeld(&Lock);
    // Dump DPB
#ifdef DUMP_DPB
    unsigned int i, j;

    if (0) // if (ParsedFrameParameters->DecodeFrameIndex < 20 || ParsedFrameParameters->DecodeFrameIndex > 160)
    {
        unsigned int count = 0;

        if (isBaseView)
        {
            SE_INFO(group_decoder_video, "DFI %d\n", ParsedFrameParameters->DecodeFrameIndex);
        }

        DecodeBufferManager->GetEstimatedBufferCount(&count);
        SE_VERBOSE(group_decoder_video, " Decode Buffer Pool for decoding DFI %d (estimated free %d x 3 raster)\n", ParsedFrameParameters->DecodeFrameIndex, count);

        for (i = 0; i < DecodeBufferCount; i++)
            if ((BufferState[i].ReferenceFrameCount != 0) && (BufferState[i].ReferenceFrameSlot != INVALID_INDEX))
            {
                SE_VERBOSE(group_decoder_video, "Buffer index %d, slot %d, buffer %p\n", i, BufferState[i].ReferenceFrameSlot, BufferState[i].Buffer);
            }

        if (isBaseView)
        {
            SE_VERBOSE(group_decoder_video, " DPB base for DFI %d:\n", ParsedFrameParameters->DecodeFrameIndex);

            for (i = 0; i < IndexBufferMapSize; i++)
                if (IndexBufferMap[i].DecodeIndex != INVALID_INDEX)
                {
                    BufferState[IndexBufferMap[i].BufferIndex].Buffer->GetOwnerCount(&count);
                    SE_VERBOSE(group_decoder_video, "index %d: DFI %d, DBI %d, buffer %p, count %d\n",
                               i, IndexBufferMap[i].DecodeIndex, IndexBufferMap[i].BufferIndex,
                               BufferState[IndexBufferMap[i].BufferIndex].Buffer, count);
                }
        }
        else
        {
            SE_VERBOSE(group_decoder_video, " DPB dep for DFI %d:\n", ParsedFrameParameters->DecodeFrameIndex);

            for (i = 0; i < IndexDepBufferMapSize; i++)
                if (IndexDepBufferMap[i].DecodeIndex != INVALID_INDEX)
                {
                    BufferState[IndexDepBufferMap[i].BufferIndex].Buffer->GetOwnerCount(&count);
                    SE_VERBOSE(group_decoder_video, "index %d: DFI %d, DBI %d, buffer %p, count %d\n",
                               i, IndexDepBufferMap[i].DecodeIndex, IndexDepBufferMap[i].BufferIndex,
                               BufferState[IndexDepBufferMap[i].BufferIndex].Buffer, count);
                }
        }
    }

#endif

    if (isBaseView)
    {
        Status = TranslateSetOfReferenceFrameLists(IncrementUseCountForReferenceFrame,
                                                   ParsedFrameParameters->ReferenceFrame,
                                                   ParsedFrameParameters->ReferenceFrameList);
    }
    else
    {
        Status = TranslateSetOfReferenceFrameLists(IncrementUseCountForReferenceFrame,
                                                   Parsed->DepSliceHeader.nal_ref_idc != 0,
                                                   Parsed->ReferenceDepFrameList);
    }

#ifdef DUMP_DPB

    // Debug ref pic list
    if (0) // if (!isBaseView && (ParsedFrameParameters->DecodeFrameIndex < 20 || ParsedFrameParameters->DecodeFrameIndex > 160))
    {
        for (i = 0; i < ParsedFrameParameters->NumberOfReferenceFrameLists; i++)
        {
            SE_VERBOSE(group_decoder_video, " Dep ref pic list %d for DFI %d, size %d\n", i, ParsedFrameParameters->DecodeFrameIndex, Parsed->ReferenceDepFrameList[i].EntryCount);

            for (j = 0; j < Parsed->ReferenceDepFrameList[i].EntryCount; j++)
                SE_VERBOSE(group_decoder_video, "DFI %d, poc %d, pn %d, buffer index %d => %p\n",
                           Parsed->ReferenceDepFrameList[i].EntryIndicies[j],
                           DecodeContext->ReferenceFrameList[i].H264ReferenceDetails[j].PicOrderCnt,
                           DecodeContext->ReferenceFrameList[i].H264ReferenceDetails[j].PictureNumber,
                           DecodeContext->ReferenceFrameList[i].EntryIndicies[j],
                           BufferState[DecodeContext->ReferenceFrameList[i].EntryIndicies[j]].Buffer);
        }
    }

#endif
    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an H264 mme transformer.
//

//  TBD: add interview only ref to list of references
CodecStatus_t   Codec_MmeVideoH264_MVC_c::FillOutDecodeCommand(void)
{
    CodecStatus_t                Status;
    MVCCodecDecodeContext_t     *Context    = (MVCCodecDecodeContext_t *)DecodeContext;
    MVCFrameParameters_t        *Parsed     = (MVCFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    H264_TransformParam_fmw_t   *Param;
    H264_HostData_t             *HostData;
    MVCStreamParameters_t       *StreamParams   = (MVCStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    Buffer_t                     PreProcessorBuffer;
    // Used in ReleaseDecodeContext()
    Context->isBaseView = isBaseView;

    // Transfer PP buffer to decode context
    if (isBaseView)
    {
        PreProcessorBuffer = Parsed->BasePreProcessorBuffer;
    }
    else
    {
        PreProcessorBuffer = Parsed->DepPreProcessorBuffer;
    }

    DecodeContextBuffer->AttachBuffer(PreProcessorBuffer);      // Must be ordered, otherwise the pre-processor
    CodedFrameBuffer->DetachBuffer(PreProcessorBuffer);         // buffer will get released in the middle

    // Now fill out the command
    if (isBaseView)
    {
        Status = PrepareDecodeCommand(&Parsed->SliceHeader, PreProcessorBuffer, ParsedFrameParameters->ReferenceFrame);
    }
    else
    {
        Status = PrepareDecodeCommand(&Parsed->DepSliceHeader, PreProcessorBuffer, Parsed->DepSliceHeader.nal_ref_idc != 0);
    }

    // Complete the command with MVC-specific parameters
    Param       = &Context->DecodeParameters;
    Param->anchor_pic_flag = (unsigned int)Parsed->isAnchor;
    Param->inter_view_flag = (unsigned int)Parsed->isInterview;
    Param->base_view_flag  = isBaseView;
    // MVC Host Data
    HostData = &Context->DecodeParameters.HostData;

    if (isBaseView)
    {
        HostData->CurrViewId = StreamParams->SubsetSequenceParameterSet->view_id[0];
    }
    else
    {
        HostData->CurrViewId = StreamParams->SubsetSequenceParameterSet->view_id[1];
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::DumpSetStreamParameters(void    *Parameters)
{
    unsigned int             i;
    H264_SetGlobalParam_t   *StreamParams;
    Codec_MmeVideoH264_c::DumpSetStreamParameters(Parameters);
    StreamParams    = (H264_SetGlobalParam_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "AZA - MvcSpecificSetGlobalParamSPS\n");
    SE_VERBOSE(group_decoder_video, "AZA - num_anchor_non_anchor_refs_l0 %d\n", StreamParams->MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l0);

    for (i = 0; i < StreamParams->MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l0; i++)
    {
        SE_VERBOSE(group_decoder_video, "AZA - num_anchor_non_anchor_refs_l0[%d] %d\n", i, StreamParams->MvcSpecificSetGlobalParamSPS.anchor_non_anchor_ref_l0[i]);
    }

    SE_VERBOSE(group_decoder_video, "AZA - num_anchor_non_anchor_refs_l1 %d\n", StreamParams->MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l1);

    for (i = 0; i < StreamParams->MvcSpecificSetGlobalParamSPS.num_anchor_non_anchor_refs_l1; i++)
    {
        SE_VERBOSE(group_decoder_video, "AZA - num_anchor_non_anchor_refs_l1[%d] %d\n", i, StreamParams->MvcSpecificSetGlobalParamSPS.anchor_non_anchor_ref_l1[i]);
    }

//
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::DumpDecodeParameters(void    *Parameters)
{
    H264_TransformParam_fmw_t        *FrameParams;
//
    Codec_MmeVideoH264_c::DumpDecodeParameters(Parameters);
    FrameParams = (H264_TransformParam_fmw_t *)Parameters;
    // H264_HostData_t *HostData    = &FrameParams->HostData;
    // H264_RefPictListAddress_t *RefPicLists = &FrameParams->InitialRefPictList;
    SE_VERBOSE(group_decoder_video, "AZA - MVC FRAME PARAMS\n");
    SE_VERBOSE(group_decoder_video, "AZA - anchor_pic_flag %d\n", FrameParams->anchor_pic_flag);
    SE_VERBOSE(group_decoder_video, "AZA - inter_view_flag %d\n", FrameParams->inter_view_flag);
    SE_VERBOSE(group_decoder_video, "AZA - base_view_flag %d\n", FrameParams->base_view_flag);
//
    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoH264_MVC_c::Input(Buffer_t CodedBuffer)
{
    unsigned int              i;
    h264pp_status_t           PPStatus;
    unsigned int              CodedDataLength;
    ParsedFrameParameters_t  *ParsedFrameParameters;
    ParsedVideoParameters_t  *ParsedVideoParameters;
    Buffer_t                  PreProcessorBuffer;
    MVCFrameParameters_t     *FrameParameters;

    // increment usage count of Coded buffer so that it's not released after the preprocessing of the base view
    CodedBuffer->IncrementReferenceCount(IdentifierH264Intermediate);
    // call ancestor to start preprocessing the base view frame

    PlayerStatus_t Status = Codec_MmeVideoH264_c::Input(CodedBuffer);
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // First extract the useful pointers from the buffer all held locally
    //
    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
    SE_ASSERT(ParsedVideoParameters != NULL);

    if (!ParsedFrameParameters->NewFrameParameters ||
        (ParsedFrameParameters->DecodeFrameIndex == INVALID_INDEX))
    {
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        return CodecNoError; // no frame inside
    }

    // Start MVC-specific part
    OS_LockMutex(&H264Lock);
    FrameParameters   = (MVCFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    // Store base PP buffer

    CodedBuffer->ObtainAttachedBufferReference(mPreProcessorBufferType, &PreProcessorBuffer);
    SE_ASSERT(PreProcessorBuffer != NULL);

    FrameParameters->BasePreProcessorBuffer = PreProcessorBuffer;
    //
    // Get a pre-processor buffer and attach it to the coded frame.
    // Note since it is attached to the coded frame, we can release our
    // personal hold on it.
    //
    BufferStatus_t BufferStatus = PreProcessorBufferPool->GetBuffer(&PreProcessorBuffer);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a pre-processor buffer (%08x)\n", BufferStatus);
        OS_UnLockMutex(&H264Lock);
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        return CodecError;
    }

    CodedBuffer->AttachBuffer(PreProcessorBuffer);
    PreProcessorBuffer->DecrementReferenceCount();
    // Store the buffer into the frame parameters, so that the MVC intermediate process
    // knows which view it is processing
    FrameParameters->DepPreProcessorBuffer = PreProcessorBuffer;

    //
    // Now get an entry into the frames list, and fill in the relevant fields
    //

    for (i = 0; i < H264_CODED_FRAME_COUNT; i++)
        if (!FramesInPreprocessorChain[i].Used)
        {
            break;
        }

    if (i == H264_CODED_FRAME_COUNT)
    {
        SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list - Implementation error\n");
        OS_UnLockMutex(&H264Lock);
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        return PlayerImplementationError;
    }

    memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
    FramesInPreprocessorChain[i].Used                                   = true;
    FramesInPreprocessorChain[i].Action                                 = ActionPassOnPreProcessedFrame;
    FramesInPreprocessorChain[i].CodedBuffer                            = CodedBuffer;
    FramesInPreprocessorChain[i].PreProcessorBuffer                     = PreProcessorBuffer;
    FramesInPreprocessorChain[i].ParsedFrameParameters                  = ParsedFrameParameters;
    FramesInPreprocessorChain[i].DecodeFrameIndex                       = ParsedFrameParameters->DecodeFrameIndex;
    CodedBuffer->ObtainDataReference(NULL, &CodedDataLength, (void **)(&FramesInPreprocessorChain[i].InputBufferCachedAddress), CachedAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].InputBufferCachedAddress != NULL); // not expected to be empty ? TBC
    CodedBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].InputBufferPhysicalAddress), PhysicalAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].InputBufferPhysicalAddress != NULL); // not expected to be empty ? TBC

    /* it could happen in corrupted stream */
    if (CodedDataLength < FrameParameters->DepDataOffset + FrameParameters->DepSlicesLength)
    {
        SE_ERROR("error DepDataOffset=%u + DepSlicesLength=%u > CodedDataLength=%u\n",
                 FrameParameters->DepDataOffset, FrameParameters->DepSlicesLength, CodedDataLength);
        OS_UnLockMutex(&H264Lock);
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        return CodecError;
    }

    FramesInPreprocessorChain[i].InputBufferCachedAddress    = (void *)((unsigned int)FramesInPreprocessorChain[i].InputBufferCachedAddress + FrameParameters->DepDataOffset);
    FramesInPreprocessorChain[i].InputBufferPhysicalAddress  = (void *)((unsigned int)FramesInPreprocessorChain[i].InputBufferPhysicalAddress + FrameParameters->DepDataOffset);
    PreProcessorBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].OutputBufferCachedAddress), CachedAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].OutputBufferCachedAddress != NULL); // not expected to be empty ? TBC
    PreProcessorBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].OutputBufferPhysicalAddress), PhysicalAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].OutputBufferPhysicalAddress != NULL); // not expected to be empty ? TBC
    //
    // Process the buffer
    //
    PPStatus    = H264ppProcessBuffer(PreProcessorDevice,
                                      &FrameParameters->DepSliceHeader,
                                      i,
                                      mPreProcessorBufferMaxSize,
                                      FrameParameters->DepSlicesLength,
                                      FramesInPreprocessorChain[i].InputBufferCachedAddress,
                                      FramesInPreprocessorChain[i].InputBufferPhysicalAddress,
                                      FramesInPreprocessorChain[i].OutputBufferCachedAddress,
                                      FramesInPreprocessorChain[i].OutputBufferPhysicalAddress);

    if (PPStatus != h264pp_ok)
    {
        SE_ERROR("Failed to process a buffer, Junking frame\n");
        FramesInPreprocessorChain[i].Used               = false;
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        OS_UnLockMutex(&H264Lock);
        return CodecError;
    }

    FramesInPreprocessorChainRing->Insert(i);
    OS_UnLockMutex(&H264Lock);

    return CodecNoError;
}

// Methode called in the PP callback, before call to Codec_MmeVideo_c::Input()
void Codec_MmeVideoH264_MVC_c::UnuseCodedBuffer(Buffer_t CodedBuffer, FramesInPreprocessorChain_t *PPentry)
{
    MVCFrameParameters_t *mvcFP;
    // find whether it's the base view or dep view
    mvcFP = (MVCFrameParameters_t *)(PPentry->ParsedFrameParameters->FrameParameterStructure);

    if (PPentry->PreProcessorBuffer == mvcFP->BasePreProcessorBuffer)
    {
        isBaseView = true;
    }
    else if (PPentry->PreProcessorBuffer == mvcFP->DepPreProcessorBuffer)
    {
        isBaseView = false;
    }
    else
    {
        SE_ERROR("unexpected PP buffer %p\n", PPentry->PreProcessorBuffer);
        // error recovery TBD
        isBaseView = true;
    }

    if (!isBaseView)
    {
        CodedBuffer->SetUsedDataSize(0);
        BufferStatus_t Status  = CodedBuffer->ShrinkBuffer(0);
        if (Status != BufferNoError)
        {
            SE_INFO(group_decoder_video, "Failed to shrink buffer\n");
        }
    }
}


// Patching Context->BufferIndex in order to release one the base/dependent decode buffer
// TBD: manage fields
// TBD: push stereo frame to manifestor

CodecStatus_t  Codec_MmeVideoH264_MVC_c::ReleaseDecodeContext(CodecBaseDecodeContext_t *Context)
{
    MVCCodecDecodeContext_t *MVCDecodeContext = (MVCCodecDecodeContext_t *) Context;
    Buffer_t                 AttachedCodedDataBuffer;
    ParsedFrameParameters_t *AttachedParsedFrameParameters;
    unsigned int             CurrentDecodeFrameIndex;
    unsigned int             stereo = MVC_stereo; // for debug, until stereo is managed in manifestor

    OS_LockMutex(&Lock);

    // Retrieve DecodeFrameIndex
    BufferState[Context->BufferIndex].Buffer->ObtainAttachedBufferReference(CodedFrameBufferType, &AttachedCodedDataBuffer);
    SE_ASSERT(AttachedCodedDataBuffer != NULL);

    AttachedCodedDataBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&AttachedParsedFrameParameters));
    SE_ASSERT(AttachedParsedFrameParameters != NULL);

    CurrentDecodeFrameIndex = AttachedParsedFrameParameters->DecodeFrameIndex;

    if (MVCDecodeContext->isBaseView)
    {
        // A little error detection (should be error recovery ! TBD)
        if (LastDecodeFrameIndex != INVALID_INDEX || LastBaseDecodeBufferIndex != INVALID_INDEX || LastDepDecodeBufferIndex != INVALID_INDEX)
        {
            SE_ERROR("base view decoded while other decode in progress\n");
        }

        // a new MVC decode has been detected
        LastDecodeFrameIndex = CurrentDecodeFrameIndex;
        LastBaseDecodeBufferIndex = Context->BufferIndex;

        if (stereo == 1) // keep only dep
        {
            BufferState[Context->BufferIndex].OutputOnDecodesComplete = false;    // do not push base to manifestor
        }
    }
    else
    {
        // A little error detection (should be error recovery ! TBD)
        if (LastDecodeFrameIndex != CurrentDecodeFrameIndex)
        {
            SE_ERROR("orphan dep view frame\n");
        }

        LastDepDecodeBufferIndex = Context->BufferIndex;

        if (stereo == 0) // keep only base
        {
            BufferState[Context->BufferIndex].OutputOnDecodesComplete = false;    // do not push dep to manifestor
        }
    }

    // Call ancestor
    CodecStatus_t Status = Codec_MmeBase_c::ReleaseDecodeContext(Context);

    // Thow away unpushed view
    if ((MVCDecodeContext->isBaseView && stereo == 1)
        || (!MVCDecodeContext->isBaseView && stereo == 0)
       )
    {
        ReleaseDecodeBufferLocked(BufferState[Context->BufferIndex].Buffer);
    }

    // Check end of stereo decode
    if (!MVCDecodeContext->isBaseView)
    {
        LastDecodeFrameIndex        = INVALID_INDEX;
        LastBaseDecodeBufferIndex   = INVALID_INDEX;
        LastDepDecodeBufferIndex    = INVALID_INDEX;
    }

    OS_UnLockMutex(&Lock);
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The mapping/translation functions for decode indices
//

CodecStatus_t   Codec_MmeVideoH264_MVC_c::MapBufferToDecodeIndex(
    unsigned int      DecodeIndex,
    unsigned int      BufferIndex)
{
    unsigned int i;

    if (isBaseView)
    {
        return Codec_MmeBase_c::MapBufferToDecodeIndex(DecodeIndex, BufferIndex);
    }

    OS_LockMutex(&Lock);

    for (i = 0; i < IndexDepBufferMapSize; i++)
        if (IndexDepBufferMap[i].DecodeIndex == INVALID_INDEX)
        {
            IndexDepBufferMap[i].DecodeIndex = DecodeIndex;
            IndexDepBufferMap[i].BufferIndex = BufferIndex;
            OS_UnLockMutex(&Lock);
            return CodecNoError;
        }

    OS_UnLockMutex(&Lock);
    return PlayerImplementationError;
}

// ---------------

CodecStatus_t   Codec_MmeVideoH264_MVC_c::UnMapBufferIndex(unsigned int BufferIndex)
{
    unsigned int    i;
    // Remove decode buffer from base view table
    Codec_MmeBase_c::UnMapBufferIndex(BufferIndex);
    OS_LockMutex(&Lock);

    // Remove decode buffer from dep view table
    for (i = 0; i < IndexDepBufferMapSize; i++)
        if (IndexDepBufferMap[i].BufferIndex == BufferIndex)
        {
            IndexDepBufferMap[i].DecodeIndex       = INVALID_INDEX;
            IndexDepBufferMap[i].BufferIndex       = INVALID_INDEX;
        }

    OS_UnLockMutex(&Lock);
    return CodecNoError;
}

// ---------------

CodecStatus_t   Codec_MmeVideoH264_MVC_c::TranslateDecodeIndex(unsigned int DecodeIndex, unsigned int *BufferIndex)
{
    unsigned int    i;

    if (isBaseView)
    {
        return Codec_MmeBase_c::TranslateDecodeIndex(DecodeIndex, BufferIndex);
    }

    OS_LockMutex(&Lock);

    for (i = 0; i < IndexDepBufferMapSize; i++)
        if (IndexDepBufferMap[i].DecodeIndex == DecodeIndex)
        {
            *BufferIndex = IndexDepBufferMap[i].BufferIndex;
            OS_UnLockMutex(&Lock);
            return CodecNoError;
        }

    OS_UnLockMutex(&Lock);
    //
    *BufferIndex  = INVALID_INDEX;
    return CodecUnknownFrame;
}

// Called from Intermediate Process when CodecFnReleaseReferenceFrame has passed thru the preprocessor
CodecStatus_t   Codec_MmeVideoH264_MVC_c::H264ReleaseReferenceFrame(unsigned int         ReferenceFrameDecodeIndex)
{
    if (ReferenceFrameDecodeIndex != CODEC_RELEASE_ALL)
    {
        // Which view ?
        isBaseView =  !(MVC_IS_DEP_FRAME_INDEX(ReferenceFrameDecodeIndex));

        // restore frame index for dep view frames
        if (!isBaseView)
        {
            ReferenceFrameDecodeIndex = MVC_DECODE_DEP_FRAME_INDEX(ReferenceFrameDecodeIndex);
        }
    }

    SE_VERBOSE(group_decoder_video, "base %d, FI %d\n", isBaseView, ReferenceFrameDecodeIndex);
    // Actually release frame(s)
    return Codec_MmeVideoH264_c::H264ReleaseReferenceFrame(ReferenceFrameDecodeIndex);
}
