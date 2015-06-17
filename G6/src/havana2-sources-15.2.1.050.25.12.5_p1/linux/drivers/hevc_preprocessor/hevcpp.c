/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include "osdev_time.h"
#include "osdev_sched.h"
#include "osdev_cache.h"
#include "osdev_device.h"

#include "hevcppio.h"
#include "hevcpp.h"

#define MAJOR_NUMBER    247

MODULE_DESCRIPTION("Hevc pre-processor platform driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

OSDEV_ThreadDesc_t hevc_thread_desc = { "SE-Vid-Hevcpp", -1, -1 };
static int thpolprio_inits[2] = {SCHED_RR, OSDEV_MID_PRIORITY };
module_param_array_named(thread_SE_Vid_Hevcpp, thpolprio_inits, int, NULL, S_IRUGO);
MODULE_PARM_DESC(thread_SE_Vid_Hevcpp, "hevcpp thread parameters: policy,priority");

/* How long we should wait for PP to complete single frame/field preprocessing before we give up */
#define HEVCPP_TIMEOUT_MS    200
/* Size of sw task fifo (per instance)
   must be >= 2 to optimize PP usage */
#define HEVCPP_TASK_FIFO_SIZE 4

//TODO to be removed after kernel 3.10 move
#ifndef CONFIG_ARCH_STI
#define TMP_FIX_DT
#endif

/**
 * Task descriptor
 * @InParam: input parameters
 * @OutParam: output parameters
 * @OutputSize: output buffer size
 * @DecodeTime: task decoding time
 * @hw_ended:  task completion in HW
 * @completed: task full completion
 * @core: core where the task has been processed
 *        if 0 -> termination signal for end of task thread
 * @id: informative field
 */
struct HevcppTask {
	struct hevcpp_ioctl_queue_t    InParam;
	struct hevcpp_ioctl_dequeue_t  OutParam;

	unsigned int            OutputSize;
	unsigned int            DecodeTime;
	struct completion       hw_ended;
	struct completion       completed;
	struct HevcppCore       *core;
	unsigned int            id;
};

/**
 * Main struct for 1 PP core
 * @name: name of core
 * @clk: its clock
 * @clk_lock: mutex lock on clk_ref_count
 * @clk_ref_count: ref counter on clock to call core initialisation
 * @hw_lock: lock on reg writes for current core
 * @irq_its: end of task IRQ
 * @hw_base_addr: remapped base address of core
 * @regs_size: size of IO ressource

 * @inuse: PP core is currently used
 * @current_task: pointer to current task processed in core
 * @last_mb_adaptive_frame_field_flag: mb_adaptive_frame_field_flag backup
 * @need_soft_reset: force soft reset
 */
struct HevcppCore {
	char              name[16];
	struct clk        *clk;
	int               clk_ref_count;
	spinlock_t        clk_lock;
	struct mutex      hw_lock;
	u32               irq_its;
	void __iomem      *hw_base_addr;
	void __iomem      *hw_hades_addr;
	int               regs_size;
	bool              inuse;
	struct HevcppTask *current_task;
	bool              need_soft_reset;
};

/**
 * Main struct for HEVCPP multi-core
 * @dev: device pointer
 * @pp_nb: number of core
 * @pp_unactive_lock: semaphore dedicated to *unactive* core number
 * @core: array of cores
 * @core_lock: protect access to core array
 * @instance_nb: client (open) number
 */
struct Hevcpp {
	struct device      *dev;
	int                pp_nb;
	struct semaphore   pp_unactive_lock;
	struct HevcppCore  *core;
	spinlock_t         core_lock;
	atomic_t           instance_nb;
	unsigned int       stbus_max_opcode_size;
	unsigned int       stbus_max_chunk_size;
	unsigned int       stbus_max_msg_size;
};

/**
 * Open context -> client instance
 * @data: private data used to store pointer to Hevcpp device
 * @fifo_task_lock: lock on fifo task variables
 * @task: task fifo
 * @task_wq: wait queue on task fifo availability
 * @input_task_idx: write index in task fifo
 * @hw_done_task_idx: read index in task fifo for task completed by HW
 * @output_task_idx: read index in task fifo for task fully completed
 * @pending_task_nb: task nb in fifo
 * @in_progress_task_nb:  number of task needing postprocessing
 * @current_task_nb: total task number pre-processed (used for task->id field)
 * @task_completed_thread: perform (asap) required actions at the end of one task
 */
struct HevcppOpenContext {
	void               *data;
	spinlock_t         fifo_task_lock;
	struct HevcppTask  task[HEVCPP_TASK_FIFO_SIZE];
	wait_queue_head_t  task_wq;
	unsigned int       input_task_idx;
	unsigned int       hw_done_task_idx;
	unsigned int       output_task_idx;
	atomic_t           pending_task_nb;
	atomic_t           in_progress_task_nb;
	unsigned int       current_task_nb;
	OSDEV_Thread_t     task_completed_thread;
};

/**
 * Pointer to hevcpp global struct. Should be accessible in open/close/... functions
 * (OSDEV_*Entrypoint abstraction prevents or masks access to struct file* which
 * can be used to retrieve pointer to hevcpp, and then remove this global variable)
 */
static struct Hevcpp *hevcpp;

/**
 * Initialise one core (i.e. resets registers to default value)
 */
static void HevcppInitCore(struct HevcppCore *core)
{
	BUG_ON(!core);

	mutex_lock(&core->hw_lock);
	if (core->need_soft_reset) {
		writel(1, (volatile void *)(core->hw_base_addr + HEVCPP_RESET));
	}
	mutex_unlock(&core->hw_lock);
}

/**
 *
 */
static void HevcppInitPlugs(struct HevcppCore *core)
{
	int j = 0;
	int retry = 0;
	uint32_t word;

	BUG_ON(!core);

	// config PP plug
	// no write posting in PP
	writel(1, (volatile void *)(core->hw_hades_addr + HEVCPP_PLUG_REG_G2)); // unlock plug

	retry = 0;
	while (retry < 10) {
		word = readl((volatile void *)(core->hw_hades_addr + HEVCPP_PLUG_REG_G3)); // PP-Wr plug IDLE register

		if (word) {
			break;
		}
		OSDEV_SleepMilliSeconds(1);
		retry ++;
		pr_info("%s - Write plug still busy (base@(0x%x)+0x%x=0x%x)\n",
		        __func__, (uint32_t)core->hw_hades_addr, HEVCPP_PLUG_REG_G3, word);
	}

	dev_dbg(hevcpp->dev, "%s - configuring PP Write Plug to %#08x\n", __func__, word);
	word = readl((volatile void *)(core->hw_hades_addr + HEVCPP_PLUG_REG_G1)); // PP-Wr plug
	word &= 0xFFFEFFFF; // no write posting
	writel(word, (volatile void *)(core->hw_hades_addr + HEVCPP_PLUG_REG_G1));

	writel(0, (volatile void *)(core->hw_hades_addr + HEVCPP_PLUG_REG_G2)); // relock plug

	// config PP read plug
	writel(1, (volatile void *)(core->hw_hades_addr + HEVCPP_RD_PLUG_REG_G2)); // unlock plug

	retry = 0;
	while (retry < 10) {
		word = readl((volatile void *)(core->hw_hades_addr + HEVCPP_RD_PLUG_REG_G3)); // PP-Re plug IDLE register
		if (word) {
			break;
		}
		OSDEV_SleepMilliSeconds(1);
		retry ++;
		pr_info("%s - Read plug still busy (base@(0x%x)+0x%x=0x%x)\n",
		        __func__, (uint32_t)core->hw_hades_addr, HEVCPP_RD_PLUG_REG_G3, word);
	};

	for (j = 0; j < 16; j++)
		writel((HEVCPP_MAX_CHUNK_SIZE | HEVCPP_MAX_OPCODE_SIZE),
		       (volatile void *)(core->hw_hades_addr + HEVCPP_RD_PLUG_REG_C1 + 16 * j));

	writel(0, (volatile void *)(core->hw_hades_addr + HEVCPP_RD_PLUG_REG_G2)); // relock plug
}

static int HevcppClockSetRate(struct device_node *of_node)
{
	int ret = 0;
	unsigned int max_freq = 0;
	if (!of_property_read_u32(of_node, "clock-frequency", &max_freq)) {
		/* check incase 0 is set for max frequency property in DT */
		if (max_freq) {
			ret = clk_set_rate(hevcpp->core[0].clk, max_freq);
			if (ret) {
				pr_err("Error: %s Failed to set max frequency for hevcpp clock\n", __func__);
				ret = -EINVAL;
			}
		}
	}
	pr_info("ret value : 0x%x max frequency value : %u\n", (unsigned int)ret, max_freq);
	return ret;
}

/**
 * Disable clock of one core
 */
void HevcppClockOff(struct HevcppCore *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->clk_lock, flags);
#ifndef TMP_FIX_DT
	clk_disable(core->clk);
#endif
	core->clk_ref_count--;
	spin_unlock_irqrestore(&core->clk_lock, flags);
}

