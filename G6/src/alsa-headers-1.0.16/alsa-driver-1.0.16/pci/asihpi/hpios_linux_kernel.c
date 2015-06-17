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

HPI Operating System function implementation for Linux

(C) Copyright AudioScience Inc. 1997-2003
******************************************************************************/
#define SOURCEFILE_NAME "hpios_linux_kernel.c"
#include "hpidebug.h"
#include <linux/delay.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2 , 6 , 14)
void HpiOs_DelayMicroSeconds(
	uint32_t dwNumMicroSec
)
{
	if ((usecs_to_jiffies(dwNumMicroSec) > 1) && !in_interrupt()) {
		/* MUST NOT SCHEDULE IN INTERRUPT CONTEXT! */
		schedule_timeout_uninterruptible(usecs_to_jiffies
			(dwNumMicroSec));
	} else if (dwNumMicroSec <= 2000)
		udelay(dwNumMicroSec);
	else
		mdelay(dwNumMicroSec / 1000);

}
#else
void HpiOs_DelayMicroSeconds(
	uint32_t dwNumMicroSec
)
{
	if ((dwNumMicroSec / 1000 >= 1000000 / HZ) && !in_interrupt()) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout((HZ * dwNumMicroSec + (HZ - 1)) / 1000000);
	} else if (dwNumMicroSec <= 2000)
		udelay(dwNumMicroSec);
	else
		mdelay(dwNumMicroSec / 1000);

}
#endif

void HpiOs_LockedMem_Init(
	void
)
{
}

/** Allocated an area of locked memory for bus master DMA operations.

On error, return -ENOMEM, and *pMemArea.size = 0
*/
u16 HpiOs_LockedMem_Alloc(
	struct consistent_dma_area *pMemArea,
	u32 size,
	struct pci_dev *pdev
)
{
	/*?? any benefit in using managed dmam_alloc_coherent? */
	pMemArea->vaddr =
		dma_alloc_coherent(&pdev->dev, size,
		&pMemArea->dma_handle, GFP_DMA32);

	if (pMemArea->vaddr) {
		HPI_DEBUG_LOG(DEBUG, "Allocated %d bytes, dma 0x%x vma %p\n",
			size,
			(unsigned int)pMemArea->dma_handle, pMemArea->vaddr);
		pMemArea->pdev = &pdev->dev;
		pMemArea->size = size;
		return 0;
	} else {
		HPI_DEBUG_LOG(WARNING,
			"Failed to allocate %d bytes locked memory\n", size);
		pMemArea->size = 0;
		return -ENOMEM;
	}
}

uint16_t HpiOs_LockedMem_Free(
	struct consistent_dma_area * pMemArea
)
{
	if (pMemArea->size) {
		dma_free_coherent(pMemArea->pdev, pMemArea->size,
			pMemArea->vaddr, pMemArea->dma_handle);
		HPI_DEBUG_LOG(DEBUG, "Freed %lu bytes, dma 0x%x vma %p\n",
			(unsigned long)pMemArea->size,
			(unsigned int)pMemArea->dma_handle, pMemArea->vaddr);
		pMemArea->size = 0;
		return 0;
	} else {
		return 1;
	}
}

void HpiOs_LockedMem_FreeAll(
	void
)
{
}
