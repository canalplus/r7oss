/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

HPI Operating System function declarations

(C) Copyright AudioScience Inc. 1997-2003
******************************************************************************/
#ifndef _HPIOS_H_
#define _HPIOS_H_

#include "hpios_linux_kernel.h"

#ifndef STR_SIZE
#define STR_SIZE(a) (a)
#endif

#ifndef __user
#define __user
#endif
#ifndef __iomem
#define __iomem
#endif

/* physical memory allocation */
void HpiOs_LockedMem_Init(
	void
);
void HpiOs_LockedMem_FreeAll(
	void
);
#define HpiOs_LockedMem_Prepare(a, b, c, d);
#define HpiOs_LockedMem_Unprepare(a)

/** Allocate and map an area of locked memory for bus master DMA operations.

On success, *pLockedMemeHandle is a valid handle, and 0 is returned
On error *pLockedMemHandle marked invalid, non-zero returned.

If this function succeeds, then HpiOs_LockedMem_GetVirtAddr() and
HpiOs_LockedMem_GetPyhsAddr() will always succed on the returned handle.
*/
u16 HpiOs_LockedMem_Alloc(
	struct consistent_dma_area *pLockedMemHandle,	/**< memory handle */
	u32 dwSize, /**< Size in bytes to allocate */
	struct pci_dev *pOsReference
	/**< OS specific data required for memory allocation */
);

/** Free mapping and memory represented by LockedMemHandle

Frees any resources, then invalidates the handle.
Returns 0 on success, 1 if handle is invalid.

*/
u16 HpiOs_LockedMem_Free(
	struct consistent_dma_area *LockedMemHandle
);

/** Get the physical PCI address of memory represented by LockedMemHandle.

If handle is invalid *pPhysicalAddr is set to zero and return 1
*/
u16 HpiOs_LockedMem_GetPhysAddr(
	struct consistent_dma_area *LockedMemHandle,
	u32 *pPhysicalAddr
);

/** Get the CPU address of of memory represented by LockedMemHandle.

If handle is NULL *ppvVirtualAddr is set to NULL and return 1
*/
u16 HpiOs_LockedMem_GetVirtAddr(
	struct consistent_dma_area *LockedMemHandle,
	void **ppvVirtualAddr
);

/** Check that handle is valid
i.e it represents a valid memory area
*/
u16 HpiOs_LockedMem_Valid(
	struct consistent_dma_area *LockedMemHandle
);

/* timing/delay */
void HpiOs_DelayMicroSeconds(
	u32 dwNumMicroSec
);

struct hpi_message;
struct hpi_response;

typedef void HPI_HandlerFunc(
	struct hpi_message *,
	struct hpi_response *
);

#endif				/* _HPIOS_H_ */

/*
*/
