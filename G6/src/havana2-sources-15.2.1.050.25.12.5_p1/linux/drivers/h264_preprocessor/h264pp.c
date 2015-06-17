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

#define MAJOR_NUMBER    246

#include "osdev_time.h"
#include "osdev_sched.h"
#include "osdev_cache.h"
#include "osdev_device.h"

#include "h264ppio.h"
#include "h264pp.h"

MODULE_DESCRIPTION("H264 pre-processor platform driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

OSDEV_ThreadDesc_t h264_thread_desc = { "SE-Vid-H264pp", -1, -1 };
static int thpolprio_inits[2] = {SCHED_RR, OSDEV_MID_PRIORITY };
module_param_array_named(thread_SE_Vid_H264pp, thpolprio_inits, int, NULL, S_IRUGO);
MODULE_PARM_DESC(thread_SE_Vid_H264pp, "H264pp thread parameters: policy,priority");

/* How long we should wait for PP to complete single frame/field preprocessing before we give up */
#define H264_PP_TIMEOUT_MS   110
/* Size of sw task fifo */
#define H264_PP_TASK_FIFO_SIZE 4

struct H264ppStatusReg {
	unsigned int pp_its;
	unsigned int pp_wdl;
	unsigned int pp_isbg;
	unsigned int pp_ipbg;
};

/**
 * Task descriptor
 * @InParam: input parameters
 * @OutParam: output parameters
 * @StsReg: task status registers
 * @OutputSize: output buffer size
 * @DecodeTime: task decoding time
 * @hw_ended:  task completion in HW
 * @completed: task full completion
 * @core: core where the task has been processed
 *        if 0 -> termination signal for end of task thread
 * @id: informative field
 */
struct H264ppTask {
	h264pp_ioctl_queue_t    InParam;
	h264pp_ioctl_dequeue_t  OutParam;
	struct H264ppStatusReg  StsReg;

	unsigned int            OutputSize;
	unsigned int            DecodeTime;
	struct completion       hw_ended;
	struct completion       completed;
	struct H264ppCore       *core;
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
struct H264ppCore {
	char              name[16];
	struct clk        *clk;
	int               clk_ref_count;
	spinlock_t        clk_lock;
	struct mutex      hw_lock;
	u32               irq_its;
	void __iomem      *hw_base_addr;
	int               regs_size;
	bool              inuse;
	struct H264ppTask *current_task;
	bool              need_soft_reset;
};

/**
 * Main struct for H264PP multi-core
 * @dev: device pointer
 * @pp_nb: number of core
 * @pp_unactive_lock: semaphore dedicated to *unactive* core number
 * @core: array of cores
 * @core_lock: protect access to core array
 * @instance_nb: client (open) number
 */
struct H264pp {
	struct device      *dev;
	int                pp_nb;
	struct semaphore   pp_unactive_lock;
	struct H264ppCore  *core;
	spinlock_t         core_lock;
	atomic_t           instance_nb;
	unsigned int       stbus_max_opcode_size;
	unsigned int       stbus_max_chunk_size;
	unsigned int       stbus_max_msg_size;
};

/**
 * Open context -> client instance
 * @data: private data used to store pointer to H264pp device
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
struct H264ppOpenContext {
	void               *data;
	spinlock_t         fifo_task_lock;
	struct H264ppTask  task[H264_PP_TASK_FIFO_SIZE];
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
 * Pointer to h264pp global struct. Should be accessible in open/close/... functions
 * (OSDEV_*Entrypoint abstraction prevents or masks access to struct file* which
 * can be used to retrieve pointer to h264pp, and then remove this global variable)
 */
static struct H264pp *h264pp;

// ////////////////////////////////////////////////////////////////////////////////
#define H264PP_MAX_OPC_SIZE_DEFAULT     5U
#define H264PP_MAX_CHUNK_SIZE_DEFAULT   0U
#define H264PP_MAX_MESSAGE_SIZE_DEFAULT 3U
/** Initialise one core (i.e. resets registers to default value) */
static void H264ppInitCore(struct H264ppCore *core)
{
	int retry = 0;
	BUG_ON(!core);

	mutex_lock(&core->hw_lock);
	if (core->need_soft_reset) {
		writel(0, (volatile void *)(core->hw_base_addr + PP_ITM));
		writel(1, (volatile void *)(core->hw_base_addr + PP_SRS));      // Perform a soft reset

		while (!(readl((volatile void *)(core->hw_base_addr + PP_ITS)) & PP_ITM__SRS_COMP)) {
			retry ++;
			if (retry >= H264_PP_RESET_TIME_LIMIT) {
				break;
			}
			OSDEV_SleepMilliSeconds(1);
		}

		if (retry == H264_PP_RESET_TIME_LIMIT) {
			pr_err("Error: %s failed to soft reset PP\n", __func__);
		} else {
			core->need_soft_reset = false;
		}
	}

	writel(0xffffffff, (volatile void *)(core->hw_base_addr + PP_ITS));   // Clear interrupt status

	/* Config of  hw message exchange protocol to default values */
	writel(h264pp->stbus_max_opcode_size, (volatile void *)(core->hw_base_addr + PP_MAX_OPC_SIZE));
	writel(h264pp->stbus_max_chunk_size, (volatile void *)(core->hw_base_addr + PP_MAX_CHUNK_SIZE));
	writel(h264pp->stbus_max_msg_size, (volatile void *)(core->hw_base_addr + PP_MAX_MESSAGE_SIZE));
	mutex_unlock(&core->hw_lock);
}

static int H264ClockSetRate(struct device_node *of_node)
{
	int ret = 0;
	unsigned int max_freq = 0;
	if (!of_property_read_u32(of_node, "clock-frequency", &max_freq)) {
		if (max_freq) {
			ret = clk_set_rate(h264pp->core[0].clk, max_freq);
			if (ret) {
				pr_err("Error: %s Failed to set max frequency for h264pp clock\n", __func__);
				ret = -EINVAL;
			}
		}
	}
	pr_info("ret value : 0x%x max frequency value : %u\n", (unsigned int)ret, max_freq);
	return ret;
}

// ////////////////////////////////////////////////////////////////////////////////
/** Disable clock of one core */
void H264ppClockOff(struct H264ppCore *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->clk_lock, flags);
	clk_disable(core->clk);
	core->clk_ref_count--;
	spin_unlock_irqrestore(&core->clk_lock, flags);
}

/** Enable clock of one core and reinit its registers if clk_ref_count==0 */
int H264ppClockOn(struct H264ppCore *core)
{
	bool need_reinit = false;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&core->clk_lock, flags);
	ret = clk_enable(core->clk);
	if (!ret) {
		core->clk_ref_count++;
		if (1 == core->clk_ref_count) {
			need_reinit = true;
		}
	}
	spin_unlock_irqrestore(&core->clk_lock, flags);
	if (need_reinit) {
		H264ppInitCore(core);
	}
	return ret;
}

