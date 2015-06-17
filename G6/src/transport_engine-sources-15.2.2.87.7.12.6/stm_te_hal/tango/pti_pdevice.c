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
   @file   pti_pdevice.c
   @brief  Physical Device Initialisation and Termination functions

   This file implements the functions for initialising and terminating a physical device.  A pDevice
   is not stictly an object, but it is referenced by a vDevice and its is a way of sharing
   information between vDevices.

   As such most of the APIs should not be

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_osal.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "pti_pdevice.h"
#include "pti_vdevice.h"
#include "pti_slot.h"
#include "pti_buffer.h"
#include "pti_isr.h"
#include "pti_mailbox.h"
#include "pti_filter.h"
#include "pti_index.h"
#include "pti_evt.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_pDeviceInit(FullHandle_t pDeviceHandle, void *params);
ST_ErrorCode_t stptiHAL_pDeviceEventTask(FullHandle_t pDeviceHandle);
ST_ErrorCode_t stptiHAL_pDeviceEventTaskAbort(FullHandle_t pDeviceHandle);
ST_ErrorCode_t stptiHAL_pDeviceLiveTask(FullHandle_t pDeviceHandle);
ST_ErrorCode_t stptiHAL_pDeviceGetCapability(FullHandle_t pDeviceHandle,
						stptiHAL_pDeviceConfigStatus_t *ConfigStatus_p);
ST_ErrorCode_t stptiHAL_pDeviceSetPowerState(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState);
ST_ErrorCode_t stptiHAL_pDeviceGetPlatformDevice(FullHandle_t pDeviceHandle, struct platform_device **pdev);

ST_ErrorCode_t stptiHAL_pDeviceTerm(FullHandle_t pDeviceHandle);
ST_ErrorCode_t stptiHAL_pDeviceConfigureLive(FullHandle_t pDeviceHandle,
					     stptiHAL_pDeviceConfigLiveParams_t *LiveParams_p);

ST_ErrorCode_t stptiHAL_pDeviceEnableSWLeakyPID(FullHandle_t pDeviceHandle, unsigned int LiveChannel,
						BOOL StreamIsActive);
ST_ErrorCode_t stptiHAL_pDeviceSWLeakyPIDAvailable(FullHandle_t pDeviceHandle, BOOL *IsAvailable);

static ST_ErrorCode_t stptiHAL_pDeviceSleep(FullHandle_t pDeviceHandle);
static ST_ErrorCode_t stptiHAL_pDeviceWakeup(FullHandle_t pDeviceHandle);
static ST_ErrorCode_t stptiHAL_pDevicePowerdown(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState);
static ST_ErrorCode_t stptiHAL_pDevicePowerUp(FullHandle_t pDeviceHandle);
static ST_ErrorCode_t stptiHAL_pDeviceEnableClocks(FullHandle_t pDeviceHandle);
static ST_ErrorCode_t stptiHAL_pDeviceDisableClocks(FullHandle_t pDeviceHandle);

ST_ErrorCode_t stptiHAL_pDeviceGetCycleCount(FullHandle_t pDeviceHandle, unsigned int *count);

static void stptiHAL_pDeviceSetInitialisationParameters(stptiTP_Interface_t *TP_Interface_p,
							struct platform_device *pdev);
static void stptiHAL_pDeviceSetupTPMailboxes(FullHandle_t pDeviceHandle);
static ST_ErrorCode_t stptiHAL_pDeviceLoadSTXP70(stptiHAL_TangoDevice_t *stxp70_p, struct platform_device *pdev);
static void stptiHAL_pDeviceResetSTXP70(stptiHAL_TangoDevice_t *stxp70_p);
static void stptiHAL_pDeviceStartSTXP70(stptiHAL_TangoDevice_t *stxp70_p);
static ST_ErrorCode_t stptiHAL_pDevicePowerdownSTXP70(stptiHAL_TangoDevice_t *stxp70_p);

static ST_ErrorCode_t stptiHAL_LiveTask(FullHandle_t pDeviceHandle);

/* Public Constants ------------------------------------------------------- */

/* Public Variables ------------------------------------------------------- */

/* Export the API */
const stptiHAL_pDeviceAPI_t stptiHAL_pDeviceAPI = {
	{
		/* Allocator          Associator  Disassociator  Deallocator */
		stptiHAL_pDeviceInit, NULL,       NULL,          stptiHAL_pDeviceTerm,
		NULL, NULL
	},

	stptiHAL_pDeviceEventTask,
	stptiHAL_pDeviceEventTaskAbort,

	stptiHAL_pDeviceLiveTask,	/* Must be exposed for TSINPUT (STFE) HAL */
	stptiHAL_pDeviceConfigureLive,	/* Must be exposed for TSINPUT (STFE) HAL */
	stptiHAL_pDeviceEnableSWLeakyPID,	/* Must be exposed for TSINPUT (STFE) HAL */
	stptiHAL_pDeviceSWLeakyPIDAvailable,	/* Must be exposed for TSINPUT (STFE) HAL */
	stptiHAL_pDeviceGetCapability,
	stptiHAL_pDeviceGetCycleCount,
	stptiHAL_pDeviceSetPowerState,
	stptiHAL_pDeviceGetPlatformDevice
};

/**
   @brief  Initialise a pDevice.

   This Maps Registers, installs interrupts, starts tasks, starts TP, and sets up the TP interface.

   @param  pDeviceHandle  The (full) handle of the pDevice.
   @param  params         Pointer to platform device to initialise

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_NO_MEMORY           (unable to map registers)
                          - ST_ERROR_INTERRUPT_INSTALL   (unable to install interrupt)
                          - ST_ERROR_TIMEOUT             (timedout waiting for TP ack)
 */
ST_ErrorCode_t stptiHAL_pDeviceInit(FullHandle_t pDeviceHandle, void *params)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	BOOL ListsAllocated = FALSE;
	BOOL PartitionedResourceManagersAllocated = FALSE;

	BOOL TPInterruptInstalled = FALSE;
	U8 *TP_MappedAddress = NULL;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	struct platform_device *pdev = (struct platform_device *)params;
	struct stpti_tp_config *tp_config = pdev->dev.platform_data;
	struct resource *res;
	S32 TP_InterruptNumber = -1;

	/* Can be useful if using timestamped debug printf (see STPTI_DEBUG_ADD_TIMESTAMPS in pti_debug.c) */
	STPTI_PRINTF("Clocks Per Second = %u.", (unsigned)HZ);

	memset(&pDevice_p->TP_MappedAddresses, 0, sizeof(pDevice_p->TP_MappedAddresses));

	/* Store reference to device (use get_device to increment device's
	 * refcount) */
	pDevice_p->pdev = to_platform_device(get_device(&pdev->dev));

	/* Set device driver data */
	platform_set_drvdata(pdev, pDevice_p);

	/* Set device DMA coherent mask (32-bit address space) */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
#else
	if (dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		dev_err(&pdev->dev, "dma_set_coherent_mask failed\n");
		Error = ST_ERROR_BAD_PARAMETER;
	}
