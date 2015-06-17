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
   @file   pti_osal.h
   @brief  Additional Operating System Abstraction Layer, and utilities.

   This file contains functions to aid operating system abstration.  It covers
   PTI specific things missing in STOS.

 */

#ifndef _PTI_OSAL_H_
#define _PTI_OSAL_H_

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"
#include "stddefs.h"

/* Includes from API level */

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */
#if defined(CONFIG_ARM)
/* Arm arch misnames ioremap_cache to ioremap_cached */
#define ioremap_cache ioremap_cached
#endif

#define STPTI_SUPPORT_DCACHE_LINE_SIZE (32)

typedef enum {
	/* We deliberately don't have the enum starting from zero to catch initialisation mistakes */
	stptiSupport_UNLOCKED = 1,
	stptiSupport_WRITELOCKED = 2,
	stptiSupport_READLOCKED = 3
} stptiSupport_AccessLockState_t;

/* IPC's for multiple access protection */
typedef struct {
	struct mutex WriteLockMutex;	/**< mutex to protect against simultaneous writes, and reading whilst writing */
	volatile BOOL WriteLockPending;	/**< blocks read requests (protected by WriteLockMutex) */
	volatile int ReadLockCount;	/**< read lock counter (protected by WriteLockMutex) blocks new write requests */
} stptiSupport_AccessLock_t;

typedef struct {
	U32 StartOfResource;
	U32 TotalSizeOfResource;
	int NumberOfPartitions;
	U32 *PartitionStart;
	U32 *PartitionSize;
} stptiHAL_PartitionedResource_t;

typedef enum {
	stptiSupport_NOT_ALLOCATED = 0,
	stptiSupport_ALLOCATED_VIA_DMA_COHERENT,
	stptiSupport_ALLOCATED_VIA_LOWMEM,
	stptiSupport_ALLOCATED_VIA_HIMEM_CACHED,
	stptiSupport_ALLOCATED_VIA_HIMEM_NOCACHE,
} stptiSupport_AllocationMethod_t;

typedef enum {
	stptiSupport_ZONE_NONE = 0,
	stptiSupport_ZONE_SMALL,
	stptiSupport_ZONE_LARGE_NON_SECURE,
	stptiSupport_ZONE_LARGE_SECURE,
} stptiSupport_AllocationZone_t;

typedef struct {
	U32 AllocatedMarker;
	void *AllocatedMemory_p;
	size_t AllocatedMemorySize;
	dma_addr_t PhysicalMemoryAddress;
	unsigned int DeltaVtoP;
	stptiSupport_AllocationMethod_t MethodUsed;
	char *Region_p;
	dma_addr_t dma_handle;
	void *FirstByte_p;	/* Of cached managed region */
	void *LastByte_p;	/* Of cached managed region */
	struct device *Dev;	/* Linux device to use for dma API */
} stptiSupport_DMAMemoryStructure_t;

/* Exported Function Prototypes -------------------------------------------- */

/* Access lock mechanism */
void stptiSupport_ReadLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiSupport_WriteLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiSupport_TryWriteLock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiSupport_Unlock(stptiSupport_AccessLock_t * Lock_p, stptiSupport_AccessLockState_t * LocalLockState_p);

/* Interrupt Message Queue */

typedef struct {
	U8 *MessageBuffer;
	U8 *Write_p;
	U8 *Read_p;
	size_t MessageSize;
	U8 *MessageBufferEnd;
	U32 MaxMessages;
	atomic_t MsgRemaining;
	atomic_t MessageWait;
	wait_queue_head_t  Msg_wq;			/**< Used to alert of a new message being added */
	wait_queue_head_t  SpaceAvail_wq;		/**< Used to alert that space is available */
} stptiSupport_MessageQueue_t;

/* The STOS message queue calls are interrupt safe for OS21, but are not interrupt safe for linux, so
   we have created our own "simple" version here, which are interrupt safe. */
stptiSupport_MessageQueue_t *stptiSupport_MessageQueueCreate(size_t max_message_size, unsigned int max_messages);
#define stptiSupport_MessageQueuePostTimeout( q, m, t ) stptiSupport_MessageQueuePostTimeoutFundamental( q, m, t, FALSE )
#define stptiSupport_MessageQueuePostTimeoutISR( q, m, t ) stptiSupport_MessageQueuePostTimeoutFundamental( q, m, t, TRUE )
BOOL stptiSupport_MessageQueuePostTimeoutFundamental(stptiSupport_MessageQueue_t * queue, void *message_p,
						     int TimeoutMS, BOOL InISR);
BOOL stptiSupport_MessageQueueReceiveTimeout(stptiSupport_MessageQueue_t * queue, void *message_p, int TimeoutMS);
void stptiSupport_MessageQueueDelete(stptiSupport_MessageQueue_t * message_queue);

/* Memory Alignment and Caching Aligning */
void stptiSupport_InvalidateRegion(stptiSupport_DMAMemoryStructure_t * p, void *addr, size_t size);
void stptiSupport_FlushRegion(stptiSupport_DMAMemoryStructure_t * p, void *addr, size_t size);
void *stptiSupport_VirtToPhys(stptiSupport_DMAMemoryStructure_t * p, void *addr);

