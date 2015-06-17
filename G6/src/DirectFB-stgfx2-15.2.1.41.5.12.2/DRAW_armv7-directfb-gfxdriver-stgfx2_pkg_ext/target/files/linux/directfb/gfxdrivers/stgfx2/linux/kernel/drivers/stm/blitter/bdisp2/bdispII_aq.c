#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/bpa2.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <asm/sizes.h>

#if defined(CONFIG_ARM)
/* ARM has mis-named this interface relative to all the other platforms that
   define it (including x86). */
#  define ioremap_cache ioremap_cached
#endif

#include "linux/stm/blitter.h"
#include <linux/stm/bdisp2_shared.h>
#include <linux/stm/bdisp2_nodegroups.h>
#include <linux/stm/bdisp2_registers.h>

#include "blit_debug.h"

#include "bdisp2/bdispII_aq.h"
#include "bdisp2/bdisp_accel.h"

#include "bdisp2/bdisp2_os.h"


static const int NODES_SIZE = 45 * PAGE_SIZE;


#define blitload_init_ticks(shared) \
	({ \
	(shared)->bdisp_ops_start_time = 0; \
	(shared)->ticks_busy = 0; \
	})

/* we want some more statistics, so lets remember the number of ticks the
   BDisp was busy executing nodes */
#define blitload_calc_busy_ticks() \
	({ \
	ktime_t now = ktime_get(); \
	shared->ticks_busy += (ktime_to_us(now)                               \
			       - shared->bdisp_ops_start_time);               \
	if (unlikely(!shared->ticks_busy))                                    \
		/* this can happen if the BDisp was faster in finishing a     \
		   Node(list) than it takes us to see a change in 'ticks'.    \
		   'Ticks' are in useconds at the moment, so fast enough; but \
		   still. Lets assume the BDisp was busy, because else the    \
		   interrupt should not have been asserted! */                \
		shared->ticks_busy = 1;                                       \
	shared->bdisp_ops_start_time = 0;                                     \
	})


int
stm_bdisp2_aq_sync(struct stm_bdisp2_aq       *aq,
		   enum stm_bdisp2_wait_type   wait,
		   const stm_blitter_serial_t * const serial)
{
	int res;

	/* We need to deal with possible signal delivery while sleeping, this is
	   indicated by a non zero return value.
	   The sleep itself is safe from race conditions */
	stm_blit_printd(3,
			"%p: Waiting for %s.... (%s, last_free/next_free %lu-%lu, CTL %.8x, STA %.8x ITS %.8x)\n",
			aq,
			((wait == STM_BDISP2_WT_IDLE)
			 ? "idle"
			 : ((wait == STM_BDISP2_WT_SPACE)
			    ? "a bit"
			    : "fence")),
			(!bdisp2_is_idle(&aq->stdrv)
			 ? "running"
			 : "   idle"),
			aq->stdrv.bdisp_shared->last_free,
			aq->stdrv.bdisp_shared->next_free,
			bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					  BDISP_AQ_CTL),
			bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					  BDISP_AQ_STA),
			bdisp2_get_reg(&aq->stdrv, BDISP_ITS));

	switch (wait) {
	case STM_BDISP2_WT_IDLE:
		/* doesn't need to be atomic, it's for statistics only
		   anyway... */
		aq->stdrv.bdisp_shared->num_wait_idle++;

		res = wait_event_interruptible(
			aq->idle_wq,
			bdisp2_is_idle(&aq->stdrv));
		break;
	case STM_BDISP2_WT_SPACE:
		/* doesn't need to be atomic, it's for statistics only
		   anyway... */
		aq->stdrv.bdisp_shared->num_wait_next++;

		res = wait_event_interruptible(
			aq->last_free_wq,
			(aq->stdrv.bdisp_shared->last_free
			 != aq->stdrv.bdisp_shared->next_free));
		break;
	case STM_BDISP2_WT_FENCE:
		{
		unsigned long lo = *serial & 0xffffffff;

		/* FIXME: add statistics for fence waits */

		res = wait_event_interruptible(
			aq->last_free_wq,
			(bdisp2_is_idle(&aq->stdrv)
			 || (bdisp2_get_reg(&aq->stdrv,
					    0x200 + 0xb4 /* BLT_USR */)
			     >= lo)));
		}
		break;
	default:
		BUG();
		res = 0;
		break;
	}

	stm_blit_printd(3,
			"%p: ...done (%s, last_free/next_free %lu-%lu, CTL %.8x, STA %.8x ITS %.8x)\n",
			aq,
			(!bdisp2_is_idle(&aq->stdrv)
			 ? "running"
			 : "   idle"),
			aq->stdrv.bdisp_shared->last_free,
			aq->stdrv.bdisp_shared->next_free,
			bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					  BDISP_AQ_CTL),
			bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					  BDISP_AQ_STA),
			bdisp2_get_reg(&aq->stdrv, BDISP_ITS));

	return res;
}

