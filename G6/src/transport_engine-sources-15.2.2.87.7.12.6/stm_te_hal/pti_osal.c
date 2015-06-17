/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_osal.c
   @brief  Additional Operating System Abstraction Layer, and utilities.

   This file contains functions to aid operating system abstration.  It covers
   PTI specific things missing in STOS.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

#include "linuxcommon.h"

#if defined( CONFIG_BPA2 )
#include <linux/bpa2.h>

#if !defined( STPTI_BPA2_SECURE_REGION )
#define STPTI_BPA2_SECURE_REGION "te-buffers"
#endif

#if !defined( STPTI_BPA2_NON_SECURE_REGION )
#define STPTI_BPA2_NON_SECURE_REGION "bigphysarea"
#endif

#else
#error Update your Kernel config to include BigPhys Area.
#endif

#include "stddefs.h"

/* Includes from API level */
#include "pti_osal.h"
#include "pti_debug.h"
#include "pti_driver.h"

/* Includes from the HAL / ObjMan level */

/* MACROS ------------------------------------------------------------------ */
#if defined( ST_OS21 )
#define STPTI_TASK_ID_FN() task_id()
#else
#define STPTI_TASK_ID_FN() 0
#endif

/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
DEFINE_SPINLOCK(MessageQueueLock);

/* Private Function Prototypes --------------------------------------------- */

/* Functions --------------------------------------------------------------- */

/**
   @brief  Request write lock.

   This function should implement a read lock functionality.  Unfortunately Read and Write locks
   are very difficult to implment outside of an OS.  It is very easy to create priority inversion
   situations, or situations where successive reads lock out write accesses.

   This function is left here in case we start using an OS that can support read locks.

   @param  Lock_p    A pointer to an access lock
   @param  LockCtx_p  (a return) A pointer to the local access lock context to be populated

 */
void stptiSupport_ReadLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	/* We currently do not support read locks.   We treat them as write locks. */
	stptiSupport_WriteLock(Lock_p, LocalLockState_p);
}

/**
   @brief  Request write lock.

   This function will block if another task has a write lock.

   @param  Lock_p    A pointer to an access lock
   @param  LockCtx_p  (a return) A pointer to the local access lock context to be populated

 */
void stptiSupport_WriteLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	/* An assertion here means...
	   (a) you didn't initialise the variable LocalLockState_p points to
	   (b) you are already locked (for read or write)  */
	STPTI_ASSERT(*LocalLockState_p == stptiSupport_UNLOCKED, "Invalid lock state");

	stpti_printf("%d:%p Requesting WriteLock", STPTI_TASK_ID_FN(), Lock_p);
	mutex_lock(&Lock_p->WriteLockMutex);
	*LocalLockState_p = stptiSupport_WRITELOCKED;
	stpti_printf("%d:%p WriteLock Granted", STPTI_TASK_ID_FN(), Lock_p);
}
/**
   @brief  try write lock.

   This function will block if another task has a write lock.

   @param  Lock_p    A pointer to an access lock
   @param  LockCtx_p  (a return) A pointer to the local access lock context to be populated
   @return 1 if the mutex has been acquired successfully, and 0 on contention.

 */
void stptiSupport_TryWriteLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	/* An assertion here means...
	   (a) you didn't initialise the variable LocalLockState_p points to
	   (b) you are already locked (for read or write)  */
	STPTI_ASSERT(*LocalLockState_p == stptiSupport_UNLOCKED, "Invalid lock state");

	stpti_printf("%d:%p Requesting WriteLock", STPTI_TASK_ID_FN(), Lock_p);
	if(mutex_trylock(&Lock_p->WriteLockMutex)) {
		*LocalLockState_p = stptiSupport_WRITELOCKED;
		stpti_printf("%d:%p WriteLock Granted", STPTI_TASK_ID_FN(), Lock_p);
	} else {
		stpti_printf("%d:%p WriteLock not Granted", STPTI_TASK_ID_FN(), Lock_p);
	}
}

/**
   @brief  Release access lock for either read or write.

   This function will release the access lock, allowing other tasks to read or write lock.

   @param  Lock_p    A pointer to an access lock
   @param  LockCtx_p  (a return) A pointer to the local access lock context to be populated

 */
void stptiSupport_Unlock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	switch (*LocalLockState_p) {
	case stptiSupport_WRITELOCKED:
	case stptiSupport_READLOCKED:
		stpti_printf("%d:%p Releasing WriteLock", STPTI_TASK_ID_FN(), Lock_p);
		mutex_unlock(&Lock_p->WriteLockMutex);
		*LocalLockState_p = stptiSupport_UNLOCKED;
		stpti_printf("%d:%p Released WriteLock", STPTI_TASK_ID_FN(), Lock_p);
		break;

	default:
		STPTI_ASSERT(FALSE, "CRITICAL ERROR, please report - Invalid state for Unlocking Access Lock (%u)",
			     (unsigned)*LocalLockState_p);
		break;
	}
}