// ////////////////////////////////////////////////////////////////////////////////
/**
 * Adds and returns one task to SW fifo
 * May blocks on task fifo fullness
 * @core on which core task is added
 * @instance pointer to instance
 * @param pointer to input parameter
 */
struct H264ppTask *H264ppPushTask(struct H264ppOpenContext *instance, h264pp_ioctl_queue_t *p)
{
	unsigned int write_idx = 0;
	struct H264ppTask *task;
	unsigned long flags;
	int ret = 0;

	/* Ensures that we can store task in FIFO */
	ret = wait_event_interruptible_timeout(
	              instance->task_wq,
	              H264_PP_TASK_FIFO_SIZE > atomic_read(&instance->pending_task_nb),
	              msecs_to_jiffies(H264_PP_TIMEOUT_MS));

	if (ret == 0) {
		pr_err("Error: %s failed to push task in fifo (instance:%p)", __func__, instance);
		return NULL;
	} else if (ret < 0) {
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
	} else
		/* Fake task to terminate end of task thread */
	{
		task->core = 0;
	}

	atomic_inc(&instance->pending_task_nb);
	atomic_inc(&instance->in_progress_task_nb);
	instance->input_task_idx++;
	if (instance->input_task_idx == H264_PP_TASK_FIFO_SIZE) {
		instance->input_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	wake_up_interruptible(&instance->task_wq);

	return task;
}

// ////////////////////////////////////////////////////////////////////////////////
/**
 * Returns hw_done_task_idx task of SW fifo
 * May blocks on hw task completion
 * @instance pointer to instance
 * if successful returns 0, else <0
 */
int H264ppGetEndedTask(struct H264ppOpenContext *instance, struct H264ppTask **t)
{
	unsigned int read_idx = 0;
	struct H264ppTask *task = NULL;
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
	if (instance->hw_done_task_idx == H264_PP_TASK_FIFO_SIZE) {
		instance->hw_done_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);
	//pr_info("H264pp: %s wait for task %d\n", __func__, task->id);

	/* Waiting for task completion */
	if (!wait_for_completion_timeout
	    (&task->hw_ended, msecs_to_jiffies(H264_PP_TIMEOUT_MS))) {
		pr_err("Error: %s TIMEOUT Wait for task %d %p\n", __func__, task->id, task);
		ret = -EINVAL;
	}

bail:
	*t = task;
	return ret;
}

// ////////////////////////////////////////////////////////////////////////////////
/**
 * Remove and returns one task of SW fifo
 * May blocks on task completion
 * @instance pointer to instance
 * if successful returns 0, else <0
 */
int H264ppPopTask(struct H264ppOpenContext *instance, struct H264ppTask *t)
{
	unsigned int read_idx = 0;
	struct H264ppTask *task;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	read_idx = instance->output_task_idx;
	task = &instance->task[read_idx];
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	//pr_info("H264pp: %s wait for task %d\n", __func__, task->id);

	/* Waiting for task completion */
	if (!wait_for_completion_timeout
	    (&task->completed, msecs_to_jiffies(H264_PP_TIMEOUT_MS))) {
		pr_err("Error: %s TIMEOUT Wait for task %d %p\n", __func__, task->id, task);
		ret = -EINVAL;
	}

	spin_lock_irqsave(&instance->fifo_task_lock, flags);
	atomic_dec(&instance->pending_task_nb);
	instance->output_task_idx++;
	if (instance->output_task_idx == H264_PP_TASK_FIFO_SIZE) {
		instance->output_task_idx = 0;
	}
	spin_unlock_irqrestore(&instance->fifo_task_lock, flags);

	/* memcpy is NEEDED as task[i] may be overwritten as soon as task_wq event is raised ! */
	*t = *task;
	wake_up_interruptible(&instance->task_wq);

	return ret;
}

// ////////////////////////////////////////////////////////////////////////////////

void H264ppCheckTaskStatus(struct H264ppTask *task)
{
	h264pp_ioctl_queue_t   *in_param  = &task->InParam;
	h264pp_ioctl_dequeue_t *out_param = &task->OutParam;
	unsigned int now = OSDEV_GetTimeInMilliSeconds();

	out_param->ErrorMask = task->StsReg.pp_its & ~(PP_ITM__SRS_COMP | PP_ITM__DMA_CMP);

	if (task->StsReg.pp_its & PP_ITM__DMA_CMP) {
		task->OutputSize = (task->StsReg.pp_wdl - task->StsReg.pp_isbg);
	} else {
		task->OutputSize = 0;
	}

	task->DecodeTime = now - task->DecodeTime;
	out_param->QueueIdentifier = in_param->QueueIdentifier;
	out_param->OutputSize = task->OutputSize;
}

// ////////////////////////////////////////////////////////////////////////////////

OSDEV_ThreadEntrypoint(task_completion)
{
	struct H264ppOpenContext *instance = (struct H264ppOpenContext *)Parameter;
	struct H264ppTask *task;
	struct H264ppCore *core;
	unsigned long flags;
	struct H264pp *h264pp = (struct H264pp *)instance->data;
	int ret = 0;

	/* we are holding a reference to the task_struct, and need it later
	   during close. Make sure that it doesn't go away prematurely (due
	   to the thread exiting while or before we have reached
	   kthread_stop() later). */
	get_task_struct(current);
	do {
		/* Block until one task has been completed by HW */
		ret = H264ppGetEndedTask(instance, &task);
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
		H264ppClockOff(core);
		H264ppCheckTaskStatus(task);
		//pr_info("H264pp: %s  H264ppCheckStatus task %d\n", __func__, task->id);
		/* Up semaphore as soon as possible : to start new task */
		spin_lock_irqsave(&h264pp->core_lock, flags);
		core->inuse = false;
		spin_unlock_irqrestore(&h264pp->core_lock, flags);
		up(&h264pp->pp_unactive_lock);
		complete(&task->completed);
	} while (1);

	pr_info("H264pp: %s TerminateThread (%p)\n", __func__, instance);
}

// ////////////////////////////////////////////////////////////////////////////////
/** Open function creates a new internal instance */
static OSDEV_OpenEntrypoint(H264ppOpen)
{
	struct H264ppOpenContext *instance;
	OSDEV_Status_t status = OSDEV_NoError;

	OSDEV_OpenEntry();

	BUG_ON(!h264pp);

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance) {
		pr_err("Error: %s failed to allocate instance\n", __func__);
		OSDEV_OpenExit(OSDEV_Error);
	}

