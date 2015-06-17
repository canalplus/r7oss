#ifndef __BDISP2_OS_LINUXKERNEL_H__
#define __BDISP2_OS_LINUXKERNEL_H__

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>

#define STM_BLITTER_DEBUG_DOMAIN(domain, str, desc) \
		static int __maybe_unused domain

#define THIS_FILE ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

#ifndef STM_BLITTER_ASSERT
#  define STM_BLITTER_ASSERT(cond) BUG_ON(!(cond))
#endif
#define STM_BLITTER_ASSUME(cond) WARN_ON(!(cond))
#define stm_blit_printw(format, args...) \
		printk(KERN_WARNING \
		       "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_blit_printe(format, args...) \
		printk(KERN_ERR "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_blit_printi(format, args...) \
		printk(KERN_INFO "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#ifdef DEBUG
#define stm_blit_printd(domain, format, args...) \
		printk(KERN_DEBUG "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#else
#define stm_blit_printd(domain, format, args...) do { } while (0)
/*
static inline int __printf(2, 3)
stm_blit_printd(int level, const char *format, ...)
{
	return 0;
}
*/
#endif


#define STM_BLITTER_N_ELEMENTS(array) ARRAY_SIZE(array)
#define STM_BLITTER_ABS(val)          abs(val)
#define STM_BLITTER_MIN(a, b)         min(a, b)

#define STM_BLITTER_UDELAY(usec)                  udelay(usec)
#define STM_BLITTER_RAW_WRITEL(val, addr)         __raw_writel(val, addr)
#define STM_BLITTER_MEMSET_IO(dst, val, size)     memset_io(dst, val, size)
#define STM_BLITTER_MEMCPY_TO_IO(dst, src, size)  memcpy_toio(dst, src, size)


static inline void
flush_bdisp2_dma_area_region(struct stm_bdisp2_dma_area *area,
			     unsigned                    offset,
			     unsigned                    size)
{
	if (!area->cached)
		return;

	/* We only need to flush the cache for the node lists. They should
	 * always be mapped write combined for the ARM, so we do not need to
	 * flush them, and it is a bug if we do as the function does not exist
	 * on the ARM
	 */
#ifdef CONFIG_SH
	flush_ioremap_region(area->base, area->memory, offset, size);
#else
	BUG();
#endif
}

static inline void flush_bdisp2_dma_area(struct stm_bdisp2_dma_area *area)
{
	flush_bdisp2_dma_area_region(area, 0, area->size);
}


uint64_t
bdisp2_div64_u64 (uint64_t n,
		  uint64_t d);

/*
 * Using udelay(1) to acheive the wait allows high response once the
 * lock is freed. But is keeping CPU active is a busy wait. This works
 * fine with normal thread, except keeping CPU busy for nothing.
 *
 * For RT thread like Scaler, this results in RT thread being blocked
 * in the busy wait and monopolizing the CPU usage blocking event the
 * lock holder from releasing it.
 *
 * Away to avoid that is to use "non-busy" wait by using usleep_range
 * function as replacement. Still usleep_range has a draw-back of may
 * generate an interruption and could results in performance drop duo
 * to high number of introduced frequency.
 *
 * Best approach will be to use udelay(1) for normal thread, as today,
 * and use usleep_range for RT thread to allow other thread to proceed.
 * Unfortunately, from kernel module there is no viable wayto detect
 * current thread scheduling priority.
 *
 * To bypass this limitation, wait for 10 time udelay(1). If we are
 * still there means propably we are in a real time thread, so use
 * usleep_range(1,20), to give time to lock holder to release it. 20 is
 * enough big to prepare more than 6 nodes, so lock will propably be
 * unlocked. And not so big to avoid RT thread performance penalty.
 */

#  define LOCK_ATOMIC_RPM(stdrv) \
	({ \
		int wait_counter=10; \
		while (unlikely(atomic_cmpxchg(&((stdrv)->bdisp_shared->lock), 0, 1) != 0))   \
		{ \
			if ( (stdrv)->bdisp_shared->bdisp_suspended == 2) \
				bdisp2_wakeup_os((stdrv)); \
			if (wait_counter-->=0) \
				udelay(1); \
			else \
				usleep_range(1,20); \
		} \
		if (unlikely((stdrv)->bdisp_shared->locked_by))                               \
			printk(KERN_WARNING                                 \
			       "locked an already locked lock by %x\n",     \
			       (stdrv)->bdisp_shared->locked_by);                             \
		(stdrv)->bdisp_shared->locked_by = 3;                                         \
	})

#  define LOCK_ATOMIC(s) \
	({ \
		int wait_counter=10; \
		while (unlikely(atomic_cmpxchg(&((s)->lock), 0, 1) != 0))   \
		{ \
			if (wait_counter-->=0) \
				udelay(1); \
			else \
				usleep_range(1,20); \
		} \
		if (unlikely((s)->locked_by))                               \
			printk(KERN_WARNING                                 \
			       "locked an already locked lock by %x\n",     \
			       (s)->locked_by);                             \
		(s)->locked_by = 3;                                         \
	})

#  define UNLOCK_ATOMIC(s) \
	({ \
		if (unlikely((s)->locked_by != 3))                          \
			printk(KERN_WARNING                                 \
			       "shared->locked_by error (%x)!\n",           \
			       (s)->locked_by);                             \
		(s)->locked_by = 0;                                         \
		smp_wmb(); /* update locked_by before releasing lock */     \
		atomic_set(&((s)->lock), 0);                                \
	})

#  define BDISP2_WMB() smp_wmb()
#  define BDISP2_MB()  smp_mb()

#define bdisp2_start_timer_os() \
	({ \
		ktime_to_us(ktime_get()); \
	})

#include <linux/io.h>

static inline uint32_t
bdisp2_get_reg(const struct stm_bdisp2_driver_data * const stdrv,
	       uint16_t                             offset)
{
	return readl(stdrv->mmio_base + 0xa00 + offset);
}

static inline void
bdisp2_set_reg(struct stm_bdisp2_driver_data * const stdrv,
	       uint16_t                       offset,
	       uint32_t                       value)
{
/*
	__raw_writel(value, stdrv->mmio_base + 0xa00 + offset);
*/
	writel(value, stdrv->mmio_base + 0xa00 + offset);
}

int
bdisp2_wakeup_os (struct stm_bdisp2_driver_data * const stdrv);

#endif /* __BDISP2_OS_LINUXKERNEL_H__ */