#define stptiSupport_AlignUpwards( x, alignment ) (((x)+(alignment)-1) & ~((alignment)-1))
void *stptiSupport_MemoryAlign(void *Memory_p, U32 Alignment);

/* Register / Peripheral Access Functions - These are done as inline functions to facilitate type
 * checking, since STAPI low level STSYS functions cast preventing detection of conflicting types. */

static __inline void stptiSupport_WriteReg8(volatile U8 * address_p, U8 value)
{
	writeb(value, address_p);
}

static __inline U8 stptiSupport_ReadReg8(volatile U8 * address_p)
{
	return readb(address_p);
}

static __inline void stptiSupport_WriteReg16(volatile U16 * address_p, U16 value)
{
	writew(value, address_p);
}

static __inline U16 stptiSupport_ReadReg16(volatile U16 * address_p)
{
	return readw(address_p);
}

static __inline void stptiSupport_WriteReg32(volatile U32 * address_p, U32 value)
{
	writel(value, address_p);
}

static __inline U32 stptiSupport_ReadReg32(volatile U32 * address_p)
{
	return readl(address_p);
}

/* U64 access need to be broken down as two U32 accesses (TANGO only handles stbus transactions up to U32) */
static __inline void stptiSupport_WriteReg64(volatile U64 * Address_p, U64 value)
{
	U32 *p = (U32 *) Address_p;
	writel((U32) value, p);
	writel((U32) (value >> 32), (p + 1));
}

/* U64 access need to be broken down as two U32 accesses (TANGO only handles stbus transactions up to U32) */
static __inline U64 stptiSupport_ReadReg64(volatile U64 * Address_p)
{
	U64 result;
	U32 *p = (U32 *) Address_p;

	result = readl(p + 1);
	result <<= 32;
	result |= readl(p);

	return result;
}

#define stptiSupport_ReadModifyWriteReg32(address, mask, value) stptiSupport_WriteReg32( address, (value) | (~(mask) & stptiSupport_ReadReg32( address )) )

/* Semaphore wrapper to standardise timeout values */
BOOL stptiSupport_TimedOutWaitingForSemaphore(struct semaphore * Semaphore_p, int timeout_ms);

/* There are cases in the driver where we need to wait for a very short time (usually less than
   a couple of microseconds), where we are waiting on the hardware/firmware.   This MACRO is for
   providing a facility to timed out waiting for it, and to encapsulate the option of Yielding to
   higher priority tasks.   By centralising it, we can remove Yielding, if PTI performance
   becomes paramount. */
#define stptiSupport_SubMilliSecondWait( x, error ) \
{                                                   \
    int sleeps = 100;                               \
    while (x && sleeps > 0)                         \
    {                                               \
        usleep_range(10,10);                        \
        sleeps--;                                   \
    }                                               \
    if (x)                                          \
	(*error) = ST_ERROR_TIMEOUT;                \
}

/* Partitioned Resource Manager */
int stptiSupport_PartitionedResourceSetup(stptiHAL_PartitionedResource_t * rms_p,
					  U32 StartOfResource, U32 TotalSizeOfResource, int NumberOfPartitions);
int stptiSupport_PartitionedResourceAllocPartition(stptiHAL_PartitionedResource_t * rms_p, U32 ContiguousSpaceRequired,
						   int *PartitionID_p);
int stptiSupport_PartitionedResourceResizePartition(stptiHAL_PartitionedResource_t * rms_p, int PartitionID, U32 NewContiguousSpaceRequired);	/* Reduce or Grow a Partition (when growing other regions may move) */
int stptiSupport_PartitionedResourceCompact(stptiHAL_PartitionedResource_t * rms_p);
int stptiSupport_PartitionedResourceFreePartition(stptiHAL_PartitionedResource_t * rms_p, int PartitionID);
int stptiSupport_PartitionedResourceDestroy(stptiHAL_PartitionedResource_t * rms_p);

#define stptiSupport_PartitionedResourceStart(rms_p, PartitionID)  ((rms_p)->PartitionStart[PartitionID])
#define stptiSupport_PartitionedResourceSize(rms_p, PartitionID)   ((rms_p)->PartitionSize[PartitionID])

/* Contiguous Memory Allocators for DMA */
void *stptiSupport_MemoryAllocateForDMA(size_t Size, U32 BaseAlignment,
					stptiSupport_DMAMemoryStructure_t * MemoryStructure,
					stptiSupport_AllocationZone_t Zone);
void *stptiSupport_MemoryAllocatePreallocatedForDMA(void *AllocatedMemory_p, void *AllocatedMemoryPHYS_p, size_t Size,
						    stptiSupport_DMAMemoryStructure_t * MemoryStructure);
void stptiSupport_MemoryDeallocateForDMA(stptiSupport_DMAMemoryStructure_t * MemoryStructure);

U32 stptiSupport_IdentifyCut(void);

#endif /* _PTI_OSAL_H_ */