	spin_lock_init(&instance->fifo_task_lock);

	init_waitqueue_head(&instance->task_wq);
	instance->data = h264pp;
	OSDEV_PrivateData = instance;

	atomic_inc(&h264pp->instance_nb);

	status = OSDEV_CreateThread(&instance->task_completed_thread, task_completion, instance, &h264_thread_desc);
	if (status != OSDEV_NoError) {
		pr_err("Error: %s failed to create thread for instance (%p)\n", __func__, instance);
		goto bail;
	}
	OSDEV_OpenExit(OSDEV_NoError);

bail:
	kfree(instance);
	OSDEV_PrivateData = NULL;
	OSDEV_OpenExit(OSDEV_Error);
}

// ////////////////////////////////////////////////////////////////////////////////
/** Close function removes instance after ensuring no task is pending */
static OSDEV_CloseEntrypoint(H264ppClose)
{
	struct H264ppOpenContext *instance;
	struct H264ppTask task;
	struct H264ppTask *t;

	OSDEV_CloseEntry();

	BUG_ON(!h264pp);

	instance = (struct H264ppOpenContext *)OSDEV_PrivateData;

	/* Terminate end of task thread */
	t = H264ppPushTask(instance, 0);
	if (t) {
		complete(&t->hw_ended);
		pr_info("H264pp: %s complete termination task instance (%p)\n", __func__, instance);
	}

	/* Remove pending tasks */
	while (atomic_read(&instance->pending_task_nb)) {
		H264ppPopTask(instance, &task);
	}

	kthread_stop(instance->task_completed_thread);
	put_task_struct(instance->task_completed_thread);

	atomic_dec(&h264pp->instance_nb);
	kfree(instance);
	OSDEV_PrivateData = NULL;

	OSDEV_CloseExit(OSDEV_NoError);
}

