/************************************************************************
Copyright (C) 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : collator_packet_dvp.cpp
Author :           Julian

Implementation of the packet collator for dvp video.


Date        Modification                                    Name
----        ------------                                    --------
08-Jul-08   Created                                         Julian

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_packet_dvp.h"
#include "dvp.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

///////////////////////////////////////////////////////////////////////////
//
// Resets and configures according to the requirements of this stream content
//

CollatorStatus_t Collator_PacketDvp_c::Input(	PlayerInputDescriptor_t  *Input,
						unsigned int		  DataLength,
						void                     *Data,
						bool			  NonBlocking,
						unsigned int		 *DataLengthRemaining )
{
CollatorStatus_t	 Status;
StreamInfo_t		*CapturedFrameDescriptor = (StreamInfo_t *)Data;
Buffer_t		 CapturedBuffer;

//

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_Packet_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    //
    // Attach the decode buffer mentioned in the input packet
    // to the current coded data frame, to ensure release
    // at the appropriate time.
    //

    if( DataLength != sizeof(StreamInfo_t) )
    {
	report( severity_error, "Collator_Packet_c::Input - Packet is wrong size (%d != %d)\n", DataLength, sizeof(StreamInfo_t) );
	return CollatorError;
    }

    CapturedBuffer	= (Buffer_t)(CapturedFrameDescriptor->buffer_class);
    CodedFrameBuffer->AttachBuffer( CapturedBuffer );

    //
    // Perform the standard packet handling
    //

    Status	= Collator_Packet_c::Input( Input, DataLength, Data );

    InputExit();
    return Status;
}