/**
   @brief  Align an address to a specified alignment.

   This function will return an address aligned on a byte boundry.

   @param  Memory_p   Address to align.
   @param  Alignment  Alignment size (2 = U16 aligned, 4 U32 aligned, 32 32byte aligned)

 */
void *stptiSupport_MemoryAlign(void *Memory_p, U32 Alignment)
{
	return (void *)stptiSupport_AlignUpwards((U32) Memory_p, Alignment);
}

/**
   @brief  Invalidate a region of memory, making sure that we are allowed to do this.

   This function checks the CachedMemory structure to make sure it is allowed to invalidate this
   region.  This adds a defensive step to guard against careless cache flushes that cause very
   difficult problems to find.

   @param  p          Pointer to a structure holding information about cache region
   @param  addr       Start of memory region (does not need to be cache aligned)
   @param  size       Size of memory region (does not need to be cache aligned)

 */
void stptiSupport_InvalidateRegion(stptiSupport_DMAMemoryStructure_t * p, void *addr, size_t size)
{
	void *inv_start = addr;
	void *inv_end = (void *)((U8 *) inv_start + size - 1);

	STPTI_ASSERT(p->AllocatedMarker == 0xBABEBABE,
		     "Invalidating a region that has not been initialised by stptiSupport_MemoryAllocateForDMA()");

	if (p->FirstByte_p <= inv_start && inv_end <= p->LastByte_p) {
		rmb();
		switch (p->MethodUsed) {
		case stptiSupport_ALLOCATED_VIA_LOWMEM:
			{
				uint32_t dataOffset = (uint32_t) addr - (uint32_t) p->AllocatedMemory_p;

				pr_debug("stptiSupport_ALLOCATED_VIA_HIMEM_LOWMEM");
				dma_sync_single_range_for_cpu(p->Dev, p->dma_handle, dataOffset, size, DMA_FROM_DEVICE);
				break;
			}
		case stptiSupport_ALLOCATED_VIA_HIMEM_CACHED:
			{
#if defined(CONFIG_ARM)
				phys_addr_t inv_start_phys = (phys_addr_t) stptiSupport_VirtToPhys(p, inv_start);

				dmac_unmap_area((const void *)(inv_start), size, DMA_FROM_DEVICE);
				outer_inv_range(inv_start_phys, inv_start_phys + size);
#elif defined(CONFIG_CPU_SH4)
				invalidate_ioremap_region(virt_to_phys(inv_start), inv_start, 0, size);
#else
#error "Unsupported arch"
#endif
				pr_debug("stptiSupport_ALLOCATED_VIA_HIMEM_CACHED");
				break;
			}
		case stptiSupport_ALLOCATED_VIA_HIMEM_NOCACHE:
		case stptiSupport_ALLOCATED_VIA_DMA_COHERENT:
		default:
			{
				/* uncached but still allow for read memory barrier */
				break;
			}
		}
		rmb();
	} else {
		STPTI_ASSERT(FALSE,
			     "SERIOUS ERROR! Invalidation violation found 0x%08x-0x%08x (valid region spans 0x%08x-0x%08x)",
			     (U32) inv_start, (U32) inv_end, (U32) p->FirstByte_p, (U32) p->LastByte_p);
	}
}

/**
   @brief  Flush a region of memory, making sure that we are allowed to do this.

   This function checks the CachedMemory structure to make sure it is allowed to flush this region.
   This adds a  defensive step to guard against careless cache flushes that cause very difficult
   problems to find.

   @param  p          Pointer to a structure holding information about cache region
   @param  addr       Start of memory region (does not need to be cache aligned)
   @param  size       Size of memory region (does not need to be cache aligned)

 */