// ////////////////////////////////////////////////////////////////////////////////
/** Actually launches task in HW */
static OSDEV_Status_t H264ppTriggerTask(struct H264ppCore *core, struct H264ppTask *task)
{
	h264pp_ioctl_queue_t *in_param;
	unsigned int val = 0;

	unsigned int InputBufferBase;
	unsigned int OutputBufferBase;
	unsigned int SourceAddress;
	unsigned int EndAddress;
	unsigned int SliceErrorStatusAddress;
	unsigned int IntermediateAddress;
	unsigned int IntermediateEndAddress;

	BUG_ON(!h264pp);
	BUG_ON(!core);
	task->core = core;

	in_param = &task->InParam;
	BUG_ON(!in_param->MaxOutputBufferSize);
	core->current_task = task;

#ifdef CONFIG_PM_RUNTIME
	/* Should power on the core (currently only used to reinit core register) */
	if (pm_runtime_get_sync(h264pp->dev) < 0) {
		pr_err("Error: %s pm_runtime_get_sync failed\n", __func__);
		return OSDEV_Error;
	}
#endif

	if (H264ppClockOn(core)) {
		pr_err("Error: %s %s failed to enable clock\n", __func__, __func__);
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_mark_last_busy(h264pp->dev);
		pm_runtime_put(h264pp->dev);
#endif
		return OSDEV_Error;
	}

	/* Calculate the address values */
	InputBufferBase  = (unsigned int)in_param->InputBufferPhysicalAddress;
	OutputBufferBase = (unsigned int)in_param->OutputBufferPhysicalAddress;

	SliceErrorStatusAddress     = OutputBufferBase;
	IntermediateAddress         = OutputBufferBase + H264_PP_SESB_SIZE;
	IntermediateEndAddress      = IntermediateAddress + in_param->MaxOutputBufferSize - H264_PP_SESB_SIZE - 1;
	SourceAddress               = InputBufferBase;
	EndAddress                  = SourceAddress + in_param->InputSize - 1;

	/* Program the preprocessor */
	/* Standard pre-processor initialization */
	OSDEV_FlushCacheRange(in_param->InputBufferCachedAddress,  InputBufferBase, in_param->InputSize);
	OSDEV_FlushCacheRange(in_param->OutputBufferCachedAddress, OutputBufferBase, H264_PP_SESB_SIZE);

	/* Launch the pre-processor */
	task->DecodeTime = OSDEV_GetTimeInMilliSeconds();
	//pr_info("H264pp: Launch task %d\n", task->id);

	/* Start write registers */
	mutex_lock(&core->hw_lock);

	writel(0xffffffff, (volatile void *)(core->hw_base_addr + PP_ITS));    // Clear interrupt status

	/* We are interested in every fail/completion interrupt */
	val = (//PP_ITM__WRITE_ERROR | PP_ITM__READ_ERROR |
	              PP_ITM__BIT_BUFFER_OVERFLOW | PP_ITM__BIT_BUFFER_UNDERFLOW |
	              PP_ITM__INT_BUFFER_OVERFLOW | PP_ITM__SRS_COMP | PP_ITM__DMA_CMP);

	writel(val, (volatile void *)(core->hw_base_addr + PP_ITM));

	/* Removed completion of error insertion
	   PP_ITM__ERROR_BIT_INSERTED | PP_ITM__ERROR_SC_DETECTED */
	/* Setup the decode */
	writel((SourceAddress & 0xfffffff8), (volatile void *)(core->hw_base_addr + PP_BBG));
	writel((EndAddress | 0x7), (volatile void *)(core->hw_base_addr + PP_BBS));
	writel(SourceAddress, (volatile void *)(core->hw_base_addr + PP_READ_START));
	writel(EndAddress, (volatile void *)(core->hw_base_addr + PP_READ_STOP));

	writel(SliceErrorStatusAddress, (volatile void *)(core->hw_base_addr + PP_ISBG));
	writel(IntermediateAddress, (volatile void *)(core->hw_base_addr + PP_IPBG));
	writel(IntermediateEndAddress, (volatile void *)(core->hw_base_addr + PP_IBS));

	writel((PP_CFG__CONTROL_MODE__START_STOP | in_param->Cfg), (volatile void *)(core->hw_base_addr + PP_CFG));
	writel(in_param->PicWidth, (volatile void *)(core->hw_base_addr + PP_PICWIDTH));
	writel(in_param->CodeLength, (volatile void *)(core->hw_base_addr + PP_CODELENGTH));

	writel(0x1, (volatile void *)(core->hw_base_addr + PP_START));
	mutex_unlock(&core->hw_lock);

	return OSDEV_NoError;
}

// ////////////////////////////////////////////////////////////////////////////////

static OSDEV_Status_t H264ppIoctlQueueBuffer(struct H264ppOpenContext *instance,
                                             h264pp_ioctl_queue_t *inParam)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct H264ppCore *core = NULL;
	struct H264ppTask *task;
	unsigned long flags;
	int i = 0;
	int ret = 0;

	h264pp = (struct H264pp *)instance->data;
	BUG_ON(!h264pp);

	/**
	 * Block calling thread if no core available
	 * Timeout to push a new task (pp_nb+1) depends on scheduling policy:
	 * (h264pp->pp_nb(marging:+1))*H264_PP_TIMEOUT_MS in FIFO scheduling policy
	 */
	ret = down_timeout(&h264pp->pp_unactive_lock, msecs_to_jiffies((h264pp->pp_nb + 1) * H264_PP_TIMEOUT_MS));
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
	spin_lock_irqsave(&h264pp->core_lock, flags);
	for (i = 0; i < h264pp->pp_nb ; i++) {
		core = &h264pp->core[i];
		if (!core->inuse) {
			core->inuse = true;
			break;
		}
	}
	spin_unlock_irqrestore(&h264pp->core_lock, flags);

	/*Should not happen -> sem pp_unactive_lock protected*/
	BUG_ON(i == h264pp->pp_nb);

	/* In case instance has been closed meanwhile */
	task = H264ppPushTask(instance, inParam);
	BUG_ON(!task);  /* Fifo full should never happen !!! */

	Status = H264ppTriggerTask(core, task);