#endif

	pDevice_p->SWLeakyPIDAvailable = tp_config->software_leaky_pid;

	/* Map TP registers */
	if (ST_NO_ERROR == Error) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tp");
		if (!res) {
			STPTI_PRINTF_ERROR("No address info found for TP %d\n",
					   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
			Error = ST_ERROR_UNKNOWN_DEVICE;
		} else {
			dev_info(&pdev->dev, "Resource %s [0x%x:0x%x]\n", res->name, res->start, res->end);
		}

		if (ST_NO_ERROR == Error) {
			TP_MappedAddress = ioremap_nocache(res->start, resource_size(res));
			STPTI_PRINTF("TANGO TP mapped... [ %08x => %p ]", res->start, TP_MappedAddress);
		}

		if (TP_MappedAddress == NULL) {
			STPTI_PRINTF_ERROR("Address Mapping Failure for TP %d",
					   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
			Error = ST_ERROR_NO_MEMORY;
		} else {
			pDevice_p->TP_MappedAddresses.TP_MappedAddress = TP_MappedAddress;
			pDevice_p->TP_MappedAddresses.dDEM_p = TP_MappedAddress + tp_config->ddem_offset;
			pDevice_p->TP_MappedAddresses.iDEM_p = TP_MappedAddress + tp_config->idem_offset;
			pDevice_p->TP_MappedAddresses.STBusPlugWrite_p =
			    (stptiHAL_TangoSTBusPlug_t *) (TP_MappedAddress + tp_config->st_bux_plug_write_offset);
			pDevice_p->TP_MappedAddresses.STBusPlugRead_p =
			    (stptiHAL_TangoSTBusPlug_t *) (TP_MappedAddress + tp_config->st_bux_plug_read_offset);
			pDevice_p->TP_MappedAddresses.CoreCtrl_p =
			    (stptiHAL_TangoCoreCtrl_t *) (TP_MappedAddress + tp_config->core_ctrl_offset);
			pDevice_p->TP_MappedAddresses.Mailbox0ToXP70_p =
			    (stptiHAL_TangoMailboxDevice_t *) (TP_MappedAddress + tp_config->mailbox0_to_xp70_offset);
			pDevice_p->TP_MappedAddresses.Mailbox0ToHost_p =
			    (stptiHAL_TangoMailboxDevice_t *) (TP_MappedAddress + tp_config->mailbox0_to_host_offset);
			pDevice_p->TP_MappedAddresses.WriteLockError_p =
			    (U32 *) (TP_MappedAddress + tp_config->writelock_error_offset);
			pDevice_p->TP_MappedAddresses.TimerCounter_p =
			    (U32 *) (TP_MappedAddress + tp_config->timer_counter_offset);
			if (tp_config->t3_addr_filter_offset) {
				pDevice_p->TP_MappedAddresses.T3AddrFilter_p =
				    (stptiHAL_TangoT3AddrFilter_t *) (TP_MappedAddress +
								      tp_config->t3_addr_filter_offset);
			}
		}
	}

	/* Semaphore setup */
	if (ST_NO_ERROR == Error) {
		sema_init(&pDevice_p->TPAckSemaphore, 0);
		memset(pDevice_p->InjectionSemaphore_p, 0, ARRAY_SIZE(pDevice_p->InjectionSemaphore_p));
	}

	/* List allocation for managing shared TP resources */
	if (ST_NO_ERROR == Error) {
		if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&pDevice_p->FrontendHandles)) {
			Error = ST_ERROR_NO_MEMORY;
		} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&pDevice_p->SoftwareInjectorHandles)) {
			Error = ST_ERROR_NO_MEMORY;
		} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&pDevice_p->SlotHandles)) {
			Error = ST_ERROR_NO_MEMORY;
		} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&pDevice_p->BufferHandles)) {
			Error = ST_ERROR_NO_MEMORY;
		} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&pDevice_p->IndexHandles)) {
			Error = ST_ERROR_NO_MEMORY;
		}

		if (ST_NO_ERROR != Error) {
			STPTI_PRINTF_ERROR("Memory Allocation Failure (for lists)");
			stptiOBJMAN_DeallocateList(&pDevice_p->FrontendHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->SoftwareInjectorHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->SlotHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->BufferHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->IndexHandles);
		} else {
			ListsAllocated = TRUE;
		}
	}

	/* Enable clocks */
	if (ST_NO_ERROR == Error) {
		stptiHAL_pDeviceEnableClocks(pDeviceHandle);
	}

	/* Reset and stall the cores if there is no Firmware loaded (first initialisation) */
	if (ST_NO_ERROR == Error) {
		if (pDevice_p->TP_Status == stptiHAL_STXP70_NO_FIRMWARE) {
			stptiHAL_pDeviceResetSTXP70(&pDevice_p->TP_MappedAddresses);
		}
	}

	/* TP Interrupts */
	if (ST_NO_ERROR == Error) {
		TP_InterruptNumber = platform_get_irq(pdev, 0);
		dev_info(&pdev->dev, "IRQ %d\n", TP_InterruptNumber);

		/* Reset the unhandle interrupt counter (should always remain zero) */
		pDevice_p->TP_UnhandledInterrupts = 0;
		pDevice_p->TP_InterruptCount = 0;

		/* setup Mailboxes for the TP */
		stptiHAL_pDeviceSetupTPMailboxes(pDeviceHandle);

		/* Setup interrupt handler */

		if (request_irq(TP_InterruptNumber, stptiHAL_TPInterruptHandler,
				0, "TANGO_TP_MBX", (void *)pDeviceHandle.word)) {
			STPTI_PRINTF_ERROR("TP ISR installation failed on number 0x%08x.", TP_InterruptNumber);
			Error = ST_ERROR_INTERRUPT_INSTALL;
		} else {
			TPInterruptInstalled = TRUE;
			STPTI_PRINTF("TP ISR installed on number 0x%08x.", TP_InterruptNumber);
		}
	}

	/* Load Transport Processor */
	if (ST_NO_ERROR == Error && pDevice_p->TP_Status == stptiHAL_STXP70_NO_FIRMWARE) {
		if (tp_config->firmware) {
			Error = stptiHAL_pDeviceLoadSTXP70(&pDevice_p->TP_MappedAddresses, pdev);
			if (ST_NO_ERROR == Error) {
				STPTI_PRINTF("TP Firmware loaded successfully");
			} else {
				STPTI_PRINTF_ERROR("TP Firmware load FAILURE");
			}
		} else {
			STPTI_PRINTF("TP Firmware assumed already loaded.");
		}
		pDevice_p->TP_Status = stptiHAL_STXP70_STOPPED;
	}

	/* Start Transport Processor */
	if (ST_NO_ERROR == Error && pDevice_p->TP_Status == stptiHAL_STXP70_STOPPED) {
		/* Prefill Memory with a value which will get overwritten by xp70 when it starts */
		stptiTP_Interface_t *TP_Interface_p = (stptiTP_Interface_t *) & pDevice_p->TP_MappedAddresses.dDEM_p[0];
		stptiHAL_pDeviceXP70Write(&TP_Interface_p->ActivityCounter, 0xF0000000);

		/* Start TP stxp70 */
		stptiHAL_pDeviceStartSTXP70(&pDevice_p->TP_MappedAddresses);
		STPTI_PRINTF("Started TP from reset (waiting for Ready)");
		pDevice_p->TP_Status = stptiHAL_STXP70_AWAITING_INITIALISATION;
	}

	if (ST_NO_ERROR == Error && pDevice_p->TP_Status == stptiHAL_STXP70_AWAITING_INITIALISATION) {
		/* wait upto 100ms for host_sync - which lets us know the device is up and running */
		if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 2000)) {

			STPTI_PRINTF_ERROR("Unable to start TP (device%d).",
					   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));

			/* Provide debug information to aid debugging why the TP failed to start */
			{
				stptiTP_Interface_t *TP_Interface_p =
				    (stptiTP_Interface_t *) & pDevice_p->TP_MappedAddresses.dDEM_p[0];
				U32 debug_ac1, debug_cs1;
				U32 debug_ac2, debug_cs2, debug_wos;
				char debug_v1[sizeof (TP_Interface_p->Version)];
				char debug_v2[sizeof (TP_Interface_p->Version)];

				debug_ac1 = stptiHAL_pDeviceXP70Read(&TP_Interface_p->ActivityCounter);
				stptiHAL_pDeviceXP70MemcpyFrom(debug_v1,
								&TP_Interface_p->Version,
								sizeof (debug_v1));
				debug_cs1 = stptiHAL_pDeviceXP70Read(&TP_Interface_p->CheckSum);
				stptiHAL_pDeviceXP70MemcpyTo(&TP_Interface_p->Version, "FFFFFFFF",
							sizeof (TP_Interface_p->Version));
				stptiHAL_pDeviceXP70Write(&TP_Interface_p->CheckSum, 0xFFFFFFFF);
				udelay(1000);

				debug_ac2 = stptiHAL_pDeviceXP70Read(&TP_Interface_p->ActivityCounter);
				stptiHAL_pDeviceXP70MemcpyFrom(debug_v2,
								&TP_Interface_p->Version,
								sizeof (debug_v2));
				debug_cs2 = stptiHAL_pDeviceXP70Read(&TP_Interface_p->CheckSum);
				debug_wos = stptiHAL_pDeviceXP70Read(&TP_Interface_p->WaitingOnStreamer);

				STPTI_PRINTF_ERROR("TP debug: (line1) Scratch: 0x%08x 0x%08x 0x%08x 0x%08x",
						   stptiHAL_pDeviceXP70Read(&TP_Interface_p->DebugScratch[0]),
						   stptiHAL_pDeviceXP70Read(&TP_Interface_p->DebugScratch[1]),
						   stptiHAL_pDeviceXP70Read(&TP_Interface_p->DebugScratch[2]),
						   stptiHAL_pDeviceXP70Read(&TP_Interface_p->DebugScratch[3]));

				STPTI_PRINTF_ERROR
				    ("TP debug: (line2) 0x%08x->0x%08x  0x%s->0x%s  0x%08x->0x%08x  0x%08x",
				     debug_ac1, debug_ac2, debug_v1, debug_v2, debug_cs1, debug_cs2, debug_wos);

				if (debug_ac2 == debug_ac1) {
					if (debug_ac2 == 0xF0000000) {
						STPTI_PRINTF_ERROR("TP debug: (line3) Failed to Start Execution.");
					} else if (debug_ac2 >= 0xF0000000) {
						STPTI_PRINTF_ERROR
						    ("TP debug: (line3) Early Bootup Failure.  Report 0x%08x.",
						     debug_ac2);
					} else {
						STPTI_PRINTF_ERROR
						    ("TP debug: (line3) Unknown Bootup Failure (ActivityCounter static at 0x%08x).",
						     debug_ac2);
					}
				} else {	/* debug_ac1 != debug_ac2 */

					if ( strncmp(debug_v2, "FFFFFFFF", 8) || debug_cs2 != 0xFFFFFFFF) {
						STPTI_PRINTF_ERROR
						    ("TP debug: (line3) TP Booted and stalled in Bootup Handshake (sent Ack, awaiting Ack from Host - Interrupt Failure?).");
					} else {
						STPTI_PRINTF_ERROR
						    ("TP debug: (line3) Unknown Bootup Failure (Skipped Bootup Handshake?).");
					}
				}

				STPTI_PRINTF_ERROR
				    ("TP debug: (line4) Mailbox Masked Status 0x%08x, Detected InterruptCount=%d, UnhandledInterrupts=%d.",
				     stptiHAL_MailboxInterruptTP(pDeviceHandle), pDevice_p->TP_InterruptCount,
				     pDevice_p->TP_UnhandledInterrupts);
			}

			if (tp_config->permit_powerdown) {
				stptiHAL_pDevicePowerdownSTXP70(&pDevice_p->TP_MappedAddresses);
			}
			pDevice_p->TP_Status = stptiHAL_STXP70_STOPPED;
			Error = ST_ERROR_TIMEOUT;
		} else {
			/* State the required capabilities for the TP */

			/* The TP_Interface structure is always first in TP memory */
#if !defined(CONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO)	/* FIXME  - if no model, register access not available for RW, simulate with allocated structure  */
			pDevice_p->TP_Interface_p = (stptiTP_Interface_t *) & pDevice_p->TP_MappedAddresses.dDEM_p[0];
#else
			pDevice_p->TP_Interface_p = kmalloc(sizeof(stptiTP_Interface_t), GFP_KERNEL);
			memset(pDevice_p->TP_Interface_p, 0, sizeof(stptiTP_Interface_t));
#endif
			stptiHAL_pDeviceSetInitialisationParameters(pDevice_p->TP_Interface_p, pdev);

			/* Indicate to TP that we have set the initialisation parameters */
			stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_SUPPLYING_INIT_PARAMS, 0);

			/* Wait for acknowledgement that initialisation is complete */
			if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
				STPTI_PRINTF_ERROR("Unable to configure TP (device%d).",
						   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
				Error = ST_ERROR_TIMEOUT;
			} else {
				unsigned int i;
				const char *VERSION=STPTI_TP_VERSION_ID;
				/* Check the TP's response. */
				stptiHAL_pDeviceXP70MemcpyFrom(&pDevice_p->TP_Interface, pDevice_p->TP_Interface_p,
							       sizeof(stptiTP_Interface_t));

				/* We check a version id in the structure to make sure the TP code is compatible with this driver.
				   Only checks the first 6 chars of the string, patch and debug version parts of the version are ignored. */
				if (strncmp( (char*)pDevice_p->TP_Interface.Version, VERSION, sizeof (pDevice_p->TP_Interface.Version)-2)) {
					STPTI_PRINTF_ERROR("TP firmware %s (version=%.8s) is not compatible with this HOST driver (%s)",
							tp_config->firmware,
							pDevice_p->TP_Interface.Version,
							STPTI_TP_VERSION_ID);

					Error = ST_ERROR_BAD_PARAMETER;
				}

				else if (pDevice_p->TP_Interface.CheckSum != SHARED_MEMORY_INTERFACE_CHECKSUM) {
					STPTI_PRINTF_ERROR("TP firmware %s interface not compatible with this driver (TP API CheckSum Mismatch).",
					     tp_config->firmware);
					Error = ST_ERROR_BAD_PARAMETER;
				}

				/* We check the response from the TP meets our requirements, if the TP will have set
				   NumberOfvDevices to 0 if it couldn't. */
				else if (pDevice_p->TP_Interface.NumberOfvDevices == 0) {
					Error = ST_ERROR_NO_MEMORY;
				}

				else {
					/* printout TP capabilities for the log */
					STPTI_PRINTF("TP (device%d) Started Successfully.",
						     stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
					STPTI_PRINTF
					    ("Across %d vDevices, there are %d slots, %d buffers, %d indexers, %d hw section filters.",
					     pDevice_p->TP_Interface.NumberOfvDevices,
					     pDevice_p->TP_Interface.NumberOfSlots,
					     pDevice_p->TP_Interface.NumberOfDMAStructures,
					     pDevice_p->TP_Interface.NumberOfIndexers,
					     pDevice_p->TP_Interface.NumberOfSectionFilters);

					/* Lastly we initialise TP Structures to a known state */

					/* Translate the TP addresses to host addresses */
					{
						/* Normally I would cast these to U32's, but the ST40 compiler in its
						   strictest mode will not let you do this ("dereferencing type-punned
						   pointer will break strict-aliasing rules").  So we use U8 * and
						   cast in the loop. */
						U8 *DestinationStart =
						    (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
						U8 *SourceStart = (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
						U8 *SourceEnd = (U8 *) & (pDevice_p->TP_Interface.StatusBlk_p);

						/* Work througn from SharedMemory_p to StatusBlk_p adding the start of dDEM to
						   create a host address (TP addresses start from 0). */
						do {
							*((U32 *) DestinationStart) =
							    (U32) & pDevice_p->TP_MappedAddresses.
							    dDEM_p[*((U32 *) SourceStart)];
							DestinationStart += sizeof(U32);
							SourceStart += sizeof(U32);
						}
						while (SourceStart <= SourceEnd);
					}

					/* TP Debug Message level */
					stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->OutputDebugLevel, 0);	/* 0 means none (default) */
					//stptiHAL_pDeviceXP70Write( &pDevice_p->TP_Interface_p->OutputDebugLevel, 1 );               /* 1 for serious errors */
					//stptiHAL_pDeviceXP70Write( &pDevice_p->TP_Interface_p->OutputDebugLevel, 2 );               /* 2 for serious errors and warnings (has a minor performance hit) */

					{
						volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p =
						    pDevice_p->TP_Interface.pDeviceInfo_p;

						stptiHAL_pDeviceXP70Write(&pDeviceInfo_p->ResetIdleCounters, 1);	/* reset the counters (in case they have been in use previously) */
						stptiHAL_pDeviceXP70Write(&pDeviceInfo_p->SWLeakyPIDChannels, 0);	/* Indicator of which STFE channels have pids set */
					}

					/* vDevices */
					for (i = 0; i < pDevice_p->TP_Interface.NumberOfvDevices; i++) {
						stptiHAL_vDeviceTPEntryInitialise(&pDevice_p->TP_Interface.
										  vDeviceInfo_p[i]);
					}

					/* Slots (unnecessary, but helps when debugging) */
					for (i = 0; i < pDevice_p->TP_Interface.NumberOfSlots; i++) {
						stptiHAL_SlotTPEntryInitialise(&pDevice_p->TP_Interface.SlotInfo_p[i]);
					}

					/* Buffers */
					for (i = 0; i < pDevice_p->TP_Interface.NumberOfDMAStructures; i++) {
						stptiHAL_BufferTPEntryInitialise(&pDevice_p->TP_Interface.DMAInfo_p[i]);
					}

					/* Indexers */
					for (i = 0; i < pDevice_p->TP_Interface.NumberOfIndexers; i++) {
						stptiHAL_IndexerTPEntryInitialise(&pDevice_p->TP_Interface.
										  IndexerInfo_p[i], i);
					}

					/* (fake) PES Markers used for splicing */
					for (i = 0; i < pDevice_p->TP_Interface.NumberOfPESMarkers; i++) {
						stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.
									  PesMarkerBuffers_p[i].allocated, 0);
						stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.
									  PesMarkerBuffers_p[i].next_index, 0xFF);
						stptiHAL_pDeviceXP70MemcpyTo((void *)pDevice_p->TP_Interface.
									     PesMarkerBuffers_p[i].data,
									     "\x00\x00\x01\xFB\x00\x14\x80\x01\x11\x80\x53\x54\x4D\x4D\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
									     26);
					}
				}
			}
		}
		pDevice_p->TP_Status = stptiHAL_STXP70_RUNNING;

		/* Put this pDevice into SLEEP state to conserve power - it will be
		 * woken-up when the first child vDevice is created */
		if (ST_NO_ERROR == Error) {
			Error = stptiHAL_pDeviceSetPowerStateInternal(pDeviceHandle, stptiHAL_PDEVICE_SLEEPING);
		}
	}

	/* Allocation of PartitionManagers for shared Resources */
	if (ST_NO_ERROR == Error) {
		int NumberOfPids = pDevice_p->TP_Interface.NumberOfPids;
		int NumberOfvDevices = pDevice_p->TP_Interface.NumberOfvDevices;
		int NumberOfSectionFilters = pDevice_p->TP_Interface.NumberOfSectionFilters;

		if (stptiSupport_PartitionedResourceSetup(&pDevice_p->PIDTablePartitioning, 0, NumberOfPids, NumberOfvDevices)) {	/* BlockSize is 1 slot */
			STPTI_PRINTF_ERROR("Unable to Setup PID Table Partitioned Resource");
			Error = ST_ERROR_NO_MEMORY;
		} else if (stptiSupport_PartitionedResourceSetup(&pDevice_p->FilterCAMPartitioning, 0, NumberOfSectionFilters, NumberOfvDevices)) {	/* BlockSize is 1 filter */
			STPTI_PRINTF_ERROR("Unable to Setup Section Filter Partitioned Resource");
			stptiSupport_PartitionedResourceDestroy(&pDevice_p->PIDTablePartitioning);
			Error = ST_ERROR_NO_MEMORY;
		} else {
			PartitionedResourceManagersAllocated = TRUE;
		}
	}

	/* Here is the place for catching errors and tidy up */
	if (ST_NO_ERROR != Error) {
		if (PartitionedResourceManagersAllocated) {
			stptiSupport_PartitionedResourceDestroy(&pDevice_p->PIDTablePartitioning);
			stptiSupport_PartitionedResourceDestroy(&pDevice_p->FilterCAMPartitioning);
		}

		/* Disable clocks for this pDevice (always safe to call even if clocks
		 * have not been enabled) */
		stptiHAL_pDeviceDisableClocks(pDeviceHandle);

		if (ListsAllocated) {
			/* Delete Lists */
			stptiOBJMAN_DeallocateList(&pDevice_p->FrontendHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->SoftwareInjectorHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->SlotHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->BufferHandles);
			stptiOBJMAN_DeallocateList(&pDevice_p->IndexHandles);
		}

		if (TPInterruptInstalled) {
			/* There was an error so we disable interrupts */
			disable_irq(TP_InterruptNumber);
			free_irq(TP_InterruptNumber, (void *)pDeviceHandle.word);
			STPTI_PRINTF("TP ISR uninstalled on number 0x%08x.", TP_InterruptNumber);
		}

		/* Unmap any mapped registers */
		if (TP_MappedAddress != NULL) {
			iounmap(pDevice_p->TP_MappedAddresses.TP_MappedAddress);
			memset(&pDevice_p->TP_MappedAddresses, 0, sizeof(pDevice_p->TP_MappedAddresses));
		}

		/* Decrement reference count to device */
		put_device(&pdev->dev);
		platform_set_drvdata(pDevice_p->pdev, NULL);

		STPTI_PRINTF("Unmapped Registers");
	}

	if (ST_NO_ERROR == Error) {
		stptiHAL_debug_add_object(pDeviceHandle);
	}

	return (Error);
}

