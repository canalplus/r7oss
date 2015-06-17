/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_ALLOCATORIO
#define H_ALLOCATORIO

#define ALLOCATOR_MAX_PARTITION_NAME_SIZE   64
#define ALLOCATOR_IOCTL_ALLOCATE_DATA       1
#define ALLOCATOR_IOCTL_CREATE_MAP          2
#define ALLOCATOR_IOCTL_REMOVE_MAP          3
#define ALLOCATOR_DEVICE                    "/dev/allocator"


#define	MEMORY_ICS_ACCESS			(1<<0)
#define	MEMORY_VMA_CACHED_ACCESS		(1<<1)
#define	MEMORY_VMA_UNCACHED_ACCESS		(1<<2)

#define	MEMORY_DEFAULT_ACCESS	MEMORY_ICS_ACCESS | MEMORY_VMA_CACHED_ACCESS
#define	MEMORY_HWONLY_ACCESS	0

typedef struct allocator_ioctl_allocate_s {
	unsigned int    RequiredSize;
	char            PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
	unsigned char  *CachedAddress;
	unsigned char  *PhysicalAddress;
	unsigned int 	MemoryAccessType;
} allocator_ioctl_allocate_t;

#endif