void stptiSupport_FlushRegion(stptiSupport_DMAMemoryStructure_t * p, void *addr, size_t size)
{
	void *flush_start = addr;
	void *flush_end = (void *)((U8 *) flush_start + size - 1);

	STPTI_ASSERT(p->AllocatedMarker == 0xBABEBABE,
		     "Flushing a region that has not been initialised by stptiSupport_MemoryAllocateForDMA()");

	if (p->FirstByte_p <= flush_start && flush_end <= p->LastByte_p) {
		wmb();
		switch (p->MethodUsed) {

		case stptiSupport_ALLOCATED_VIA_LOWMEM:
			{
				uint32_t dataOffset = (uint32_t) addr - (uint32_t) p->AllocatedMemory_p;

				pr_debug("stptiSupport_ALLOCATED_VIA_LOWMEM");
				dma_sync_single_range_for_device(p->Dev, p->dma_handle, dataOffset, size,
								 DMA_TO_DEVICE);
				break;
			}
		case stptiSupport_ALLOCATED_VIA_HIMEM_CACHED:
			{
#if defined(CONFIG_ARM)
				phys_addr_t flush_start_phys = (phys_addr_t) stptiSupport_VirtToPhys(p, flush_start);

				dmac_map_area((const void *)(flush_start), size, DMA_TO_DEVICE);
				outer_clean_range(flush_start_phys, flush_start_phys + size);
#elif defined(CONFIG_CPU_SH4)
				writeback_ioremap_region(virt_to_phys(flush_start), flush_start, 0, size);
#else
#error "Unsupported arch"
#endif
				pr_debug("stptiSupport_ALLOCATED_VIA_HIMEM_CACHED");
				break;
			}
		case stptiSupport_ALLOCATED_VIA_HIMEM_NOCACHE:
		case stptiSupport_ALLOCATED_VIA_DMA_COHERENT:
		default:
			{
				break;
			}
		}
		wmb();
	} else {
		STPTI_ASSERT(FALSE,
			     "SERIOUS ERROR! Flush violation found 0x%08x-0x%08x (valid region spans 0x%08x-0x%08x)",
			     (U32) flush_start, (U32) flush_end, (U32) p->FirstByte_p, (U32) p->LastByte_p);
	}
}

/**
   @brief  Convert a Virtual Pointer to a Physical One.

   This function converts between a Virtual Pointer to a Physical Pointer for Memory Regions
   allocated by stptiSupport_MemoryAllocateForDMA.

   @param  p          Pointer to a structure holding information about cache region
   @param  addr       Start of memory region (does not need to be cache aligned)
   @param  size       Size of memory region (does not need to be cache aligned)

 */
void *stptiSupport_VirtToPhys(stptiSupport_DMAMemoryStructure_t * p, void *addr)
{
	void *paddr = NULL;

	STPTI_ASSERT(p->AllocatedMarker == 0xBABEBABE,
		     "Accessing a region that has not been initialised by stptiSupport_MemoryAllocateForDMA()");

	if (p->FirstByte_p <= addr && addr <= p->LastByte_p) {
		paddr = (U8 *) addr + p->DeltaVtoP;	/* Do Virt to Phys translation */
	} else {
		STPTI_PRINTF_ERROR("VirtToPhys violation found 0x%08x (valid region spans 0x%08x-0x%08x)", (U32) addr,
				   (U32) p->FirstByte_p, (U32) p->LastByte_p);
	}

	return (paddr);
}

/**
   @brief  Waits a specified time (or forever) for semaphore to be signalled.

   This function wraps the OS semaphore calls to make the timing relative rather than absolute.

   @param  Semaphore_p  Pointer to the semaphore
   @param  timeout_ms   Timeout in miliseconds.  Can be 0 for immediate checking or, -1 for
                        wait forever.

   @return              TRUE if TimedOut, otherwise FALSE if semaphore obtained.
 */
BOOL stptiSupport_TimedOutWaitingForSemaphore(struct semaphore * Semaphore_p, int TimeoutMS)
{
	BOOL TimedOut = FALSE;

#if !defined(CONFIG_STM_VIRTUAL_PLATFORM)	/* FIXME  - WA - VSOC - MODEL/Driver not yet ready - bypass timeouted wait ... */
	if (TimeoutMS > 0) {
		if (down_timeout(Semaphore_p, msecs_to_jiffies(TimeoutMS)))
			TimedOut = TRUE;
	} else if (TimeoutMS == 0) {
		if (down_trylock(Semaphore_p))
			TimedOut = TRUE;
	} else {
		if (down_interruptible(Semaphore_p))
			TimedOut = TRUE;
	}
#else

#if defined(CONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO)	/* FIXME - MODEL currently decrease platform performance - model removed on v2.1 platform */
	STPTI_PRINTF("VSOC - stptiSupport_TimedOutWaitingForSemaphore - BYPASSED\n");
#else
	STPTI_PRINTF("VSOC - stptiSupport_TimedOutWaitingForSemaphore - INFINITE\n");
	down_interruptible(Semaphore_p);
#endif
	return FALSE;
#endif

	return TimedOut;
}

/**
   @brief  Sets up a Partitioned Resource Management Table.

   This function sets up the Partitioned Resource Management Table used for managing a shared items
   where you might need to reserve a contiguous block of them.  Examples are slots, which are
   allocated as a whole in a pDevice, but need to be divided between the vDevices.

   @param  rms_p                  A pointer to the Resource Management Structure for the Partition
                                  Allocation
   @param  StartOfResource        start of the resource
   @param  TotalSizeOfResource    total space available for the resource
   @param  NumberOfPartitions     maximum number of partitions that can be allocated for this
                                  resource

   @return                        0 if successful
 */
