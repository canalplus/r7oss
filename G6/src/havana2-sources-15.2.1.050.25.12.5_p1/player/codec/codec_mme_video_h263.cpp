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

#include "codec_mme_video_h263.h"
#include "h263.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoH263_c"

//{{{
typedef struct H263CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
    H263D_GlobalParams_t                StreamParameters;
} H263CodecStreamParameterContext_t;

#define BUFFER_H263_CODEC_STREAM_PARAMETER_CONTEXT               "H263CodecStreamParameterContext"
#define BUFFER_H263_CODEC_STREAM_PARAMETER_CONTEXT_TYPE  {BUFFER_H263_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(H263CodecStreamParameterContext_t)}

static BufferDataDescriptor_t           H263CodecStreamParameterContextDescriptor = BUFFER_H263_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct H263CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    H263D_TransformParams_t             DecodeParameters;
    H263D_DecodeReturnParams_t          DecodeStatus;
} H263CodecDecodeContext_t;

#define BUFFER_H263_CODEC_DECODE_CONTEXT         "H263CodecDecodeContext"
#define BUFFER_H263_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_H263_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(H263CodecDecodeContext_t)}

static BufferDataDescriptor_t                   H263CodecDecodeContextDescriptor = BUFFER_H263_CODEC_DECODE_CONTEXT_TYPE;
//}}}

Codec_MmeVideoH263_c::Codec_MmeVideoH263_c(void)
    : Codec_MmeVideo_c()
    , H263TransformerCapability()
    , H263InitializationParameters()
{
    Configuration.CodecName                             = "H263 video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &H263CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &H263CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;
    Configuration.TransformName[0]                      = H263_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = H263_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(H263D_Capability_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&H263TransformerCapability);
    Configuration.AddressingMode                        = CachedAddress;
    //
    // We do not need the coded data after decode is complete
    //
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
}