static irqreturn_t
stm_bdisp2_aq_interrupt(int   irq,
			void *data)
{
	struct stm_bdisp2_aq *aq = data;
	unsigned long flags, irqmask;
	volatile struct stm_bdisp2_shared_area __iomem *shared;

	shared = aq->stdrv.bdisp_shared;

	/* check and clear interrupt bits for this queue */
	spin_lock_irqsave(&aq->bc->register_lock, flags);
	irqmask = bdisp2_get_reg(&aq->stdrv, BDISP_ITS) & (aq->lna_irq_bit
							   | aq->node_irq_bit);
	bdisp2_set_reg(&aq->stdrv, BDISP_ITS, irqmask);
	spin_unlock_irqrestore(&aq->bc->register_lock, flags);

#if defined(CONFIG_PM_RUNTIME)
	/* Mark current time as being last busy */
	pm_runtime_mark_last_busy(aq->dev);

	if (shared->num_pending_requests == 0) {
		shared->num_pending_requests++;

		/* Call for autosuspend */
		pm_runtime_put_autosuspend(aq->dev);
	}
#endif

	/* check if interrupt was for us */
	if (irqmask) {
		++shared->num_irqs;

		/* the status register hold the address of the last node that
		   generated an interrupt */
		shared->last_free = (bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
						       BDISP_AQ_STA)
				     - aq->stdrv.bdisp_nodes.base);

		stm_blit_printd(3, "%s @ %p: ITS: %.8lx: %c%c\n",
				__func__, aq, irqmask,
				(irqmask & aq->node_irq_bit) ? 'n' : '_',
				(irqmask & aq->lna_irq_bit) ?  'q' : '_');

		if (irqmask & aq->node_irq_bit) {
			ktime_t now = ktime_get();

			blitload_calc_busy_ticks();
			shared->bdisp_ops_start_time = ktime_to_us(now);

			++shared->num_node_irqs;
			if (shared->last_lut
			    && (shared->last_free == shared->last_lut))
				shared->last_lut = 0;
		}

		if (irqmask & aq->lna_irq_bit) {
			/* We have processed all currently available nodes in
			   the buffer so signal that the blitter is now finished
			   for SyncChip. */
			/* It can happen that the IRQ was fired at the same time
			   as userspace was updating the LNA, in which case we
			   get the LNA interrupt and the BDisp is not idle
			   (anymore) as it's already executing the new
			   instruction(s). */
			if (!bdisp2_is_idle(&aq->stdrv)) {
				stm_blit_printd(
					3,
					"%s @ %p: atomic...: %.8lx/%.8x\n",
					__func__, aq,
					shared->prev_set_lna,
					bdisp2_get_AQ_reg(&aq->stdrv,
							  &aq->stdev,
							  BDISP_AQ_LNA));
			} else {
				/* For compatibility with old lib */
				shared->bdisp_running = 0;

				++shared->num_idle;

				stm_blit_printd(
					3,
					"%s @ %p: done: %.8lx/%.8x\n",
					__func__, aq,
					shared->prev_set_lna,
					bdisp2_get_AQ_reg(&aq->stdrv,
							  &aq->stdev,
							  BDISP_AQ_LNA));

				blitload_calc_busy_ticks();
			}

			++shared->num_lna_irqs;
			shared->last_lut = 0;

			mb();

			wake_up_all(&aq->idle_wq);
		}

		mb();

		/* signal to sleepers that aq->shared->last_free has changed */
		wake_up_all(&aq->last_free_wq);
	}

	return IRQ_RETVAL(irqmask);
}


