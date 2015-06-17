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

Source file name : codec_mme_audio.cpp
Author :           Daniel

Implementation of the audio codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
26-Apr-07   Created (from codec_mme_video.cpp)              Daniel
11-Sep-07   Reorganised to allow override by WMA/OGG to
	    make frame->stream possible                     Adam

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudio_c
///
/// Audio specific code to assist operation of the AUDIO_DECODER transformer
/// provided by the audio firmware.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#define CODEC_TAG "Audio codec"
#include "codec_mme_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// lookup table to convert from ACC frequency code to sample rate in Hertz
const int ACC_SamplingFreqLUT[ACC_FS_reserved + 1]=
  {
    48000,  //  ACC_FS48k
    44100,  //  ACC_FS44k
    32000,  //  ACC_FS32k
    -1 ,    //  ACC_FS_reserved_3,
    96000,  //  ACC_FS96k,
    88200,  //  ACC_FS88k,
    64000,  //  ACC_FS64k,
    -1,     //  ACC_FS_reserved_7,
    192000, //  ACC_FS192k,
    176400, //  ACC_FS176k,
    128000, //  ACC_FS128k,
    -1,     //  ACC_FS_reserved_11,
    384000, //  ACC_FS384k,
    352800, //  ACC_FS352k,
    256000, //  ACC_FS256k,
    -1,     //  ACC_FS_reserved_15,
    12000,  //  ACC_FS12k,
    11025,  //  ACC_FS11k,
    8000,   //  ACC_FS8k,
    -1,     //  ACC_FS_reserved_19,
    24000,  //  ACC_FS24k,
    22050,  //  ACC_FS22k,
    16000,  //  ACC_FS16k,
    -1,     //  ACC_FS_reserved_23,
    -1,     //  ACC_FS_reserved
  };


const int ACC_AcMode2ChannelCountLUT[]=
  {
    2, //    ACC_MODE20t,           /*  0 */
    1, //    ACC_MODE10,            /*  1 */
    2, //    ACC_MODE20,            /*  2 */
    3, //    ACC_MODE30,            /*  3 */
    3, //    ACC_MODE21,            /*  4 */
    4, //    ACC_MODE31,            /*  5 */
    4, //    ACC_MODE22,            /*  6 */
    5, //    ACC_MODE32,            /*  7 */
    5, //    ACC_MODE23,            /*  8 */
    6, //    ACC_MODE33,            /*  9 */
    6, //    ACC_MODE24,            /*  A */
    7, //    ACC_MODE34,            /*  B */
    6, //    ACC_MODE42,            /*  C */
    8, //    ACC_MODE44,            /*  D */
    7, //    ACC_MODE52,            /*  E */
    8, //    ACC_MODE53,            /*  F */
  };

