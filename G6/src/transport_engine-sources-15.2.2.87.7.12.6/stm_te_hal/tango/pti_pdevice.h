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
   @file   pti_pdevice.h
   @brief  Physical Device Initialisation and Termination functions

   This file implements the functions for initialising and terminating a physical device.  A pDevice
   is not stictly an object, but it is referenced by a vDevice and its is a way of sharing
   information between vDevices.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_PDEVICE_H_
#define _PTI_PDEVICE_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_tango.h"
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"
#include "../config/pti_platform.h"

/* Exported Types ---------------------------------------------------------- */

typedef enum {
	stptiHAL_STXP70_NO_FIRMWARE = 0,		/**< Needs firmware loading */
	stptiHAL_STXP70_STOPPED,			/**< Firmware loaded, CPU needs enabling */
	stptiHAL_STXP70_AWAITING_INITIALISATION,	/**< Firmware loaded, CPU running, needs configuration */
	stptiHAL_STXP70_RUNNING,			/**< Firmware loaded, CPU running, configured and active */
	stptiHAL_STXP70_PREPARING_TO_SLEEP,		/**< Firmware loaded, configured, and CPU preparing to sleep */
	stptiHAL_STXP70_SLEEPING,			/**< Firmware loaded, configured, but CPU sleeping */
	stptiHAL_STXP70_POWERDOWN,			/**< CPU Powerdown similar to no firmware but shared memory needs restore */
} stptiHAL_STxP70_Status_t;

typedef struct {
	void *TP_MappedAddress;
	volatile U8 *dDEM_p;
	volatile U8 *iDEM_p;
	stptiHAL_TangoSTBusPlug_t *STBusPlugWrite_p;
	stptiHAL_TangoSTBusPlug_t *STBusPlugRead_p;
	stptiHAL_TangoCoreCtrl_t *CoreCtrl_p;
	stptiHAL_TangoMailboxDevice_t *Mailbox0ToXP70_p;
	stptiHAL_TangoMailboxDevice_t *Mailbox0ToHost_p;
	stptiHAL_TangoT3AddrFilter_t *T3AddrFilter_p;
	volatile U32 *WriteLockError_p;
	volatile U32 *TimerCounter_p;
} stptiHAL_TangoDevice_t;

typedef struct {
	Object_t ObjectHeader;	/* Must be first in structure                                  */

	/* Register Interface Regions */
	stptiHAL_TangoDevice_t TP_MappedAddresses;		/**< Mapped TP device address(es) */
	stptiHAL_STxP70_Status_t TP_Status;			/**< TP Firmware Status */
	U32 TP_InterruptCount;					/**< A count of all TP interrupts (specifically # of ISR calls) */
	U32 TP_UnhandledInterrupts;				/**< A count of unhandled TP interrupts */

	stptiTP_Interface_t *TP_Interface_p;			/**< A pointer to the TP_Interface (TP Memory Map) note any "pointers" in this structure will be TP addresses rather than HOST addresses */
	stptiTP_Interface_t TP_Interface;			/**< A copy of the TP_Interface (TP Memory Map with any pointers converted to HOST addresses) */

	U8 *SharedMemoryBackup_p;				/**< Pointer to save shared memory interface memory for powerdown in Passive Standby */

	/* Resource Management (the position in each list corresponds to the index into TP structure) */
	/* The driver should access these lists through a vDevice pointer - not via pDevice pointer */
	List_t FrontendHandles;					/**< List of Pointers to Frontend Objects (Shared TP Resource) */
	List_t SoftwareInjectorHandles;				/**< List of Pointers to SoftwareInjection Objects (Shared TP Resource) */
	List_t SlotHandles;					/**< List of Pointers to Buffer Objects (Shared Resource) */
	List_t BufferHandles;					/**< List of Pointers to Buffer Objects (Shared Resource) */
	List_t IndexHandles;					/**< List of Pointers to Index Objects (Shared Resource) */

	/* Partitioned Resources Management */
	stptiHAL_PartitionedResource_t PIDTablePartitioning;	/**< Boundries of SlotInfo in TP interface */
	stptiHAL_PartitionedResource_t FilterCAMPartitioning;	/**< Boundries of CAM Filter bytes in TP interface */

	/* Message Queues */
	stptiSupport_MessageQueue_t *EventQueue_p;		/**< A pointer to the OS message queue used to receive status blocks for events */
	U32 StatusBlockBufferOverflows;
	U32 EventQueueOverflows;
	U32 ConcurrentSuccessfulEventPushes;

	/* Signal Queue Stats */
	U32 SignallingQueueOverflows;

	/* semaphores for task/interrupt interaction */
	struct semaphore TPAckSemaphore;						/**< TP HOST SYNC Semaphore */
	struct semaphore *InjectionSemaphore_p[STREAM_AVAILIABLITY_PLAYBACK_CHANNELS];	/**< SoftwareInjection (Playback) Node Completion Semaphores */

	BOOL SWLeakyPIDAvailable;
	BOOL ClocksEnabled;
	struct platform_device *pdev;
} stptiHAL_pDevice_t;