bail:
	return Status;
}

// ////////////////////////////////////////////////////////////////////////////////
/**
 * Ioctl GetPreprocessedBuffer -> waits for buffer to be pre-processed and
 * returns output parameters pointed by ParameterAddress
 */
static OSDEV_Status_t H264ppIoctlGetPreprocessedBuffer(struct H264ppOpenContext *instance,
                                                       h264pp_ioctl_dequeue_t *OutParam)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct H264ppTask task;
	struct H264pp *h264pp;
	int ret = 0;

	h264pp = (struct H264pp *)instance->data;
	BUG_ON(!h264pp);

	ret = H264ppPopTask(instance, &task);
	if (ret) {
		Status = OSDEV_Error;
	}

#ifdef CONFIG_PM_RUNTIME
	/** pm_runtime_put -> decrement pm ref_counter and if == 0 calls H264ppPmRuntimeSuspend */
	pm_runtime_mark_last_busy(h264pp->dev);
	pm_runtime_put(h264pp->dev);
#endif

	/* Dump register status if task failed */
	if (task.StsReg.pp_its != PP_ITM__DMA_CMP) {
		pr_info("H264pp: %s task %d (pp_its: %x)%s%s%s%s%s%s%s%s%s\n", __func__,
		        task.id, task.StsReg.pp_its,
		        (task.StsReg.pp_its & PP_ITM__DMA_CMP              ? "" : " !DMA_CMP"),
		        (task.StsReg.pp_its & PP_ITM__SRS_COMP             ? " SRS" : ""),
		        (task.StsReg.pp_its & PP_ITM__ERROR_SC_DETECTED    ? " SC" : ""),
		        (task.StsReg.pp_its & PP_ITM__ERROR_BIT_INSERTED   ? " BIT" : ""),
		        (task.StsReg.pp_its & PP_ITM__INT_BUFFER_OVERFLOW  ? " INT_OVER" : ""),
		        (task.StsReg.pp_its & PP_ITM__BIT_BUFFER_UNDERFLOW ? " BIT_UNDER" : ""),
		        (task.StsReg.pp_its & PP_ITM__BIT_BUFFER_OVERFLOW  ? " BIT_OVER" : ""),
		        (task.StsReg.pp_its & PP_ITM__READ_ERROR           ? " R_ERR" : ""),
		        (task.StsReg.pp_its & PP_ITM__WRITE_ERROR          ? " W_ERR" : ""));
		ret = -EFAULT;
	}

	//pr_info("H264pp: %s Returns task %d\n", __func__, task.id);

	*OutParam = task.OutParam;

	return Status;
}

// ////////////////////////////////////////////////////////////////////////////////
static OSDEV_IoctlEntrypoint(H264ppIoctl)
{
	OSDEV_Status_t            Status = OSDEV_NoError;
	struct H264ppOpenContext  *instance;
	h264pp_ioctl_queue_t      InParam;
	h264pp_ioctl_dequeue_t    OutParam;

	OSDEV_IoctlEntry();
	instance = (struct H264ppOpenContext *)OSDEV_PrivateData;
	BUG_ON(!instance);

	switch (OSDEV_IoctlCode) {
	case H264_PP_IOCTL_QUEUE_BUFFER:
		OSDEV_CopyToDeviceSpace(&InParam, OSDEV_ParameterAddress,
		                        sizeof(h264pp_ioctl_queue_t));
		Status = H264ppIoctlQueueBuffer(instance, &InParam);
		break;

	case H264_PP_IOCTL_GET_PREPROCESSED_BUFFER:
		Status = H264ppIoctlGetPreprocessedBuffer(instance, &OutParam);
		OSDEV_CopyToUserSpace(OSDEV_ParameterAddress,
		                      &OutParam, sizeof(h264pp_ioctl_dequeue_t));
		break;

	default:
		pr_err("Error: %s Invalid ioctl %08x\n", __func__, OSDEV_IoctlCode);
		Status = OSDEV_Error;
		break;
	}

	OSDEV_IoctlExit(Status);
}

static OSDEV_Descriptor_t H264ppDeviceDescriptor = {
	.Name = "H264 Pre-processor Module",
	.MajorNumber = MAJOR_NUMBER,
	.OpenFn  = H264ppOpen,
	.CloseFn = H264ppClose,
	.IoctlFn = H264ppIoctl,
	.MmapFn  = NULL
};

// ////////////////////////////////////////////////////////////////////////////////