/**
   @brief  Power Up a pDevice from powerdown to sleep state

   This Maps Registers, installs interrupts, starts tasks, starts TP, and sets up the TP interface.

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_NO_MEMORY           (unable to map registers)
                          - ST_ERROR_INTERRUPT_INSTALL   (unable to install interrupt)
                          - ST_ERROR_TIMEOUT             (timedout waiting for TP ack)
 */
ST_ErrorCode_t stptiHAL_pDevicePowerUp(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	struct stpti_tp_config *tp_config = pDevice_p->pdev->dev.platform_data;

	/* Enable clocks */
	Error = stptiHAL_pDeviceEnableClocks(pDeviceHandle);

	if (ST_NO_ERROR == Error) {
		/* Place the TP into reset ready for load */
		if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != NULL) {
			stptiHAL_pDeviceResetSTXP70(&pDevice_p->TP_MappedAddresses);
			pDevice_p->TP_Status = stptiHAL_STXP70_STOPPED;

			/* Bombproofing... consume any stray TPAcks */
			while (!stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 0)) ;
		}

		/* Restart the TP */
		if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != 0) {
			/*  Load the TP firmware */
			if (tp_config->firmware) {
				Error = stptiHAL_pDeviceLoadSTXP70(&pDevice_p->TP_MappedAddresses, pDevice_p->pdev);
				if (ST_NO_ERROR == Error) {
					STPTI_PRINTF("TP Firmware loaded successfully");
				} else {
					STPTI_PRINTF_ERROR("TP Firmware load FAILURE");
				}
			} else {
				STPTI_PRINTF("TP Firmware assumed already loaded.");
			}

			if (pDevice_p->TP_Status != stptiHAL_STXP70_NO_FIRMWARE) {
				stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->CheckSum, 0);

				/* setup Mailboxes for the TP */
				stptiHAL_pDeviceSetupTPMailboxes(pDeviceHandle);

				/* restore the shared memory interface here */
				if (pDevice_p->SharedMemoryBackup_p == NULL) {
					STPTI_PRINTF_ERROR("TP unable to restore shared memory for powerup (device%d).",
							   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
				} else {
					stptiHAL_pDeviceXP70MemcpyFrom((void *)pDevice_p->TP_Interface.SharedMemory_p,
								       pDevice_p->SharedMemoryBackup_p,
								       pDevice_p->TP_Interface.SizeOfSharedMemoryRegion);
				}

				stptiHAL_pDeviceStartSTXP70(&pDevice_p->TP_MappedAddresses);

				/* wait for ack */
				if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
					STPTI_PRINTF("Unable to restart TP (device%d).",
						     stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
					Error = ST_ERROR_TIMEOUT;
				} else {
					pDevice_p->TP_Status = stptiHAL_STXP70_AWAITING_INITIALISATION;

					stptiHAL_pDeviceSetInitialisationParameters(pDevice_p->TP_Interface_p,
										    pDevice_p->pdev);
					stptiHAL_pDeviceXP70BitClear(&pDevice_p->TP_Interface_p->FirmwareConfig, TP_FIRMWARE_RESET_SHARED_MEMORY);	/* IMPORTANT! don't reset the Shared Memory Intce */

					/* Indicate to TP that we have set the initialisation parameters */
					stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_SUPPLYING_INIT_PARAMS, 0);

					/* Wait for acknowledgement that initialisation is complete */
					if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
						STPTI_PRINTF_ERROR("Unable to reconfigure TP (device%d).",
								   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
						Error = ST_ERROR_TIMEOUT;
					} else {
						/* Check the TP's response. */
						stptiHAL_pDeviceXP70MemcpyFrom(&pDevice_p->TP_Interface,
									       pDevice_p->TP_Interface_p,
									       sizeof(stptiTP_Interface_t));

						if (pDevice_p->TP_Interface.CheckSum != SHARED_MEMORY_INTERFACE_CHECKSUM
						    && pDevice_p->TP_Interface.NumberOfvDevices == 0) {
							STPTI_PRINTF_ERROR("TP (device%d) failed to restart.",
									   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
							Error = ST_ERROR_NO_MEMORY;
						} else {
							/* printout TP capabilities for the log */
							STPTI_PRINTF("TP (device%d) restarted successfully.",
								     stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
							pDevice_p->TP_Status = stptiHAL_STXP70_RUNNING;
						}
						/* Translate the TP addresses to host addresses */
						{
							/* Normally I would cast these to U32's, but the ST40 compiler in its
							   strictest mode will not let you do this ("dereferencing type-punned
							   pointer will break strict-aliasing rules").  So we use U8 * and
							   cast in the loop. */
							U8 *DestinationStart =
							    (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
							U8 *SourceStart =
							    (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
							U8 *SourceEnd = (U8 *) & (pDevice_p->TP_Interface.StatusBlk_p);

							/* Work througn from SharedMemory_p to StatusBlk_p adding the start of dDEM to
							   create a host address (TP addresses start from 0). */
							do {
								*((U32 *) DestinationStart) =
								    (U32) & pDevice_p->TP_MappedAddresses.dDEM_p[*((U32 *) SourceStart)];
								DestinationStart += sizeof(U32);
								SourceStart += sizeof(U32);
							}
							while (SourceStart <= SourceEnd);
						}
					}
				}
			}
		}
	}
	return Error;
}

/**
   @brief  Call the Event Task.

   This function calls the event task function for this pDevice.

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceEventTask(FullHandle_t pDeviceHandle)
{
	/* We deliberately don't lock here it is handled in the lower function */
	return (stptiHAL_EventTask(pDeviceHandle));
}

