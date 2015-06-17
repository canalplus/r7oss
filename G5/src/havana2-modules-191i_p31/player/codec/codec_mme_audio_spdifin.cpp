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

Source file name : codec_mme_audio_spdifin.cpp
Author :           Gael Lassure

Implementation of the spdif-input audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
24-July-07   Created (from codec_mme_audio_eac3.cpp)        Gael Lassure

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioSpdifIn_c
///
/// The SpdifIn audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define CODEC_TAG "SPDIFIN audio codec"
#include "codec_mme_audio_spdifin.h"
#include "codec_mme_audio_eac3.h"
#include "codec_mme_audio_dtshd.h"
#include "lpcm.h"
#include "spdifin_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct SpdifinAudioCodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    MME_LxAudioDecoderGlobalParams_t    StreamParameters;
} SpdifinAudioCodecStreamParameterContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "SpdifinAudioCodecStreamParameterContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(SpdifinAudioCodecStreamParameterContext_t)}
#else
#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT        "SpdifinAudioCodecStreamParameterContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE   {BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SpdifinAudioCodecStreamParameterContext_t)}
#endif

static BufferDataDescriptor_t           SpdifinAudioCodecStreamParameterContextDescriptor = BUFFER_SPDIFIN_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct SpdifinAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    MME_LxAudioDecoderFrameParams_t     DecodeParameters;
    MME_LxAudioDecoderFrameStatus_t     DecodeStatus;


    // Input Buffer is sent in a separate Command
    MME_Command_t                       BufferCommand;
    MME_StreamingBufferParams_t         BufferParameters;
    MME_LxAudioDecoderFrameStatus_t     BufferStatus;
} SpdifinAudioCodecDecodeContext_t;

//#if __KERNEL__
#if 0
#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT          "SpdifinAudioCodecDecodeContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromDeviceMemory, 32, 0, true, true, sizeof(SpdifinAudioCodecDecodeContext_t)}
#else
#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT          "SpdifinAudioCodecDecodeContext"
#define BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT_TYPE     {BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(SpdifinAudioCodecDecodeContext_t)}
#endif

static BufferDataDescriptor_t           SpdifinAudioCodecDecodeContextDescriptor = BUFFER_SPDIFIN_AUDIO_CODEC_DECODE_CONTEXT_TYPE;

// --------

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
/// \todo Correctly setup AudioDecoderTransformCapabilityMask
///

Codec_MmeAudioSpdifin_c::Codec_MmeAudioSpdifin_c( void )
{
    Configuration.CodecName                             = "SPDIFIN audio";

    // for SPDIFin we know that the incoming data is never longer than 1024 samples giving us a fairly
    // small maximum frame size (reducing the maximum frame size allows us to make more efficient use of
    /// the coded frame buffer) - NICK, if we wish to do this it will have to move into the wrapper now

    // Large because if it changes each frame we won't be freed until the decodes have rippled through (causing deadlock)
    Configuration.StreamParameterContextCount           = 10;
    Configuration.StreamParameterContextDescriptor      = &SpdifinAudioCodecStreamParameterContextDescriptor;

    // Send up to 10 frames for look-ahead.
    Configuration.DecodeContextCount                    = 10;
    Configuration.DecodeContextDescriptor               = &SpdifinAudioCodecDecodeContextDescriptor;

    DecodeErrors					= 0;
    NumberOfSamplesProcessed				= 0;

    //AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_SPDIFIN);

    DecoderId                                           = ACC_SPDIFIN_ID;

    memset(&EOF, 0x00, sizeof(Codec_SpdifinEOF_t));
    EOF.Command.CmdStatus.State = MME_COMMAND_FAILED; // always park the command in a not-running state
    SpdifStatus.State                                   = SPDIFIN_STATE_PCM_BYPASS;
    SpdifStatus.StreamType                              = SPDIFIN_RESERVED;
    SpdifStatus.PlayedSamples                           = 0;

    Reset();
}


