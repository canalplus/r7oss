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

#include "theora.h"
#include "codec_mme_video_theora.h"
#include "allocinline.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoTheora_c"

//{{{
typedef struct TheoraCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} TheoraCodecStreamParameterContext_t;

typedef struct TheoraCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    THEORA_TransformParam_t             DecodeParameters;
    THEORA_ReturnParams_t               DecodeStatus;
} TheoraCodecDecodeContext_t;

#define BUFFER_THEORA_CODEC_STREAM_PARAMETER_CONTEXT               "TheoraCodecStreamParameterContext"
#define BUFFER_THEORA_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_THEORA_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(TheoraCodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   TheoraCodecStreamParameterContextDescriptor = BUFFER_THEORA_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_THEORA_CODEC_DECODE_CONTEXT         "TheoraCodecDecodeContext"
#define BUFFER_THEORA_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_THEORA_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(TheoraCodecDecodeContext_t)}
static BufferDataDescriptor_t                   TheoraCodecDecodeContextDescriptor        = BUFFER_THEORA_CODEC_DECODE_CONTEXT_TYPE;

//}}}

Codec_MmeVideoTheora_c::Codec_MmeVideoTheora_c(void)
    : Codec_MmeVideo_c()
    , CodedWidth(THEORA_DEFAULT_PICTURE_WIDTH)
    , CodedHeight(THEORA_DEFAULT_PICTURE_HEIGHT)
    , TheoraInitializationParameters()
    , TheoraTransformCapability()
    , TheoraGlobalParamSequence()
    , InfoHeaderMemoryDevice(NULL)
    , CommentHeaderMemoryDevice(NULL)
    , SetupHeaderMemoryDevice(NULL)
    , BufferMemoryDevice(NULL)
    , RestartTransformer(false)
    , RasterToOmega(false)
{
    Configuration.CodecName                             = "Theora video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &TheoraCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &TheoraCodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;
    Configuration.TransformName[0]                      = THEORADEC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = THEORADEC_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;
    Configuration.AddressingMode                        = CachedAddress;
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
}

