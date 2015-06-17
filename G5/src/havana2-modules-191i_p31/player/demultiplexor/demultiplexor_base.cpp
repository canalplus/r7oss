/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : demultiplexor_base.cpp
Author :           Nick

Implementation of the base demultiplexor class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
13-Nov-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "demultiplexor_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Demultiplexor_Base_c::Demultiplexor_Base_c( void )
{
    InitializationStatus        = DemultiplexorError;

//

    InitializationStatus        = DemultiplexorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Demultiplexor_Base_c::~Demultiplexor_Base_c(    void )
{
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Create context function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::CreateContext(    DemultiplexorContext_t   *Context )
{
unsigned int			i;
DemultiplexorContext_t		NewContext;
DemultiplexorBaseContext_t      BaseContext;

//

    NewContext  = (DemultiplexorContext_t)new unsigned char[SizeofContext];
    memset( NewContext, 0x00, SizeofContext );

    BaseContext = (DemultiplexorBaseContext_t)NewContext;
    for( i=0; i<DEMULTIPLEXOR_MAX_STREAMS; i++ )
    {
	BaseContext->Streams[i].Stream		= NULL;
	BaseContext->Streams[i].Identifier	= INVALID_INDEX;
    }

    OS_InitializeMutex( &BaseContext->Lock );

    *Context    = NewContext;
    return DemultiplexorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Destroy context function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::DestroyContext(   DemultiplexorContext_t    Context )
{
DemultiplexorBaseContext_t      BaseContext = (DemultiplexorBaseContext_t)Context;

    OS_LockMutex( &BaseContext->Lock );

    OS_TerminateMutex( &BaseContext->Lock );

    delete (unsigned char *)Context;
    return DemultiplexorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The add a stream to a context function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::AddStream(
						DemultiplexorContext_t    Context,
						PlayerStream_t            Stream,
						unsigned int              StreamIdentifier )
{
unsigned int                    i;
DemultiplexorBaseContext_t      BaseContext = (DemultiplexorBaseContext_t)Context;

//
    report( severity_info, "Demultiplexor_Base_c::AddStream - %x, %d.\n", Stream, StreamIdentifier );

    OS_LockMutex( &BaseContext->Lock );
    for( i=0; i<DEMULTIPLEXOR_MAX_STREAMS; i++ )
	if( BaseContext->Streams[i].Stream == NULL )
	{
	    Player->GetClassList( Stream, &BaseContext->Streams[i].Collator, NULL, NULL, NULL, NULL );
	    BaseContext->Streams[i].Stream      = Stream;
	    BaseContext->Streams[i].Identifier  = StreamIdentifier;
	    BaseContext->LastStreamSet          = i;

	    Player->AttachDemultiplexor( Stream, this, Context );

	    OS_UnLockMutex( &BaseContext->Lock );
	    return DemultiplexorNoError;
	}

//

    OS_UnLockMutex( &BaseContext->Lock );
    report( severity_error, "Demultiplexor_Base_c::AddStream - Too many streams.\n" );
    return DemultiplexorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The remove a stream from a context function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::RemoveStream(
						DemultiplexorContext_t    Context,
						unsigned int              StreamIdentifier )
{
unsigned int                    i,j;
DemultiplexorBaseContext_t      BaseContext = (DemultiplexorBaseContext_t)Context;

//

    report( severity_info, "Demultiplexor_Base_c::RemoveStream - %d.\n", StreamIdentifier );

    OS_LockMutex( &BaseContext->Lock );
    for( i=0; i<DEMULTIPLEXOR_MAX_STREAMS; i++ )
	if( BaseContext->Streams[i].Identifier == StreamIdentifier )
	{
	    //
	    // Scan, and if no other identifier (pid in ts) is injecting to the same stream
	    // then we can detach the multiplexor from it. NOTE this means we will work if you
	    // have multiple identifiers going to the same stream.
	    //

	    for( j=0; j<DEMULTIPLEXOR_MAX_STREAMS; j++ )
		if( (j != i) && (BaseContext->Streams[i].Stream == BaseContext->Streams[j].Stream) )
		    break;

	    if( j == DEMULTIPLEXOR_MAX_STREAMS )
		Player->DetachDemultiplexor( BaseContext->Streams[i].Stream );

	    //
	    // release and return.
	    //

	    BaseContext->Streams[i].Stream      = NULL;
	    BaseContext->Streams[i].Identifier	= INVALID_INDEX;
	}

//

    OS_UnLockMutex( &BaseContext->Lock );
    return DemultiplexorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The switch a stream function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::SwitchStream(
						DemultiplexorContext_t    Context,
						PlayerStream_t            Stream )
{
unsigned int                    i;
DemultiplexorBaseContext_t      BaseContext = (DemultiplexorBaseContext_t)Context;

//
    report( severity_info, "Demultiplexor_Base_c::SwitchStream - %x.\n", Stream );

    for( i=0; i<DEMULTIPLEXOR_MAX_STREAMS; i++ )
	if( BaseContext->Streams[i].Stream == Stream )
	    Player->GetClassList( Stream, &BaseContext->Streams[i].Collator, NULL, NULL, NULL, NULL );

//

    return DemultiplexorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The default input jump function currently does nothing
//

DemultiplexorStatus_t   Demultiplexor_Base_c::InputJump(
						DemultiplexorContext_t    Context )
{
    return DemultiplexorError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The demux function
//

DemultiplexorStatus_t   Demultiplexor_Base_c::Demux(
						PlayerPlayback_t          Playback,
						DemultiplexorContext_t    Context,
						Buffer_t                  Buffer )
{
PlayerStatus_t                    Status;
DemultiplexorBaseContext_t        BaseContext = (DemultiplexorBaseContext_t)Context;

//

    Status      = Buffer->ObtainMetaDataReference( Player->MetaDataInputDescriptorType, (void **)(&BaseContext->Descriptor) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Demultiplexor_Base_c::Demux - Unable to obtain the meta data input descriptor.\n" );
	return Status;
    }

//

    Status  = Buffer->ObtainDataReference( NULL, &BaseContext->BufferLength, (void **)(&BaseContext->BufferData) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Demultiplexor_Base_c::Demux - unable to obtain data reference.\n" );
	return Status;
    }

//

    return DemultiplexorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Set the context size
//

void   Demultiplexor_Base_c::SetContextSize(    unsigned int    SizeofContext )
{
    this->SizeofContext = SizeofContext;
}

