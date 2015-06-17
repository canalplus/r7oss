/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : ring.h
Author :           Nick

Definition of the pure virtual class defining the interface to a simple ring
storage device.


Date        Modification                                    Name
----        ------------                                    --------
11-Dec-03   Created                                         Nick

************************************************************************/

#ifndef H_RING
#define H_RING

#include "osinline.h"

typedef enum 
{
    RingNoError		= 0,
    RingNoMemory,
    RingTooManyEntries,
    RingNothingToGet
} RingStatus_t;

//

#define	RING_NONE_BLOCKING	0

//

class Ring_c
{
public:

    RingStatus_t	InitializationStatus;

    virtual ~Ring_c( void ) {};

    virtual RingStatus_t Insert(  unsigned int	 Value ) = 0;
    virtual RingStatus_t Extract( unsigned int  *Value,
                                  unsigned int   BlockingPeriod = RING_NONE_BLOCKING ) = 0;
    virtual RingStatus_t Flush( void ) = 0;
    virtual bool         NonEmpty( void ) = 0;
};

typedef class Ring_c	*Ring_t;
#endif