/**
 * Enable clock of one core and reinit its registers if clk_ref_count==0
 */
int HevcppClockOn(struct HevcppCore *core)
{
	bool need_reinit = false;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&core->clk_lock, flags);
	//TODO To be restored once moved to kernel 3.10 (!!!assumes clock is on)
#ifndef TMP_FIX_DT
	ret = clk_enable(core->clk);
#endif
	if (!ret) {
		core->clk_ref_count++;
		if (1 == core->clk_ref_count) {
			need_reinit = true;
		}
	}
	spin_unlock_irqrestore(&core->clk_lock, flags);
	if (need_reinit) {
		HevcppInitCore(core);
	}
	return ret;
}

/**
 * Adds and returns one task to SW fifo
 * May blocks on task fifo fullness
 * @core on which core task is added
 * @instance pointer to instance
 * @param pointer to input parameter
 */
struct HevcppTask *HevcppPushTask(struct HevcppOpenContext *instance,
                                  struct hevcpp_ioctl_queue_t *p)
{
	unsigned int write_idx = 0;
	struct HevcppTask *task;
	unsigned long flags;
	int ret = 0;

	/* Ensures that we can store task in FIFO
	   No timeout as it will depends on codec buffer availability -> unpredictible */
	ret = wait_event_interruptible(
	              instance->task_wq,
	              HEVCPP_TASK_FIFO_SIZE > atomic_read(&instance->pending_task_nb));

	if (ret < 0) {
		pr_err("Error: %s push task in fifo interrupted (instance:%p)", __func__, instance);
		return NULL;
	}

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	write_idx = instance->input_task_idx;
	task = &instance->task[write_idx];
	memset(task, 0, sizeof(*task));
	init_completion(&task->hw_ended);
	init_completion(&task->completed);

	task->id = ++instance->current_task_nb;
	if (p) {
		task->InParam = *p;
	} else {
		/* Fake task to terminate end of task thread */
		task->core = 0;
	}

