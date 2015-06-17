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

Source file name : collator_packet_dvp.h
Author :           Nick

Definition of the dvp extension to the basic packet input


Date        Modification                                    Name
----        ------------                                    --------
08-Jul-08   Created					    Nick

************************************************************************/

#ifndef H_COLLATOR_PACKET_DVP
#define H_COLLATOR_PACKET_DVP

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_packet.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PacketDvp_c : public Collator_Packet_c
{
protected:

public:

    //
    // Collator class functions
    //

    CollatorStatus_t   Input(	PlayerInputDescriptor_t  *Input,
				unsigned int		  DataLength,
				void                     *Data,
				bool			  NonBlocking = false,
				unsigned int		 *DataLengthRemaining = NULL );
};

#endif