////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset
///     are executed for all levels of the class.
///
Codec_MmeAudioSpdifin_c::~Codec_MmeAudioSpdifin_c( void )
{
    Halt();
    Reset();
}

CodecStatus_t   Codec_MmeAudioSpdifin_c::Reset(      void )
{
    EOF.SentEOFCommand = false;
    return Codec_MmeAudio_c::Reset();
}

const static enum eAccFsCode LpcmSpdifin2ACC[] =
{

    // DVD Video Supported Frequencies
    ACC_FS48k,
    ACC_FS96k,
    ACC_FS192k,
    ACC_FS_reserved,
    ACC_FS32k,
    ACC_FS16k,
    ACC_FS22k,
    ACC_FS24k,
    // DVD Audio Supported Frequencies
    ACC_FS44k,
    ACC_FS88k,
    ACC_FS176k,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,

    // SPDIFIN Supported frequencies
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
    ACC_FS_reserved,
};


static const LpcmAudioStreamParameters_t  DefaultStreamParameters =
{
    TypeLpcmSPDIFIN,
    ACC_MME_FALSE,          // MuteFlag
    ACC_MME_FALSE,          // EmphasisFlag
    LpcmWordSize32,
    LpcmWordSizeNone,
    LpcmSamplingFreq48,
    LpcmSamplingFreqNone,
    2,                      // NbChannels
    0,
    LPCM_DEFAULT_CHANNEL_ASSIGNMENT, // derived from NbChannels.

/*
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
*/
};

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for SPDIFIN audio.
///
CodecStatus_t Codec_MmeAudioSpdifin_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
    CodecStatus_t Status;

    //
    LpcmAudioStreamParameters_t       *Parsed;
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;

    GlobalParams.StructSize = sizeof(MME_LxAudioDecoderGlobalParams_t);

    //
    if (ParsedFrameParameters == NULL)
    {
        // At transformer init, stream properties might be unknown...
        Parsed = (LpcmAudioStreamParameters_t*) &DefaultStreamParameters;
    }
    else
    {
        Parsed = (LpcmAudioStreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    }


    MME_SpdifinConfig_t  &Config       = *((MME_SpdifinConfig_t *) GlobalParams.DecConfig);
    Config.DecoderId                   = ACC_SPDIFIN_ID;
    Config.StructSize                  = sizeof(MME_SpdifinConfig_t);

    // Setup default IEC config
    Config.Config[IEC_SFREQ]           = LpcmSpdifin2ACC[Parsed->SamplingFrequency1]; // should be 48 by default.
    Config.Config[IEC_NBSAMPLES]       = Parsed->NumberOfSamples;                     // should be 1024
    Config.Config[IEC_DEEMPH   ]       = Parsed->EmphasisFlag;

    // Setup default DD+ decoder config
    memset(&Config.DecConfig[0], 0, sizeof(Config.DecConfig));
    Config.DecConfig[DD_CRC_ENABLE]    = ACC_MME_TRUE;
    Config.DecConfig[DD_LFE_ENABLE]    = ACC_MME_TRUE;
    Config.DecConfig[DD_COMPRESS_MODE] = DD_LINE_OUT;
    Config.DecConfig[DD_HDR]           = 0xFF;
    Config.DecConfig[DD_LDR]           = 0xFF;

//

    Status = Codec_MmeAudio_c::FillOutTransformerGlobalParameters( GlobalParams_p );

    if( Status != CodecNoError )
    {
	return Status;
    }

//
    
    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
	*((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    
    MME_Resamplex2GlobalParams_t &resamplex2 = PcmParams.Resamplex2;
    // Id already set
    // StructSize already set
    resamplex2.Apply = ACC_MME_AUTO;
    resamplex2.Range = ACC_FSRANGE_48k;

//

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for SPDIFIN audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// SPDIFIN audio decoder.
///
CodecStatus_t   Codec_MmeAudioSpdifin_c::FillOutTransformerInitializationParameters( void )
{
    CodecStatus_t Status;
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

//

    MMEInitializationParameters.TransformerInitParamsSize = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p = &Params;

//

    Status = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
	return Status;

    // Spdifin decoder must be handled as streambase.
    AUDIODEC_SET_STREAMBASE((&Params), ACC_MME_TRUE);

//

    return FillOutTransformerGlobalParameters( &Params.GlobalParams );
}


////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for SPDIFIN audio.
///
CodecStatus_t   Codec_MmeAudioSpdifin_c::FillOutSetStreamParametersCommand( void )
{
    CodecStatus_t                               Status;
    SpdifinAudioCodecStreamParameterContext_t  *Context = (SpdifinAudioCodecStreamParameterContext_t *)StreamParameterContext;

    //
    // Examine the parsed stream parameters and determine what type of codec to instanciate
    //

    DecoderId = ACC_SPDIFIN_ID;

    //
    // Now fill out the actual structure
    //
    memset( &(Context->StreamParameters), 0, sizeof(Context->StreamParameters) );

    Status = FillOutTransformerGlobalParameters( &(Context->StreamParameters) );
    if( Status != CodecNoError )
	return Status;

    //
    // Fillout the actual command
    //

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_TRANSFORM parameters for SPDIFIN audio.
///
CodecStatus_t   Codec_MmeAudioSpdifin_c::FillOutDecodeCommand(       void )
{
    SpdifinAudioCodecDecodeContext_t   *Context        = (SpdifinAudioCodecDecodeContext_t *)DecodeContext;

    //
    // Initialize the frame parameters (we don't actually have much to say here)
    //

    memset( &Context->DecodeParameters, 0, sizeof(Context->DecodeParameters) );

    //
    // Zero the reply structure
    //

    memset( &Context->DecodeStatus, 0, sizeof(Context->DecodeStatus) );

    //
    // Fillout the actual command
    //

    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                    = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                      = (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}


#ifdef __KERNEL__
extern "C"{void flush_cache_all();};
#endif

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SEND_BUFFERS parameters for SPDIFIN audio.
/// Copy some code of codec_mme_base.cpp
/// Do not expect any Callback upon completion of this SEND_BUFFER as its
/// completion must be synchronous with the TRANSFORM command that contains the
/// corresponding decoded buffer.
CodecStatus_t   Codec_MmeAudioSpdifin_c::FillOutSendBufferCommand(       void )
{
    SpdifinAudioCodecDecodeContext_t   *Context        = (SpdifinAudioCodecDecodeContext_t *)DecodeContext;

    if (EOF.SentEOFCommand)
    {
	CODEC_TRACE("Already sent EOF command - refusing to queue more buffers\n");
	return CodecNoError;
    }

    //
    // Initialize the input buffer parameters (we don't actually have much to say here)
    //

    memset( &Context->BufferParameters, 0, sizeof(Context->BufferParameters) );

    //
    // Zero the reply structure
    //

    memset( &Context->BufferStatus, 0, sizeof(Context->BufferStatus) );

    //
    // Fillout the actual command
    //

    Context->BufferCommand.CmdStatus.AdditionalInfoSize = sizeof(Context->BufferStatus);
    Context->BufferCommand.CmdStatus.AdditionalInfo_p   = (MME_GenericParams_t)(&Context->BufferStatus);
    Context->BufferCommand.ParamSize                    = sizeof(Context->BufferParameters);
    Context->BufferCommand.Param_p                      = (MME_GenericParams_t)(&Context->BufferParameters);

    // Feed back will be managed at same time as the return of the corresponding MME_TRANSFORM.
    Context->BufferCommand.StructSize                   = sizeof(MME_Command_t);
    Context->BufferCommand.CmdCode                      = MME_SEND_BUFFERS;
    Context->BufferCommand.CmdEnd                       = MME_COMMAND_END_RETURN_NO_INFO;

#ifdef __KERNEL__
    flush_cache_all();
#endif

    MME_ERROR Status = MME_SendCommand( MMEHandle, &Context->BufferCommand );
    if( Status != MME_SUCCESS )
    {
	report( severity_error, "Codec_MmeAudioSpdifin_c::FillOutSendBufferCommand(%s) - Unable to send buffer command (%08x).\n", Configuration.CodecName, Status );
	return CodecError;
    }

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///  Set SPDIFIN StreamBase style TRANSFORM command IOs
///  Set DecodeContext the same way as for FrameBase TRANSFORMS
///  but Send the Input buffer in a Specific SEND_BUFFER command.
///  TRANSFORM Command is preset to emit only the corresponding Output buffer

void Codec_MmeAudioSpdifin_c::SetCommandIO(void)
{
    SpdifinAudioCodecDecodeContext_t   *Context        = (SpdifinAudioCodecDecodeContext_t *)DecodeContext;

    // Attach both I/O buffers to DecodeContext.
    PresetIOBuffers();

    // StreamBase Transformer : 1 Input Buffer sent through SEND_BUFFER / 1 Output Buffer sent through MME_TRANSFORM

    // Prepare SEND_BUFFER Command to transmit Input Buffer
    Context->BufferCommand.NumberInputBuffers  = 1;
    Context->BufferCommand.NumberOutputBuffers = 0;
    Context->BufferCommand.DataBuffers_p       = &DecodeContext->MMEBufferList[0];

    // Prepare MME_TRANSFORM Command to transmit Output Buffer
    DecodeContext->MMECommand.NumberInputBuffers      = 0;
    DecodeContext->MMECommand.NumberOutputBuffers     = 1;
    DecodeContext->MMECommand.DataBuffers_p           = &DecodeContext->MMEBufferList[1];


    //
    // Load the parameters into MME SendBuffer command
    //

    FillOutSendBufferCommand();

}

CodecStatus_t Codec_MmeAudioSpdifin_c::SendEofCommand()
{
    MME_Command_t *eof = &EOF.Command;
    
    if (EOF.SentEOFCommand) {
	CODEC_TRACE("Already sent EOF command once, refusing to do it again.\n");
	
	return CodecNoError;
    }
    
    EOF.SentEOFCommand = true;

    // Setup EOF Command ::
    eof->StructSize                     = sizeof(MME_Command_t);
    eof->CmdCode                        = MME_SEND_BUFFERS;
    eof->CmdEnd                         = MME_COMMAND_END_RETURN_NO_INFO;
    eof->NumberInputBuffers             = 1;
    eof->NumberOutputBuffers            = 0;
    eof->DataBuffers_p                  = (MME_DataBuffer_t **) &EOF.DataBuffers;
    eof->ParamSize                      = sizeof(MME_StreamingBufferParams_t);
    eof->Param_p                        = &EOF.Params;

    //
    // The following fields were reset during the Class Instantiation ::
    //
    //eof->DueTime                        = 0;
    //eof->CmdStatus.AdditionalInfoSize   = 0;
    //eof->CmdStatus.AdditionalInfo_p     = NULL;


    // Setup EOF Params
    EOF.Params.StructSize               = sizeof(MME_StreamingBufferParams_t);
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128	
    STREAMING_SET_BUFFER_TYPE(((unsigned int*)&EOF.Params.BufferFlags), STREAMING_DEC_EOF);
#else
    STREAMING_SET_BUFFER_TYPE(EOF.Params.BufferParams, STREAMING_DEC_EOF);
#endif 

    // Setup DataBuffer ::
    EOF.DataBuffers[0]                  = &EOF.DataBuffer;
    EOF.DataBuffer.StructSize           = sizeof(MME_DataBuffer_t);
    EOF.DataBuffer.UserData_p           = NULL;
    EOF.DataBuffer.NumberOfScatterPages = 1;
    EOF.DataBuffer.ScatterPages_p       = &EOF.ScatterPage;

    //
    // The following fields were reset during the Class Instantiation ::
    //
    //eof->DueTime                  = 0; // immediate.
    //EOF.DataBuffer.Flags          = 0;
    //EOF.DataBuffer.StreamNumber   = 0;
    //EOF.DataBuffer.TotalSize      = 0;
    //EOF.DataBuffer.StartOffset    = 0;


    // Setup EOF ScatterPage ::

    //
    // The following fields were reset during the Class Instantiation ::
    //
    //EOF.ScatterPage.Page_p    = NULL;
    //EOF.ScatterPage.Size      = 0;
    //EOF.ScatterPage.BytesUsed = 0;
    //EOF.ScatterPage.FlagsIn   = 0;
    //EOF.ScatterPage.FlagsOut  = 0;

    MME_ERROR Result = MME_SendCommand( MMEHandle, eof);
    if( Result != MME_SUCCESS )
    {
	CODEC_ERROR("Unable to send eof (%08x).\n", Result );
	return CodecError;
    }
    
    return CodecNoError;
}

CodecStatus_t Codec_MmeAudioSpdifin_c::DiscardQueuedDecodes()
{
    CodecStatus_t Status;
    
    Status = Codec_MmeAudio_c::DiscardQueuedDecodes();
    if (CodecNoError != Status)
	return Status;
    
    Status = SendEofCommand();
    if (CodecNoError != Status)
	return Status;
    
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Status Information display

#define SPDIFIN_TEXT(x) #x

const char * SpdifinStreamTypeText[]=
{
    SPDIFIN_TEXT(SPDIFIN_NULL_DATA_BURST),
    SPDIFIN_TEXT(SPDIFIN_AC3),
    SPDIFIN_TEXT(SPDIFIN_PAUSE_BURST),
    SPDIFIN_TEXT(SPDIFIN_MP1L1),
    SPDIFIN_TEXT(SPDIFIN_MP1L2L3),
    SPDIFIN_TEXT(SPDIFIN_MP2MC),
    SPDIFIN_TEXT(SPDIFIN_MP2AAC),
    SPDIFIN_TEXT(SPDIFIN_MP2L1LSF),
    SPDIFIN_TEXT(SPDIFIN_MP2L2LSF),
    SPDIFIN_TEXT(SPDIFIN_MP2L3LSF),
    SPDIFIN_TEXT(SPDIFIN_DTS1),
    SPDIFIN_TEXT(SPDIFIN_DTS2),
    SPDIFIN_TEXT(SPDIFIN_DTS3),
    SPDIFIN_TEXT(SPDIFIN_ATRAC),
    SPDIFIN_TEXT(SPDIFIN_ATRAC2_3),
    SPDIFIN_TEXT(SPDIFIN_IEC60937_RESERVED),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_16),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_17),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_18),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_19),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_20),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_21),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_22),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_23),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_24),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_25),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_26),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_27),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_28),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_29),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_30),
    SPDIFIN_TEXT(SPDIFIN_RESERVED_31),
    SPDIFIN_TEXT(SPDIFIN_IEC60958_PCM),
    SPDIFIN_TEXT(SPDIFIN_IEC60958_DTS14),
    SPDIFIN_TEXT(SPDIFIN_IEC60958_DTS16),
    SPDIFIN_TEXT(SPDIFIN_RESERVED)
};

