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

Source file name : codec_dvp_video.cpp
Author :           Chris

Implementation of the dvp null codec class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
07-Aug-07   Created                                         Chris

************************************************************************/

#include "codec_dvp_video.h"
#include "dvp.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// Class Constructor
//

Codec_DvpVideo_c::Codec_DvpVideo_c()
{
    //
    // Initialize class variables
    //

    DataTypesInitialized		= false;

    //
    // Setup the trick mode parameters
    //

    DvpTrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 120;
    DvpTrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 120;
    DvpTrickModeParameters.SubstandardDecodeSupported        = false;
    DvpTrickModeParameters.SubstandardDecodeRateIncrease     = 1;

    DvpTrickModeParameters.DefaultGroupSize                  = 1;
    DvpTrickModeParameters.DefaultGroupReferenceFrameCount   = 1;

//
   
    InitializationStatus	= CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Class Destructor
//

Codec_DvpVideo_c::~Codec_DvpVideo_c()
{
    BaseComponentClass_c::Reset();
    BaseComponentClass_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
// Register the output buffer ring
//

CodecStatus_t   Codec_DvpVideo_c::RegisterOutputBufferRing( Ring_t Ring )
{
PlayerStatus_t          Status;

//

    OutputRing  = Ring;

    //
    // Obtain the class list
    //

    Player->GetBufferManager( &BufferManager );
    if( Manifestor == NULL )
    {
	report( severity_error, "Codec_DvpVideo_c::RegisterOutputBufferRing - This implementation does not support no-output decoding.\n" );
	return PlayerNotSupported;
    }

    //
    // Obtain the decode buffer pool
    //

    Player->GetDecodeBufferPool( Stream, &DecodeBufferPool );
    if( DecodeBufferPool == NULL )
    {
	report( severity_error, "Codec_DvpVideo_c::RegisterOutputBufferRing(DVP) - This implementation does not support no-output decoding.\n");
	return PlayerNotSupported;
    }

    //
    // Attach the stream specific (audio|video|data)
    // parsed frame parameters to the decode buffer pool.
    //

    Status      = DecodeBufferPool->AttachMetaData(Player->MetaDataParsedVideoParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::RegisterOutputBufferRing(DVP) - Failed to attach stream specific parsed parameters to all decode buffers.\n");
	return Status;
    }

    //
    // Go live
    //

    SetComponentState( ComponentRunning );
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Release a decode buffer - just do a decrement reference count
//

CodecStatus_t   Codec_DvpVideo_c::ReleaseDecodeBuffer( Buffer_t Buffer )
{
#if 0
unsigned int Length;
unsigned char     *Pointer;

    Buffer->ObtainDataReference( &Length, NULL, (void **)(&Pointer), CachedAddress );
    memset( Pointer, 0x10, 0xa8c00 );
#endif

    Buffer->DecrementReferenceCount();
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Input a buffer - just pass it on
//

CodecStatus_t   Codec_DvpVideo_c::Input( Buffer_t CodedBuffer )
{
CodecStatus_t			 Status;
unsigned int			 CodedDataLength;
StreamInfo_t			*StreamInfo;
Buffer_t			 MarkerBuffer;
BufferStructure_t		 BufferStructure;
ParsedFrameParameters_t		*ParsedFrameParameters;
ParsedVideoParameters_t		*ParsedVideoParameters;
Buffer_t			 CapturedBuffer;
ParsedVideoParameters_t		*CapturedParsedVideoParameters;

    //
    // Extract the useful coded data information
    //

    Status      = CodedBuffer->ObtainDataReference( NULL, &CodedDataLength, (void **)(&StreamInfo), CachedAddress );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Unable to obtain data reference.\n" );
	return Status;
    }

    Status      = CodedBuffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Unable to obtain the meta data \"ParsedFrameParameters\".\n" );
	return Status;
    }

    Status      = CodedBuffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void**)&ParsedVideoParameters );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Unable to obtain the meta data \"ParsedVideoParameters\".\n" );
	return Status;
    }

    //
    // Handle the special case of a marker frame
    //

    if( (CodedDataLength == 0) && !ParsedFrameParameters->NewStreamParameters && !ParsedFrameParameters->NewFrameParameters )
    {
	//
	// Get a marker buffer
	//

	memset( &BufferStructure, 0x00, sizeof(BufferStructure_t) );
	BufferStructure.Format  = FormatMarkerFrame;

	Status      = Manifestor->GetDecodeBuffer( &BufferStructure, &MarkerBuffer );
	if( Status != ManifestorNoError )
	{
	    report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Failed to get marker decode buffer from manifestor.\n");
	    return Status;
	}

	MarkerBuffer->TransferOwnership( IdentifierCodec );

	Status      = MarkerBuffer->AttachMetaData( Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Unable to attach a reference to \"ParsedFrameParameters\" to the marker buffer.\n");
	    return Status;
	}

	MarkerBuffer->AttachBuffer( CodedBuffer );

	//
	// Queue/pass on the buffer
	//

	OutputRing->Insert( (unsigned int)MarkerBuffer );
	return CodecNoError;
    }

    //
    // Attach the coded data fields to the decode/captured buffer
    //

    CapturedBuffer	= (Buffer_t)StreamInfo->buffer_class;
    if( CapturedBuffer == NULL )
    {
	report( severity_fatal,"Codec_DvpVideo_c::Input(DVP) - NULL Buffer\n" );
	return CodecNoError;
    }

//

    Status      = CapturedBuffer->ObtainMetaDataReference( Player->MetaDataParsedVideoParametersType, (void**)&CapturedParsedVideoParameters );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Unable to obtain the meta data \"ParsedVideoParameters\" from the captured buffer.\n" );
	return Status;
    }

    memcpy( CapturedParsedVideoParameters, ParsedVideoParameters, sizeof(ParsedVideoParameters_t) );

//

    Status      = CapturedBuffer->AttachMetaData( Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Failed to attach Frame Parameters\n");
	return Status;
    }

    //
    // Switch the ownership hierarchy, and allow the captured buffer to exist on it's own.
    //

    CapturedBuffer->IncrementReferenceCount();

    Status	= CodedBuffer->DetachBuffer( CapturedBuffer );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Failed to detach captured buffer from coded frame buffer\n");
	return Status;
    }

    Status      = CapturedBuffer->AttachBuffer( CodedBuffer );
    if( Status != BufferNoError )
    {
	report( severity_error, "Codec_DvpVideo_c::Input(DVP) - Failed to attach captured buffer to Coded Frame Buffer\n");
	return Status;
    }

    //
    // Pass the captured buffer on
    //

    OutputRing->Insert( (unsigned int)CapturedBuffer );

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Return the trick mode parameters
//

CodecStatus_t   Codec_DvpVideo_c::GetTrickModeParameters( CodecTrickModeParameters_t    *TrickModeParameters )
{
    *TrickModeParameters    = DvpTrickModeParameters;
    return CodecNoError;
}


