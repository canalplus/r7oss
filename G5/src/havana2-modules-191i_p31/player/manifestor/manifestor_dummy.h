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

Source file name : manifestor_dummy.h
Author :           Nick

Dummy instance of the manifestor class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Feb-07   Created                                         Nick

************************************************************************/

#ifndef H_MANIFESTOR_DUMMY
#define H_MANIFESTOR_DUMMY

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

// ---------------------------------------------------------------------
//
// Class definition
//

class Manifestor_Dummy_c : public Manifestor_c
{
    BufferManager_t			 BufferManager;
    BufferPool_t			 BufferPool;

    unsigned char			 Memory[8*1024*1024];
    void				*MemoryPointers[3];

    VideoOutputSurfaceDescriptor_t	 OutputSurfaceDescriptor;

    bool				 GotEventRecord;
    PlayerEventRecord_t			 EventRecord;

    ParsedFrameParameters_t		*ParsedFrameParameters;

    Ring_t				 OutputRing;

public:

    Manifestor_Dummy_c(	void )
    { report( severity_info, "Manifestor_Dummy_c - Called\n" ); }

    ~Manifestor_Dummy_c(	void )
    { report( severity_info, "~Manifestor_Dummy_c - Called\n" ); }


    ManifestorStatus_t   GetDecodeBufferPool(		BufferPool_t		 *Pool )
	    {
		#define BUFFER_DECODE_BUFFER             "DecodeBuffer"
		#define BUFFER_DECODE_BUFFER_TYPE   	 {BUFFER_DECODE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 256, 0x1000, false, false, 0}
		static BufferDataDescriptor_t     	 InitialDecodeBufferDescriptor = BUFFER_DECODE_BUFFER_TYPE;
		BufferDataDescriptor_t			*DecodeBufferDescriptor;
		BufferType_t				 DecodeBufferType;
		BufferStatus_t				 Status;
//

		report( severity_info, "Manifestor_Dummy_c::GetDecodeBufferPool - Called\n" ); 

		Player->GetBufferManager( &BufferManager );
	        Status  = BufferManager->CreateBufferDataType( &InitialDecodeBufferDescriptor, &DecodeBufferType );
        	if( Status != BufferNoError )
	        {
        	    report( severity_error, "Manifestor_Dummy_c::GetDecodeBufferPool - Failed to create the decode buffer data type.\n" );
	            return ManifestorError;
        	}
		BufferManager->GetDescriptor( DecodeBufferType, BufferDataTypeBase, &DecodeBufferDescriptor );

//

		MemoryPointers[CachedAddress]	= Memory;
		MemoryPointers[UnCachedAddress]	= Memory;
		MemoryPointers[PhysicalAddress]	= Memory;
		Status     = BufferManager->CreatePool( Pool, DecodeBufferType, 32, 8*1024*1024, MemoryPointers );
        	if( Status != BufferNoError )
	        {
        	    report( severity_error, "Manifestor_Dummy_c::GetDecodeBufferPool - Failed to create the pool.\n" );
	            return ManifestorError;
        	}

//

		BufferPool		= *Pool;

//

		BufferPool->AttachMetaData( Player->MetaDataBufferStructureType );

//

		return ManifestorNoError; 
	    }


    ManifestorStatus_t   RegisterOutputBufferRing(	Ring_t			  Ring )
	    { 
		OutputRing	= Ring;
		return ManifestorNoError;
	    }

//

    ManifestorStatus_t   GetSurfaceParameters(		void			**SurfaceParameters )
	    { 
		// Display information
		OutputSurfaceDescriptor.DisplayWidth	= 720;
		OutputSurfaceDescriptor.DisplayHeight	= 576;
		OutputSurfaceDescriptor.Progressive	= true;
		OutputSurfaceDescriptor.FrameRate	= 50;

		*SurfaceParameters	= &OutputSurfaceDescriptor;
		return ManifestorNoError;
	    }

//

