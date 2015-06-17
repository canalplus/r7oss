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

Source file name : codec_mme_video_h263.cpp
Author :           Mark C

Implementation of the H263 video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
20-May-08   Created                                         Julian

************************************************************************/
// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_video_h263.h"
#include "h263.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


//{{{  Locally defined structures
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

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoH263_c::Codec_MmeVideoH263_c( void )
{
        //printf( "Codec_MmeVideoH263_c::Codec_MmeVideoH263_c - decode context coming from system memory - needs fixing inbox\n" );

    Configuration.CodecName                             = "H263 video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_MacroBlock;

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
    Configuration.TransformCapabilityStructurePointer   = (void*)(&H263TransformerCapability);

    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    Configuration.AddressingMode                        = UnCachedAddress;

    //
    // We do not need the coded data after decode is complete
    //

    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;
//

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset 
//      are executed for all levels of the class.
//

Codec_MmeVideoH263_c::~Codec_MmeVideoH263_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for H263 specific members.
//      
//

CodecStatus_t   Codec_MmeVideoH263_c::Reset( void )
{
    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an H263 mme transformer.
//
CodecStatus_t   Codec_MmeVideoH263_c::HandleCapabilities( void )
{
    CODEC_TRACE("'%s' capabilities are :-\n", H263_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    caps len                          = %d bytes\n", H263TransformerCapability.caps_len);

    return CodecNoError;
}
//}}}
//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities 
//      structure for an H263 mme transformer.
//
CodecStatus_t   Codec_MmeVideoH263_c::FillOutTransformerInitializationParameters( void )
{

    report (severity_info, "Codec_MmeVideoH263_c::%s\n", __FUNCTION__);
    //
    // Fillout the command parameters
    //

    H263InitializationParameters.PictureWidhth                  = H263_WIDTH_CIF;
    H263InitializationParameters.PictureHeight                  = H263_HEIGHT_CIF;
    H263InitializationParameters.ErrorConceal_type              = MOTION_COMPENSATED;   // doesn't really matter as not currently supported
    //H263InitializationParameters.Post_processing                = 0;

    CODEC_TRACE("FillOutTransformerInitializationParameters\n");
    CODEC_TRACE("  PictureWidth              %6u\n", H263_WIDTH_CIF);
    CODEC_TRACE("  PictureHeight             %6u\n", H263_HEIGHT_CIF);

    //
    // Fillout the actual command
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
CodecStatus_t   Codec_MmeVideoH263_c::FillOutSetStreamParametersCommand( void )
{
    H263CodecStreamParameterContext_t*          Context = (H263CodecStreamParameterContext_t*)StreamParameterContext;
    H263StreamParameters_t*                     Parsed  = (H263StreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;

    report (severity_info, "Codec_MmeVideoH263_c::FillOutSetStreamParametersCommand (%dx%d)\n", Parsed->SequenceHeader.width, Parsed->SequenceHeader.height);

    //
    // Fillout the command parameters
    //

    Context->StreamParameters.PictureWidhth             = Parsed->SequenceHeader.width;
    Context->StreamParameters.PictureHeight             = Parsed->SequenceHeader.height;

    //
    // Fillout the actual command
    //

    memset( &Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t) );

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

CodecStatus_t   Codec_MmeVideoH263_c::FillOutDecodeCommand (void)
{
    H263CodecDecodeContext_t*           Context        = (H263CodecDecodeContext_t*)DecodeContext;
    H263FrameParameters_t*              Frame          = (H263FrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;
    //H263StreamParameters_t*             Stream         = (H263StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;

    H263D_TransformParams_t*            Param;
    unsigned int                        i;

    report (severity_info, "Codec_MmeVideoH263_c::%s\n", __FUNCTION__);

    //
    // For H263 we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                  = true;

    //
    // Fillout the straight forward command parameters
    //

    Param                                       = &Context->DecodeParameters;

    Param->Width                                = ParsedVideoParameters->Content.Width;
    Param->Height                               = ParsedVideoParameters->Content.Height;
    Param->FrameType                            = (H263PictureCodingType_t)Frame->PictureHeader.ptype;          // I/P


    //
    // Fillout the actual command
    //

    memset (&Context->BaseContext.MMECommand, 0x00, sizeof(MME_Command_t));

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(H263D_DecodeReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t*)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = H263_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = H263_NUM_MME_OUTPUT_BUFFERS;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(H263D_TransformParams_t);
    DecodeContext->MMECommand.Param_p                           = NULL;//(MME_GenericParams_t *)&ConcealmentParams;

    // Fill in details for all buffers
    for (i = 0; i < H263_NUM_MME_BUFFERS; i++)
    {
        DecodeContext->MMEBufferList[i]                             = &DecodeContext->MMEBuffers[i];
        DecodeContext->MMEBuffers[i].StructSize                     = sizeof (MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].UserData_p                     = NULL;
        DecodeContext->MMEBuffers[i].Flags                          = 0;
        DecodeContext->MMEBuffers[i].StreamNumber                   = 0;
        DecodeContext->MMEBuffers[i].NumberOfScatterPages           = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p                 = &DecodeContext->MMEPages[i];
        DecodeContext->MMEBuffers[i].TotalSize                      = ((ParsedVideoParameters->Content.Width * ParsedVideoParameters->Content.Height) * 3) /2;
        DecodeContext->MMEBuffers[i].StartOffset                    = 0;

        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p       = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size         = DecodeContext->MMEBuffers[i].TotalSize;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].BytesUsed    = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsIn      = 0;
        DecodeContext->MMEBuffers[i].ScatterPages_p[0].FlagsOut     = 0;

    }
    // Then overwrite bits specific to coded data buffer
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].TotalSize                      = CodedDataLength;
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p       = (void*)CodedData;
    DecodeContext->MMEBuffers[H263_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Size         = CodedDataLength;

    if (DecodeContext->ReferenceFrameList[0].EntryCount == 1)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];
        DecodeContext->MMEBuffers[H263_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p     = BufferState[Entry].BufferLumaPointer;
    }


#if 0

    for(i = 0; i < H263_NUM_MME_BUFFERS; i++)
    {
        report(severity_info, "Buffer List      (%d)  %x\n",i,DecodeContext->MMEBufferList[i]);
        report(severity_info, "Struct Size      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StructSize);
        report(severity_info, "User Data p      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].UserData_p);
        report(severity_info, "Flags            (%d)  %x\n",i,DecodeContext->MMEBuffers[i].Flags);
        report(severity_info, "Stream Number    (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StreamNumber);
        report(severity_info, "No Scatter Pages (%d)  %x\n",i,DecodeContext->MMEBuffers[i].NumberOfScatterPages);
        report(severity_info, "Scatter Pages p  (%d)  %x\n",i,DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
        report(severity_info, "Start Offset     (%d)  %x\n\n",i,DecodeContext->MMEBuffers[i].StartOffset);
    }

    report(severity_info, "Additional Size  %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize);
    report(severity_info, "Additional p     %x\n",DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p);
    report(severity_info, "Param Size       %x\n",DecodeContext->MMECommand.ParamSize);
    report(severity_info, "Param p          %x\n",DecodeContext->MMECommand.Param_p);
    report(severity_info, "DataBuffer p     %x\n",DecodeContext->MMECommand.DataBuffers_p);


#endif  

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
CodecStatus_t   Codec_MmeVideoH263_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
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
CodecStatus_t   Codec_MmeVideoH263_c::DumpSetStreamParameters(         void    *Parameters )
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
CodecStatus_t   Codec_MmeVideoH263_c::DumpDecodeParameters(            void    *Parameters )
{
    H263D_TransformParams_t*            FrameParams;

    FrameParams     = (H263D_TransformParams_t *)Parameters;

    H263StaticPicture++;

    report (severity_info, "********** Picture %d *********\n", H263StaticPicture-1);

    return CodecNoError;
}
//}}}

