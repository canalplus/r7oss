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

Source file name : codec_mme_video_mjpeg.cpp
Author :           Julian

Implementation of the Sorenson H263 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-May-08   Created                                         Julian

************************************************************************/
// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_mjpeg.h"
#include "mjpeg.h"
#ifdef __KERNEL__
extern "C"{void flush_cache_all();};
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

//{{{  Locally defined structures
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

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoMjpeg_c::Codec_MmeVideoMjpeg_c( void )
{
    Configuration.CodecName                             = "Mjpeg video";

#if defined (USE_JPEG_HW_TRANSFORMER)
    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;
#else
    Configuration.DecodeOutputFormat                    = FormatVideo422_Planar;
#endif

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &MjpegCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &MjpegCodecDecodeContextDescriptor;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;

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

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoMjpeg_c::~Codec_MmeVideoMjpeg_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for Mjpeg specific members.
//      
//

CodecStatus_t   Codec_MmeVideoMjpeg_c::Reset( void )
{
    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Mjpeg mme transformer.
//
CodecStatus_t   Codec_MmeVideoMjpeg_c::HandleCapabilities( void )
{
#if defined (USE_JPEG_HW_TRANSFORMER)
    CODEC_TRACE("MME Transformer '%s' capabilities are :-\n", JPEGHWDEC_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    Api version %x\n", MjpegTransformCapability.api_version);
#else
    CODEC_TRACE("MME Transformer '%s' capabilities are :-\n", JPEGDEC_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    caps_len %d\n", MjpegTransformCapability.caps_len);
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
CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutTransformerInitializationParameters( void )
{
#if defined (USE_JPEG_HW_TRANSFORMER)
    // Fillout the command parameters
    MjpegInitializationParameters.CircularBufferBeginAddr_p     = (U32*)0x00000000;
    MjpegInitializationParameters.CircularBufferEndAddr_p       = (U32*)0xffffffff;

    // Fillout the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(MjpegInitializationParameters);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&MjpegInitializationParameters);

#else
    CODEC_TRACE("%s\n", __FUNCTION__);
    // Fillout the actual command
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
CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutSetStreamParametersCommand( void )
{
    CODEC_ERROR ("%s - This should not be called as we have no stream parameters\n");
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

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(       void )
{
    MjpegCodecDecodeContext_t*          Context         = (MjpegCodecDecodeContext_t *)DecodeContext;
    JPEGDECHW_VideoDecodeParams_t*      Param;
    JPEGDECHW_DecodedBufferAddress_t*   Decode;

    CODEC_DEBUG("%s\n", __FUNCTION__);

    // For Mjpeg we do not do slice decodes.
    KnownLastSliceInFieldFrame                  = true;

    Param                                       = &Context->DecodeParameters;
    Decode                                      = &Param->DecodedBufferAddr;

    Param->PictureStartAddr_p                   = (JPEGDECHW_CompressedData_t)CodedData;
    Param->PictureEndAddr_p                     = (JPEGDECHW_CompressedData_t)(CodedData + CodedDataLength + 128);

    Decode->Luma_p                              = (JPEGDECHW_LumaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    Decode->Chroma_p                            = (JPEGDECHW_ChromaAddress_t)BufferState[CurrentDecodeBufferIndex].BufferChromaPointer;
    Decode->LumaDecimated_p                     = NULL;
    Decode->ChromaDecimated_p                   = NULL;

    //{{{  Initialise decode buffers to bright pink
    #if 0
    MjpegFrameParameters_t*             Parsed          = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t*          PictureHeader   = &Parsed->PictureHeader;

    unsigned int        LumaSize        = PictureHeader->frame_width*PictureHeader->frame_heightDecodingHeight;
    unsigned char*      LumaBuffer;
    unsigned char*      ChromaBuffer;
    CurrentDecodeBuffer->ObtainDataReference( NULL, NULL, (void**)&LumaBuffer, UnCachedAddress);
    ChromaBuffer                        = LumaBuffer+LumaSize;
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, 0xff, LumaSize/2);
    #endif
    //}}}

    //{{{  Fill in remaining fields
    Param->MainAuxEnable                                        = JPEGDECHW_MAINOUT_EN;
    Param->HorizontalDecimationFactor                           = JPEGDECHW_HDEC_1;
    Param->VerticalDecimationFactor                             = JPEGDECHW_VDEC_1;

    Param->xvalue0                                              = 0;
    Param->xvalue1                                              = 0;
    Param->yvalue0                                              = 0;
    Param->yvalue1                                              = 0;
    Param->DecodingMode                                         = JPEGDECHW_NORMAL_DECODE;
    Param->AdditionalFlags                                      = JPEGDECHW_ADDITIONAL_FLAG_NONE;
    //}}}

    // Fillout the actual command
    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(JPEGDECHW_VideoDecodeReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.ParamSize                         = sizeof(Context->DecodeParameters);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(&Context->DecodeParameters);

    DecodeContext->MMECommand.NumberInputBuffers                = 0;
    DecodeContext->MMECommand.NumberOutputBuffers               = 0;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

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

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(       void )
{
    MjpegCodecDecodeContext_t*          Context                 = (MjpegCodecDecodeContext_t *)DecodeContext;
    MjpegFrameParameters_t*             Parsed                  = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t*          PictureHeader           = &Parsed->PictureHeader;
    JPEGDEC_TransformParams_t*          Param;
    JPEGDEC_InitParams_t*               Setup;

    CODEC_TRACE("%s\n", __FUNCTION__);

    Param                                                       = &Context->DecodeParameters;
    Setup                                                       = &Param->outputSettings;

    memset (Setup, 0, sizeof (JPEGDEC_InitParams_t));
    Setup->outputWidth                                          = PictureHeader->frame_width;
    Setup->outputHeight                                         = PictureHeader->frame_height;

    // Fillout the actual command
    memset( &DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t) );

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(Context->DecodeStatus);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = MJPEG_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = MJPEG_NUM_MME_OUTPUT_BUFFERS;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(JPEGDEC_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(Param);

    //{{{  Fill in details for all buffers
    for (int i = 0; i < MJPEG_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                         = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                 = sizeof (MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                 = NULL;
        DecodeContext->MMEBuffers[i].Flags                      = 0;
        DecodeContext->MMEBuffers[i].StreamNumber               = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages       = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p             = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                = 0;
    }
    //}}}
    //{{{  Then overwrite bits specific to other buffers
    DecodeContext->MMEBuffers[MJPEG_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p     = (void*)CodedData;
    DecodeContext->MMEBuffers[MJPEG_MME_CODED_DATA_BUFFER].TotalSize                    = CodedDataLength;
    DecodeContext->MMEBuffers[MJPEG_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p  = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    DecodeContext->MMEBuffers[MJPEG_MME_CURRENT_FRAME_BUFFER].TotalSize                 = Setup->outputWidth*Setup->outputHeight*2;
    //}}}
    //{{{  Initialise remaining scatter page values
    for (int i = 0; i < MJPEG_NUM_MME_BUFFERS; i++)
    {                                                                 // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }
    //}}}

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

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutDecodeCommand(       void )
{
    CODEC_TRACE("%s\n", __FUNCTION__);

    memset (&DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t));
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_SEND_BUFFERS;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NO_INFO;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.NumberInputBuffers                = 1;
    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    memset (&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];

    memset (&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
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

CodecStatus_t   Codec_MmeVideoMjpeg_c::FillOutTransformCommand(       void )
{
    MjpegCodecDecodeContext_t*          Context                 = (MjpegCodecDecodeContext_t *)DecodeContext;
    MjpegFrameParameters_t*             Parsed                  = (MjpegFrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    MjpegVideoPictureHeader_t*          PictureHeader           = &Parsed->PictureHeader;
    JPEGDEC_TransformParams_t*          Param;
    JPEGDEC_InitParams_t*               Setup;

    CODEC_TRACE("%s\n", __FUNCTION__);

    // Initialize the frame parameters
    memset (&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    // Zero the reply structure
    memset (&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    Param                                                       = &Context->DecodeParameters;
    Setup                                                       = &Param->outputSettings;

    memset (Setup, 0, sizeof (JPEGDEC_InitParams_t));
    Setup->outputWidth                                          = PictureHeader->frame_width;
    Setup->outputHeight                                         = PictureHeader->frame_height;

    // Fillout the actual command
    memset( &DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t) );
    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(Context->DecodeStatus);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t)(&Context->DecodeStatus);
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberOutputBuffers               = 1;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(JPEGDEC_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)(Param);

    memset (&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];

    memset (&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
    DecodeContext->MMEBuffers[0].TotalSize                      = Setup->outputWidth*Setup->outputHeight*2;
    DecodeContext->MMEPages[0].Page_p                           = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    DecodeContext->MMEPages[0].Size                             = DecodeContext->MMEBuffers[0].TotalSize;

    return CodecNoError;
}

CodecStatus_t   Codec_MmeVideoMjpeg_c::SendMMEDecodeCommand(  void )
{
    MME_ERROR           MMEStatus;

    MMECommandPreparedCount++;

    // Check that we have not commenced shutdown.
    if( TestComponentState(ComponentHalted) )
    {
        MMECommandAbortedCount++;
        return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //

#ifdef DUMP_COMMANDS
    DumpMMECommand( &DecodeContext->MMECommand );
#endif

    //
    // Perform the mme transaction - Note at this point we invalidate
    // our pointer to the context buffer, after the send we invalidate
    // out pointer to the context data.
    //

#ifdef __KERNEL__
    flush_cache_all();
#endif

    DecodeContextBuffer                 = NULL;
    DecodeContext->DecodeCommenceTime   = OS_GetTimeInMicroSeconds();

    //report (severity_error, "Sending actual MME Decode frame command %d\n",DecodeContext->MMECommand.CmdCode);

    MMEStatus                           = MME_SendCommand( MMEHandle, &DecodeContext->MMECommand );
    if (MMEStatus != MME_SUCCESS)
    {
        CODEC_ERROR ("(%s) - Unable to send decode command (%08x).\n", Configuration.CodecName, MMEStatus);
        return CodecError;
    }

    FillOutTransformCommand ();

    MMEStatus                           = MME_SendCommand (MMEHandle, &DecodeContext->MMECommand);
    if (MMEStatus != MME_SUCCESS)
    {
        CODEC_ERROR ("(%s) - Unable to send decode command (%08x).\n", Configuration.CodecName, MMEStatus);
        return CodecError;
    }


    DecodeContext       = NULL;

//

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
CodecStatus_t   Codec_MmeVideoMjpeg_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    MjpegCodecDecodeContext_t*          MjpegContext    = (MjpegCodecDecodeContext_t *)Context;

    CODEC_DEBUG("%s\n", __FUNCTION__);
#if defined (USE_JPEG_HW_TRANSFORMER)
    CODEC_DEBUG("    pm_cycles            %d\n", MjpegContext->DecodeStatus.pm_cycles);
    CODEC_DEBUG("    pm_dmiss             %d\n", MjpegContext->DecodeStatus.pm_dmiss);
    CODEC_DEBUG("    pm_imiss             %d\n", MjpegContext->DecodeStatus.pm_imiss);
    CODEC_DEBUG("    pm_bundles           %d\n", MjpegContext->DecodeStatus.pm_bundles);
    CODEC_DEBUG("    pm_pft               %d\n", MjpegContext->DecodeStatus.pm_pft);
    //CODEC_DEBUG("    DecodeTimeInMicros   %d\n", MjpegContext->DecodeStatus.DecodeTimeInMicros);
#else
    CODEC_TRACE("    bytes_written        %d\n", MjpegContext->DecodeStatus.bytes_written);
    CODEC_TRACE("    decodedImageHeight   %d\n", MjpegContext->DecodeStatus.decodedImageHeight);
    CODEC_TRACE("    decodedImageWidth    %d\n", MjpegContext->DecodeStatus.decodedImageWidth);
    CODEC_TRACE("    Total_cycle          %d\n", MjpegContext->DecodeStatus.Total_cycle);
    CODEC_TRACE("    DMiss_Cycle          %d\n", MjpegContext->DecodeStatus.DMiss_Cycle);
    CODEC_TRACE("    IMiss_Cycle          %d\n", MjpegContext->DecodeStatus.IMiss_Cycle);
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
CodecStatus_t   Codec_MmeVideoMjpeg_c::DumpSetStreamParameters(         void    *Parameters )
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
CodecStatus_t   Codec_MmeVideoMjpeg_c::DumpDecodeParameters(            void    *Parameters )
{
#if defined (USE_JPEG_HW_TRANSFORMER)
    JPEGDECHW_VideoDecodeParams_t*              FrameParams;

    FrameParams     = (JPEGDECHW_VideoDecodeParams_t *)Parameters;

    MjpegStaticPicture++;

    CODEC_TRACE("********** Picture %d *********\n", MjpegStaticPicture-1);

    return CodecNoError;
#endif
}
//}}}