irqreturn_t H264ppInterruptHandler(int irq, void *data)
{
	struct H264ppCore *core = (struct H264ppCore *)data;
	struct H264ppTask *task = core->current_task;

	//pr_info("H264pp: IRQ handler end of task %d\n", task->id);

	task->StsReg.pp_its  = readl((volatile void *)(core->hw_base_addr + PP_ITS));
	task->StsReg.pp_isbg = readl((volatile void *)(core->hw_base_addr + PP_ISBG));
	task->StsReg.pp_ipbg = readl((volatile void *)(core->hw_base_addr + PP_IPBG));
	task->StsReg.pp_wdl  = readl((volatile void *)(core->hw_base_addr + PP_WDL));

	writel(0xffffffff, (volatile void *)(core->hw_base_addr + PP_ITS));    // Clear interrupt status

	complete(&task->hw_ended);


	return IRQ_HANDLED;
}

// ////////////////////////////////////////////////////////////////////////////////
static int h264pp_get_of_pdata(struct platform_device *pdev, struct H264pp *h264pp)
{
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
#ifndef CONFIG_ARCH_STI
	unsigned char clk_alias[20];
	char *clkName;
	int i;
#endif
	struct H264ppCore *core;
	int ret = 0;

	ret = of_property_read_u32(np, "stbus-opc-size", &h264pp->stbus_max_opcode_size);
	if (ret < 0) {
		pr_err("Error: %s to get OF property stbus-opc-size\n", __func__);
		h264pp->stbus_max_opcode_size = H264PP_MAX_OPC_SIZE_DEFAULT;
	}
	ret = of_property_read_u32(np, "stbus-chunk-size",  &h264pp->stbus_max_chunk_size);
	if (ret < 0) {
		pr_err("Error: %s to get OF property stbus-chunk-size\n", __func__);
		h264pp->stbus_max_chunk_size = H264PP_MAX_CHUNK_SIZE_DEFAULT;
	}

	ret = of_property_read_u32(np, "stbus-msg-size", &h264pp->stbus_max_msg_size);
	if (ret < 0) {
		pr_err("Error: %s to get OF property stbus-msg-size\n", __func__);
		h264pp->stbus_max_msg_size = H264PP_MAX_MESSAGE_SIZE_DEFAULT;
	}
#ifdef CONFIG_ARCH_STI
	//Assume that we have only one device/clock
	core = &h264pp->core[0];
	core->clk = devm_clk_get(&pdev->dev, "clk_h264pp_0");
	if (IS_ERR(core->clk)) {
		pr_err("Error: %s failed to get h264pp clock\n", __func__);
		return PTR_ERR(core->clk);
	}

	ret = H264ClockSetRate(pdev->dev.of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for h264pp clock\n", __func__);
		return -EINVAL;
	}

#else
	for (i = 0; i < h264pp->pp_nb ; i++) {
		core = &h264pp->core[i];
		ret = of_property_read_string_index(np, "st,dev_clk", i, (const char **)&clkName);
		if (ret < 0) {
			pr_err("Error: %s to get OF clock name %d\n", __func__, i);
			return -EINVAL;
		}

		snprintf(clk_alias, sizeof(clk_alias), "clk_h264pp_%d", i);
		clk_alias[sizeof(clk_alias) - 1] = '\0';

		clk_add_alias(clk_alias, NULL, clkName, NULL);
		core->clk = devm_clk_get(&pdev->dev, clk_alias);

		if (IS_ERR(core->clk)) {
			pr_err("Error: %s failed to get h264pp clock[%d]\n", __func__, i);
			return PTR_ERR(core->clk);
		}
	}
#endif //CONFIG_ARCH_STI
#endif //CONFIG_OF

	return 0;
}

int h264pp_get_pdata(struct platform_device *pdev, struct H264pp *h264pp)
{
	struct h264pp_platform_data *pdata =
	        (struct h264pp_platform_data *)(pdev->dev.platform_data);
	unsigned char clk_alias[20];
	struct H264ppCore *core;
	int i;

	h264pp->stbus_max_opcode_size = H264PP_MAX_OPC_SIZE_DEFAULT;
	h264pp->stbus_max_chunk_size = H264PP_MAX_CHUNK_SIZE_DEFAULT;
	h264pp->stbus_max_msg_size = H264PP_MAX_MESSAGE_SIZE_DEFAULT;

	for (i = 0; i < h264pp->pp_nb ; i++) {
		core = &h264pp->core[i];

		snprintf(clk_alias, sizeof(clk_alias), "clk_h264pp_%d", i);
		clk_alias[sizeof(clk_alias) - 1] = '\0';

		clk_add_alias(clk_alias, NULL, pdata->clockName[i], NULL);
		core->clk = devm_clk_get(&pdev->dev, clk_alias);

		if (IS_ERR(core->clk)) {
			pr_err("Error: %s failed to get h264pp clock[%d]\n", __func__, i);
			return PTR_ERR(core->clk);
		}
	}
	return 0;
}

