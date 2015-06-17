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

Source file name : buffer_pool_generic.h
Author :           Nick

Implementation of the class definition of the buffer pool class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
14-Jul-06   Created                                         Nick

************************************************************************/

#ifndef H_BUFFER_POOL_GENERIC
#define H_BUFFER_POOL_GENERIC

//

struct BlockDescriptor_s
{
    BlockDescriptor_t		  Next;
    BufferDataDescriptor_t	 *Descriptor;
    bool			  AttachedToPool;

    unsigned int		  Size;
    void			 *Address[3];		// 3 address types (cached uncached physical) - only cached for meta data

    allocator_device_t		  MemoryAllocatorDevice;
    unsigned int		  PoolAllocatedOffset;
};

//

class BufferPool_Generic_c : public BufferPool_c
{
friend class BufferManager_Generic_c;
friend class Buffer_Generic_c;

private:

    // Friend Data

    BufferManager_Generic_t	  Manager;
    BufferPool_Generic_t	  Next;

    // Data

    OS_Mutex_t			  Lock;

    unsigned int		  ReferenceCount;		// A global reference count for the pool

    BufferDataDescriptor_t	 *BufferDescriptor;
    unsigned int		  NumberOfBuffers;
    unsigned int		  CountOfBuffers;		// In effect when NumberOfBuffers == NOT_SPECIFIED
    unsigned int		  Size;

    Buffer_Generic_t		  ListOfBuffers;
    Ring_c			 *FreeBuffer;
    void			 *MemoryPool[3];		// If memory pool is specified, its three addresses
    Allocator_c			 *MemoryPoolAllocator;
    allocator_device_t		  MemoryPoolAllocatorDevice;
    char			  MemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    BlockDescriptor_t		  BufferBlock;
    BlockDescriptor_t		  ListOfMetaData;

    bool			  AbortGetBuffer;		// Flag to abort a get buffer call
    bool			  BufferReleaseSignalWaitedOn;
    OS_Event_t			  BufferReleaseSignal;

    unsigned int		  CountOfReferencedBuffers;	// Statistics (held rather than re-calculated every request)
    unsigned int		  TotalAllocatedMemory;
    unsigned int		  TotalUsedMemory;

    // Functions

    void		TidyUp( 		void );
    void   		FreeUpABuffer( 		Buffer_Generic_t	  Buffer );

    BufferStatus_t	CheckMemoryParameters(	BufferDataDescriptor_t	 *Descriptor,
						bool			  ArrayAllocate,
						unsigned int		  Size,
						void			 *MemoryPool,
						void			 *ArrayOfMemoryBlocks,
						char			 *MemoryPartitionName,
						const char		 *Caller,
						unsigned int		 *ItemSize	= NULL );

    BufferStatus_t	AllocateMemoryBlock(	BlockDescriptor_t	  Block,
						bool			  ArrayAllocate,
						unsigned int		  Index,
						Allocator_c		 *PoolAllocator,
						void			 *MemoryPool,
						void			 *ArrayOfMemoryBlocks,
						char			 *MemoryPartitionName,
						const char		 *Caller,
						bool			  RequiredSizeIsLowerBound	= false );

    BufferStatus_t	DeAllocateMemoryBlock(	BlockDescriptor_t	  Block );

    BufferStatus_t	ShrinkMemoryBlock( 	BlockDescriptor_t	  Block,
						unsigned int		  NewSize );

    BufferStatus_t	ExtendMemoryBlock( 	BlockDescriptor_t	  Block,
						unsigned int		 *NewSize,
						bool			  ExtendUpwards );


public:

    BufferPool_Generic_c(			BufferManager_Generic_t	  Manager,
						BufferDataDescriptor_t	 *Descriptor, 
					    	unsigned int		  NumberOfBuffers,
						unsigned int		  Size,
					    	void			 *MemoryPool[3],
					    	void			 *ArrayOfMemoryBlocks[][3],
						char			 *DeviceMemoryPartitionName );

    ~BufferPool_Generic_c( void );

    //
    // Global operations on all pool members
    //

    BufferStatus_t	 AttachMetaData(	MetaDataType_t	  Type,
						unsigned int	  Size				= UNSPECIFIED_SIZE,
						void		 *MemoryPool			= NULL,
						void		 *ArrayOfMemoryBlocks[]		= NULL,
						char		 *DeviceMemoryPartitionName	= NULL );

    BufferStatus_t	 DetachMetaData(	MetaDataType_t	  Type );

    //
    // Get/Release a buffer - overloaded get
    //

    BufferStatus_t	 GetBuffer(		Buffer_t	 *Buffer,
						unsigned int	  OwnerIdentifier		= UNSPECIFIED_OWNER,
						unsigned int	  RequiredSize			= UNSPECIFIED_SIZE,
						bool		  NonBlocking 			= false,
						bool		  RequiredSizeIsLowerBound	= false );

    BufferStatus_t	 AbortBlockingGetBuffer(void );

    BufferStatus_t	 ReleaseBuffer(		Buffer_t	  Buffer );

    //
    // Usage/query/debug methods
    //

    BufferStatus_t	 GetType(		BufferType_t	 *Type );

    BufferStatus_t	 GetPoolUsage(		unsigned int	 *BuffersInPool,
						unsigned int     *BuffersWithNonZeroReferenceCount	= NULL,
						unsigned int	 *MemoryInPool				= NULL,
						unsigned int	 *MemoryAllocated			= NULL,
						unsigned int	 *MemoryInUse				= NULL,
						unsigned int	 *LargestFreeMemoryBlock		= NULL );

    BufferStatus_t	 CountBuffersReferencedBy( 
						unsigned int	  OwnerIdentifier,
						unsigned int	 *Count );

    BufferStatus_t	 GetAllUsedBuffers(	unsigned int	  ArraySize,
						Buffer_t	 *ArrayOfBuffers,
						unsigned int	  OwnerIdentifier );

    //
    // Status dump/reporting
    //

    void		 Dump( 			unsigned int	  Flags	= DumpAll );
};

#endif