Codec_MmeVideoTheora_c::~Codec_MmeVideoTheora_c(void)
{
    Halt();

    AllocatorClose(&InfoHeaderMemoryDevice);

    AllocatorClose(&CommentHeaderMemoryDevice);

    AllocatorClose(&SetupHeaderMemoryDevice);

    AllocatorClose(&BufferMemoryDevice);
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::HandleCapabilities(void)
{
    BufferFormat_t OutputFormat                 = FormatVideo420_Planar;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", THEORADEC_MME_TRANSFORMER_NAME);
    SE_INFO(group_decoder_video, "  display_buffer_size  %d\n", TheoraTransformCapability.display_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_size   %d\n", TheoraTransformCapability.packed_buffer_size);
    SE_DEBUG(group_decoder_video, "  packed_buffer_total  %d\n", TheoraTransformCapability.packed_buffer_total);
    //
    // We can query the firmware here to determine if we can/should convert to
    // an Omega Surface format output
    //
    RasterToOmega      = true;

    if (RasterToOmega)
    {
        SE_DEBUG(group_decoder_video, "  OmegaOutput Conversion enabled\n");
        OutputFormat = FormatVideo420_MacroBlock;
    }

    Stream->GetDecodeBufferManager()->FillOutDefaultList(OutputFormat,
                                                         PrimaryManifestationElement | VideoDecodeCopyElement,
                                                         Configuration.ListOfDecodeBufferComponents);
    Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[0]   = 16;
    Configuration.ListOfDecodeBufferComponents[0].ComponentBorders[1]   = 16;
    Configuration.ListOfDecodeBufferComponents[1].DataType              = RasterToOmega ? FormatVideo420_Planar : FormatVideo420_MacroBlock;
    Configuration.ListOfDecodeBufferComponents[1].DefaultAddressMode    = CachedAddress;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[0]   = 16;
    Configuration.ListOfDecodeBufferComponents[1].ComponentBorders[1]   = 16;
    return CodecNoError;
}
//}}}

//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::FillOutTransformerInitializationParameters(void)
{
    // The command parameters
    SE_INFO(group_decoder_video, "CodedWidth           %6u\n", CodedWidth);
    SE_INFO(group_decoder_video, "CodedHeight          %6u\n", CodedHeight);
    TheoraInitializationParameters.CircularBufferBeginAddr = 0;
    TheoraInitializationParameters.CircularBufferEndAddr   = 0xffffffff;
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize  = sizeof(THEORA_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p    = (MME_GenericParams_t)(&TheoraInitializationParameters);
    return CodecNoError;
}
//}}}

//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::FillOutSetStreamParametersCommand(void)
{
    TheoraStreamParameters_t   *Parsed                  = (TheoraStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    TheoraVideoSequence_t      *SequenceHeader          = &Parsed->SequenceHeader;
    DecodeBufferComponentType_t RasterComponent         = RasterToOmega ? VideoDecodeCopy : PrimaryManifestationComponent;
    {
        allocator_status_t      AStatus;
        CodedWidth                                      = SequenceHeader->DecodedWidth;
        CodedHeight                                     = SequenceHeader->DecodedHeight;

        if (SequenceHeader->PixelFormat == THEORA_PIXEL_FORMAT_422)
        {
            SE_INFO(group_decoder_video, "Switching to 422 planar format\n");
            Stream->GetDecodeBufferManager()->ModifyComponentFormat(RasterComponent, FormatVideo422_Planar);
        }
        else
        {
            Stream->GetDecodeBufferManager()->ModifyComponentFormat(RasterComponent, FormatVideo420_Planar);
        }

        AllocatorClose(&InfoHeaderMemoryDevice);

        AllocatorClose(&CommentHeaderMemoryDevice);

        AllocatorClose(&SetupHeaderMemoryDevice);

        AllocatorClose(&BufferMemoryDevice);

        SE_VERBOSE(group_decoder_video, "Allocating %d bytes for info header:\n", SequenceHeader->InfoHeaderSize);
        AStatus = PartitionAllocatorOpen(&InfoHeaderMemoryDevice, Configuration.AncillaryMemoryPartitionName, SequenceHeader->InfoHeaderSize, MEMORY_DEFAULT_ACCESS);

        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate info header memory\n");
            return CodecError;
        }

        SE_VERBOSE(group_decoder_video, "Allocating %d bytes for comment header\n", SequenceHeader->CommentHeaderSize);
        AStatus = PartitionAllocatorOpen(&CommentHeaderMemoryDevice, Configuration.AncillaryMemoryPartitionName, SequenceHeader->CommentHeaderSize, MEMORY_DEFAULT_ACCESS);

        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate comment header memory\n");
            return CodecError;
        }

        SE_VERBOSE(group_decoder_video, "Allocating %d bytes for setup header\n", SequenceHeader->SetupHeaderSize);
        AStatus = PartitionAllocatorOpen(&SetupHeaderMemoryDevice, Configuration.AncillaryMemoryPartitionName, SequenceHeader->SetupHeaderSize, MEMORY_DEFAULT_ACCESS);

        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate setup header memory\n");
            return CodecError;
        }

        TheoraGlobalParamSequence.InfoHeader.PacketStartOffset  = (U32)AllocatorPhysicalAddress(InfoHeaderMemoryDevice);
        TheoraGlobalParamSequence.InfoHeader.PacketStopOffset  = (U32)AllocatorPhysicalAddress(InfoHeaderMemoryDevice) + SequenceHeader->InfoHeaderSize;
        memcpy(AllocatorUserAddress(InfoHeaderMemoryDevice), SequenceHeader->InfoHeaderBuffer, SequenceHeader->InfoHeaderSize);
        OS_FlushCacheRange(AllocatorUserAddress(InfoHeaderMemoryDevice), (OS_PhysAddr_t)AllocatorPhysicalAddress(InfoHeaderMemoryDevice), SequenceHeader->InfoHeaderSize);
        TheoraGlobalParamSequence.CommentHeader.PacketStartOffset = (U32)AllocatorPhysicalAddress(CommentHeaderMemoryDevice);
        TheoraGlobalParamSequence.CommentHeader.PacketStopOffset = (U32)AllocatorPhysicalAddress(CommentHeaderMemoryDevice) + SequenceHeader->CommentHeaderSize;
        memcpy(AllocatorUserAddress(CommentHeaderMemoryDevice), SequenceHeader->CommentHeaderBuffer, SequenceHeader->CommentHeaderSize);
        OS_FlushCacheRange(AllocatorUserAddress(CommentHeaderMemoryDevice), (OS_PhysAddr_t)AllocatorPhysicalAddress(CommentHeaderMemoryDevice), SequenceHeader->CommentHeaderSize);
        TheoraGlobalParamSequence.SetUpHeader.PacketStartOffset = (U32)AllocatorPhysicalAddress(SetupHeaderMemoryDevice);
        TheoraGlobalParamSequence.SetUpHeader.PacketStopOffset = (U32)AllocatorPhysicalAddress(SetupHeaderMemoryDevice) + SequenceHeader->SetupHeaderSize;
        memcpy(AllocatorUserAddress(SetupHeaderMemoryDevice), SequenceHeader->SetupHeaderBuffer, SequenceHeader->SetupHeaderSize);
        OS_FlushCacheRange(AllocatorUserAddress(SetupHeaderMemoryDevice), (OS_PhysAddr_t)AllocatorPhysicalAddress(SetupHeaderMemoryDevice), SequenceHeader->SetupHeaderSize);
        {
            unsigned int        yhfrags                 = CodedWidth >> 3;
            unsigned int        yvfrags                 = CodedHeight >> 3;
            unsigned int        hdec                    = !(SequenceHeader->PixelFormat & 1);
            unsigned int        vdec                    = !(SequenceHeader->PixelFormat & 2);
            unsigned int        chfrags                 = (yhfrags + hdec) >> hdec;
            unsigned int        cvfrags                 = (yvfrags + vdec) >> vdec;
            unsigned int        yfrags                  = yhfrags * yvfrags;
            unsigned int        cfrags                  = chfrags * cvfrags;
            unsigned int        num_8x8_blocks          = yfrags + (2 * cfrags);
            unsigned int        CoefficientBufferSize   = (64 * 3 * num_8x8_blocks) + 512;
            SE_VERBOSE(group_decoder_video, "Allocating %d bytes for buffer memory\n", CoefficientBufferSize);
            AStatus = PartitionAllocatorOpen(&BufferMemoryDevice, Configuration.AncillaryMemoryPartitionName, CoefficientBufferSize, MEMORY_DEFAULT_ACCESS);

            if (AStatus != allocator_ok)
            {
                SE_ERROR("Failed to allocate buffer memory\n");
                return CodecError;
            }

            TheoraGlobalParamSequence.CoefficientBuffer = (U32)AllocatorPhysicalAddress(BufferMemoryDevice);
        }
    }
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfoSize = 0;
    StreamParameterContext->MMECommand.CmdStatus.AdditionalInfo_p   = NULL;
    StreamParameterContext->MMECommand.ParamSize  = sizeof(THEORA_SetGlobalParamSequence_t);
    StreamParameterContext->MMECommand.Param_p    = (MME_GenericParams_t)(&TheoraGlobalParamSequence);
    return CodecNoError;
}
//}}}

//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Theora mme transformer.
//

CodecStatus_t   Codec_MmeVideoTheora_c::FillOutDecodeCommand(void)
{
    TheoraCodecDecodeContext_t         *Context        = (TheoraCodecDecodeContext_t *)DecodeContext;
    THEORA_TransformParam_t            *Param;
    unsigned char           *CodedPictureData = 0;
    U32                 CodedPictureDataLength = 0;
    THEORA_PacketInfo_t        *Picture;
    unsigned int                        i;
    Buffer_t                DecodeBuffer;
    //
    // Invert the usage of the buffers when we change display mode
    //
    DecodeBufferComponentType_t RasterComponent        = RasterToOmega ? VideoDecodeCopy : PrimaryManifestationComponent;
    DecodeBufferComponentType_t OmegaComponent         = RasterToOmega ? PrimaryManifestationComponent : VideoDecodeCopy;
    SE_DEBUG(group_decoder_video, " %d (%dx%d)\n", CodedDataLength, CodedWidth, CodedHeight);
    //
    // For Theora we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    //
    // Fill out the straight forward command parameters
    //
    Param                  = &Context->DecodeParameters;
    Param->AdditionalFlags = 0;
    Param->IsSkippedVideoPacket = 0;
    Picture                     = &Param->VideoFrameHeader;
    CodedFrameBuffer->ObtainDataReference(NULL, &CodedPictureDataLength, (void **)(&CodedPictureData), PhysicalAddress);
    SE_ASSERT(CodedPictureData != NULL); // cannot be empty
    Picture->PacketStartOffset          = (U32)(CodedPictureData);
    Picture->PacketStopOffset           = (U32)(CodedPictureData) + CodedPictureDataLength;
    //
    // Fill out the actual command
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(THEORA_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t *)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = THEORA_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = THEORA_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(THEORA_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < THEORA_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;
    }

    //}}}
    // Then overwrite bits specific to other buffers
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_RASTER].ScatterPages_p[0].Page_p  = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, RasterComponent);
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_RASTER].TotalSize                 = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, RasterComponent);

    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[THEORA_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p   = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
        DecodeContext->MMEBuffers[THEORA_MME_REFERENCE_FRAME_BUFFER].TotalSize                  = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
    }

    //}}}
    //{{{  Golden frame decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 2)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[THEORA_MME_GOLDEN_FRAME_BUFFER].ScatterPages_p[0].Page_p      = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, RasterComponent);
        DecodeContext->MMEBuffers[THEORA_MME_GOLDEN_FRAME_BUFFER].TotalSize                     = Stream->GetDecodeBufferManager()->ComponentSize(ReferenceBuffer, RasterComponent);
    }

    //}}}
    //}}}
    //{{{  Current output macroblock buffer luma
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_LUMA].ScatterPages_p[0].Page_p    = (void *)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_LUMA].TotalSize                   = Stream->GetDecodeBufferManager()->LumaSize(DecodeBuffer, OmegaComponent);
    //}}}
    //{{{  Current output macroblock buffer chroma
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_CHROMA].ScatterPages_p[0].Page_p  = (void *)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, OmegaComponent);
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER_CHROMA].TotalSize                 = Stream->GetDecodeBufferManager()->ChromaSize(DecodeBuffer, OmegaComponent);
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < THEORA_NUM_MME_BUFFERS; i++)
    {
        // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }

    //}}}

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        SE_VERBOSE(group_decoder_video, "DECODE COMMAND");

        for (i = 0; i < (THEORA_NUM_MME_BUFFERS); i++)
        {
            SE_VERBOSE(group_decoder_video, "  Buffer List      (%d)  %p\n", i, DecodeContext->MMEBufferList[i]);
            SE_VERBOSE(group_decoder_video, "  Struct Size      (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StructSize);
            SE_VERBOSE(group_decoder_video, "  User Data p      (%d)  %p\n", i, DecodeContext->MMEBuffers[i].UserData_p);
            SE_VERBOSE(group_decoder_video, "  Flags            (%d)  %x\n", i, DecodeContext->MMEBuffers[i].Flags);
            SE_VERBOSE(group_decoder_video, "  Stream Number    (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StreamNumber);
            SE_VERBOSE(group_decoder_video, "  No Scatter Pages (%d)  %x\n", i, DecodeContext->MMEBuffers[i].NumberOfScatterPages);
            SE_VERBOSE(group_decoder_video, "  Scatter Pages p  (%d)  %p\n", i, DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
            SE_VERBOSE(group_decoder_video, "  Start Offset     (%d)  %d\n", i, DecodeContext->MMEBuffers[i].StartOffset);
            SE_VERBOSE(group_decoder_video, "  Size             (%d)  %d\n\n", i, DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size);
        }

        SE_VERBOSE(group_decoder_video, "  Additional Size  %x\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
        SE_VERBOSE(group_decoder_video, "  Additional p     %p\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
        SE_VERBOSE(group_decoder_video, "  Param Size       %x\n", DecodeContext->MMECommand.ParamSize);
        SE_VERBOSE(group_decoder_video, "  Param p          %p\n", DecodeContext->MMECommand.Param_p);
    }

    OS_UnLockMutex(&Lock);
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeBufferRequest
// /////////////////////////////////////////////////////////////////////////
//
//      The specific video function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeVideoTheora_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t       *Request)
{
    SE_VERBOSE(group_decoder_video, "\n");
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);
    return CodecNoError;
}
//}}}
//{{{  ValidateDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Unconditionally return success.
///
/// Success and failure codes are located entirely in the generic MME structures
/// allowing the super-class to determine whether the decode was successful. This
/// means that we have no work to do here.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeVideoTheora_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    SE_VERBOSE(group_decoder_video, "\n");
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//
CodecStatus_t   Codec_MmeVideoTheora_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_video, "\n");
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

unsigned int TheoraStaticPicture;
CodecStatus_t   Codec_MmeVideoTheora_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", TheoraStaticPicture++, Parameters);
    return CodecNoError;
}
//}}}

//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(THEORA_INVALID_PTR);                              /* An invalid pointer was provided */
        E(THEORA_INVALID_ARG);                              /* An invalid argument was provided */
        E(THEORA_ACTION_DISABLED);                          /* Requested action is disabled */
        E(THEORA_BAD_HEADER_PACKET);                        /* The contents of the header were incomplete); invalid); or unexpected */
        E(THEORA_ILLEGAL_FORMAT);                           /* The header does not belong to a E(THEORA stream */
        E(THEORA_BAD_VERSION);                              /* The bitstream version is too high */
        E(THEORA_FEATURE_NOT_IMPLEMENTED);                  /* Feature or action not implemented */
        E(THEORA_BAD_VIDEO_PACKET);                     /* Video Data Packet is corrupted */
        E(THEORA_UNHANDLED_PACKET);                     /* Packet is an (ignorable) unhandled extension */
        E(THEORA_FIRMWARE_TIME_OUT_ENCOUNTERED);            /* Decoder task timeout has occurred */
        E(THEORA_BAD_ERROR_CODE);                           /* Illegal Error code */
    default: return "THEORA_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoTheora_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t         *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t   *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    THEORA_ReturnParams_t *AdditionalInfo_p = (THEORA_ReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->Errortype != THEORA_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->Errortype), AdditionalInfo_p->Errortype);
            switch (AdditionalInfo_p->Errortype)
            {
            case THEORA_FIRMWARE_TIME_OUT_ENCOUNTERED:
                Stream->Statistics().FrameDecodeErrorTaskTimeOutError++;
                break;

            default:
                Stream->Statistics().FrameDecodeError++;
                break;
            }
        }
    }

    return CodecNoError;
}
//}}}
