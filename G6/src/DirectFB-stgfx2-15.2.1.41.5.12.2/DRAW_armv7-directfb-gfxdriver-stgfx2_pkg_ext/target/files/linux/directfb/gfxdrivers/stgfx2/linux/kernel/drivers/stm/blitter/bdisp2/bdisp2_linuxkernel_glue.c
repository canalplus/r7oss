#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <linux/io.h>

#include "bdisp2/bdispII_aq.h"
#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"


#ifdef BDISP2_PRINT_NODE_WAIT_TIME
struct _my_timer_state {
	ktime_t start;
	ktime_t end;
};

void *
bdisp2_wait_space_start_timer_os(void)
{
	struct _my_timer_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state)
		state->start = ktime_get();

	return state;
}

void
bdisp2_wait_space_end_timer_os(void *handle)
{
	if (handle) {
		struct _my_timer_state * const state = handle;
		struct timeval tv;

		state->end = ktime_get();
		state->end = ktime_sub(state->end, state->start);
		tv = ktime_to_timeval(state->end);

		stm_blit_printd(BDISP_BLT,
				"BDisp/BLT: had to wait for space %lu.%06lus\n",
				tv.tv_sec, tv.tv_usec);

		kfree(state);
	}
}
#endif


void
bdisp2_wait_space_os(struct stm_bdisp2_driver_data       * const stdrv,
		     const struct stm_bdisp2_device_data * const stdev)
{
	int res;
	struct stm_bdisp2_aq * const aq
		= container_of(stdrv, struct stm_bdisp2_aq, stdrv);

	res = stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_SPACE, NULL);
	if (res != 0)
		printk(KERN_DEBUG "sync returned %d\n", res);
}


void
bdisp2_wait_fence_os(struct stm_bdisp2_driver_data       * const stdrv,
		     const struct stm_bdisp2_device_data * const stdev,
		     const stm_blitter_serial_t          * const serial)
{
	int res;
	struct stm_bdisp2_aq * const aq
		= container_of(stdrv, struct stm_bdisp2_aq, stdrv);

	res = stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_FENCE, serial);
	if (res != 0)
		printk(KERN_DEBUG "sync returned %d\n", res);
}


int
bdisp2_engine_sync_os(struct stm_bdisp2_driver_data       * const stdrv,
		      const struct stm_bdisp2_device_data * const stdev)
{
	int ret = 0;
	struct stm_bdisp2_aq * const aq
		= container_of(stdrv, struct stm_bdisp2_aq, stdrv);

	while (!bdisp2_is_idle(&aq->stdrv)) {
		ret = stm_bdisp2_aq_sync(aq, STM_BDISP2_WT_IDLE, NULL);
		if (ret >= 0)
			break;

		stm_blit_printe("BDisp/BLT: STM_BLITTER_SYNC failed!\n");

		printk(KERN_INFO
		       "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu next/last %lu/%lu\n",
		       !bdisp2_is_idle(&aq->stdrv) ? "" : "not ",
		       bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					 BDISP_AQ_CTL),
		       ((bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					   BDISP_AQ_IP)
			 - aq->stdrv.bdisp_shared->nodes_phys)
			/ sizeof(struct _BltNode_Complete)),
		       ((bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					   BDISP_AQ_LNA)
			 - aq->stdrv.bdisp_shared->nodes_phys)
			/ sizeof(struct _BltNode_Complete)),
		       ((bdisp2_get_AQ_reg(&aq->stdrv, &aq->stdev,
					   BDISP_AQ_STA)
			 - aq->stdrv.bdisp_shared->nodes_phys)
			/ sizeof(struct _BltNode_Complete)),
		       aq->stdrv.bdisp_shared->next_free,
		       aq->stdrv.bdisp_shared->last_free);

		break;
	}

	return ret;
}

int
bdisp2_alloc_memory_os(struct stm_bdisp2_driver_data * const stdrv,
		       struct stm_bdisp2_device_data * const stdev,
		       int                            alignment,
		       bool                           cached,
		       struct stm_bdisp2_dma_area    * const dma_mem)
{
	return stm_bdisp2_allocate_bpa2(dma_mem->size,
					alignment, cached, dma_mem);
}

int
bdisp2_wakeup_os(struct stm_bdisp2_driver_data * const stdrv)
{
#if defined(CONFIG_PM_RUNTIME)
	struct stm_bdisp2_aq * const aq
		= container_of(stdrv, struct stm_bdisp2_aq, stdrv);

	return pm_runtime_get_sync(aq->dev);
#else
	return 0;
#endif
}

void
bdisp2_free_memory_os(struct stm_bdisp2_driver_data * const stdrv,
		      struct stm_bdisp2_device_data * const stdev,
		      struct stm_bdisp2_dma_area    * const dma_mem)
{
	stm_bdisp2_free_bpa2(dma_mem);
}


uint64_t
bdisp2_div64_u64(uint64_t n,
		 uint64_t d)
{
	if (unlikely(d & 0xffffffff00000000ULL)) {
		printk(KERN_WARNING
		       "stm_bdispII: 64-bit/64-bit division will lose precision due to 64bit divisor (0x%llx / 0x%llx)\n",
		       n, d);
		BUG();
	}

	return div64_u64(n, d);
}
