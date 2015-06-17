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

Source file name : codec_mme_video_flv1.cpp
Author :           Julian

Implementation of the Sorenson H263 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-May-08   Created                                         Julian

************************************************************************/
// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_flv1.h"
#include "h263.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

//{{{  Locally defined structures
typedef struct Flv1CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} Flv1CodecStreamParameterContext_t;

typedef struct Flv1CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    FLV1_TransformParam_t               DecodeParameters;
    FLV1_ReturnParams_t                 DecodeStatus;
} Flv1CodecDecodeContext_t;

#define BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT               "Flv1CodecStreamParameterContext"
#define BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Flv1CodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   Flv1CodecStreamParameterContextDescriptor = BUFFER_FLV1_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_FLV1_CODEC_DECODE_CONTEXT        "Flv1CodecDecodeContext"
#define BUFFER_FLV1_CODEC_DECODE_CONTEXT_TYPE   {BUFFER_FLV1_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Flv1CodecDecodeContext_t)}
static BufferDataDescriptor_t                   Flv1CodecDecodeContextDescriptor        = BUFFER_FLV1_CODEC_DECODE_CONTEXT_TYPE;


//}}}

//{{{  C Callback stub
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C wrapper for the MME callback
//

typedef void (*MME_GenericCallback_t) (MME_Event_t Event, MME_Command_t * CallbackData, void *UserData);

static void MMECallbackStub(    MME_Event_t      Event,
                                MME_Command_t   *CallbackData,
                                void            *UserData )
{
Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;

    Self->CallbackFromMME( Event, CallbackData );
    return;
}


