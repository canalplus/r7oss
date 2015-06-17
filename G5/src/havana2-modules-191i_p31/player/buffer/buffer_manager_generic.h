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

Source file name : buffer_manager_generic.h
Author :           Nick

Implementation of the class definition of the buffer manager class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
14-Jul-06   Created                                         Nick

************************************************************************/

#ifndef H_BUFFER_MANAGER_GENERIC
#define H_BUFFER_MANAGER_GENERIC

//

class BufferManager_Generic_c : public BufferManager_c
{
friend class BufferPool_Generic_c;
friend class Buffer_Generic_c;

private:

    // Data

    OS_Mutex_t			Lock;

    unsigned int		TypeDescriptorCount;
    BufferDataDescriptor_t	TypeDescriptors[MAX_BUFFER_DATA_TYPES];

    BufferPool_Generic_t	ListOfBufferPools;

    // Functions

public:

    BufferManager_Generic_c( void );
    ~BufferManager_Generic_c( void );

    //
    // Add to the defined types
    //

    BufferStatus_t	CreateBufferDataType(	BufferDataDescriptor_t	 *Descriptor,
						BufferType_t		 *Type );

    BufferStatus_t	FindBufferDataType(	const char		 *TypeName,
						BufferType_t		 *Type );

    BufferStatus_t	GetDescriptor( 		BufferType_t		  Type,
						BufferPredefinedType_t	  RequiredKind,
						BufferDataDescriptor_t	**Descriptor );

    //
    // Create/destroy a pool of buffers overloaded creation function
    //

    BufferStatus_t	 CreatePool(		BufferPool_t	 *Pool,
						BufferType_t	  Type,
					    	unsigned int	  NumberOfBuffers		= UNRESTRICTED_NUMBER_OF_BUFFERS,
						unsigned int	  Size				= UNSPECIFIED_SIZE,
					    	void		 *MemoryPool[3]			= NULL,
					    	void		 *ArrayOfMemoryBlocks[][3]	= NULL,
						char		 *DeviceMemoryPartitionName	= NULL );

    BufferStatus_t	 DestroyPool(		BufferPool_t		  Pool );

    //
    // Status dump/reporting
    //

    void		 Dump( 			unsigned int	  Flags	= DumpAll );

};

#endif
