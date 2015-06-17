/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : allocator.h
Author :           Nick

Definition of the pure virtual class defining the interface to a generic
allocator of regions of memory.


Date        Modification                                    Name
----        ------------                                    --------
29-Jun-05   Created                                         Nick

************************************************************************/

#ifndef H_ALLOCATOR
#define H_ALLOCATOR

//

typedef enum
{
    AllocatorNoError            = 0,
    AllocatorError,
    AllocatorNoMemory,
    AllocatorUnableToAllocate
} AllocatorStatus_t;

//

class Allocator_c
{
public:

    AllocatorStatus_t   InitializationStatus;

    virtual ~Allocator_c( void ) {};

    virtual AllocatorStatus_t Allocate( unsigned int      Size,
					unsigned char   **Block,
					bool              NonBlocking   = false ) = 0;

    virtual AllocatorStatus_t AllocateLargest( 
					unsigned int	 *Size,
					unsigned char	**Block,
					bool              NonBlocking   = false ) = 0;

    virtual AllocatorStatus_t ExtendToLargest( 
					unsigned int	 *Size,
					unsigned char	**Block,
					bool		  ExtendUpwards	= true ) = 0;

    virtual AllocatorStatus_t Free(     void ) = 0;

    virtual AllocatorStatus_t Free(     unsigned int      Size,
					unsigned char    *Block ) = 0;

    virtual AllocatorStatus_t LargestFreeBlock( 	unsigned int	 *Size ) = 0;
};
#endif

