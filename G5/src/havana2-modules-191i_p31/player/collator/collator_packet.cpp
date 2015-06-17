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

Source file name : collator_packet.cpp
Author :           Nick

Implementation of the generic packet collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Jul-08   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_packet.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

////////////////////////////////////////////////////////////////////////////
//
// Initialize the class by resetting it.
//

Collator_Packet_c::Collator_Packet_c( void )
{
    if( InitializationStatus != CollatorNoError )
	return;

    Collator_Packet_c::Reset();
}


////////////////////////////////////////////////////////////////////////////
//
// Resets and configures according to the requirements of this stream content
//

CollatorStatus_t Collator_Packet_c::Reset( void )
{
CollatorStatus_t Status;

//

    Status = Collator_Base_c::Reset();
    if( Status != CollatorNoError )
	return Status;

    Configuration.GenerateStartCodeList      = false;					// Packets have no start codes
    Configuration.MaxStartCodes              = 0;
    Configuration.StreamIdentifierMask       = 0x00;
    Configuration.StreamIdentifierCode       = 0x00;
    Configuration.BlockTerminateMask         = 0x00;
    Configuration.BlockTerminateCode         = 0x00;
    Configuration.IgnoreCodesRangeStart      = 0x00;
    Configuration.IgnoreCodesRangeEnd        = 0x00;	
    Configuration.InsertFrameTerminateCode   = false;
    Configuration.TerminalCode               = 0x00;
    Configuration.ExtendedHeaderLength       = 0;
    Configuration.DeferredTerminateFlag      = false;

    return CollatorNoError;
}


////////////////////////////////////////////////////////////////////////////
//
// Input, simply take the supplied packet and pass it on
//

CollatorStatus_t   Collator_Packet_c::Input(	PlayerInputDescriptor_t  *Input,
						unsigned int		  DataLength,
						void                     *Data,
						bool			  NonBlocking )
{
CollatorStatus_t Status;

//

    COLLATOR_ASSERT( !NonBlocking );
    AssertComponentState( "Collator_Packet_c::Input", ComponentRunning );
    InputEntry( Input, DataLength, Data, NonBlocking );

    //
    // Extract the descriptor timing information
    //

    ActOnInputDescriptor( Input );

    //
    // Transfer the packet to the next coded data frame and pass on
    //

    Status	= AccumulateData( DataLength, (unsigned char *)Data );
    if( Status == CollatorNoError )
	Status	= InternalFrameFlush();

//

    InputExit();
    return Status;
}