/**
   @brief  Abort the Event Task.

   This function aborts the event task function for this pDevice.

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceEventTaskAbort(FullHandle_t pDeviceHandle)
{
	/* We deliberately don't lock here it is handled in the lower function */
	return (stptiHAL_EventTaskAbort(pDeviceHandle));
}

/**
   @brief  Call the Live Monitor Task.

   This function calls the event task function for this pDevice.

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceLiveTask(FullHandle_t pDeviceHandle)
{
	/* We deliberately don't lock here it is handled in the lower function */
	return (stptiHAL_LiveTask(pDeviceHandle));

}

/**
   @brief  Terminate a pDevice.

   This unmaps registers, stops the TP, stops tasks and uninstalls interrupts.

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_INTERRUPT_UNINSTALL   (unable to uninstall interrupt)
 */
ST_ErrorCode_t stptiHAL_pDeviceTerm(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	if (ST_NO_ERROR == Error) {
		/* Power down the TP to make sure that it is idle
		 * NOTE: If the TP is currently sleeping, we need to wake it up first
		 * so we can stop it cleanlyi
		 * Ignore errors here since we cannot recover at this point */
		if (stptiHAL_STXP70_SLEEPING == pDevice_p->TP_Status) {
			stptiHAL_pDeviceWakeup(pDeviceHandle);
		}
		stptiHAL_pDevicePowerdown(pDeviceHandle, stptiHAL_PDEVICE_STOPPED);

		if (pDevice_p->SharedMemoryBackup_p != NULL) {
			kfree(pDevice_p->SharedMemoryBackup_p);
			pDevice_p->SharedMemoryBackup_p = NULL;
		}
		disable_irq(platform_get_irq(pDevice_p->pdev, 0));
		free_irq(platform_get_irq(pDevice_p->pdev, 0), (void *)pDeviceHandle.word);
		STPTI_PRINTF("TP ISR uninstalled on number 0x%08x.", platform_get_irq(pDevice_p->pdev, 0));

		/* Delete Lists */
		stptiOBJMAN_DeallocateList(&pDevice_p->FrontendHandles);
		stptiOBJMAN_DeallocateList(&pDevice_p->SoftwareInjectorHandles);
		stptiOBJMAN_DeallocateList(&pDevice_p->SlotHandles);
		stptiOBJMAN_DeallocateList(&pDevice_p->BufferHandles);
		stptiOBJMAN_DeallocateList(&pDevice_p->IndexHandles);

		/* Delete partitioned resources */
		stptiSupport_PartitionedResourceDestroy(&pDevice_p->PIDTablePartitioning);
		stptiSupport_PartitionedResourceDestroy(&pDevice_p->FilterCAMPartitioning);

		stpti_printf("Unmapping registers");
		if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != NULL) {
			iounmap(pDevice_p->TP_MappedAddresses.TP_MappedAddress);
			memset(&pDevice_p->TP_MappedAddresses, 0, sizeof(pDevice_p->TP_MappedAddresses));
		}
#if defined(CONFIG_STM_VIRTUAL_PLATFORM_DISABLE_TANGO) /* FIXME  - if no model, register access not available for RW, simulate with allocated structure  */
		kfree(pDevice_p->TP_Interface_p);
#endif
		/* Decrement reference count to platform device */
		put_device(&pDevice_p->pdev->dev);
		platform_set_drvdata(pDevice_p->pdev, NULL);
	}

	return (Error);
}

/**
   @brief  Reset the Transport Process and Secure CoPro (if we are controlling it)

   This function restarts the TP and SC/SP.  This is not recommended, but can be useful in debug
   situations, and for potential workarounds.  It will reset all the pDevice.

   @param  pDeviceHandle  The handle for the pDevice to be reset

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceReset(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	STPTI_PRINTF("Request to Restart (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));

	/* Place the TP into reset if we have firmware loaded */
	if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != NULL) {
		if (pDevice_p->TP_Status != stptiHAL_STXP70_NO_FIRMWARE) {
			stptiHAL_pDeviceResetSTXP70(&pDevice_p->TP_MappedAddresses);
		}
		pDevice_p->TP_Status = stptiHAL_STXP70_STOPPED;

		/* Bombproofing... consume any stray TPAcks */
		while (!stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 0));
	}

	/* Restart the TP */
	if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != 0) {
		if (pDevice_p->TP_Status != stptiHAL_STXP70_NO_FIRMWARE) {
			stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->CheckSum, 0);

			/* setup Mailboxes for the TP */
			stptiHAL_pDeviceSetupTPMailboxes(pDeviceHandle);

			stptiHAL_pDeviceStartSTXP70(&pDevice_p->TP_MappedAddresses);

			/* wait for ack */
			if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
				STPTI_PRINTF("Unable to restart TP (device%d)",
					     stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
				Error = ST_ERROR_TIMEOUT;
			} else {
				pDevice_p->TP_Status = stptiHAL_STXP70_AWAITING_INITIALISATION;

				stptiHAL_pDeviceSetInitialisationParameters(pDevice_p->TP_Interface_p, pDevice_p->pdev);
				stptiHAL_pDeviceXP70BitClear(&pDevice_p->TP_Interface_p->FirmwareConfig, TP_FIRMWARE_RESET_SHARED_MEMORY);	/* IMPORTANT! don't reset the Shared Memory Interface */

				/* Indicate to TP that we have set the initialisation parameters */
				stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_SUPPLYING_INIT_PARAMS, 0);

				/* Wait for acknowledgement that initialisation is complete */
				if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
					STPTI_PRINTF_ERROR("Unable to reconfigure TP (device%d)",
							   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
					Error = ST_ERROR_TIMEOUT;
				} else {
					/* Check the TP's response. */
					stptiHAL_pDeviceXP70MemcpyFrom(&pDevice_p->TP_Interface,
								       pDevice_p->TP_Interface_p,
								       sizeof(stptiTP_Interface_t));

					if (pDevice_p->TP_Interface.CheckSum != SHARED_MEMORY_INTERFACE_CHECKSUM
					    && pDevice_p->TP_Interface.NumberOfvDevices == 0) {
						STPTI_PRINTF_ERROR("TP (device%d) failed to restart",
								   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
						Error = ST_ERROR_NO_MEMORY;
					} else {
						/* printout TP capabilities for the log */
						STPTI_PRINTF("TP (device%d) restarted successfully",
							     stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
						pDevice_p->TP_Status = stptiHAL_STXP70_RUNNING;
					}
					/* Translate the TP addresses to host addresses */
					{
						/* Normally I would cast these to U32's, but the ST40 compiler in its
						   strictest mode will not let you do this ("dereferencing type-punned
						   pointer will break strict-aliasing rules").  So we use U8 * and
						   cast in the loop. */
						U8 *DestinationStart = (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
						U8 *SourceStart = (U8 *) & (pDevice_p->TP_Interface.SharedMemory_p);
						U8 *SourceEnd = (U8 *) & (pDevice_p->TP_Interface.StatusBlk_p);

						/* Work througn from SharedMemory_p to StatusBlk_p adding the start of dDEM to
						   create a host address (TP addresses start from 0). */
						do {
							*((U32 *) DestinationStart) = (U32) & pDevice_p->TP_MappedAddresses.dDEM_p[*((U32 *) SourceStart)];
							DestinationStart += sizeof(U32);
							SourceStart += sizeof(U32);
						}
						while (SourceStart <= SourceEnd);
					}
				}
			}
		}
	}

	return (Error);
}

/**
   @brief  Powerdown the Transport Processor

   This function powers down/stops the TP from the sleep state. Depending on
   the requested state the TP shared memory is saved.

   @param  pDeviceHandle  The handle for the pDevice to be powered down
   @param  NewState       Requested power state (POWERDOWN or STOPPED)

   @return                A standard st error type...
                          - ST_ERROR_TIMEOUT        - unable to power down
                          - ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_pDevicePowerdown(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	struct stpti_tp_config *tp_config = pDevice_p->pdev->dev.platform_data;

	if (!tp_config->permit_powerdown) {
		/* If we cannot perform full powerdown, just disable clocks */
		return stptiHAL_pDeviceDisableClocks(pDeviceHandle);
	}

	if (pDevice_p->TP_Status == stptiHAL_STXP70_POWERDOWN && \
			NewState != stptiHAL_PDEVICE_POWERDOWN)
		stptiHAL_pDeviceEnableClocks(pDeviceHandle);

	/* 1. Reset the TP
	 * 2. Save the shared memory (if going to POWERDOWN state)
	 * 3. Disable closks
	 * 4. Update the TP state */
	stptiHAL_pDeviceResetSTXP70(&pDevice_p->TP_MappedAddresses);
	STPTI_PRINTF("TP held in reset (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));

	if (NewState == stptiHAL_PDEVICE_POWERDOWN) {
		STPTI_PRINTF("Saving TP shared memory (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));

		/* We need to save the shared memory here */
		if (pDevice_p->SharedMemoryBackup_p == NULL) {
			pDevice_p->SharedMemoryBackup_p =
			    kmalloc(pDevice_p->TP_Interface.SizeOfSharedMemoryRegion, GFP_KERNEL);
		}

		if (pDevice_p->SharedMemoryBackup_p == NULL) {
			STPTI_PRINTF_ERROR("Unable to save TP shared memory (device%d)",
					   stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
		} else {
			stptiHAL_pDeviceXP70MemcpyFrom((void *)pDevice_p->SharedMemoryBackup_p,
						       (const void *)pDevice_p->TP_Interface.SharedMemory_p,
						       pDevice_p->TP_Interface.SizeOfSharedMemoryRegion);
		}
		STPTI_PRINTF("TP powered down (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
		pDevice_p->TP_Status = stptiHAL_STXP70_POWERDOWN;
	} else {		/* Stop */

		STPTI_PRINTF("TP stopped (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
		pDevice_p->TP_Status = stptiHAL_STXP70_STOPPED;
	}

	/* Disable clocks */
	Error = stptiHAL_pDeviceDisableClocks(pDeviceHandle);

	return Error;
}

/**
   @brief  Wakeup the Transport Processor

   This function wakes up the TP from sleep state to running state

   @param  pDeviceHandle  The handle for the pDevice to be powered down

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_pDeviceWakeup(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	/* Enable clocks */
	Error = stptiHAL_pDeviceEnableClocks(pDeviceHandle);

	if (ST_NO_ERROR == Error) {
		/* Wakeup the TP */
#if defined( GNBvd50868_SLOW_RATE_STREAM_WA )
		stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_START_STFE_FLUSH, TRUE);
		stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_STOP_STFE_FLUSH, TRUE);
#endif
		stptiHAL_pDeviceStartSTXP70(&pDevice_p->TP_MappedAddresses);

		STPTI_PRINTF("TP woken up (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
		pDevice_p->TP_Status = stptiHAL_STXP70_RUNNING;
	}

	return Error;
}

/**
   @brief  Put the Transport Processor to sleep

   This function puts the TP into sleep state from running state

   @param  pDeviceHandle  The handle for the pDevice to be powered down

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_pDeviceSleep(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	struct stpti_tp_config *tp_config = pDevice_p->pdev->dev.platform_data;

#if defined( GNBvd50868_SLOW_RATE_STREAM_WA )
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_START_STFE_FLUSH, FALSE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_STOP_STFE_FLUSH, FALSE);
#endif

	/* Events are discarded in the event task when PERPARING_TO_SLEEP or
	 * SLEEPING
	 * Signals are discarded in SignalWaitBuffer (this is because it subject to
	 * application task priorities) */
	pDevice_p->TP_Status = stptiHAL_STXP70_PREPARING_TO_SLEEP;
	if (tp_config->permit_powerdown) {
		Error = stptiHAL_pDevicePowerdownSTXP70(&pDevice_p->TP_MappedAddresses);
	}

	if (ST_NO_ERROR == Error) {
		STPTI_PRINTF("TP sleeping (device%d)", stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
		pDevice_p->TP_Status = stptiHAL_STXP70_SLEEPING;

		/* Disable clocks */
		Error = stptiHAL_pDeviceDisableClocks(pDeviceHandle);
	}

	return Error;
}

/**
 * @brief Enables clocks for a given pDevice
 *
 * Should be called before the xp70 for the given pDevice is started
 *
 * @param  pDeviceHandle  The handle for the pDevice whose clocks should be
 *                        stopped
 *
 * @retval ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_pDeviceEnableClocks(FullHandle_t pDeviceHandle)
{
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	struct stpti_tp_config *dev_data = pDevice_p->pdev->dev.platform_data;
	int i;
	int err;

	if (pDevice_p->ClocksEnabled) {
		/* Clocks are already enabled for this pDevice. Don't do anything */
		return ST_NO_ERROR;
	}

	for (i = 0; i < dev_data->nb_clk; i++) {
		if (dev_data->clk[i].clk) {
			dev_info(&pDevice_p->pdev->dev, "Enabling clock %s\n", dev_data->clk[i].name);
			err = clk_prepare_enable(dev_data->clk[i].clk);
			if (err)
				pr_err("Unable to enable clock %s (%d)\n", dev_data->clk[i].name, err);

			if (dev_data->clk[i].parent_clk) {
				clk_set_parent(dev_data->clk[i].clk, dev_data->clk[i].parent_clk);
			}

			if (dev_data->clk[i].freq) {
				dev_info(&pDevice_p->pdev->dev, "Setting clock %s to %uHz\n",
					 dev_data->clk[i].name, dev_data->clk[i].freq);
				err = clk_set_rate(dev_data->clk[i].clk, dev_data->clk[i].freq);
				if (err)
					pr_err("Unable to set rate (%d)\n", err);
			}
			dev_info(&pDevice_p->pdev->dev, "Clock %s @ %luHz\n",
				 dev_data->clk[i].name, clk_get_rate(dev_data->clk[i].clk));
		} else {
			dev_warn(&pDevice_p->pdev->dev, "Clock %s unavailable\n", dev_data->clk[i].name);
		}
	}
	pDevice_p->ClocksEnabled = TRUE;
	return ST_NO_ERROR;
}