	atomic_inc(&instance->pending_task_nb);
	atomic_inc(&instance->in_progress_task_nb);
	instance->input_task_idx++;
	if (instance->input_task_idx == HEVCPP_TASK_FIFO_SIZE) {
		instance->input_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	wake_up_interruptible(&instance->task_wq);

	return task;
}

/**
 * Returns hw_done_task_idx task of SW fifo
 * May blocks on hw task completion
 * @instance pointer to instance
 * if successful returns 0, else <0
 */
int HevcppGetEndedTask(struct HevcppOpenContext *instance, struct HevcppTask **t)
{
	struct HevcppTask *task = NULL;
	unsigned int read_idx = 0;
	unsigned long flags;
	int ret = 0;

	/**
	 * Ensures one task is available in FIFO
	 * (prevent from blocking on uninitialized task completion)
	 */
	ret = wait_event_interruptible(instance->task_wq,
	                               atomic_read(&instance->in_progress_task_nb));

	if (ret < 0) {
		pr_err("Error: %s get task from fifo interrupted (instance:%p)", __func__, instance);
		goto bail;
	}

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	read_idx = instance->hw_done_task_idx;
	task = &instance->task[read_idx];
	atomic_dec(&instance->in_progress_task_nb);
	instance->hw_done_task_idx++;
	if (instance->hw_done_task_idx == HEVCPP_TASK_FIFO_SIZE) {
		instance->hw_done_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);
	//pr_info("%s wait for task %d\n", __func__, task->id);

	/* Waiting for task completion */
	if (!wait_for_completion_timeout
	    (&task->hw_ended, msecs_to_jiffies(HEVCPP_TIMEOUT_MS))) {
		pr_info("%s: Wait for task %d: %p -> TIMEOUT\n",
		        __func__, task->id, task);
		task->core->need_soft_reset = true;
		task->OutParam.hwStatus = hevcpp_timeout;
		ret = -EINVAL;
	}

bail:
	*t = task;
	return ret;
}

/**
 * Remove and returns one task of SW fifo
 * May blocks on task completion
 * @instance pointer to instance
 * if successful returns 0, else <0
 */
int HevcppPopTask(struct HevcppOpenContext *instance, struct HevcppTask *t)
{
	unsigned int read_idx = 0;
	struct HevcppTask *task;
	unsigned long flags;
	int ret = 0;
	bool taskTimeout = false;

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	read_idx = instance->output_task_idx;
	task = &instance->task[read_idx];
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	//pr_info("%s wait for task %d\n", __func__, task->id);

	/* Waiting for task completion */
	if (!wait_for_completion_timeout
	    (&task->completed, msecs_to_jiffies(HEVCPP_TIMEOUT_MS))) {
		pr_info("Hevcpp: %s wait for task %d: %p -> TIMEOUT\n",
		        __func__, task->id, task);
		taskTimeout = true;
		ret = -EINVAL;
	}

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	if (taskTimeout) {
		task->core->need_soft_reset = true;
		task->OutParam.hwStatus = hevcpp_timeout;
	}
	atomic_dec(&instance->pending_task_nb);
	instance->output_task_idx++;
	if (instance->output_task_idx == HEVCPP_TASK_FIFO_SIZE) {
		instance->output_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	/* memcpy is NEEDED as task[i] may be overwritten as soon as task_wq event is raised ! */
	*t = *task;
	wake_up_interruptible(&instance->task_wq);

	return ret;
}

/**
 *
 */
void HevcppCheckTaskStatus(struct HevcppTask *task)
{
	unsigned int now = OSDEV_GetTimeInMilliSeconds();

	//if( task->StsReg.pp_its & PP_ITM__DMA_CMP )
	//task->OutputSize = (task->StsReg.pp_wdl - task->StsReg.pp_isbg);
	//else
	//task->OutputSize = 0;

	task->DecodeTime = now - task->DecodeTime;
	// TODO
	//out_param->OutputSize = task->OutputSize;
}

/**
 * task_completion thread catches the task completion and deals with HW
 * operations (clock/power).
 */
OSDEV_ThreadEntrypoint(task_completion)
{
	struct HevcppOpenContext *instance = (struct HevcppOpenContext *)Parameter;
	struct HevcppTask *task;
	struct HevcppCore *core;
	unsigned long flags;
	struct Hevcpp *hevcpp = (struct Hevcpp *)instance->data;
	int ret = 0;

	/**
	 * We are holding a reference to the task_struct, and need it later
	 * during close. Make sure that it doesn't go away prematurely (due
	 * to the thread exiting while or before we have reached
	 * kthread_stop() later).
	 */
	get_task_struct(current);
	do {
		/* Block until one task has been completed by HW */
		ret = HevcppGetEndedTask(instance, &task);
		if (!task) {
			continue;
		}
		/* Termination signal */
		if (!task->core) {
			complete(&task->completed);
			break;
		}
		core = task->core;
		BUG_ON(!core);
		if (ret) {
			core->need_soft_reset = true;
		}

		HevcppClockOff(core);
		/** pm_runtime_put -> decrement pm ref_counter and if == 0 calls HevcppPmRuntimeSuspend */
		pm_runtime_mark_last_busy(hevcpp->dev);
		pm_runtime_put(hevcpp->dev);
		HevcppCheckTaskStatus(task);
		//pr_info("%s  HevcppCheckStatus task %d\n", __func__, task->id);
		/* Up semaphore as soon as possible : to start new task */
		spin_lock_irqsave(&hevcpp->core_lock, flags);
		core->inuse = false;
		spin_unlock_irqrestore(&hevcpp->core_lock, flags);
		up(&hevcpp->pp_unactive_lock);
		complete(&task->completed);
	} while (1);

	pr_info("%s TerminateThread (%p)\n", __func__, instance);
}

/**
* Open function creates a new internal instance
*/
static OSDEV_OpenEntrypoint(HevcppOpen)
{
	struct HevcppOpenContext *instance;
	OSDEV_Status_t status = OSDEV_NoError;

	OSDEV_OpenEntry();

	BUG_ON(!hevcpp);

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance) {
		pr_err("Error: %s Unable to allocate instance\n", __func__);
		OSDEV_OpenExit(OSDEV_Error);
	}

	spin_lock_init(&instance->fifo_task_lock);

	init_waitqueue_head(&instance->task_wq);
	instance->data = hevcpp;
	OSDEV_PrivateData = instance;

	atomic_inc(&hevcpp->instance_nb);

	status = OSDEV_CreateThread(&instance->task_completed_thread, task_completion, instance, &hevc_thread_desc);
	if (status != OSDEV_NoError) {
		pr_err("Error: %s Failed to create thread for instance (%p)\n", __func__, instance);
		goto bail;
	}
	OSDEV_OpenExit(OSDEV_NoError);

bail:
	kfree(instance);
	OSDEV_PrivateData = NULL;
	OSDEV_OpenExit(OSDEV_Error);
}

/**
* Close function removes instance after ensuring no task is pending
*/
static OSDEV_CloseEntrypoint(HevcppClose)
{
	struct HevcppOpenContext *instance;
	struct HevcppTask task;
	struct HevcppTask *t;

	OSDEV_CloseEntry();

	BUG_ON(!hevcpp);

	instance = (struct HevcppOpenContext *)OSDEV_PrivateData;

	/* Terminate end of task thread */
	t = HevcppPushTask(instance, 0);
	if (t) {
		complete(&t->hw_ended);
		pr_info("%s complete termination task instance (%p)\n", __func__, instance);
	}

	/* Remove pending tasks */
	while (atomic_read(&instance->pending_task_nb)) {
		HevcppPopTask(instance, &task);
	}

	kthread_stop(instance->task_completed_thread);
	put_task_struct(instance->task_completed_thread);

	atomic_dec(&hevcpp->instance_nb);
	kfree(instance);
	OSDEV_PrivateData = NULL;

	OSDEV_CloseExit(OSDEV_NoError);
}

/**
 * Program PP registers
 */
void HevcppTaskSet(struct HevcppCore *core, hevcdecpreproc_transform_param_t *cmd)
{
	int width_computed = 0;
	int height_computed = 0;
	int tile_col_end_prev = -1;
	int tile_row_end_prev = -1;
	uint32_t word;

	word = 0;
	word |= ((cmd->pic_width_in_luma_samples) & 0xffff);
	word |= (((cmd->pic_height_in_luma_samples) & 0xffff) << 16);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_PICTURE_SIZE));

	word = 0;
	word |= ((cmd->chroma_format_idc) & 0x3);
	word |= (((cmd->separate_colour_plane_flag) & 0x1) << 2);
	word |= (((cmd->log2_min_coding_block_size_minus3) & 0x3) << 8);
	word |= (((cmd->log2_diff_max_min_coding_block_size) & 0x3) << 10);
	word |= (((cmd->amp_enabled_flag) & 0x1) << 16);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_CONFIG));

	word = 0;
	word |= ((cmd->tiles_enabled_flag) & 0x1);
	word |= (((cmd->entropy_coding_sync_enabled_flag) & 0x1) << 1);
	word |= (((cmd->num_tile_columns) & 0x1F) << 8);
	word |= (((cmd->num_tile_rows) & 0x1F) << 16);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_PARALLEL_CONFIG));

#define TILE_COL_END(i, reg) \
	word = 0; \
	width_computed = cmd->tile_column_width[(i) * 2]; \
	word |= ((width_computed + tile_col_end_prev) & 0xffff ); \
	tile_col_end_prev = (word & 0xffff) ; \
	width_computed = cmd->tile_column_width[(i) * 2 + 1]; \
	word |= (((width_computed + tile_col_end_prev) & 0xffff )<< 16); \
	tile_col_end_prev = (word >> 16) & 0xffff ; \
	writel(word, (volatile void *)(core->hw_base_addr + reg));

	TILE_COL_END(0, HEVCPP_TILE_COL_END_0);
	TILE_COL_END(1, HEVCPP_TILE_COL_END_1);
	TILE_COL_END(2, HEVCPP_TILE_COL_END_2);
	TILE_COL_END(3, HEVCPP_TILE_COL_END_3);
	TILE_COL_END(4, HEVCPP_TILE_COL_END_4);

#define TILE_ROW_END_0(i, reg) \
	word = 0; \
	height_computed = cmd->tile_row_height[(i)*2]; \
	word |= ((height_computed + tile_row_end_prev) & 0xffff); \
	tile_row_end_prev = (word & 0xffff) ; \
	writel(word, (volatile void *)(core->hw_base_addr + reg));

#define TILE_ROW_END_16(i, reg) \
	height_computed = cmd->tile_row_height[(i)*2+1]; \
	word |= (((height_computed + tile_row_end_prev) & 0xffff ) << 16); \
	tile_row_end_prev = (word >> 16) & 0xffff ; \
	writel(word, (volatile void *)(core->hw_base_addr + reg));

