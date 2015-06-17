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
   @file   pti_swinjector.c
   @brief  The Software Injector for playback streams.

   This file implements the functions for injecting a stream into the demux.

   The Injector has queue of injection nodes - structures that tell the transport process where to
   get the data.  The injection nodes can be chained together, which helps when injectiong non
   contiguous data.

   Effectively the array of nodes form a queue implemented as a circular buffer. Nodes are added
   to the queue and linked together to form a linked list.

   Once injection is started (stptiHAL_SoftwareInjectorStart) these nodes are left untouched are
   committed to being injected and can only be removed by Aborting the injection.

   Once started any new nodes added are not linked to the "committed nodes", but form a new chained
   set.  Another stptiHAL_SoftwareInjectorStart is needed to start injection.

   The InjectionFlush operation will remove any node not "committed" for playback and remove any
   partial packets, so that the next injection will be aligned to a packet boundry.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_pdevice.h"
#include "pti_vdevice.h"
#include "pti_swinjector.h"
#include "pti_dbg.h"
#include "pti_mailbox.h"
#include "firmware/pti_tp_api.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAllocator(FullHandle_t SoftwareInjectorHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SoftwareInjectorDeallocator(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorAddInjectNode(FullHandle_t SoftwareInjectorHandle, U8 *Data, U32 Size,
						      U32 Ctrl);
