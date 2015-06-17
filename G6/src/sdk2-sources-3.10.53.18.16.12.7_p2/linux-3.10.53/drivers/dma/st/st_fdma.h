/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ST_FDMA_H__
#define __ST_FDMA_H__

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/libelf.h>

#include <linux/platform_data/dma-st-fdma.h>

#include "st_fdma_llu.h"
#include "st_fdma_regs.h"

/*
 * FDMA firmware specific
 */

#define EM_SLIM			102	/* No official SLIM ELF ID yet */
#define EF_SLIM_FDMA		2	/* ELF header e_flags indicates usage */

#define ST_FDMA_FW_NAME_LEN	23
#define ST_FDMA_FW_SEGMENTS	2

enum st_fdma_fw_state {
	ST_FDMA_FW_STATE_INIT,
	ST_FDMA_FW_STATE_LOADING,
	ST_FDMA_FW_STATE_LOADED,
	ST_FDMA_FW_STATE_ERROR
};


/*
 * FDMA request line specific
 */

#define ST_FDMA_NUM_DREQS	32

struct st_fdma_dreq_router {
	int (*route)(struct st_fdma_dreq_router *router, int input_req_line,
			int fdma, int fdma_req_line);
	struct list_head list;
	u8 xbar_id;
};

/*
 * FDMA channel specific
 */

#define ST_FDMA_MIN_CHANNEL	0
#define ST_FDMA_MAX_CHANNEL	15
#define ST_FDMA_NUM_CHANNELS	16

#define ST_FDMA_IS_CYCLIC	1
#define ST_FDMA_IS_PARKED	2

enum st_fdma_state {
	ST_FDMA_STATE_IDLE,
	ST_FDMA_STATE_RUNNING,
	ST_FDMA_STATE_STOPPING,
	ST_FDMA_STATE_PAUSED,
	ST_FDMA_STATE_ERROR
};

struct st_fdma_desc;
struct st_fdma_device;

struct st_fdma_chan {
	struct st_fdma_device *fdev;
	struct dma_chan dma_chan;

	u32 id;
	spinlock_t lock;
	unsigned long flags;
	enum st_dma_type type;
	enum st_fdma_state state;
	struct st_dma_dreq_config *dreq;
	dma_addr_t dma_addr;

	u32 desc_count;
	struct list_head desc_free;
	struct list_head desc_queue;
	struct list_head desc_active;
	struct st_fdma_desc *desc_park;

	struct tasklet_struct tasklet_complete;

	dma_cookie_t last_completed;

	void *extension;
};


/*
 * FDMA specific device structure
 */

enum {
	CLK_SLIM,
	CLK_HI,
	CLK_LOW,
	CLK_IC,
	CLK_MAX_NUM,
};
struct st_fdma_device {
	struct platform_device *pdev;
	struct device *dev;
	struct dma_device dma_device;
	u32 fdma_id;

	struct clk *clks[CLK_MAX_NUM];
	struct resource *io_res;
	void __iomem *io_base;
	spinlock_t lock;
	spinlock_t irq_lock;

	const char *fw_name;
	enum st_fdma_fw_state fw_state;
	wait_queue_head_t fw_load_q;

	struct st_fdma_chan ch_list[ST_FDMA_NUM_CHANNELS];

	spinlock_t dreq_lock;
	struct st_dma_dreq_config dreq_list[ST_FDMA_NUM_DREQS];
	u32 dreq_mask;

	struct dma_pool *dma_pool;

	struct st_plat_fdma_hw *hw;
	struct st_plat_fdma_fw_regs *fw;
	u8 xbar;

	struct st_fdma_regs regs;

#ifdef CONFIG_DEBUG_FS
	/* debugfs */
	struct dentry *debug_dir;
	struct dentry *debug_regs;
	struct dentry *debug_dmem;
	struct dentry *debug_chans[ST_FDMA_NUM_CHANNELS];
#endif
};


/*
 * FDMA descriptor specific
 */

#define ST_FDMA_DESCRIPTORS	32

struct st_fdma_desc {
	struct list_head node;

	struct st_fdma_chan *fchan;

	struct st_fdma_llu *llu;
	struct list_head llu_list;

	struct dma_async_tx_descriptor dma_desc;

	void *extension;
};

/*
 * Type functions
 */
static inline struct st_fdma_chan *to_st_fdma_chan(struct dma_chan *chan)
{
	BUG_ON(!chan);
	return container_of(chan, struct st_fdma_chan, dma_chan);
}

static inline struct st_fdma_device *to_st_fdma_device(struct dma_device *dev)
{
	BUG_ON(!dev);
	return container_of(dev, struct st_fdma_device, dma_device);
}

