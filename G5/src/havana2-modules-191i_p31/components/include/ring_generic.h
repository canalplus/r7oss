/************************************************************************
Copyright (C) 2004 STMicroelectronics. All Rights Reserved.

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

Source file name : ring_blocking.h
Author :           Nick

Definition of the class defining the interface to a simple ring
storage device, that blocks on extraction, and has protected insertion.
It has the inbuild assumption that there can be only one extractor of data.


Date        Modification                                    Name
----        ------------                                    --------
23-Sep-04   Created                                         Nick
15-Nov-06   Revised                                         Nick

************************************************************************/

#ifndef H_RING_GENERIC
#define H_RING_GENERIC

//

#include "ring.h"
#include "osinline.h"

//

class RingGeneric_c : public Ring_c
{
private:

    OS_Mutex_t		 Lock;
    OS_Event_t		 Signal;
    unsigned int 	 Limit;
    unsigned int 	 NextExtract;
    unsigned int 	 NextInsert;
    unsigned int	*Storage;

public:

    RingGeneric_c( unsigned int MaxEntries = 16 );
    ~RingGeneric_c( void );

    RingStatus_t Insert(  unsigned int	 Value );
    RingStatus_t Extract( unsigned int	*Value,
			  unsigned int   BlockingPeriod = OS_INFINITE );
    RingStatus_t Flush( void );
    bool         NonEmpty( void );
};

#endif