/**
 * @brief Disables clocks for a given pDevice
 *
 * Should be called after the xp70 for the given pDevice has been stopped
 *
 * @param  pDeviceHandle  The handle for the pDevice whose clocks should be
 *                        stopped
 *
 * @retval ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_pDeviceDisableClocks(FullHandle_t pDeviceHandle)
{
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	struct stpti_tp_config *dev_data = pDevice_p->pdev->dev.platform_data;
	int i;

	if (!pDevice_p->ClocksEnabled) {
		/* Clocks are already disabled for this pDevice. Don't do anything */
		return ST_NO_ERROR;
	}

	for (i = 0; i < dev_data->nb_clk; i++) {
		if (dev_data->clk[i].clk) {
			dev_info(&pDevice_p->pdev->dev, "Disabling clock %s\n", dev_data->clk[i].name);
			clk_disable_unprepare(dev_data->clk[i].clk);
		}
	}
	pDevice_p->ClocksEnabled = FALSE;
	return ST_NO_ERROR;
}

/**
   @brief  Configure a Live channel on this physical device

   This function sets up the specified live channel on the TP

   @param  pDeviceHandle  The handle for the pDevice to be examined
   @param  LiveParams_p   A pointer to a structure containing the params

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceConfigureLive(FullHandle_t pDeviceHandle,
					     stptiHAL_pDeviceConfigLiveParams_t *LiveParams_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	volatile stptiTP_Live_t *TP_Live_p;
	stptiTP_Live_t Live;

	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	/* MIND: previous version has a lock here but with the new design
	 * of STFE this function can be called in two different situation:
	 * 1- When setting the pid:  the call is safe because lock is already
	 *	 taken by caller of TSInput.TSHAL_TSInputSetClearPid
	 *	 therefore, getting the same lock again will cause a deadlock
	 * 2- when disabling STFE channel: no lock is acquired, therefore
	 * 	code gets it.
	 * 	MIND this call is not 100% secure, because you could execute it
	 * 	also when some other HAL calls are executed too
	 */
	stptiOBJMAN_TryWriteLock(pDeviceHandle, &LocalLockState);

	TP_Live_p = (volatile stptiTP_Live_t *)pDevice_p->TP_Interface.Live_p;
	if (stptiHAL_STXP70_RUNNING == pDevice_p->TP_Status) {

		Live.channel = LiveParams_p->Channel;
		Live.buffer_base = LiveParams_p->Base;
		Live.buffer_size_in_pkts = LiveParams_p->SizeInPkts;
		Live.packet_len = LiveParams_p->PktLength;

		stpti_printf("pdevice %x Base %x SizeInPkts %u PktLength %u Channel %u", pDeviceHandle.word,
				 Live.buffer_base, Live.buffer_size_in_pkts, Live.packet_len,
				 LiveParams_p->Channel);

		stptiHAL_pDeviceXP70MemcpyTo((void *)TP_Live_p, &Live, sizeof(stptiTP_Live_t));

		if (Live.buffer_base != 0) {
			STPTI_PRINTF("Starting STFE Live channel %u on TP (device%d)", LiveParams_p->Channel,
					 stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
			stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_START_LIVE, 0);
		} else {
			/* Disable the tsinput channel instead */
			STPTI_PRINTF("Stopping STFE Live channel %u on TP (device%d)", LiveParams_p->Channel,
					 stptiOBJMAN_GetpDeviceIndex(pDeviceHandle));
			stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_STOP_LIVE, 0);
		}

		/* Wait for acknowledgement that Live initialisation is complete */
		if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
			STPTI_PRINTF_ERROR("Timed out waiting for Ack from TP");
			Error = ST_ERROR_TIMEOUT;
		}

	} else {
		Error = ST_ERROR_SUSPENDED;
	}

	if (LocalLockState == stptiSupport_WRITELOCKED)
		stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	return Error;
}

ST_ErrorCode_t stptiHAL_pDeviceEnableSWLeakyPID(FullHandle_t pDeviceHandle, unsigned int LiveChannel,
						BOOL StreamIsActive)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
	{
		stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

		volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;

		if (StreamIsActive) {
			stptiHAL_pDeviceXP70BitSet(&pDeviceInfo_p->SWLeakyPIDChannels, 1 << LiveChannel);
		} else {
			stptiHAL_pDeviceXP70BitClear(&pDeviceInfo_p->SWLeakyPIDChannels, 1 << LiveChannel);
		}
	}
	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	return (Error);
}

ST_ErrorCode_t stptiHAL_pDeviceSWLeakyPIDAvailable(FullHandle_t pDeviceHandle, BOOL *IsAvailable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	if (NULL != pDevice_p) {
		*IsAvailable = pDevice_p->SWLeakyPIDAvailable;
	} else {
		Error = ST_ERROR_BAD_PARAMETER;
	}

	return Error;
}

/**
   @brief  Get capabilities of the (physical) device

   This function returns the capabilities of the pdevice.

   @param  pDeviceHandle            The handle for the pDevice to be examined
   @param  ConfigStatus_p           (return) A pointer to where to put the capabilities

   @return                          A standard st error type...
					- ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceGetCapability(FullHandle_t pDeviceHandle,
						stptiHAL_pDeviceConfigStatus_t *ConfigStatus_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);

	ConfigStatus_p->NumbervDevice = pDevice_p->TP_Interface.NumberOfvDevices;
	ConfigStatus_p->NumberSlots = pDevice_p->TP_Interface.NumberOfSlots;
	ConfigStatus_p->NumberDMAs = pDevice_p->TP_Interface.NumberOfDMAStructures;
	ConfigStatus_p->NumberHWSectionFilters = pDevice_p->TP_Interface.NumberOfSectionFilters;
	ConfigStatus_p->NumberIndexer = pDevice_p->TP_Interface.NumberOfIndexers;
	ConfigStatus_p->NumberStatusBlk = pDevice_p->TP_Interface.NumberOfStatusBlks;

	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	return ST_NO_ERROR;
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Set the Initialisation Parameters for starting the Transport Processor

   This function sets all the parameters for configuring the Transport Processor.

   @param  TP_Interface_p  Pointer to the TP Interface structure.
   @param  tp_config       Pointer to TP device configuration struct

   @return                 A standard st error type...
                           - ST_NO_ERROR
 */
static void stptiHAL_pDeviceSetInitialisationParameters(stptiTP_Interface_t *TP_Interface_p,
							struct platform_device *pdev)
{
	struct stpti_tp_config *tp_config = pdev->dev.platform_data;
	U32 NumberOfSlots = tp_config->nb_slot;

	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfvDevices, tp_config->nb_vdevice);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfSlots, NumberOfSlots);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfPids, NumberOfSlots + NumberOfSlots / 2);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfSectionFilters, tp_config->nb_section_filter);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfDMAStructures, tp_config->nb_dma_structure);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfIndexers, tp_config->nb_indexer);
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfStatusBlks, tp_config->nb_status_blk);

	if (tp_config->nb_vdevice > MAX_NUMBER_OF_VDEVICES) {
		STPTI_PRINTF_ERROR("Exceeding maximum number permitted for vDevices, capping number to %d",
				   MAX_NUMBER_OF_VDEVICES);
		stptiHAL_pDeviceXP70Write(&TP_Interface_p->NumberOfvDevices, MAX_NUMBER_OF_VDEVICES);
	}

	/* Setup the facilities permitted for the firmware */
	stptiHAL_pDeviceXP70Write(&TP_Interface_p->FirmwareConfig, TP_FIRMWARE_RESET_SHARED_MEMORY);

	if (tp_config->software_leaky_pid) {
		stptiHAL_pDeviceXP70Write(&TP_Interface_p->SlowRateStreamTimeout,
					  tp_config->software_leaky_pid_timeout);
	} else {
		stptiHAL_pDeviceXP70Write(&TP_Interface_p->SlowRateStreamTimeout, 0xFFFFFFFF);
	}

	if (tp_config->permit_powerdown) {
		stptiHAL_pDeviceXP70BitSet(&TP_Interface_p->FirmwareConfig, TP_FIRMWARE_ALLOW_POWERDOWN);
	}

	stptiHAL_pDeviceXP70BitSet(&TP_Interface_p->FirmwareConfig,
				   TP_FIRMWARE_USE_TIMER_COUNTER | (tp_config->timer_counter_divider & TP_FIRMWARE_TIMER_COUNTER_MASK));

	if (tp_config->sc_bypass) {
		/* SC/SP bypassed */
		STPTI_PRINTF("Bypassing Secure CoProcessor.");
		stptiHAL_pDeviceXP70BitSet(&TP_Interface_p->FirmwareConfig, TP_FIRMWARE_BYPASS_SECURE_COPRO);
	} else {
		/* SC controlled by TKD driver */
		STPTI_PRINTF("Assuming STTKD driver in system.");
	}
}

