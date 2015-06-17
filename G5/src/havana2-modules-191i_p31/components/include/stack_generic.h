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

Source file name : stack_generic.h
Author :           Nick

Implementation of the class defining the interface to a simple stack
storage device.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-07   Created                                         Nick

************************************************************************/

#ifndef H_STACK_GENERIC
#define H_STACK_GENERIC

#include "stack.h"

//

class StackGeneric_c : public Stack_c
{
private:

    OS_Mutex_t		 Lock;
    unsigned int 	 Limit;
    unsigned int 	 Level;
    unsigned int	*Storage;

public:

    StackGeneric_c( unsigned int MaxEntries = 16 );
    ~StackGeneric_c( void );

    StackStatus_t Push(	unsigned int	 Value );
    StackStatus_t Pop( 	unsigned int	*Value );
    StackStatus_t Flush( void );
    bool          NonEmpty( void );
};
#endif
