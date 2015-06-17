/*
 * Host driver base API
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

#ifndef _STHORM_HOST_ALLOC_H_
#define _STHORM_HOST_ALLOC_H_

/**
 * \defgroup p12Alloc STHORM Host-Fabric shared memory allocation API
 * @{
 */

typedef struct
{
	void* virtual;
	void* physical;
	size_t size;
} HostAddress;

/**
 * \brief Allocate a contiguous buffer that can be shared with the fabric
 *
 * The allocated buffer is in a region that can be shared with SThorm/P2012
 * and is contiguous (no pagination using the host MMU). Typically the
 * buffer is allocated in external memory (L3/DDR).
 *
 * \return a non zero integer if buffer addresses are valid
 */
int sthorm_malloc(HostAddress* ha, size_t size);
#ifndef HEVC_HADES_CUT1
int sthorm_allocMessage(HostAddress* ha, size_t size);
void sthorm_freeMessage(HostAddress* ha);
#endif

/**
 * \brief Free a shared host/fabric memory region.
 *
 * \param[in] hostAddr the address of the allocated memory region in the Host address space.
 */
void sthorm_free(HostAddress* hostAddr);

/**
 * \brief Map a user-provided buffer into cached virtual memory
 *
 * \return a non zero integer if mapping is successfull
 */
int sthorm_map(HostAddress* ha, void* buffer, size_t size);

/**
 * \brief UnMap a user-provided buffer into cached virtual
 *        memory
 *
 */
void sthorm_unmap(HostAddress* ha);

/**
 * \brief Flush the cache to physical memory
 *        Must be called after the host writes to memory and before Hades reads it
 *
 * \param[in] hostAddr the address of the allocated memory region in the Host address space.
 */
void sthorm_flush(HostAddress* hostAddr);

/**
 * \brief Invalidate the cache
 *        Must be called before the host reads memory written by Hades
 *
 * \param[in] hostAddr the address of the allocated memory region in the Host address space.
 */
void sthorm_invalidate(HostAddress* ha);


/**@}*/

#endif /* _STHORM_HOST_ALLOC_H_ */
