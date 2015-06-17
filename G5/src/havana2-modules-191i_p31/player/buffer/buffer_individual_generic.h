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

Source file name : buffer_individual_generic.h
Author :           Nick

Implementation of the generic class definition of the buffer individual 
class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
26-Jul-06   Created                                         Nick

************************************************************************/

#ifndef H_BUFFER_INDIVIDUAL_GENERIC
#define H_BUFFER_INDIVIDUAL_GENERIC

//

class Buffer_Generic_c : public Buffer_c
{
friend class BufferManager_Generic_c;
friend class BufferPool_Generic_c;

private:

    // Friend Data

    BufferManager_Generic_t	  Manager;
    BufferPool_Generic_t	  Pool;
    Buffer_Generic_t	  	  Next;

    unsigned int	 	  Index;

    unsigned int		  ReferenceCount;
    unsigned int		  OwnerIdentifier[MAX_BUFFER_OWNER_IDENTIFIERS];

    BlockDescriptor_t		  BufferBlock;
    BlockDescriptor_t		  ListOfMetaData;
    Buffer_t			  AttachedBuffers[MAX_ATTACHED_BUFFERS];


    unsigned int		  DataSize;

    // Data

    OS_Mutex_t			  Lock;
		
    // Functions

public:

    Buffer_Generic_c( 				BufferManager_Generic_t	  Manager,
						BufferPool_Generic_t	  Pool,
						BufferDataDescriptor_t	 *Descriptor );
    ~Buffer_Generic_c( void );

    //
    // Meta data activities
    //

    BufferStatus_t	 AttachMetaData(	MetaDataType_t	  Type,
						unsigned int	  Size				= UNSPECIFIED_SIZE,
						void		 *MemoryBlock			= NULL,
						char		 *DeviceMemoryPartitionName	= NULL );

    BufferStatus_t	 DetachMetaData(	MetaDataType_t	  Type );

    BufferStatus_t	 ObtainMetaDataReference(
						MetaDataType_t	  Type,
						void		**Pointer );

    //
    // Buffer manipulators
    //

    BufferStatus_t	 SetUsedDataSize(	unsigned int	  DataSize );

    BufferStatus_t	 ShrinkBuffer(		unsigned int	  NewSize );

    BufferStatus_t	 ExtendBuffer(		unsigned int	 *NewSize,
						bool		  ExtendUpwards		= true );

    BufferStatus_t	 PartitionBuffer(	unsigned int	  LeaveInFirstPartitionSize,
						bool		  DuplicateMetaData,
						Buffer_t	 *SecondPartition,
						unsigned int	  SecondOwnerIdentifier	= UNSPECIFIED_OWNER,
						bool		  NonBlocking 		= false );

    //
    // Reference manipulation, and ownership control
    //

    BufferStatus_t	 RegisterDataReference(	unsigned int	  BlockSize,
						void		 *Pointer,
						AddressType_t	  AddressType = CachedAddress );

    BufferStatus_t	 RegisterDataReference(	unsigned int	  BlockSize,
						void		 *Pointers[3] );

    BufferStatus_t	 ObtainDataReference(	unsigned int	 *BlockSize,
						unsigned int	 *UsedDataSize,
						void		**Pointer,
						AddressType_t	  AddressType = CachedAddress );

    BufferStatus_t	 TransferOwnership(	unsigned int	  OwnerIdentifier0,
						unsigned int	  OwnerIdentifier1	= UNSPECIFIED_OWNER );

    BufferStatus_t	 IncrementReferenceCount(
						unsigned int	  OwnerIdentifier	= UNSPECIFIED_OWNER );

    BufferStatus_t	 DecrementReferenceCount(
						unsigned int	  OwnerIdentifier	= UNSPECIFIED_OWNER );

    //
    // linking of other buffers to this buffer - for increment/decrement management
    //

    BufferStatus_t	 AttachBuffer(		Buffer_t	  Buffer );
    BufferStatus_t	 DetachBuffer(		Buffer_t	  Buffer );
    BufferStatus_t	 ObtainAttachedBufferReference(
						BufferType_t	  Type,
						Buffer_t	 *Buffer );

    //
    // Usage/query/debug methods
    //

    BufferStatus_t	 GetType(		BufferType_t	 *Type );

    BufferStatus_t	 GetIndex(		unsigned int	 *Index );

    BufferStatus_t	 GetMetaDataCount( 	unsigned int	 *Count );

    BufferStatus_t	 GetMetaDataList(	unsigned int	  ArraySize,
						unsigned int	 *ArrayOfMetaDataTypes );

    BufferStatus_t	 GetOwnerCount( 	unsigned int	 *Count );

    BufferStatus_t	 GetOwnerList(		unsigned int	  ArraySize,
						unsigned int	 *ArrayOfOwnerIdentifiers );

	//
	// Cache functions
	// 
		
    BufferStatus_t	 FlushCache(		void);		
    BufferStatus_t	 PurgeCache(		void);		
		
    //
    // Status dump/reporting
    //

    void		 Dump( 			unsigned int	  Flags	= DumpAll );

    void		DumpToRelayFS ( unsigned int id, unsigned int type, void *param );

};

#endif
