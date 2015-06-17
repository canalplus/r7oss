#ifndef __STM_BDISPII_AQ_H__
#define __STM_BDISPII_AQ_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock_types.h>
#include <linux/bpa2.h>
#include <linux/clk.h>

#include "bdisp2/bdispII_device_features.h"
#include "bdisp2/bdispII_aq_state.h"

#define STM_BDISP2_MAX_AQs 4


#if defined(CONFIG_ARM)
#  if defined(CONFIG_CPU_V6)
#    define shm_align_mask (SHMLBA - 1)
#  else
#    define shm_align_mask (PAGE_SIZE - 1)
#  endif
#elif defined(CONFIG_MIPS)
#elif defined(CONFIG_SUPERH)
#else
#warning please check shm_align_mask for your architecture!
#define shm_align_mask (PAGE_SIZE - 1)
#endif



struct stm_bdisp2_aq {
	/* */
	unsigned int  queue_prio;

	unsigned int  lna_irq_bit;
	unsigned int  node_irq_bit;

	wait_queue_head_t idle_wq;
	wait_queue_head_t last_free_wq;

	atomic_t n_users;

	/* */
	struct stm_bdisp2_dma_area shared_area;

	struct stm_bdisp2_driver_data stdrv;
	struct stm_bdisp2_device_data stdev;

	/* */
	int irq;
	struct stm_bdisp2_config *bc;

#if defined(CONFIG_PM_RUNTIME)
    /* */
    struct device *dev;
#endif
};

struct stm_bdisp2_config {
	struct resource      *mem_region;
	void __iomem         *io_base;
	struct clk           *clk_ip;
	unsigned int          clk_ip_freq;
#ifndef CONFIG_ARCH_STI
	struct clk           *clk_ic;
#endif

	struct stm_bdisp2_aq  aq_data[STM_BDISP2_MAX_AQs];
	spinlock_t            register_lock;
};


int stm_bdisp2_aq_init(struct stm_bdisp2_aq      *aq,
		       int                        index,
		       int                        irq,
		       struct stm_plat_blit_data *plat_data,
		       struct stm_bdisp2_config  *bc);
void stm_bdisp2_aq_release(struct stm_bdisp2_aq *aq);


int stm_bdisp2_aq_suspend(struct stm_bdisp2_aq *aq);
#if defined(CONFIG_PM)
int stm_bdisp2_aq_resume(struct stm_bdisp2_aq *aq);
#ifndef CONFIG_ARCH_STI
int stm_bdisp2_aq_freeze(struct stm_bdisp2_aq *aq);
int stm_bdisp2_aq_restore(struct stm_bdisp2_aq *aq);
#endif
#endif /* CONFIG_PM */


enum stm_bdisp2_wait_type {
	STM_BDISP2_WT_IDLE,
	STM_BDISP2_WT_SPACE,
	STM_BDISP2_WT_FENCE,
};

int stm_bdisp2_aq_sync(struct stm_bdisp2_aq       *aq,
		       enum stm_bdisp2_wait_type   wait,
		       const stm_blitter_serial_t *serial);

int stm_bdisp2_allocate_bpa2(size_t                      size,
			     uint32_t                    align,
			     bool                        cached,
			     struct stm_bdisp2_dma_area *area);

void stm_bdisp2_free_bpa2(struct stm_bdisp2_dma_area *area);


#endif /* __STM_BDISPII_AQ_H__ */