/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr, size)   (((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr, size) ((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr, size)      _ALIGN_UP(addr, size)

int
stm_bdisp2_allocate_bpa2(size_t                      size,
			 uint32_t                    align,
			 bool                        cached,
			 struct stm_bdisp2_dma_area *area)
{
	static const char * const partnames[] = { "blitter" };
	unsigned int       idx; /* index into partnames[] */
	int                pages;

	BUG_ON(area == NULL);

	area->part = NULL;
	for (idx = 0; idx < ARRAY_SIZE(partnames); ++idx) {
		area->part = bpa2_find_part(partnames[idx]);
		if (area->part)
			break;
		stm_blit_printd(0,
				"BPA2 partition '%s' not found!\n",
				partnames[idx]);
	}
	if (!area->part) {
		stm_blit_printe("No BPA2 partition found!\n");
		goto out;
	}
	stm_blit_printd(0, "Using BPA2 partition '%s'\n", partnames[idx]);

	/* round size to pages */
	pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	area->size = pages * PAGE_SIZE;

	area->base = bpa2_alloc_pages(area->part,
				      pages, align / PAGE_SIZE, GFP_KERNEL);
	if (!area->base) {
		stm_blit_printe(
			"Couldn't allocate %d pages from BPA2 partition '%s'\n",
			pages, partnames[idx]);
		goto out;
	}

	/* remap physical */
	if (cached)
		area->memory = ioremap_cache(area->base, area->size);
	else
		area->memory = ioremap_wc(area->base, area->size);
	if (!area->memory) {
		stm_blit_printe("ioremap of BPA2 memory failed\n");
		goto out_bpa2_free;
	}

	area->cached = cached;

	return 0;

out_bpa2_free:
	bpa2_free_pages(area->part, area->base);
out:
	return -ENOMEM;
}

void
stm_bdisp2_free_bpa2(struct stm_bdisp2_dma_area *area)
{
	BUG_ON(area == NULL);

	iounmap((void *) area->memory);
	bpa2_free_pages(area->part, area->base);

	area->part = NULL;
	area->base = 0;
	area->size = 0;
	area->memory = NULL;
}

int
stm_bdisp2_aq_init(struct stm_bdisp2_aq      *aq,
		   int                        index,
		   int                        irq,
		   struct stm_plat_blit_data *plat_data,
		   struct stm_bdisp2_config  *bc)
{
	int res;

	if (index >= STM_BDISP2_MAX_AQs)
		return -ENODEV;

	aq->bc = bc;

	aq->stdev.queue_id = index;
	aq->queue_prio = STM_BDISP2_MAX_AQs - index - 1;

	aq->stdrv.mmio_base = bc->io_base;
	aq->lna_irq_bit = (BDISP_ITS_AQ_LNA_REACHED
			   << (BDISP_ITS_AQ1_SHIFT + (index * 4)));
	aq->node_irq_bit = (BDISP_ITS_AQ_NODE_NOTIFY
			    << (BDISP_ITS_AQ1_SHIFT + (index * 4)));

	init_waitqueue_head(&aq->idle_wq);
	init_waitqueue_head(&aq->last_free_wq);
	atomic_set(&aq->n_users, 0);

	/* for userspace BDisp, we need an area of memory shared with userspace,
	   so we can do efficient interrupt handling. It shall be aligned to a
	   page boundary, and size shall be an integer multiple of PAGE_SIZE,
	   too so the mmap() implementation is easier. Since our shared area
	   consists of the actual shared area (shared between IRQ handler and
	   userspace) and the nodelist for the blitter, we want to make sure
	   it is possible to mmap() the nodelist _un_cached and the shared
	   struct _cached_. Therefore, the struct will occupy one complete
	   page.
	   In addition, because we create two virtual mappings (kernel and user
	   space) we have to make sure to avoid cache aliasing on VIVT caches.
	   Therefore, it needs to be aligned to shm_align_mask.
	   And more - on ARM we can't create dissimilar mappings (cached and
	   uncached) of the same physical address. Either userspace must be
	   fixed to take care of flushing the nodes, too, or we have to alter
	   the allocation and do everything uncached in the kernel. */
	res = stm_bdisp2_allocate_bpa2(
			_ALIGN_UP(sizeof(*aq->stdrv.bdisp_shared),
				  PAGE_SIZE),
			(2 * shm_align_mask) & ~shm_align_mask,
			true,
			&aq->shared_area);
	if (res) {
		stm_blit_printe("failed to allocate BDisp shared area\n");
		return res;
	}
	aq->stdrv.bdisp_shared = (void *) aq->shared_area.memory;

	/* nodes */
	res = stm_bdisp2_allocate_bpa2(
			NODES_SIZE,
			(2*shm_align_mask) & ~shm_align_mask,
			false,
			&aq->stdrv.bdisp_nodes);
	if (res) {
		stm_blit_printe("failed to allocate BDisp nodelist\n");
		goto out_shared;
	}

	/* initialize shared area */
	memset((void *) aq->shared_area.memory, 0xab, aq->shared_area.size);

	aq->stdrv.bdisp_shared->version = 3;
	aq->stdrv.bdisp_shared->aq_index   = aq->stdev.queue_id;
	aq->stdrv.bdisp_shared->nodes_size = aq->stdrv.bdisp_nodes.size;
	aq->stdrv.bdisp_shared->nodes_phys = aq->stdrv.bdisp_nodes.base;

	aq->stdrv.bdisp_shared->plat_blit = *plat_data;

	atomic_set(&aq->stdrv.bdisp_shared->lock, 0);
	aq->stdrv.bdisp_shared->locked_by = 0;
	blitload_init_ticks(aq->stdrv.bdisp_shared);

#ifndef CONFIG_ARCH_STI
	clk_prepare_enable(aq->bc->clk_ic);
#endif
	clk_prepare_enable(aq->bc->clk_ip);

	/* do a BDisp global soft reset on the first device */
	if (index == 0)
		bdisp2_engine_reset(&aq->stdrv, &aq->stdev);

	bdisp2_initialize(&aq->stdrv, &aq->stdev);

	stm_blit_printd(0,
			"%s @ %p: shared: v/s: %p/%zu, nodes: p/s: %.8lx/%zu\n",
			__func__, aq,
			aq->shared_area.memory, aq->shared_area.size,
			aq->stdrv.bdisp_nodes.base,
			aq->stdrv.bdisp_nodes.size);

	aq->irq = irq;
	/* not threaded for the time being */
	res = request_threaded_irq(aq->irq,
				   stm_bdisp2_aq_interrupt,
				   NULL,
				   IRQF_SHARED,
				   aq->stdrv.bdisp_shared->plat_blit.device_name,
				   aq);
	if (res < 0) {
		stm_blit_printe("Failed request_irq %d: %d!\n", aq->irq, res);
		aq->irq = 0;
		goto out_nodes;
	}

	return 0;

out_nodes:
	stm_bdisp2_free_bpa2(&aq->stdrv.bdisp_nodes);
out_shared:
	stm_bdisp2_free_bpa2(&aq->shared_area);
	aq->stdrv.bdisp_shared = NULL;
	return res;
}

void
stm_bdisp2_aq_release(struct stm_bdisp2_aq *aq)
{
	if (!aq->stdrv.bdisp_shared->bdisp_suspended)
		stm_bdisp2_aq_suspend(aq);

	bdisp2_cleanup(&aq->stdrv, &aq->stdev);

	free_irq(aq->irq, aq);
	stm_bdisp2_free_bpa2(&aq->stdrv.bdisp_nodes);
	stm_bdisp2_free_bpa2(&aq->shared_area);
	aq->stdrv.bdisp_shared = NULL;
}

#ifndef CONFIG_ARCH_STI
int
stm_bdisp2_aq_suspend(struct stm_bdisp2_aq *aq)
{
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	LOCK_ATOMIC(shared);

	/* We should stop emitting commands before sync, otherwise the
	   driver would take a long time before going to suspend as it
	   may have some other emited commands to process. */
	shared->bdisp_suspended = 1;
	stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_IDLE, NULL);

	UNLOCK_ATOMIC(shared);

	clk_disable_unprepare(aq->bc->clk_ip);
	clk_disable_unprepare(aq->bc->clk_ic);

	return 0;
}

#if defined(CONFIG_PM)
int
stm_bdisp2_aq_resume(struct stm_bdisp2_aq *aq)
{
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	clk_prepare_enable(aq->bc->clk_ic);
	clk_prepare_enable(aq->bc->clk_ip);

	/* Don't break current execution, we should wait until the
	   current execution is finished. */
	stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_IDLE, NULL);

	LOCK_ATOMIC(shared);

	shared->bdisp_suspended = 0;

	UNLOCK_ATOMIC(shared);

	return 0;
}

int
stm_bdisp2_aq_freeze(struct stm_bdisp2_aq *aq)
{
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	/* Lock the access while the blitter is suspended */
	LOCK_ATOMIC(shared);

	/* We should stop emitting commands before sync, otherwise the
	   driver would take a long time before going to suspend as it
	   may have some other emited commands to process. */
	shared->bdisp_suspended = 1;
	stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_IDLE, NULL);

	/* Disable the blitter IRQ */
	disable_irq(aq->irq);

	/* Disable the blitter clocks */
	clk_disable_unprepare(aq->bc->clk_ip);
	clk_disable_unprepare(aq->bc->clk_ic);

	return 0;
}

