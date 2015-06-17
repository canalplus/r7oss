/*
 * Host driver emulation layer API
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *          Germain Haugou (germain.haugou@st.com)
 *
 */

#ifndef _HOST_DRIVER_EMU_h_
#define _HOST_DRIVER_EMU_h_

#include <linux/types.h>

#include "pedf_host_abi.h"

/**
 * \defgroup driverEmu Emulation layer for the P2012 host driver
 * Provides the necessary function to interact with the STHORM plaform
 * from a "fake" host executing on the workstation.
 * @{
 */

typedef void (*sthorm_host_callback_t)(uintptr_t msg);

/**
 * \brief Initialize driver emulation layer
 *
 * This should be called before sthorm_boot
 *
 * \return 0 if the operation is successfull, -1 if there was an error
 */
int sthorm_init(uintptr_t regsBaseAddress, uint32_t regsSize, uint32_t interruptNumberBase, char* partition);

/**
 * \brief Terminate driver emulation layer
 *
 * \return 0 if the operation is successfull, -1 if there was an error
 */
int sthorm_term(void);

/**
 * \brief Boot the SThorm/P2012 platform
 *
 * \return 0 if the operation is successfull, -1 if there was an error
 */
int sthorm_boot(void);

/**
 * \brief brief Start handling messages comming from SThorm
 *
 * This functions must be called once when the platform boots.
 *
 * Messages from the SThorm fabric are notified through an interrupt, the
 * message receiver responsabilities are to handle the interrupt request
 * and to determine which callback to execute based on the content on the
 * message. The actualy execution of the callback can then be scheduled for
 * latter. Depending on the host/OS type this may be handled by a mechanism
 * such as threads, tasklets or workqueues.
 *
 * \return 0 on success
 */
int sthorm_initMsgReceiver(void);

/**
 * \brief Send a message to the fabric controller
 *
 * Only the address of the actual message structure is written in the FC mailbox.
 * This will trigger a remote function execution on the FC.
 * On the host, this function may lead to a context switch if the semaphore
 * of the FC mailbox is not free.
 *
 * \param[in] msgFabAddr physical address of the message, readable by the FC STxP70
 *
 * \return 0 if succesful
 */
int sthorm_sendMsg(uint32_t msgFabAddr);

/**
 * \brief Send a blocking message to the fabric controller
 *
 * This function behaves like sthorm_sendMsg except that is also wait for
 * the corresponding answer from the fabric. Therefore implementing a
 * synchronous RPC. The caller of this function may be context-switched while
 * waiting.
 *
 * \param[in] msgFabAddr    physical address of the message, readable by the FC STxP70
 * \param[in] msg           address of the message in the host space. The replyCallback
 *                          field of the header will be updated.
 *
 * \return 0 if succesful
 */
int sthorm_sendSyncMsg(uint32_t msgFabAddr, p12_msgHeader_t *msg);

/**
 * \brief A sbrk like function to provide memory to an allocator
 *
 * This function can be called by an allocator like dlmalloc to get new pages
 *
 * \param[in]  size     The amount of memory needed
 *
 * \return the start of the memory region as seen from the host
 */
uint32_t sthorm_sbrk(size_t size);

/**
 * \brief Translate from a host to a fabric address
 *
 * Convert a L3/ExtMem address seen by the host to an L3 address accesible by
 * the SThrom fabric
 *
 * \param[in]   hostAddr    The address in the host space (can be virtual)
 *
 * \return  The physical external address seen by the STxP70 on the fabric
 */
uint32_t sthorm_transExtMemHostToFabric(uintptr_t hostAddr);

/**
 * \brief Translate from a fabric external address to a host address
 *
 * \param[in]    fabricAddr The physical external address seen by the STxP70 on the fabric
 *
 * \return  The address in the host space (can be virtual)
 */
uintptr_t sthorm_transExtMemFabricToHost(uint32_t fabricAddr);

/**
 * \brief Memory copy
 *
 * Do a memcopy from the host to the SThorm fabric. A typicial usage scenario
 * is to load the base-runtime in L3 (External memory). The destination address
 * is assumed to be a physical address in the Fabric memory space and might
 * need translation for the host to access it
 *
 * \param[in]  dest     Destination address in the fabric memory space
 * \param[in]  source   Source address in the host memory space
 * \param[in]  size     Transfer size
 * \param[in]  cookie   [Parameter not used for now, set to NULL]
 */
void sthorm_memcpy(void *dest, void *source, int size);

/**
 * \brief Memory set
 *
 * Do a memset from the host in the SThorm fabric memories. A typicial usage
 * scenario is to load the base-runtime in L3 (External memory).
 *
 * \param[in]  dest     Destination address in the fabric memory space
 * \param[in]  value    Value to set in the memory
 * \param[in]  size     Buffer size
 * \param[in]  cookie   [Parameter not used for now, set to NULL]
 */
void sthorm_memset(void *dest, int value, int size);

/**@}*/

#endif /* _HOST_DRIVER_EMU_h_ */