int stptiSupport_PartitionedResourceSetup(stptiHAL_PartitionedResource_t * rms_p,
					  U32 StartOfResource, U32 TotalSizeOfResource, int NumberOfPartitions)
{
	int ret = 0;
	void *PartitionStart_p = NULL, *PartitionSize_p = NULL;
	int NumberOfPartitionsPlus1 = NumberOfPartitions + 1;

	if (NumberOfPartitions == 0) {
		ret = -1;
	} else if (NULL == (PartitionStart_p = kmalloc(NumberOfPartitionsPlus1 * sizeof(U32), GFP_KERNEL))) {
		ret = -1;
	} else if (NULL == (PartitionSize_p = kmalloc(NumberOfPartitionsPlus1 * sizeof(U32), GFP_KERNEL))) {
		kfree(PartitionStart_p);
		ret = -1;
	} else {
		rms_p->NumberOfPartitions = NumberOfPartitions;
		rms_p->StartOfResource = StartOfResource;
		rms_p->TotalSizeOfResource = TotalSizeOfResource;
		rms_p->PartitionStart = PartitionStart_p;
		rms_p->PartitionSize = PartitionSize_p;

		/* clear the allocation table */
		memset(rms_p->PartitionStart, 0, NumberOfPartitionsPlus1 * sizeof(U32));
		memset(rms_p->PartitionSize, 0, NumberOfPartitionsPlus1 * sizeof(U32));
	}
	return (ret);
}

/**
   @brief  Finds contiguous free space for allocating partitioned resources

   This function works through all the allocation table given by the Resource Management Structure
   and reserves and returns the start of a contiguous space (or 0xFFFFFFFF if a contiguous space
   was not found).

   @param  rms_p                      A pointer to the Resource Management Structure for the Resource
   @param  ContiguousSpaceRequired    The amount of contiguous space required
   @param  PartitionID_p              A pointer to where to put the PartitionID for allocated region.

   @return                            0 if successful

 */
int stptiSupport_PartitionedResourceAllocPartition(stptiHAL_PartitionedResource_t * rms_p, U32 ContiguousSpaceRequired,
						   int *PartitionID_p)
{
	int ret = 0;

	int i, FirstFreePartitionIndex = -1;
	BOOL Collision;
	U32 PotentialStart, PotentialLastItem, AllocatedStart, AllocatedLastItem, StartOfResource, LimitOfResource;

	if (ContiguousSpaceRequired == 0) {
		return (-1);
	}

	StartOfResource = rms_p->StartOfResource;
	LimitOfResource = StartOfResource + rms_p->TotalSizeOfResource - ContiguousSpaceRequired;

	for (PotentialStart = StartOfResource; PotentialStart <= LimitOfResource; PotentialStart++) {
		Collision = FALSE;

		for (i = 0; i < rms_p->NumberOfPartitions; i++) {
			if (rms_p->PartitionSize[i] > 0) {
				PotentialLastItem = PotentialStart + ContiguousSpaceRequired - 1;
				AllocatedStart = rms_p->PartitionStart[i];
				AllocatedLastItem = rms_p->PartitionStart[i] + rms_p->PartitionSize[i] - 1;
				if (!((AllocatedLastItem < PotentialStart) || (PotentialLastItem < AllocatedStart))) {
					Collision = TRUE;
					PotentialStart = AllocatedLastItem;	/* accelerate past the end of the colliding blocks (note for loop will +1) */
					break;	/* no point checking the other resources */
				}
			} else if (FirstFreePartitionIndex < 0) {
				/* end of the allocation table found, record it for future use */
				FirstFreePartitionIndex = i;
			}
		}
		if (!Collision)
			break;
	}

	/* If no space in the PartitionedResource, or no space in the allocation table */
	if ((PotentialStart > LimitOfResource) || (FirstFreePartitionIndex < 0)) {
		ret = -1;
	} else {
		rms_p->PartitionStart[FirstFreePartitionIndex] = PotentialStart;
		rms_p->PartitionSize[FirstFreePartitionIndex] = ContiguousSpaceRequired;
		*PartitionID_p = FirstFreePartitionIndex;
	}

	return (ret);
}

/**
   @brief  Resizes a region withing a Partitioned Resource.

   This function resizes reserved space allocated stptiSupport_PartitionedResourceAllocPartition.
   As partitions are put next to each other, more often than not you will not be able resize
   upwards, but you can always make smaller.

   When you need to enlarge a space which has no room, you need to free it via
   stptiSupport_PartitionedResourceFreePartition(), maybe compact the Partitioned Resource via
   stptiSupport_PartitionedResourceCompact(), and then reallocate it by
   stptiSupport_PartitionedResourceAllocPartition().

   @param  rms_p                        A pointer to the Resource Management Structure for the Resource
   @param  PartitionID                  The PartitionID to free
   @param  NewContiguousSpaceRequired   New size

   @return                              0 if successful

 */
