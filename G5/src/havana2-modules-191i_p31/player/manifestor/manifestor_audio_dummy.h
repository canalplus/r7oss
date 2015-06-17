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

Source file name : manifestor_audio.h
Author :           Daniel

Temporary (dummy) audio manifestor to complete the audio pipeline


Date        Modification                                    Name
----        ------------                                    --------
04-May-07   Created (from manifestor_dummy.h)               Daniel

************************************************************************/

#ifndef H_MANIFESTOR_AUDIO_DUMMY
#define H_MANIFESTOR_AUDIO_DUMMY

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

// ---------------------------------------------------------------------
//
// Class definition
//

class Manifestor_AudioDummy_c : public Manifestor_c
{
    BufferManager_t			 BufferManager;
    BufferPool_t			 BufferPool;

    AudioOutputSurfaceDescriptor_t	 OutputSurfaceDescriptor;

    bool				 GotEventRecord;
    PlayerEventRecord_t			 EventRecord;

    ParsedFrameParameters_t		*ParsedFrameParameters;
    ParsedAudioParameters_t		*ParsedAudioParameters;

    Ring_t				 OutputRing;

    allocator_device_t			 MemoryPointersDevice;
    void				*MemoryPointers[3];

public:

    Manifestor_AudioDummy_c(	void )
    {
    	report( severity_info, "Manifestor_AudioDummy_c - Called\n" );
        BufferPool = NULL;
    }

    ~Manifestor_AudioDummy_c(	void )
    {
    	report( severity_info, "~Manifestor_AudioDummy_c - Called\n" );

        if (BufferPool != NULL)
        {
	    AllocatorClose( MemoryPointersDevice );
            BufferManager->DestroyPool (BufferPool);
            BufferPool        = NULL;
        }
    }


