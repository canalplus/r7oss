#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/hrtimer.h>
#include <linux/stat.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/stm/bdisp2_shared_ioctl.h>
#include "bdisp2/bdispII_fops.h"
#include "bdisp2/bdispII_debugfs.h"
#include "bdisp2/bdispII_aq.h"
#include "bdisp2_os_linuxkernel.h"

static int
stm_bdisp2_open(const struct device *sbcd_device)
{
	return 0;
}

static void
stm_bdisp2_close(const struct device *sbcd_device) { }


/* The following couple lines of code make sure that in case a DirectFB
   application crashes/gets killed/interrupted while being in the process
   of processing new operation (i.e new nodes) lock _not_ left locked, which
   would give ourselves a hard time and leave us in a funny state. We
   have no other way of finding this out. It's important to do sth about
   that here, since else e.g. unloading this module could hang forever. */
static void
stm_blitter_iomm_vma_open(struct vm_area_struct * const vma)
{
	struct stm_bdisp2_aq * const aq = vma->vm_private_data;
	unsigned long physaddr = (vma->vm_pgoff << PAGE_SHIFT);

	if (physaddr == aq->bc->mem_region->start)
		atomic_inc(&aq->n_users);
}

static void
stm_blitter_iomm_vma_close(struct vm_area_struct * const vma)
{
	struct stm_bdisp2_aq * const aq = vma->vm_private_data;
	unsigned long physaddr = (vma->vm_pgoff << PAGE_SHIFT);

	if (physaddr == aq->bc->mem_region->start
	    && atomic_dec_and_test(&aq->n_users)) {
		/* now that nobody has the fb device open for BDisp access
		   anymore, it's a good idea to clear the BDisp shared area,
		   just in case the previous user crashed or so and left it
		   in a state inconsistent w/ the hardware ... */
		/* Only unlock if not locked by kernel, otherwise could
		   reset a needed kernel lock */
		if (aq->stdrv.bdisp_shared->locked_by != 3)
			atomic_set(&aq->stdrv.bdisp_shared->lock, 0);
	}
}

static int
stm_blitter_iomm_vma_fault(struct vm_area_struct *vma,
			   struct vm_fault       *vmf)
{
	/* we want to provoke a bus error rather than give the client
	   the zero page */
	return VM_FAULT_SIGBUS;
}

static const struct vm_operations_struct stm_blitter_iomm_nopage_ops = {
	.open  = stm_blitter_iomm_vma_open,
	.close = stm_blitter_iomm_vma_close,
	.fault = stm_blitter_iomm_vma_fault,
};

/* in 3.10 kernel VM_RESERVED has been removed and deprected,
 * use VM_DONTDUMP instead */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define _LOCAL_VM_FLAGS		(VM_IO | VM_RESERVED | VM_DONTEXPAND)
#else
#define _LOCAL_VM_FLAGS		(VM_IO | VM_DONTDUMP | VM_DONTEXPAND)
#endif

static int
stm_bdisp2_mmap(const struct device   *sbcd_device,
		struct vm_area_struct *vma)
{
	struct stm_bdisp2_aq * const aq = dev_get_drvdata(sbcd_device);
	struct stm_bdisp2_driver_data * const stdrv = &aq->stdrv;
	unsigned long size_wanted = vma->vm_end - vma->vm_start;

