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

Source file name : demultiplexor_base.h
Author :           Nick

Definition of the base demultiplexor class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
13-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_DEMULTIPLEXOR_BASE
#define H_DEMULTIPLEXOR_BASE

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "player.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define	DEMULTIPLEXOR_MAX_STREAMS	4

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct DemultiplexorBaseStreamContext_s
{
    PlayerStream_t	Stream;
    unsigned int	Identifier;
    Collator_t		Collator;
} DemultiplexorBaseStreamContext_t;

//

struct DemultiplexorBaseContext_s
{
    OS_Mutex_t				  Lock;

    unsigned int			  LastStreamSet;
    PlayerInputDescriptor_t		 *Descriptor;
    unsigned int			  BufferLength;
    unsigned char			 *BufferData;
    DemultiplexorBaseStreamContext_t	  Streams[DEMULTIPLEXOR_MAX_STREAMS];
};

//

typedef struct DemultiplexorBaseContext_s   *DemultiplexorBaseContext_t;

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Demultiplexor_Base_c : public Demultiplexor_c
{
protected:

    // Data

    unsigned int	  SizeofContext;

    // Functions

    void		  SetContextSize(	unsigned int	SizeofContext );

public:

    //
    // Constructor/Destructor methods
    //

    Demultiplexor_Base_c( 	void );
    ~Demultiplexor_Base_c( 	void );

    //
    // Context managment functions
    //

    DemultiplexorStatus_t   CreateContext(	DemultiplexorContext_t	 *Context );

    DemultiplexorStatus_t   DestroyContext(	DemultiplexorContext_t	  Context );

    DemultiplexorStatus_t   AddStream(		DemultiplexorContext_t	  Context,
						PlayerStream_t		  Stream,
						unsigned int		  StreamIdentifier );

    DemultiplexorStatus_t   RemoveStream(	DemultiplexorContext_t	  Context,
						unsigned int		  StreamIdentifier );

    DemultiplexorStatus_t   SwitchStream(	DemultiplexorContext_t	  Context,
						PlayerStream_t		  Stream );

    DemultiplexorStatus_t   InputJump(		DemultiplexorContext_t	  Context );

    DemultiplexorStatus_t   Demux(		PlayerPlayback_t	  Playback,
						DemultiplexorContext_t	  Context,
						Buffer_t		  Buffer );
};

#endif