#define TILE_ROW_END(i, reg) TILE_ROW_END_0 (i, reg); TILE_ROW_END_16(i, reg);

	TILE_ROW_END(0, HEVCPP_TILE_ROW_END_0);
	TILE_ROW_END(1, HEVCPP_TILE_ROW_END_1);
	TILE_ROW_END(2, HEVCPP_TILE_ROW_END_2);
	TILE_ROW_END(3, HEVCPP_TILE_ROW_END_3);
	TILE_ROW_END(4, HEVCPP_TILE_ROW_END_4);
	TILE_ROW_END_0(5, HEVCPP_TILE_ROW_END_5);

	word = 0;
	word |= ((cmd->dependent_slice_segments_enabled_flag) & 0x1);
	word |= (((cmd->output_flag_present_flag) & 0x1) << 1);
	word |= (((cmd->cabac_init_present_flag) & 0x1) << 2);
	word |= (((cmd->slice_segment_header_extension_flag) & 0x1) << 3);
	word |= (((cmd->log2_max_pic_order_cnt_lsb_minus4) & 0xF) << 8);
	word |= (((cmd->weighted_pred_flag) & 0x1) << 16);
	word |= (((cmd->weighted_bipred_flag) & 0x1) << 17);
	word |= (((cmd->sps_temporal_mvp_enabled_flag) & 0x1) << 18);
	word |= (((cmd->num_extra_slice_header_bits) & 0x7) << 19);
	word |= (((cmd->pps_slice_chroma_qp_offsets_present_flag) & 0x1) << 22);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_SLICE_CONFIG));

	word = 0;
	word |= ((cmd->num_ref_idx_l0_default_active_minus1) & 0xF);
	word |= (((cmd->num_ref_idx_l1_default_active_minus1) & 0xF) << 4);
	word |= (((cmd->long_term_ref_pics_present_flag) & 0x1) << 8);
	word |= (((cmd->num_long_term_ref_pics_sps) & 0x3F) << 9);
	word |= (((cmd->lists_modification_present_flag) & 0x1) << 15);
	word |= (((cmd->num_poc_total_curr) & 0xF) << 16);
	word |= (((cmd->num_short_term_ref_pic_sets) & 0x7F) << 24);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_REF_LIST_CONFIG));

#define NUM_DELTA_POC(i, reg) \
	word = 0; \
	word |= ((cmd->num_delta_pocs[(i) * 4 + 0]) & 0x1f ); \
	word |= (((cmd->num_delta_pocs[(i) * 4 + 1]) & 0x1f )<< 8); \
	word |= (((cmd->num_delta_pocs[(i) * 4 + 2]) & 0x1f )<< 16); \
	word |= (((cmd->num_delta_pocs[(i) * 4 + 3]) & 0x1f )<< 24); \
	writel(word, (volatile void *)(core->hw_base_addr + reg));

	NUM_DELTA_POC(0, HEVCPP_NUM_DELTA_POC_0);
	NUM_DELTA_POC(1, HEVCPP_NUM_DELTA_POC_1);
	NUM_DELTA_POC(2, HEVCPP_NUM_DELTA_POC_2);
	NUM_DELTA_POC(3, HEVCPP_NUM_DELTA_POC_3);
	NUM_DELTA_POC(4, HEVCPP_NUM_DELTA_POC_4);
	NUM_DELTA_POC(5, HEVCPP_NUM_DELTA_POC_5);
	NUM_DELTA_POC(6, HEVCPP_NUM_DELTA_POC_6);
	NUM_DELTA_POC(7, HEVCPP_NUM_DELTA_POC_7);
	NUM_DELTA_POC(8, HEVCPP_NUM_DELTA_POC_8);
	NUM_DELTA_POC(9, HEVCPP_NUM_DELTA_POC_9);
	NUM_DELTA_POC(10, HEVCPP_NUM_DELTA_POC_10);
	NUM_DELTA_POC(11, HEVCPP_NUM_DELTA_POC_11);
	NUM_DELTA_POC(12, HEVCPP_NUM_DELTA_POC_12);
	NUM_DELTA_POC(13, HEVCPP_NUM_DELTA_POC_13);
	NUM_DELTA_POC(14, HEVCPP_NUM_DELTA_POC_14);
	NUM_DELTA_POC(15, HEVCPP_NUM_DELTA_POC_15);

	word = 0;
	word |= (((cmd->init_qp_minus26) & 0x3F) << 16);
	word |= ((cmd->log2_min_transform_block_size_minus2) & 0x3);
	word |= (((cmd->log2_diff_max_min_transform_block_size) & 0x3) << 2);
	word |= (((cmd->max_transform_hierarchy_depth_intra) & 0x7) << 4);
	word |= (((cmd->max_transform_hierarchy_depth_inter) & 0x7) << 7);
	word |= (((cmd->transquant_bypass_enable_flag) & 0x1) << 13);
	word |= (((cmd->transform_skip_enabled_flag) & 0x1) << 14);
	word |= (((cmd->sign_data_hiding_flag) & 0x1) << 15);
	word |= (((cmd->cu_qp_delta_enabled_flag) & 0x1) << 24);
	word |= (((cmd->diff_cu_qp_delta_depth) & 0x3) << 25);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_RESIDUAL_CONFIG));

	word = 0;
	word |= ((cmd->pcm_enabled_flag) & 0x1);
	word |= (((cmd->log2_min_pcm_coding_block_size_minus3) & 0x3) << 8);
	word |= (((cmd->log2_diff_max_min_pcm_coding_block_size) & 0x3) << 10);
	word |= (((cmd->pcm_bit_depth_luma_minus1) & 0xF) << 16);
	word |= (((cmd->pcm_bit_depth_chroma_minus1) & 0xF) << 24);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_IPCM_CONFIG));

	word = 0;
	word |= ((cmd->pps_loop_filter_across_slices_enabled_flag) & 0x1);
	word |= (((cmd->deblocking_filter_control_present_flag) & 0x1) << 1);
	word |= (((cmd->sample_adaptive_offset_enabled_flag) & 0x1) << 2);
	word |= (((cmd->deblocking_filter_override_enabled_flag) & 0x1) << 8);
	word |= (((cmd->pps_deblocking_filter_disable_flag) & 0x1) << 9);
	word |= (((cmd->pps_beta_offset_div2) & 0xF) << 16);
	word |= (((cmd->pps_tc_offset_div2) & 0xF) << 24);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_LOOPFILTER_CONFIG));

	word = ((cmd->bit_buffer.base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_BIT_BUFFER_BASE_ADDR));

	word = ((cmd->bit_buffer.length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_BIT_BUFFER_LENGTH));

	word = ((cmd->bit_buffer.start_offset) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_BIT_BUFFER_START_OFFSET));

#ifdef HEVC_HADES_CUT1
	word = ((cmd->bit_buffer_stop_offset) & 0xffffffff);
#else
	word = ((cmd->bit_buffer.end_offset) & 0xffffffff);
#endif
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_BIT_BUFFER_STOP_OFFSET));

	word = ((cmd->intermediate_buffer.slice_table_base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_SLICE_TABLE_BASE_ADDR));

	word = ((cmd->intermediate_buffer.ctb_table_base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_TABLE_BASE_ADDR));

	word = ((cmd->intermediate_buffer.slice_headers_base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_SLICE_HEADERS_BASE_ADDR));

	word = ((cmd->intermediate_buffer.ctb_commands.base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_BASE_ADDR));

	word = ((cmd->intermediate_buffer.ctb_commands.length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_LENGTH));

	word = ((cmd->intermediate_buffer.ctb_commands.start_offset) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_START_OFFSET));

#ifndef HEVC_HADES_CUT1
	word = ((cmd->intermediate_buffer.ctb_commands.end_offset) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_END_OFFSET));
#endif
	word = ((cmd->intermediate_buffer.ctb_residuals.base_addr) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_BASE_ADDR));

	word = ((cmd->intermediate_buffer.ctb_residuals.length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_LENGTH));

	word = ((cmd->intermediate_buffer.ctb_residuals.start_offset) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_START_OFFSET));

#ifndef HEVC_HADES_CUT1
	word = ((cmd->intermediate_buffer.ctb_residuals.end_offset) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_END_OFFSET));
#endif
	word = ((cmd->intermediate_buffer.slice_table_length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_SLICE_TABLE_LENGTH));

	word = ((cmd->intermediate_buffer.ctb_table_length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CTB_TABLE_LENGTH));

	word = ((cmd->intermediate_buffer.slice_headers_length) & 0xffffffff);
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_SLICE_HEADERS_LENGTH));