//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoFlv1_c::Codec_MmeVideoFlv1_c( void )
{
    Configuration.CodecName                             = "Flv1 video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_Planar;

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Flv1CodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Flv1CodecDecodeContextDescriptor;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;

    Configuration.TransformName[0]                      = FLV1_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = FLV1_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;

    // FLV1 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;

    Configuration.AddressingMode                        = UnCachedAddress;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    TranscodedFrameMemory[CachedAddress]                = NULL;
    TranscodedFrameMemory[UnCachedAddress]              = NULL;
    TranscodedFrameMemory[PhysicalAddress]              = NULL;

    // To support the Unrestricted Motion Vector feature of the flv1 codec the buffers must be allocated as follows
    // For macroblock and planar buffers
    // Luma         (width + 32) * (height + 32)
    // Chroma       2 * ((width / 2) + 32) * ((height / 2) + 32)    = Luma / 2
    // For raster buffers
    // Luma         (width + 32) * (height + 32)
    // Chroma       (width + 32) * (height + 32)
    MaxBytesPerFrame                                    = ((H263_WIDTH_CIF + 32) * (H263_HEIGHT_CIF + 32) * 3) / 2;

    DecodingWidth                                       = H263_WIDTH_CIF;
    DecodingHeight                                      = H263_HEIGHT_CIF;

    RestartTransformer                                  = true;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoFlv1_c::~Codec_MmeVideoFlv1_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for Flv1 specific members.
//      
//

CodecStatus_t   Codec_MmeVideoFlv1_c::Reset( void )
{
    if (TranscodedFrameMemory[CachedAddress] != NULL)
    {
        AllocatorClose (TranscodedFrameMemoryDevice);
        TranscodedFrameMemory[CachedAddress]            = NULL;
        TranscodedFrameMemory[UnCachedAddress]          = NULL;
        TranscodedFrameMemory[PhysicalAddress]          = NULL;
    }

    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  RegisterOutputBufferRing
///////////////////////////////////////////////////////////////////////////
///
///     Allocate the FLV1 Planar structure.
///
///     Due to the structure of the shared super-class, most codecs allocate
///     the majority of the resources when the player supplies it with an
///     output buffer.
///
CodecStatus_t   Codec_MmeVideoFlv1_c::RegisterOutputBufferRing (Ring_t  Ring)
{
    CodecStatus_t       Status;

    Status = Codec_MmeVideo_c::RegisterOutputBufferRing (Ring);
    if (Status != CodecNoError)
        return Status;

    // Create the buffer for the transformer to transcode into.
    allocator_status_t          AllocatorStatus;

    AllocatorStatus             = AllocatorOpen (&TranscodedFrameMemoryDevice, MaxBytesPerFrame, true);
    if (AllocatorStatus != allocator_ok)
    {
        CODEC_ERROR("Failed to allocate transcode buffer\n");
        return PlayerInsufficientMemory;
    }

    TranscodedFrameMemory[CachedAddress]        = AllocatorUserAddress         (TranscodedFrameMemoryDevice);
    TranscodedFrameMemory[UnCachedAddress]      = AllocatorUncachedUserAddress (TranscodedFrameMemoryDevice);
    TranscodedFrameMemory[PhysicalAddress]      = AllocatorPhysicalAddress     (TranscodedFrameMemoryDevice);

    TranscodedFrameLumaBuffer                   = (unsigned char*)TranscodedFrameMemory[UnCachedAddress];
    TranscodedFrameChromaBuffer                 = &TranscodedFrameLumaBuffer[MaxBytesPerFrame/2];

    return CodecNoError;
}


//}}}
//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::HandleCapabilities( void )
{
    CODEC_TRACE ("MME Transformer '%s' capabilities are :-\n", FLV1_MME_TRANSFORMER_NAME);

    return CodecNoError;
}
//}}}
//{{{  InitializeMMETransformer
///////////////////////////////////////////////////////////////////////////
///
/// Verify that the transformer exists
///
CodecStatus_t   Codec_MmeVideoFlv1_c::InitializeMMETransformer(      void )
{
    MME_ERROR                        MMEStatus;
    MME_TransformerCapability_t      Capability;

    memset( &Capability, 0, sizeof(MME_TransformerCapability_t) );
    memset( Configuration.TransformCapabilityStructurePointer, 0x00, Configuration.SizeOfTransformCapabilityStructure );

    Capability.StructSize           = sizeof(MME_TransformerCapability_t);
    Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
    Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;

    MMEStatus                       = MME_GetTransformerCapability( Configuration.TransformName[SelectedTransformer], &Capability );
    if( MMEStatus != MME_SUCCESS )
    {
        if (MMEStatus == MME_UNKNOWN_TRANSFORMER)
            CODEC_ERROR("(%s) - Transformer %s not found.\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer]);
        else
            CODEC_ERROR("(%s:%s) - Unable to read capabilities (%08x).\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer], MMEStatus );
        return CodecError;
    }

    return HandleCapabilities();
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutTransformerInitializationParameters( void )
{
    // Fillout the command parameters
    Flv1InitializationParameters.codec_id                       = FLV1;
    Flv1InitializationParameters.CodedWidth                     = DecodingWidth;
    Flv1InitializationParameters.CodedHeight                    = DecodingHeight;

    CODEC_TRACE("FillOutTransformerInitializationParameters\n");
    CODEC_TRACE("  DecodingWidth              %6u\n", DecodingWidth);
    CODEC_TRACE("  DecodingHeight             %6u\n", DecodingHeight);

    // Fillout the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(FLV1_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Flv1InitializationParameters);

    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoFlv1_c::SendMMEStreamParameters (void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;


    // There are no set stream parameters for Flv1 decoder so the transformer is
    // terminated and restarted when required (i.e. if width or height change).

    if (RestartTransformer)
    {
        TerminateMMETransformer();

        memset (&MMEInitializationParameters, 0x00, sizeof(MME_TransformerInitParams_t));

        MMEInitializationParameters.Priority                    = MME_PRIORITY_NORMAL;
        MMEInitializationParameters.StructSize                  = sizeof(MME_TransformerInitParams_t);
        MMEInitializationParameters.Callback                    = &MMECallbackStub;
        MMEInitializationParameters.CallbackUserData            = this;

        FillOutTransformerInitializationParameters ();

        MMEStatus               = MME_InitTransformer (Configuration.TransformName[SelectedTransformer],
                                                       &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
            CODEC_DEBUG ("New Stream Params %dx%d\n", DecodingWidth, DecodingHeight);
            CodecStatus                                         = CodecNoError;
            RestartTransformer                                  = false;
            ParsedFrameParameters->NewStreamParameters          = false;

            MMEInitialized                                      = true;
        }
    }

    //
    // The base class has very helpfully acquired a stream 
    // parameters context for us which we must release.
    // But only if everything went well, otherwise the callers
    // will helpfully release it as well (Nick).
    //

    if( CodecStatus == CodecNoError )
        StreamParameterContextBuffer->DecrementReferenceCount ();

//

    return CodecStatus;


}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Flv1 mme transformer.
//
CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutSetStreamParametersCommand( void )
{
    H263StreamParameters_t*                     Parsed  = (H263StreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;

    if ((Parsed->SequenceHeader.width != DecodingWidth) || (Parsed->SequenceHeader.height != DecodingHeight))
    {
        DecodingWidth           = Parsed->SequenceHeader.width;
        DecodingHeight          = Parsed->SequenceHeader.height;
        RestartTransformer      = true;
    }

    return CodecNoError;

}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Flv1 mme transformer.
//

CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutDecodeCommand(       void )
{
    Flv1CodecDecodeContext_t*           Context        = (Flv1CodecDecodeContext_t*)DecodeContext;
    H263FrameParameters_t*              Frame          = (H263FrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;
    //Flv1StreamParameters_t*             Stream         = (Flv1StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    FLV1_TransformParam_t*              Param;
    FLV1_ParamPicture_t*                Picture;
    unsigned int                        i;

    //
    // For Flv1 we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                  = true;

    //
    // Fillout the straight forward command parameters
    //

    Param                                       = &Context->DecodeParameters;
    Picture                                     = &Param->PictureParameters;

    Picture->StructSize                         = sizeof (FLV1_ParamPicture_t);
    Picture->EnableDeblocking                   = Frame->PictureHeader.dflag;
    Picture->PicType                            = (Frame->PictureHeader.ptype == H263_PICTURE_CODING_TYPE_I) ? FLV1_DECODE_PICTURE_TYPE_I :
                                                                                                               FLV1_DECODE_PICTURE_TYPE_P;
    Picture->H263Flv                            = Frame->PictureHeader.version + 1;             // Flv1 -> 2
    Picture->ChromaQscale                       = Frame->PictureHeader.pquant;
    Picture->Qscale                             = Frame->PictureHeader.pquant;
    Picture->Dropable                           = Frame->PictureHeader.ptype > H263_PICTURE_CODING_TYPE_P;
    Picture->index                              = Frame->PictureHeader.bit_offset;

    Picture->PictureStartOffset                 = 0;                                            // Picture starts at beginning of buffer
    Picture->PictureStopOffset                  = CodedDataLength + 32;
    Picture->size_in_bits                       = CodedDataLength * 8;

    //
    // Fillout the actual command
    //

    memset (&DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t));

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(FLV1_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t*)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = FLV1_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = FLV1_NUM_MME_OUTPUT_BUFFERS;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(FLV1_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < FLV1_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof (MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;
    }
    //}}}

    // Then overwrite bits specific to other buffers
    //{{{  Input compressed data buffer - need pointer to coded data and its length
    DecodeContext->MMEBuffers[FLV1_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p              = (void*)CodedData;
    DecodeContext->MMEBuffers[FLV1_MME_CODED_DATA_BUFFER].TotalSize                             = CodedDataLength;
    //}}}
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p           = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER].TotalSize                          = MaxBytesPerFrame;//((DecodingWidth+32)*(DecodingHeight+32)*3)/2;
    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 1)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];

        DecodeContext->MMEBuffers[FLV1_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = (void*)(void*)BufferState[Entry].BufferLumaPointer;
        DecodeContext->MMEBuffers[FLV1_MME_REFERENCE_FRAME_BUFFER].TotalSize                    = MaxBytesPerFrame;//((DecodingWidth+32)*(DecodingHeight+32)*3)/2;
    }
    //}}}
    //{{{  Current output macroblock buffer luma
    // Luma         (width + 32) * (height + 32)
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_LUMA].ScatterPages_p[0].Page_p      = TranscodedFrameLumaBuffer;
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_LUMA].TotalSize                     = DecodingWidth*DecodingHeight;
    //}}}
    //{{{  Current output macroblock buffer chroma
    // Chroma       2 * ((width / 2) + 32) * ((height / 2) + 32)    = Luma / 2
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA].ScatterPages_p[0].Page_p    = TranscodedFrameLumaBuffer;
    DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA].TotalSize                   = (DecodingWidth*DecodingHeight) / 2;
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < FLV1_NUM_MME_BUFFERS; i++)
    {                                                                 // Only one scatterpage, so size = totalsize
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;
    }
    //}}}

#if 0
    // Initialise decode buffers to bright pink
    unsigned int        LumaSize        = (DecodingWidth+32)*(DecodingHeight+32);
    unsigned char*      LumaBuffer      = (unsigned char*)DecodeContext->MMEBuffers[FLV1_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p;
    unsigned char*      ChromaBuffer    = &LumaBuffer[LumaSize];
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, 0xff, LumaSize/2);
#endif

#if 0
    for (i = 0; i < (FLV1_NUM_MME_BUFFERS); i++)
    {
        CODEC_DEBUG ("Buffer List      (%d)  %x\n",i,DecodeContext->MMEBufferList[i]);
        CODEC_DEBUG ("Struct Size      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StructSize);
        CODEC_DEBUG ("User Data p      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].UserData_p);
        CODEC_DEBUG ("Flags            (%d)  %x\n",i,DecodeContext->MMEBuffers[i].Flags);
        CODEC_DEBUG ("Stream Number    (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StreamNumber);
        CODEC_DEBUG ("No Scatter Pages (%d)  %x\n",i,DecodeContext->MMEBuffers[i].NumberOfScatterPages);
        CODEC_DEBUG ("Scatter Pages p  (%d)  %x\n",i,DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
        CODEC_DEBUG ("Start Offset     (%d)  %x\n\n",i,DecodeContext->MMEBuffers[i].StartOffset);
    }

    CODEC_DEBUG ("Additional Size  %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
    CODEC_DEBUG ("Additional p     %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
    CODEC_DEBUG ("Param Size       %x\n",DecodeContext->MMECommand.ParamSize);
    CODEC_DEBUG ("Param p          %x\n",DecodeContext->MMECommand.Param_p);
#endif

    return CodecNoError;

}
//}}}
//{{{  FillOutDecodeBufferRequest
// /////////////////////////////////////////////////////////////////////////
//
//      The specific video function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeVideoFlv1_c::FillOutDecodeBufferRequest(   BufferStructure_t        *Request )
{
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);

    Request->ComponentBorder[0]         = 16;
    Request->ComponentBorder[1]         = 16;

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
CodecStatus_t   Codec_MmeVideoFlv1_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
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
CodecStatus_t   Codec_MmeVideoFlv1_c::DumpSetStreamParameters(         void    *Parameters )
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

unsigned int Flv1StaticPicture;
CodecStatus_t   Codec_MmeVideoFlv1_c::DumpDecodeParameters(            void    *Parameters )
{
    FLV1_TransformParam_t*              FrameParams;

    FrameParams     = (FLV1_TransformParam_t *)Parameters;

    Flv1StaticPicture++;

    CODEC_DEBUG ("********** Picture %d *********\n", Flv1StaticPicture-1);

    return CodecNoError;
}
//}}}