int stptiSupport_PartitionedResourceResizePartition(stptiHAL_PartitionedResource_t * rms_p, int PartitionID,
						    U32 NewContiguousSpaceRequired)
{
	int i;
	int ret = -1;

	U32 NextPartitionStart = rms_p->StartOfResource + rms_p->TotalSizeOfResource;
	U32 CurrentPartitionStart = rms_p->PartitionStart[PartitionID];
	U32 CurrentPartitionEnd = CurrentPartitionStart + rms_p->PartitionSize[PartitionID];

	/* Find the start of the next region */
	for (i = 0; i < rms_p->NumberOfPartitions; i++) {
		if (i != PartitionID && rms_p->PartitionSize[i] > 0) {
			U32 start = rms_p->PartitionStart[i];

			/* Find the real start of the next region */
			if (CurrentPartitionEnd <= start && start < NextPartitionStart) {
				NextPartitionStart = start;
			}
		}
	}

	if (NextPartitionStart >= (CurrentPartitionStart + NewContiguousSpaceRequired)) {
		/* Room to grow region */
		rms_p->PartitionSize[PartitionID] = NewContiguousSpaceRequired;
		ret = 0;
	}

	return (ret);
}

/**
   @brief  Compacts a Partitioned Resource.

   This function reshuffles all the regions removing any space between them.  As a result
   all regions will move, and you will need to rebuild any dependent structures.

   As a side effect the regions will end up ordered in terms of PartitionID.

   @param  rms_p           A pointer to the Resource Management Structure for the Resource

   @return                 0 if successful

 */
int stptiSupport_PartitionedResourceCompact(stptiHAL_PartitionedResource_t * rms_p)
{
	int i;
	U32 base = rms_p->StartOfResource;

	for (i = 0; i < rms_p->NumberOfPartitions; i++) {
		if (rms_p->PartitionSize[i] > 0) {
			rms_p->PartitionStart[i] = base;
			base += rms_p->PartitionSize[i];
		} else {
			rms_p->PartitionStart[i] = 0;
		}
	}
	return (0);
}

/**
   @brief  Releases a contiguous free space previously reserved.

   This function releases (frees) reserved space found by stptiSupport_PartitionedResourceAllocPartition.

   @param  rms_p           A pointer to the Resource Management Structure for the Resource
   @param  PartitionID     The PartitionID to free

   @return                 0 if successful

 */
int stptiSupport_PartitionedResourceFreePartition(stptiHAL_PartitionedResource_t * rms_p, int PartitionID)
{
	rms_p->PartitionStart[PartitionID] = 0;
	rms_p->PartitionSize[PartitionID] = 0;

	return (0);
}

/**
   @brief  Frees memory used by Resource Management Table.

   This function frees the memory used by the allocation table for a partitioned

   This function is the opposite of stptiSupport_PartitionedResourceSetup.

   @param  rms_p       A pointer to the Resource Management Structure for the Resource
                       (to release)

   @return             0 if successful

 */
int stptiSupport_PartitionedResourceDestroy(stptiHAL_PartitionedResource_t * rms_p)
{
	if (rms_p == NULL) {
		return (-1);
	} else {
		kfree(rms_p->PartitionStart);
		kfree(rms_p->PartitionSize);

		rms_p->StartOfResource = 0;
		rms_p->TotalSizeOfResource = 0;
		rms_p->NumberOfPartitions = 0;
		rms_p->PartitionStart = NULL;
		rms_p->PartitionSize = NULL;
	}

	return (0);
}

/**
   @brief  Allocates large chunks memory guaranteed to be Contiguous.

   This function allocates large regions of memory and should be called for where IP blocks access
   memory.

   @param  Size                    Amount of Memory to allocate (in bytes)
   @param  BaseAlignment           Address Boundary Alignment
   @param  p                       A pointer to where to put the Memory Structure.
   @param  Zone                    Indication of the memory type required
   @return                         A pointer to the memory allocated (or NULL if unsuccessful)

 */