    ManifestorStatus_t   GetDecodeBufferPool(		BufferPool_t		 *Pool )
	    {
		#define BUFFER_DECODE_BUFFER             "DecodeBuffer"
		#define BUFFER_DECODE_BUFFER_TYPE   	 {BUFFER_DECODE_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 256, 0x1000, false, false, 0}
		static BufferDataDescriptor_t     	 InitialDecodeBufferDescriptor = BUFFER_DECODE_BUFFER_TYPE;
		BufferDataDescriptor_t			*DecodeBufferDescriptor;
		BufferType_t				 DecodeBufferType;
		BufferStatus_t				 Status;
		allocator_status_t			 AStatus;

//

		report( severity_info, "Manifestor_AudioDummy_c::GetDecodeBufferPool - Called\n" ); 

                // Only create the pool if it doesn't exist and buffers have been created
                if (BufferPool != NULL)
                {
                    *Pool   = BufferPool;
                    return ManifestorNoError;
                }


		Player->GetBufferManager( &BufferManager );
	        Status  = BufferManager->CreateBufferDataType( &InitialDecodeBufferDescriptor, &DecodeBufferType );
        	if( Status != BufferNoError )
	        {
        	    report( severity_error, "Manifestor_AudioDummy_c::GetDecodeBufferPool - Failed to create the decode buffer data type.\n" );
	            return ManifestorError;
        	}
		BufferManager->GetDescriptor( DecodeBufferType, BufferDataTypeBase, &DecodeBufferDescriptor );

//
	
        	AStatus = PartitionAllocatorOpen( &MemoryPointersDevice, SYS_LMI_PARTITION, 6 * 128 * 1024, true );
        	if( AStatus != allocator_ok )
        	{
        	    report( severity_error, "Manifestor_AudioDummy_c::GetDecodeBufferPool - Failed to allocate memory\n" );
        	    return PlayerInsufficientMemory;
        	}

	        MemoryPointers[CachedAddress]         = AllocatorUserAddress( MemoryPointersDevice );
	        MemoryPointers[UnCachedAddress]       = AllocatorUncachedUserAddress( MemoryPointersDevice );
	        MemoryPointers[PhysicalAddress]       = AllocatorPhysicalAddress( MemoryPointersDevice );

//

		Status     = BufferManager->CreatePool( Pool, DecodeBufferType, 32, 6 * 128 * 1024, MemoryPointers );
        	if( Status != BufferNoError )
	        {
        	    report( severity_error, "Manifestor_AudioDummy_c::GetDecodeBufferPool - Failed to create the pool.\n" );
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
		// Decode/display buffer information
		OutputSurfaceDescriptor.BitsPerSample	= 32;
		OutputSurfaceDescriptor.ChannelCount	= 2;
		OutputSurfaceDescriptor.SampleRateHz	= 48000;

		*SurfaceParameters	= &OutputSurfaceDescriptor;
		return ManifestorNoError;
	    }

//

    ManifestorStatus_t   GetNextQueuedManifestationTime(unsigned long long	 *Time)
		{ report( severity_info, "Manifestor_AudioDummy_c::GetNextQueuedManifestationTime - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(	void )
		{ report( severity_info, "Manifestor_AudioDummy_c::ReleaseQueuedDecodeBuffers - Called\n" ); return ManifestorNoError; }

    ManifestorStatus_t   InitialFrame(		Buffer_t		  Buffer )
		{ report( severity_info, "Manifestor_AudioDummy_c::InitialFrame - Called\n" ); return ManifestorNoError; }

//

    ManifestorStatus_t   QueueDecodeBuffer(		Buffer_t		  Buffer )
	    {
		ManifestorStatus_t	Status;

                //report( severity_info, "Manifestor_AudioDummy_c::QueueDecodeBuffer - Called\n" );
		Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters) );
		if( Status != PlayerNoError )
                {
		    report( severity_error, "Manifestor_AudioDummy_c::QueueDecodeBuffer - Unable to obtain the meta data \"ParsedFrameParameters\".\n" );
		    return ManifestorError;
		}

		Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedAudioParametersType, (void **)(&ParsedAudioParameters) );
		if( Status != PlayerNoError )
                {
		    report( severity_error, "Manifestor_AudioDummy_c::QueueDecodeBuffer - Unable to obtain the meta data \"ParsedAudioParameters\".\n" );
		    return ManifestorError;
		}

		if( GotEventRecord )
		{
		    if( EventRecord.PlaybackTime == INVALID_TIME )
			EventRecord.PlaybackTime	= ParsedFrameParameters->NativePlaybackTime;

		    Player->SignalEvent( &EventRecord );
		    GotEventRecord	= false;
		}

		
		//
		// Dump the first four samples of the buffer (assuming it to be a ten channel buffer)
		//
		
		// TODO: this really ought to extract the ParsedAudioParameters and use the number of channels
		//       found there...		

		unsigned int *pcm;
		Status = Buffer->ObtainDataReference( NULL, NULL, (void **) &pcm, CachedAddress );
		if( Status != PlayerNoError )
		{
		    report( severity_error, "Manifestor_AudioDummy_c::QueueDecodeBuffer - Unable to obtain the buffer data parser\n" );
		    return ManifestorError;
		}
		report( severity_info, "Manifestor_AudioDummy_c::QueueDecodeBuffer - Queueing %4d - Normalized playback time %lluus\n",
		                       ParsedFrameParameters->DisplayFrameIndex, ParsedFrameParameters->NormalizedPlaybackTime );
	        for (unsigned int i=0; i<4; i++) {
	            unsigned int j = ParsedAudioParameters->Source.ChannelCount * i;
	            report (severity_info, "Sample[%3d] (24-bits): %06x %06x %06x %06x %06x %06x %06x %06x %06x %06x\n", i,
		                           pcm[j+0]>>8, pcm[j+1]>>8, pcm[j+2]>>8, pcm[j+3]>>8, pcm[j+4]>>8,
		                           pcm[j+5]>>8, pcm[j+6]>>8, pcm[j+7]>>8, pcm[j+8]>>8, pcm[j+9]>>8);
	        }


	        OutputRing->Insert( (unsigned int)Buffer );
		return ManifestorNoError;
	    }

//

    ManifestorStatus_t   QueueNullManifestation(	void )
		{ report( severity_info, "Manifestor_AudioDummy_c::QueueNullManifestation - Called\n" ); return ManifestorNoError; }

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

	RequestedStructure->ComponentCount	= 1;
	RequestedStructure->ComponentOffset[0]	= 0;

	RequestedStructure->Strides[0][0]	= RequestedStructure->Dimension[0]/8;
	RequestedStructure->Strides[1][0]	= (RequestedStructure->Dimension[0]/8) * RequestedStructure->Dimension[1];

	RequestedStructure->Size		= (RequestedStructure->Dimension[0]/8) * RequestedStructure->Dimension[1] * RequestedStructure->Dimension[2];

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
	RequestedStructure->ComponentCount	= 1;
	RequestedStructure->ComponentOffset[0]	= 0;

	RequestedStructure->Strides[0][0]	= RequestedStructure->Dimension[0]/8;
	RequestedStructure->Strides[1][0]	= (RequestedStructure->Dimension[0]/8) * RequestedStructure->Dimension[1];

	RequestedStructure->Size		= (RequestedStructure->Dimension[0]/8) * RequestedStructure->Dimension[1] * RequestedStructure->Dimension[2];

	*Count	= (6 * 128 * 1024) / RequestedStructure->Size;
	return ManifestorNoError;
    }
};

#endif // H_MANIFESTOR_AUDIO_DUMMY