static int H264ppProbe(struct platform_device *pdev)
{
	OSDEV_Status_t Status = OSDEV_NoError;
	struct H264ppCore *core = NULL;
	struct resource *iomem;
	int ret = 0;
	int i = 0;

	BUG_ON(pdev == NULL);

	h264_thread_desc.policy   = thpolprio_inits[0];
	h264_thread_desc.priority = thpolprio_inits[1];
	pr_info("%s %s pol:%d prio:%d\n", __func__, h264_thread_desc.name,
	        h264_thread_desc.policy, h264_thread_desc.priority);

	h264pp = devm_kzalloc(&pdev->dev, sizeof(*h264pp), GFP_KERNEL);
	if (!h264pp) {
		pr_err("Error: %s struct alloc failed\n", __func__);
		ret = -EINVAL;
		goto h264pp_bail;
	}

	/**
	 * Saving pointer to main struct to be able to retrieve it in pm_runtime
	 * callbacks for example (it is *NOT* platform device data, just *driver* data)
	 */
	platform_set_drvdata(pdev, h264pp);
	h264pp->dev = &pdev->dev;

	/* Platform include IOMEM and IRQs */
	h264pp->pp_nb = pdev->num_resources / 2;
	BUG_ON(h264pp->pp_nb > H264_PP_MAX_SUPPORTED_PRE_PROCESSORS);
	pr_info("H264pp: hw core nb: %d\n", h264pp->pp_nb);
	h264pp->core = devm_kzalloc(&pdev->dev, h264pp->pp_nb * sizeof(*h264pp->core), GFP_KERNEL);
	if (!h264pp->core) {
		pr_err("Error: %s core struct alloc failed\n", __func__);
		ret = -EINVAL;
		goto h264pp_bail;
	}

	sema_init(&h264pp->pp_unactive_lock, h264pp->pp_nb);
	spin_lock_init(&h264pp->core_lock);

	if (pdev->dev.of_node) {
		ret = h264pp_get_of_pdata(pdev, h264pp);
	} else {
		pr_warn("warning: %s Probing device without DT\n", __func__);
		ret = h264pp_get_pdata(pdev, h264pp);
	}
	if (ret) {
		pr_err("Error: %s failed to retrieve driver data\n", __func__);
		goto h264pp_bail;
	}

	for (i = 0; i < h264pp->pp_nb ; i++) {
		core = &h264pp->core[i];

		snprintf(core->name, sizeof(core->name), "%s_%d", H264_PP_DEVICE_NAME, i);
		core->name[sizeof(core->name) - 1] = '\0';

		mutex_init(&core->hw_lock);
		spin_lock_init(&core->clk_lock);
		core->inuse = false;
		core->need_soft_reset = false;

		iomem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!iomem) {
			pr_err("Error: %s Get ressource MEM[%d] failed\n", __func__, i);
			ret = -ENODEV;
			goto h264pp_bail;
		}
		core->regs_size = iomem->end - iomem->start + 1;

		core->hw_base_addr =
		        devm_ioremap_nocache(&pdev->dev, iomem->start, core->regs_size);
		if (!core->hw_base_addr) {
			pr_err("Error: %s ioremap registers region[%d] failed\n", __func__, i);
			ret = -ENOMEM;
			goto h264pp_bail;
		}

		core->irq_its = platform_get_irq(pdev, i);
		if (core->irq_its <= 0) {
			pr_err("Error: %s Get ressource IRQ[%d] failed\n", __func__, i);
			ret = -ENODEV;
			goto h264pp_bail;
		}

		ret = devm_request_irq(&pdev->dev, core->irq_its, H264ppInterruptHandler,
		                       IRQF_DISABLED, core->name, (void *)core);
		if (ret) {
			pr_err("Error: %s Request IRQ 0x%x failed\n", __func__, core->irq_its);
			goto h264pp_bail;
		}
		disable_irq(core->irq_its);

		ret = clk_prepare(core->clk);
		if (ret) {
			pr_err("Error: %s failed to prepare h264pp clock[%d]\n", __func__, i);
			goto h264pp_bail;
		}
		enable_irq(core->irq_its);
	}

	Status = OSDEV_RegisterDevice(&H264ppDeviceDescriptor);
	if (Status != OSDEV_NoError) {
		pr_err("Error: %s failed to get major %d\n", __func__, MAJOR_NUMBER);
		ret = -ENODEV;
		goto h264pp_bail;
	}

	Status = OSDEV_LinkDevice(H264_PP_DEVICE, MAJOR_NUMBER, 0);
	if (Status != OSDEV_NoError) {
		OSDEV_DeRegisterDevice(&H264ppDeviceDescriptor);
		ret = -ENODEV;
		goto h264pp_bail;
	}

#ifdef CONFIG_PM_RUNTIME
	/* Clear the device's 'power.runtime_error' flag and set the device's runtime PM status to 'suspended' */
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);
	pm_runtime_enable(&pdev->dev);
#endif

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s probe done ok\n", H264_PP_DEVICE_NAME);

	return 0;

h264pp_bail:
	if (core) {
		for (i = 0 ; i < h264pp->pp_nb ; i++)
			if (h264pp->core[i].clk) {
				clk_unprepare(core->clk);
			}
	}

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_err("Error: %s %s probe failed\n", __func__, H264_PP_DEVICE_NAME);

	return ret;
}

// ////////////////////////////////////////////////////////////////////////////////