void *stptiSupport_MemoryAllocateForDMA(size_t Size, U32 BaseAlignment, stptiSupport_DMAMemoryStructure_t * p,
					stptiSupport_AllocationZone_t Zone)
{
	void *CachedAlignedAddress = NULL;
	void *AllocatedMemory_p = NULL;
	size_t AllocationSize = Size;
	struct bpa2_part *part;
	long unsigned int PhysicalMemory_p = 0;
	dma_addr_t PhysicalMemoryAddress;

	/* Make Cache Safe */
	if (BaseAlignment < STPTI_SUPPORT_DCACHE_LINE_SIZE) {
		BaseAlignment = STPTI_SUPPORT_DCACHE_LINE_SIZE;
	}

	AllocationSize += BaseAlignment + STPTI_SUPPORT_DCACHE_LINE_SIZE;

	/* Policy translation */
	switch (Zone) {
	case stptiSupport_ZONE_LARGE_NON_SECURE:
		p->Region_p = STPTI_BPA2_NON_SECURE_REGION;
		break;
	case stptiSupport_ZONE_LARGE_SECURE:
		p->Region_p = STPTI_BPA2_SECURE_REGION;
		break;
	case stptiSupport_ZONE_SMALL:
	default:
		p->Region_p = NULL;
		break;
	}

	if (p->Region_p == NULL) {
		AllocatedMemory_p =
		    dma_alloc_coherent(p->Dev, AllocationSize, (dma_addr_t *) & PhysicalMemoryAddress,
				       GFP_KERNEL | __GFP_DMA);
		p->PhysicalMemoryAddress = PhysicalMemoryAddress;
		if (AllocatedMemory_p != NULL) {
			p->PhysicalMemoryAddress = PhysicalMemoryAddress;
			p->AllocatedMemory_p = AllocatedMemory_p;
			p->DeltaVtoP = (U8 *) PhysicalMemoryAddress - (U8 *) AllocatedMemory_p;
			p->AllocatedMemorySize = AllocationSize;
			p->MethodUsed = stptiSupport_ALLOCATED_VIA_DMA_COHERENT;
			stpti_printf
			    ("Allocating %d bytes using dma_alloc_coherent virtual address 0x%x physical address 0x%x\n",
			     AllocationSize, (unsigned int)AllocatedMemory_p, (unsigned int)PhysicalMemoryAddress);
		}
	} else {
		U32 pages = AllocationSize / PAGE_SIZE;

		if (AllocationSize % PAGE_SIZE)
			pages++;
		AllocationSize = pages * PAGE_SIZE;

		part = bpa2_find_part(p->Region_p);
		if (part == NULL) {
			pr_err("Requested bpa2 Region(%s) not available\n",
					p->Region_p);
			return NULL;
		}

		PhysicalMemory_p = bpa2_alloc_pages(part, pages, 0, GFP_KERNEL | __GFP_DMA);
		if (PhysicalMemory_p) {
			if (bpa2_low_part(part))
				p->MethodUsed = stptiSupport_ALLOCATED_VIA_LOWMEM;
			else
				p->MethodUsed = stptiSupport_ALLOCATED_VIA_HIMEM_CACHED;

			if (p->MethodUsed == stptiSupport_ALLOCATED_VIA_HIMEM_CACHED) {
				pr_debug("stptiSupport_ALLOCATED_VIA_HIMEM_CACHED");
				AllocatedMemory_p = ioremap_cache(PhysicalMemory_p, AllocationSize);
			} else {
				pr_debug("stptiSupport_ALLOCATED_VIA_LOWMEM");
				AllocatedMemory_p = phys_to_virt(PhysicalMemory_p);
				p->dma_handle =
				    dma_map_single(p->Dev, AllocatedMemory_p, AllocationSize, DMA_BIDIRECTIONAL);
			}
		} else {
			pr_err("Failed to allocate %d pages from bpa2 region %s\n", pages, p->Region_p);
		}

		if (AllocatedMemory_p != NULL) {
			p->AllocatedMemory_p = AllocatedMemory_p;
			p->DeltaVtoP = (U8 *) PhysicalMemory_p - (U8 *) AllocatedMemory_p;
			p->AllocatedMemorySize = AllocationSize;
			stpti_printf
			    ("Allocating %d bytes using bpa2_alloc_pages virtual address 0x%x physical address 0x%x\n",
			     AllocationSize, (unsigned int)AllocatedMemory_p, (unsigned int)PhysicalMemory_p);
		}
	}

	if (AllocatedMemory_p != NULL) {
		U32 CachedMemorySize = stptiSupport_AlignUpwards(Size, STPTI_SUPPORT_DCACHE_LINE_SIZE);
		CachedAlignedAddress = stptiSupport_MemoryAlign(AllocatedMemory_p, BaseAlignment);
		p->FirstByte_p = CachedAlignedAddress;
		p->LastByte_p = (void *)((U8 *) CachedAlignedAddress + CachedMemorySize - 1);
		p->AllocatedMarker = 0xBABEBABE;	/* indicate this as being populated */
	} else {
		/* indicate this as being not allocated */
		p->AllocatedMemory_p = NULL;
		p->AllocatedMemorySize = 0;
		p->DeltaVtoP = 0;
		p->AllocatedMarker = 0;
		p->MethodUsed = stptiSupport_NOT_ALLOCATED;
		STPTI_PRINTF_ERROR("Unable to Allocate %d Bytes for DMA", AllocationSize);
	}
	return (CachedAlignedAddress);
}

/**
   @brief  Allocates large chunks memory guaranteed to be Contiguous.

   This function allocates large regions of memory and should be called for where IP blocks access
   memory.

   @param  AllocatedMemory_p       Virtual pointer for preallocated memory
   @param  AllocatedMemoryPHYS_p   Physical pointer for preallocated memory
   @param  Size                    Amount of Memory to allocate (in bytes)
   @param  p                       A pointer to where to put the Memory Structure.

   @return                         A pointer to the memory allocated (or NULL if unsuccessful)

 */