////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters and reset everything.
///
Codec_MmeAudio_c::Codec_MmeAudio_c( void )
{
    Configuration.CodecName                             = "unknown audio";

    Configuration.DecodeOutputFormat                    = FormatAudio;

    Configuration.StreamParameterContextCount           = 0;
    Configuration.StreamParameterContextDescriptor      = 0;

    Configuration.DecodeContextCount                    = 0;
    Configuration.DecodeContextDescriptor               = 0;

    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;

    // provide default values for the transformer names. these can be overridden
    // by the sub-class or, more commonly, with SetModuleParameters()
    for (int i =0; i< CODEC_MAX_TRANSFORMERS; i++)
    {
	strcpy(TransformName[i], AUDIO_DECODER_TRANSFORMER_NAME);
	Configuration.TransformName[i]                  = TransformName[i];
    }
    Configuration.AvailableTransformers                 = CODEC_MAX_TRANSFORMERS;

    Configuration.SizeOfTransformCapabilityStructure    = sizeof(AudioDecoderTransformCapability);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&AudioDecoderTransformCapability);

    Configuration.AddressingMode                        = UnCachedAddress;

    Configuration.ShrinkCodedDataBuffersAfterDecode     = false;

    //

    memset( &AudioDecoderTransformCapabilityMask, 0, sizeof(AudioDecoderTransformCapabilityMask) );

    ProtectTransformName                                = false;

    Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CodecStatus_t   Codec_MmeAudio_c::Halt(         void )
{

    AudioOutputSurface          = NULL;
    ParsedAudioParameters       = NULL;
    SelectedChannel             = ChannelSelectStereo;

    return Codec_MmeBase_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variables
//

CodecStatus_t   Codec_MmeAudio_c::Reset(        void )
{

    AudioOutputSurface          = NULL;
    ParsedAudioParameters       = NULL;
    SelectedChannel             = ChannelSelectStereo;
    memset( &DRC, 0, sizeof(DRC) );
    OutmodeMain                 = ACC_MODE_ID; // output raw decoded data by default

    memset( &AudioDecoderStatus, 0, sizeof(AudioDecoderStatus));

    return Codec_MmeBase_c::Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      SetModuleParameters function
//      Action  : Allows external user to set up important environmental parameters
//      Input   :
//      Output  :
//      Result  :
//

CodecStatus_t   Codec_MmeAudio_c::SetModuleParameters(
						unsigned int   ParameterBlockSize,
						void          *ParameterBlock )
{
    struct CodecParameterBlock_s*       CodecParameterBlock = (struct CodecParameterBlock_s*)ParameterBlock;


    if (ParameterBlockSize != sizeof(struct CodecParameterBlock_s))
    {
	report( severity_error, "Codec_MmeAudio_c::SetModuleParameters: Invalid parameter block.\n");
	return CodecError;
    }

    if( CodecParameterBlock->ParameterType == CodecSelectChannel )
    {
	if (SelectedChannel != CodecParameterBlock->Channel)
	{
	    SelectedChannel         = CodecParameterBlock->Channel;
	    CODEC_DEBUG( "Setting selected channel to %d\n", SelectedChannel );

	    ForceStreamParameterReload      = true;
	}
	return CodecNoError;
    }

    if( CodecParameterBlock->ParameterType == CodecSpecifyTransformerPostFix )
    {
	unsigned int Transformer = CodecParameterBlock->TransformerPostFix.Transformer;
	char PostFix = CodecParameterBlock->TransformerPostFix.PostFix;

	if( ProtectTransformName )
	{
	    CODEC_TRACE("Transform name is protected");
	    return CodecError;
	}

	unsigned int Len = strlen(TransformName[Transformer]);

	if (Len >= (sizeof(TransformName[Transformer]) - 2))
	{
	    CODEC_ERROR("Transform name is too long to accept postfix (%d of %d)\n",
			Len, sizeof(TransformName[Transformer]) - 2);
	    return CodecError;
	}

	TransformName[Transformer][Len] = PostFix;
	TransformName[Transformer][Len+1] = '\0';

	// use of the configuration alias is intentional. some transformers will override the names by a
	// different means and we don't was to issue misleading diagnostics.
	CODEC_DEBUG("Transformer %d: %s\n", Transformer, Configuration.TransformName[Transformer]);

	return CodecNoError;
    }

    if( CodecParameterBlock->ParameterType == CodecSpecifyDRC )
    {
	if (memcmp(&DRC, &CodecParameterBlock->DRC, sizeof(DRC)))
	{

	    DRC         = CodecParameterBlock->DRC;
	    CODEC_DEBUG( "Setting DRC to { Enable : %d / Type : %d /  HDR : %d / LDR : %d }\n", DRC.Enable, DRC.Type, DRC.HDR, DRC.LDR );

	    ForceStreamParameterReload      = true;
	}
	return CodecNoError;
    }

    if( CodecParameterBlock->ParameterType == CodecSpecifyDownmix )
    {
	if( CodecParameterBlock->Downmix.OutmodeMain != OutmodeMain )
	{
	    OutmodeMain = static_cast<eAccAcMode>(CodecParameterBlock->Downmix.OutmodeMain);

	    ForceStreamParameterReload      = true;
	}
	return CodecNoError;
    }	    

    return Codec_MmeBase_c::SetModuleParameters( ParameterBlockSize, ParameterBlock );
}


////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command IOs
///  Populate DecodeContext with 1 Input and 1 output Buffer
///  Populate I/O MME_DataBuffers
void Codec_MmeAudio_c::PresetIOBuffers(void)
{
CodecBufferState_t      *State;

    // plumbing
    DecodeContext->MMEBufferList[0] = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBufferList[1] = &DecodeContext->MMEBuffers[1];

    for( int i=0; i<2; ++i )
    {
	DecodeContext->MMEBufferList[i] = &DecodeContext->MMEBuffers[i];

	memset( &DecodeContext->MMEBuffers[i], 0, sizeof(MME_DataBuffer_t) );
	DecodeContext->MMEBuffers[i].StructSize           = sizeof(MME_DataBuffer_t);
	DecodeContext->MMEBuffers[i].NumberOfScatterPages = 1;
	DecodeContext->MMEBuffers[i].ScatterPages_p       = &DecodeContext->MMEPages[i];

	memset( &DecodeContext->MMEPages[i], 0, sizeof(MME_ScatterPage_t) );
    }

    // input
    DecodeContext->MMEBuffers[0].TotalSize = CodedDataLength;
    DecodeContext->MMEPages[0].Page_p      = CodedData;
    DecodeContext->MMEPages[0].Size        = CodedDataLength;

    // output

    State   = &BufferState[CurrentDecodeBufferIndex];
    if( State->BufferStructure->ComponentCount != 1 )
	report( severity_fatal, "Codec_MmeAudio_c::PresetIOBuffers - Decode buffer structure contains unsupported number of components (%d).\n", State->BufferStructure->ComponentCount );

    DecodeContext->MMEBuffers[1].TotalSize = State->BufferLength - State->BufferStructure->ComponentOffset[0];
    DecodeContext->MMEPages[1].Page_p      = State->BufferPointer + State->BufferStructure->ComponentOffset[0];
    DecodeContext->MMEPages[1].Size        = State->BufferLength - State->BufferStructure->ComponentOffset[0];
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.
void Codec_MmeAudio_c::SetCommandIO(void)
{
    PresetIOBuffers();

    // FrameBase Transformer :: 1 Input Buffer / 1 Output Buffer sent through same MME_TRANSFORM
    DecodeContext->MMECommand.NumberInputBuffers  = 1;
    DecodeContext->MMECommand.NumberOutputBuffers = 1;
    DecodeContext->MMECommand.DataBuffers_p = DecodeContext->MMEBufferList;

}

// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

CodecStatus_t   Codec_MmeAudio_c::RegisterOutputBufferRing( Ring_t        Ring )
{
CodecStatus_t   Status;

    //
    // Obtain from the manifestor, the surface parameters we will be dealing with.
    //
    // This is done before the standard operations since
    // Codec_MmeAudio_c::FillOutTransformerInitializationParameters uses AudioOutputSurface
    // to determine how to initialize the decoder.
    //
    // NOTE this causes get class list to be called twice, but this is no problem
    //

    if( Manifestor != NULL )
    {
	Status  = Manifestor->GetSurfaceParameters( (void **)(&AudioOutputSurface) );
	if( Status != ManifestorNoError )
	{
	    report( severity_error, "Codec_MmeAudio_c::RegisterOutputBufferRing(%s) - Failed to get output surface parameters.\n", Configuration.CodecName );
	    return Status;
	}
    }

    //
    // Perform the standard operations
    //

    Status      = Codec_MmeBase_c::RegisterOutputBufferRing( Ring );
    if( Status != CodecNoError )
	return Status;



//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudio_c::Input(        Buffer_t          CodedBuffer )
{
CodecStatus_t     Status;

    //
    // Are we allowed in here
    //

    AssertComponentState( "Codec_MmeAudio_c::Input", ComponentRunning );

    //
    // First perform base operations
    //

    Status      = Codec_MmeBase_c::Input( CodedBuffer );
    if( Status != CodecNoError )
	return Status;

    CODEC_DEBUG( "Handling frame %d\n", ParsedFrameParameters->DisplayFrameIndex );

    //
    // Do we need to issue a new set of stream parameters
    //

    if( ParsedFrameParameters->NewStreamParameters )
    {
	memset( &StreamParameterContext->MMECommand, 0x00, sizeof(MME_Command_t) );

	Status          = FillOutSetStreamParametersCommand();
	if( Status == CodecNoError )
	    Status      = SendMMEStreamParameters();

	if( Status != CodecNoError )
	{
	    report( severity_error, "Codec_MmeAudio_c::Input(%s) - Failed to fill out, and send, a set stream parameters command.\n", Configuration.CodecName );
	    StreamParameterContextBuffer->DecrementReferenceCount();
	    return Status;
	}

	StreamParameterContextBuffer    = NULL;
	StreamParameterContext          = NULL;
    }

    //
    // If there is no frame to decode then exit now.
    //

    if( !ParsedFrameParameters->NewFrameParameters )
	return CodecNoError;

    Status      = FillOutDecodeContext();
    if( Status != CodecNoError )
    {
	return Status;
    }

    //
    // Load the parameters into MME command
    //

    //! set up default to MME_TRANSFORM - fine for most codecs
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;

    //! will change to MME_SEND_BUFFERS in WMA/OGG over-ridden FillOutDecodeCommand(), thread to do MME_TRANSFORMs
    Status      = FillOutDecodeCommand();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeAudio_c::Input(%s) - Failed to fill out a decode command.\n", Configuration.CodecName );
	ReleaseDecodeContext( DecodeContext );
	return Status;
    }



    //
    // Ensure that the coded frame will be available throughout the
    // life of the decode by attaching the coded frame to the decode
    // context prior to launching the decode.
    //

    AttachCodedFrameBuffer();

    Status      = SendMMEDecodeCommand();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeAudio_c::Input(%s) - Failed to send a decode command.\n", Configuration.CodecName );
	ReleaseDecodeContext( DecodeContext );
	return Status;
    }

    //
    // We have finished decoding into this buffer
    //

    FinishedDecode();

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Atach the coded frame buffer to the decode frame
/// This can be over-ridden by EAC3 to attach also a transcoded buffer
///
void Codec_MmeAudio_c::AttachCodedFrameBuffer( void ) 
{
    DecodeContextBuffer->AttachBuffer( CodedFrameBuffer );
    //CODEC_TRACE ("%s\n", __FUNCTION__);
    //DecodeContextBuffer->Dump (DumpAll);
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the DecodeContext structure with parameters for MPEG audio.
/// This can be over-ridden by WMA to not have any output buffer, as it will
/// do an MME_SEND_BUFFERS instead of MME_TRANSFORM
///
CodecStatus_t Codec_MmeAudio_c::FillOutDecodeContext( void )
{
CodecStatus_t     Status;
    //
    // Obtain a new buffer if needed
    //

    if( CurrentDecodeBufferIndex == INVALID_INDEX )
    {
	Status  = GetDecodeBuffer();
	if( Status != CodecNoError )
	{
	    report( severity_error, "Codec_MmeAudio_c::Input(%s) - Failed to get decode buffer.\n", Configuration.CodecName );
	    ReleaseDecodeContext( DecodeContext );
	    return Status;
	}

    }

    //
    // Record the buffer being used in the decode context
    //

    DecodeContext->BufferIndex  = CurrentDecodeBufferIndex;

    //
    // Provide default values for the input and output buffers (the sub-class can change this if it wants to).
    //
    // Happily the audio firmware is less mad than the video firmware on the subject of buffers.
    //

    memset( &DecodeContext->MMECommand, 0x00, sizeof(MME_Command_t) );

    SetCommandIO();

    OS_LockMutex( &Lock );

    DecodeContext->DecodeInProgress     = true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = true;

    OS_UnLockMutex( &Lock );


    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Clear up - to be over-ridden by WMA codec to do nothing, as actual decode done elsewhere
///
void Codec_MmeAudio_c::FinishedDecode( void )
{
    //
    // We have finished decoding into this buffer
    //

    CurrentDecodeBufferIndex    = INVALID_INDEX;
    CurrentDecodeIndex          = INVALID_INDEX;
}


////////////////////////////////////////////////////////////////////////////
///
///     The intercept to the initailize data types function, that
///     ensures the audio specific type is recorded in the configuration
///     record.
///

CodecStatus_t   Codec_MmeAudio_c::InitializeDataTypes(  void )
{
    //
    // Add audio specific types, and the address of
    // parsed audio parameters to the configuration
    // record. Then pass on down to the base class
    //

    Configuration.AudioVideoDataParsedParametersType    = Player->MetaDataParsedAudioParametersType;
    Configuration.AudioVideoDataParsedParametersPointer = (void **)&ParsedAudioParameters;
    Configuration.SizeOfAudioVideoDataParsedParameters  = sizeof(ParsedAudioParameters_t);

    return Codec_MmeBase_c::InitializeDataTypes();
}


////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// In addition to some diagnostic output and some trivial error checking the
/// primary purpose of this function is to ensure that the firmware is capable
/// of performing the requested decode.
///
CodecStatus_t   Codec_MmeAudio_c::HandleCapabilities( void )
{
    // Locally rename the VeryLongMemberNamesUsedInTheSuperClass
    MME_LxAudioDecoderInfo_t &Capability = AudioDecoderTransformCapability;
    MME_LxAudioDecoderInfo_t &Mask = AudioDecoderTransformCapabilityMask;

    // Check for version skew
    if ( Capability.StructSize != sizeof(MME_LxAudioDecoderInfo_t) )
    {
	CODEC_ERROR( "%s reports different sizeof MME_LxAudioDecoderInfo_t (version skew?)\n",
		     Configuration.TransformName[SelectedTransformer] );
	return CodecError;
    }

    // Dump the transformer capability structure
    CODEC_DEBUG( "Transformer Capability: %s\n", Configuration.TransformName[SelectedTransformer]);
    CODEC_DEBUG( "\tDecoderCapabilityFlags = %x (requires %x)\n",
		 Capability.DecoderCapabilityFlags, Mask.DecoderCapabilityFlags );
    CODEC_DEBUG( "\tDecoderCapabilityExtFlags[0..1] = %x %x\n",
	      Capability.DecoderCapabilityExtFlags[0],
	      Capability.DecoderCapabilityExtFlags[1]);
    CODEC_DEBUG( "\tPcmProcessorCapabilityFlags[0..1] = %x %x\n",
	      Capability.PcmProcessorCapabilityFlags[0],
	      Capability.PcmProcessorCapabilityFlags[1]);

    // Confirm that the transformer is sufficiently capable to support the required decoder
    if( ( Mask.DecoderCapabilityFlags         !=
	  ( Mask.DecoderCapabilityFlags         & Capability.DecoderCapabilityFlags         ) ) ||
	( Mask.DecoderCapabilityExtFlags[0]   !=
	  ( Mask.DecoderCapabilityExtFlags[0]   & Capability.DecoderCapabilityExtFlags[0]   ) ) ||
	( Mask.DecoderCapabilityExtFlags[1]   !=
	  ( Mask.DecoderCapabilityExtFlags[1]   & Capability.DecoderCapabilityExtFlags[1]   ) ) ||
	( Mask.PcmProcessorCapabilityFlags[0] !=
	  ( Mask.PcmProcessorCapabilityFlags[0]   & Capability.PcmProcessorCapabilityFlags[0]) ) ||
	( Mask.PcmProcessorCapabilityFlags[1] !=
	  ( Mask.PcmProcessorCapabilityFlags[1]   & Capability.PcmProcessorCapabilityFlags[1]) ) )
    {
	CODEC_ERROR( "%s is not capable of decoding %s\n",
		     Configuration.TransformName[SelectedTransformer], Configuration.CodecName );
	return CodecError;
    }

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for MPEG audio.
///
CodecStatus_t Codec_MmeAudio_c::FillOutTransformerGlobalParameters( MME_LxAudioDecoderGlobalParams_t *GlobalParams_p )
{
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    MME_LxDecConfig_t &Config = *((MME_LxDecConfig_t *) GlobalParams.DecConfig);
    unsigned char *PcmParams_p = ((unsigned char *) &Config) + Config.StructSize;

//

    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams =
	*((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    PcmParams.StructSize = sizeof(PcmParams);
    PcmParams.DigSplit   = ACC_SPLIT_AUTO;
    PcmParams.AuxSplit   = ACC_SPLIT_AUTO;

    MME_CMCGlobalParams_t &cmc = PcmParams.CMC;
    cmc.Id = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, ACC_MIX_MAIN);
    cmc.StructSize = sizeof(MME_CMCGlobalParams_t);
    cmc.Config[CMC_OUTMODE_MAIN] = OutmodeMain;
    cmc.Config[CMC_OUTMODE_AUX] = ACC_MODE_ID; // no stereo downmix
    cmc.Config[CMC_DUAL_MODE] = (SelectedChannel == ChannelSelectLeft)  ? ACC_DUAL_LEFT_MONO :
				(SelectedChannel == ChannelSelectRight) ? ACC_DUAL_RIGHT_MONO :
									  ACC_DUAL_LR;
    cmc.Config[CMC_PCM_DOWN_SCALED] = ACC_MME_TRUE;
    cmc.CenterMixCoeff = ACC_M3DB;
    cmc.SurroundMixCoeff = ACC_M3DB;

//

    MME_DMixGlobalParams_t &dmix = PcmParams.DMix;
    dmix.Id = PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, ACC_MIX_MAIN);
    dmix.Apply = (OutmodeMain == ACC_MODE_ID ? ACC_MME_DISABLED : ACC_MME_ENABLED);
    dmix.StructSize = sizeof(MME_DMixGlobalParams_t);
    dmix.Config[DMIX_USER_DEFINED] = ACC_MME_FALSE;
    dmix.Config[DMIX_STEREO_UPMIX] = ACC_MME_FALSE;
    dmix.Config[DMIX_MONO_UPMIX] = ACC_MME_FALSE;
    dmix.Config[DMIX_MEAN_SURROUND] = ACC_MME_FALSE;
    dmix.Config[DMIX_SECOND_STEREO] = ACC_MME_TRUE;
    dmix.Config[DMIX_NORMALIZE] = ACC_MME_TRUE;
    dmix.Config[DMIX_NORM_IDX] = 0;
    dmix.Config[DMIX_DIALOG_ENHANCE] = ACC_MME_FALSE;

//

    MME_Resamplex2GlobalParams_t &resamplex2 = PcmParams.Resamplex2;
    resamplex2.Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, ACC_MIX_MAIN);
    resamplex2.StructSize = sizeof(MME_Resamplex2GlobalParams_t);
    resamplex2.Apply = ACC_MME_DISABLED;

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for audio.
///
/// Zero the entire initialization structure and populate those portions that
/// are generic for all AUDIO_DECODER instanciations.
///
/// WARNING: This method does not do what you think it does.
///
/// More exactly, if you think that this method will populate the
/// global parameters structure which forms the bulk of the initialization
/// parameters structure then you're wrong! Basically the codec specific
/// initialization parameters are variable size so from this method we don't
/// know what byte offset to use.
///
/// The upshot of all this is that this method \b must be overriden in
/// our sub-classes.
///
CodecStatus_t Codec_MmeAudio_c::FillOutTransformerInitializationParameters()
{
MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;

//

    memset(&Params, 0, sizeof(Params));

//

    Params.StructSize = sizeof(MME_LxAudioDecoderInitParams_t);
    Params.CacheFlush = ACC_MME_ENABLED;

    // Detect changes between BL025 and BL028 (delete this code when BL025 is accient history)
    #if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    Params.BlockWise.u32 = 0;
    #else
	Params.BlockWise = ACC_MME_FALSE;
    #endif

    // upsampling must be enabled seperately for each codec since it requires frame analyser support
    Params.SfreqRange = ACC_FSRANGE_UNDEFINED;

    // run the audio decoder with a fixed number of main (surround) channels and no auxillary output at all
    Params.NChans[ACC_MIX_MAIN] = AudioOutputSurface->ChannelCount;
    Params.NChans[ACC_MIX_AUX]  = 0;
    Params.ChanPos[ACC_MIX_MAIN] = 0;
    Params.ChanPos[ACC_MIX_AUX] = 0;

//

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the PCM processing extended status structures and squawk loudly if problems are found.
///
CodecStatus_t Codec_MmeAudio_c::ValidatePcmProcessingExtendedStatus(
		CodecBaseDecodeContext_t *Context,
		MME_PcmProcessingFrameExtStatus_t *PcmStatus )
{
    //
    // provide default values for parameters that may be influenced by the extended status
    //

    ParsedAudioParameters_t *AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;

    AudioParameters->MixingMetadata.IsMixingMetadataPresent = false;

    //
    // Get ready to process the status messages (and perform a little bounds checking
    //

    int BytesLeft = PcmStatus->BytesUsed;
    MME_PcmProcessingStatusTemplate_t *SpecificStatus = &(PcmStatus->PcmStatus);

    if (BytesLeft > (int) (sizeof(MME_PcmProcessingFrameExtStatus_Concrete_t) - sizeof(PcmStatus->BytesUsed))) {
	CODEC_ERROR("BytesLeft is too large - BytesLeft %d\n", BytesLeft);
	return CodecError;
    }

    //
    // Iterate through the status messages processing each one
    //

    while (BytesLeft > 0) {
	// check for sanity
	if (SpecificStatus->StructSize < 8 ||
	    SpecificStatus->StructSize > (unsigned) BytesLeft)
	{
	    CODEC_ERROR ("PCM extended status is too %s - Id %x  StructSize %d",
			 (SpecificStatus->StructSize < 8 ? "small" : "large"),
			 SpecificStatus->Id, SpecificStatus->StructSize);
	    return CodecError;
	}

	// handle the status report
	switch (SpecificStatus->Id) { 
	case ACC_PCMPROCESS_MAIN_MT << 8:
	    CODEC_DEBUG("Found [Output 0 (MAIN)]\n");
	    // do nothing with it
	    break;

	case ACC_MIX_METADATA_ID:
	    CODEC_DEBUG("Found ACC_MIX_METADATA_ID - Id %x  StructSize %d\n",
			SpecificStatus->Id, SpecificStatus->StructSize);
	    HandleMixingMetadata(Context, SpecificStatus);
	    break;

	default:
	    CODEC_TRACE("Ignoring unexpected PCM extended status - Id %x  StructSize %d\n",
			SpecificStatus->Id, SpecificStatus->StructSize);
	    // just ignore it... don't propagate the error
	}

	// move to the next status field. we checked for sanity above so we shouldn't need any bounds checks.
	BytesLeft -= SpecificStatus->StructSize;
	SpecificStatus = (MME_PcmProcessingStatusTemplate_t *)
			(((char *) SpecificStatus) + SpecificStatus->StructSize);
    }

    return CodecNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Error generating stub function. Should be unreachable.
///
/// This is method is deliberately not abstract since not all sub-classes are
/// required to override it.
///
void Codec_MmeAudio_c::HandleMixingMetadata( CodecBaseDecodeContext_t *Context,
					      MME_PcmProcessingStatusTemplate_t *PcmStatus )
{
    CODEC_ERROR("Found mixing metadata but sub-class didn't provide a mixing metadata handler\n");
}

// This function simply uses the table defined above
int Codec_MmeAudio_c::ConvertCodecSamplingFreq(enum eAccFsCode Code)
{
  int nLUTs = sizeof(ACC_SamplingFreqLUT) / sizeof(ACC_SamplingFreqLUT[0]);
  return (Code >= nLUTs ? -1 : ACC_SamplingFreqLUT[Code]);
}


// This function simply uses the table defined above
unsigned char Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration(enum eAccAcMode Mode)
{
  int nLUTs = sizeof(ACC_AcMode2ChannelCountLUT) / sizeof(ACC_AcMode2ChannelCountLUT[0]);
  return (Mode >= nLUTs ? 0 : ACC_AcMode2ChannelCountLUT[Mode]);
}


// /////////////////////////////////////////////////////////////////////////
//
//      The generic audio function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeAudio_c::FillOutDecodeBufferRequest(   BufferStructure_t        *Request )
{
    memset( Request, 0x00, sizeof(BufferStructure_t) );

    //
    // Nick fudge
    //

    ParsedAudioParameters->Source.BitsPerSample = 32;
    ParsedAudioParameters->Source.ChannelCount  = 8;

//

    Request->Format             = Configuration.DecodeOutputFormat;
    Request->DimensionCount     = 3;
    Request->Dimension[0]       = ParsedAudioParameters->Source.BitsPerSample;
    Request->Dimension[1]       = ParsedAudioParameters->Source.ChannelCount;
    Request->Dimension[2]       = ParsedAudioParameters->SampleCount?ParsedAudioParameters->SampleCount:Configuration.MaximumSampleCount;

    if( (Request->Dimension[0] != 32) ||
	(Request->Dimension[1] != 8) ||
	(Request->Dimension[2] == 0) )
	report( severity_info, "Codec_MmeAudio_c::FillOutDecodeBufferRequest - Non-standard parameters (%2d %d %d).\n",
		Request->Dimension[0], Request->Dimension[1], Request->Dimension[2] );

    return CodecNoError;
}

CodecStatus_t  Codec_MmeAudio_c::CreateAttributeEvents (void)
{
    PlayerStatus_t            Status;
    PlayerEventRecord_t       Event;
    void                     *EventUserData = NULL;

    Event.Playback          = Playback;
    Event.Stream            = Stream;
    Event.PlaybackTime      = TIME_NOT_APPLICABLE;
    Event.UserData          = EventUserData;
    Event.Value[0].Pointer  = this;

    Status = Codec_MmeBase_c::CreateAttributeEvents();
    if (Status != PlayerNoError)
	return  Status;

    Event.Code              = EventSampleFrequencyCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	CODEC_ERROR("Failed to signal event.\n" );
	return CodecError;
    }

    Event.Code              = EventNumberChannelsCreated;
    Status  = Player->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	CODEC_ERROR("Failed to signal event.\n" );
	return CodecError;
    }

    return  CodecNoError;
}

CodecStatus_t Codec_MmeAudio_c::GetAttribute (const char *Attribute, PlayerAttributeDescriptor_t *Value)
{
    //report( severity_error, "Codec_MmeAudio_c::GetAttribute Enter\n");

    if (0 == strcmp(Attribute, "sample_frequency"))
    {
	enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) AudioDecoderStatus.SamplingFreq;
	Value->Id                        = SYSFS_ATTRIBUTE_ID_INTEGER;

	if (SamplingFreqCode < ACC_FS_reserved)
	    Value->u.Int = Codec_MmeAudio_c::ConvertCodecSamplingFreq(SamplingFreqCode);
	else
	    Value->u.Int = 0;

	return CodecNoError;
    }
    else if (0 == strcmp(Attribute, "number_channels"))
    {
#if 0
	int lfe_mask = ((int) ACC_MODE20t_LFE - (int) ACC_MODE20t);
	int low_channel = 0;
	int number_channels = 0;

	Value->Id                        = SYSFS_ATTRIBUTE_ID_INTEGER;
	if (AudioDecoderStatus.DecAudioMode & lfe_mask) low_channel = 1;
	number_channels = GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode)(AudioDecoderStatus.DecAudioMode & ~lfe_mask));
	Value->u.Int = number_channels + low_channel;

#else

#define C(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## _LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break
#define Cn(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break
#define Ct(f,b) case ACC_MODE ## f ## b ## t: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## t_LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break

    Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;
    switch((enum eAccAcMode)(AudioDecoderStatus.DecAudioMode))
    {
    Ct(2,0);
    C(1,0);
    C(2,0);
    C(3,0);
    C(2,1);
    C(3,1);
    C(2,2);
    C(3,2);
    C(2,3);
    C(3,3);
    C(2,4);
    C(3,4);
    Cn(4,2);
    Cn(4,4);
    Cn(5,2);
    Cn(5,3);
    default:
	Value->u.ConstCharPointer = "UNKNOWN";
    }

#endif

	return CodecNoError;
    }
    else
    {
	CODEC_ERROR("This attribute does not exist.\n" );
	return CodecError;
    }

    return CodecNoError;
}

