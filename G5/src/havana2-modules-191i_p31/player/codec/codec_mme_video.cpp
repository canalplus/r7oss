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

Source file name : codec_mme_video.cpp
Author :           Nick

Implementation of the video codec class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
23-Jan-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "codec_mme_video.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DECODE_RATE_LOWER_LIMIT				Rational_t(3,4)		// Used to be 1, changed to trap failure to decode of divxhd
#define DECODE_RATE_UPPER_LIMIT				6

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// 	The Halt function, give up access to any registered resources
//

CodecStatus_t   Codec_MmeVideo_c::Halt( 	void )
{

    VideoOutputSurface		= NULL;
    ParsedVideoParameters	= NULL;
    KnownLastSliceInFieldFrame	= false;

    return Codec_MmeBase_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The Reset function release any resources, and reset all variables
//

CodecStatus_t   Codec_MmeVideo_c::Reset( 	void )
{

    VideoOutputSurface		= NULL;
    ParsedVideoParameters	= NULL;
    KnownLastSliceInFieldFrame	= false;

    return Codec_MmeBase_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The register output ring function function
//

CodecStatus_t   Codec_MmeVideo_c::RegisterOutputBufferRing( Ring_t	  Ring )
{
CodecStatus_t	Status;

    //
    // Perform the standard operations
    //

    Status	= Codec_MmeBase_c::RegisterOutputBufferRing( Ring );
    if( Status != CodecNoError )
	return Status;

    //
    // Obtain from the manifestor, the surface parameters we will be dealing with
    //

    if( Manifestor != NULL )
    {
	Status	= Manifestor->GetSurfaceParameters( (void **)(&VideoOutputSurface) );
	if( Status != ManifestorNoError )
	{
	    report( severity_error, "Codec_MmeVideo_c::RegisterOutputBufferRing(%s) - Failed to get output surface parameters.\n", Configuration.CodecName );
	    SetComponentState(ComponentInError);
	    return Status;
	}
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeVideo_c::Input(	Buffer_t	  CodedBuffer )
{
unsigned int		  i;
CodecStatus_t		  Status;
ParsedVideoParameters_t	 *PreviousFieldParameters;
bool			  LastSlice;
bool			  SeenBothFields;
bool			  LastDecodeIntoThisBuffer;
CodecBufferState_t	 *State;

    //
    // Are we allowed in here
    //

    AssertComponentState( "Codec_MmeVideo_c::Input", ComponentRunning );

    //
    // First perform base operations
    //

    Status      = Codec_MmeBase_c::Input( CodedBuffer );
    if( Status != CodecNoError )
        return Status;

    //
    // Do we need to issue a new set of stream parameters
    //

    if( ParsedFrameParameters->NewStreamParameters )
    {
	Status		= FillOutSetStreamParametersCommand();
	if( Status == CodecNoError )
	    Status	= SendMMEStreamParameters();
	if( Status != CodecNoError )
	{
	    report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to fill out, and send, a set stream parameters command.\n", Configuration.CodecName );
	    ForceStreamParameterReload		= true;
	    StreamParameterContextBuffer->DecrementReferenceCount();
	    StreamParameterContextBuffer	= NULL;
	    StreamParameterContext		= NULL;
	    return Status;
	}

	StreamParameterContextBuffer	= NULL;
	StreamParameterContext		= NULL;
    }

    //
    // If there is no frame to decode then exit now.
    //

    if( !ParsedFrameParameters->NewFrameParameters )
	return CodecNoError;

    //
    // Test if this frame indicates completion of any previous decodes (for slice decoding)
    //

    if( Configuration.SliceDecodePermitted &&
	ParsedVideoParameters->FirstSlice &&
	(CurrentDecodeBufferIndex != INVALID_INDEX) &&
	(BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure == StructureFrame) )
    {
	// Operation cannot fail
	SetOutputOnDecodesComplete( CurrentDecodeBufferIndex, true );
	CurrentDecodeBufferIndex	= INVALID_INDEX;
    }

    //
    // Check for a half buffer
    //

    if( (CurrentDecodeBufferIndex != INVALID_INDEX) && ParsedFrameParameters->FirstParsedParametersForOutputFrame )
    {
	report( severity_error, "Codec_MmeVideo_c::Input(%s) - New frame starts when we have one field in decode buffer.\n", Configuration.CodecName );
	Codec_MmeVideo_c::OutputPartialDecodeBuffers();
    }

    //
    // Obtain a new buffer if needed
    //

    if( CurrentDecodeBufferIndex == INVALID_INDEX )
    {
	Status	= GetDecodeBuffer();
	if( Status != CodecNoError )
	{
	    report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to get decode buffer.\n", Configuration.CodecName );
	    ReleaseDecodeContext( DecodeContext );
	    return Status;
	}

	//
	// reset the buffer content to contain no data, and derive the chroma and luma pointers.
	//

	State	= &BufferState[CurrentDecodeBufferIndex];

	State->ParsedVideoParameters->PictureStructure	= StructureEmpty;
	State->FieldDecode				= ParsedVideoParameters->PictureStructure != StructureFrame;

	if( Manifestor != NULL )
	{
	    if( State->BufferStructure->ComponentCount == 1 )
	    {
		State->BufferLumaPointer    = NULL;
		State->BufferChromaPointer  = NULL;
		State->BufferRasterPointer  = State->BufferPointer + State->BufferStructure->ComponentOffset[0];
	    }
	    else if( State->BufferStructure->ComponentCount == 2 )
	    {
	    State->BufferLumaPointer	= State->BufferPointer + State->BufferStructure->ComponentOffset[0];
	    State->BufferChromaPointer	= State->BufferPointer + State->BufferStructure->ComponentOffset[1];
		State->BufferRasterPointer  = NULL;
	    }
	    else if( State->BufferStructure->ComponentCount == 4 )
	    {
		      
	       State->BufferLumaPointer	        = State->BufferPointer + State->BufferStructure->ComponentOffset[0];
	       State->BufferChromaPointer	= State->BufferPointer + State->BufferStructure->ComponentOffset[1];
	       
	       State->DecimatedLumaPointer	= State->BufferPointer + State->BufferStructure->ComponentOffset[2];
	       State->DecimatedChromaPointer	= State->BufferPointer + State->BufferStructure->ComponentOffset[3];
	       
	       State->BufferRasterPointer       = NULL;
	    }	   
	    else
		report( severity_fatal, "Codec_MmeVideo_c::Input(%s) - Decode buffer structure contains unsupported number of components (%d).\n", Configuration.CodecName, State->BufferStructure->ComponentCount );
	}
    }

    //
    // If we are re-using the buffer, and this is the first slice 
    // (hence of a second field) we update the field counts in the 
    // buffer record.
    //

    else if( ParsedVideoParameters->FirstSlice )
    {
	Status	= MapBufferToDecodeIndex( ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeBufferIndex );
	if( Status != CodecNoError )
	{
	    report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to map second field index to decode buffer - Implementation error.\n", Configuration.CodecName );
	    Codec_MmeVideo_c::OutputPartialDecodeBuffers();
	    return PlayerImplementationError;
	}

//

	PreviousFieldParameters				= BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters;
	if( PreviousFieldParameters->DisplayCount[1] != 0 )
	{
	    report( severity_error, "Codec_MmeVideo_c::Input(%s) - DisplayCount for second field non-zero after decoding only first field - Implementation error.\n", Configuration.CodecName );
	    Codec_MmeVideo_c::OutputPartialDecodeBuffers();
	    return PlayerImplementationError;
	}

	PreviousFieldParameters->DisplayCount[1]	= ParsedVideoParameters->DisplayCount[0];

//

	if( (PreviousFieldParameters->PanScan.Count + ParsedVideoParameters->PanScan.Count) >
	    MAX_PAN_SCAN_VALUES )
	{
	    report( severity_error, "Codec_MmeVideo_c::Input(%s) - Cummulative PanScanCount in two fields too great (%d + %d) - Implementation error.\n", Configuration.CodecName, PreviousFieldParameters->PanScan.Count, ParsedVideoParameters->PanScan.Count );
	    Codec_MmeVideo_c::OutputPartialDecodeBuffers();
	    return PlayerImplementationError;
	}

	for( i=0; i<ParsedVideoParameters->PanScan.Count; i++ )
	{
	    PreviousFieldParameters->PanScan.DisplayCount[i + PreviousFieldParameters->PanScan.Count]		= ParsedVideoParameters->PanScan.DisplayCount[i];
	    PreviousFieldParameters->PanScan.HorizontalOffset[i + PreviousFieldParameters->PanScan.Count]	= ParsedVideoParameters->PanScan.HorizontalOffset[i];
	    PreviousFieldParameters->PanScan.VerticalOffset[i + PreviousFieldParameters->PanScan.Count]		= ParsedVideoParameters->PanScan.VerticalOffset[i];
	}
	PreviousFieldParameters->PanScan.Count	+= ParsedVideoParameters->PanScan.Count;
    }

    //
    // Record the buffer being used in the decode context
    //

    DecodeContext->BufferIndex	= CurrentDecodeBufferIndex;

    //
    // Translate reference lists, and Update the reference frame access counts
    //

    Status	= TranslateReferenceFrameLists( ParsedVideoParameters->FirstSlice );
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to find all reference frames - Implementation error.\n", Configuration.CodecName );
    }

    //
    // Provide default arguments for the input and output buffers. Default to no buffers not because there
    // are no buffers but because the video firmware interface uses a backdoor to gain access to the buffers.
    // Yes, this does violate the spec. but does nevertheless work on the current crop of systems.
    //

    DecodeContext->MMECommand.NumberInputBuffers		= 0;
    DecodeContext->MMECommand.NumberOutputBuffers		= 0;
    DecodeContext->MMECommand.DataBuffers_p			= NULL;
    
    //
    // Load the parameters into MME command
    //

    Status	= FillOutDecodeCommand();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to fill out a decode command.\n", Configuration.CodecName );
	ReleaseDecodeContext( DecodeContext );

	return Status;
    }

    //
    // Update ongoing decode count, and completion flags
    //

    BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure	|= ParsedVideoParameters->PictureStructure;

    LastSlice			= Configuration.SliceDecodePermitted ? KnownLastSliceInFieldFrame : true;
    SeenBothFields		= BufferState[CurrentDecodeBufferIndex].ParsedVideoParameters->PictureStructure == StructureFrame;
    LastDecodeIntoThisBuffer	= SeenBothFields && LastSlice;

    OS_LockMutex( &Lock );

    DecodeContext->DecodeInProgress	= true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete	= LastDecodeIntoThisBuffer;

    OS_UnLockMutex( &Lock );

    //
    // Ensure that the coded frame will be available throughout the 
    // life of the decode by attaching the coded frame to the decode 
    // context prior to launching the decode.
    //

    DecodeContextBuffer->AttachBuffer( CodedFrameBuffer );

    //! set up MME_TRANSFORM - SendMMEDecodeCommand no longer does this as we nned to do
    //! MME_SEND_BUFFERS instead for certain codecs, WMA being one, OGG Vorbis another
    DecodeContext->MMECommand.CmdCode = MME_TRANSFORM;

    Status	= SendMMEDecodeCommand();
    if( Status != CodecNoError )
    {
	report( severity_error, "Codec_MmeVideo_c::Input(%s) - Failed to send a decode command.\n", Configuration.CodecName );
	ReleaseDecodeContext( DecodeContext );
	return Status;
    }

    //
    // have we finished decoding into this buffer
    //
	
    if( LastDecodeIntoThisBuffer )
    {
	CurrentDecodeBufferIndex	= INVALID_INDEX;
	CurrentDecodeIndex		= INVALID_INDEX;
    }
    else
    {
	CurrentDecodeIndex		= ParsedFrameParameters->DecodeFrameIndex;
    }

//

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The intercept to the initailize data types function, that 
//	ensures the video specific type is recorded in the configuration 
//	record.
//

CodecStatus_t   Codec_MmeVideo_c::InitializeDataTypes(	void )
{
    //
    // Add video specific types, and the address of 
    // parsed video parameters to the configuration 
    // record. Then pass on down to the base class
    //

    Configuration.AudioVideoDataParsedParametersType	= Player->MetaDataParsedVideoParametersType;
    Configuration.AudioVideoDataParsedParametersPointer	= (void **)&ParsedVideoParameters;
    Configuration.SizeOfAudioVideoDataParsedParameters	= sizeof(ParsedVideoParameters_t);

    return Codec_MmeBase_c::InitializeDataTypes();
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The generic video function used to fill out a buffer structure
//	request.
//

CodecStatus_t   Codec_MmeVideo_c::FillOutDecodeBufferRequest(	BufferStructure_t	 *Request )
{
    memset( Request, 0x00, sizeof(BufferStructure_t) );

    Request->Format		= Configuration.DecodeOutputFormat;
   
    if (Configuration.DecimatedDecodePermitted && 
       (Player->PolicyValue (Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled))
    {        
       Request->DimensionCount	= 4;
       Request->DecimationRequired = true;
    }
    else
       Request->DimensionCount	= 2;
     
    Request->Dimension[0]	= ParsedVideoParameters->Content.Width;
    Request->Dimension[1]	= ParsedVideoParameters->Content.Height;

    return CodecNoError;
}