Codec_MmeVideoH263_c::~Codec_MmeVideoH263_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an H263 mme transformer.
//
CodecStatus_t   Codec_MmeVideoH263_c::HandleCapabilities(void)
{
    SE_INFO(group_decoder_video, "'%s' capabilities are :-\n", H263_MME_TRANSFORMER_NAME);
    SE_DEBUG(group_decoder_video, "  caps len                          = %d bytes\n", H263TransformerCapability.caps_len);
    Stream->GetDecodeBufferManager()->FillOutDefaultList(FormatVideo420_MacroBlock,
                                                         PrimaryManifestationElement,
                                                         Configuration.ListOfDecodeBufferComponents);
    Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = CachedAddress;
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an H263 mme transformer.
//
CodecStatus_t   Codec_MmeVideoH263_c::FillOutTransformerInitializationParameters(void)
{
    //
    // Fill out the command parameters
    //
    H263InitializationParameters.PictureWidhth                  = H263_WIDTH_CIF;
    H263InitializationParameters.PictureHeight                  = H263_HEIGHT_CIF;
    H263InitializationParameters.ErrorConceal_type              = MOTION_COMPENSATED;   // doesn't really matter as not currently supported
    //H263InitializationParameters.Post_processing                = 0;
    SE_INFO(group_decoder_video, "PictureWidth              %6u\n", H263_WIDTH_CIF);
    SE_INFO(group_decoder_video, "PictureHeight             %6u\n", H263_HEIGHT_CIF);
    //
    // Fill out the actual command
    //
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(H263D_GlobalParams_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&H263InitializationParameters);
    return CodecNoError;
}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an H263 mme transformer.
//
CodecStatus_t   Codec_MmeVideoH263_c::FillOutSetStreamParametersCommand(void)
{
    H263CodecStreamParameterContext_t          *Context = (H263CodecStreamParameterContext_t *)StreamParameterContext;
    H263StreamParameters_t                     *Parsed  = (H263StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    SE_INFO(group_decoder_video, "(%dx%d)\n", Parsed->SequenceHeader.width, Parsed->SequenceHeader.height);
    //
    // Fill out the command parameters
    //
    Context->StreamParameters.PictureWidhth             = Parsed->SequenceHeader.width;
    Context->StreamParameters.PictureHeight             = Parsed->SequenceHeader.height;
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(H263D_GlobalParams_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an H263 mme transformer.
//

CodecStatus_t   Codec_MmeVideoH263_c::FillOutDecodeCommand(void)
{
    H263CodecDecodeContext_t           *Context        = (H263CodecDecodeContext_t *)DecodeContext;
    H263FrameParameters_t              *Frame          = (H263FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //H263StreamParameters_t*             Stream         = (H263StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    H263D_TransformParams_t            *Param;
    unsigned int                        i;
    Buffer_t                DecodeBuffer;
    SE_VERBOSE(group_decoder_video, "\n");

    if (ParsedVideoParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedVideoParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    //
    // For H263 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                = BufferState[CurrentDecodeBufferIndex].Buffer;
    //
    // Fill out the straight forward command parameters
    //
    Param                                       = &Context->DecodeParameters;
    Param->Width                                = ParsedVideoParameters->Content.Width;
    Param->Height                               = ParsedVideoParameters->Content.Height;
    Param->FrameType                            = (H263PictureCodingType_t)Frame->PictureHeader.ptype;          // I/P
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(H263D_DecodeReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t *)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = H263_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = H263_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(H263D_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = NULL;//(MME_GenericParams_t *)&ConcealmentParams;

    // Fill in details for all buffers
    for (i = 0; i < H263_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].TotalSize                      = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, PrimaryManifestationComponent);
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p       = Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, PrimaryManifestationComponent);
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }

    // Then overwrite bits specific to coded data buffer
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].TotalSize                      = CodedDataLength;
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p       = (void *)CodedData;
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Size         = CodedDataLength;

    if (DecodeContext->ReferenceFrameList[0].EntryCount == 1)
    {
        unsigned int    Entry       = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        Buffer_t    ReferenceBuffer = BufferState[Entry].Buffer;
        DecodeContext->MMEBuffers[H263_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer, PrimaryManifestationComponent);
    }

    OS_UnLockMutex(&Lock);

    if (SE_IS_VERBOSE_ON(group_decoder_video))
    {
        for (i = 0; i < H263_NUM_MME_BUFFERS; i++)
        {
            SE_VERBOSE(group_decoder_video, "Buffer List      (%d)  %p\n", i, DecodeContext->MMEBufferList[i]);
            SE_VERBOSE(group_decoder_video, "Struct Size      (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StructSize);
            SE_VERBOSE(group_decoder_video, "User Data p      (%d)  %p\n", i, DecodeContext->MMEBuffers[i].UserData_p);
            SE_VERBOSE(group_decoder_video, "Flags            (%d)  %x\n", i, DecodeContext->MMEBuffers[i].Flags);
            SE_VERBOSE(group_decoder_video, "Stream Number    (%d)  %x\n", i, DecodeContext->MMEBuffers[i].StreamNumber);
            SE_VERBOSE(group_decoder_video, "No Scatter Pages (%d)  %x\n", i, DecodeContext->MMEBuffers[i].NumberOfScatterPages);
            SE_VERBOSE(group_decoder_video, "Scatter Pages p  (%d)  %p\n", i, DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
            SE_VERBOSE(group_decoder_video, "Start Offset     (%d)  %x\n\n", i, DecodeContext->MMEBuffers[i].StartOffset);
        }

        SE_VERBOSE(group_decoder_video, "Additional Size  %x\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
        SE_VERBOSE(group_decoder_video, "Additional p     %p\n", DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
        SE_VERBOSE(group_decoder_video, "Param Size       %x\n", DecodeContext->MMECommand.ParamSize);
        SE_VERBOSE(group_decoder_video, "Param p          %p\n", DecodeContext->MMECommand.Param_p);
        SE_VERBOSE(group_decoder_video, "DataBuffer p     %p\n", DecodeContext->MMECommand.DataBuffers_p);
    }

    Context->BaseContext.MMECommand.ParamSize                       = sizeof(H263D_TransformParams_t);
    Context->BaseContext.MMECommand.Param_p                         = (MME_GenericParams_t)(&Context->DecodeParameters);
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
CodecStatus_t   Codec_MmeVideoH263_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//
CodecStatus_t   Codec_MmeVideoH263_c::DumpSetStreamParameters(void    *Parameters)
{
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

unsigned int H263StaticPicture;
CodecStatus_t   Codec_MmeVideoH263_c::DumpDecodeParameters(void    *Parameters)
{
    H263StaticPicture++;
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", H263StaticPicture - 1, Parameters);
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
        E(WRONG_START_CODE_EMULATION);              // Bit should be always 1
        E(WRONG_DISTINCTION_MARKER);                // Distinction with H261 always 0
        E(SPLIT_SCREEN_NOT_SUPPORTED);          // Split screen not supported in this version
        E(DOC_CAMERA_INDICATOR_NOT_SUPPORTED);      // Document camera indicator not supported
        E(PICTURE_FREEZE_NOT_SUPPORTED);            // Picture freeze not supported in this version
        E(UNSUPPORTED_RESOLUTION);                  // Maximum resolution supported is CIF
        E(ANNEX_D_NOT_SUPPORTED);                   // Unrestricted Motion vector mode not supported in this version
        E(ANNEX_E_NOT_SUPPORTED);                   // Syntax based arithmetic coding not supported
        E(ANNEX_F_NOT_SUPPORTED);                   // Advanced prediction mode not supported
        E(ANNEX_G_NOT_SUPPORTED);                   // PB-Frame mode not supported
        E(CPM_MODE_NOT_SUPPORTED);                  // CPM mode not supported
        E(INVALID_UFEP);                            // Invalid UFEP bit
        E(ANNEX_N_NOT_SUPPORTED);                   // Reference picture selection mode not supported
        E(ANNEX_R_NOT_SUPPORTED);                   // Independent Segment mode decoding not supported
        E(ANNEX_S_NOT_SUPPORTED);                   // Alternate Inter VLC mode not supported
        E(ANNEX_I_NOT_SUPPORTED);                   // Advanced Intra coding mode not supported
        E(ANNEX_J_NOT_SUPPORTED);                   // Deblocking Filter mode not supported
        E(ANNEX_K_NOT_SUPPORTED);                   // Slice structure mode not supported
        E(ANNEX_T_NOT_SUPPORTED);                   // Modified Quantization mode not supported
        E(CORRUPT_MARKER_BIT);                      // Corrupted markler bit
        E(ANNEX_P_NOT_SUPPORTED);                   // Reference picture resampling mode not supported
        E(ANNEX_Q_NOT_SUPPORTED);                   // Reduced resolution update mode not supported
        E(INVALID_P_TYPE);                          // Ptype not valid
        E(UNSUPPORTED_RECTANGULAR_SLICE);          // Rectangular slice order not supported
        E(ARBITARY_SLICE_ORDER);                   // Arbitary slice order not supported
        E(PICTURE_SC_ERROR);                       // Could not find the begining of the picture layer

    default: return "H263_DECODER_UNKNOWN_ERROR";
    }

#undef E
}
CodecStatus_t   Codec_MmeVideoH263_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t              *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t        *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    H263D_DecodeReturnParams_t *AdditionalInfo_p = (H263D_DecodeReturnParams_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorType != H263_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->ErrorType), AdditionalInfo_p->ErrorType);
            Stream->Statistics().FrameDecodeError++;
        }
    }

    return CodecNoError;
}
//}}}