ST_ErrorCode_t stptiHAL_SoftwareInjectorStart(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorWaitForCompletion(FullHandle_t SoftwareInjectorHandle, int TimeoutInMS);
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbort(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorFlush(FullHandle_t SoftwareInjectorHandle);

/* Public Constants ------------------------------------------------------- */

/* Export the API */
const stptiHAL_SoftwareInjectorAPI_t stptiHAL_SoftwareInjectorAPI = {
	{
		/* Allocator                        Associator  Disassociator  Deallocator */
		stptiHAL_SoftwareInjectorAllocator, NULL,        NULL,         stptiHAL_SoftwareInjectorDeallocator,
		NULL, NULL
	},
	stptiHAL_SoftwareInjectorAddInjectNode,
	stptiHAL_SoftwareInjectorStart,
	stptiHAL_SoftwareInjectorWaitForCompletion,
	stptiHAL_SoftwareInjectorAbort,
	stptiHAL_SoftwareInjectorFlush
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocator for a software injector.

   This function allocate a software injector and reserves a playback channel on this TANGO device.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @param  params_p                A pointer to the stptiHAL_SoftwareInjectorConfigParams_t struct
                                   populated with the initialisation.

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAllocator(FullHandle_t SoftwareInjectorHandle, void *params_p)
{
	Object_t *AssocdObject_p;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(SoftwareInjectorHandle);
	stptiHAL_SoftwareInjector_t *SoftwareInjector_p = stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);
	stptiHAL_SoftwareInjectorConfigParams_t *Parameters_p = (stptiHAL_SoftwareInjectorConfigParams_t *) params_p;

	/* Already write locked */

	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SoftwareInjectorHandle);
	int Channel, index;

	int MaxNumberOfNodes = Parameters_p->MaxNumberOfNodes;

	/* Check that there is not any software injectors on this vDevice already */
	stptiOBJMAN_FirstInList(vDevice_p->SoftwareInjectorHandles, (void *)&AssocdObject_p, &index);
	while (index >= 0) {
		FullHandle_t ObjectHandle = stptiOBJMAN_ObjectPointerToHandle(AssocdObject_p);
		if (stptiOBJMAN_GetvDeviceIndex(SoftwareInjectorHandle) == stptiOBJMAN_GetvDeviceIndex(ObjectHandle)) {
			Error = STPTI_ERROR_ONLY_ONE_INJECTOR_ALLOWED_ON_A_DEVICE;
			break;
		}
		stptiOBJMAN_NextInList(vDevice_p->SoftwareInjectorHandles, (void *)&AssocdObject_p, &index);
	}

	if (ST_NO_ERROR == Error) {
		if (ST_NO_ERROR ==
		    stptiOBJMAN_AddToList(vDevice_p->SoftwareInjectorHandles, SoftwareInjector_p, FALSE, &Channel)) {
			if (Channel >= STREAM_AVAILIABLITY_PLAYBACK_CHANNELS) {
				/* Too many channels */
				stptiOBJMAN_RemoveFromList(vDevice_p->SoftwareInjectorHandles, Channel);
				Error = ST_ERROR_NO_MEMORY;
			} else {
				sema_init(&SoftwareInjector_p->InjectionSemaphore, 0);
			}
		}
	}
	if (ST_NO_ERROR == Error) {
		/* Allocate Aligned Memory for Nodes */
		U32 Memory2Allocate = MaxNumberOfNodes * sizeof(stptiHAL_SoftwareInjectionNode_t);
		SoftwareInjector_p->CachedMemoryStructure.Dev = &pDevice_p->pdev->dev;
		SoftwareInjector_p->InjectionNodeList_p =
		    (stptiHAL_SoftwareInjectionNode_t *) stptiSupport_MemoryAllocateForDMA(Memory2Allocate,
											   STPTI_SUPPORT_DCACHE_LINE_SIZE,
											   &SoftwareInjector_p->CachedMemoryStructure,
											   stptiSupport_ZONE_SMALL);
		if (SoftwareInjector_p->InjectionNodeList_p == NULL) {
			stptiOBJMAN_RemoveFromList(vDevice_p->SoftwareInjectorHandles, Channel);
			Error = ST_ERROR_NO_MEMORY;
		} else {
			/* We clear the allocated memory (in particular partial_packet element in the stptiHAL_SoftwareInjectionNode_t array) */
			memset(SoftwareInjector_p->InjectionNodeList_p, 0, Memory2Allocate);
		}
	}

	if (ST_NO_ERROR == Error) {
		SoftwareInjector_p->Channel = Channel;
		SoftwareInjector_p->IsActive = FALSE;

		SoftwareInjector_p->PacketLength = vDevice_p->PacketSize;

		SoftwareInjector_p->LengthOfNodeList = MaxNumberOfNodes;
		SoftwareInjector_p->NextToBeInjected = 0;
		SoftwareInjector_p->InjectionNodeCount = 0;
		SoftwareInjector_p->NodeCount = 0;
		SoftwareInjector_p->FlushPartialPkts = TRUE;

		stptiHAL_GetObjectpDevice_p(SoftwareInjectorHandle)->InjectionSemaphore_p[Channel] =
		    &SoftwareInjector_p->InjectionSemaphore;

		stpti_printf("Allocated SoftwareInjector on Channel %d", Channel);
	} else {
		STPTI_PRINTF_ERROR("Error during allocation of SoftwareInjector.");
	}

	return (Error);
}

/**
   @brief  Deallocator for a software injector.

   This function deallocate a software injector and stops it if it is injecting.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorDeallocator(FullHandle_t SoftwareInjectorHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_WRITELOCKED; /* Yes write locked, as it has already been locked by ObjectDeallocate() */

	stptiHAL_SoftwareInjector_t *SoftwareInjector_p = NULL;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SoftwareInjectorHandle);
	volatile stptiTP_Playback_t *TP_PlaybackNode_p = vDevice_p->TP_Interface.Playback_p;

	/* We signal (as a fail safe) to unblock anything waiting on the injection */
	stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);

	/* Ignore Error (we can't do much about it as we are deallocating) */
	stptiHAL_SoftwareInjectorAbort(SoftwareInjectorHandle);

	stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);

	/* We get the object pointer again in case anything has changed since we unlocked */
	SoftwareInjector_p = stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);

	/* Stop Injection (if it is injecting) */
	stptiHAL_pDeviceXP70Write(&TP_PlaybackNode_p->channel, SoftwareInjector_p->Channel);
	/* The rest of the node structure is not looked at by the TP for the playback stop command. */

	stptiHAL_MailboxPost(SoftwareInjectorHandle, MAILBOX_STOP_PLAYBACK, 0);

	/* Signal the completion just incase */
	up(&SoftwareInjector_p->InjectionSemaphore);

	/* Belt and braces - probably not required */
	SoftwareInjector_p->NodeCount = 0;
	SoftwareInjector_p->InjectionNodeCount = 0;
	SoftwareInjector_p->IsActive = FALSE;

	/* Allow any tasks waiting on the semaphore to complete */
	msleep(1);

	stptiSupport_MemoryDeallocateForDMA(&SoftwareInjector_p->CachedMemoryStructure);

	stptiHAL_GetObjectpDevice_p(SoftwareInjectorHandle)->InjectionSemaphore_p[SoftwareInjector_p->Channel] = NULL;

	stptiOBJMAN_RemoveFromList(vDevice_p->SoftwareInjectorHandles, SoftwareInjector_p->Channel);

	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Add an injection node to a software injector.

   This function allocates a software injection node (a memory region defining packets to be
   injected).  It can be called repeatedly to chain together memory regions.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.
   @param  Data                    A pointer to the start of the memory region for this node.
   @param  Size                    The size of the memory region for this node.
   @param  Ctrl                    Always 0.

   @return                         A standard st error type...
                                   - ST_ERROR_DEVICE_BUSY if already injecting
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAddInjectNode(FullHandle_t SoftwareInjectorHandle, U8 *Data, U32 Size,
						      stptiHAL_InjectionFlags_t InjectionFlags)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SoftwareInjectorHandle);
		stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
		    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);
		unsigned int PacketLength = SoftwareInjector_p->PacketLength;
		stptiTP_InjectionFlags_t injection_flags = 0;

		if (InjectionFlags & stptiHAL_PKT_INCLUDES_STFE_TAGS) {
			PacketLength += 8;
		} else if (InjectionFlags & stptiHAL_PKT_INCLUDES_DNLA_TTS_TAGS) {
			PacketLength += 4;
			injection_flags |= INJECTION_INCLUDES_DLNA_TAGS;
		} else {
			/* Default case, no tags */
			injection_flags |= INJECTION_EXCLUDES_TAGS;
		}

		if (SoftwareInjector_p->FlushPartialPkts) {
			injection_flags |= DISCARD_PREVIOUS_PARTIAL_PKT;
			SoftwareInjector_p->FlushPartialPkts = FALSE;
		}

		if (SoftwareInjector_p->NodeCount >= SoftwareInjector_p->LengthOfNodeList) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			int index =
			    (SoftwareInjector_p->NextToBeInjected + SoftwareInjector_p->NodeCount -
			     SoftwareInjector_p->InjectionNodeCount) % SoftwareInjector_p->LengthOfNodeList;
			stptiHAL_SoftwareInjectionNode_t *NewNode_p = (SoftwareInjector_p->InjectionNodeList_p) + index;
			stptiTSHAL_TimerValue_t TimerValue = { 0, 0, 0, 0, 0 };
			int i, checksum = 0;
			uint32_t *p = NULL;

			NewNode_p->PlaybackNode.channel = SoftwareInjector_p->Channel;
			NewNode_p->PlaybackNode.buffer_base_pb = (U32) Data;
			NewNode_p->PlaybackNode.num_of_bytes_pb = Size;	/* in Bytes */
			NewNode_p->PlaybackNode.ctrl = 0;	/* would be control bits (BluRay) */
			NewNode_p->PlaybackNode.injection_flags = injection_flags;
			NewNode_p->PlaybackNode.next_node = 0xFFFFFFFF;	/* END OF LIST */
			NewNode_p->PlaybackNode.packet_len_pb = PacketLength;

			/* Get the Time (purposefully ignore any returned error) */
			stptiTSHAL_call(TSInput.TSHAL_TSInputStfeGetTimer, 0, vDevice_p->StreamID, &TimerValue, NULL);

			NewNode_p->PlaybackNode.tag_header0 = TimerValue.TagLSWord;
			NewNode_p->PlaybackNode.tag_header1 = TimerValue.TagMSWord & 0x0000FFFF;
			NewNode_p->PlaybackNode.tag_header1 |=
			    (PLAYBACK_TAG_IS_VDEVICE_INDEX | stptiOBJMAN_GetvDeviceIndex(SoftwareInjectorHandle)) << 16;

			/* compute the checksum */
			p = (uint32_t *) & NewNode_p->PlaybackNode.channel;

			for (i = 0; i < ((sizeof(stptiTP_Playback_t) - sizeof(uint32_t))); i += sizeof(uint32_t)) {
				checksum ^= *(p++);
			}
			NewNode_p->PlaybackNode.checksum = checksum;

			stpti_printx(NewNode_p->PlaybackNode.buffer_base_pb);
			stpti_printd(NewNode_p->PlaybackNode.num_of_bytes_pb);
			stpti_printd(NewNode_p->PlaybackNode.ctrl);
			stpti_printd(NewNode_p->PlaybackNode.packet_len_pb);

			stptiSupport_FlushRegion(&SoftwareInjector_p->CachedMemoryStructure, NewNode_p,
						 sizeof(stptiHAL_SoftwareInjectionNode_t));

			/* Unless this is the first node we need to link them together */
			if ((SoftwareInjector_p->NodeCount - SoftwareInjector_p->InjectionNodeCount) > 0) {
				stptiHAL_SoftwareInjectionNode_t *LastNode_p;
				if (index > 0) {
					LastNode_p = (SoftwareInjector_p->InjectionNodeList_p) + index - 1;
				} else {
					LastNode_p =
					    (SoftwareInjector_p->InjectionNodeList_p) +
					    (SoftwareInjector_p->LengthOfNodeList - 1);
				}
				LastNode_p->PlaybackNode.next_node =
				    (U32) stptiSupport_VirtToPhys(&SoftwareInjector_p->CachedMemoryStructure,
								  NewNode_p);

				/* Update the checksum as you have changed the next_mode ptr */
				p = (uint32_t *) & LastNode_p->PlaybackNode.channel;
				/* compute the checksum */
				for (i = 0, checksum = 0; i < ((sizeof(stptiTP_Playback_t) - sizeof(uint32_t)));
				     i += sizeof(uint32_t)) {
					checksum ^= *(p++);
				}
				LastNode_p->PlaybackNode.checksum = checksum;

				stptiSupport_FlushRegion(&SoftwareInjector_p->CachedMemoryStructure, LastNode_p,
							 sizeof(stptiHAL_SoftwareInjectionNode_t));
			}

			SoftwareInjector_p->NodeCount++;
		}
	}
	stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);
	return (Error);
}