	/* check if which memory was requested */
	if ((!vma->vm_pgoff
	     || (vma->vm_pgoff == (aq->bc->mem_region->start >> PAGE_SHIFT)))
	    && (size_wanted == PAGE_SIZE)) {
		/* mmap of MMIO was requested - we mmap this uncached */
		vma->vm_pgoff = aq->bc->mem_region->start >> PAGE_SHIFT;
		vma->vm_flags |= _LOCAL_VM_FLAGS;
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		if (io_remap_pfn_range(vma, vma->vm_start,
				       vma->vm_pgoff,
				       size_wanted,
				       vma->vm_page_prot))
			return -EAGAIN;

		vma->vm_private_data = aq;

		/* ensure we get bus errors when we access an illegal
		   memory address */
		vma->vm_ops = &stm_blitter_iomm_nopage_ops;
		stm_blitter_iomm_vma_open(vma);

		return 0;
	} else if ((vma->vm_pgoff == (aq->shared_area.base >> PAGE_SHIFT))
		   && (size_wanted == aq->shared_area.size)) {
		/* mmap of shared area was requested - we mmap this cached */
		vma->vm_pgoff = aq->shared_area.base >> PAGE_SHIFT;
		vma->vm_flags |= _LOCAL_VM_FLAGS;
		if (!aq->shared_area.cached) {
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		} else if (vma->vm_start & shm_align_mask) {
			/* this is bad as it will create cache aliasing. Deny
			   it */
			printk(KERN_DEBUG "refusing mmap() to insecure address\n");
			return -EPERM;
		}
		if (io_remap_pfn_range(vma, vma->vm_start,
				       vma->vm_pgoff,
				       size_wanted,
				       vma->vm_page_prot))
			return -EAGAIN;

		return 0;
	} else if ((vma->vm_pgoff == (stdrv->bdisp_nodes.base >> PAGE_SHIFT))
		   && (size_wanted == stdrv->bdisp_nodes.size)) {
		/* mmap of nodelist was requested - we mmap this uncached */
		vma->vm_pgoff = stdrv->bdisp_nodes.base >> PAGE_SHIFT;
		vma->vm_flags |= _LOCAL_VM_FLAGS;
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		if (io_remap_pfn_range(vma, vma->vm_start,
				       vma->vm_pgoff,
				       size_wanted,
				       vma->vm_page_prot))
			return -EAGAIN;

		return 0;
	}

	return -EPERM;
}

static long
stm_bdisp2_ioctl(const struct device *sbcd_device,
		 unsigned int         cmd,
		 unsigned long        arg)
{
	struct stm_bdisp2_aq * const aq = dev_get_drvdata(sbcd_device);
	struct stm_bdisp2_driver_data * const stdrv = &aq->stdrv;

	switch (cmd) {
	case STM_BDISP2_GET_SHARED_AREA:
		{
		struct stm_bdisp2_shared_cookie shared = {
			.cookie = aq->shared_area.base,
			.size   = aq->shared_area.size,
		};

		if (copy_to_user((void *) arg, &shared, sizeof(shared)))
			return -EFAULT;
		}
		return 0;
		break;

	case STM_BLITTER_SYNC:
	case STM_BDISP2_WAIT_NEXT:
		{
		int res;
		enum stm_bdisp2_wait_type wait = ((cmd == STM_BLITTER_SYNC)
						  ? STM_BDISP2_WT_IDLE
						  : STM_BDISP2_WT_SPACE);

		res = stm_bdisp2_aq_sync(aq, wait, NULL);
		if (res != 0)
			printk(KERN_DEBUG "sync returned %d\n", res);
		return res;
		}
		break;

	case STM_BLITTER_GET_BLTLOAD:
	case STM_BLITTER_GET_BLTLOAD2:
		{
		unsigned long long busy;

		/* we don't do any locking here - this all is for statistics
		   only anyway... */
		if (stdrv->bdisp_shared->bdisp_ops_start_time) {
			/* normally, we calculate the busy ticks upon a node
			   interrupt. But if GET_BLTLOAD is called more often
			   than interrupts occur, it will return zero for a
			   very long time, and then all of a sudden a huge(!)
			   value for busy ticks. To prevent this from
			   happening, we calculate the busy ticks here as
			   well. */
			unsigned long long nowtime = ktime_to_us(ktime_get());
			stdrv->bdisp_shared->ticks_busy
				+= (nowtime
				    - stdrv->bdisp_shared->bdisp_ops_start_time
				   );
			stdrv->bdisp_shared->bdisp_ops_start_time = nowtime;
		}

		busy = stdrv->bdisp_shared->ticks_busy;
		stdrv->bdisp_shared->ticks_busy = 0;

		if (cmd == STM_BLITTER_GET_BLTLOAD)
			return put_user((unsigned long) busy,
					(unsigned long *) arg);

		return put_user(busy, (unsigned long long *) arg);
		}
		break;
	case STM_BLITTER_WAKEUP:
		{
		int res;

		res = bdisp2_wakeup_os(stdrv);

		return res;
		}
		break;

	}

	return -ENOTTY;
}

const struct stm_blitter_file_ops bdisp2_aq_fops = {
	.open  = stm_bdisp2_open,
	.close = stm_bdisp2_close,

	.mmap  = stm_bdisp2_mmap,
	.ioctl = stm_bdisp2_ioctl,

	.debugfs = stm_bdisp2_add_debugfs,
};