    ManifestorStatus_t   GetNextQueuedManifestationTime(unsigned long long	 *Time)
		{ report( severity_info, "Manifestor_Dummy_c::GetNextQueuedManifestationTime - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(	void )
		{ report( severity_info, "Manifestor_Dummy_c::ReleaseQueuedDecodeBuffers - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   InitialFrame(		Buffer_t		  Buffer )
		{ report( severity_info, "Manifestor_Dummy_c::InitialFrame - Called\n" ); return ManifestorNoError; }

//

    ManifestorStatus_t   QueueDecodeBuffer(		Buffer_t		  Buffer )
	    {
		ManifestorStatus_t	Status;

		Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters) );
		if( Status != PlayerNoError )
                {
		    report( severity_error, "Manifestor_Dummy_c::QueueDecodeBuffer - Unable to obtain the meta data \"ParsedFrameParameters\".\n" );
		    return ManifestorError;
		}

		if( GotEventRecord )
		{
		    if( EventRecord.PlaybackTime == INVALID_TIME )
			EventRecord.PlaybackTime	= ParsedFrameParameters->NativePlaybackTime;

		    Player->SignalEvent( &EventRecord );
		    GotEventRecord	= false;
		}

		report( severity_info, "Manifestor_Dummy_c::QueueDecodeBuffer - Queueing %3d - Native playback time %016llx\n", ParsedFrameParameters->DisplayFrameIndex, ParsedFrameParameters->NativePlaybackTime );
		OutputRing->Insert( (unsigned int)Buffer );
		return ManifestorNoError;
	    }

//

    ManifestorStatus_t   QueueNullManifestation(	void )
		{ report( severity_info, "Manifestor_Dummy_c::QueueNullManifestation - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   QueueEventSignal(		PlayerEventRecord_t	 *Event )
	    {
		GotEventRecord	= true;
		memcpy( &EventRecord, Event, sizeof(PlayerEventRecord_t) );
	        return ManifestorNoError;
	    }

//

    ManifestorStatus_t   GetDecodeBuffer(
					               BufferStructure_t        *RequestedStructure,
                                                       Buffer_t                 *Buffer )
    {
	ManifestorStatus_t	 Status;
	BufferStructure_t	*AttachedRequestStructure;

	RequestedStructure->Dimension[0]	= ((RequestedStructure->Dimension[0] + 31) / 32) * 32;
	RequestedStructure->Dimension[1]	= ((RequestedStructure->Dimension[1] + 31) / 32) * 32;

	RequestedStructure->ComponentCount	= 2;
	RequestedStructure->ComponentOffset[0]	= 0;
	RequestedStructure->ComponentOffset[1]	= RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

	RequestedStructure->Strides[0][0]	= RequestedStructure->Dimension[0];
	RequestedStructure->Strides[0][1]	= RequestedStructure->Dimension[0];

	RequestedStructure->Size		= (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 3)/2;

	Status	= BufferPool->GetBuffer( Buffer, IdentifierManifestor, RequestedStructure->Size );

	if( Status == BufferNoError )
	{
	    (*Buffer)->ObtainMetaDataReference( Player->MetaDataBufferStructureType,
                                                (void **)(&AttachedRequestStructure) );
             memcpy( AttachedRequestStructure, RequestedStructure, sizeof(BufferStructure_t) );
	}

	return Status;
    }

    ManifestorStatus_t   GetDecodeBufferCount(
						        BufferStructure_t        *RequestedStructure,
                                                        unsigned int             *Count )
    {
	RequestedStructure->Dimension[0]	= ((RequestedStructure->Dimension[0] + 31) / 32) * 32;
	RequestedStructure->Dimension[1]	= ((RequestedStructure->Dimension[1] + 31) / 32) * 32;

	RequestedStructure->ComponentCount	= 2;
	RequestedStructure->ComponentOffset[0]	= 0;
	RequestedStructure->ComponentOffset[1]	= RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1];

	RequestedStructure->Strides[0][0]	= RequestedStructure->Dimension[0];
	RequestedStructure->Strides[0][1]	= RequestedStructure->Dimension[0];

	RequestedStructure->Size		= (RequestedStructure->Dimension[0] * RequestedStructure->Dimension[1] * 3)/2;

	*Count	= (8*1024*1024) / RequestedStructure->Size;
	return ManifestorNoError;
    }

};
#endif