/**
   @brief  Sets up the Mailboxes for the Transport Processor

   This function sets up the Mailboxes for the Transport Processor.

   @param  pDeviceHandle   The handle of the physical device.

   @return                 A standard st error type...
                           - ST_NO_ERROR
 */
void stptiHAL_pDeviceSetupTPMailboxes(FullHandle_t pDeviceHandle)
{
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_ALL_INCOMING_TP_ITEMS, FALSE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_IS_TP_PRINTF_WAITING, TRUE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_ACKNOWLEDGE, TRUE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION, TRUE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_HAS_A_BUFFER_SIGNALLED, TRUE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_CHECK_FOR_STATUS_BLOCK, TRUE);

#if defined( GNBvd50868_SLOW_RATE_STREAM_WA )
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_START_STFE_FLUSH, TRUE);
	stptiHAL_MailboxInterruptCtrl(pDeviceHandle, MAILBOX_TP_STOP_STFE_FLUSH, TRUE);
#endif
}

/**
   @brief  Loads the firmware into the STxP70's memories.

   This function loads firmare into the STxP70's memories based on elf_sections passed into it.
   The elf sections are declared in from firmware header files, and instantiated elsewhere.

   @param  stxp70_p     The virtual base address of the STxP70 device.

   @return              Nothing

 */
static ST_ErrorCode_t stptiHAL_pDeviceLoadSTXP70(stptiHAL_TangoDevice_t *stxp70_p, struct platform_device *pdev)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	struct stpti_tp_config *tp_config = pdev->dev.platform_data;
	ST_ErrorCode_t err = ST_NO_ERROR;
	int i;

	BUG_ON(STPTI_Driver.cachedTPFW == NULL);

	dev_info(&pdev->dev, "Loading firmware: %s\n", tp_config->firmware);

	/* Check ELF magic */
	ehdr = (Elf32_Ehdr *) STPTI_Driver.cachedTPFW->data;
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
		dev_err(&pdev->dev, "Invalid ELF magic\n");
		err = ST_ERROR_UNKNOWN_DEVICE;
	}

	/* Check machine type is STxP7x (0xa6) and type is correct */
	if (ST_NO_ERROR == err && (0xA6 != ehdr->e_machine || ET_EXEC != ehdr->e_type)) {
		dev_err(&pdev->dev, "Unexpected ELF header: machine:0x%x, type:0x%x\n", ehdr->e_machine, ehdr->e_type);
		err = ST_ERROR_UNKNOWN_DEVICE;
	}

	/* Check program headers are within firmware size */
	if (ehdr->e_phoff + (ehdr->e_phnum * sizeof(Elf32_Phdr)) > STPTI_Driver.cachedTPFW->size) {
		dev_err(&pdev->dev, "Program headers outside of firmware file\n");
		err = ST_ERROR_UNKNOWN_DEVICE;
	}

	phdr = (Elf32_Phdr *) (STPTI_Driver.cachedTPFW->data + ehdr->e_phoff);
	for (i = 0; i < ehdr->e_phnum && ST_NO_ERROR == err; i++) {
		/* Only consider LOAD segments */
		if (PT_LOAD == phdr->p_type) {
			volatile U8 *dest;

			/* Check segment is contained within the fw->data buffer */
			if (phdr->p_offset + phdr->p_filesz > STPTI_Driver.cachedTPFW->size) {
				dev_err(&pdev->dev, "Segment %d is outside of firmware file\n", i);
				err = ST_ERROR_BAD_PARAMETER;
				break;
			}

			/* in xp70 addressing... xp70's iDEM is at 0x00400000, dDEM at
			 * 0x00000000 */
			if (phdr->p_paddr < 0x00400000) {
				dest = stxp70_p->dDEM_p + (phdr->p_paddr & 0xFFFFF); /* dDEM */
			} else {
				dest = stxp70_p->iDEM_p + (phdr->p_paddr & 0xFFFFF); /* iDEM */
			}

			/* Copy the segment data from the ELF file and pad segment with
			 * zeroes */
			dev_info(&pdev->dev, "Loading segment %d 0x%08x (0x%x bytes) -> 0x%08x (0x%x bytes)\n",
				 i, phdr->p_paddr, phdr->p_filesz, (U32)dest, phdr->p_memsz);
			stptiHAL_pDeviceXP70Memset((U8 *)dest, 0, phdr->p_filesz);
			stptiHAL_pDeviceXP70MemcpyTo((U8 *)dest, STPTI_Driver.cachedTPFW->data + phdr->p_offset, phdr->p_filesz);

			memset((U8 *)dest + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
		}
		phdr++;
	}

	return err;
}

/**
   @brief  Reset and starts a STxP70 (PNX) micro running.

   This function resets and starts the STxP70 running.  The STxP70 should never be stopped
   or reset.  Once started they should be left running, even if all the vDevices are terminated.

   @param  stxp70_p     The virtual base address of the STxP70 device.

   @return              Nothing

 */
static void stptiHAL_pDeviceResetSTXP70(stptiHAL_TangoDevice_t *stxp70_p)
{
	if (stxp70_p != NULL) {
		ST_ErrorCode_t Error = ST_NO_ERROR;

		/* disable instruction fetching */
		stptiSupport_WriteReg32(&stxp70_p->CoreCtrl_p->CoreRunConfig, 0x0);

		/* Reset the core and wait for acknowledgement */
		stptiSupport_WriteReg32(&stxp70_p->CoreCtrl_p->ResetCtrl, 0x1);

#if !defined(CONFIG_STM_VIRTUAL_PLATFORM)	/* FIXME  - VSOC tango model not yet ready */
		stptiSupport_SubMilliSecondWait((stptiSupport_ReadReg32(&stxp70_p->CoreCtrl_p->ResetStatus) & 0x1) != 0x1, &Error);
		if (ST_ERROR_TIMEOUT == Error)
			STPTI_PRINTF("STXP70 reset failed ST_ERROR_TIMEOUT!");
#endif

		/* Set the T3 address range registers (if applicable)
		 * Note: we assume two LMIs with a boundary at 0x80000000 */
		if (stxp70_p->T3AddrFilter_p) {
			stxp70_p->T3AddrFilter_p->AddrRangeBase[0] = 0x80000000;
			stxp70_p->T3AddrFilter_p->AddrRangeBase[1] = 0x80000000;
			stxp70_p->T3AddrFilter_p->AddrRangeBase[2] = 0x80000000;
		}
#if defined( TANGOFPGA )
		/* For the FPGA Platform we override the plug settings */
		stxp70_p->STBusPlugRead_p->PageSize = 0x1;	/* 0 = 64bytes, 1 = 128bytes, 2 = 256bytes, 3 = 512bytes, 4 = 1024bytes ... */
		stxp70_p->STBusPlugRead_p->MinOpSize = 0x3;	/* 0 = 1bytes, 1 = 2bytes, 2 = 4bytes, ... */
		stxp70_p->STBusPlugRead_p->MaxOpSize = 0x5;	/* 0 = 1bytes, 1 = 2bytes, 2 = 4bytes, 3 = 8bytes, 4 = 16bytes, 5 = 32bytes, ... */
		stxp70_p->STBusPlugRead_p->MaxChunkSize = 0x1;	/* 2^N = maxop_size */
		stxp70_p->STBusPlugRead_p->MaxMsgSize = 0;	/* 2^N = maxchunk_size - was 2, but must be zero as STBus Messaging disabled on Freeman */

		stxp70_p->STBusPlugWrite_p->PageSize = 0x1;	/* 0 = 64bytes, 1 = 128bytes, 2 = 256bytes, 3 = 512bytes, 4 = 1024bytes ... */
		stxp70_p->STBusPlugWrite_p->MinOpSize = 0x3;	/* 0 = 1bytes, 1 = 2bytes, 2 = 4bytes, ... */
		stxp70_p->STBusPlugWrite_p->MaxOpSize = 0x5;	/* 0 = 1bytes, 1 = 2bytes, 2 = 4bytes, 3 = 8bytes, 4 = 16bytes, 5 = 32bytes, ... */
		stxp70_p->STBusPlugWrite_p->MaxChunkSize = 0x1;	/* 2^N = maxop_size */
		stxp70_p->STBusPlugWrite_p->MaxMsgSize = 0;	/* 2^N = maxchunk_size - was 2, but must be zero as STBus Messaging disabled on Freeman */
#endif
	}
}

/**
   @brief  Reset and starts a STxP70 (PNX) micro running.

   This function resets and starts the STxP70 running.  The STxP70 should never be stopped
   or reset.  Once started they should be left running, even if all the vDevices are terminated.

   @param  stxp70_p     The virtual base address of the STxP70 device.

   @return              Nothing

 */
static void stptiHAL_pDeviceStartSTXP70(stptiHAL_TangoDevice_t *stxp70_p)
{
	if (stxp70_p != NULL) {
		/* Start core running by waking it up and preventing powerdown */
		stptiSupport_WriteReg32(&stxp70_p->CoreCtrl_p->PowerdownConfig, 0x2);

		/* And then enabling instruction fetching */
		stptiSupport_WriteReg32(&stxp70_p->CoreCtrl_p->CoreRunConfig, 0x1);
	}
}

/**
   @brief  Powerdown an STxP70 (PNX) micro.

   This function permits the xp70 to drop into power down.  Use stptiHAL_pDeviceStartSTXP70() to
   wake it up.

   Before calling this function you must make sure that the xp70 is not performing any streamer
   operations, by disabling the pids (both STFE and internal TP PID filter) and by doing a
   DMA_WAIT operation on all buffers.

   @param  stxp70_p     The virtual base address of the STxP70 device.

   @return              Nothing

 */
static ST_ErrorCode_t stptiHAL_pDevicePowerdownSTXP70(stptiHAL_TangoDevice_t *stxp70_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	/* Powerdown is desirable for a clean shutdown.  Some systems such as the simulator don't
	   have xp70 facilities for it, and so we skip over this, and end up brutally terminating it. */

	if (stxp70_p != NULL) {
		/* Allow the core to powerdown (powered down when the XP70 reaches the IDLE instruction) */
		stptiSupport_WriteReg32(&stxp70_p->CoreCtrl_p->PowerdownConfig, 0x1);

#if !defined(CONFIG_STM_VIRTUAL_PLATFORM)	/* FIXME  - VSOC tango model not yet ready */
		/* Wait for xp70 to actually power down */
		stptiSupport_SubMilliSecondWait((stptiSupport_ReadReg32(&stxp70_p->CoreCtrl_p->PowerdownStatus) & 0x1e)
						== 0, &Error);
#endif
	}

	return (Error);
}