int
stm_bdisp2_aq_restore(struct stm_bdisp2_aq *aq)
{
	unsigned long                         next_valid_ip;
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	/* Enable bdisp2 clocks */
	clk_prepare_enable(aq->bc->clk_ic);
	clk_prepare_enable(aq->bc->clk_ip);

	/* Do a BDisp global soft reset on the first device */
	if (aq->stdev.queue_id == 0)
		bdisp2_engine_reset(&aq->stdrv, &aq->stdev);

	/* Enable the blitter IRQ */
	enable_irq(aq->irq);

	/* Set up the hardware -> point registers to right place */
	bdisp2_set_AQ_reg (&aq->stdrv, &aq->stdev,
			   BDISP_AQ_CTL,
			   (3 - shared->aq_index)
			   | (0
			      | BDISP_AQ_CTL_QUEUE_EN
			      | BDISP_AQ_CTL_EVENT_SUSPEND
			      | BDISP_AQ_CTL_IRQ_NODE_COMPLETED
			      | BDISP_AQ_CTL_IRQ_LNA_REACHED
			     )
			  );

	/* Setup the AQ iterrupt mask if needed */
	if (aq->stdev.features.hw.need_itm)
		bdisp2_set_reg (&aq->stdrv, BDISP_ITM0,
			        (BDISP_ITS_AQ_MASK << (BDISP_ITS_AQ1_SHIFT
			                               + (shared->aq_index * 4))));

	next_valid_ip = shared->next_free + shared->nodes_phys;

	/* Program AQ_IP with new next valid IP register value */
	bdisp2_set_AQ_reg (&aq->stdrv, &aq->stdev, BDISP_AQ_IP, next_valid_ip);

	/* Update the driver power state */
	shared->bdisp_suspended = 0;

	/* We can now unlock the driver */
	UNLOCK_ATOMIC(shared);

	return 0;
}
#endif /*CONFIG_PM */


