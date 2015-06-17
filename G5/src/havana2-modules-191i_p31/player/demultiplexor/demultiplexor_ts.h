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

Source file name : demultiplexor_ts.h
Author :           Nick

Definition of the transport stream demultiplexor class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
13-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_DEMULTIPLEXOR_TS
#define H_DEMULTIPLEXOR_TS

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers
#include "demultiplexor_base.h"
#include "dvb.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DEMULTIPLEXOR_SELECT_ON_PRIORITY	0x00002000
#define DEMULTIPLEXOR_PRIORITY_HIGH		0x00004000
#define DEMULTIPLEXOR_PRIORITY_LOW		0x00000000

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct DemultiplexorStreamContext_s
{
    bool		ValidExpectedContinuityCount;
    unsigned char	ExpectedContinuityCount;
    bool                SelectOnPriority;
    bool                DesiredPriority;
    unsigned long long	TimeOfLastDiscontinuityPrint;
} DemultiplexorStreamContext_t;

//

struct DemultiplexorContext_s
{
    struct DemultiplexorBaseContext_s	Base;

    DemultiplexorStreamContext_t	Streams[DEMULTIPLEXOR_MAX_STREAMS];
    unsigned char			PidTable[DVB_MAX_PIDS];

    bool                                AddedNewStream;
    unsigned int                        BluRayExtraData;
};


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Demultiplexor_Ts_c : public Demultiplexor_Base_c
{
private:

    // Data

    // Functions

public:

    //
    // Constructor/Destructor methods 
    //

    Demultiplexor_Ts_c( void );

    //
    // API functions
    //

    DemultiplexorStatus_t   GetHandledMuxType(	PlayerInputMuxType_t	 *HandledType );

    DemultiplexorStatus_t   AddStream(		DemultiplexorContext_t	  Context,
						PlayerStream_t		  Stream,
						unsigned int		  StreamIdentifier );

    DemultiplexorStatus_t   RemoveStream(	DemultiplexorContext_t	  Context,
						unsigned int		  StreamIdentifier );

    DemultiplexorStatus_t   InputJump(		DemultiplexorContext_t	  Context );

    DemultiplexorStatus_t   Demux(		PlayerPlayback_t	  Playback,
						DemultiplexorContext_t	  Context,
						Buffer_t		  Buffer );
};

#endif