/**
   @brief  Start a Software Injector Injecting the Node Chain.

   This function starts the injector.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_ERROR_TIMEOUT if unable to start the injection
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorStart(FullHandle_t SoftwareInjectorHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* See if the Injector has finished (must be done OUTSIDE an ACCESS LOCK) */
	Error = stptiHAL_SoftwareInjectorWaitForCompletion(SoftwareInjectorHandle, 0);

	/* ST_ERROR_TIMEOUT means the Injector has not finished previous injection */
	/* ST_NO_ERROR means the Injector is now idle */
	if (ST_NO_ERROR == Error) {
		stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

		/* Start Injection */
		stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);
		{
			stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(SoftwareInjectorHandle);
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SoftwareInjectorHandle);
			stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
			    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);

			volatile stptiTP_Playback_t *TP_PlaybackNode_p = vDevice_p->TP_Interface.Playback_p;
			volatile stptiHAL_SoftwareInjectionNode_t *InjectionNode_p =
			    (SoftwareInjector_p->InjectionNodeList_p) + SoftwareInjector_p->NextToBeInjected;

			/* Swallow any extra "injection complete" signals (maybe from a previous abort) to avoid confusing the this injection */
			while (!stptiSupport_TimedOutWaitingForSemaphore(&SoftwareInjector_p->InjectionSemaphore, 0)) ;

			/* Only inject if there are nodes to inject */
			if (SoftwareInjector_p->NodeCount > 0) {
				stptiHAL_pDeviceXP70MemcpyTo((void *)TP_PlaybackNode_p,
							     (void *)&InjectionNode_p->PlaybackNode,
							     sizeof(stptiTP_Playback_t));
				wmb();

				stptiHAL_MailboxPost(SoftwareInjectorHandle, MAILBOX_START_PLAYBACK, 0);

				/* Wait for acknowledgement that playback initialisation is complete */
				if (stptiSupport_TimedOutWaitingForSemaphore(&pDevice_p->TPAckSemaphore, 100)) {
					STPTI_PRINTF_ERROR("Unable to start playback on channel %d.",
							   SoftwareInjector_p->Channel);
					Error = ST_ERROR_TIMEOUT;
				} else {
					/* We know that InjectionNodeCount is zero (since the injector has completed), so all nodes present need to be injected */
					SoftwareInjector_p->InjectionNodeCount = SoftwareInjector_p->NodeCount;
					SoftwareInjector_p->NextToBeInjected =
					    (SoftwareInjector_p->NextToBeInjected +
					     SoftwareInjector_p->NodeCount) % SoftwareInjector_p->LengthOfNodeList;
					SoftwareInjector_p->IsActive = TRUE;
					stpti_printf("Playback Started on channel %d.", SoftwareInjector_p->Channel);
				}
			}
		}
		stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);
	}

	return (Error);
}