/**
   @brief Puts the pDevice into the requested power state - internal lockless
          API

   @param  pDeviceHandle  The handle of the pDevice whose power state to change
   @param  NewState       The requested power state

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceSetPowerStateInternal(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	switch (NewState) {
	case stptiHAL_PDEVICE_STARTED:
		switch (pDevice_p->TP_Status) {
		case stptiHAL_STXP70_RUNNING:
			/* Already running - do nothing */
			break;
		case stptiHAL_STXP70_SLEEPING:
			Error = stptiHAL_pDeviceWakeup(pDeviceHandle);
			break;
		default:
			STPTI_PRINTF_ERROR("Cannot start pDevice (xp70 state=%d)", pDevice_p->TP_Status);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		}
		break;

	case stptiHAL_PDEVICE_SLEEPING:
		switch (pDevice_p->TP_Status) {
		case stptiHAL_STXP70_SLEEPING:
			/* Already sleeping - do nothing */
			break;
		case stptiHAL_STXP70_POWERDOWN:
			/* Need to power-up first before sleeping */
			Error = stptiHAL_pDevicePowerUp(pDeviceHandle);
			if (ST_NO_ERROR == Error) {
				Error = stptiHAL_pDeviceSleep(pDeviceHandle);
			}
			break;
		case stptiHAL_STXP70_RUNNING:
			Error = stptiHAL_pDeviceSleep(pDeviceHandle);
			break;
		default:
			STPTI_PRINTF_ERROR("Cannot sleep pDevice (current xp70 state=%d)", pDevice_p->TP_Status);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		}
		break;

	case stptiHAL_PDEVICE_POWERDOWN:
	case stptiHAL_PDEVICE_STOPPED:
		switch (pDevice_p->TP_Status) {
		case stptiHAL_STXP70_POWERDOWN:
		case stptiHAL_STXP70_STOPPED:
			break;
		case stptiHAL_STXP70_SLEEPING:
			/* Need to wake-up first before power-down */
			Error = stptiHAL_pDeviceWakeup(pDeviceHandle);
			if (ST_NO_ERROR == Error) {
				Error = stptiHAL_pDevicePowerdown(pDeviceHandle, NewState);
			}
			break;
		default:
			STPTI_PRINTF_ERROR("Cannot power-down pDevice (current xp70 state=%d)", pDevice_p->TP_Status);
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
			break;
		}
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return Error;
}

/**
   @brief Puts the pDevice into the requested power state

   @param  pDeviceHandle  The handle of the pDevice whose power state to change
   @param  NewState       The requested power state

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceSetPowerState(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	BOOL WakeUp = FALSE;

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);

	if (stptiHAL_STXP70_SLEEPING == pDevice_p->TP_Status && stptiHAL_PDEVICE_STARTED == NewState) {
		WakeUp = TRUE;
	}

	Error = stptiHAL_pDeviceSetPowerStateInternal(pDeviceHandle, NewState);

	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	/* If we have woken-up from sleep state notify the TSHAL that this pDevice
	 * is now running
	 * NOTE: This must be done _outside_ of the pDevice lock since
	 * TSHAL_TSInputNotifyPDevicePowerState may call the Tango HAL API */
	if (ST_NO_ERROR == Error && WakeUp) {
		Error =
		    stptiTSHAL_call(TSInput.TSHAL_TSInputNotifyPDevicePowerState, 0,
				    stptiOBJMAN_GetpDeviceIndex(pDeviceHandle), TRUE);
	}

	return Error;
}

ST_ErrorCode_t stptiHAL_pDeviceGetPlatformDevice(FullHandle_t pDeviceHandle, struct platform_device **pdev)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	if (!pDevice_p)
		return ST_ERROR_UNKNOWN_DEVICE;

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
	*pdev = pDevice_p->pdev;
	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	return ST_NO_ERROR;
}
/**
   @brief  Safe Memcpy for xp70 memory (copying to XP70 from DDR)

   This function implements a memcpy but guarantees that nothing higher than an U32 bus opcode
   is used.  It also guarantees that the memory accesses are volatile.

   @param  d  Destination pointer
   @param  s  Source pointer
   @param  n  Number of bytes to copy

   @return destination pointer

 */
void *stptiHAL_pDeviceXP70MemcpyTo(void *d, const void *s, size_t n)
{
	U32 alignment = ((U32) d | (U32) s);
	volatile U8 *src_p = (U8 *) s, *dest_p = (U8 *) d;

	if ((alignment & 3) == 0) {	/* u32 aligned copy */
		unsigned const copysize = sizeof(U32);
		while (n >= copysize) {
			stptiSupport_WriteReg32((U32 *) dest_p, *((U32 *) src_p));
			dest_p += copysize;
			src_p += copysize;
			n -= copysize;
		}
	} else if ((alignment & 1) == 0) {	/* u16 aligned copy */
		unsigned const copysize = sizeof(U16);
		while (n >= copysize) {
			stptiSupport_WriteReg16((U16 *) dest_p, *((U16 *) src_p));
			dest_p += copysize;
			src_p += copysize;
			n -= copysize;
		}
	}

	/* u8 aligned copy (for the remainder) */
	while ((n--) > 0) {
		stptiSupport_WriteReg8(dest_p++, *(src_p++));
	}

	return (d);
}

/**
   @brief  Safe Memcpy for xp70 memory (copying from XP70 to DDR)

   This function implements a memcpy but guarantees that nothing higher than an U32 bus opcode
   is used.  It also guarantees that the memory accesses are volatile.

   @param  d  Destination pointer
   @param  s  Source pointer
   @param  n  Number of bytes to copy

   @return destination pointer

 */
void *stptiHAL_pDeviceXP70MemcpyFrom(void *d, const void *s, size_t n)
{
	U32 alignment = ((U32) d | (U32) s);
	volatile U8 *src_p = (U8 *) s, *dest_p = (U8 *) d;

	if ((alignment & 3) == 0) {	/* u32 aligned copy */
		unsigned const copysize = sizeof(U32);
		while (n >= copysize) {
			*((U32 *) dest_p) = stptiSupport_ReadReg32((U32 *) src_p);
			dest_p += copysize;
			src_p += copysize;
			n -= copysize;
		}
	} else if ((alignment & 1) == 0) {	/* u16 aligned copy */
		unsigned const copysize = sizeof(U16);
		while (n >= copysize) {
			*((U16 *) dest_p) = stptiSupport_ReadReg16((U16 *) src_p);
			dest_p += copysize;
			src_p += copysize;
			n -= copysize;
		}
	}

	/* u8 aligned copy (for the remainder) */
	while ((n--) > 0) {
		*(dest_p++) = stptiSupport_ReadReg8(src_p++);
	}

	return (d);
}

/**
   @brief  Safe Memset for xp70 memory

   This function implements a memset but guarantees that nothing higher than an U32 bus opcode
   is used.  It also guarantees that the memory accesses are volatile.

   @param  d  Destination pointer
   @param  c  value (converted to U8) to be written to memory
   @param  n  Number of bytes to write

   @return destination pointer

 */
void *stptiHAL_pDeviceXP70Memset(void *d, int c, size_t n)
{
	U32 alignment = (U32) d;
	volatile U8 *dest_p = (U8 *) d;
	U32 value = (U32) c & 0xFF;

	value |= (c << 24) | (c << 16) | (c << 8);

	if ((alignment & 3) == 0) {	/* u32 aligned copy */
		unsigned const copysize = sizeof(U32);
		while (n >= copysize) {
			stptiSupport_WriteReg32((U32 *) dest_p, (U32) value);
			dest_p += copysize;
			n -= copysize;
		}
	} else if ((alignment & 1) == 0) {	/* u16 aligned copy */
		unsigned const copysize = sizeof(U16);
		while (n >= copysize) {
			stptiSupport_WriteReg16((U16 *) dest_p, (U16) value);
			dest_p += copysize;
			n -= copysize;
		}
	}

	/* u8 aligned copy (for the remainder) */
	while ((n--) > 0) {
		stptiSupport_WriteReg8(dest_p++, (U8) value);
	}

	return (d);
}

/**
   @brief  defragment the pdevice's vdevice resoure pid partitions.

   This function reorganises pid table partitions removing any gaps in the
   partitions.

   @param  pDeviceHandle  physicall device handle.

   @return none
 */
void stptiHAL_pDeviceDefragmentPidTablePartitions(FullHandle_t pDeviceHandle)
{
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	/* OK sort all the partitions so there are no gaps */
	stptiSupport_PartitionedResourceCompact(&pDevice_p->PIDTablePartitioning);
}

/**
   @brief  Compact the pdevice's vdevice resoure pid partitions.

   This function reorganises and compacts the pid table partitions but
   will not actually rewrite the pid tables in TP DMEM.
   Any vdevice with an invalid partition will be ignored. i.e if freeing
   a partition before the call of this function.

   @param  pDeviceHandle  physicall device handle.

   @return none
 */
void stptiHAL_pDeviceCompactPidTablePartitions(FullHandle_t pDeviceHandle)
{
	FullHandle_t vDeviceHandles[MAX_NUMBER_OF_VDEVICES];
	U32 vDeviceCount = 0;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	int Count;

	vDeviceCount = stptiOBJMAN_ReturnChildObjects(pDeviceHandle,
						      vDeviceHandles, MAX_NUMBER_OF_VDEVICES, OBJECT_TYPE_VDEVICE);

	for (Count = 0; Count < vDeviceCount; Count++) {
		if (!stptiOBJMAN_IsNullHandle(vDeviceHandles[Count])) {
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandles[Count]);
			U16 number_current_pids;

			/* How many pids are we using on theis vdeivce ? */
			number_current_pids = stptiOBJMAN_GetNumberOfItemsInList(&(vDevice_p->PidIndexes));

			if (number_current_pids < vDevice_p->TP_Interface.NumberOfSlots) {
				if (number_current_pids == 0) {
					number_current_pids++;	/* Make sure we don't allocate a list partition of zero */
				}
				/* Resize the partition */
				if (!(stptiSupport_PartitionedResourceResizePartition(&pDevice_p->PIDTablePartitioning,
										      vDevice_p->PIDTablePartitionID,
										      number_current_pids))) {
					vDevice_p->TP_Interface.NumberOfSlots = number_current_pids;
				}
				/* Ignore if any failures as we will have free a vdevice partition just before this call */
			}
		}
	}
	/* OK compact all the partitions to remove any gaps */
	stptiSupport_PartitionedResourceCompact(&pDevice_p->PIDTablePartitioning);
}

/**
   @brief  Update the pdevice's entire pid table.

   This function rebuilds a new copy of the pid slot tables
   and informs the TP for update via streamer DMA engine.

   @param  pDeviceHandle  physicall device handle.

   @return none
 */
