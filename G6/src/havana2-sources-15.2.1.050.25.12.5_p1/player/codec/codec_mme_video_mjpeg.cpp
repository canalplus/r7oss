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

#include "codec_mme_video_mjpeg.h"
#include "mjpeg.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoMjpeg_c"

//{{{
typedef struct MjpegCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} MjpegCodecStreamParameterContext_t;

typedef struct MjpegCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
#if defined (USE_JPEG_HW_TRANSFORMER)
    JPEGDECHW_VideoDecodeParams_t       DecodeParameters;
    JPEGDECHW_VideoDecodeReturnParams_t DecodeStatus;
#else
    JPEGDEC_TransformParams_t           DecodeParameters;
    JPEGDEC_TransformReturnParams_t     DecodeStatus;
#endif
} MjpegCodecDecodeContext_t;

#define BUFFER_MJPEG_CODEC_STREAM_PARAMETER_CONTEXT               "MjpegCodecStreamParameterContext"
#define BUFFER_MJPEG_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_MJPEG_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MjpegCodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   MjpegCodecStreamParameterContextDescriptor = BUFFER_MJPEG_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_MJPEG_CODEC_DECODE_CONTEXT        "MjpegCodecDecodeContext"
#define BUFFER_MJPEG_CODEC_DECODE_CONTEXT_TYPE   {BUFFER_MJPEG_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(MjpegCodecDecodeContext_t)}
static BufferDataDescriptor_t                   MjpegCodecDecodeContextDescriptor        = BUFFER_MJPEG_CODEC_DECODE_CONTEXT_TYPE;

//}}}

Codec_MmeVideoMjpeg_c::Codec_MmeVideoMjpeg_c(void)
    : Codec_MmeVideo_c()
    , MjpegTransformCapability()
#if defined (USE_JPEG_HW_TRANSFORMER)
    , MjpegInitializationParameters()
#endif
{
    Configuration.CodecName                             = "Mjpeg video";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &MjpegCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &MjpegCodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
#if defined (USE_JPEG_HW_TRANSFORMER)
    Configuration.TransformName[0]                      = JPEGHWDEC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = JPEGHWDEC_MME_TRANSFORMER_NAME "1";
    Configuration.AddressingMode                        = PhysicalAddress;
#else
    Configuration.TransformName[0]                      = JPEGDEC_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = JPEGDEC_MME_TRANSFORMER_NAME "1";
    Configuration.AddressingMode                        = UnCachedAddress;
#endif
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(MjpegTransformCapability);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&MjpegTransformCapability);
    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
}

Codec_MmeVideoMjpeg_c::~Codec_MmeVideoMjpeg_c(void)
{
    Halt();
}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Mjpeg mme transformer.
//
CodecStatus_t   Codec_MmeVideoMjpeg_c::HandleCapabilities(void)
{
    BufferFormat_t  DisplayFormat;
    DeltaTop_TransformerCapability_t *DeltaTopCapabilities = (DeltaTop_TransformerCapability_t *) Configuration.DeltaTopCapabilityStructurePointer;
#if defined (USE_JPEG_HW_TRANSFORMER)
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", JPEGHWDEC_MME_TRANSFORMER_NAME);
    SE_DEBUG(group_decoder_video, "  Api version %x\n", MjpegTransformCapability.api_version);
#if (JPEGHW_MME_VERSION >= 14)

    if ((DeltaTopCapabilities != NULL) && (DeltaTopCapabilities->DisplayBufferFormat == DELTA_OUTPUT_RASTER))
    {
        DisplayFormat = FormatVideo420_Raster2B;
    }
    else
#endif
    {
        DisplayFormat = FormatVideo420_MacroBlock;
    }

#else
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", JPEGDEC_MME_TRANSFORMER_NAME);
    SE_DEBUG(group_decoder_video, "  caps_len %d\n", MjpegTransformCapability.caps_len);
    DisplayFormat = FormatVideo422_Planar;
#endif
    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat, PrimaryManifestationElement,
                                                         Configuration.ListOfDecodeBufferComponents);
#if !defined (USE_JPEG_HW_TRANSFORMER)
    Configuration.ListOfDecodeBufferComponents[0].DefaultAddressMode    = UnCachedAddress;
