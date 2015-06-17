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

Source file name : buffer_generic.h
Author :           Nick

Common header file bringing together the buffer headers for the generic 
implementation, note the sub-includes are order dependent, at least that 
is the plan.


Date        Modification                                    Name
----        ------------                                    --------
14-Jul-06   Created                                         Nick

************************************************************************/


#ifndef H_BUFFER_GENERIC
#define H_BUFFER_GENERIC

//

#include "player.h"
#include "allocinline.h"
#include "allocator.h"

//

#define MAX_BUFFER_DATA_TYPES		256		// Number of data types, no restriction, uses minimal memory
#define MAX_BUFFER_OWNER_IDENTIFIERS	4
#define MAX_ATTACHED_BUFFERS		4

#define BUFFER_MAXIMUM_EVENT_WAIT	50		// ms

#define TYPE_INDEX_MASK			(MAX_BUFFER_DATA_TYPES - 1)
#define TYPE_TYPE_MASK			(~TYPE_INDEX_MASK)

//

typedef struct BlockDescriptor_s 	*BlockDescriptor_t;

typedef  class Buffer_Generic_c		*Buffer_Generic_t;
typedef  class BufferPool_Generic_c	*BufferPool_Generic_t;
typedef  class BufferManager_Generic_c	*BufferManager_Generic_t;

//

#include "buffer_individual_generic.h"
#include "buffer_pool_generic.h"
#include "buffer_manager_generic.h"

//

#endif
