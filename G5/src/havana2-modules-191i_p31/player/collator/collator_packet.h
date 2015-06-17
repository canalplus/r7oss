/************************************************************************
Copyright (C) 2006, 2008 STMicroelectronics. All Rights Reserved.

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

Source file name : collator_packet.h
Author :           Nick

Definition of the base collator packet class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Jul-08   Created                                         Nick

************************************************************************/

#ifndef H_COLLATOR_PACKET
#define H_COLLATOR_PACKET

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_Packet_c : public Collator_Base_c
{
protected:

    // Data

    // Functions

//

public:

    //
    // Constructor/Destructor methods
    //

    Collator_Packet_c( 	void );

    //
    // Base class overrides
    //

    CollatorStatus_t   Reset( void );

    //
    // Collator class functions
    //

    CollatorStatus_t   Input(	PlayerInputDescriptor_t  *Input,
				unsigned int		  DataLength,
				void                     *Data,
				bool			  NonBlocking = false );
};

#endif