const char * SpdifinStateText[]=
{
    SPDIFIN_TEXT(SPDIFIN_STATE_RESET),
    SPDIFIN_TEXT(SPDIFIN_STATE_PCM_BYPASS),
    SPDIFIN_TEXT(SPDIFIN_STATE_COMPRESSED_BYPASS),
    SPDIFIN_TEXT(SPDIFIN_STATE_UNDERFLOW),
    SPDIFIN_TEXT(SPDIFIN_STATE_INVALID)
};

static inline const char * reportStreamType(enum eMulticomSpdifinPC type)
{
    return (type < SPDIFIN_RESERVED) ? SpdifinStreamTypeText[type] : SpdifinStreamTypeText[SPDIFIN_RESERVED];
}

static inline const char * reportState(enum eMulticomSpdifinState state)
{
    return (state < SPDIFIN_STATE_INVALID) ? SpdifinStateText[state] : SpdifinStateText[SPDIFIN_STATE_INVALID];
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the ACC status structure and squawk loudly if problems are found.
///
/// Dispite the squawking this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
///
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioSpdifin_c::ValidateDecodeContext( CodecBaseDecodeContext_t *Context )
{
    SpdifinAudioCodecDecodeContext_t * DecodeContext = (SpdifinAudioCodecDecodeContext_t *) Context;
    MME_LxAudioDecoderFrameStatus_t  & Status        = DecodeContext->DecodeStatus;
    ParsedAudioParameters_t          * AudioParameters;

    enum eMulticomSpdifinState NewState, OldState = SpdifStatus.State;
    enum eMulticomSpdifinPC    NewPC   , OldPC    = SpdifStatus.StreamType;
    tMMESpdifinStatus         *FrameStatus        = (tMMESpdifinStatus *) &Status.FrameStatus[0];

    NewState = (enum eMulticomSpdifinState) FrameStatus->CurrentState;
    NewPC    = (enum eMulticomSpdifinPC   ) FrameStatus->PC;

    bool StatusChange = (AudioDecoderStatus.SamplingFreq != Status.SamplingFreq) ||
                        (AudioDecoderStatus.DecAudioMode != Status.DecAudioMode);

    // HACK: This should bloody well be in the super-class
    AudioDecoderStatus = Status;

    if ((OldState != NewState) || (OldPC != NewPC) || StatusChange)
    {
        SpdifStatus.State      = NewState;
        SpdifStatus.StreamType = NewPC;

        report( severity_info, "Codec_MmeAudioSpdifin_c::ValidateDecodeContext() New State      :: %s after %d samples\n", reportState(NewState) ,  SpdifStatus.PlayedSamples);
        report( severity_info, "Codec_MmeAudioSpdifin_c::ValidateDecodeContext() New StreamType :: [%d] %s after %d samples\n", NewPC, reportStreamType(NewPC), SpdifStatus.PlayedSamples);


        PlayerStatus_t            PlayerStatus;
        PlayerEventRecord_t       Event;
        void                     *EventUserData = NULL;

        Event.Code              = EventInputFormatChanged;
        Event.Playback          = Playback;
        Event.Stream            = Stream;
        Event.PlaybackTime      = TIME_NOT_APPLICABLE;
        Event.UserData          = EventUserData;
        Event.Value[0].Pointer  = this; // pointer to the component

        PlayerStatus  = Player->SignalEvent( &Event );
        if( PlayerStatus != PlayerNoError )
        {
            report( severity_error, "Codec_MmeAudioSpdifin_c::ValidateDecodeContext - Failed to signal event.\n" );
            return CodecError;
        }
        // END SYSFS

    }

    SpdifStatus.PlayedSamples += Status.NbOutSamples;

    NumberOfSamplesProcessed  += Status.NbOutSamples;	// SYSFS

    CODEC_DEBUG("Codec_MmeAudioSpdifin_c::ValidateDecodeContext() Transform Cmd returned \n");

    if (ENABLE_CODEC_DEBUG)
    {
	//DumpCommand(bufferIndex);
    }

    if (Status.DecStatus)
    {

	CODEC_ERROR("SPDIFIN audio decode error (muted frame): %d\n", Status.DecStatus);

	DecodeErrors++;

	//DumpCommand(bufferIndex);
	// don't report an error to the higher levels (because the frame is muted)
    }

    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //

    AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;

    // TODO: these values should be extracted from the codec's reply
    if (AudioOutputSurface)
    {
        AudioParameters->Source.BitsPerSample = AudioOutputSurface->BitsPerSample;
        AudioParameters->Source.ChannelCount  = AudioOutputSurface->ChannelCount;
    }

    AudioParameters->Organisation         = Status.AudioMode;
    AudioParameters->SampleCount          = Status.NbOutSamples;

    enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) Status.SamplingFreq;
    if (SamplingFreqCode < ACC_FS_reserved)
    {
	AudioParameters->Source.SampleRateHz = Codec_MmeAudio_c::ConvertCodecSamplingFreq(SamplingFreqCode);
	//AudioParameters->Source.SampleRateHz = 44100;
    }
    else
    {
	AudioParameters->Source.SampleRateHz = 0;
        CODEC_ERROR("SPDIFIn audio decode bad sampling freq returned: 0x%x\n", SamplingFreqCode);
    }

    if (SpdifStatus.StreamType == SPDIFIN_AC3)
    {
        Codec_MmeAudioEAc3_c::FillStreamMetadata(AudioParameters, (MME_LxAudioDecoderFrameStatus_t*)&Status);
    }
    else if (((SpdifStatus.StreamType >= SPDIFIN_DTS1) && ((SpdifStatus.StreamType <= SPDIFIN_DTS3))) ||
             (SpdifStatus.StreamType == SPDIFIN_IEC60958_DTS14) || (SpdifStatus.StreamType == SPDIFIN_IEC60958_DTS16))
    {
        Codec_MmeAudioDtshd_c::FillStreamMetadata(AudioParameters, (MME_LxAudioDecoderFrameStatus_t*)&Status);
    }
    else
    {
	// do nothing, the AudioParameters are zeroed by FrameParser_Audio_c::Input() which is
	// appropriate (i.e. OriginalEncoding is AudioOriginalEncodingUnknown)
    }

    return CodecNoError;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Terminate the SPDIFIN  mme transformer
/// First Send an Empty BUFFER with EOF Tag so that in unlocks pending TRANSFORM in case it is waiting
/// for more buffers, then wait for all SentBuffers and DecodeTransforms to have returned

CodecStatus_t   Codec_MmeAudioSpdifin_c::TerminateMMETransformer( void )
{
    CodecStatus_t           Status;

    if( MMEInitialized )
    {
        Status = SendEofCommand();
        if (CodecNoError != Status)
            return Status;

        // Call base class that waits enough time for all MME_TRANSFORMS to return
        Status = Codec_MmeBase_c::TerminateMMETransformer();

        return Status;
    }

    return CodecNoError;
}




// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSpdifin_c::DumpSetStreamParameters(          void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioSpdifin_c::DumpDecodeParameters(             void    *Parameters )
{
    CODEC_ERROR("Not implemented\n");
    return CodecNoError;
}

CodecStatus_t  Codec_MmeAudioSpdifin_c::CreateAttributeEvents (void)
{
    PlayerStatus_t            Status;
    PlayerEventRecord_t       Event;
    void                     *EventUserData = NULL;

    Event.Playback          = Playback;
    Event.Stream            = Stream;
    Event.PlaybackTime      = TIME_NOT_APPLICABLE;
    Event.UserData          = EventUserData;
    Event.Value[0].Pointer  = this;

    Status = Codec_MmeAudio_c::CreateAttributeEvents();
    if (Status != PlayerNoError)
	return  Status;

    Event.Code               = EventInputFormatCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
    	CODEC_ERROR("Failed to signal event.\n");
	return CodecError;
    }

    Event.Code              = EventSupportedInputFormatCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	CODEC_ERROR("Failed to signal event.\n" );
	return CodecError;
    }

    Event.Code              = EventDecodeErrorsCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	CODEC_ERROR("Failed to signal event.\n" );
	return CodecError;
    }

    Event.Code              = EventNumberOfSamplesProcessedCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	CODEC_ERROR("Failed to signal event.\n" );
	return CodecError;
    }

    return  CodecNoError;
}