/**
   @brief  Abort Software Injector Injection.

   This function stops the injector.  You can stop the injector at any time.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbort(FullHandle_t SoftwareInjectorHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* See if the Injector has finished (must be done OUTSIDE an ACCESS LOCK) */
	Error = stptiHAL_SoftwareInjectorWaitForCompletion(SoftwareInjectorHandle, 0);

	/* ST_ERROR_TIMEOUT means the Injector has not finished previous injection */
	/* ST_NO_ERROR means the Injector is now idle */
	if (Error == ST_ERROR_TIMEOUT) {
		/* IsActive must be TRUE */
		/* Okay lets force the Injection to stop */

		stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
		stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SoftwareInjectorHandle);
			stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
			    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);
			volatile stptiTP_Playback_t *TP_PlaybackNode_p = vDevice_p->TP_Interface.Playback_p;

			stptiHAL_pDeviceXP70Write(&TP_PlaybackNode_p->channel, SoftwareInjector_p->Channel);
			/* The rest of the node structure is not looked at by the TP for the playback stop command. */

			stptiHAL_MailboxPost(SoftwareInjectorHandle, MAILBOX_STOP_PLAYBACK, 0);

			up(&SoftwareInjector_p->InjectionSemaphore);

			SoftwareInjector_p->NodeCount = 0;
			SoftwareInjector_p->InjectionNodeCount = 0;
			SoftwareInjector_p->IsActive = FALSE;

			msleep(1);
		}
		stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);

		Error = ST_NO_ERROR;
	}

	return (Error);
}

