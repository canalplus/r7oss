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

Source file name : codec_mme_video_vp6.cpp
Author :           Julian

Implementation of the On2 VP6 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Jun-08   Created                                         Julian

************************************************************************/
// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_vp6.h"
#include "vp6.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

//{{{  Locally defined structures
typedef struct Vp6CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;
} Vp6CodecStreamParameterContext_t;

typedef struct Vp6CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;
    VP6_TransformParam_t                DecodeParameters;
    VP6_ReturnParams_t                  DecodeStatus;
} Vp6CodecDecodeContext_t;

#define BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT               "Vp6CodecStreamParameterContext"
#define BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT_TYPE {BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp6CodecStreamParameterContext_t)}
static BufferDataDescriptor_t                   Vp6CodecStreamParameterContextDescriptor = BUFFER_VP6_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VP6_CODEC_DECODE_CONTEXT         "Vp6CodecDecodeContext"
#define BUFFER_VP6_CODEC_DECODE_CONTEXT_TYPE    {BUFFER_VP6_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(Vp6CodecDecodeContext_t)}
static BufferDataDescriptor_t                   Vp6CodecDecodeContextDescriptor        = BUFFER_VP6_CODEC_DECODE_CONTEXT_TYPE;

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

Codec_MmeVideoVp6_c::Codec_MmeVideoVp6_c( void )
{
    Configuration.CodecName                             = "Vp6 video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_Planar;

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &Vp6CodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &Vp6CodecDecodeContextDescriptor;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = false;

    Configuration.TransformName[0]                      = VP6_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = VP6_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;

#if 0
    // VP6 codec does not properly implement MME_GetTransformerCapability() and any attempt
    // to do so results in a MME_NOT_IMPLEMENTED error. For this reason it is better not to
    // ask.
    Configuration.SizeOfTransformCapabilityStructure    = 0;
    Configuration.TransformCapabilityStructurePointer   = NULL;
#endif
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(VP6_CapabilityParams_t);
    Configuration.TransformCapabilityStructurePointer   = (void*)(&Vp6TransformCapability);

    Configuration.AddressingMode                        = UnCachedAddress;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

    // To support the Unrestricted Motion Vector feature of the Vp6 codec the buffers must be allocated as follows
    // For macroblock and planar buffers
    // Luma         (width + 32) * (height + 32)
    // Chroma       2 * ((width / 2) + 32) * ((height / 2) + 32)    = Luma / 2
    DecodingWidth                                       = VP6_DEFAULT_PICTURE_WIDTH;
    DecodingHeight                                      = VP6_DEFAULT_PICTURE_HEIGHT;

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

Codec_MmeVideoVp6_c::~Codec_MmeVideoVp6_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for Vp6 specific members.
//      
//

CodecStatus_t   Codec_MmeVideoVp6_c::Reset( void )
{
    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::HandleCapabilities( void )
{
    CODEC_TRACE ("MME Transformer '%s' capabilities are :-\n", VP6_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    display_buffer_size       %d\n", Vp6TransformCapability.display_buffer_size);
    CODEC_TRACE("    packed_buffer_size        %d\n", Vp6TransformCapability.packed_buffer_size);
    CODEC_TRACE("    packed_buffer_total       %d\n", Vp6TransformCapability.packed_buffer_total);

    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::FillOutTransformerInitializationParameters( void )
{
    // Fillout the command parameters
    Vp6InitializationParameters.CodedWidth                      = DecodingWidth;
    Vp6InitializationParameters.CodedHeight                     = DecodingHeight;
    Vp6InitializationParameters.CodecVersion                    = VP6f;

    CODEC_TRACE("FillOutTransformerInitializationParameters\n");
    CODEC_TRACE("  DecodingWidth              %6u\n", DecodingWidth);
    CODEC_TRACE("  DecodingHeight             %6u\n", DecodingHeight);

    // Fillout the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(VP6_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&Vp6InitializationParameters);

    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoVp6_c::SendMMEStreamParameters (void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;


    // There are no set stream parameters for Vp6 decoder so the transformer is
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
//      structure for an Vp6 mme transformer.
//
CodecStatus_t   Codec_MmeVideoVp6_c::FillOutSetStreamParametersCommand( void )
{
    Vp6StreamParameters_t*              Parsed  = (Vp6StreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;

    if ((Parsed->SequenceHeader.encoded_width != DecodingWidth) || (Parsed->SequenceHeader.encoded_height != DecodingHeight))
    {
        DecodingWidth           = Parsed->SequenceHeader.encoded_width;
        DecodingHeight          = Parsed->SequenceHeader.encoded_height;
        RestartTransformer      = true;
    }

    return CodecNoError;

}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Vp6 mme transformer.
//

CodecStatus_t   Codec_MmeVideoVp6_c::FillOutDecodeCommand(       void )
{
    Vp6CodecDecodeContext_t*            Context        = (Vp6CodecDecodeContext_t*)DecodeContext;
    Vp6FrameParameters_t*               Frame          = (Vp6FrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;
    Vp6StreamParameters_t*              Stream         = (Vp6StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    VP6_TransformParam_t*               Param;
    VP6_ParamPicture_t*                 Picture;
    unsigned int                        i;

    //
    // For Vp6 we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                  = true;

    //
    // Fillout the straight forward command parameters
    //

    Param                                       = &Context->DecodeParameters;
    Picture                                     = &Param->PictureParameters;

    Picture->StructSize                         = sizeof (VP6_ParamPicture_t);
    Picture->KeyFrame                           = Frame->PictureHeader.ptype == VP6_PICTURE_CODING_TYPE_I;
    Picture->SampleVarianceThreshold            = Stream->SequenceHeader.variance_threshold;
    Picture->Deblock_Filtering                  = Frame->PictureHeader.deblock_filtering;
    Picture->Quant                              = Frame->PictureHeader.pquant;
    Picture->MaxVectorLength                    = Stream->SequenceHeader.max_vector_length;
    Picture->FilterMode                         = Stream->SequenceHeader.filter_mode;
    Picture->FilterSelection                    = Stream->SequenceHeader.filter_selection;

    // The following three items are retrieved directly from the range decoder in the frame parser
    Picture->high                               = Frame->PictureHeader.high;
    Picture->bits                               = Frame->PictureHeader.bits;
    Picture->code_word                          = Frame->PictureHeader.code_word;

    Picture->PictureStartOffset                 = Frame->PictureHeader.offset;
    Picture->PictureStopOffset                  = CodedDataLength;

    //
    // Fillout the actual command
    //

    memset (&DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t));

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(VP6_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t*)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = VP6_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = VP6_NUM_MME_OUTPUT_BUFFERS;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(VP6_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < VP6_NUM_MME_BUFFERS; i++)
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
    DecodeContext->MMEBuffers[VP6_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p              = (void*)CodedData;
    DecodeContext->MMEBuffers[VP6_MME_CODED_DATA_BUFFER].TotalSize                             = CodedDataLength;
    //}}}
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p           = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER].TotalSize                          = ((DecodingWidth+32)*(DecodingHeight+32)*3)/2;
    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];

        DecodeContext->MMEBuffers[VP6_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = (void*)(void*)BufferState[Entry].BufferLumaPointer;
        DecodeContext->MMEBuffers[VP6_MME_REFERENCE_FRAME_BUFFER].TotalSize                    = ((DecodingWidth+32)*(DecodingHeight+32)*3)/2;
    }
    //}}}
    //{{{  Golden frame decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 2)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];

        DecodeContext->MMEBuffers[VP6_MME_GOLDEN_FRAME_BUFFER].ScatterPages_p[0].Page_p         = (void*)(void*)BufferState[Entry].BufferLumaPointer;
        DecodeContext->MMEBuffers[VP6_MME_GOLDEN_FRAME_BUFFER].TotalSize                        = ((DecodingWidth+32)*(DecodingHeight+32)*3)/2;
    }
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < VP6_NUM_MME_BUFFERS; i++)
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
    unsigned char*      LumaBuffer      = (unsigned char*)DecodeContext->MMEBuffers[VP6_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p;
    unsigned char*      ChromaBuffer    = &LumaBuffer[LumaSize];
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, 0xff, LumaSize/2);
#endif

#if 0
    for (i = 0; i < (VP6_NUM_MME_BUFFERS); i++)
    {
        CODEC_TRACE ("Buffer List      (%d)  %x\n",i,DecodeContext->MMEBufferList[i]);
        CODEC_TRACE ("Struct Size      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StructSize);
        CODEC_TRACE ("User Data p      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].UserData_p);
        CODEC_TRACE ("Flags            (%d)  %x\n",i,DecodeContext->MMEBuffers[i].Flags);
        CODEC_TRACE ("Stream Number    (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StreamNumber);
        CODEC_TRACE ("No Scatter Pages (%d)  %x\n",i,DecodeContext->MMEBuffers[i].NumberOfScatterPages);
        CODEC_TRACE ("Scatter Pages p  (%d)  %x\n",i,DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
        CODEC_TRACE ("Start Offset     (%d)  %x\n\n",i,DecodeContext->MMEBuffers[i].StartOffset);
    }

    CODEC_TRACE ("Additional Size  %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
    CODEC_TRACE ("Additional p     %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
    CODEC_TRACE ("Param Size       %x\n",DecodeContext->MMECommand.ParamSize);
    CODEC_TRACE ("Param p          %x\n",DecodeContext->MMECommand.Param_p);
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

CodecStatus_t   Codec_MmeVideoVp6_c::FillOutDecodeBufferRequest(   BufferStructure_t        *Request )
{
    Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);

    Request->ComponentBorder[0]         = 32;
    Request->ComponentBorder[1]         = 32;

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
CodecStatus_t   Codec_MmeVideoVp6_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
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
CodecStatus_t   Codec_MmeVideoVp6_c::DumpSetStreamParameters(         void    *Parameters )
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

unsigned int Vp6StaticPicture;
CodecStatus_t   Codec_MmeVideoVp6_c::DumpDecodeParameters(            void    *Parameters )
{
    VP6_TransformParam_t*               FrameParams;

    FrameParams      = (VP6_TransformParam_t *)Parameters;

    CODEC_DEBUG ("********** Picture %d *********\n", Vp6StaticPicture);

    Vp6StaticPicture++;

    return CodecNoError;
}
//}}}