#ifndef HEVC_HADES_CUT1
	// ERC settings : 0x700
	word = HEVCPP_CHKSYN_DIS_RBSP_TRAILING_DATA | HEVCPP_CHKSYN_DIS_RBSP_TRAILING_BITS | HEVCPP_CHKSYN_DIS_DATA_ALIGNMENT;
	writel(word, (volatile void *)(core->hw_base_addr + HEVCPP_CHKSYN_DIS));
#endif
}

/**
* Actually launches task in HW
*/
static OSDEV_Status_t HevcppTriggerTask(struct HevcppCore *core, struct HevcppTask *task)
{
	struct hevcpp_ioctl_queue_t *in_param;
	unsigned int InputBufferBase;

	BUG_ON(!hevcpp);
	BUG_ON(!core);
	task->core = core;

	in_param = &task->InParam;
	core->current_task = task;

	/* Should power on the core (currently only used to reinit core register) */
	if (pm_runtime_get_sync(hevcpp->dev) < 0) {
		pr_err("Error: %s pm_runtime_get_sync failed\n", __func__);
		return OSDEV_Error;
	}

	if (HevcppClockOn(core)) {
		pr_err("Error: %s failed to enable clock\n", __func__);
		pm_runtime_mark_last_busy(hevcpp->dev);
		pm_runtime_put(hevcpp->dev);
		return OSDEV_Error;
	}

	InputBufferBase  = (unsigned int)(in_param->iCmd.bit_buffer.base_addr +
	                                  in_param->iCmd.bit_buffer.start_offset);

#ifdef HEVC_HADES_CUT1
	if (in_param->iCmd.bit_buffer_stop_offset > in_param->iCmd.bit_buffer.start_offset) {
		// No buffer wrap
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress +
		                      in_param->iCmd.bit_buffer.start_offset,
		                      InputBufferBase,
		                      in_param->iCmd.bit_buffer_stop_offset -
		                      in_param->iCmd.bit_buffer.start_offset);
	} else if (in_param->iCmd.bit_buffer_stop_offset < in_param->iCmd.bit_buffer.start_offset) {
		// Buffer wrap - need to issue two commands
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress +
		                      in_param->iCmd.bit_buffer.start_offset,
		                      InputBufferBase,
		                      in_param->iCmd.bit_buffer.length -
		                      in_param->iCmd.bit_buffer.start_offset);
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress,
		                      (phys_addr_t) in_param->iCmd.bit_buffer.base_addr,
		                      in_param->iCmd.bit_buffer_stop_offset);
	}
#else
	if (in_param->iCmd.bit_buffer.end_offset > in_param->iCmd.bit_buffer.start_offset) {
		// No buffer wrap
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress +
		                      in_param->iCmd.bit_buffer.start_offset,
		                      InputBufferBase,
		                      in_param->iCmd.bit_buffer.end_offset -
		                      in_param->iCmd.bit_buffer.start_offset);
	} else if (in_param->iCmd.bit_buffer.end_offset < in_param->iCmd.bit_buffer.start_offset) {
		// Buffer wrap - need to issue two commands
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress +
		                      in_param->iCmd.bit_buffer.start_offset,
		                      InputBufferBase,
		                      in_param->iCmd.bit_buffer.length -
		                      in_param->iCmd.bit_buffer.start_offset);
		OSDEV_FlushCacheRange(in_param->iCodedBufferCachedAddress,
		                      (phys_addr_t) in_param->iCmd.bit_buffer.base_addr,
		                      in_param->iCmd.bit_buffer.end_offset);
	}

#endif

	/* Launch the pre-processor */
	task->DecodeTime = OSDEV_GetTimeInMilliSeconds();
	//pr_info("%s Launch task %d\n", __func__, task->id);

	/* Start write registers */
	mutex_lock(&core->hw_lock);

	writel(1, (volatile void *)(core->hw_base_addr + HEVCPP_RESET));

	HevcppTaskSet(core, &in_param->iCmd);

	writel(1, (volatile void *)(core->hw_base_addr + HEVCPP_START));
	mutex_unlock(&core->hw_lock);

	return OSDEV_NoError;
}

/**
* Ioctl to send a task to PP
*/
static OSDEV_Status_t HevcppIoctlQueueBuffer(struct HevcppOpenContext *instance,
                                             struct hevcpp_ioctl_queue_t *inParam)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct HevcppCore *core = NULL;
	struct HevcppTask *task;
	unsigned long flags;
	int i = 0;
	int ret = 0;

	hevcpp = (struct Hevcpp *)instance->data;
	BUG_ON(!hevcpp);

	/**
	 * Block calling thread if no core available
	 * Timeout to push a new task (pp_nb+1) depends on scheduling policy:
	 * (hevcpp->pp_nb(marging:+1))*HEVCPP_TIMEOUT_MS in FIFO scheduling policy
	 */
	ret = down_timeout(&hevcpp->pp_unactive_lock, msecs_to_jiffies((hevcpp->pp_nb + 1) * HEVCPP_TIMEOUT_MS));
	if (ret == -ETIME) {
		pr_err("Error: %s Waiting for task semaphore timeout!", __func__);
		Status = OSDEV_Error;
		goto bail;
	} else if (ret < 0) {
		pr_err("Error: %s Waiting for task semaphore interrupted!", __func__);
		Status = OSDEV_Error;
		goto bail;
	}

	/* Assign core where the task is processed */
	spin_lock_irqsave(&hevcpp->core_lock, flags);
	for (i = 0; i < hevcpp->pp_nb ; i++) {
		core = &hevcpp->core[i];
		if (!core->inuse) {
			core->inuse = true;
			break;
		}
	}
	spin_unlock_irqrestore(&hevcpp->core_lock, flags);

	/* Should not happen -> sem pp_unactive_lock protected*/
	BUG_ON(i == hevcpp->pp_nb);

	/* In case instance has been closed meanwhile */
	task = HevcppPushTask(instance, inParam);
	if (!task) {
		pr_err("Error: %s to push task : Fifo full\n", __func__);
		Status = OSDEV_Error;
		goto bail;
	}
	Status = HevcppTriggerTask(core, task);

bail:
	return Status;
}

// ////////////////////////////////////////////////////////////////////////////////
/**
 * Ioctl GetPreprocessedBuffer -> waits for buffer to be pre-processed and
 * returns output parameters pointed by ParameterAddress
 */
static OSDEV_Status_t HevcppIoctlGetPreprocessedBuffer(struct HevcppOpenContext *instance,
                                                       struct hevcpp_ioctl_dequeue_t *OutParam)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct HevcppTask task;
	struct Hevcpp *hevcpp;
	uint32_t err_sts;
	int ret = 0;

	hevcpp = (struct Hevcpp *)instance->data;
	BUG_ON(!hevcpp);

	ret = HevcppPopTask(instance, &task);
	if (ret) {
		Status = OSDEV_Error;
	}

	if (task.OutParam.hwStatus != hevcpp_ok) {
		pr_err("Error: %s - Task status NOK: %d\n", __func__, task.OutParam.hwStatus);
	}

	err_sts = task.OutParam.iStatus.error;
#ifdef HEVC_HADES_CUT1
	if ((err_sts != HEVCPP_NO_ERROR) &&
	    (err_sts != (HEVCPP_NO_ERROR | HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_3BYTES)) &&
	    (err_sts != (HEVCPP_NO_ERROR | HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_4BYTES)) &&
	    (err_sts != (HEVCPP_NO_ERROR | HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_4BYTES |
	                 HEVCPP_ERROR_STATUS_EBR_FORBIDDEN_3BYTES))) {
		pr_err("Error: %s HW returns %#08x\n", __func__, err_sts);
		task.OutParam.hwStatus = hevcpp_unrecoverable_error;
	}