static inline struct st_fdma_desc *to_st_fdma_desc(
		struct dma_async_tx_descriptor *desc)
{
	BUG_ON(!desc);
	return container_of(desc, struct st_fdma_desc, dma_desc);
}

/*
 * Function prototypes
 */
#ifdef CONFIG_DEBUG_FS

void st_fdma_debugfs_init(void);
void st_fdma_debugfs_exit(void);
void st_fdma_debugfs_register(struct st_fdma_device *fdev);
void st_fdma_debugfs_unregister(struct st_fdma_device *fdev);

#else

#define st_fdma_debugfs_init()		do { } while (0)
#define st_fdma_debugfs_exit()		do { } while (0)
#define st_fdma_debugfs_register(a)	do { } while (0)
#define st_fdma_debugfs_unregister(a)	do { } while (0)

#endif


dma_cookie_t st_fdma_tx_submit(struct dma_async_tx_descriptor *desc);

struct st_fdma_desc *st_fdma_desc_alloc(struct st_fdma_chan *fchan);
void st_fdma_desc_free(struct st_fdma_desc *fdesc);
struct st_fdma_desc *st_fdma_desc_get(struct st_fdma_chan *fchan);
void st_fdma_desc_put(struct st_fdma_desc *fdesc);
void st_fdma_desc_chain(struct st_fdma_desc **head,
		struct st_fdma_desc **prev, struct st_fdma_desc *fdesc);
void st_fdma_desc_start(struct st_fdma_chan *fchan);
void st_fdma_desc_unmap_buffers(struct st_fdma_desc *fdesc);
void st_fdma_desc_complete(unsigned long data);

int st_fdma_register_dreq_router(struct st_fdma_dreq_router *router);
void st_fdma_unregister_dreq_router(struct st_fdma_dreq_router *router);
struct st_dma_dreq_config *st_fdma_dreq_alloc(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config);
void st_fdma_dreq_free(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config);
int st_fdma_dreq_config(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config);


int st_fdma_fw_check(struct st_fdma_device *fdev);
int st_fdma_fw_load(struct st_fdma_device *fdev, struct ELF32_info *elfinfo);

void st_fdma_hw_enable(struct st_fdma_device *fdev);
void st_fdma_hw_disable(struct st_fdma_device *fdev);
void st_fdma_hw_get_revisions(struct st_fdma_device *fdev,
		int *hw_major, int *hw_minor, int *fw_major, int *fw_minor);
void st_fdma_hw_channel_reset(struct st_fdma_chan *fchan);
int st_fdma_hw_channel_enable_all(struct st_fdma_device *fdev);
int st_fdma_hw_channel_disable_all(struct st_fdma_device *fdev);
int st_fdma_hw_channel_set_dreq(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config);
void st_fdma_hw_channel_start(struct st_fdma_chan *fchan,
		struct st_fdma_desc *fdesc);
void st_fdma_hw_channel_pause(struct st_fdma_chan *fchan, int flush);
void st_fdma_hw_channel_resume(struct st_fdma_chan *fchan);
void st_fdma_hw_channel_switch(struct st_fdma_chan *fchan,
		struct st_fdma_desc *fdesc, struct st_fdma_desc *tdesc,
		int ioc);
int st_fdma_hw_channel_status(struct st_fdma_chan *fchan);
int st_fdma_hw_channel_error(struct st_fdma_chan *fchan);

/*
 * Extension API function prototypes
 */
#ifdef CONFIG_ST_FDMA_AUDIO
int st_fdma_audio_alloc_chan_resources(struct st_fdma_chan *fchan);
void st_fdma_audio_free_chan_resources(struct st_fdma_chan *fchan);
#else
#define st_fdma_audio_alloc_chan_resources(f)	0
#define st_fdma_audio_free_chan_resources(f)	do { } while (0)
#endif

#ifdef CONFIG_ST_FDMA_TELSS
int st_fdma_telss_alloc_chan_resources(struct st_fdma_chan *fchan);
void st_fdma_telss_free_chan_resources(struct st_fdma_chan *fchan);
#else
#define st_fdma_telss_alloc_chan_resources(f)	0
#define st_fdma_telss_free_chan_resources(f)	do { } while (0)
#endif

#ifdef CONFIG_ST_FDMA_MCHI
int st_fdma_mchi_alloc_chan_resources(struct st_fdma_chan *fchan);
void st_fdma_mchi_free_chan_resources(struct st_fdma_chan *fchan);
#else
#define st_fdma_mchi_alloc_chan_resources(f)	0
#define st_fdma_mchi_free_chan_resources(f)	do { } while (0)
#endif

#endif /* __ST_FDMA_H__ */
