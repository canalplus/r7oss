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

Source file name : demultiplexor.h
Author :           Nick

Definition of the demultiplexor class module for player 2.


Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_DEMULTIPLEXOR
#define H_DEMULTIPLEXOR

#include "player.h"

// ---------------------------------------------------------------------
//
// Local type definitions
//

enum
{
    DemultiplexorNoError			= PlayerNoError,
    DemultiplexorError				= PlayerError,

						// GetBuffer
//    Demultiplexor...				= BASE_DEMULTIPLEXOR
};

typedef PlayerStatus_t	DemultiplexorStatus_t;

// ---------------------------------------------------------------------
//
// Class definition
//

class Demultiplexor_c : public BaseComponentClass_c
{
public:

    virtual DemultiplexorStatus_t    GetHandledMuxType(	PlayerInputMuxType_t	 *HandledType ) = 0;

    virtual DemultiplexorStatus_t    CreateContext(	DemultiplexorContext_t	 *Context ) = 0;

    virtual DemultiplexorStatus_t    DestroyContext(	DemultiplexorContext_t	  Context ) = 0;

    virtual DemultiplexorStatus_t    AddStream(		DemultiplexorContext_t	  Context,
							PlayerStream_t		  Stream,
							unsigned int		  StreamIdentifier ) = 0;

    virtual DemultiplexorStatus_t    RemoveStream(	DemultiplexorContext_t	  Context,
							unsigned int		  StreamIdentifier ) = 0;

    virtual DemultiplexorStatus_t    SwitchStream(	DemultiplexorContext_t	  Context,
							PlayerStream_t		  Stream ) = 0;

    virtual DemultiplexorStatus_t    InputJump(		DemultiplexorContext_t	  Context ) = 0;

    virtual DemultiplexorStatus_t    Demux(		PlayerPlayback_t	  Playback,
							DemultiplexorContext_t	  Context,
							Buffer_t		  Buffer ) = 0;
};

// ---------------------------------------------------------------------
//
// Docuentation
//

/*! \class Demultiplexor_c
\brief Responsible for taking as input a block of multiplexed data and splitting this into stream specific data.

The demultiplexor class is responsible for taking as input a block of 
multiplexed data and splitting this into stream specific data. This is 
a list of its entrypoints, and a partial list of the calls it makes, 
and the data structures it accesses, these are in addition to the 
standard class entrypointsm, and the complete list of support 
entrypoints in the Player class.

The partial list of entrypoints used by this class:

- Supplied by the collator class:
  - <b>Input</b>, Used to pass on data blocks to the appropriate collators.

The partial list of meta data types used by this class:

- Attached to input buffers:
  - <b>PlayerInputDescriptor</b>, Describes the input block.

- There are no output buffers, but as part of the collator call parameter list, the following is attached:
  - <b>PlayerInputDescriptor</b>, Describing the block being passed.

*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::GetHandledMuxType(PlayerInputMuxType_t *HandledType)
\brief Get the data format that this demultiplexor handles.

\param HandledType A pointer to be filled with the MuxType that this class handles.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::CreateContext(DemultiplexorContext_t *Context)
\brief Create a demultiplexor context.

<b>TODO: This method is not adequately documented.</b>

\param Context A pointer to a DemultiplexorContext_t, which is to be filled with an identifier for the created context.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::DestroyContext(DemultiplexorContext_t Context)
\brief Destroy a demultiplexor context.

\param Context The identifier of the context to be destroyed.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::AddStream(DemultiplexorContext_t Context, PlayerStream_t Stream, unsigned int StreamIdentifier)
\brief Add a demultiplexor stream.

<b>TODO: This method is not adequately documented.</b>

\param Context          A demultiplexor context identifier.
\param Stream           A player stream identifier.
\param StreamIdentifier The demultiplexors native stream identifier (i.e. a PID for MPEG transport stream, a sub stream identifier for DVD-PES).

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/
	
/*! \fn DemultiplexorStatus_t Demultiplexor_c::RemoveStream(DemultiplexorContext_t Context, unsigned int StreamIdentifier)
\brief Remove a demultiplexor stream.

\param Context          A demultiplexor context identifier.
\param StreamIdentifier The demultiplexors native stream identifier (i.e. a PID for MPEG transport stream, a sub stream identifier for DVD-PES).

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::SwitchStream(DemultiplexorContext_t Context, PlayerStream_t Stream)
\brief Inform a context that a stream has switched.

This call informs a demupltiplexor context that a switch stream event has occurred, consequently any record
it has regarding the component classes of the stream is now out of date and should be refreshed.

\param Context          A demultiplexor context identifier.
\param Stream           A player stream identifier.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/
	
/*! \fn DemultiplexorStatus_t Demultiplexor_c::InputJump( DemultiplexorContext_t Context )
\brief Inform the demultiplxor that a jump has occurred.

\param Context          A demultiplexor context identifier.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

/*! \fn DemultiplexorStatus_t Demultiplexor_c::Demux(PlayerPlayback_t Playback, DemultiplexorContext_t Context, Buffer_t Buffer)
\brief Demultiplex the supplied data buffer.

This is the workhorse function of this class, when it returns, the 
input block will have been demultiplexed, and all components sent to 
the appropriate collator functions.

\param Playback         Playback identifier.
\param Context          A demultiplexor context identifier.
\param Buffer           A pointer to a Buffer_c instance.

\return Demultiplexor status code, DemultiplexorNoError indicates success.
*/

#endif
