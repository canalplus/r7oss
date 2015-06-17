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

Source file name : codec_mme_video_theora.cpp
Author :           Julian

Implementation of the Ogg Theora video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
10-Mar-08   Created                                         Julian

************************************************************************/
// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "theora.h"
#include "codec_mme_video_theora.h"
#include "allocinline.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

//{{{  Locally defined structures
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

    CODEC_DEBUG("%s\n",__FUNCTION__);

    Self->CallbackFromMME( Event, CallbackData );
    return;
}


//}}}

//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Cosntructor function, fills in the codec specific parameter values
//

Codec_MmeVideoTheora_c::Codec_MmeVideoTheora_c( void )
{
    Configuration.CodecName                             = "Theora video";

    Configuration.DecodeOutputFormat                    = FormatVideo420_Planar;

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

    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    //Configuration.AddressingMode                        = PhysicalAddress;
    Configuration.AddressingMode                        = UnCachedAddress;

    // We do not need the coded data after decode is complete
    Configuration.ShrinkCodedDataBuffersAfterDecode     = true;

#if (THEORADEC_MME_VERSION < 20)
    TheoraInitializationParameters.CodedWidth           = THEORA_DEFAULT_PICTURE_WIDTH;
    TheoraInitializationParameters.CodedHeight          = THEORA_DEFAULT_PICTURE_HEIGHT;
    TheoraInitializationParameters.CodecVersion         = THEORA_DEFAULT_CODEC_VERSION;
    TheoraInitializationParameters.theora_tables        = 0;
#else
    CodedWidth                                          = THEORA_DEFAULT_PICTURE_WIDTH;
    CodedHeight                                         = THEORA_DEFAULT_PICTURE_HEIGHT;
    InfoHeaderMemoryDevice                              = NULL;
    CommentHeaderMemoryDevice                           = NULL;
    SetupHeaderMemoryDevice                             = NULL;
    BufferMemoryDevice                                  = NULL;
#endif

    RestartTransformer                                  = false;

    Reset();
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset
//      are executed for all levels of the class.
//

Codec_MmeVideoTheora_c::~Codec_MmeVideoTheora_c( void )
{
    Halt();
    Reset();
}
//}}}
//{{{  Reset
// /////////////////////////////////////////////////////////////////////////
//
//      Reset function for Theora specific members.
//      
//

CodecStatus_t   Codec_MmeVideoTheora_c::Reset( void )
{
    if (InfoHeaderMemoryDevice != NULL)
    {
        AllocatorClose (InfoHeaderMemoryDevice);
        InfoHeaderMemoryDevice          = NULL;
    }
    if (CommentHeaderMemoryDevice != NULL)
    {
        AllocatorClose (CommentHeaderMemoryDevice);
        CommentHeaderMemoryDevice       = NULL;
    }
    if (SetupHeaderMemoryDevice != NULL)
    {
        AllocatorClose (SetupHeaderMemoryDevice);
        SetupHeaderMemoryDevice         = NULL;
    }
    if (BufferMemoryDevice != NULL)
    {
        AllocatorClose (BufferMemoryDevice);
        BufferMemoryDevice              = NULL;
    }

    return Codec_MmeVideo_c::Reset();
}
//}}}

//{{{  HandleCapabilities
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::HandleCapabilities( void )
{
    CODEC_TRACE ("MME Transformer '%s' capabilities are :-\n", THEORADEC_MME_TRANSFORMER_NAME);
    CODEC_TRACE("    display_buffer_size       %d\n", TheoraTransformCapability.display_buffer_size);
    CODEC_TRACE("    packed_buffer_size        %d\n", TheoraTransformCapability.packed_buffer_size);
    CODEC_TRACE("    packed_buffer_total       %d\n", TheoraTransformCapability.packed_buffer_total);

    return CodecNoError;
}
//}}}
//{{{  InitializeMMETransformer
///////////////////////////////////////////////////////////////////////////
///
/// Verify that the transformer exists
///
CodecStatus_t   Codec_MmeVideoTheora_c::InitializeMMETransformer(      void )
{
    MME_ERROR                        MMEStatus;
    MME_TransformerCapability_t      Capability;

    memset( &Capability, 0, sizeof(MME_TransformerCapability_t) );
    memset( Configuration.TransformCapabilityStructurePointer, 0x00, Configuration.SizeOfTransformCapabilityStructure );

    Capability.StructSize           = sizeof(MME_TransformerCapability_t);
    Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
    Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;

    MMEStatus                       = MME_GetTransformerCapability( Configuration.TransformName[SelectedTransformer], &Capability );
    if (MMEStatus != MME_SUCCESS)
    {
        if (MMEStatus == MME_UNKNOWN_TRANSFORMER)
            CODEC_ERROR("(%s) - Transformer %s not found.\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer]);
        else
            CODEC_ERROR("(%s:%s) - Unable to read capabilities (%08x).\n", Configuration.CodecName, Configuration.TransformName[SelectedTransformer], MMEStatus);
        return CodecError;
    }

    return HandleCapabilities ();
}
//}}}

//{{{  FillOutTransformerInitializationParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::FillOutTransformerInitializationParameters( void )
{
    // The command parameters

    CODEC_TRACE ("%s\n", __FUNCTION__);
    CODEC_TRACE("  CodedWidth           %6u\n", CodedWidth);
    CODEC_TRACE("  CodedHeight          %6u\n", CodedHeight);
#if (THEORADEC_MME_VERSION < 20)
    CODEC_TRACE("  CodecVersion         %6x\n", TheoraInitializationParameters.CodecVersion);
#endif

    // Fillout the actual command
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(THEORA_InitTransformerParam_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&TheoraInitializationParameters);


    return CodecNoError;
}
//}}}
//{{{  SendMMEStreamParameters
CodecStatus_t Codec_MmeVideoTheora_c::SendMMEStreamParameters (void)
{
    CodecStatus_t       CodecStatus     = CodecNoError;
    unsigned int        MMEStatus       = MME_SUCCESS;

    CODEC_TRACE("%s\n", __FUNCTION__);

    // There are no set stream parameters for Rmv decoder so the transformer is
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

        CODEC_TRACE("%s: Initilising Theora transformer %s\n",__FUNCTION__, Configuration.TransformName[SelectedTransformer]);

        MMEStatus               = MME_InitTransformer (Configuration.TransformName[SelectedTransformer],
                                                       &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus ==  MME_SUCCESS)
        {
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

    if (CodecStatus == CodecNoError)
        StreamParameterContextBuffer->DecrementReferenceCount ();

//

    return CodecStatus;


}
//}}}
//{{{  FillOutSetStreamParametersCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an Theora mme transformer.
//
CodecStatus_t   Codec_MmeVideoTheora_c::FillOutSetStreamParametersCommand( void )
{

    TheoraStreamParameters_t*   Parsed                  = (TheoraStreamParameters_t*)ParsedFrameParameters->StreamParameterStructure;
    TheoraVideoSequence_t*      SequenceHeader          = &Parsed->SequenceHeader;

    CODEC_DEBUG("%s\n", __FUNCTION__);

#if (THEORADEC_MME_VERSION < 20)

    CodedWidth                                          = SequenceHeader->DecodedWidth;
    CodedHeight                                         = SequenceHeader->DecodedHeight;

    TheoraInitializationParameters.CodedWidth           = SequenceHeader->DecodedWidth;
    TheoraInitializationParameters.CodedHeight          = SequenceHeader->DecodedHeight;
    TheoraInitializationParameters.CodecVersion         = SequenceHeader->Version;

    TheoraInitializationParameters.theora_tables        = 1;
    memcpy (TheoraInitializationParameters.filter_limit_values,    SequenceHeader->LoopFilterLimit, sizeof(TheoraInitializationParameters.filter_limit_values));
    memcpy (TheoraInitializationParameters.coded_ac_scale_factor,  SequenceHeader->AcScale,         sizeof(TheoraInitializationParameters.coded_ac_scale_factor));
    memcpy (TheoraInitializationParameters.coded_dc_scale_factor,  SequenceHeader->DcScale,         sizeof(TheoraInitializationParameters.coded_ac_scale_factor));

    memcpy (TheoraInitializationParameters.base_matrix,            SequenceHeader->BaseMatrix,      sizeof(TheoraInitializationParameters.base_matrix));
    memcpy (TheoraInitializationParameters.qr_count,               SequenceHeader->QRCount,         sizeof(TheoraInitializationParameters.qr_count));
    memcpy (TheoraInitializationParameters.qr_size,                SequenceHeader->QRSize,          sizeof(TheoraInitializationParameters.qr_size));
    memcpy (TheoraInitializationParameters.qr_base,                SequenceHeader->QRBase,          sizeof(TheoraInitializationParameters.qr_base));

    TheoraInitializationParameters.hti                  = SequenceHeader->hti;
    TheoraInitializationParameters.hbits                = SequenceHeader->HBits;
    TheoraInitializationParameters.entries              = SequenceHeader->Entries;
    TheoraInitializationParameters.huff_code_size       = SequenceHeader->HuffmanCodeSize;
    memcpy (TheoraInitializationParameters.huffman_table,          SequenceHeader->HuffmanTable,    sizeof(TheoraInitializationParameters.huffman_table));

    //{{{  Trace
    #if 0
    {
        int qi, bmi, qti, pli, ci;
        report (severity_info, "Filter Limit values:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%02x ", TheoraInitializationParameters.filter_limit_values[qi]);
            if (((qi+1)&0x1f)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nAC Scale:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%08x ", TheoraInitializationParameters.coded_ac_scale_factor[qi]);
            if (((qi+1)&0x07)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nDC Scale:\n");
        for (qi=0; qi<64; qi++)
        {
            report (severity_info, "%04x ", TheoraInitializationParameters.coded_dc_scale_factor[qi]);
            if (((qi+1)&0x0f)== 0)
                report (severity_info, "\n");
        }
        report (severity_info, "\nBm Indexes %d\n", 3);
        for (bmi=0; bmi<3; bmi++)
        {
            report (severity_info, "%d:\n", bmi);
            for (ci=0; ci<64; ci++)
            {
                report (severity_info, "%02x ", TheoraInitializationParameters.base_matrix[bmi][ci]);
                if (((ci+1)&0x1f)== 0)
                    report (severity_info, "\n");
            }
        }
        report (severity_info, "\nQR Counts\n");
        for (qti=0; qti<=1; qti++)
        {
            report (severity_info, "%d:\n", qti);
            for (pli=0; pli<=2; pli++)
                report (severity_info, "%02x ", TheoraInitializationParameters.qr_count[qti][pli]);
        }
                if (((ci+1)&0x1f)== 0)
                    report (severity_info, "\n");
            {
            }
        report (severity_info, "\nQR Size\n");
        for (qti=0; qti<=1; qti++)
        {
            for (pli=0; pli<=2; pli++)
            {
                report (severity_info, "%d:%d:\n", qti, pli);
                for (qi=0; qi<64; qi++)
                {
                    report (severity_info, "%02x ", TheoraInitializationParameters.qr_size[qti][pli][qi]);
                    if (((qi+1)&0x1f)== 0)
                        report (severity_info, "\n");
                }
            }
        }
        report (severity_info, "\nQR Base\n");
        for (qti=0; qti<=1; qti++)
        {
            for (pli=0; pli<=2; pli++)
            {
                report (severity_info, "%d:%d:\n", qti, pli);
                for (qi=0; qi<64; qi++)
                {
                    report (severity_info, "%04x ", TheoraInitializationParameters.qr_base[qti][pli][qi]);
                    if (((qi+1)&0x0f)== 0)
                        report (severity_info, "\n");
                }
            }
        }
        report (severity_info, "\nHuffman table hti %d, hbits %d, entries %d, huffman_code_size %d\n", TheoraInitializationParameters.hti,
                 TheoraInitializationParameters.hbits, TheoraInitializationParameters.entries, TheoraInitializationParameters.huff_code_size);
        for (qti=0; qti<80; qti++)
        {
            report (severity_info, "%d:\n", qti);
            for (pli=0; pli<32; pli++)
            {
                report (severity_info, "(%04x %04x)", TheoraInitializationParameters.huffman_table[qti][pli][0],TheoraInitializationParameters.huffman_table[qti][pli][1]);
                if (((pli+1)&0x07)== 0)
                    report (severity_info, "\n");
            }
            report (severity_info, "\n");
        }
    }
    #endif
    //}}}

#else
    {
        allocator_status_t      AStatus;

        CodedWidth                                      = SequenceHeader->DecodedWidth;
        CodedHeight                                     = SequenceHeader->DecodedHeight;

        if (SequenceHeader->PixelFormat == THEORA_PIXEL_FORMAT_422)
        {
            CODEC_TRACE("Switching to 422 planar format\n");
            Configuration.DecodeOutputFormat            = FormatVideo422_Planar;
        }
        else
            Configuration.DecodeOutputFormat            = FormatVideo420_Planar;        // Default to 420 for now

        if (InfoHeaderMemoryDevice != NULL)
        {
            AllocatorClose (InfoHeaderMemoryDevice);
            InfoHeaderMemoryDevice                      = NULL;
        }
        if (CommentHeaderMemoryDevice != NULL)
        {
            AllocatorClose (CommentHeaderMemoryDevice);
            CommentHeaderMemoryDevice                   = NULL;
        }
        if (SetupHeaderMemoryDevice != NULL)
        {
            AllocatorClose (SetupHeaderMemoryDevice);
            SetupHeaderMemoryDevice                     = NULL;
        }

        if (BufferMemoryDevice != NULL)
        {
            AllocatorClose (BufferMemoryDevice);
            BufferMemoryDevice                          = NULL;
        }

        CODEC_TRACE("Allocating %d bytes for info header:\n", SequenceHeader->InfoHeaderSize);
        AStatus                                         = LmiAllocatorOpen (&InfoHeaderMemoryDevice, SequenceHeader->InfoHeaderSize, true);
        if( AStatus != allocator_ok )
        {
            CODEC_ERROR ("Failed to allocate info header memory\n" );
            return CodecError;
        }
        CODEC_TRACE("Allocating %d bytes for comment header\n", SequenceHeader->CommentHeaderSize);
        AStatus                                         = LmiAllocatorOpen (&CommentHeaderMemoryDevice, SequenceHeader->CommentHeaderSize, true);
        if( AStatus != allocator_ok )
        {
            CODEC_ERROR ("Failed to allocate comment header memory\n" );
            return CodecError;
        }
        CODEC_TRACE("Allocating %d bytes for setup header\n", SequenceHeader->SetupHeaderSize);
        AStatus                                         = LmiAllocatorOpen (&SetupHeaderMemoryDevice, SequenceHeader->SetupHeaderSize, true);
        if( AStatus != allocator_ok )
        {
            CODEC_ERROR ("Failed to allocate setup header memory\n" );
            return CodecError;
        }

        TheoraInitializationParameters.InfoHeader.Data          = (U32)AllocatorPhysicalAddress (InfoHeaderMemoryDevice);
        TheoraInitializationParameters.InfoHeader.Size          = SequenceHeader->InfoHeaderSize;;
        memcpy (AllocatorUncachedUserAddress (InfoHeaderMemoryDevice), SequenceHeader->InfoHeaderBuffer, SequenceHeader->InfoHeaderSize);

        TheoraInitializationParameters.CommentHeader.Data       = (U32)AllocatorPhysicalAddress (CommentHeaderMemoryDevice);
        TheoraInitializationParameters.CommentHeader.Size       = SequenceHeader->CommentHeaderSize;;
        memcpy (AllocatorUncachedUserAddress (CommentHeaderMemoryDevice), SequenceHeader->CommentHeaderBuffer, SequenceHeader->CommentHeaderSize);

        TheoraInitializationParameters.SetUpHeader.Data         = (U32)AllocatorPhysicalAddress (SetupHeaderMemoryDevice);
        TheoraInitializationParameters.SetUpHeader.Size         = SequenceHeader->SetupHeaderSize;;
        memcpy (AllocatorUncachedUserAddress (SetupHeaderMemoryDevice), SequenceHeader->SetupHeaderBuffer, SequenceHeader->SetupHeaderSize);

#if (THEORADEC_MME_VERSION >= 30)
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

            CODEC_TRACE("Allocating %d bytes for buffer memory\n", CoefficientBufferSize);
            AStatus                                         = LmiAllocatorOpen (&BufferMemoryDevice, CoefficientBufferSize, true);
            if (AStatus != allocator_ok)
            {
                CODEC_ERROR ("Failed to allocate buffer memory\n" );
                return CodecError;
            }
            TheoraInitializationParameters.CoefficientBuffer        = (U32)AllocatorPhysicalAddress (BufferMemoryDevice);
        }
#else
        CODEC_TRACE("Allocating %d bytes for buffer memory\n", THEORA_BUFFER_SIZE);
        AStatus                                         = LmiAllocatorOpen (&BufferMemoryDevice, THEORA_BUFFER_SIZE, true);
        if( AStatus != allocator_ok )
        {
            CODEC_ERROR ("Failed to allocate buffer memory\n" );
            return CodecError;
        }
        TheoraInitializationParameters.Buffer.Data              = (U32)AllocatorPhysicalAddress (BufferMemoryDevice);
        TheoraInitializationParameters.Buffer.Size              = THEORA_BUFFER_SIZE;
#endif
#if 0
        unsigned char*  Buff;
        Buff    = (unsigned char*)AllocatorUncachedUserAddress (InfoHeaderMemoryDevice);
        for (int i=0;i<32; i++)
            report (severity_info, "%02x ", Buff[i]);
        report (severity_info, "\n");
        Buff    = (unsigned char*)AllocatorUncachedUserAddress (CommentHeaderMemoryDevice);
        for (int i=0;i<32; i++)
            report (severity_info, "%02x ", Buff[i]);
        report (severity_info, "\n");
        Buff    = (unsigned char*)AllocatorUncachedUserAddress (SetupHeaderMemoryDevice);
        for (int i=0;i<32; i++)
            report (severity_info, "%02x ", Buff[i]);
        report (severity_info, "\n");
#endif
    }
#endif

    RestartTransformer                                  = true;

    return CodecNoError;

}
//}}}
//{{{  FillOutDecodeCommand
// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an Theora mme transformer.
//

CodecStatus_t   Codec_MmeVideoTheora_c::FillOutDecodeCommand(       void )
{
    TheoraCodecDecodeContext_t*         Context        = (TheoraCodecDecodeContext_t*)DecodeContext;
    //TheoraFrameParameters_t*            Frame          = (TheoraFrameParameters_t*)ParsedFrameParameters->FrameParameterStructure;
    //TheoraVideoSequence_t*              SequenceHeader = &Stream->SequenceHeader;

    THEORA_TransformParam_t*            Param;
    THEORA_ParamPicture_t*              Picture;
    unsigned int                        i;

    CODEC_DEBUG("%s:%d (%dx%d)\n", __FUNCTION__, CodedDataLength, CodedWidth, CodedHeight);
    //
    // For Theora we do not do slice decodes.
    //

    KnownLastSliceInFieldFrame                  = true;

    //
    // Fillout the straight forward command parameters
    //

    Param                                       = &Context->DecodeParameters;
    Picture                                     = &Param->PictureParameters;

    Picture->StructSize                         = sizeof (THEORA_ParamPicture_t);
    Picture->PictureStartOffset                 = 0;                    // Picture starts at beginning of buffer
    Picture->PictureStopOffset                  = CodedDataLength;

    //
    // Fillout the actual command
    //

    memset (&DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t));

    DecodeContext->MMECommand.CmdStatus.AdditionalInfoSize      = sizeof(THEORA_ReturnParams_t);
    DecodeContext->MMECommand.CmdStatus.AdditionalInfo_p        = (MME_GenericParams_t*)&Context->DecodeStatus;
    DecodeContext->MMECommand.StructSize                        = sizeof (MME_Command_t);
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = (MME_Time_t)0;
    DecodeContext->MMECommand.NumberInputBuffers                = THEORA_NUM_MME_INPUT_BUFFERS;
    DecodeContext->MMECommand.NumberOutputBuffers               = THEORA_NUM_MME_OUTPUT_BUFFERS;

    DecodeContext->MMECommand.DataBuffers_p                     = (MME_DataBuffer_t**)DecodeContext->MMEBufferList;

    DecodeContext->MMECommand.ParamSize                         = sizeof(THEORA_TransformParam_t);
    DecodeContext->MMECommand.Param_p                           = (MME_GenericParams_t)Param;

    //{{{  Fill in details for all buffers
    for (i = 0; i < THEORA_NUM_MME_BUFFERS; i++)
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
    DecodeContext->MMEBuffers[THEORA_MME_CODED_DATA_BUFFER].ScatterPages_p[0].Page_p            = (void*)CodedData;
    DecodeContext->MMEBuffers[THEORA_MME_CODED_DATA_BUFFER].TotalSize                           = CodedDataLength;
    #if 0
    report (severity_info, "Picture (%d)\n", CodedDataLength);
    extern "C"{void flush_cache_all();};
    flush_cache_all();
    for (i=0; i<32; i++)
        report (severity_info, "%02x ", CodedData[i]);
    report (severity_info, "\n");
    #endif
    //}}}
    //{{{  Current output decoded buffer
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p         = (void*)(void*)BufferState[CurrentDecodeBufferIndex].BufferLumaPointer;
    DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER].TotalSize                        = (CodedWidth * CodedHeight * 3)/2;
    //}}}
    //{{{  Previous decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount > 0)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[0];

        DecodeContext->MMEBuffers[THEORA_MME_REFERENCE_FRAME_BUFFER].ScatterPages_p[0].Page_p   = (void*)(void*)BufferState[Entry].BufferLumaPointer;
        DecodeContext->MMEBuffers[THEORA_MME_REFERENCE_FRAME_BUFFER].TotalSize                  = (CodedWidth * CodedHeight * 3)/2;
    }
    //}}}
    //{{{  Golden frame decoded buffer - get its planar buffer
    if (DecodeContext->ReferenceFrameList[0].EntryCount == 2)
    {
        unsigned int    Entry   = DecodeContext->ReferenceFrameList[0].EntryIndicies[1];

        DecodeContext->MMEBuffers[THEORA_MME_GOLDEN_FRAME_BUFFER].ScatterPages_p[0].Page_p      = (void*)(void*)BufferState[Entry].BufferLumaPointer;
        DecodeContext->MMEBuffers[THEORA_MME_GOLDEN_FRAME_BUFFER].TotalSize                     = (CodedWidth * CodedHeight * 3)/2;
    }
    //}}}

    //{{{  Initialise remaining scatter page values
    for (i = 0; i < THEORA_NUM_MME_BUFFERS; i++)
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
    unsigned char*      LumaBuffer      = (unsigned char*)DecodeContext->MMEBuffers[THEORA_MME_CURRENT_FRAME_BUFFER].ScatterPages_p[0].Page_p;
    unsigned char*      ChromaBuffer    = &LumaBuffer[LumaSize];
    memset (LumaBuffer,   0xff, LumaSize);
    memset (ChromaBuffer, 0xff, LumaSize/2);
#endif

#if 0
    for (i = 0; i < (THEORA_NUM_MME_BUFFERS); i++)
    {
#if 0
        CODEC_TRACE ("Buffer List      (%d)  %x\n",i,DecodeContext->MMEBufferList[i]);
        CODEC_TRACE ("Struct Size      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StructSize);
        CODEC_TRACE ("User Data p      (%d)  %x\n",i,DecodeContext->MMEBuffers[i].UserData_p);
        CODEC_TRACE ("Flags            (%d)  %x\n",i,DecodeContext->MMEBuffers[i].Flags);
        CODEC_TRACE ("Stream Number    (%d)  %x\n",i,DecodeContext->MMEBuffers[i].StreamNumber);
        CODEC_TRACE ("No Scatter Pages (%d)  %x\n",i,DecodeContext->MMEBuffers[i].NumberOfScatterPages);
#endif
        CODEC_TRACE ("Scatter Pages p  (%d)  %x\n",i,DecodeContext->MMEBuffers[i].ScatterPages_p[0].Page_p);
        CODEC_TRACE ("Start Offset     (%d)  %d\n",i,DecodeContext->MMEBuffers[i].StartOffset);
        CODEC_TRACE ("Size             (%d)  %d\n\n",i,DecodeContext->MMEBuffers[i].ScatterPages_p[0].Size);
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

CodecStatus_t   Codec_MmeVideoTheora_c::FillOutDecodeBufferRequest(   BufferStructure_t        *Request )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);

    //Request->Format                     = Configuration.DecodeOutputFormat;
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
CodecStatus_t   Codec_MmeVideoTheora_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    CODEC_DEBUG("%s\n", __FUNCTION__);
    return CodecNoError;
}
//}}}
//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//
CodecStatus_t   Codec_MmeVideoTheora_c::DumpSetStreamParameters(         void    *Parameters )
{
    CODEC_TRACE ("%s\n", __FUNCTION__);
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
CodecStatus_t   Codec_MmeVideoTheora_c::DumpDecodeParameters(            void    *Parameters )
{
    THEORA_TransformParam_t*            FrameParams;

    FrameParams      = (THEORA_TransformParam_t *)Parameters;

    CODEC_TRACE ("********** Picture %d *********\n", TheoraStaticPicture++);

    return CodecNoError;
}
//}}}