static int H264ppRemove(struct platform_device *pdev)
{
	struct H264pp *h264pp;
	struct H264ppCore *core;
	int i = 0;

	BUG_ON(pdev == NULL);

	h264pp = dev_get_drvdata(&pdev->dev);
	BUG_ON(h264pp == NULL);

	for (i = 0 ; i < h264pp->pp_nb ; i++) {
		core = &h264pp->core[i];
		disable_irq(core->irq_its);
		clk_unprepare(core->clk);
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
#endif

	OSDEV_UnLinkDevice(H264_PP_DEVICE);

	OSDEV_DeRegisterDevice(&H264ppDeviceDescriptor);

	dev_set_drvdata(&pdev->dev, NULL);

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s remove done\n", H264_PP_DEVICE_NAME);

	return 0;
}

// ////////////////////////////PM_RUNTIME///////////////////////////////////////////
#ifdef CONFIG_PM_RUNTIME
/**
 * pm_runtime_resume callback for Active Standby
 * called if pm_runtime ref counter == 0 (and for Active Standby)
 */
static int H264ppPmRuntimeResume(struct device *dev)
{
	//struct H264pp *h264pp = dev_get_drvdata(dev);

	return 0;
}
/**
 * pm_runtime_suspend callback for Active Standby
 * called if pm_runtime ref counter == 0 (and for Active Standby)
 */
static int H264ppPmRuntimeSuspend(struct device *dev)
{
	//struct H264pp *h264pp = dev_get_drvdata(dev);

	return 0;
}
#endif
// ////////////////////////////////PM///////////////////////////////////////////
#ifdef CONFIG_PM
/** Resume callback, being called on Host Passive Standby exit (HPS exit) */
static int H264ppPmResume(struct device *dev)
{
	int ret;
	pr_info("%s : HPS exit (pmResume)\n", __func__);

	ret = H264ClockSetRate(dev->of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for h264pp clock\n", __func__);
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
static int H264ppPmSuspend(struct device *dev)
{
	int i;
	struct H264pp *h264pp = dev_get_drvdata(dev);

	pr_info("%s : HPS enter (pmSuspend)\n", __func__);
	BUG_ON(!h264pp);
	/**
	* Here we assume that there are no more jobs in progress.
	* This is ensured at SE level, which has already entered "low power state",
	* in PM notifier callback of player2 module (on PM_SUSPEND_PREPARE event)
	*/
	for (i = 0; i < h264pp->pp_nb; i++) {
		if (h264pp->core[i].inuse) {
			pr_err("Error: %s : Core %d used (should not happen)\n", __func__, i);
		}
	}
	return 0;
}
/** Restore callback, being called on Controller Passive Standby exit (CPS exit) */
static int H264ppPmRestore(struct device *dev)
{
	int ret;
	pr_info("%s : CPS exit (pmRestore)\n", __func__);

	ret = H264ClockSetRate(dev->of_node);
	if (ret < 0) {
		pr_err("Error: %s Failed to set max frequency for h264pp clock\n", __func__);
		return -EINVAL;
	}
	/**
	 * Here we assume that no new jobs have been posted yet.
	 * This is ensured at SE level, which will exit "low power state" later on,
	 * in PM notifier callback of player2 module (on PM_POST_HIBERNATION event).
	 * Also we have to ensure that H264pp driver will be able to restore all necessary registers configuration.
	 */

	return 0;
}
/** Freeze callback, being called on Controller Passive Standby entry (CPS entry) */
static int H264ppPmFreeze(struct device *dev)
{
	int i;
	struct H264pp *h264pp = dev_get_drvdata(dev);

	pr_info("%s : CPS enter (pmFreeze)\n", __func__);
	BUG_ON(!h264pp);
	/**
	 * Here we assume that there are no more jobs in progress.
	 * This is ensured at SE level, which has already entered "low power state",
	 * in PM notifier callback of player2 module (on PM_HIBERNATION_PREPARE event).
	 * Also we have to ensure that H264pp driver will be able to restore all necessary registers configuration on CPS exit.
	 */
	for (i = 0; i < h264pp->pp_nb; i++) {
		if (h264pp->core[i].inuse) {
			pr_err("Error: %s : Core %d used (should not happen)\n", __func__, i);
		}
	}
	return 0;
}

static struct dev_pm_ops H264ppPmOps = {
	.resume  = H264ppPmResume,
	.suspend = H264ppPmSuspend,
	.restore = H264ppPmRestore,
	.freeze  = H264ppPmFreeze,
#ifdef CONFIG_PM_RUNTIME
	.runtime_resume   = H264ppPmRuntimeResume,
	.runtime_suspend  = H264ppPmRuntimeSuspend,
#endif
};
#endif // CONFIG_PM

#ifdef CONFIG_OF
static struct of_device_id stm_h264pp_match[] = {
	{
		.compatible = "st,se-h264pp",
	},
	{},
};
//MODULE_DEVICE_TABLE(of, stm_h264pp_match);
#endif

static struct platform_driver H264ppDriver = {
	.driver = {
		.name = H264_PP_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_h264pp_match),
#ifdef CONFIG_PM
		.pm = &H264ppPmOps,
#endif
	},
	.probe = H264ppProbe,
	.remove = H264ppRemove,
};

module_platform_driver(H264ppDriver);