#endif
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Mjpeg mme transformer.
//
CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutTransformerInitializationParameters(void)
{
#if defined (USE_JPEG_HW_TRANSFORMER)
    // Fill out the command parameters
    MjpegInitializationParameters.CircularBufferBeginAddr_p     = (U32 *)0x00000000;
    MjpegInitializationParameters.CircularBufferEndAddr_p       = (U32 *)0xffffffff;
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(MjpegInitializationParameters);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&MjpegInitializationParameters);
#else
    SE_VERBOSE(group_decoder_video, "\n");
    // Fill out the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = 0;
    MMEInitializationParameters.TransformerInitParams_p         = NULL;
#endif
    return CodecNoError;
}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Mjpeg mme transformer.
//
CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutSetStreamParametersCommand(void)
{
    SE_ERROR("This should not be called as we have no stream parameters\n");
    return CodecError;
}
//}}}
//{{{  FillOutDecodeCommand (HWDEC)
#if defined (USE_JPEG_HW_TRANSFORMER)
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an mjpeg mme transformer.
//

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(void)
{
    MjpegCodecDecodeContext_t          *Context         = (MjpegCodecDecodeContext_t *)DecodeContext;
    MjpegFrameParameters_t             *Parsed          = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t          *PictureHeader   = &Parsed->PictureHeader;
    JPEGDECHW_VideoDecodeParams_t      *Param;
    JPEGDECHW_DecodedBufferAddress_t   *Decode;
#if (JPEGHW_MME_VERSION >= 14)
    JPEGDECHW_DisplayBufferAddress_t   *Display;
#endif
    Buffer_t                            DecodeBuffer;
    SE_VERBOSE(group_decoder_video, "\n");
    // For Mjpeg we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer                                = BufferState[CurrentDecodeBufferIndex].Buffer;
    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddr;
    Param->PictureStartAddr_p                   = (JPEGDECHW_CompressedData_t)CodedData;
    Param->PictureEndAddr_p                     = (JPEGDECHW_CompressedData_t)(CodedData + CodedDataLength + 128);
    //{{{  Fill in remaining fields
#if (JPEGHW_MME_VERSION >= 14)
    Decode->Luma_p                              = NULL;
    Decode->Chroma_p                            = NULL;
    Display                                     = &Param->DisplayBufferAddr;
    Display->StructSize                         = sizeof(JPEGDECHW_DisplayBufferAddress_t);
    Display->DisplayLuma_p                      = (JPEGDECHW_LumaAddress_t)NULL;
    Display->DisplayChroma_p                    = (JPEGDECHW_ChromaAddress_t)NULL;
    Display->DisplayDecimatedLuma_p             = (JPEGDECHW_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, PrimaryManifestationComponent);
    Display->DisplayDecimatedChroma_p           = (JPEGDECHW_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, PrimaryManifestationComponent);
    Param->MainAuxEnable                        = JPEGDECHW_DISP_AUX_EN;
    Param->AdditionalFlags                      = JPEGDECHW_ADDITIONAL_FLAG_420MB;
#else
    Decode->Luma_p                              = NULL;
    Decode->Chroma_p                            = NULL;
    Decode->LumaDecimated_p                     = (JPEGDECHW_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, PrimaryManifestationComponent);
    Decode->ChromaDecimated_p                   = (JPEGDECHW_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, PrimaryManifestationComponent);
    Param->MainAuxEnable                        = JPEGDECHW_AUXOUT_EN;
    Param->AdditionalFlags                      = JPEGDECHW_ADDITIONAL_FLAG_NONE;
#endif
    Param->HorizontalDecimationFactor           = JPEGDECHW_HDEC_1;
    Param->VerticalDecimationFactor             = JPEGDECHW_VDEC_1;
    Param->xvalue0                              = 0;
    Param->xvalue1                              = 0;
    Param->yvalue0                              = 0;
    Param->yvalue1                              = 0;
    Param->DecodingMode                         = JPEGDECHW_NORMAL_DECODE;
#if (JPEGHW_MME_VERSION >= 16)
    Param->FieldFlag                            = PictureHeader->field_flag;
#endif

    //}}}
    // Fill out the actual command
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(JPEGDECHW_VideoDecodeReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.ParamSize                         = sizeof(Context->DecodeParameters);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(&Context->DecodeParameters);
    DecodeContext->MMECommand.NumberInputBuffers                = 0;
    DecodeContext->MMECommand.NumberOutputBuffers               = 0;
    DecodeContext->MMECommand.DataBuffers_p                     = NULL;

    OS_UnLockMutex(&Lock);

    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand (SWDEC frame based)
#elif defined (USE_JPEG_FRAME_TRANSFORMER)
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an mjpeg mme transformer.
//

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(void)
{
    MjpegCodecDecodeContext_t          *Context                 = (MjpegCodecDecodeContext_t *)DecodeContext;
    MjpegFrameParameters_t             *Parsed                  = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t          *PictureHeader           = &Parsed->PictureHeader;
    JPEGDEC_TransformParams_t          *Param;
    JPEGDEC_InitParams_t               *Setup;
    Buffer_t                DecodeBuffer;
    SE_VERBOSE(group_decoder_video, "\n");
    OS_LockMutex(&Lock);
    DecodeBuffer                                                = BufferState[CurrentDecodeBufferIndex].Buffer;
    Param                                                       = &Context->DecodeParameters;
    Setup                                                       = &Param->outputSettings;
    memset(Setup, 0, sizeof(JPEGDEC_InitParams_t));
    Setup->outputWidth                                          = PictureHeader->frame_width;
    Setup->outputHeight                                         = PictureHeader->frame_height;
#if (JPEGHW_MME_VERSION >= 16)
    Param->FieldFlag                                            = PictureHeader->field_flag;
#endif
    // Fill out the actual command
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(Context->DecodeStatus);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = MJPEG_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = MJPEG_NUM_MME_OUTPUT_BUFFERS;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(JPEGDEC_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(Param);

    //{{{  Fill in details for all buffers
    for (int i = 0; i < MJPEG_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                         = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                 = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                 = NULL;
        DecodeContext->MMEBuffers[i].Flags                      = 0;
        DecodeContext->MMEBuffers[i].StreamNumber               = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages       = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p             = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                = 0;
    }

    //}}}
    //{{{  Then overwrite bits specific to other buffers
    DecodeContext->MMEBuffers[MJPEG_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p     = (void *)CodedData;
    DecodeContext->MMEBuffers[MJPEG_MME_CODED_DATA_BUFFER].TotalSize                    = CodedDataLength;
    DecodeContext->MMEBuffers[MJPEG_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p  = (void *)Stream->GetDecodeBufferManager()->Raster(DecodeBuffer, PrimaryManifestationComponent);
    DecodeContext->MMEBuffers[MJPEG_MME_CURRENT_FRAME_BUFFER].TotalSize                 = Stream->GetDecodeBufferManager()->RasterSize(DecodeBuffer, PrimaryManifestationComponent);

    //}}}
    //{{{  Initialise remaining scatter page values
    for (int i = 0; i < MJPEG_NUM_MME_BUFFERS; i++)
    {
        // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }

    //}}}
    OS_UnLockMutex(&Lock);
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand (SWDEC stream based)
#else
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an mjpeg mme transformer.
//

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(void)
{
    SE_VERBOSE(group_decoder_video, "\n");
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_SEND_BUFFERS;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NO_INFO;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.NumberInputBuffers                = 1;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    memset(&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];
    memset(&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
    DecodeContext->MMEBuffers[0].TotalSize                      = CodedDataLength;
    DecodeContext->MMEPages[0].Page_p                           = CodedData;
    DecodeContext->MMEPages[0].Size                             = CodedDataLength;
    return CodecNoError;
}
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an mjpeg mme transformer.
//

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutTransformCommand(void)
{
    MjpegCodecDecodeContext_t          *Context                 = (MjpegCodecDecodeContext_t *)DecodeContext;
    MjpegFrameParameters_t             *Parsed                  = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t          *PictureHeader           = &Parsed->PictureHeader;
    JPEGDEC_TransformParams_t          *Param;
    JPEGDEC_InitParams_t               *Setup;
    Buffer_t                DecodeBuffer;
    SE_VERBOSE(group_decoder_video, "\n");
    // Initialize the frame parameters
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));
    // Zero the reply structure
    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
    OS_LockMutex(&Lock);
    DecodeBuffer                                                = BufferState[CurrentDecodeBufferIndex].Buffer;
    Param                                                       = &Context->DecodeParameters;
    Setup                                                       = &Param->outputSettings;
    memset(Setup, 0, sizeof(JPEGDEC_InitParams_t));
    Setup->outputWidth                                          = PictureHeader->frame_width;
    Setup->outputHeight                                         = PictureHeader->frame_height;
#if (JPEGHW_MME_VERSION >= 16)
    Param->FieldFlag                                            = PictureHeader->field_flag;
#endif
    // Fill out the actual command
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(Context->DecodeStatus);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberOutputBuffers               = 1;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t **)DecodeContext->MMEBufferList;
    DecodeContext->MMECommand.ParamSize                         = sizeof(JPEGDEC_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(Param);
    memset(&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];
    memset(&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
    DecodeContext->MMEBuffers[0].TotalSize                      = Setup->outputWidth * Setup->outputHeight * 2;
    DecodeContext->MMEPages[0].Page_p                           = (void *)Stream->GetDecodeBufferManager()->Raster(DecodeBuffer, PrimaryManifestationComponent);
    DecodeContext->MMEPages[0].Size                             = Stream->GetDecodeBufferManager()->ComponentSize(DecodeBuffer, PrimaryManifestationComponent);
    OS_UnLockMutex(&Lock);
    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoMjpeg_c::SendMMEDecodeCommand(void)
{
    MME_ERROR           MMEStatus;
    MMECommandPreparedCount++;

    // Check that we have not commenced shutdown.
    if (TestComponentState(ComponentHalted))
    {
        MMECommandAbortedCount++;
        return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //
    DumpMMECommand(&DecodeContext->MMECommand);
    //
    // Perform the mme transaction - Note at this point we invalidate
    // our pointer to the context buffer, after the send we invalidate
    // out pointer to the context data.
    //
    OS_FlushCacheAll(); // TODO(pht) replace with OS_FlushCacheRange

    DecodeContextBuffer                 = NULL;
    DecodeContext->DecodeCommenceTime   = OS_GetTimeInMicroSeconds();
    MMEStatus                           = MME_SendCommand(MMEHandle, &DecodeContext->MMECommand);

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("(%s) Unable to send decode command (%08x)\n", Configuration.CodecName, MMEStatus);
        return CodecError;
    }

    FillOutTransformCommand();
    MMEStatus                           = MME_SendCommand(MMEHandle, &DecodeContext->MMECommand);

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("(%s) Unable to send decode command (%08x)\n", Configuration.CodecName, MMEStatus);
        return CodecError;
    }

    DecodeContext       = NULL;

    return CodecNoError;
}
#endif
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
CodecStatus_t   Codec_MmeVideoMjpeg_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    MjpegCodecDecodeContext_t          *MjpegContext    = (MjpegCodecDecodeContext_t *)Context;
#if defined (USE_JPEG_HW_TRANSFORMER)
    SE_DEBUG(group_decoder_video, "    pm_cycles            %d\n", MjpegContext->DecodeStatus.pm_cycles);
    SE_DEBUG(group_decoder_video, "    pm_dmiss             %d\n", MjpegContext->DecodeStatus.pm_dmiss);
    SE_DEBUG(group_decoder_video, "    pm_imiss             %d\n", MjpegContext->DecodeStatus.pm_imiss);
    SE_DEBUG(group_decoder_video, "    pm_bundles           %d\n", MjpegContext->DecodeStatus.pm_bundles);
    SE_DEBUG(group_decoder_video, "    pm_pft               %d\n", MjpegContext->DecodeStatus.pm_pft);
    //SE_DEBUG(group_decoder_video, "    DecodeTimeInMicros   %d\n", MjpegContext->DecodeStatus.DecodeTimeInMicros);
#else
    SE_DEBUG(group_decoder_video, "    bytes_written        %d\n", MjpegContext->DecodeStatus.bytes_written);
    SE_DEBUG(group_decoder_video, "    decodedImageHeight   %d\n", MjpegContext->DecodeStatus.decodedImageHeight);
    SE_DEBUG(group_decoder_video, "    decodedImageWidth    %d\n", MjpegContext->DecodeStatus.decodedImageWidth);
    SE_DEBUG(group_decoder_video, "    Total_cycle          %d\n", MjpegContext->DecodeStatus.Total_cycle);
    SE_DEBUG(group_decoder_video, "    DMiss_Cycle          %d\n", MjpegContext->DecodeStatus.DMiss_Cycle);
    SE_DEBUG(group_decoder_video, "    IMiss_Cycle          %d\n", MjpegContext->DecodeStatus.IMiss_Cycle);
#endif
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//
CodecStatus_t   Codec_MmeVideoMjpeg_c::DumpSetStreamParameters(void    *Parameters)
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

unsigned int MjpegStaticPicture;
CodecStatus_t   Codec_MmeVideoMjpeg_c::DumpDecodeParameters(void    *Parameters)
{
#if defined (USE_JPEG_HW_TRANSFORMER)
    MjpegStaticPicture++;
    SE_VERBOSE(group_decoder_video, "Picture %d %p\n", MjpegStaticPicture - 1, Parameters);
#endif
    return CodecNoError;
}
//}}}

// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(JPEG_DECODER_UNDEFINED_HUFF_TABLE);
        E(JPEG_DECODER_UNSUPPORTED_MARKER);
        E(JPEG_DECODER_UNABLE_ALLOCATE_MEMORY);
        E(JPEG_DECODER_NON_SUPPORTED_SAMP_FACTORS);
        E(JPEG_DECODER_BAD_PARAMETER);
        E(JPEG_DECODER_DECODE_ERROR);
        E(JPEG_DECODER_BAD_RESTART_MARKER);
        E(JPEG_DECODER_UNSUPPORTED_COLORSPACE);
        E(JPEG_DECODER_BAD_SOS_SPECTRAL);
        E(JPEG_DECODER_BAD_SOS_SUCCESSIVE);
        E(JPEG_DECODER_BAD_HEADER_LENGHT);
        E(JPEG_DECODER_BAD_COUNT_VALUE);
        E(JPEG_DECODER_BAD_DHT_MARKER);
        E(JPEG_DECODER_BAD_INDEX_VALUE);
        E(JPEG_DECODER_BAD_NUMBER_HUFFMAN_TABLES);
        E(JPEG_DECODER_BAD_QUANT_TABLE_LENGHT);
        E(JPEG_DECODER_BAD_NUMBER_QUANT_TABLES);
        E(JPEG_DECODER_BAD_COMPONENT_COUNT);
        E(JPEG_DECODER_DIVIDE_BY_ZERO_ERROR);
        E(JPEG_DECODER_NOT_JPG_IMAGE);
        E(JPEG_DECODER_UNSUPPORTED_ROTATION_ANGLE);
        E(JPEG_DECODER_UNSUPPORTED_SCALING);
        E(JPEG_DECODER_INSUFFICIENT_OUTPUTBUFFER_SIZE);
        E(JPEG_DECODER_BAD_HWCFG_GP_VERSION_VALUE);
        E(JPEG_DECODER_BAD_VALUE_FROM_RED);
        E(JPEG_DECODER_BAD_SUBREGION_PARAMETERS);
        E(JPEG_DECODER_PROGRESSIVE_DECODE_NOT_SUPPORTED);
        E(JPEG_DECODER_ERROR_TASK_TIMEOUT);

    default: return "JPEG_DECODER_UNKNOWN_ERROR";
    }

#undef E
}

CodecStatus_t   Codec_MmeVideoMjpeg_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    MME_Command_t                       *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t                 *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    JPEGDECHW_VideoDecodeReturnParams_t *AdditionalInfo_p = (JPEGDECHW_VideoDecodeReturnParams_t *)  CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        if (AdditionalInfo_p->ErrorCode != JPEG_DECODER_NO_ERROR)
        {
            // Set the decode quality to worst because there is a decoding error and there is no emulation.
            // This is done to allow flexibity for not displaying the corrupt output, using player policy, after encountering the decoding error.
            Context->DecodeQuality = 0;
            SE_INFO(group_decoder_video, "%s  0x%x\n", LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);
        }
    }

    return CodecNoError;
}