/* Exported Variables ------------------------------------------------------ */

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_pDeviceAPI_t stptiHAL_pDeviceAPI;

#define stptiHAL_GetObjectpDevice_p(pDeviceHandle)     ((stptiHAL_pDevice_t *)stptiOBJMAN_HandleToObjectPointer(pDeviceHandle,OBJECT_TYPE_PDEVICE))

ST_ErrorCode_t stptiHAL_pDeviceReset(FullHandle_t pDeviceHandle);
ST_ErrorCode_t stptiHAL_pDeviceSetPowerStateInternal(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState);

/* Transport Processor Shared Memory Interface Access Functions */

/* One MACRO for any size writes... the intention is that the compiler optimisation will trivialise this */
#define stptiHAL_pDeviceXP70Write( Item, Value ) \
            { if(sizeof(*(Item))==1)      {stptiSupport_WriteReg8((U8 *)(Item), (U8)(Value));}       \
              else if(sizeof(*(Item))==2) {stptiSupport_WriteReg16((U16 *)(Item), (U16)(Value));}    \
              else if(sizeof(*(Item))==4) {stptiSupport_WriteReg32((U32 *)(Item), (U32)(Value));}         \
              else if(sizeof(*(Item))==8) {stptiSupport_WriteReg64((U64 *)(Item), (U64)(Value));}         \
              else {STPTI_ASSERT( FALSE, "TP Write Failure - unhandled size %dbytes", (int)sizeof(*(Item)) );}       \
            }

/* One MACRO for any size reads... the intention is that the compiler optimisation will trivialise this */
#define stptiHAL_pDeviceXP70Read( Item ) ( (__typeof__(*(Item))) \
            ( (sizeof(*(Item))==1) ? stptiSupport_ReadReg8((U8 *)(Item))   : \
            ( (sizeof(*(Item))==2) ? stptiSupport_ReadReg16((U16 *)(Item)) : \
            ( (sizeof(*(Item))==4) ? stptiSupport_ReadReg32((U32 *)(Item)) : \
            ( (sizeof(*(Item))==8) ? stptiSupport_ReadReg64((U64 *)(Item)) : \
            ( 0 ) ) ) ) ) )

#define stptiHAL_pDeviceXP70ReadModifyWrite( Item, Mask, Value )    stptiHAL_pDeviceXP70Write( Item, ( (Value) | (~(Mask) & stptiHAL_pDeviceXP70Read(Item)) ) )
#define stptiHAL_pDeviceXP70BitSet( Item, Mask )                    stptiHAL_pDeviceXP70Write( Item, ( (Mask) | stptiHAL_pDeviceXP70Read(Item) ) )
#define stptiHAL_pDeviceXP70BitClear( Item, Mask )                  stptiHAL_pDeviceXP70Write( Item, ( (~Mask) & stptiHAL_pDeviceXP70Read(Item) ) )

void *stptiHAL_pDeviceXP70MemcpyTo(void *d, const void *s, size_t n);
void *stptiHAL_pDeviceXP70MemcpyFrom(void *d, const void *s, size_t n);
void *stptiHAL_pDeviceXP70Memset(void *d, int c, size_t n);

void stptiHAL_pDeviceDefragmentPidTablePartitions(FullHandle_t pDeviceHandle);
void stptiHAL_pDeviceCompactPidTablePartitions(FullHandle_t pDeviceHandle);
void stptiHAL_pDeviceUpdatePidTable(FullHandle_t pDeviceHandle);

#endif /* _PTI_PDEVICE_H_ */