/**
   @brief  Wait for the current software injection to complete

   This function waits on the injection complete semaphore.

   This function is called outside of an API lock so it is possible that the software injector could have
   been destroyed by another task.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.
   @param  TimeoutInMS             Specifies how long to wait (in miliseconds) before returning.  To
                                   poll if an injection has completed use 0 (zero).  To wait forever
                                   use a negative value (-1).

   @return                         A standard st error type...
                                   - ST_ERROR_TIMEOUT if still waiting for an injection to finish after the timeout
                                   - ST_NO_ERROR if not injecting (idle)
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorWaitForCompletion(FullHandle_t SoftwareInjectorHandle, int TimeoutInMS)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	struct semaphore *Semaphore;
	BOOL InjectorIsActive;

	stptiOBJMAN_ReadLock(SoftwareInjectorHandle, &LocalLockState);
	{
		stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
		    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);
		Semaphore = &SoftwareInjector_p->InjectionSemaphore;
		InjectorIsActive = SoftwareInjector_p->IsActive;
	}
	stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);

	if (!InjectorIsActive) {
		/* We are not injecting, so lets not waste time looking for the semaphore */
		Error = ST_NO_ERROR;
	} else {
		/* Wait on a semaphore.  Must be done, outside a lock to allow context switching */
		if (stptiSupport_TimedOutWaitingForSemaphore(Semaphore, TimeoutInMS)) {
			/* Timeout getting the semaphore - Injection still happening */
			Error = ST_ERROR_TIMEOUT;
		} else {
			stpti_printf("Injection Completed");
			stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);
			{
				if (LocalLockState == stptiSupport_WRITELOCKED) {
					stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
					    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);

					if (SoftwareInjector_p != NULL) {
						SoftwareInjector_p->NodeCount -= SoftwareInjector_p->InjectionNodeCount;
						SoftwareInjector_p->InjectionNodeCount = 0;
						SoftwareInjector_p->IsActive = FALSE;
					}
					stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);
				} else {
					/* In the unlikely event the Software Injector handle has been destroyed during the wait return an error */
					Error = STPTI_ERROR_NOT_INITIALISED;
				}
			}
		}

		/* In the current mechanism we should only get one signal per injection, and every injection
		   must be started here.  So if we have two then we have a fault. */
		{
			BOOL FlaggedWarning = FALSE;
			while (!stptiSupport_TimedOutWaitingForSemaphore(Semaphore, 0)) {
				if (!FlaggedWarning) {
					STPTI_PRINTF_WARNING
					    ("Too many injection complete signals.  Did you STPTI_Close whilst injecting?");
					FlaggedWarning = TRUE;
				}
			}
		}
	}

	return (Error);
}

/**
   @brief  Flush out any inactive nodes, and packet align next injection.

   This function discards any inactive injection nodes (it does not stop the current injection), and
   will make sure that the next injection is considered as the start of a packet.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorFlush(FullHandle_t SoftwareInjectorHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SoftwareInjectorHandle, &LocalLockState);
	{
		stptiHAL_SoftwareInjector_t *SoftwareInjector_p =
		    stptiHAL_GetObjectSoftwareInjector_p(SoftwareInjectorHandle);

		SoftwareInjector_p->FlushPartialPkts = TRUE;
		SoftwareInjector_p->NodeCount = SoftwareInjector_p->InjectionNodeCount;
	}
	stptiOBJMAN_Unlock(SoftwareInjectorHandle, &LocalLockState);

	return (Error);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Abort activity on the Software Injector.

   This function performs the same task as stptiHAL_SoftwareInjectorAbort() but is provided for
   internal HAL use.  It is used during powerdown to make sure the software injector is inactive.

   NOTE!  It must be called outside of a HAL access lock.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbortInternal(FullHandle_t SoftwareInjectorHandle)
{
	return (stptiHAL_SoftwareInjectorAbort(SoftwareInjectorHandle));
}
