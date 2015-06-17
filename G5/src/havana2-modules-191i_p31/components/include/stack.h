/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : stack.h
Author :           Nick

Definition of the pure virtual class defining the interface to a simple stack
storage device.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_STACK
#define H_STACK

#include "osinline.h"

typedef enum 
{
    StackNoError		= 0,
    StackNoMemory,
    StackTooManyEntries,
    StackNothingToGet
} StackStatus_t;

//

class Stack_c
{
public:

    StackStatus_t	InitializationStatus;

    virtual ~Stack_c( void ) {};

    virtual StackStatus_t Push(	unsigned int	 Value ) = 0;
    virtual StackStatus_t Pop( 	unsigned int	*Value ) = 0;
    virtual StackStatus_t Flush( void ) = 0;
    virtual bool          NonEmpty( void ) = 0;
};

typedef class Stack_c	*Stack_t;
#endif