void stptiHAL_pDeviceUpdatePidTable(FullHandle_t pDeviceHandle)
{
	U16 *NewPIDTable_p;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	stptiSupport_DMAMemoryStructure_t CachedMemoryStructure;
	volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;
	unsigned int total_size = (sizeof(uint16_t) * ((pDevice_p->TP_Interface.NumberOfPids) * 2))
	    + (sizeof(uint16_t) * ((pDevice_p->TP_Interface.NumberOfvDevices) * 2));

	/* Walk through the vdevices and compute the addresses for the pid table/slot mapping table */

	stpti_printf("Updating pid tables on device 0x%x!\n", pDeviceHandle.word);

	/* Allocate a block of memory for DMA the size of the pid tables and slot mapping tables */
	CachedMemoryStructure.Dev = &pDevice_p->pdev->dev;
	NewPIDTable_p = stptiSupport_MemoryAllocateForDMA(total_size,
							  STPTI_SUPPORT_DCACHE_LINE_SIZE,
							  &CachedMemoryStructure, stptiSupport_ZONE_SMALL);
	if (NULL != NewPIDTable_p) {
		FullHandle_t vDeviceHandles[MAX_NUMBER_OF_VDEVICES];
		U32 vDeviceCount = 0;
		int Count;
		U16 *vDeviceData = &NewPIDTable_p[(pDevice_p->TP_Interface.NumberOfPids) * 2];

		stpti_printf("Initialising Pid Table @0x%x Slot Mapping Table @0x%x entries %u\n",
			     (unsigned int)NewPIDTable_p,
			     (unsigned int)&(NewPIDTable_p[pDevice_p->TP_Interface.NumberOfPids]),
			     pDevice_p->TP_Interface.NumberOfPids);
		stpti_printf("vDevice data @0x%x\n", (unsigned int)vDeviceData);
		/* Initialise the temp pid table */
		stptiHAL_SlotPIDTableInitialise((volatile U16 *)NewPIDTable_p,
						(volatile U16 *)&(NewPIDTable_p[pDevice_p->TP_Interface.NumberOfPids]),
						pDevice_p->TP_Interface.NumberOfPids);

		vDeviceCount = stptiOBJMAN_ReturnChildObjects(pDeviceHandle,
							      vDeviceHandles,
							      MAX_NUMBER_OF_VDEVICES, OBJECT_TYPE_VDEVICE);

		stpti_printf("vDeviceCount %u\n", vDeviceCount);

		memset(vDeviceData, 0x00, (sizeof(uint16_t) * ((pDevice_p->TP_Interface.NumberOfvDevices) * 2)));

#ifdef DETAILED_PID_DEBUG
		for (Count = 0; Count < pDevice_p->TP_Interface.NumberOfvDevices; Count++) {
			stpti_printf("vDevice %u Pid Table Offset  : %u\n",
				     Count, pDevice_p->TP_Interface.vDeviceInfo_p[Count].pid_filter_base);
			stpti_printf("vDevice %u Pid Table Entries : %u\n",
				     Count, pDevice_p->TP_Interface.vDeviceInfo_p[Count].pid_filter_size);
		}
		for (Count = 0; Count < pDevice_p->TP_Interface.NumberOfvDevices; Count++) {
			stpti_printf("Checking vDevice %u New Pid Table Offset  : %u\n", Count, vDeviceData[Count]);
			stpti_printf("Checking vDevice %u New Pid Table Entries : %u\n",
				     Count, vDeviceData[Count + pDevice_p->TP_Interface.NumberOfvDevices]);
		}
#endif
		for (Count = 0; Count < vDeviceCount; Count++) {
			if (!stptiOBJMAN_IsNullHandle(vDeviceHandles[Count])) {
				stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandles[Count]);
				U16 *PIDTable_p = NULL;
				U16 *PIDSlotMappingTable_p = NULL;
				int PidIndex;
				stptiHAL_Slot_t *Slot_p = NULL;

				/* Build the table contents */
				PIDTable_p = NewPIDTable_p;
				PIDTable_p += stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
										    vDevice_p->PIDTablePartitionID);
				PIDSlotMappingTable_p = &NewPIDTable_p[pDevice_p->TP_Interface.NumberOfPids];
				PIDSlotMappingTable_p +=
				    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
									  vDevice_p->PIDTablePartitionID);

				/* Loop for pids indexes */
				stpti_printf("vDevice 0x%x\n", vDeviceHandles[Count].word);
				stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &PidIndex);
				while (PidIndex >= 0) {
					if (Slot_p->PID != stptiHAL_InvalidPID) {
						PIDTable_p[Slot_p->PidIndex] = Slot_p->PID;
						PIDSlotMappingTable_p[Slot_p->PidIndex] = Slot_p->SlotIndex;
						stpti_printf
						    ("Building PID table - PidTable[%03u]=0x%04x SlotMappingTable[%03u]=%03u\n",
						     Slot_p->PidIndex, Slot_p->PID, Slot_p->PidIndex,
						     Slot_p->SlotIndex);
					}
					stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &PidIndex);
				}
#ifdef DETAILED_PID_DEBUG
				stpti_printf("Old Pid Table     : 0x%x\n",
					     (unsigned int)vDevice_p->TP_Interface.PIDTable_p);
				stpti_printf("Old Slot Map Table: 0x%x\n",
					     (unsigned int)vDevice_p->TP_Interface.PIDSlotMappingTable_p);
#endif
				/* Update the actual pid table addresses in the vdevices */
				vDevice_p->TP_Interface.PIDTable_p = pDevice_p->TP_Interface.PIDTable_p;
				vDevice_p->TP_Interface.PIDTable_p +=
				    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
									  vDevice_p->PIDTablePartitionID);
				vDevice_p->TP_Interface.PIDSlotMappingTable_p =
				    pDevice_p->TP_Interface.PIDSlotMappingTable_p;
				vDevice_p->TP_Interface.PIDSlotMappingTable_p +=
				    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
									  vDevice_p->PIDTablePartitionID);
#ifdef DETAILED_PID_DEBUG
				stpti_printf("New Pid Table     : 0x%x\n",
					     (unsigned int)vDevice_p->TP_Interface.PIDTable_p);
				stpti_printf("New Slot Map Table: 0x%x\n",
					     (unsigned int)vDevice_p->TP_Interface.PIDSlotMappingTable_p);
#endif
				vDeviceData[Count] =
				    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
									  vDevice_p->PIDTablePartitionID);
				vDeviceData[Count + pDevice_p->TP_Interface.NumberOfvDevices] =
				    stptiSupport_PartitionedResourceSize(&pDevice_p->PIDTablePartitioning,
									 vDevice_p->PIDTablePartitionID);
#ifdef DETAILED_PID_DEBUG
				stpti_printf("Old Pid Table Offset  : %u\n",
					     vDevice_p->TP_Interface.vDeviceInfo_p->pid_filter_base);
				stpti_printf("Old Pid Table Entries : %u\n",
					     vDevice_p->TP_Interface.vDeviceInfo_p->pid_filter_size);
				stpti_printf("New Pid Table Offset  : %u\n", vDeviceData[Count]);
				stpti_printf("New Pid Table Entries : %u\n",
					     vDeviceData[Count + pDevice_p->TP_Interface.NumberOfvDevices]);
#endif
			}
		}
#ifdef DETAILED_PID_DEBUG
		for (Count = 0; Count < pDevice_p->TP_Interface.NumberOfvDevices; Count++) {
			stpti_printf("vDevice %u New Pid Table Offset  : %u\n", Count, vDeviceData[Count]);
			stpti_printf("vDevice %u New Pid Table Entries : %u\n",
				     Count, vDeviceData[Count + pDevice_p->TP_Interface.NumberOfvDevices]);
		}
#endif
		pDeviceInfo_p->pid_table_base = (unsigned int)stptiSupport_VirtToPhys(&CachedMemoryStructure,
										      NewPIDTable_p);
		stpti_printf("Physical address of new pid table 0x%08x\n", pDeviceInfo_p->pid_table_base);

		stptiSupport_FlushRegion(&CachedMemoryStructure, NewPIDTable_p, total_size);

		stptiHAL_MailboxPost(pDeviceHandle, MAILBOX_UPDATING_NEW_PID_TABLE, 0);

		if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
			panic("Transport processor failed to respond to pid table update!");
		} else {
			stpti_printf("Successfully signalled the TP with the new pid table!\n\n");
#ifdef DETAILED_PID_DEBUG
			for (Count = 0; Count < pDevice_p->TP_Interface.NumberOfPids; Count++) {
				stpti_printf("PID [%03u] = 0x%04x SLOT[%03u] = 0x%04x\n", Count,
					     pDevice_p->TP_Interface.PIDTable_p[Count],
					     Count, pDevice_p->TP_Interface.PIDSlotMappingTable_p[Count]);
			}
			for (Count = 0; Count < pDevice_p->TP_Interface.NumberOfvDevices; Count++) {
				stpti_printf("vDevice %u Pid Table Offset  : %u\n",
					     Count, pDevice_p->TP_Interface.vDeviceInfo_p[Count].pid_filter_base);
				stpti_printf("vDevice %u Pid Table Entries : %u\n",
					     Count, pDevice_p->TP_Interface.vDeviceInfo_p[Count].pid_filter_size);
			}
			for (Count = 0; Count < (pDevice_p->TP_Interface.NumberOfvDevices * 2); Count++) {
				stpti_printf("Vdevice data 0x%04x %u\n",
					     pDevice_p->TP_Interface.
					     PIDTable_p[(pDevice_p->TP_Interface.NumberOfPids * 2) + Count],
					     (pDevice_p->TP_Interface.NumberOfPids * 2) + Count);
			}
#endif
			/* Make sure an unexpected mailbox int won't crash the firmware */
			pDeviceInfo_p->pid_table_base = 0;
		}
	} else {
		STPTI_ASSERT(FALSE, "Unable to allocate memory for TE pid table update");
	}
	stptiSupport_MemoryDeallocateForDMA(&CachedMemoryStructure);
}

/**
    @brief  Task for monitoring of live channels DMA beteen STFE and TP

    @param  Param                A pointer the the pDevice structure

    @return                      None
*/
static ST_ErrorCode_t stptiHAL_LiveTask(FullHandle_t pDeviceHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	STPTI_PRINTF("Entering Live Monitor Task for %08x", (unsigned)pDeviceHandle.word);

	/* Main task loop */
	while (!kthread_should_stop()) {
		/* Wake up once a second and monitor system */
		msleep(1000);
		if (!kthread_should_stop()) {
			/* Only monitor status blocks (events) if the xp70 is running - else we will stall when
			   accessing the shared memory interface.  In practices this means we might loose events
			   when going into a power down state. */

			if (pDevice_p->TP_Status == stptiHAL_STXP70_RUNNING) {
				/* Process the status block here */
				stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
				{
					U8 num_channels =
					    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.NumberOfLiveChannels);
					U32 read_pointers[32];
					int i;

					/* Obtain the pointers from the firmware */
					for (i = 0; i < num_channels && i < 32; i++) {
						read_pointers[i] =
						    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.DMAPointers_p[i]);
						stpti_printf("TP RP 0x%x on channel %u\n", read_pointers[i], i);
					}

					/* Call tshal function */
					stptiTSHAL_call(TSInput.TSHAL_TSInputStfeMonitorDREQLevels, 0, read_pointers,
							num_channels);
				}
				stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);
			}
		}
	}

	STPTI_PRINTF("Leaving Monitor Task for %08x", (unsigned)pDeviceHandle.word);

	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiHAL_pDeviceGetCycleCount(FullHandle_t pDeviceHandle, unsigned int *count)
{
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);

	if (count == NULL)
		return (ST_ERROR_BAD_PARAMETER);

	*count = 0;

	if (pDevice_p == NULL)
		return (ST_NO_ERROR);

	if (pDevice_p->TP_Status != stptiHAL_STXP70_RUNNING)
		return (ST_NO_ERROR);

	*count = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->NotIdleCount);

	/* reset the counters (in case they have been in use previously) */
	stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.pDeviceInfo_p->ResetIdleCounters, 1);

	return (ST_NO_ERROR);
}