CodecStatus_t Codec_MmeAudioSpdifin_c::GetAttribute (const char *Attribute, PlayerAttributeDescriptor_t *Value)
{
    //report( severity_error, "Codec_MmeAudioSpdifin_c::GetAttribute Enter\n");
    if (0 == strcmp(Attribute, "input_format"))
    {
	Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;

	#define C(x) case SPDIFIN_ ## x: Value->u.ConstCharPointer = #x; return CodecNoError

	switch (SpdifStatus.StreamType)
	{
		C(NULL_DATA_BURST);
		C(AC3);
		C(PAUSE_BURST);
		C(MP1L1);
		C(MP1L2L3);
		C(MP2MC);
		C(MP2AAC);
		C(MP2L1LSF);
		C(MP2L2LSF);
		C(MP2L3LSF);
		C(DTS1);
		C(DTS2);
		C(DTS3);
		C(ATRAC);
		C(ATRAC2_3);
		case SPDIFIN_IEC60958:
		    Value->u.ConstCharPointer = "PCM";
		    return CodecNoError;
		C(IEC60958_DTS14);
		C(IEC60958_DTS16);
		default:
		    CODEC_ERROR("This input_format does not exist.\n" );
		    return CodecError;
	}

	#undef C
    }
    else if (0 == strcmp(Attribute, "decode_errors"))
    {
	Value->Id 	= SYSFS_ATTRIBUTE_ID_INTEGER;
	Value->u.Int    = DecodeErrors;
	return CodecNoError;
    }
    else if (0 == strcmp(Attribute, "supported_input_format"))
    {
	//report( severity_error, "%s %d\n", __FUNCTION__, __LINE__);

	MME_LxAudioDecoderInfo_t &Capability = AudioDecoderTransformCapability;
	Value->Id 			     = SYSFS_ATTRIBUTE_ID_BOOL;

	switch (SpdifStatus.StreamType)
	{
		case SPDIFIN_AC3:
		    Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x8; // ACC_SPDIFIN_DD
		    return CodecNoError;
		case SPDIFIN_DTS1:
		case SPDIFIN_DTS2:
		case SPDIFIN_DTS3:
		case SPDIFIN_IEC60958_DTS14:
		case SPDIFIN_IEC60958_DTS16:
		    Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x10; // ACC_SPDIFIN_DTS
		    return CodecNoError;
		case SPDIFIN_MP2AAC:
		    Value->u.Bool = Capability.DecoderCapabilityExtFlags[0] & 0x20; // ACC_SPDIFIN_MPG to be renamed ACC_SPDIFIN_AAC
		case SPDIFIN_IEC60958:
		case SPDIFIN_NULL_DATA_BURST:
		case SPDIFIN_PAUSE_BURST:
		    Value->u.Bool = true;
		    return CodecNoError;
		case SPDIFIN_MP1L1:
		case SPDIFIN_MP1L2L3:
		case SPDIFIN_MP2MC:
		case SPDIFIN_MP2L1LSF:
		case SPDIFIN_MP2L2LSF:
		case SPDIFIN_MP2L3LSF:
		case SPDIFIN_ATRAC:
		case SPDIFIN_ATRAC2_3:
		default:
		    Value->u.Bool = false;
		    return CodecNoError;
	}
    }
    else if (0 == strcmp(Attribute, "number_of_samples_processed"))
    {
	Value->Id 	= SYSFS_ATTRIBUTE_ID_UNSIGNEDLONGLONGINT;
	Value->u.UnsignedLongLongInt = NumberOfSamplesProcessed;
	return CodecNoError;
    }
    else
    {
	CodecStatus_t Status;
	Status = Codec_MmeAudio_c::GetAttribute (Attribute, Value);
	if (Status != CodecNoError)
	{
	    CODEC_ERROR("This attribute does not exist.\n" );
            return CodecError;
	}
    }

    return CodecNoError;
}

CodecStatus_t Codec_MmeAudioSpdifin_c::SetAttribute (const char *Attribute,  PlayerAttributeDescriptor_t *Value)
{
    if (0 == strcmp(Attribute, "decode_errors"))
    {
	DecodeErrors    = Value->u.Int;
	return CodecNoError;
    }
    else
    {
	CODEC_ERROR("This attribute cannot be set.\n" );
	return CodecError;
    }

    return CodecNoError;
}