#else
// As per ERC architecture doc:
//-----------------------------------------------------------------------------------------------------
// Bit                    Meaning                                  Expected   Action
//-----------------------------------------------------------------------------------------------------
// BUFFERx_DONE (x1..5) | Intermediate buffer written             | 1       | If 0, discard
// END_OF_PROCESS       | Decoding is finished                    | 1       | If 0, discard
// OVERFLOW (x1..5)     | Intermediate buffer overflow            | 0       | If 1, discard
// END_OF_DMA           | All specified bit buffer has been read  | 1       | If 0, follow policy
// PICTURE_COMPLETED    | Last CTB read and DMA end               | 1       | If 0, follow policy
// UNEXPECTED_SC        | Start code in slice header or data      | 0       | If 1, follow policy
// ERROR_ON_DATA_FLAG   | Error in slice data, will be emulated   | 0       | If 1, follow policy
// ERROR_ON_HEADER_FLAG | Error in slice header, will be emulated | 0       | If 1, follow policy
// MOVE_OUT_OF_DMA      | Still parsing at bit buffer end         | 0       | If 1, follow policy

	if (((err_sts & (HEVCPP_ERC_NO_ERROR              | HEVCPP_ERROR_STATUS_BUFFER1_DONE |
	                 HEVCPP_ERROR_STATUS_BUFFER2_DONE | HEVCPP_ERROR_STATUS_BUFFER3_DONE |
	                 HEVCPP_ERROR_STATUS_BUFFER4_DONE | HEVCPP_ERROR_STATUS_BUFFER5_DONE))
	     != (HEVCPP_ERC_NO_ERROR              | HEVCPP_ERROR_STATUS_BUFFER1_DONE |
	         HEVCPP_ERROR_STATUS_BUFFER2_DONE | HEVCPP_ERROR_STATUS_BUFFER3_DONE |
	         HEVCPP_ERROR_STATUS_BUFFER4_DONE | HEVCPP_ERROR_STATUS_BUFFER5_DONE)) ||
	    (err_sts & (HEVCPP_ERROR_STATUS_SLICE_TABLE_OVERFLOW   |
	                HEVCPP_ERROR_STATUS_SLICE_HEADER_OVERFLOW  | HEVCPP_ERROR_STATUS_CTB_TABLE_OVERFLOW |
	                HEVCPP_ERROR_STATUS_CTB_CMD_OVERFLOW       | HEVCPP_ERROR_STATUS_RES_CMD_OVERFLOW   |
	                HEVCPP_ERROR_STATUS_DISABLE_OVHD_VIOLATION | HEVCPP_ERROR_STATUS_DISABLE_HEVC_VIOLATION))) {
		pr_err("Error: %s HW returns unrecoverable error %#08x\n", __func__, err_sts);
		task.OutParam.hwStatus = hevcpp_unrecoverable_error;
	} else if (((err_sts & (HEVCPP_ERROR_STATUS_PICTURE_COMPLETED | HEVCPP_ERROR_STATUS_END_OF_DMA))
	            != (HEVCPP_ERROR_STATUS_PICTURE_COMPLETED | HEVCPP_ERROR_STATUS_END_OF_DMA)) ||
	           (err_sts & (HEVCPP_ERROR_STATUS_MOVE_OUT_OF_DMA | HEVCPP_ERROR_STATUS_ERROR_ON_HEADER_FLAG
	                       | HEVCPP_ERROR_STATUS_ERROR_ON_DATA_FLAG | HEVCPP_ERROR_STATUS_UNEXPECTED_SC))) {
		pr_err("Error: %s HW returns recoverable error %#08x\n", __func__, err_sts);
		task.OutParam.hwStatus = hevcpp_recoverable_error;
	}
#endif
	//pr_info("Hevcpp: %s Returns task %d err_sts: 0x%x\n",
	//__func__, task.id, err_sts);

	*OutParam = task.OutParam;

	return Status;
}

/**
 * Ioctl GetPreprocAvailability -> waits for fifo availability
 * to avoid interlock with buffer availability codec side...
 */
static OSDEV_Status_t HevcppIoctlGetPreprocAvailability(struct HevcppOpenContext *instance)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	int ret = 0;

	ret = wait_event_interruptible(
	              instance->task_wq,
	              (HEVCPP_TASK_FIFO_SIZE - 1) > atomic_read(&instance->in_progress_task_nb));

	if (ret < 0) {
		pr_err("Error: %s PP fifo not ready (instance:%p)", __func__, instance);
		Status = OSDEV_Error;
	}
	return Status;
}

/**
 * Main ioctl function
 */
static OSDEV_IoctlEntrypoint(HevcppIoctl)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct HevcppOpenContext *instance;
	struct hevcpp_ioctl_queue_t InParam;
	struct hevcpp_ioctl_dequeue_t OutParam;

	OSDEV_IoctlEntry();
	instance = (struct HevcppOpenContext *)OSDEV_PrivateData;
	BUG_ON(!instance);

	switch (OSDEV_IoctlCode) {
	case HEVC_PP_IOCTL_QUEUE_BUFFER:
		OSDEV_CopyToDeviceSpace(&InParam, OSDEV_ParameterAddress,
		                        sizeof(struct hevcpp_ioctl_queue_t));
		Status = HevcppIoctlQueueBuffer(instance, &InParam);
		break;

	case HEVC_PP_IOCTL_GET_PREPROCESSED_BUFFER:
		Status = HevcppIoctlGetPreprocessedBuffer(instance, &OutParam);
		OSDEV_CopyToUserSpace(OSDEV_ParameterAddress,
		                      &OutParam, sizeof(struct hevcpp_ioctl_dequeue_t));
		break;

	case HEVC_PP_IOCTL_GET_PREPROC_AVAILABILITY:
		Status = HevcppIoctlGetPreprocAvailability(instance);
		break;

	default:
		pr_err("Error: %s Invalid ioctl %08x\n", __func__, OSDEV_IoctlCode);
		Status = OSDEV_Error;
		break;
	}

	OSDEV_IoctlExit(Status);
}

static OSDEV_Descriptor_t HevcppDeviceDescriptor = {
	.Name = "hevc Pre-processor Module",
	.MajorNumber = MAJOR_NUMBER,
	.OpenFn  = HevcppOpen,
	.CloseFn = HevcppClose,
	.IoctlFn = HevcppIoctl,
	.MmapFn  = NULL
};

/**
 * Interrupt handler for task completion
 */
irqreturn_t HevcppInterruptHandler(int irq, void *data)
{
	struct HevcppCore *core = (struct HevcppCore *)data;
	struct HevcppTask *task = core->current_task;
	hevcdecpreproc_command_status_t *sts = &task->OutParam.iStatus;
#ifndef HEVC_HADES_CUT1
	unsigned int picture_size;
#endif

	sts->error                     = readl((volatile void *)(core->hw_base_addr + HEVCPP_ERROR_STATUS));

#ifndef HEVC_HADES_CUT1
	// WA for bz56274 (HW bug RnDHV00060580)
	// After blowing the over_hd fuse, if a stream with 4K resolution played it will return
	// over hd voilation error as expected. However this error bit in status register is not
	// getting cleared after the error status register read. Now if a stream with resolution
	// HD or below played, status register read will still return over hd voilation bit as 1.
	// WA is to ignore this preprocessor error status bit for picture size HD or below so that
	// subsequent operation in SE are not effected.
	picture_size = readl((volatile void *)(core->hw_base_addr + HEVCPP_PICTURE_SIZE));
	if ((picture_size & 0xFFFF) <= 1920) {
		// reset DISABLE_OVHD_VIOLATION bit in status read from PP error status register
		sts->error &= ~(HEVCPP_ERROR_STATUS_DISABLE_OVHD_VIOLATION);
	}
#endif

	sts->ctb_commands_stop_offset  = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_STOP_OFFSET));
	sts->ctb_residuals_stop_offset = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_STOP_OFFSET));
	sts->slice_table_entries       = readl((volatile void *)(core->hw_base_addr + HEVCPP_SLICE_TABLE_ENTRIES));
	sts->slice_table_entries       = (sts->slice_table_entries >> 5) & 0x7ffffff;
	sts->slice_table_stop_offset   = readl((volatile void *)(core->hw_base_addr + HEVCPP_SLICE_TABLE_STOP_OFFSET));
	sts->ctb_table_stop_offset     = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_TABLE_STOP_OFFSET));
	sts->slice_headers_stop_offset = readl((volatile void *)(core->hw_base_addr + HEVCPP_SLICE_HEADERS_STOP_OFFSET));