void *stptiSupport_MemoryAllocatePreallocatedForDMA(void *AllocatedMemory_p, void *AllocatedMemoryPHYS_p, size_t Size,
						    stptiSupport_DMAMemoryStructure_t * p)
{
	p->AllocatedMemory_p = AllocatedMemory_p;
	p->DeltaVtoP = (U8 *) AllocatedMemoryPHYS_p - (U8 *) AllocatedMemory_p;
	p->FirstByte_p = AllocatedMemory_p;
	p->LastByte_p = (void *)((U8 *) AllocatedMemory_p + Size - 1);
	p->MethodUsed = stptiSupport_NOT_ALLOCATED;
	p->AllocatedMarker = 0xBABEBABE;	/* indicate this as being populated */

	return (AllocatedMemory_p);
}

/**
   @brief  Deallocates Memory allocated by stptiSupport_MemoryAllocateForDMA.

   This function deallocates memory allocated by stptiSupport_MemoryAllocateForDMA.

   @param  p         The Memory Structure returned by stptiSupport_MemoryAllocateForDMA

 */
void stptiSupport_MemoryDeallocateForDMA(stptiSupport_DMAMemoryStructure_t * p)
{
	struct bpa2_part *part = NULL;

	if (p->MethodUsed == stptiSupport_ALLOCATED_VIA_LOWMEM ||
			p->MethodUsed == stptiSupport_ALLOCATED_VIA_HIMEM_CACHED ||
			p->MethodUsed == stptiSupport_ALLOCATED_VIA_HIMEM_NOCACHE) {
		part = bpa2_find_part(p->Region_p);
		if (part == NULL) {
			STPTI_ASSERT(FALSE,
					"Failure to deallocate memory at 0x%08x (unknown bpa2 Region: %s)",
					(unsigned)p->AllocatedMemory_p,
					p->Region_p);
			return;
		}
	}

	switch (p->MethodUsed) {
	case stptiSupport_ALLOCATED_VIA_LOWMEM:
		{
			pr_debug("stptiSupport_ALLOCATED_VIA_LOWMEM");
			dma_unmap_single(p->Dev, p->dma_handle, p->AllocatedMemorySize, DMA_BIDIRECTIONAL);
			bpa2_free_pages(part, (long unsigned int)stptiSupport_VirtToPhys(p, p->AllocatedMemory_p));
		}
		break;
	case stptiSupport_ALLOCATED_VIA_HIMEM_CACHED:
	case stptiSupport_ALLOCATED_VIA_HIMEM_NOCACHE:
		{
			pr_debug("stptiSupport_ALLOCATED_VIA_HIMEM_xxxxxx ");
			iounmap(p->AllocatedMemory_p);
			bpa2_free_pages(part,
					(long unsigned int)stptiSupport_VirtToPhys(p, p->AllocatedMemory_p));
		}
		break;
	case stptiSupport_ALLOCATED_VIA_DMA_COHERENT:
		dma_free_coherent(p->Dev, p->AllocatedMemorySize, p->AllocatedMemory_p, p->PhysicalMemoryAddress);
		stpti_printf("dma_free_coherent %p %u bytes %x handle\n", p->AllocatedMemory_p, p->AllocatedMemorySize,
			     (unsigned int)p->PhysicalMemoryAddress);
		break;
	default:
		STPTI_ASSERT(FALSE, "Failure to deallocate memory at 0x%08x (unknown method %d)",
			     (unsigned)p->AllocatedMemory_p, (int)p->MethodUsed);
		break;
	}
	/* indicate this as being no longer allocated */
	p->AllocatedMemory_p = NULL;
	p->DeltaVtoP = 0;
	p->AllocatedMarker = 0;
	p->MethodUsed = stptiSupport_NOT_ALLOCATED;
}

stptiSupport_MessageQueue_t *stptiSupport_MessageQueueCreate(size_t max_message_size, unsigned int max_messages)
{
	stptiSupport_MessageQueue_t *p;
	p = kmalloc(sizeof(stptiSupport_MessageQueue_t), GFP_KERNEL);
	if (p != NULL) {

		init_waitqueue_head(&p->Msg_wq);
		init_waitqueue_head(&p->SpaceAvail_wq);
		atomic_set(&p->MessageWait, 0);
		atomic_set(&p->MsgRemaining, max_messages);
		p->MaxMessages = max_messages;

		p->MessageBuffer = kmalloc(max_message_size * (max_messages), GFP_KERNEL);

		if (p->MessageBuffer != NULL) {
			/* Everything allocated okay, so set defaults */
			p->Write_p = p->MessageBuffer;
			p->Read_p = p->MessageBuffer;
			p->MessageSize = max_message_size;
			p->MessageBufferEnd = p->MessageBuffer + max_message_size * max_messages;
		} else {
			/* A problem with allocation, lets unallocate anything that did allocate successfully */
			kfree(p);
			p = NULL;
		}
	}
	return (p);
}