#else  /* CONFIG_ARCH_STI  */
/* Below is the proposed implementation with simple suspend/resume callbacks */


int
stm_bdisp2_aq_suspend(struct stm_bdisp2_aq *aq)
{
	int res;
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	/* Lock the access while the blitter is suspended */
	LOCK_ATOMIC(shared);

	/* We should stop emitting commands before sync, otherwise the
	   driver would take a long time before going to suspend as it
	   may have some other emited commands to process. */
	shared->bdisp_suspended = 1;
	res = stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_IDLE, NULL);

	if (res != 0)
		printk(KERN_DEBUG "sync returned %d\n", res);

	/* Save AQ_IP register value to be restored after resume */
	shared->next_ip = bdisp2_get_AQ_reg (&aq->stdrv, &aq->stdev, BDISP_AQ_IP);

	/* Disable the blitter IRQ */
	disable_irq(aq->irq);

	/* Disable the blitter clocks */
	clk_disable_unprepare(aq->bc->clk_ip);

	return 0;
}

#if defined(CONFIG_PM)
int
stm_bdisp2_aq_resume(struct stm_bdisp2_aq *aq)
{
	unsigned long                         next_valid_ip;
	struct stm_bdisp2_shared_area * const shared =
		aq->stdrv.bdisp_shared;

	/* Enable bdisp2 clocks */
	clk_prepare_enable(aq->bc->clk_ip);

    /* Set clock frequency */
	if (aq->bc->clk_ip_freq) {
		/* Should not get error, as this rate has
		 * been already set at boot time */
		clk_set_rate(aq->bc->clk_ip, aq->bc->clk_ip_freq);
	}

	/* Do a BDisp global soft reset on the first device */
	if (aq->stdev.queue_id == 0)
		bdisp2_engine_reset(&aq->stdrv, &aq->stdev);

	/* Enable the blitter IRQ */
	enable_irq(aq->irq);

	/* Set up the hardware -> point registers to right place */
	bdisp2_set_AQ_reg (&aq->stdrv, &aq->stdev,
			   BDISP_AQ_CTL,
			   (3 - shared->aq_index)
			   | (0
			      | BDISP_AQ_CTL_QUEUE_EN
			      | BDISP_AQ_CTL_EVENT_SUSPEND
			      | BDISP_AQ_CTL_IRQ_NODE_COMPLETED
			      | BDISP_AQ_CTL_IRQ_LNA_REACHED
			     )
			  );

	/* Setup the AQ iterrupt mask if needed */
	if (aq->stdev.features.hw.need_itm)
		bdisp2_set_reg (&aq->stdrv, BDISP_ITM0,
			        (BDISP_ITS_AQ_MASK << (BDISP_ITS_AQ1_SHIFT
			                               + (shared->aq_index * 4))));

	next_valid_ip = shared->next_ip;

	/* Program AQ_IP with new next valid IP register value */
	bdisp2_set_AQ_reg (&aq->stdrv, &aq->stdev, BDISP_AQ_IP, next_valid_ip);

	/* Update the driver power state */
	shared->bdisp_suspended = 0;

	/* We can now unlock the driver */
	UNLOCK_ATOMIC(shared);

	return 0;
}
#endif /* CONFIG_PM */
#endif /* CONFIG_ARCH_STI */