#ifdef HEVC_HADES_CUT1
	sts->slice_table_crc           = readl((volatile void *)(core->hw_base_addr + HEVCPP_SLICE_TABLE_CRC));
	sts->ctb_table_crc             = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_TABLE_CRC));
	sts->slice_headers_crc         = readl((volatile void *)(core->hw_base_addr + HEVCPP_SLICE_HEADERS_CRC));
	sts->ctb_commands_crc          = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_COMMANDS_CRC));
	sts->ctb_residuals_crc         = readl((volatile void *)(core->hw_base_addr + HEVCPP_CTB_RESIDUALS_CRC));
#endif

	// Clear IRQ
	writel(0x2, (volatile void *)(core->hw_base_addr + HEVCPP_CTRL_OBS));
	writel(0, (volatile void *)(core->hw_base_addr + HEVCPP_CTRL_OBS));

	//pr_info("%s IRQ handler end of task %d\n", __func__, task->id);
	complete(&task->hw_ended);

	return IRQ_HANDLED;
}

/**
* DT stuff
*/
static int hevcpp_get_of_pdata(struct platform_device *pdev, struct Hevcpp *hevcpp)
{
#ifdef CONFIG_OF
	struct HevcppCore *core;

#ifdef CONFIG_ARCH_STI
	int ret;

	core = &hevcpp->core[0];
	core->clk = devm_clk_get(&pdev->dev, "clk_hevcpp");
	if (IS_ERR(core->clk)) {
		pr_err("Error: %s Unable to get hevcpp clock\n", __func__);
		return PTR_ERR(core->clk);
	}

	ret = HevcppClockSetRate(pdev->dev.of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for hevcpp clock\n", __func__);
		return -EINVAL;
	}
#else
	unsigned char clk_alias[20];

#ifndef TMP_FIX_DT  //Disable OF clock definition temporarly (due to move to kernel 3.10)
	struct device_node *np = pdev->dev.of_node;
	int i;
	int ret = 0;
	char *clkName;

	for (i = 0; i < hevcpp->pp_nb ; i++) {
		core = &hevcpp->core[i];
		ret = of_property_read_string_index(np, "st,dev_clk", i, (const char **)&clkName);
		if (ret < 0) {
			pr_info("Hevcpp unable to get OF clock name %d\n", i);
			return -EINVAL;
		}

		snprintf(clk_alias, sizeof(clk_alias), "clk_hevcpp_%d", i);
		clk_alias[sizeof(clk_alias) - 1] = '\0';

		clk_add_alias(clk_alias, NULL, clkName, NULL);
		core->clk = devm_clk_get(&pdev->dev, clk_alias);

		if (IS_ERR(core->clk)) {
			pr_err("Error: %s Unable to get hevcpp clock[%d]\n", __func__, i);
			return PTR_ERR(core->clk);
		}
	}
#else //TODO
	core = &hevcpp->core[0];

	snprintf(clk_alias, sizeof(clk_alias), "clk_hevcpp_0");
	clk_alias[sizeof(clk_alias) - 1] = '\0';

	clk_add_alias(clk_alias, NULL, "CLK_PP_HADES", NULL);
	core->clk = devm_clk_get(&pdev->dev, clk_alias);
	if (IS_ERR(core->clk)) {
		pr_err("Error: %s Unable to get hevcpp clock\n", __func__);
		return PTR_ERR(core->clk);
	}
#endif //TMP_FIX_DT
#endif //CONFIG_ARCH_STI
#endif //CONFIG_OF

	return 0;
}

/**
*
*/
static int HevcppProbe(struct platform_device *pdev)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct HevcppCore *core = NULL;
	struct resource *iomem;
	int ret = 0;
	int i = 0;

	BUG_ON(pdev == NULL);

	hevc_thread_desc.policy   = thpolprio_inits[0];
	hevc_thread_desc.priority = thpolprio_inits[1];
	pr_info("%s %s pol:%d prio:%d\n", __func__, hevc_thread_desc.name,
	        hevc_thread_desc.policy, hevc_thread_desc.priority);

	hevcpp = devm_kzalloc(&pdev->dev, sizeof(*hevcpp), GFP_KERNEL);
	if (!hevcpp) {
		pr_err("Error: %s alloc failed\n", __func__);
		ret = -EINVAL;
		goto hevcpp_bail;
	}

	/**
	 * Saving pointer to main struct to be able to retrieve it in pm_runtime
	 * callbacks for example (it is *NOT* platform device data, just *driver* data)
	 */
	platform_set_drvdata(pdev, hevcpp);
	hevcpp->dev = &pdev->dev;

	/* Platform include IOMEM and IRQs */
	hevcpp->pp_nb = pdev->num_resources / 2;
	BUG_ON(hevcpp->pp_nb > HEVCPP_MAX_SUPPORTED_PRE_PROCESSORS);
	pr_info("%s hw core nb: %d\n", __func__, hevcpp->pp_nb);
	hevcpp->core = devm_kzalloc(&pdev->dev, hevcpp->pp_nb * sizeof(*hevcpp->core), GFP_KERNEL);
	if (!hevcpp->core) {
		pr_err("Error: %s core alloc failed\n", __func__);
		ret = -EINVAL;
		goto hevcpp_bail;
	}

	sema_init(&hevcpp->pp_unactive_lock, hevcpp->pp_nb);
	spin_lock_init(&hevcpp->core_lock);

	if (pdev->dev.of_node) {
		ret = hevcpp_get_of_pdata(pdev, hevcpp);
		if (ret) {
			pr_err("Error: %s Failed to retrieve driver data\n", __func__);
			goto hevcpp_bail;
		}
	} else {
		pr_warn("warning: %s No DT entry\n", __func__);
	}

	for (i = 0; i < hevcpp->pp_nb ; i++) {
		core = &hevcpp->core[i];

		snprintf(core->name, sizeof(core->name), "%s_%d", HEVCPP_DEVICE_NAME, i);
		core->name[sizeof(core->name) - 1] = '\0';

		mutex_init(&core->hw_lock);
		spin_lock_init(&core->clk_lock);
		core->inuse = false;
		core->need_soft_reset = false;

		iomem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!iomem) {
			pr_err("Error: %s Get ressource MEM[%d] failed\n", __func__, i);
			ret = -ENODEV;
			goto hevcpp_bail;
		}
		core->regs_size = iomem->end - iomem->start + 1;

		/* DT defines hades base addr as we need it to configure PP */
		core->hw_hades_addr =
		        devm_ioremap_nocache(&pdev->dev, iomem->start, core->regs_size);
		if (!core->hw_hades_addr) {
			pr_err("Error: %s ioremap registers region[%d] failed\n", __func__, i);
			ret = -ENOMEM;
			goto hevcpp_bail;
		}

		core->hw_base_addr = core->hw_hades_addr + HEVCPP_REGISTER_OFFSET;

		core->irq_its = platform_get_irq(pdev, i);
		if (core->irq_its <= 0) {
			pr_err("Error: %s Get ressource IRQ[%d] failed\n", __func__, i);
			ret = -ENODEV;
			goto hevcpp_bail;
		}

		ret = devm_request_irq(&pdev->dev, core->irq_its, HevcppInterruptHandler,
		                       IRQF_DISABLED, core->name, (void *)core);
		if (ret) {
			pr_err("Error: %s Request IRQ 0x%x failed\n", __func__, core->irq_its);
			goto hevcpp_bail;
		}
		disable_irq(core->irq_its);

		ret = clk_prepare(core->clk);
		if (ret) {
			pr_err("Error: %s Failed to prepare hevcpp clock[%d]\n", __func__, i);
			goto hevcpp_bail;
		}
		enable_irq(core->irq_its);
	}

	Status = OSDEV_RegisterDevice(&HevcppDeviceDescriptor);
	if (Status != OSDEV_NoError) {
		pr_err("Error: %s Unable to get major %d\n", __func__, MAJOR_NUMBER);
		ret = -ENODEV;
		goto hevcpp_bail;
	}

	Status = OSDEV_LinkDevice(HEVCPP_DEVICE, MAJOR_NUMBER, 0);
	if (Status != OSDEV_NoError) {
		OSDEV_DeRegisterDevice(&HevcppDeviceDescriptor);
		ret = -ENODEV;
		goto hevcpp_bail;
	}

	/* Clear the device's 'power.runtime_error' flag and set the device's runtime PM status to 'suspended' */
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);

	/* Init of plugs can be done once only in probe (not under hevcpp clock) */
	HevcppInitPlugs(core);

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("Hevcpp probe done ok\n");

	return 0;