BOOL stptiSupport_MessageQueuePostTimeoutFundamental(stptiSupport_MessageQueue_t * queue, void *message_p,
						     int TimeoutMS, BOOL InISR)
{
	unsigned long flags;
	BOOL timed_out = FALSE;

	if (TimeoutMS > 0) {
		/* 0 if the @timeout elapsed, -%ERESTARTSYS if it was
		* interrupted by a signal, or the remaining jiffies
		* * (at least 1) if the @condition evaluated to %true
		* before the @timeout elapsed.*/
		if (wait_event_interruptible_timeout(queue->SpaceAvail_wq,
		(atomic_read(&queue->MsgRemaining) <= queue->MaxMessages),
		msecs_to_jiffies(TimeoutMS)) == 0)
			timed_out = TRUE;

	} else if (TimeoutMS == 0) {
		if ((atomic_read(&queue->MsgRemaining) <= queue->MaxMessages)\
				== 0) {
			timed_out = TRUE;
		}
	} else {
		/*return 0 if condition evaluated to true*/
		if (wait_event_interruptible(queue->SpaceAvail_wq,
		(atomic_read(&queue->MsgRemaining) <= queue->MaxMessages))) {
			timed_out = TRUE;
		}
	}
	if (!timed_out) {
		spin_lock_irqsave(&MessageQueueLock, flags);
		{
			U8 *newWrite_p = queue->Write_p + queue->MessageSize;
			if (newWrite_p == queue->MessageBufferEnd) {
				newWrite_p = queue->MessageBuffer;
			}
			memcpy(queue->Write_p, message_p, queue->MessageSize);
			queue->Write_p = newWrite_p;
			atomic_set(&queue->MessageWait, 1);
			atomic_dec(&queue->MsgRemaining);
		}
		spin_unlock_irqrestore(&MessageQueueLock, flags);
		wake_up_interruptible(&queue->Msg_wq);
	}

	return (!timed_out);
}

BOOL stptiSupport_MessageQueueReceiveTimeout(stptiSupport_MessageQueue_t * queue, void *message_p, int TimeoutMS)
{
	unsigned long flags;
	BOOL timed_out = FALSE;
#if !defined(CONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO)	/* FIXME - MODEL currently decrease platform performance - model removed on v2.1 platform */
	if (TimeoutMS > 0) {
		/* 0 if the @timeout elapsed, -%ERESTARTSYS if it was
		* interrupted by a signal, or the remaining jiffies
		* * (at least 1) if the @condition evaluated to %true
		* before the @timeout elapsed.*/
		if (wait_event_interruptible_timeout(queue->Msg_wq,
				(atomic_read(&queue->MessageWait) == 1) ,
				msecs_to_jiffies(TimeoutMS)) == 0)
			timed_out = TRUE;

	} else if (TimeoutMS == 0) {
		if ((atomic_read(&queue->MessageWait) == 1) == 0)
			timed_out = TRUE;
	} else {
		if (wait_event_interruptible(queue->Msg_wq,
				(atomic_read(&queue->MessageWait) == 1)))
			timed_out = TRUE;
	}
#else
	/* VSOC - TANGO MODEL removed due to platform performance, as this timed waiting is bypassed, we try to dequeue unexisting message - so this wait is needed */
	if (wait_event_interruptible(queue->Msg_wq,
				(atomic_read(&queue->MessageWait) == 1)))
		timed_out = TRUE;
#endif

	if (!timed_out) {
		spin_lock_irqsave(&MessageQueueLock, flags);
		{
			U8 *NewRead_p = queue->Read_p + queue->MessageSize;
			if (NewRead_p == queue->MessageBufferEnd) {
				NewRead_p = queue->MessageBuffer;
			}
			memcpy(message_p, queue->Read_p, queue->MessageSize);
			queue->Read_p = NewRead_p;
			atomic_set(&queue->MessageWait, 0);
			atomic_inc(&queue->MsgRemaining);
		}
		spin_unlock_irqrestore(&MessageQueueLock, flags);
		wake_up_interruptible(&queue->SpaceAvail_wq);
	}

	return (!timed_out);
}

void stptiSupport_MessageQueueDelete(stptiSupport_MessageQueue_t * message_queue)
{
	kfree(message_queue->MessageBuffer);
	kfree(message_queue);
}

/* Cut is identified by first nibble dot second nibble, e.g. 0x10 is Cut 1.0, 0x21 is Cut 2.1 */
U32 stptiSupport_IdentifyCut(void)
{
	/* Currently hard-coded cut: 0x10 */
	return 0x10;
}