hevcpp_bail:
	if (core) {
		for (i = 0 ; i < hevcpp->pp_nb ; i++)
			if (hevcpp->core[i].clk) {
				clk_unprepare(core->clk);
			}
	}

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_err("Error: %s Hevcpp probe failed\n", __func__);

	return ret;
}

/**
*
*/
static int HevcppRemove(struct platform_device *pdev)
{
	struct Hevcpp *hevcpp;
	struct HevcppCore *core;
	int i = 0;

	BUG_ON(pdev == NULL);

	hevcpp = dev_get_drvdata(&pdev->dev);
	BUG_ON(hevcpp == NULL);

	for (i = 0 ; i < hevcpp->pp_nb ; i++) {
		core = &hevcpp->core[i];
		disable_irq(core->irq_its);
		clk_unprepare(core->clk);
	}

	pm_runtime_disable(&pdev->dev);

	OSDEV_UnLinkDevice(HEVCPP_DEVICE);

	OSDEV_DeRegisterDevice(&HevcppDeviceDescriptor);

	dev_set_drvdata(&pdev->dev, NULL);

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("Hevcpp remove done\n");

	return 0;
}

// ////////////////////////////PM_RUNTIME///////////////////////////////////////////
/**
 * pm_runtime_resume callback for Active Standby
 * called if pm_runtime ref counter == 0 (and for Active Standby)
 */
static int HevcppPmRuntimeResume(struct device *dev)
{
	//struct Hevcpp *hevcpp = dev_get_drvdata(dev);

	return 0;
}
/**
 * pm_runtime_suspend callback for Active Standby
 * called if pm_runtime ref counter == 0 (and for Active Standby)
 */
static int HevcppPmRuntimeSuspend(struct device *dev)
{
	//struct Hevcpp *hevcpp = dev_get_drvdata(dev);

	return 0;
}

// ////////////////////////////////PM///////////////////////////////////////////
#ifdef CONFIG_PM
/** Resume callback, being called on Host Passive Standby exit (HPS exit) */
static int HevcppPmResume(struct device *dev)
{
	int ret;
	pr_info("%s : HPS exit (pmResume)\n", __func__);

	ret = HevcppClockSetRate(dev->of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for hevcpp clock\n", __func__);
		return -EINVAL;
	}

	/**
	 * Here we assume that no new jobs have been posted yet.
	 * This is ensured at SE level, which will exit "low power state" later on,
	 * in PM notifier callback of player2 module (on PM_POST_SUSPEND event).
	 */

	return 0;
}
/**
 * Suspend callback, being called on Host Passive Standby entry (HPS entry)
 * Prevent suspension if some clients on going
 */
static int HevcppPmSuspend(struct device *dev)
{
	int i;
	struct Hevcpp *hevcpp = dev_get_drvdata(dev);

	pr_info("%s : HPS enter (pmSuspend)\n", __func__);
	BUG_ON(!hevcpp);
	/**
	* Here we assume that there are no more jobs in progress.
	* This is ensured at SE level, which has already entered "low power state",
	* in PM notifier callback of player2 module (on PM_SUSPEND_PREPARE event)
	*/
	for (i = 0; i < hevcpp->pp_nb; i++) {
		if (hevcpp->core[i].inuse) {
			pr_info("%s : Core %d used (should not happen)!\n", __func__, i);
		}
	}
	return 0;
}
/** Restore callback, being called on Controller Passive Standby exit (CPS exit) */
static int HevcppPmRestore(struct device *dev)
{
	int ret;
	pr_info("%s : CPS exit (pmRestore)\n", __func__);

	ret = HevcppClockSetRate(dev->of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for hevcpp clock\n", __func__);
		return -EINVAL;
	}

	/**
	 * Here we assume that no new jobs have been posted yet.
	 * This is ensured at SE level, which will exit "low power state" later on,
	 * in PM notifier callback of player2 module (on PM_POST_HIBERNATION event).
	 * Also we have to ensure that Hevcpp driver will be able to restore all necessary registers configuration.
	 */

	return 0;
}
/** Freeze callback, being called on Controller Passive Standby entry (CPS entry) */
static int HevcppPmFreeze(struct device *dev)
{
	int i;
	struct Hevcpp *hevcpp = dev_get_drvdata(dev);

	pr_info("%s : CPS enter (pmFreeze)\n", __func__);
	BUG_ON(!hevcpp);
	/**
	 * Here we assume that there are no more jobs in progress.
	 * This is ensured at SE level, which has already entered "low power state",
	 * in PM notifier callback of player2 module (on PM_HIBERNATION_PREPARE event).
	 * Also we have to ensure that Hevcpp driver will be able to restore all necessary registers configuration on CPS exit.
	 */
	for (i = 0; i < hevcpp->pp_nb; i++) {
		if (hevcpp->core[i].inuse) {
			pr_info("%s : Core %d used (should not happen)!\n", __func__, i);
		}
	}
	return 0;
}

static struct dev_pm_ops HevcppPmOps = {
	.resume  = HevcppPmResume,
	.suspend = HevcppPmSuspend,
	.restore = HevcppPmRestore,
	.freeze  = HevcppPmFreeze,
	.runtime_resume   = HevcppPmRuntimeResume,
	.runtime_suspend  = HevcppPmRuntimeSuspend,
};
#endif // CONFIG_PM

#ifdef CONFIG_OF
static struct of_device_id stm_hevcpp_match[] = {
	{
		.compatible = "st,se-hevcpp",
	},
	{},
};
//MODULE_DEVICE_TABLE(of, stm_hevcpp_match);
#endif

static struct platform_driver HevcppDriver = {
	.driver = {
		.name = "hevcpp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_hevcpp_match),
#ifdef CONFIG_PM
		.pm = &HevcppPmOps,
#endif
	},
	.probe = HevcppProbe,
	.remove = HevcppRemove,
};

module_platform_driver(HevcppDriver);

