/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#if !defined(CONFIG_ARCH_STI)
#include <linux/stm/clk.h>
#endif
#include <linux/platform_device.h>
#include <linux/kref.h>

#if defined(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#if defined(CONFIG_KPTRACE)
#include <linux/relay.h>
#include <trace/kptrace.h>
#endif

#if defined(CONFIG_BPA2)
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/atomic.h>

#if defined(CONFIG_ARM)
#include <asm/mach-types.h>
/*
 * ARM has mis-named this interface relative to all the other platforms
 * that define it (including x86). However this is convenient because we
 * on ARM we are missing all the cache management functions. So we are going
 * to define the cached flag as uncached write combined, which for the display
 * driver's usage is what we really want anyway.
 */
#define ioremap_cache ioremap_wc

/*
 * After v2.6.32, we no longer have flush_ioremap_region on ARM
 * TODO: Decide if we need to ensure a BPA allocated page is not stale
 *       in the cache after ioremapping.
 */
#define flush_ioremap_region(phys, virt, offset, size) do { } while (0)

/*
 * Also define the missing invalidate_ioremap_region to {} so we don't have to
 * conditionalize the code.
 */
#define invalidate_ioremap_region(phys, virt, offset, size) do { } while (0)

#endif

#if defined(CONFIG_STM_DMA)
#include <linux/stm/stm-dma.h>

struct DMA_Channel
{
  long channel_nr;
  void *paced_request;
};

#endif

#if defined(CONFIG_ARCH_STI)
#include <linux/gpio.h>
#else
#if defined(HAVE_BIOS)
#include <linux/stm/st_bios.h>
#endif

#include <linux/stm/gpio.h>
#include <linux/stm/soc.h>
#endif

#ifdef CONFIG_STM_DISPLAY_SDP
#include <smcs_common.h>
#include <smcs.h>
#endif

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#if (defined(CONFIG_STM_FDMA) || defined(CONFIG_ST_FDMA))
#if defined(CONFIG_STM_FDMA)
#include <linux/stm/dma.h>
#else
#include <linux/platform_data/dma-st-fdma.h>
#endif

struct DMA_Channel
{
#if defined(CONFIG_STM_FDMA)
  char *dev_name;
  struct stm_dma_paced_config config;
#else
  const char *dev_name;
  struct st_dma_paced_config config;
#endif
  STM_DMA_TRANSFER_PACING pacing;
  STM_DMA_TRANSFER_FLAGS flags;
  uint32_t bytes_per_req;
  struct dma_chan *channel;
};

/* provide some compatibility for older kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static inline struct dma_async_tx_descriptor *dmaengine_prep_slave_sg(
    struct dma_chan *chan, struct scatterlist *sgl, unsigned int sg_len,
    enum dma_transfer_direction dir, unsigned long flags)
{
    return chan->device->device_prep_slave_sg(chan, sgl, sg_len,
                          dir, flags);
}
#endif

#endif

static const int debug_io_rw          = 0; /* Set to 1 to trace with delay all register reads and writes for debug */
static const int disable_io_rw        = 0; /* Set to 1 to disable all register reads and writes for debug */

static const int dmaarea_alloc_debug  = 0;
static const int dmaarea_access_debug = 0;

static const uint32_t botguard0  = 0x12345678;
static const uint32_t botguard1  = 0x87654321;
static const uint32_t topguard0  = 0x89abcdef;
static const uint32_t topguard1  = 0xfedcba98;
static const uint32_t guardfreed = 0x000f3eed;


/* trace */
#define MAX_LENGTH_ARG 255
#define MAX_LENGTH_FCT 128
#define MAX_STRING_LENGTH 50

/************************************************************/

#ifdef DEBUG
#include <thread_vib_settings.h>

#define ASYNC_PRINT_MAX_STRING_LENGTH   300
#define ASYNC_PRINT_SLOTS_NBR           100

typedef struct async_print_slot_s
{
    char    text[ASYNC_PRINT_MAX_STRING_LENGTH];
} async_print_slot_t;

static VIBE_OS_WaitQueue_t async_wait_queue = 0;
static void*        async_print_thread_desc = NULL;

static bool         async_print_trace_lost  = false;
static uint32_t     async_print_read_index  = 0;
static uint32_t     async_print_write_index = 0;

async_print_slot_t  async_slots[ASYNC_PRINT_SLOTS_NBR];

static bool         start_async_print_thread(void);
static bool         stop_async_print_thread(void);
static void         async_print_thread(void *data);

static int thread_vib_asyncprint[2] = { THREAD_VIB_ASYNCPRINT_POLICY, THREAD_VIB_ASYNCPRINT_PRIORITY };
module_param_array_named(thread_VIB_AsyncPrint,thread_vib_asyncprint, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_AsyncPrint, "VIB-AsyncPrint thread:s(Mode),p(Priority)");

#endif /* DEBUG */

/************************************************************/

typedef struct _RESOURCE_LOCK
{
  spinlock_t    theLock;
  unsigned long lockFlags;
} RESOURCE_LOCK;


struct stm_dma_transfer_handle
{
  int channel;
#if defined(CONFIG_ST_FDMA)
  struct st_dma_params *params;
#else
  struct stm_dma_params *params;
#endif
};


#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
/*
 * The virtual platform developers want to prevent firmwares being loaded.
 * They have been told that (a) this is wrong and (b) this change will not
 * actually stop a lot of firmwares actually being loaded, because they
 * are currently hardcoded into the source. But for the moment this is a
 * minimal version of the change they wanted.
 */
const int DISABLE_FIRMWARE_LOADING = 1;
#else
const int DISABLE_FIRMWARE_LOADING = 0;
#endif

static const char *video_memory_partition  = "coredisplay-video";
static const char *system_memory_partition = "coredisplay-video"; /*"bigphysarea";*/
#ifdef CONFIG_STM_DISPLAY_SDP
static const char *secure_memory_partition = "display-secure";
#endif

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#define FIRMWARE_NAME_MAX 30
typedef struct
{
  struct list_head       fw_list;
  struct platform_device *pdev;
  STMFirmware            firmware;
  const struct firmware *lnx_firmware;
  char                   name[FIRMWARE_NAME_MAX + 1];

  struct kref            kref;
} STMFirmware_private;

struct firmware_data {
  struct list_head fw_cache;
};

static struct firmware_data firmware_data = {
  .fw_cache = LIST_HEAD_INIT (firmware_data.fw_cache),
};
#endif

#if defined(CONFIG_STM_DMA)
static int dma_req_lines[SDTP_LAST_ENTRY] = {-1, -1, -1, -1};
static int dma_first_usable_channel = -1; /* Default, search by capability */
static int dma_last_usable_channel = -1; /* Default, search by capability */
#endif

#if (defined(CONFIG_STM_FDMA) || defined(CONFIG_ST_FDMA))
enum
{
  STM_FDMA_XBAR,
  STM_FDMA_DIRECT
};
static int dma_req_lines[SDTP_LAST_ENTRY] = {-1, -1, -1, -1};
#if defined(CONFIG_ST_FDMA)
static const char *dma_dev_name[SDTP_LAST_ENTRY] = {NULL, NULL, NULL, NULL};
static struct device_node *dma_np[SDTP_LAST_ENTRY] = {NULL, NULL, NULL, NULL};
#else
static char *dma_dev_name[SDTP_LAST_ENTRY] = {NULL, NULL, NULL, NULL};
#endif
static int dma_connection[SDTP_LAST_ENTRY] = {STM_FDMA_XBAR, STM_FDMA_XBAR, STM_FDMA_XBAR, STM_FDMA_XBAR};
#endif

static void determine_dma_req_lines(struct device * device)
{
#if (defined(CONFIG_STM_DMA) || defined(CONFIG_STM_FDMA) || defined(CONFIG_ST_FDMA))
#if defined(CONFIG_ARCH_STI)
  struct device_node *sdtp_node, *dma_node;
  int id;
#endif
  memset(dma_req_lines, -1, sizeof(dma_req_lines));
#endif
  /* ARM Based SoCs, 3.3.x kernel */

  /*
   * NOTE: When the new kernel using device trees is released we will need to
   *       move all of this configuration out to a device tree node with a
   *       set of properties that replicate this information. The machine_is..
   *       macros are not there in a device tree kernel and the CONFIG_MACH
   *       definitions will be unreliable because more than one may be defined
   *       if you are building against a unified device ARM kernel (i.e. one
   *       kernel image that can be booted on multiple SoCs).
   */
#if defined(CONFIG_ARM) && (defined(CONFIG_STM_FDMA) || defined(CONFIG_ST_FDMA))
#if defined(CONFIG_ARCH_STI)
  sdtp_node = of_get_child_by_name(device->of_node, "sdtp");
  if(!sdtp_node)
  {
    dev_warn(device, "No sdtp node found\n");
  }
  else
  {
    dma_node = NULL;
    while((dma_node = of_get_next_child(sdtp_node, dma_node)))
    {
      of_property_read_u32(dma_node, "dma-id", &id);
      dev_dbg(device, "dma-id=%d\n", id);
      if(id >= 0)
      {
        /* get dma node */
        if(!IS_ERR_OR_NULL(of_get_property(dma_node, "dmas", NULL)))
        {
          dma_np[id] = of_find_node_by_phandle(be32_to_cpup(of_get_property(dma_node, "dmas", NULL)));
          dev_dbg(device, "fdma-dt-node=%p\n", dma_np[id]);
        }
        of_property_read_string(dma_node, "dma-names", &dma_dev_name[id]);
        dev_dbg(device, "dma-name=%s\n", dma_dev_name[id]);
        of_property_read_u32(dma_node, "fdma-direct-conn", &dma_connection[id]);
        dev_dbg(device, "fdma-direct-conn=%d\n", dma_connection[id]);
        of_property_read_u32(dma_node, "fdma-request-line", &dma_req_lines[id]);
        dev_dbg(device, "fdma-request-line=%d\n", dma_req_lines[id]);
      }
      of_node_put(dma_node); /* free the node */
    }
    of_node_put(sdtp_node);
  }
#else
#if defined(CONFIG_MACH_STM_STIH416_VSOC) || defined(CONFIG_MACH_STM_STIH416)
  {
    dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
    dma_req_lines[SDTP_DENC1_TELETEXT] = 32;
    dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
    dma_dev_name[SDTP_UNPACED]         = "stm-fdma.0";
    dma_dev_name[SDTP_DENC1_TELETEXT]  = "stm-fdma.3";
    printk(KERN_INFO "%s: pacing request channel is %s[%d]\n",__FUNCTION__,dma_dev_name[SDTP_DENC1_TELETEXT], dma_req_lines[SDTP_DENC1_TELETEXT]);
  }
#elif defined(CONFIG_MACH_STM_STIH407_VSOC) || defined(CONFIG_MACH_STM_STIH407)
  {
    dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
    /* TODO: This is a direct pacing line, it does not go through the x-bar,
     *       it will require additional setup when the updated FDMA driver
     *       to support this functionality is available in the kernel. This new
     *       driver functionality is currently being developed for SASC1.
     */
    dma_req_lines[SDTP_DENC1_TELETEXT]  = 8;
    dma_req_lines[SDTP_DENC2_TELETEXT]  = -1;
    dma_dev_name[SDTP_UNPACED]          = "stm-fdma.0";
    dma_dev_name[SDTP_DENC1_TELETEXT]   = "stm-fdma.0"; /* Assuming this will be the name of FDMA_11 */
    dma_connection[SDTP_UNPACED]        = STM_FDMA_DIRECT;
    dma_connection[SDTP_DENC1_TELETEXT] = STM_FDMA_DIRECT;
    printk(KERN_INFO "%s: pacing request channel is %s[%d]\n",__FUNCTION__,dma_dev_name[SDTP_DENC1_TELETEXT], dma_req_lines[SDTP_DENC1_TELETEXT]);
  }
#else
  {
    printk(KERN_WARNING "%s: DMA pacing request is not supported by chip\n",__FUNCTION__);
  }
#endif
#endif
#endif /* defined(CONFIG_ARM) && defined(CONFIG_STM_FDMA) */
}


/*
 * We need a device for the IOS implementation in order to be able to use
 * the dma-mapping interfaces.
 */
static void ios_dev_release (struct device * device) { }

static struct device ios_device = {
  .init_name         = "stm-ios",
  .coherent_dma_mask = DMA_BIT_MASK(32),
  .release           = ios_dev_release
};

int vibe_os_init(void)
{
  int   failure;

  if(!device_is_registered(&ios_device))
  {
    failure = device_register (&ios_device);
    if (unlikely (failure))
    {
      dev_err (&ios_device, "Device registration [%s] failed: %d\n", dev_name(&ios_device), failure);
      return -1;
    }
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
    dev_set_drvdata(&ios_device, &firmware_data);
#endif
    ios_device.of_node = of_find_compatible_node(NULL,NULL,"st,display");
  }

  determine_dma_req_lines(&ios_device);

#ifdef DEBUG
  if (!start_async_print_thread())
  {
      dev_err (&ios_device, "Failed to start the VIBE ASYNC thread\n");
      return -1;
  }
#endif

  return 0;
}

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
static void firmware_release (struct device * device);
#endif

void vibe_os_release(void)
{
#ifdef DEBUG
  if (!stop_async_print_thread())
  {
      dev_err (&ios_device, "Failed to stop the VIBE ASYNC thread\n");
  }
#endif

  if (likely (device_is_registered(&ios_device)))
  {
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
    firmware_release(&ios_device);
#endif
    of_node_put(ios_device.of_node);
    device_unregister(&ios_device);
  }
}


bool vibe_os_create_thread(VIBE_OS_ThreadFct_t          fct,
                         void                          *parameter,
                         const char                    *name,
                         const VIBE_OS_thread_settings *thread_settings,
                         void                         **thread_desc)
{
  struct task_struct **thread = (struct task_struct **) thread_desc;
  int                  sched_policy;
  struct sched_param   sched_param;

  /* Create thread */
  *thread = kthread_create(fct, parameter, name);
  if( *thread == NULL)
  {
    TRC(TRC_ID_ERROR, "FAILED to create thread %s", name);
    return false;
  }

  /* Set thread scheduling settings */
  if( thread_settings == NULL )
  {
    TRC(TRC_ID_ERROR, "FAILED no thread_settings for thread %s", name);
    return false;
  }
  sched_policy               = thread_settings->policy;
  sched_param.sched_priority = thread_settings->priority;
  if ( sched_setscheduler(*thread, sched_policy, &sched_param) )
  {
    TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
        name, sched_policy, sched_param.sched_priority);
    return false;
  }

  return true;
}

int vibe_os_stop_thread(void *thread_desc)
{
  struct task_struct *thread = (struct task_struct *) thread_desc;

  return (kthread_stop(thread));
}

bool vibe_os_wake_up_thread(void *thread_desc)
{
  struct task_struct *thread = (struct task_struct *) thread_desc;

  wake_up_process(thread);

  return true;
}

bool vibe_os_thread_should_stop(void)
{
    return (kthread_should_stop());
}


void vibe_os_sleep_ms(uint32_t ms)
{
  if (ms > 0)
  {
    if (ms < 20)
    {
        usleep_range(ms, ms+1);
    }
    else
    {
      msleep(ms);
    }
  }
}


uint32_t vibe_os_read_register(volatile uint32_t *baseAddress, uint32_t offset)
{
  if(unlikely(debug_io_rw))
  {
    printk("vibe_os_read_register(%p,0x%08x)\n",baseAddress,offset);
    mdelay(1);
  }
  if(likely(!disable_io_rw))
    return readl(baseAddress+(offset>>2));
  else
    return 0;
}


void vibe_os_write_register(volatile uint32_t *baseAddress, uint32_t offset, uint32_t val)
{
  if(unlikely(debug_io_rw))
  {
    printk("vibe_os_write_register(%p,0x%08x,0x%08x)\n",baseAddress,offset,val);
    mdelay(1);
  }

  if(likely(!disable_io_rw))
    writel(val, baseAddress+(offset>>2));
}


uint8_t vibe_os_read_byte_register(volatile uint8_t *reg)
{
  if(unlikely(debug_io_rw))
  {
    printk("vibe_os_read_byte_register(%p)\n",reg);
    mdelay(1);
  }

  if(likely(!disable_io_rw))
    return readb(reg);
  else
    return 0;
}


void vibe_os_write_byte_register(volatile uint8_t *reg,uint8_t val)
{
  if(unlikely(debug_io_rw))
  {
    printk("vibe_os_write_byte_register(%p,0x%08x)\n",reg,(uint32_t)val);
    mdelay(1);
  }

  if(likely(!disable_io_rw))
    writeb(val, reg);
}


vibe_time64_t vibe_os_get_system_time(void)
{
  struct timespec ts;

  getrawmonotonic(&ts);

  return ktime_to_us(timespec_to_ktime(ts));
}


vibe_time64_t vibe_os_get_one_second(void)
{
  return USEC_PER_SEC;
}


void vibe_os_stall_execution(uint32_t ulDelay)
{
  uint32_t ulms = ulDelay/1000;
  uint32_t ulus = ulDelay%1000;

  /* if the delay is greater than one millisecond then use mdelay for each
   * full millisecond as calling udelay with a large number can cause wrapping
   * problems on fast machines.
   */
  if (ulms)
    mdelay(ulms);

  /* Mop up any fraction of a millisecond with a direct call to udelay */
  if (ulus)
    udelay(ulus);

}


void* vibe_os_map_memory(uint32_t ulBaseAddr, uint32_t ulSize)
{
  return ioremap(ulBaseAddr, ulSize);
}


void vibe_os_unmap_memory(void* pVMem)
{
  iounmap(pVMem);
}


/* vibe_os_zero_memory() must not be called for initializing a C++ object
 * because the virtual destructor table will be corrupted.
 * The C++ object constructor must be used for initializing it.
 * This functions is dedicated for structure or table of pointers initialization.
 */
void vibe_os_zero_memory(void* pMem, uint32_t count)
{
  memset(pMem, 0, count);
}


int vibe_os_memcmp(const void *pMem1, const void *pMem2, uint32_t size)
{
    return memcmp(pMem1, pMem2, size);
}


void vibe_os_memcpy(void *pMem1, const void *pMem2, uint32_t size)
{
  memcpy(pMem1, pMem2, size);
}

int vibe_os_check_dma_area_guards(DMA_Area *mem)
{
  uint32_t *guard,*topguard;

  if(!mem->pMemory)
    return 0;

  if(((uint32_t)mem->pMemory & 0x3) != 0)
  {
    printk("vibe_os_check_dma_area_guards: unaligned mem = %p pMemory = %p\n",mem,mem->pMemory);
    return -1;
  }

  guard = (uint32_t*)mem->pMemory;
  topguard = (uint32_t *)(mem->pData+mem->ulDataSize);
  if(mem->ulFlags & SDAAF_IOREMAPPED)
  {
    if((readl(guard) != botguard0) || (readl(guard+1) != botguard1))
    {
      printk("vibe_os_check_dma_area_guards: bad guard before data mem = %p pMemory = %p guard[0] = 0x%x guard[1] = 0x%x\n",mem,mem->pMemory,guard[0],guard[1]);
      return -1;
    }
    if((readl(topguard) != topguard0) || (readl(topguard+1) != topguard1))
    {
      printk("vibe_os_check_dma_area_guards: bad guard after data mem = %p pMemory = %p guard[0] = 0x%x guard[1] = 0x%x\n",mem,mem->pMemory,topguard[0],topguard[1]);
      return -1;
    }
  }
  else
  {
    if((guard[0] != botguard0) || (guard[1] != botguard1))
    {
      printk("vibe_os_check_dma_area_guards: bad guard before data mem = %p pMemory = %p guard[0] = 0x%x guard[1] = 0x%x\n",mem,mem->pMemory,guard[0],guard[1]);
      return -1;
    }
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      printk("vibe_os_check_dma_area_guards: bad guard after data mem = %p pMemory = %p guard[0] = 0x%x guard[1] = 0x%x\n",mem,mem->pMemory,topguard[0],topguard[1]);
      return -1;
    }
  }

  return 0;
}


/*
 * Internal DMA area flushing used by the Flush, Memset and MemcpyTo entrypoints
 */
static inline void flush_dma_area(DMA_Area *mem, uint32_t offset, uint32_t count)
{
  if(mem->ulFlags & SDAAF_IOREMAPPED)
  {
    if (!(mem->ulFlags & SDAAF_UNCACHED))
      flush_ioremap_region(mem->ulPhysical, mem->pData, offset, count);
  }
  else
  {
    uint32_t dataOffset = (uint32_t)mem->pData - (uint32_t)mem->pMemory;
    dma_sync_single_range_for_device(&ios_device, mem->allocHandle,
                                     dataOffset+offset, count, DMA_TO_DEVICE);
  }
  /*
   * And stick in a memory barrier particularly for when we are doing
   * uncached write combined.
   */
  wmb();
}


void vibe_os_flush_dma_area(DMA_Area *mem, uint32_t offset, uint32_t count)
{
  if(dmaarea_access_debug)
    printk("vibe_os_flush_dma_area: mem = %p, pMemory = %p, pData = %p, size = %u offset = %u count = %u\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset > mem->ulDataSize) ||
     (count  > mem->ulDataSize) ||
     ((offset+count) > mem->ulDataSize))
  {
    printk("vibe_os_flush_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
  }

  flush_dma_area(mem,offset,count);
}


void vibe_os_memset_dma_area(DMA_Area *mem, uint32_t offset, int val, uint32_t count)
{
  if(dmaarea_access_debug)
    printk("vibe_os_memset_dma_area: mem = %p, pMemory = %p, pData = %p, size = %u offset = %u count = %u\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset > mem->ulDataSize) ||
     (count  > mem->ulDataSize) ||
     ((offset+count) > mem->ulDataSize))
  {
    printk(KERN_ERR "vibe_os_memset_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
    return;
  }

  if(mem->ulFlags & SDAAF_IOREMAPPED)
  {
    memset_io(mem->pData+offset, val, count);
  }
  else
  {
    memset(mem->pData+offset, val, count);
  }

  vibe_os_check_dma_area_guards(mem);

  flush_dma_area(mem, offset, count);
}


void vibe_os_memcpy_to_dma_area(DMA_Area *mem, uint32_t offset, const void* pSrc, uint32_t count)
{
  if(dmaarea_access_debug)
    printk("vibe_os_memcpy_to_dma_area: mem = %p, pMemory = %p, pData = %p, size = %u offset = %u count = %u\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset > mem->ulDataSize) ||
     (count  > mem->ulDataSize) ||
     ((offset+count) > mem->ulDataSize))
  {
    printk(KERN_ERR "vibe_os_memcpy_to_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
    return;
  }

  if(mem->ulFlags & SDAAF_IOREMAPPED)
  {
    memcpy_toio(mem->pData+offset, pSrc, count);
  }
  else
  {
    memcpy(mem->pData+offset, pSrc, count);
  }

  vibe_os_check_dma_area_guards(mem);

  flush_dma_area(mem, offset, count);
}


void vibe_os_memcpy_from_dma_area(DMA_Area *mem, uint32_t offset, void* pDst, uint32_t count)
{
  if(dmaarea_access_debug)
    printk("vibe_os_memcpy_from_dma_area: mem = %p, pMemory = %p, pData = %p, size = %u offset = %u count = %u\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset > mem->ulDataSize) ||
     (count  > mem->ulDataSize) ||
     ((offset+count) > mem->ulDataSize))
  {
    printk(KERN_ERR "vibe_os_memcpy_from_dma_area: access out of range, size = %u offset = %u count = %u\n",mem->ulDataSize,offset,count);
    return;
  }

  if(mem->ulFlags & SDAAF_IOREMAPPED)
  {
    if (!(mem->ulFlags & SDAAF_UNCACHED))
      invalidate_ioremap_region(mem->ulPhysical, mem->pData, offset, count);

    memcpy_fromio(pDst, mem->pData+offset, count);
  }
  else
  {
    uint32_t dataOffset = (uint32_t)mem->pData - (uint32_t)mem->pMemory;
    dma_sync_single_range_for_device(&ios_device, mem->allocHandle,
                                     dataOffset+offset, count, DMA_FROM_DEVICE);
    memcpy(pDst, mem->pData+offset, count);
  }
}


/* Do do not use this function with a C++ object as it will not call the
 * constructor or destructor (new/delete must be used for C++ object).
 * Also this function can not be mixed with new/delete.
 */
void *vibe_os_allocate_memory(uint32_t size)
{
  char  *mem   = (char*)kmalloc(size+(sizeof(uint32_t)*4), GFP_KERNEL);
  uint32_t *guard = (uint32_t *)mem;

  if(mem == 0)
  {
    printk("vibe_os_allocate_memory: unable to kmalloc %u bytes\n",size);
    return 0;
  }

  guard[0] = size;
  guard[1] = botguard1;

  guard = (uint32_t *)(mem+size+(sizeof(uint32_t)*2));

  guard[0] = topguard0;
  guard[1] = topguard1;

  return mem+(sizeof(uint32_t)*2);
}


/* Do do not use this function with a C++ object as it will not call the
 * constructor or destructor (new/delete must be used for C++ object).
 * Also this function can not be mixed with new/delete.
 */
void vibe_os_free_memory(void *mem)
{
  uint32_t *guard;
  uint32_t *topguard;
  uint32_t size;

  if(mem == 0)
    return;

  guard = (uint32_t *)((char*)mem - (sizeof(uint32_t)*2));

  size  = guard[0];

  if(guard[1] != botguard1)
  {
    printk("vibe_os_free_memory: bad guard before data mem = %p allocation size = %u guard[1] = 0x%x\n",mem,size,guard[1]);
    printk("vibe_os_free_memory: freeing anyway\n");
  }
  else
  {
    topguard = (uint32_t *)((char*)mem+size);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      printk("vibe_os_free_memory: bad guard after data mem = %p topguard[0] = 0x%x topguard[1] = 0x%x\n",mem,topguard[0],topguard[1]);
      printk("vibe_os_free_memory: freeing anyway\n");
    }
  }

  guard[0] = guardfreed;
  guard[1] = guardfreed;

  kfree(guard);

  return;
}


void vibe_os_allocate_bpa2_dma_area(DMA_Area *mem, uint32_t align)
{
  uint32_t         *guard;
  uint32_t          nBytes;
  uint32_t          nPages;
  struct bpa2_part *part;
  const char       *part_name = NULL;
  uint32_t          dataOffset;

  if(dmaarea_alloc_debug)
    printk("vibe_os_allocate_bpa2_dma_area: mem = %p, align = %u\n",mem, align);

  /* Allow room for top and bottom memory guards and for alignment */
  nBytes = mem->ulDataSize + sizeof(uint32_t)*4 + align;
  nPages = (nBytes + PAGE_SIZE - 1) / PAGE_SIZE;

  /*
   * Adjust allocation size as bpa2 allocates whole pages
   */
  mem->ulAllocatedSize = nPages*PAGE_SIZE;

  if (mem->ulFlags & SDAAF_VIDEO_MEMORY)
    part_name = video_memory_partition;
  else if (mem->ulFlags & SDAAF_SECURE)
#ifdef CONFIG_STM_DISPLAY_SDP
    part_name = secure_memory_partition;
#else
    part_name = video_memory_partition;
#endif
  else
    part_name = system_memory_partition;

  part = bpa2_find_part(part_name);
  if(part)
    mem->allocHandle = bpa2_alloc_pages(part, nPages, 1, GFP_KERNEL);
  else
    printk("%s: BPA2 partition '%s' not available in this kernel\n",
           __PRETTY_FUNCTION__, part_name);

  if(mem->allocHandle == 0)
  {
    if(dmaarea_alloc_debug)
      printk("vibe_os_allocate_bpa2_dma_area: allocation failed\n");

    memset(mem, 0, sizeof(DMA_Area));
    return;
  }

  /*
   * Remap physical.
   */
  mem->pMemory = (mem->ulFlags & SDAAF_UNCACHED)
                 ? ioremap_nocache(mem->allocHandle, mem->ulAllocatedSize)
                 : ioremap_cache(mem->allocHandle, mem->ulAllocatedSize);

  if(mem->pMemory == 0)
  {
    if(dmaarea_alloc_debug)
      printk("vibe_os_allocate_bpa2_dma_area: ioremap failed\n");

    memset(mem, 0, sizeof(DMA_Area));
    return;
  }

  mem->ulFlags |= SDAAF_IOREMAPPED;

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    flush_ioremap_region(mem->allocHandle, mem->pMemory, 0, mem->ulAllocatedSize);

  /* Move start of data up to allow room for the bottom memory guard */
  mem->pData = (char*)((uint32_t)mem->pMemory + sizeof(uint32_t)*2);

  /* Now align the data area in the standard fashion */
  if(align>0)
    mem->pData = (char*)(((uint32_t)mem->pData + (align-1)) & ~(align-1));

  /* Adjust the physical for the alignment */
  dataOffset = (uint32_t)mem->pData - (uint32_t)mem->pMemory;
  mem->ulPhysical = mem->allocHandle + dataOffset;

  /*
   * Write guards using IO functions
   */
  guard = (uint32_t*)mem->pMemory;
  writel(botguard0, guard);
  writel(botguard1, guard+1);

  guard = (uint32_t*)(mem->pData+mem->ulDataSize);
  writel(topguard0, guard);
  writel(topguard1, guard+1);
}


void vibe_os_allocate_kernel_dma_area(DMA_Area *mem, uint32_t align)
{
  uint32_t *guard;
  uint32_t  dataOffset;

  if(dmaarea_alloc_debug)
    printk("vibe_os_allocate_kernel_dma_area: mem = %p, align = %u\n",mem, align);

  /*
   * Allow room for top and bottom memory guards and for alignment.
   *
   * NOTE: kmalloc always returns at least a cacheline aligned pointer
   */
  mem->ulAllocatedSize = mem->ulDataSize + sizeof(uint32_t)*4 + align;

  if((mem->pMemory = kmalloc(mem->ulAllocatedSize, GFP_KERNEL)) == NULL)
  {
    memset(mem, 0, sizeof(DMA_Area));
    return;
  }

  /* Move start of data up to allow room for the bottom memory guard */
  mem->pData = (char*)((uint32_t)mem->pMemory + sizeof(uint32_t)*2);

  /* Now align the data area in the standard fashion */
  if(align>0)
    mem->pData = (char*)(((uint32_t)mem->pData + (align-1)) & ~(align-1));

  mem->allocHandle = dma_map_single(&ios_device, mem->pMemory,mem->ulAllocatedSize, DMA_BIDIRECTIONAL );
  /* Adjust the physical for the data start alignment */
  dataOffset = (uint32_t)mem->pData - (uint32_t)mem->pMemory;
  mem->ulPhysical = mem->allocHandle + dataOffset;

  /*
   * Write guards directly
   */
  guard = (uint32_t*)mem->pMemory; /* Note: NOT pData!!! */
  guard[0] = botguard0;
  guard[1] = botguard1;

  guard = (uint32_t*)(mem->pData+mem->ulDataSize);
  guard[0] = topguard0;
  guard[1] = topguard1;

  dma_sync_single_for_device(&ios_device, mem->allocHandle, mem->ulAllocatedSize, DMA_TO_DEVICE);
}


void vibe_os_allocate_dma_area(DMA_Area *mem, uint32_t size, uint32_t align, STM_DMA_AREA_ALLOC_FLAGS flags)
{
  const int guardSize = sizeof(uint32_t)*4;

  if(dmaarea_alloc_debug)
    printk("vibe_os_allocate_dma_area: mem = %p, size = %u, align = %u, flags = 0x%x\n",mem, size, align, flags);

  if(!mem)
  {
    printk(KERN_ERR "%s: error: mem parameter == NULL\n", __FUNCTION__);
    return;
  }

  memset(mem, 0, sizeof(DMA_Area));

  // Allocate DMA area size of 0
  if(size == 0)
  {
    printk(KERN_ERR "vibe_os_allocate_dma_area: error: size==0\n");
    return;
  }

  mem->ulDataSize      = size;
  mem->ulFlags         = flags & ~SDAAF_IOREMAPPED;

  if((mem->ulDataSize > (PAGE_SIZE-guardSize-align)) ||
     (flags & (SDAAF_VIDEO_MEMORY | SDAAF_UNCACHED | SDAAF_SECURE)))
  {
    vibe_os_allocate_bpa2_dma_area(mem, align);
  }
  else
  {
    vibe_os_allocate_kernel_dma_area(mem, align);
  }

  if(dmaarea_alloc_debug)
    printk("vibe_os_allocate_dma_area: mem = %p, pMemory = %p (%s), pData = %p,\n\t allocated size = %u data size = %u\n",mem,mem->pMemory,(flags&SDAAF_UNCACHED)?"uncached":"cached",mem->pData,mem->ulAllocatedSize,mem->ulDataSize);
}


void vibe_os_free_dma_area(DMA_Area *mem)
{
  uint32_t *guard;

  if(dmaarea_alloc_debug)
    printk("vibe_os_free_dma_area: mem = %p, pMemory = %p, pData = %p, size = %u\n",mem,mem->pMemory,mem->pData,mem->ulDataSize);

  if(!mem->pMemory)
    return;

  if(vibe_os_check_dma_area_guards(mem)<0)
    printk("vibe_os_free_dma_area: ******************* freeing corrupted memory ***************\n");

  if (mem->ulFlags & SDAAF_IOREMAPPED)
  {
    struct bpa2_part *part;

    guard = (uint32_t*)mem->pMemory;
    writel(guardfreed, guard);
    writel(guardfreed, guard+1);

    part = bpa2_find_part((mem->ulFlags & SDAAF_VIDEO_MEMORY)?video_memory_partition:system_memory_partition);

    if(part)
    {
      iounmap(mem->pMemory);
      bpa2_free_pages(part, mem->allocHandle);
    }
  }
  else
  {
    guard = (uint32_t*)mem->pMemory;
    guard[0] = guardfreed;
    guard[1] = guardfreed;

    dma_unmap_single(&ios_device, mem->allocHandle, mem->ulAllocatedSize, DMA_BIDIRECTIONAL);
    kfree(mem->pMemory);
  }

  memset(mem, 0, sizeof(DMA_Area));
}

#if defined(CONFIG_STM_DMA)
static const char *fdmac_id[]    = { STM_DMAC_ID, NULL };
static const char *fdma_cap_lb[] = { STM_DMA_CAP_LOW_BW, NULL };

DMA_Channel *vibe_os_get_dma_channel(STM_DMA_TRANSFER_PACING pacing, uint32_t bytes_per_req, STM_DMA_TRANSFER_FLAGS flags)
{

  DMA_Channel *chan = kzalloc(sizeof(DMA_Channel), GFP_KERNEL);

  if(!chan)
    return NULL;

  if(dma_first_usable_channel == -1)
  {
    chan->channel_nr = request_dma_bycap(fdmac_id, fdma_cap_lb, "coredisplay");
  }
  else
  {
    int ch;
    chan->channel_nr = -1;
    for(ch=dma_first_usable_channel;ch<=dma_last_usable_channel;ch++)
    {
      if(request_dma(ch, "coredisplay"))
        continue;

      chan->channel_nr = ch;
      break;
    }
  }

  if(chan->channel_nr<0)
  {
    kfree(chan);
    printk(KERN_WARNING "%s: unable to get FDMA channel\n",__FUNCTION__);
    return NULL;
  }

  if(pacing!=SDTP_UNPACED)
  {
    struct stm_dma_req_config fdma_req_config = {
            .rw        = REQ_CONFIG_WRITE,
            .opcode    = REQ_CONFIG_OPCODE_4,
            .count     = 0,
            .increment = 0,
            .hold_off  = 0,
            .initiator = 0
    };

    int requestline = dma_req_lines[pacing];

    if(requestline == -1)
    {
      printk(KERN_WARNING "%s: pacing request (%d) not supported by chip\n",__FUNCTION__,pacing);
      goto error_exit;
    }

    fdma_req_config.count = bytes_per_req / 4;
    fdma_req_config.increment = (flags & SDTF_DST_IS_SINGLE_REGISTER)?0:1;

    chan->paced_request = dma_req_config(chan->channel_nr, requestline, &fdma_req_config);
    if(chan->paced_request == NULL)
    {
      printk(KERN_ERR "%s: unable to get pacing (%d) request line %d\n",__FUNCTION__,pacing,requestline);
      goto error_exit;
    }

#ifdef DEBUG
    printk("%s: got get pacing (%d) request line %d\n",__FUNCTION__,pacing,requestline);
#endif
  }

  return chan;

error_exit:
  free_dma(chan->channel_nr);
  kfree(chan);
  return NULL;
}


void vibe_os_release_dma_channel(DMA_Channel *chan)
{
  if(!chan)
    return;

#ifdef DEBUG
    printk("%s: releasing channel %ld pacing request %p\n",__FUNCTION__,chan->channel_nr,chan->paced_request);
#endif

  if(chan->paced_request)
    dma_req_free(chan->channel_nr,(struct stm_dma_req *)chan->paced_request);

  free_dma(chan->channel_nr);
  kfree(chan);
}


void vibe_os_stop_dma_channel(DMA_Channel *chan)
{
  if(!chan)
    return;

  dma_stop_channel(chan->channel_nr);
  dma_wait_for_completion(chan->channel_nr);
}


void* vibe_os_create_dma_transfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags)
{
  DMA_transfer *tmp = transfer_list;
  struct stm_dma_params *params = NULL,*tail = NULL;
  struct stm_dma_transfer_handle *handle;

  if(!transfer_list)
  {
    printk(KERN_ERR "vibe_os_create_dma_transfer: error: transfer_list==NULL\n");
    return NULL;
  }

  if(!chan)
  {
    printk(KERN_ERR "vibe_os_create_dma_transfer: error: chan==NULL\n");
    return NULL;
  }

  if((handle = (struct stm_dma_transfer_handle*)kmalloc(sizeof(struct stm_dma_transfer_handle),GFP_KERNEL)) == NULL)
    return NULL;

  handle->channel = chan->channel_nr;

  if(handle->channel < 0)
  {
    printk(KERN_ERR "%s: DMA channel number invalid\n",__FUNCTION__);
    goto error_exit;
  }


  while(tmp->src != NULL)
  {
    struct stm_dma_params *node;

#ifdef DEBUG
    printk("%s: src =%p dst = %p offset = %u size = %u\n",
           __FUNCTION__, tmp->src, tmp->dst, tmp->src_offset, tmp->size);

    if (vibe_os_check_dma_area_guards(tmp->src)<0)
    {
      printk(KERN_ERR "%s: DMA guard error for src\n", __FUNCTION__);
      goto error_exit;
    }
    if (vibe_os_check_dma_area_guards(tmp->dst)<0)
    {
      printk(KERN_ERR "%s: DMA guard error for dst\n", __FUNCTION__);
      goto error_exit;
    }
#endif

    if((tmp->src_offset+tmp->size) > tmp->src->ulDataSize)
    {
      printk(KERN_ERR "%s: src parameters out of range offset = %u size = %u dmaarea size = %u\n",
                       __FUNCTION__, tmp->src_offset, tmp->size, tmp->src->ulDataSize);
      goto error_exit;
    }

    if(!(flags & SDTF_DST_IS_SINGLE_REGISTER) &&
        (tmp->dst_offset+tmp->size > tmp->dst->ulDataSize))
    {
      printk(KERN_ERR "%s: dst parameters out of range offset = %u size = %u dmaarea size = %u\n",
                       __FUNCTION__, tmp->dst_offset, tmp->size, tmp->dst->ulDataSize);
      goto error_exit;
    }

    node = (struct stm_dma_params *)kmalloc(sizeof(struct stm_dma_params),GFP_KERNEL);
    if(!node)
      goto error_exit;

    dma_params_init(node,(pacing==SDTP_UNPACED)?MODE_FREERUNNING:MODE_PACED,STM_DMA_LIST_OPEN);

    if(flags & SDTF_DST_IS_SINGLE_REGISTER)
      dma_params_DIM_1_x_0(node);
    else
      dma_params_DIM_1_x_1(node);

    dma_params_req(node,(struct stm_dma_req *)chan->paced_request);
    dma_params_addrs(node,tmp->src->ulPhysical+tmp->src_offset,
                          tmp->dst->ulPhysical+tmp->dst_offset,
                          tmp->size);

    if(tmp->completed_cb != NULL)
    {
      /*
       * NOTE: The nasty casting of the callback is OK as we know this code
       *       path will only ever be executed on SH4 where void * and unsigned
       *       long are the same size.
       */
      dma_params_comp_cb(node, (void (*)(unsigned long))tmp->completed_cb, (unsigned long)tmp->cb_param, STM_DMA_CB_CONTEXT_ISR);
    }

#ifdef DEBUG
    printk("%s: 0x%x (%u) -> 0x%x\n",__FUNCTION__,node->sar,node->node_bytes,node->dar);
#endif

    if(!params)
    {
      params = node;
      tail = node;
    }
    else
    {
      tail->next = node;
      tail = node;
    }

    tmp++;
  }

  if(params == NULL)
    goto error_exit;

  if(dma_compile_list(handle->channel,params, GFP_KERNEL)<0)
    goto error_exit;

#ifdef DEBUG
  printk("%s: dma transfer compiled successfully\n",__FUNCTION__);
#endif


  handle->params = params;

  return handle;

error_exit:

  while(params != 0)
  {
    struct stm_dma_params *tmp = params->next;

    if(params->req != NULL)
      dma_req_free(transfer->channel,params->req);

    dma_params_free(params);

    kfree(params);
    params = tmp;
  }

  kfree(handle);

  return NULL;
}


void  vibe_os_delete_dma_transfer(void *handle)
{
  struct stm_dma_transfer_handle *transfer = (struct stm_dma_transfer_handle*)handle;
  struct stm_dma_params *params;

  if(!handle)
    return;

  params = transfer->params;

  /*
   * We have to free the list ourselves.
   */
  while(params)
  {
    struct stm_dma_params *tmp = params->next;

    if(params->req != NULL)
      dma_req_free(transfer->channel,params->req);

    /*
     * This doesn't actually free the list, it frees any internal memory
     * created when the list was compiled.
     */
    dma_params_free(params);

    kfree(params);
    params = tmp;
  }

  kfree(transfer);
}


int vibe_os_start_dma_transfer(void *handle)
{
  int chan_status;
  struct stm_dma_transfer_handle *transfer = (struct stm_dma_transfer_handle*)handle;
  if(!handle)
    return -1;

  chan_status = dma_get_status(transfer->channel);
  if(chan_status != DMA_CHANNEL_STATUS_IDLE)
  {
    if (printk_ratelimit())
      printk(KERN_WARNING "%s: channel %d status = %d\n",__FUNCTION__,transfer->channel,chan_status);
    return -1;
  }

  return dma_xfer_list(transfer->channel,transfer->params);
}
#endif /* CONFIG_STM_DMA */


#if (defined(CONFIG_STM_FDMA) || defined(CONFIG_ST_FDMA))

static bool fdma_filter_fn(struct dma_chan *chan, void *fn_param)
{
  DMA_Channel *tmp = (DMA_Channel *)fn_param;

  if (tmp == NULL)
  {
    dev_err(&ios_device, "error: fn_param==NULL\n");
    return false;
  }

  if(tmp->dev_name != NULL)
  {
#if defined(CONFIG_STM_FDMA)
    /* If a device name has been specified only match that device */
    if (!stm_dma_is_fdma_name(chan, tmp->dev_name))
#else
  /* If FDMA name has been specified, attempt to match channel to it */
  if (dma_np[tmp->pacing] && (dma_np[tmp->pacing] != chan->device->dev->of_node))
#endif
    {
      dev_err(&ios_device, "invalid fdma name %s ('%s')\n", tmp->dev_name, dev_name(chan->device->dev));
      return false;
    }
  }

  if(tmp->pacing != SDTP_UNPACED)
  {
    if(dma_req_lines[tmp->pacing] == -1)
    {
      dev_err(&ios_device, "pacing request (%d) not supported by chip\n", tmp->pacing);
      return false;
    }


#if defined(CONFIG_STM_FDMA)
    tmp->config.type                     = STM_DMA_TYPE_PACED;
#else
    tmp->config.type                     = ST_DMA_TYPE_PACED;
#endif
    tmp->config.dreq_config.direction    = DMA_MEM_TO_DEV;
    tmp->config.dreq_config.initiator    = 0;
    tmp->config.dreq_config.hold_off     = 0;
    tmp->config.dreq_config.data_swap    = 0;
    tmp->config.dreq_config.request_line = dma_req_lines[tmp->pacing];
    tmp->config.dreq_config.increment    = (tmp->flags & SDTF_DST_IS_SINGLE_REGISTER)?0:1;
    tmp->config.dreq_config.buswidth     = DMA_SLAVE_BUSWIDTH_4_BYTES;
    tmp->config.dreq_config.maxburst     = tmp->bytes_per_req / 4;
    tmp->config.dreq_config.direct_conn  = dma_connection[tmp->pacing];
  }
  else
  {
#if defined(CONFIG_STM_FDMA)
    tmp->config.type = STM_DMA_TYPE_FREE_RUNNING;
#else
    tmp->config.type = ST_DMA_TYPE_FREE_RUNNING;
#endif
  }

  /*
   * We have to link the channel private field now to point to the config,
   * because once we return true dma_request_channel() will call dma_chan_get()
   * which in turn will try and allocate the dma resources for the channel.
   *
   * The FDMA device implementation of this looks at chan->private to do the
   * paced channel setup including the xbar setup for the pacing line.
   */
  chan->private = &tmp->config;

  dev_dbg(&ios_device, "Using FDMA '%s' channel %d",
      dev_name(chan->device->dev), chan->chan_id);

  return true;
}


DMA_Channel *vibe_os_get_dma_channel(STM_DMA_TRANSFER_PACING pacing, uint32_t bytes_per_req, STM_DMA_TRANSFER_FLAGS flags)
{
  DMA_Channel *chan = kzalloc(sizeof(DMA_Channel), GFP_KERNEL);
  dma_cap_mask_t mask;


  if(!chan)
    return NULL;

  dma_cap_zero(mask);
  dma_cap_set(DMA_SLAVE, mask);

  /* Request a matching dma channel */
  chan->pacing        = pacing;
  chan->flags         = flags;
  chan->bytes_per_req = bytes_per_req;
  chan->dev_name      = dma_dev_name[pacing];
  chan->channel       = dma_request_channel(mask, fdma_filter_fn, chan);

  if(!chan->channel)
  {
    printk(KERN_WARNING "%s: unable to find usable DMA channel for %s\n",__FUNCTION__, chan->dev_name);
    goto error_exit;
  }

  return chan;

error_exit:
  kfree(chan);
  return NULL;
}


void vibe_os_release_dma_channel(DMA_Channel *chan)
{
  if(!chan)
    return;

  dma_release_channel(chan->channel);
  kfree(chan);
}


void vibe_os_stop_dma_channel(DMA_Channel *chan)
{
  if(!chan)
    return;

  dmaengine_terminate_all(chan->channel);
}


void* vibe_os_create_dma_transfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags)
{
  struct dma_async_tx_descriptor *desc;
  struct dma_slave_config config;
  struct scatterlist *sg;
  unsigned long tx_flags;
  int list_length;
  int i;

  if(!chan)
  {
    printk(KERN_ERR "%s: error: chan==NULL\n", __FUNCTION__);
    return NULL;
  }

  if(!transfer_list)
    return NULL;

  list_length = 0;
  while(transfer_list[list_length].src)
  {
    /*
     * We are only supporting memory to device register transfers so the
     * destination must be the same physical address.
     */
    if((transfer_list[list_length].dst != transfer_list[0].dst) ||
       (transfer_list[list_length].dst_offset != transfer_list[0].dst_offset) )
    {
#ifdef DEBUG
      printk(KERN_WARNING "%s: transfer list entry %d is not consistent with a SG FDMA hardware transfer\n",__FUNCTION__,list_length);
#endif
      return NULL;
    }

    list_length++;
  }

  if(!list_length)
    return NULL;

#ifdef DEBUG
    printk(KERN_WARNING "%s: transfer list has %d entries\n",__FUNCTION__,list_length);
#endif

  config.direction = DMA_MEM_TO_DEV;
  config.dst_addr = transfer_list[0].dst->ulPhysical + transfer_list[0].dst_offset;
  if(pacing != SDTP_UNPACED)
  {
    config.dst_addr_width = chan->config.dreq_config.buswidth;
    config.dst_maxburst = chan->config.dreq_config.maxburst;
  }

  if(dmaengine_slave_config(chan->channel, &config))
  {
    printk(KERN_WARNING "%s: unable to set DMA channel slave config\n",__FUNCTION__);
    return NULL;
  }

  sg = kmalloc(sizeof(struct scatterlist)*list_length, GFP_KERNEL);
  if(!sg)
    return NULL;

  sg_init_table(sg,list_length);

  for(i=0;i<list_length;i++)
  {
    sg_dma_address(&sg[i]) = transfer_list[i].src->ulPhysical + transfer_list[i].src_offset;
    sg_dma_len(&sg[i]) = transfer_list[i].size;
#ifdef DEBUG
    printk(KERN_WARNING "%s: sg[%d] = %d bytes @ 0x%lx\n",__FUNCTION__,i,(int)sg_dma_len(&sg[i]),(unsigned long)sg_dma_address(&sg[i]));
#endif

  }

  tx_flags = DMA_COMPL_SKIP_SRC_UNMAP | DMA_COMPL_SKIP_DEST_UNMAP;

  desc = dmaengine_prep_slave_sg(chan->channel,
                  sg, list_length, DMA_MEM_TO_DEV, tx_flags);

#ifdef DEBUG
  if(!desc)
    printk(KERN_WARNING "%s: failed to prep async slave sg transfer\n",__FUNCTION__);
#endif

  if(desc)
  {
    /*
     * We can only take a callback at the end of the SG transaction, which is
     * OK that is the usage we actually want in practice, rather than actually
     * getting a callback after every element of the transfer list.
     */
    desc->callback = transfer_list[list_length-1].completed_cb;
    desc->callback_param = transfer_list[list_length-1].cb_param;
  }

  kfree(sg);
  return desc;
}


void  vibe_os_delete_dma_transfer(void *handle)
{
  struct dma_async_tx_descriptor *desc = (struct dma_async_tx_descriptor *)handle;

  if(!desc)
    return;

  /*
   * This just sets the ACK flag in the descriptor, it will be recycled the
   * next time a descriptor is created or if the channel is released.
   */
  async_tx_ack(desc);
}


int vibe_os_start_dma_transfer(void *handle)
{
  struct dma_async_tx_descriptor *desc = (struct dma_async_tx_descriptor *)handle;

  if(!desc)
  {
    printk(KERN_ERR "%s: error: desc==NULL\n", __FUNCTION__);
    return -EINVAL;
  }

  return dma_submit_error(dmaengine_submit(desc))?-1:0;
}

#endif /* CONFIG_STM_FDMA || CONFIG_ST_FDMA */


#if !defined(CONFIG_STM_DMA) && !defined(CONFIG_STM_FDMA) && !defined(CONFIG_ST_FDMA)
DMA_Channel *vibe_os_get_dma_channel(STM_DMA_TRANSFER_PACING pacing, uint32_t bytes_per_req, STM_DMA_TRANSFER_FLAGS flags) { return NULL; }
void vibe_os_release_dma_channel(DMA_Channel *chan) {}
void vibe_os_stop_dma_channel(DMA_Channel *chan) {}
void* vibe_os_create_dma_transfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags) { return NULL; }
void  vibe_os_delete_dma_transfer(void *handle) {}
int vibe_os_start_dma_transfer(void *handle) { return -1; }
#endif /* !defined(CONFIG_STM_DMA) && !defined(CONFIG_STM_FDMA) && !defined(CONFIG_ST_FDMA)*/

int vibe_os_grant_access(void *buf, unsigned int size, enum vibe_access_transformer_id tid, enum vibe_access_mode mode)
{
    int r = 0;
#ifdef CONFIG_STM_DISPLAY_SDP
    enum smcs_access_mode smcs_mode = 0;
    enum smcs_transformer_id smcs_tid = SMCS_TID_NB;
    struct smcs_buf sbuf;

    if(!buf)
    {
      TRC(TRC_ID_SDP,"NULL buffer");
      return -EINVAL;
    }

    switch(tid)
    {
    case VIBE_ACCESS_TID_DISPLAY:
        smcs_tid = SMCS_TID_DISPLAY;
        break;
    case VIBE_ACCESS_TID_HDMI_CAPTURE:
        smcs_tid = SMCS_TID_HDMI_CAPTURE;
        break;
    default:
        TRC(TRC_ID_ERROR, "unknown transformer id %d", tid);
        return -EINVAL;
    }

    switch(mode)
    {
    case VIBE_ACCESS_O_READ:
        smcs_mode = SMCS_O_READ;
        break;
    case VIBE_ACCESS_O_WRITE:
        smcs_mode = SMCS_O_WRITE;
        break;
    default:
        TRC(TRC_ID_ERROR, "unknown access mode %d", mode);
        return -EINVAL;
    }

    TRC(TRC_ID_SDP, "grant buffer 0x%p size %d", buf, size);
    sbuf.type = SMCS_MEM_TYPE_PHYSICAL;
    sbuf.data.paddr = (unsigned long)buf;
    r = smcs_grant_access(&sbuf, size, smcs_tid, smcs_mode);
#endif
    return r;
}

int vibe_os_revoke_access(void *buf, unsigned int size, enum vibe_access_transformer_id tid, enum vibe_access_mode mode)
{
    int r = 0;
#ifdef CONFIG_STM_DISPLAY_SDP
    enum smcs_access_mode smcs_mode = 0;
    enum smcs_transformer_id smcs_tid = SMCS_TID_NB;
    struct smcs_buf sbuf;

    if(!buf)
    {
      TRC(TRC_ID_SDP, "NULL buffer");
      return -EINVAL;
    }

    switch(tid)
    {
    case VIBE_ACCESS_TID_DISPLAY:
        smcs_tid = SMCS_TID_DISPLAY;
        break;
    case VIBE_ACCESS_TID_HDMI_CAPTURE:
        smcs_tid = SMCS_TID_HDMI_CAPTURE;
        break;
    default:
        TRC(TRC_ID_ERROR, "unknown transformer id %d", tid);
        return -EINVAL;
    }

    switch(mode)
    {
    case VIBE_ACCESS_O_READ:
        smcs_mode = SMCS_O_READ;
        break;
    case VIBE_ACCESS_O_WRITE:
        smcs_mode = SMCS_O_WRITE;
        break;
    default:
        TRC(TRC_ID_ERROR, "unknown access mode %d", mode);
        return -EINVAL;
    }

    TRC(TRC_ID_SDP, "revoke buffer 0x%p size %d", buf, size);
    sbuf.type = SMCS_MEM_TYPE_PHYSICAL;
    sbuf.data.paddr = (unsigned long)buf;
    r = smcs_revoke_access(&sbuf, size, smcs_tid, smcs_mode);
#endif
    return r;
}

int vibe_os_create_region(void *buf, unsigned int l)
{
  int r = 0;
#ifdef CONFIG_STM_DISPLAY_SDP
  struct smcs_buf sbuf;
  TRC(TRC_ID_SDP, "create region %p size %d", buf, l);
  sbuf.type = SMCS_MEM_TYPE_PHYSICAL;
  sbuf.data.paddr = (unsigned long)buf;
  r = smcs_create_region(&sbuf, l);
#endif
  return r;
}

int vibe_os_destroy_region(void *buf, unsigned int l)
{
  int r = 0;
#ifdef CONFIG_STM_DISPLAY_SDP
  struct smcs_buf sbuf;
  TRC(TRC_ID_SDP, "destroy region %p size %d", buf, l);
  sbuf.type = SMCS_MEM_TYPE_PHYSICAL;
  sbuf.data.paddr = (unsigned long)buf;
  r = smcs_destroy_region(&sbuf, l);
#endif
  return r;
}

/* -------- */
/* | lock | */
/* -------- */

void * vibe_os_create_resource_lock ( void )
{
    RESOURCE_LOCK *pLock = kmalloc(sizeof(RESOURCE_LOCK),GFP_KERNEL);
    spin_lock_init(&(pLock->theLock));
    return pLock;
}


void vibe_os_delete_resource_lock ( void * ulHandle )
{
    kfree(ulHandle);
}


void vibe_os_lock_resource ( void * ulHandle )
{
    RESOURCE_LOCK *pLock = (RESOURCE_LOCK*)ulHandle;
    spin_lock_irqsave(&(pLock->theLock), pLock->lockFlags);
}


void vibe_os_unlock_resource ( void * ulHandle )
{
    RESOURCE_LOCK *pLock = (RESOURCE_LOCK*)ulHandle;
    spin_unlock_irqrestore(&(pLock->theLock),pLock->lockFlags);
}

/* ------------- */
/* | semaphore | */
/* ------------- */

void * vibe_os_create_semaphore ( int initVal )
{
  struct semaphore *pSema = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
  sema_init(pSema,initVal);
  return pSema;
}


void vibe_os_delete_semaphore ( void * ulHandle )
{
  kfree(ulHandle);
}


int vibe_os_down_semaphore ( void * ulHandle )
{
  int res;
  struct semaphore *pSema = ulHandle;

  res = down_interruptible(pSema);
  if(res != 0)
  {
    TRC(TRC_ID_ERROR, "Failed to down sem 0x%p (res = %d)", pSema, res);
  }

  return res;
}


int vibe_os_down_trylock_semaphore ( void * ulHandle )
{
  /* if the semaphore is not available at the time of the call, down_trylock returns immediately with */
  /* a nonzero return value. Else it take the semaphore and return zero value. */
  struct semaphore *pSema = ulHandle;
  return( down_trylock(pSema) );
}


void vibe_os_up_semaphore ( void * ulHandle )
{
  struct semaphore *pSema = ulHandle;
  up(pSema);
}

/* --------- */
/* | mutex | */
/* --------- */

void * vibe_os_create_mutex ( void )
{
  /* Initialize the mutex to unlocked state.                  */
  /* It is not allowed to initialize an already locked mutex. */

  /* The CONFIG_DEBUG_MUTEXES .config option turns on debugging checks that will enforce the */
  /* restrictions and will also do deadlock debugging.                                       */

  struct mutex *pMutex = kmalloc(sizeof(struct mutex),GFP_KERNEL);
  mutex_init(pMutex);
  return pMutex;
}


int vibe_os_is_locked_mutex ( void * ulHandle )
{
  /* Returns 1 if the mutex is locked, 0 if unlocked. */
  struct mutex *pMutex = ulHandle;
  return mutex_is_locked(pMutex);
}


void vibe_os_delete_mutex ( void * ulHandle )
{
  /* Release any resource allocated by mutex_init() and free allocated memory. */
  /* Must not be called if mutex is still locked.                              */
  struct mutex *pMutex = ulHandle;
  mutex_destroy(pMutex);

  kfree(pMutex);
}


int vibe_os_lock_mutex ( void * ulHandle )
{
  /* int mutex_lock_interruptible(struct mutex *) lock the mutex like void mutex_lock(struct mutex *), */
  /* and return 0 if the mutex has been acquired or sleep until the mutex becomes available.           */
  /* If a signal arrives while waiting for the lock then this function returns -EINTR.                 */

  /* The mutex must later on be released by the same task that acquired it.                           */
  /* Recursive locking is not allowed.                                                                */
  /* The task may not exit without first unlocking the mutex.                                         */
  /* Also, kernel memory where the mutex resides mutex must not be freed with the mutex still locked. */
  struct mutex *pMutex = ulHandle;
  int res;

  res = mutex_lock_interruptible(pMutex);
  if(res != 0)
  {
    TRC(TRC_ID_ERROR, "Failed to lock mutex 0x%p (res = %d)", pMutex, res);
  }

  return res;
}


int vibe_os_trylock_mutex ( void * ulHandle )
{
  /* Try to acquire the mutex atomically. Returns 1 if the mutex has been acquired successfully, */
  /* and 0 on contention. */

  /* This function must not be used in interrupt context.          */
  /* The mutex must be released by the same task that acquired it. */
  struct mutex *pMutex = ulHandle;
  return( mutex_trylock(pMutex) );
}


void vibe_os_unlock_mutex ( void * ulHandle )
{
  /* Unlock a mutex that has been locked by this task previously. */
  /* This function must not be used in interrupt context.         */
  /* Unlocking of a not locked mutex is not allowed.              */
  struct mutex *pMutex = ulHandle;
  mutex_unlock(pMutex);
}

/* ---------  */

#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
static void firmware_free_data (struct kref *kref)
{
  STMFirmware_private * const fw = container_of(kref, STMFirmware_private, kref);

  printk(KERN_INFO "%s: No more firmwares users: free cache data\n",fw->name);
  /* free STM firmware data */
  kfree(fw->firmware.pData);
}

static void firmware_release (struct device * device)
{
  /* free firmware cache */
  struct firmware_data * const fwd = (struct firmware_data *)dev_get_drvdata(device);
  struct list_head *pos, *n;

  if(!fwd)
    return;

  if(!device_trylock(device))
    return;

  list_for_each_safe (pos, n, &fwd->fw_cache)
  {
    STMFirmware_private * const fw = list_entry (pos,
                                                 STMFirmware_private,
                                                 fw_list);

    /* Release all fw users and free all fw cache data
     * Be carefull this will need to set the initial number
     * of kref to 2 (requester and us) to make it valid at this time
     */
    if(WARN_ON(kref_put(&fw->kref, firmware_free_data) != 1))
      firmware_free_data(&fw->kref);

    dev_info(device, "%s: Free fw cache and container\n", fw->name);
    /* no users left */
    list_del_init(&fw->fw_list);

    kfree(fw);
  }

  /* Release device drvdata */
  list_del_init(&fwd->fw_cache);
  dev_set_drvdata(device, NULL);

  device_unlock(device);
}

/*
 * requst_firmware_nowait() callback function
 *
 * This function is called by the kernel when a firmware is made available,
 * or if it times out waiting for the firmware.
 */
static void firmware_load_data(const struct firmware *fw, void *context)
{
  STMFirmware_private *fwp = context;
  struct device *dev = &fwp->pdev->dev;

  dev_info(dev, "loading firmware data.\n");

  fwp->firmware.pData = 0;

  if (!fw)
  {
    dev_err(dev, "firmware not found\n");
    goto out;
  }

  /* get some memory for STM firmware data */
  fwp->firmware.pData = kzalloc (fw->size, GFP_KERNEL);
  if (!fwp->firmware.pData)
  {
    dev_err(dev, "could not allocate memory for firmware data\n");
    goto release_out;
  }

  /* copy data to requestor */
  memcpy((void *)fwp->firmware.pData, fw->data, fw->size);
  fwp->firmware.ulDataSize = fw->size;

  dev_info(dev, "firmware data is loaded (size=%d).\n", fw->size);

release_out:
  /* release the linux firmware */
  release_firmware(fw);
out:
  return;
}

static int CacheGetFirmware (const char                 * const name,
                             const STMFirmware_private **st_firmware_priv)
{
  struct list_head     *pos;
  struct firmware_data * const fwd = (struct firmware_data *)dev_get_drvdata(&ios_device);
  int                   error = -ENOENT;
#if defined(CONFIG_ARCH_STI)
  struct device_node *fw_node=NULL;
  const char *fw_name=NULL;
#endif

  if (!st_firmware_priv)
    return -EINVAL;
  if (*st_firmware_priv != NULL)
    return -EINVAL;

  device_lock(&ios_device);

  /* first check if we have it cached already */
  list_for_each (pos, &fwd->fw_cache)
  {
    STMFirmware_private * const fw = list_entry (pos,
                                                 STMFirmware_private,
                                                 fw_list);
    if (!strcmp (fw->name, name))
    {
      pr_info("%s: successfully loaded from cache\n", fw->name);
      /* yes - so use it */
      kref_get (&fw->kref);

      *st_firmware_priv = fw;
      error = 0;
      goto out;
    }
  }

  /* firmware not found in cache - so load it. Do so only if it is save
   * to potentially sleep - request_firmware() is very likely to sleep!
   * We could kzalloc(GFP_ATOMIC) and use request_firmware_nowait() to
   * solve this problem, though.
   */

  might_sleep ();
  if (in_atomic ()
      || irqs_disabled ())
  {
    /* not save to sleep */
    printk ("%s: firmware '%s' not in cache and refusing to load\n",
            __FUNCTION__, name);
  }
  else
  {
    /* save to sleep - continue loading */
    STMFirmware_private *fw;

    /* get some memory */
    fw = kzalloc (sizeof (*fw), GFP_KERNEL);
    if (!fw)
    {
      error = -ENOMEM;
      goto out;
    }

    fw->pdev = platform_device_register_data(&ios_device, name, -1, NULL, 0);
    if (IS_ERR(fw->pdev))
    {
      kfree(fw);
      pr_err("Failed to register firmware platform device.\n");
      error = -EINVAL;
      goto out;
    }

    /* request linux firmware */
    dev_info(&fw->pdev->dev, "requesting frm: '%s', %p/%p\n", name, &ios_device, &fw->firmware);
#if !defined(CONFIG_ARCH_STI)
    error = request_firmware (&fw->lnx_firmware, name, &fw->pdev->dev);
#else
    fw_node = of_get_child_by_name(ios_device.of_node, "firmwares");
    if(IS_ERR_OR_NULL(fw_node))
    {
      dev_err(&fw->pdev->dev, "firmwares DT settings are missing\n");
      error = -ENOENT;
    }
    else
    {
      error = of_property_read_string(fw_node, name, &fw_name);
      if (likely (!error))
      {
        dev_info(&fw->pdev->dev, "using %s for %s firmware\n", fw_name, name);
        error = request_firmware (&fw->lnx_firmware, fw_name, &fw->pdev->dev);
      }
      of_node_put(fw_node);
    }
#endif
    if (likely (error))
    {
      platform_device_unregister(fw->pdev);
      kfree (fw);
      goto out;
    }

    firmware_load_data(fw->lnx_firmware, fw);
    if(!fw->firmware.pData)
    {
      error = -ENOENT;
      platform_device_unregister(fw->pdev);
      kfree (fw);
      goto out;
    }

    dev_info(&fw->pdev->dev, "frm '%s' successfully loaded\n", name);

    /* have the firmware */
    *st_firmware_priv = fw;

    /* remember fw data */
    strncpy (fw->name, name, FIRMWARE_NAME_MAX);
    fw->name[FIRMWARE_NAME_MAX] = '\0';

    /* Initialize the object users */
    kref_init (&fw->kref);
    /* Register ourself to be able to unload data at the end */
    kref_get (&fw->kref);

    /* add to cache */
    list_add_tail (&fw->fw_list, &fwd->fw_cache);

    platform_device_unregister(fw->pdev);
  }

out:
  if (unlikely (error))
    printk ("loading frm: '%s' failed: %d\n", name, error);

  device_unlock(&ios_device);
  return error;
}
#endif

int vibe_os_request_firmware (const STMFirmware **firmware_p,
                            const char         * const name)
{
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
  int                        error;
  const STMFirmware_private *st_firmware_priv = NULL;

  if(DISABLE_FIRMWARE_LOADING)
  {
    return -ENOENT;
  }

  if (unlikely (!firmware_p
                || *firmware_p
                || !name
                || !*name))
  {
    printk(KERN_ERR "vibe_os_request_firmware: error in parameters\n");
    return -EINVAL;
  }

  if (unlikely (!device_is_registered(&ios_device)))
    return -ENODEV;

  error = CacheGetFirmware (name, &st_firmware_priv);
  if (likely (!error))
    *firmware_p = &st_firmware_priv->firmware;

  return error;
#else
# warning No firmware loading support!
  printk (KERN_NOTICE "%s: Your kernel was compiled without firmware loading "
          "support!\n", __FUNCTION__);
  return -ENOENT;
#endif
}

int vibe_os_get_master_output_id(int output_id)
{
  struct device_node *pipe_node=NULL;
  int id = -1, master_id = -1;
  const char *s;

  if (output_id < 0)
    return -1;

  while ((pipe_node = of_get_next_child(ios_device.of_node, pipe_node))) {
    s = NULL;
    of_property_read_string(pipe_node, "display-pipe", &s);
    if(s)
    {
      of_property_read_u32(pipe_node, "dvo-output-id", &id);
      if ((id != -1) && (id == output_id)) {
          of_property_read_u32(pipe_node, "main-output-id", &master_id);
          of_node_put(pipe_node);
          break;
      }
      of_property_read_u32(pipe_node, "hdmi-output-id", &id);
      if ((id != -1) && (id == output_id)) {
          of_property_read_u32(pipe_node, "main-output-id", &master_id);
          of_node_put(pipe_node);
          break;
      }
    }
    of_node_put(pipe_node);
  }

  return master_id;
}
EXPORT_SYMBOL(vibe_os_get_master_output_id);


void vibe_os_release_firmware (const STMFirmware *fw_pub)
{
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
  struct list_head *pos, *n;
  struct firmware_data * const fwd = (struct firmware_data *)dev_get_drvdata(&ios_device);

  STMFirmware_private * const fw = container_of (fw_pub,
                                                 STMFirmware_private,
                                                 firmware);

  if (fw_pub == NULL)
  {
    printk(KERN_ERR "vibe_os_release_firmware: error: fw_pub==NULL\n");
    return;
  }

  /* first check if we have it cached already */
  list_for_each_safe (pos, n, &fwd->fw_cache)
  {
    STMFirmware_private * const fw_loc = list_entry (pos,
                                                 STMFirmware_private,
                                                 fw_list);
    if (!strcmp (fw->name, fw_loc->name))
    {
      /* yes - free it */
      kref_put(&fw_loc->kref, firmware_free_data);
      pr_info("%s: successfully removed from cache list\n", fw->name);
    }
  }
#endif
}

int
__attribute__ ((format (printf, 3, 4)))
vibe_os_snprintf(char * buf, uint32_t size, const char * fmt, ...)

{
  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,size,fmt,args);
  va_end(args);
  return i;
}

uint32_t vibe_os_call_bios (long call, unsigned int n_args, ...)
{
#ifdef HAVE_BIOS
  va_list va_args;
  uint32_t retval;
  int32_t args[n_args];

  va_start (va_args, n_args);
  for (retval = 0; retval < n_args; ++retval)
    args[retval] = va_arg (va_args, int32_t);
  va_end (va_args);

  switch (n_args)
    {
    case 0:
      retval = INLINE_BIOS (call, 0);
      break;
    case 1:
      retval = INLINE_BIOS (call, 1, args[0]);
      break;
    case 2:
      retval = INLINE_BIOS (call, 2, args[0], args[1]);
      break;
    case 3:
      retval = INLINE_BIOS (call, 3, args[0], args[1], args[2]);
      break;
    case 4:
      retval = INLINE_BIOS (call, 4, args[0], args[1], args[2], args[3]);
      break;
    case 5:
      retval = INLINE_BIOS (call, 5, args[0], args[1], args[2], args[3], args[4]);
      break;
    case 6:
      retval = INLINE_BIOS (call, 6, args[0], args[1], args[2], args[3], args[4], args[5]);
      break;

    default:
      retval = 0 /* BIOS_FAIL */;
      break;
    }

  return retval;
#else /* HAVE_BIOS */
  return 0; /* BIOS_FAIL */
#endif /* HAVE_BIOS */
}

/* Function reducing the numerator and denominator (without lost) */
void vibe_os_rational_reduction(uint64_t *pNum, uint64_t *pDen)
{
  if(*pNum == *pDen)
  {
    *pNum = 1;
    *pDen = 1;
  }
  else
  {
      // Reduction by multiple of 2
      while( (*pNum >= 2)              &&
             (*pDen >= 2)              &&
             ( (*pNum & 0x01ULL) == 0) &&
             ( (*pDen & 0x01ULL) == 0) )
      {
        *pNum = *pNum >> 1;
        *pDen = *pDen >> 1;
      }
  }
}

uint64_t vibe_os_div64(uint64_t num, uint64_t den)
{
  if (unlikely(den & 0xffffffff00000000ULL))
  {
    // Perform a loss less reduction of this rational number
    vibe_os_rational_reduction(&num, &den);

    // Check if the denominator is still too big
    if(den & 0xffffffff00000000ULL)
    {
      TRC( TRC_ID_MAIN_INFO, "64-bit/64-bit division will lose precision due to 64-bits divisor (0x%llx / 0x%llx)", num, den);
    }
  }

  return div64_u64(num, den);
}

/******************************************************************************
 *  Work queue implementation
 */

struct VIBE_OS_Worker_s
{
    struct work_struct    work;
    VIBE_OS_WorkerFct_t   function;
    void                 *data;
};

VIBE_OS_WorkQueue_t vibe_os_create_work_queue(const char *name)
{
    struct workqueue_struct *workqueue;

    // Create a single thread and its queue of work/message
    workqueue = create_singlethread_workqueue(name);

    return(workqueue);
}

void vibe_os_destroy_work_queue(VIBE_OS_WorkQueue_t workqueue)
{

    flush_workqueue((struct workqueue_struct *)workqueue);

    destroy_workqueue((struct workqueue_struct *)workqueue);
}

void common_work_function(struct work_struct *my_work)
{
    struct VIBE_OS_Worker_s *my_worker = container_of(my_work, struct VIBE_OS_Worker_s, work);

    (*(my_worker->function))(&(my_worker->data));
}

void* vibe_os_allocate_workerdata(VIBE_OS_WorkerFct_t function, int data_size)
{
    struct VIBE_OS_Worker_s *worker;

    // allocate worker + data size
    worker = (struct VIBE_OS_Worker_s *) kmalloc(sizeof(struct VIBE_OS_Worker_s) - sizeof(void *) + data_size, GFP_KERNEL);
    if(worker == 0)
    {
        return 0;
    }

    // INIT_WORK calls PREPARE_WORK
    INIT_WORK(&(worker->work), common_work_function);

    worker->function = function;

    return ((void *)(((char *)worker) + offsetof(struct VIBE_OS_Worker_s, data)));
}

void vibe_os_delete_workerdata(void* data)
{
    struct VIBE_OS_Worker_s *my_worker = (struct VIBE_OS_Worker_s *) (((char *)data) - offsetof(struct VIBE_OS_Worker_s, data));

    kfree(my_worker);
}

bool vibe_os_queue_workerdata(VIBE_OS_WorkQueue_t workqueue, void* data)
{
    int result;

    struct VIBE_OS_Worker_s *my_worker = (struct VIBE_OS_Worker_s *) (((char *)data) - offsetof(struct VIBE_OS_Worker_s, data));

    result = queue_work((struct workqueue_struct *)workqueue, &(my_worker->work));
    if(!result)
    {
        // 0 means work already on a queueu, non-zero otherwise
        return false;
    }

    return true;
}

/******************************************************************************
 *  Message thread implementation
 */

struct VIBE_OS_MessageThread_s
{
    struct task_struct *thread;             /* message thread structure */

    wait_queue_head_t   wait_queue;         /* wait queue for blocking message thread: waiting new message arrival */
    bool                wait_queue_cond;    /* wait queue condition (set at true before waking up message thread)  */

    struct list_head    msg_list;           /* message list header                                                 */
    spinlock_t          msg_lock;           /* lock for inserting/removing message to/from message list header     */
};

struct VIBE_OS_Message_s
{
    struct list_head            list;       /* for linking message together                            */
    VIBE_OS_MessageCallback_t   callback;   /* message callback                                        */
    void                       *data;       /* message data to be provided to the message callback     */
};

int common_msg_thread_function(void *data)
{
    struct VIBE_OS_MessageThread_s *thread_ctx = (struct VIBE_OS_MessageThread_s *)data;
    struct VIBE_OS_Message_s       *msg_ctx;
    unsigned long                   flags;

    while(1)
    {
        /* wait message(s) to be posted to message thread */
        if(wait_event_interruptible(thread_ctx->wait_queue,
                                    (kthread_should_stop() ||
                                    thread_ctx->wait_queue_cond )
                                   ) !=0)
        {
            printk(KERN_INFO "%s: %d\n",__FUNCTION__, __LINE__);
            return -ERESTARTSYS;
        }

        /* reset wait queue condition */
        thread_ctx->wait_queue_cond = false;

        /* check if message thread should be stopped */
        if(kthread_should_stop())
        {
            return 0;
        }

        /* treat all pending message until queue is empty */
        while(1)
        {
            spin_lock_irqsave(&(thread_ctx->msg_lock), flags);
            if(!list_empty(&(thread_ctx->msg_list)))
            {
                /* extract first message pushed in the queue */
                msg_ctx = list_first_entry(&(thread_ctx->msg_list), struct VIBE_OS_Message_s, list);
                list_del_init(&(msg_ctx->list));
                spin_unlock_irqrestore(&(thread_ctx->msg_lock), flags);

                /* call message callback with its data parameter */
                (*(msg_ctx->callback))(&(msg_ctx->data));
            }
            else
            {
                /* no more message in the queue */
                spin_unlock_irqrestore(&(thread_ctx->msg_lock), flags);
                break;
            }
        }
    }

    return 0;
}

VIBE_OS_MessageThread_t vibe_os_create_message_thread(const char *name, const int *thread_settings)
{
    struct VIBE_OS_MessageThread_s *thread_ctx;
    int                             thread_sched_policy;
    struct sched_param              thread_sched_param;

    /* check input parameters */
    if((name==NULL) || (thread_settings==NULL))
    {
        TRC(TRC_ID_ERROR, "FAILED, invalid input parameters!");
        goto exit_alloc_fail;
    }

    /* allocate message thread */
    thread_ctx = (struct VIBE_OS_MessageThread_s *) kmalloc(sizeof(struct VIBE_OS_MessageThread_s), GFP_KERNEL);
    if(thread_ctx == 0)
    {
        printk(KERN_ERR "%s: Failed to allocate message thread!\n",__FUNCTION__);
        goto exit_alloc_fail;
    }

    /* initialize wait queue */
    init_waitqueue_head(&(thread_ctx->wait_queue));
    thread_ctx->wait_queue_cond = false;

    /* initialize message list header */
    INIT_LIST_HEAD(&(thread_ctx->msg_list));

    /* initialize message lock */
    spin_lock_init(&(thread_ctx->msg_lock));

    /* create message thread without waking up */
    thread_ctx->thread = kthread_create(common_msg_thread_function, thread_ctx, name);
    if(IS_ERR(thread_ctx->thread))
    {
        printk(KERN_ERR "%s: Failed to create message thread!\n",__FUNCTION__);
        goto exit_create_thread_fail;
    }

    /* Set thread scheduling settings */
    thread_sched_policy               = thread_settings[0];
    thread_sched_param.sched_priority = thread_settings[1];
    if( sched_setscheduler(thread_ctx->thread, thread_sched_policy, &thread_sched_param) )
    {
        TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
            name, thread_sched_policy, thread_sched_param.sched_priority);
    }

    /* wake up message thread */
    wake_up_process(thread_ctx->thread);

    return((VIBE_OS_MessageThread_t)thread_ctx);

exit_create_thread_fail:
    kfree(thread_ctx);

exit_alloc_fail:
    return 0;
}

void vibe_os_destroy_message_thread(VIBE_OS_MessageThread_t message_thread)
{
    unsigned long flags;
    struct VIBE_OS_MessageThread_s *thread_ctx = (struct VIBE_OS_MessageThread_s *)message_thread;

    /* check message thread validity */
    if(!thread_ctx || !thread_ctx->thread)
    {
        printk(KERN_ERR "%s: Failed, invalid message thread\n",__FUNCTION__);
        return;
    }

    /* stop and wait message thread ending */
    if(kthread_stop(thread_ctx->thread))
    {
        printk(KERN_ERR "%s: Failed to stop message thread\n",__FUNCTION__);
        return;
    }

    /* check message queue is empty */
    spin_lock_irqsave(&(thread_ctx->msg_lock), flags);
    if(!list_empty(&(thread_ctx->msg_list)))
    {
        spin_unlock_irqrestore(&(thread_ctx->msg_lock), flags);
        printk(KERN_ERR "%s: Failed message list is not empty\n",__FUNCTION__);
        return;
    }
    spin_unlock_irqrestore(&(thread_ctx->msg_lock), flags);

    /* delete message thread */
    thread_ctx->thread = 0;
    kfree(thread_ctx);
}

void* vibe_os_allocate_message(VIBE_OS_MessageCallback_t callback, int data_size)
{
    struct VIBE_OS_Message_s *msg_ctx;

    /* check message validity */
    if(!callback)
    {
        printk(KERN_ERR "%s: Failed, invalid callback for this message\n",__FUNCTION__);
        return 0;
    }

    /* allocate message + data size */
    msg_ctx = (struct VIBE_OS_Message_s *) kmalloc(sizeof(struct VIBE_OS_Message_s) - sizeof(void *) + data_size, GFP_KERNEL);
    if(msg_ctx == 0)
    {
        printk(KERN_ERR "%s: Failed, cannot allocate this message\n",__FUNCTION__);
        return 0;
    }

    /* fill message with callback function to call */
    msg_ctx->callback = callback;

    /* initialize linked list */
    INIT_LIST_HEAD(&(msg_ctx->list));

    /* let user filling the data */
    return ((void *)(((char*)msg_ctx) + offsetof(struct VIBE_OS_Message_s, data)));
}

void vibe_os_delete_message(void* data)
{
    struct VIBE_OS_Message_s *msg_ctx = (struct VIBE_OS_Message_s *) (((char *)data) - offsetof(struct VIBE_OS_Message_s, data));

    /* check message validity */
    if(!msg_ctx->callback)
    {
        printk(KERN_ERR "%s: Failed, message already deleted\n",__FUNCTION__);
        return;
    }

    /* delete the message */
    msg_ctx->callback = 0;
    kfree(msg_ctx);
}

bool vibe_os_queue_message(VIBE_OS_MessageThread_t message_thread, void* data)
{
    unsigned long flags;

    struct VIBE_OS_MessageThread_s *thread_ctx = (struct VIBE_OS_MessageThread_s *)message_thread;
    struct VIBE_OS_Message_s        *msg_ctx   = (struct VIBE_OS_Message_s *) (((char *)data) - offsetof(struct VIBE_OS_Message_s, data));

    /* Put message in the queue */
    spin_lock_irqsave(&(thread_ctx->msg_lock), flags);
    list_add_tail(&(msg_ctx->list), &(thread_ctx->msg_list) );
    spin_unlock_irqrestore(&(thread_ctx->msg_lock), flags);

    /* Wakeup message thread */
    thread_ctx->wait_queue_cond = true;
    wake_up_interruptible(&(thread_ctx->wait_queue));

    return true;
}

/******************************************************************************
 *  Wait queue implementation
 */

struct VIBE_OS_WaitQueue_s
{
  wait_queue_head_t  wait_queue;
};


int vibe_os_allocate_queue_event(VIBE_OS_WaitQueue_t *queue)
{
  // allocate wait queue
  *queue = (VIBE_OS_WaitQueue_t) kmalloc(sizeof(struct VIBE_OS_WaitQueue_s), GFP_KERNEL);
  if(*queue == NULL)
  {
    return -ENOMEM;
  }

  init_waitqueue_head(&((*queue)->wait_queue));

  return 0;
}

void vibe_os_release_queue_event(VIBE_OS_WaitQueue_t queue)
{
  kfree(queue);
  queue = 0;
}

int vibe_os_wait_queue_event(VIBE_OS_WaitQueue_t queue,
                        volatile uint32_t* var, uint32_t value,
                        int cond, unsigned int timeout_ms)
{
  if(!var)
  {
    /* Unconditional sleep */
    interruptible_sleep_on(&queue->wait_queue);
    if(signal_pending(current))
    {
      return -ERESTARTSYS;
    }
  }
  else
  {
    /* Two conditions, wait either until *var == value or *var != value */
    if(timeout_ms == STMIOS_WAIT_TIME_FOREVER)
    {
      return wait_event_interruptible(queue->wait_queue,(cond?(*var == value):(*var != value)));
    }
    else
    {
      /* convert timeout from millisconds to jiffies value */
      uint32_t time_jiffies = (HZ * timeout_ms)/1000;
      return wait_event_interruptible_timeout(queue->wait_queue,(cond?(*var == value):(*var != value)), time_jiffies);
    }
  }
  return 0;
}

void vibe_os_wake_up_queue_event(VIBE_OS_WaitQueue_t queue)
{
  wake_up_interruptible(&(queue->wait_queue));
}

void * vibe_os_get_wait_queue_data(VIBE_OS_WaitQueue_t queue)
{
  void *data = &(queue->wait_queue);
  return data;
}

/******************************************************************************
 *  Enable/Disable interrupt implementation
 */
void vibe_os_enable_irq(unsigned int irq)
{
    enable_irq(irq);
}

void vibe_os_disable_irq_nosync(unsigned int irq)
{
    disable_irq_nosync(irq);
}

void vibe_os_disable_irq(unsigned int irq)
{
    disable_irq(irq);
}

int vibe_os_in_interrupt(void)
{
    // Returns nonzero if in interrupt context and zero if in process context
    return( in_interrupt() );
}

/******************************************************************************
 *  Gpio access implementation
 */
int32_t vibe_os_gpio_get_value(uint32_t gpio)
{
    return gpio_get_value(gpio);
}

/******************************************************************************
 *  clock LLA implementation
 */
int vibe_os_clk_enable(struct vibe_clk *clock)
{
  struct clk *clk;

  if(clock->enabled)
    return -EALREADY;

  clk = devm_clk_get(&ios_device, clock->name);
  if(IS_ERR_OR_NULL(clk))
  {
    dev_err (&ios_device, "%s: Failed, unable to get clock %s\n",__func__,clock->name);
    return PTR_ERR(clk);
  }

  if(clk_prepare_enable(clk))
  {
    devm_clk_put(&ios_device, clk);
    dev_err (&ios_device, "%s: Failed, unable to enable clock %s\n",__func__,clock->name);
    return -EBUSY;
  }

  clock->clk = clk;
  clock->enabled = true;

  return 0;
}

int vibe_os_clk_disable(struct vibe_clk *clock)
{
  struct clk *clk = (struct clk *) clock->clk;

  if(!clock->enabled)
    return -EALREADY;

  clk_disable_unprepare(clk);
  devm_clk_put(&ios_device, clk);

  clock->clk = 0;
  clock->enabled = false;

  return 0;
}

int vibe_os_clk_resume(struct vibe_clk *clock)
{
  /* Nothing to do if already disabled */
  if(!clock->enabled)
    return 0;

  if(IS_ERR_OR_NULL(clock->clk))
  {
    dev_err (&ios_device, "%s: Failed, invalid clock %s\n",__func__,clock->name);
    return PTR_ERR(clock->clk);
  }

  dev_dbg (&ios_device, "%s: Enabling clock %s\n", __func__, clock->name);
  if(clk_prepare_enable(clock->clk))
  {
    dev_err (&ios_device, "%s: Failed, unable to enable clock %s\n",__func__, clock->name);
    return -EBUSY;
  }

  /*
   * Typically the parent should be preserved while suspending the device
   * But we're still doing this until we got confirmation from kernel guys
   */
  if(clock->parent && !IS_ERR_OR_NULL(clock->parent->clk))
  {
    dev_dbg (&ios_device, "%s: Setting parent %s for clock %s\n", __func__, clock->parent->name, clock->name);
    if(clk_set_parent(clock->clk, clock->parent->clk))
    {
      dev_err (&ios_device, "%s: Failed, unable to set parent for clock %s\n",__func__, clock->name);
      return -EINVAL;
    }
  }

  /*
   * Typically the rate should be preserved while suspending the device
   * But we're still doing this until we got confirmation from kernel guys
   */
  if(clock->rate)
  {
    dev_dbg (&ios_device, "%s: Setting rate %d for clock %s\n", __func__, clock->rate, clock->name);
    if(clk_set_rate(clock->clk, clock->rate))
    {
      dev_err (&ios_device, "%s: Failed, unable to set rate for clock %s\n",__func__, clock->name);
      return -EINVAL;
    }
  }

  return 0;
}

int vibe_os_clk_suspend(struct vibe_clk *clock)
{
  /* Nothing to do if already disabled */
  if(!clock->enabled)
    return 0;

  dev_dbg (&ios_device, "%s: suspending clock %s\n", __func__, clock->name);
  clk_disable_unprepare(clock->clk);

  return 0;
}

int vibe_os_clk_set_parent(struct vibe_clk *clock, struct vibe_clk *parent)
{
  struct clk *clk = (struct clk *) clock->clk;
  struct clk *pclk = (struct clk *) parent->clk;

  if(clk_set_parent(clk, pclk))
  {
    dev_err (&ios_device, "%s: Failed, unable to set parent for clock %s\n",__func__, clock->name);
    return -EINVAL;
  }

  clock->parent = parent;

  return 0;
}

struct vibe_clk *vibe_os_clk_get_parent(struct vibe_clk *clock)
{
  return clock->parent;
}

int vibe_os_clk_set_rate(struct vibe_clk *clock, unsigned long rate)
{
  int error = 0;
  struct clk *clk = (struct clk *) clock->clk;

  clock->rate = 0;

  if(clk)
  {
    if(clk_set_rate(clk, rate))
    {
      dev_err (&ios_device, "%s: Failed, unable to set rate for clock %s\n",__func__, clock->name);
      error = -EINVAL;
    }
  }
  else
  {
    clk = devm_clk_get(&ios_device, clock->name);
    if(IS_ERR_OR_NULL(clk))
      return PTR_ERR(clk);

    if(clk_set_rate(clk, rate))
    {
      dev_err (&ios_device, "%s: Failed, unable to set rate for clock %s\n",__func__, clock->name);
      error = -EINVAL;
    }
    devm_clk_put(&ios_device, clk);
  }

  if(!error)
    clock->rate = rate;

  return error;
}

unsigned long vibe_os_clk_get_rate(struct vibe_clk *clock)
{
  unsigned long  rate = 0;
  struct clk *clk = (struct clk *) clock->clk;

  if(clk)
    return clk_get_rate(clk);

  clk = devm_clk_get(&ios_device, clock->name);

  if(IS_ERR_OR_NULL(clk))
    return PTR_ERR(clk);

#if defined(CONFIG_ARCH_STI)
  if (in_interrupt())
    rate = ((clk->new_rate > 0)&&(clk->new_rate != clk->rate))?clk->new_rate:clk->rate;
  else
    rate = clk_get_rate(clk);
#else
  rate = clk_get_rate(clk);
#endif
  devm_clk_put(&ios_device, clk);

  return rate;
}

/******************************************************************************
*  Chip version
 */

bool vibe_os_get_chip_version(uint32_t *major, uint32_t *minor)
{
    uint32_t maj, min;

    /* Check input parameters valid */
    if((major==NULL) || (minor==NULL))
    {
        return false;
    }

#if defined(CONFIG_ARCH_STI)
    maj = 1;
    min = 0;
#else
    /* Get major version of the chip*/
    maj = stm_soc_version_major() + 1;

    /* stm_soc_version_minor is currently not working (See Bug 35189).
       For the moment, we implement it by reading a dedicated register but only for STiH416.
     */
    if(stm_soc_is_stih416())
    {
        if(maj < 2)
        {
            #define STiH416_MPE_CUT_MIN_REGISTER_BASE   0xFD6D509C
            #define STiH416_MPE_CUT_MIN_ADDR_SIZE       0x00000004
            uint32_t *mpe_cut_min_reg;

            mpe_cut_min_reg = (uint32_t*)ioremap(STiH416_MPE_CUT_MIN_REGISTER_BASE, STiH416_MPE_CUT_MIN_ADDR_SIZE );
            if(mpe_cut_min_reg==0)
            {
                return false;
            }

            min = readl(mpe_cut_min_reg) & 0xF;

            iounmap(mpe_cut_min_reg);
        }
        else
        {
            maj = 2;
            min = 0;
        }
    }
    else
    {
        /* Get minor version of the chip, currently always returns -1 */
        min = (uint32_t)stm_soc_version_minor();
    }
#endif
    *major = maj;
    *minor = min;
    return true;
}

/******************************************************************************
 *  Code for asynchronous Prints
 */

#ifdef DEBUG

static bool start_async_print_thread(void)
{
  VIBE_OS_thread_settings thread_settings;

  /* Create a thread and a wait_queue for asynchronous prints */
  vibe_os_allocate_queue_event(&async_wait_queue);
  if(!async_wait_queue)
  {
    printk(KERN_ERR "%s: Failed to create async_wait_queue!\n",__FUNCTION__);
  }

  /* This thread will be waken up when there is something new to print asynchronously */
  thread_settings.policy   = thread_vib_asyncprint[0];
  thread_settings.priority = thread_vib_asyncprint[1];
  if(!vibe_os_create_thread((VIBE_OS_ThreadFct_t) async_print_thread,
                                   NULL,
                                   "VIB-AsyncPrint",
                                   &thread_settings,
                                   &async_print_thread_desc))
  {
    dev_err (&ios_device, "Failed to create VIB-AsyncPrint thread");
    return false;
  }

  vibe_os_wake_up_thread(async_print_thread_desc);

  /* Reset the variables used for asynchronous prints */
  async_print_trace_lost  = false;
  async_print_read_index  = 0;
  async_print_write_index = 0;

  return true;
}

static bool stop_async_print_thread(void)
{
  /* Release the thread used for asynchronous prints */
  if (async_print_thread_desc)
  {
    vibe_os_stop_thread(async_print_thread_desc);
    async_print_thread_desc = 0;
  }

  return true;
}

static bool async_buffer_write_data(const char *fmt, va_list args)
{
  async_print_slot_t  *pSlot;
  uint32_t             next_index;
  char                 text_to_print[ASYNC_PRINT_MAX_STRING_LENGTH];

  pSlot = &(async_slots[async_print_write_index]);

  // Use snprintf and vsnprintf to truncate the trace to the max size that can fit in the slot

  // Retrieve the text to print and expand the arguments
  vsnprintf (text_to_print, sizeof(text_to_print), fmt, args);

  // Amend a prefix showing that it is an asynchronous print
  snprintf(pSlot->text, sizeof(pSlot->text), "ASYNC : %s", text_to_print);

  // The string may have been truncated. Ensure that it ends with \n\0
  pSlot->text[ASYNC_PRINT_MAX_STRING_LENGTH-2] = '\n';
  pSlot->text[ASYNC_PRINT_MAX_STRING_LENGTH-1] = '\0';

  // We're now going to increment the write index but we should first check if the buffer is not full
  next_index = (async_print_write_index + 1) % ASYNC_PRINT_SLOTS_NBR;

  if (next_index == async_print_read_index)
  {
    /* The Async buffer is full, the write_index is not incremented so the next trace will overwrite the current one */
    async_print_trace_lost = true;
  }
  else
  {
    async_print_write_index = next_index;
  }

  // Inform the thread that a new trace is available
  vibe_os_wake_up_queue_event(async_wait_queue);

  return true;
}

static bool async_read_slot(char *pText)
{
  if (async_print_write_index == async_print_read_index)
  {
    // The buffer is empty
    return false;
  }
  else
  {
    memcpy(pText, async_slots[async_print_read_index].text, ASYNC_PRINT_MAX_STRING_LENGTH);

    // Increment the read index
    async_print_read_index = (async_print_read_index + 1) % ASYNC_PRINT_SLOTS_NBR;

    return true;
  }
}

static void async_print_thread(void *data)
{
  char    text[ASYNC_PRINT_MAX_STRING_LENGTH];

  while (!vibe_os_thread_should_stop())
  {
    // Wait for something new to print
    vibe_os_wait_queue_event(async_wait_queue, 0, 0, 0, 0);

    while (async_read_slot(text))
    {
      printk(text);
    }

    if(async_print_trace_lost)
    {
      printk("=== WARNING! Some asynchronous traces have been lost! ===\n");
      async_print_trace_lost = false;
    }
  }

  /* "async_wait_queue" is not needed anymore */
  if(async_wait_queue)
  {
    vibe_os_release_queue_event(async_wait_queue);
    async_wait_queue = 0;
  }
}

#endif /* DEBUG */

/******************************************************************************
 *  IDebug implementation
 */
#if defined(DEBUG)
unsigned int trcmask[VIBE_OS_TRCMASK_SIZE] = { ( 1 << TRC_ID_BY_CONSOLE ) | ( 1 << TRC_ID_ERROR ) | ( 1 << TRC_ID_MAIN_INFO ), 0 };
#else
unsigned int trcmask[VIBE_OS_TRCMASK_SIZE] = { ( 1 << TRC_ID_BY_CONSOLE ) | ( 1 << TRC_ID_ERROR ), 0 };
#endif


#if defined(CONFIG_DISPLAY_REMOVE_TRACES)


int  vibe_debug_init(void) { return 0; }

void vibe_debug_release(void) { }

void vibe_break(const char *cond, const char *file, unsigned int line) { BUG(); }

void vibe_printf(const char *fmt,...) { }

void vibe_kpprintf(const char *fmt,...) { }

void vibe_trace( int id, char c, const char *pf, const char *fmt, ... ) { }


#else /* CONFIG_DISPLAY_REMOVE_TRACES */


#if defined(CONFIG_DEBUG_FS)
static ssize_t get_trace_levels(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
  int i = 0;
  ssize_t ret = 0;
  char out_buf[512];
  char *out_buf_ptr;
  int out_buf_size = 0;

  memset(out_buf, 0, sizeof(out_buf));

  out_buf_ptr = out_buf;

  // Print trace groups one by one
  for (i = 0 ; i < VIBE_OS_TRCMASK_SIZE; i++)
  {
    sprintf(out_buf_ptr, "%d ", trcmask[i]);
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'
  }

  sprintf((out_buf_ptr-1), "\n"); // Overwrite the last space
  ret = simple_read_from_buffer(user_buf, count, ppos, out_buf, out_buf_size);

  return ret;
}

static ssize_t set_trace_levels(struct file *file, const char *user_buf, size_t count, loff_t *ppos)
{
  int i = 0;
  char in_buf[512];
  char *buf;
  char *token;
  int val;

  memset(in_buf, '\0', sizeof(in_buf));

  if (simple_write_to_buffer(in_buf, sizeof(in_buf), ppos, user_buf, count) != count)
    return count;

  //strcat(in_buf, " "); // Add the last space

  buf = in_buf; /* since strsep modifies its pointer arg */
  for (token = strsep(&buf, " "); token != NULL; token = strsep(&buf, " "))
  {
    if (kstrtoint(token, 10, &val) != 0)
      break;

    if (i < VIBE_OS_TRCMASK_SIZE)
      trcmask[i++] = val;

    if (i == VIBE_OS_TRCMASK_SIZE)
      break;
  }

  while((i > 0) && (i < VIBE_OS_TRCMASK_SIZE))
  {
    trcmask[i] = 0;
    i++;
  }

  return count;
}

const struct file_operations trace_levels_fops =
{
  .open   = simple_open,
  .read   = get_trace_levels,
  .write  = set_trace_levels,
};

static struct dentry *vibe_trace_file;

#define VIBE_TRACE_FILE_NAME "vibe_trace_mask"

static void vibe_trace_create_debugfs(void)
{
  if(debugfs_initialized())
  {
    vibe_trace_file = debugfs_create_file(VIBE_TRACE_FILE_NAME, 0600, NULL,
                             (void *) &trcmask, &trace_levels_fops);
  }
}

static void vibe_trace_remove_debugfs(void)
{
  if(vibe_trace_file)
    debugfs_remove(vibe_trace_file);
}
#endif

int  vibe_debug_init(void)
{
  if ( TRC_ID_LAST > 64 )
    {
      printk( KERN_ERR "%c[%d;%dmToo many traces according to vibe_trace_mask size%c[%dm", SET_COLOR, BOLD, RED, SET_COLOR, ATTR_OFF );
    }

#if defined(CONFIG_DEBUG_FS)
  vibe_trace_create_debugfs();
#endif

  return 0;
}

void vibe_debug_release(void)
{
#if defined(CONFIG_DEBUG_FS)
  vibe_trace_remove_debugfs();
#endif
}


void vibe_break(const char *cond, const char *file, unsigned int line)
{
  printk(KERN_ERR "%s:%u:FATAL ASSERTION FAILURE %s\n", file, line, cond);
  BUG();
}

void vibe_printf(const char *fmt,...)
{
  va_list ap;

  va_start(ap,fmt);

#ifdef DEBUG
  if (in_interrupt())
  {
    /* In case of Interrupt context, use an asynchronous print */
    async_buffer_write_data(fmt, ap);
  }
  else
#endif
  {
    vprintk(fmt,ap);
  }

  va_end(ap);
}

void vibe_kpprintf(const char *fmt,...)
{
/* Enable KPTRACE inside the kernel, else no traces will be outputed */
/* make menuconfig => Kernel Hacking: <*> KPTrace */
#if defined(CONFIG_KPTRACE)
  va_list ap;

  va_start(ap,fmt);
  kpprintf((char*)fmt, ap);
  va_end(ap);
#endif
}

void vibe_trace( int id, char c, const char *pf, const char *fmt, ... )
{
  char arg[MAX_LENGTH_ARG];
  char fct[MAX_LENGTH_FCT];
  char prefix[MAX_STRING_LENGTH];
  int i, j;
  va_list ap;

  if ( ! ( trcmask[0] & (( 1 << TRC_ID_BY_CONSOLE ) | ( 1 << TRC_ID_BY_KPTRACE ) | ( 1 << TRC_ID_BY_STM ))))
    {
      return; /* no output selected */
    }

  fct[0] = 0;

  if ( c != ' ' ) // not done for TRCBL
    {
      /* get the function name (class name::function name in c++) from __PRETTY_FUNCTION__ */
      for ( i = 0, j = 0; i < MAX_LENGTH_FCT; i++ )
        {
          if (( pf[i] == '(' ) || ( pf[i] == 0 )) /* begin of parameters area */
            {
              break;
            }
          else if (( pf[i] == ' ' ) || ( pf[i] == '*' )) /* not still in function name */
            {
              j = 0;
            }
          else
            {
              fct[j++] = pf[i];
            }
        }

      fct[j++] = ' '; /* add " -" at the end of function name */
      fct[j++] = '-';
      fct[j] = 0; /* terminate function name string */
    }

  va_start( ap, fmt );

  if(id == TRC_ID_ERROR)
  {
    snprintf( prefix, MAX_STRING_LENGTH, "DE_ERROR");
  }
  else
  {
    prefix[0] = 0;
  }

  if ( trcmask[0] & ( 1 << TRC_ID_BY_CONSOLE ))
    {
      char color_in[11];
      char color_out[5];
      color_in[0] = 0;
      color_out[0] = 0;

      if ( id == TRC_ID_ERROR )
        {
          snprintf( color_in, 11, "%c[%d;%dm", SET_COLOR, BOLD, RED );
          snprintf( color_out, 5, "%c[%dm", SET_COLOR, ATTR_OFF );
        }

      vsnprintf( arg, MAX_LENGTH_ARG, fmt, ap );
      /* no blank needed between fct and arg because fmt always begins with a blank */
      if ( id == TRC_ID_ERROR )
        pr_err("vibe  %c %s %s%s%s%s\n", c, prefix, color_in, fct, arg, color_out);
      else
        pr_info("vibe  %c %s %s%s%s%s\n", c, prefix, color_in, fct, arg, color_out);
    }

#ifdef CONFIG_KPTRACE
  if ( trcmask[0] & ( 1 << TRC_ID_BY_KPTRACE ))
    {
      vsnprintf( arg, MAX_LENGTH_ARG, fmt, ap );
      /* no blank needed between fct and arg because fmt always begins with a blank */
      kpprintf( "vibe  %c %s %s%s\n", c, prefix, fct, arg );
    }
#endif

  va_end( ap );
}

#endif /* CONFIG_DISPLAY_REMOVE_TRACES */

/******************************************************************************/


static void __exit vibe_os_module_exit(void)
{
    printk(KERN_INFO "vibe_os_module_exit\n");

    vibe_debug_release();
    vibe_os_release();
}


static int __init vibe_os_module_init(void)
{
    printk(KERN_INFO "vibe_os_module_init\n");

    if(vibe_os_init()<0)
      return -1;

    if (vibe_debug_init()<0)
      return -1;

    return 0;
}

/******************************************************************************
 *  Modularization
 */

module_init(vibe_os_module_init);
module_exit(vibe_os_module_exit);

MODULE_LICENSE("GPL");

/******************************************************************************
 *  Exported symbols
 */

EXPORT_SYMBOL(vibe_os_init);
EXPORT_SYMBOL(vibe_os_release);
EXPORT_SYMBOL(vibe_os_create_thread);
EXPORT_SYMBOL(vibe_os_stop_thread);
EXPORT_SYMBOL(vibe_os_wake_up_thread);
EXPORT_SYMBOL(vibe_os_thread_should_stop);
EXPORT_SYMBOL(vibe_os_sleep_ms);
EXPORT_SYMBOL(vibe_os_read_register);
EXPORT_SYMBOL(vibe_os_write_register);
EXPORT_SYMBOL(vibe_os_read_byte_register);
EXPORT_SYMBOL(vibe_os_write_byte_register);
EXPORT_SYMBOL(vibe_os_get_system_time);
EXPORT_SYMBOL(vibe_os_get_one_second);
EXPORT_SYMBOL(vibe_os_stall_execution);
EXPORT_SYMBOL(vibe_os_map_memory);
EXPORT_SYMBOL(vibe_os_unmap_memory);
EXPORT_SYMBOL(vibe_os_zero_memory);
EXPORT_SYMBOL(vibe_os_memcmp);
EXPORT_SYMBOL(vibe_os_memcpy);
EXPORT_SYMBOL(vibe_os_check_dma_area_guards);
EXPORT_SYMBOL(vibe_os_flush_dma_area);
EXPORT_SYMBOL(vibe_os_memset_dma_area);
EXPORT_SYMBOL(vibe_os_memcpy_to_dma_area);
EXPORT_SYMBOL(vibe_os_memcpy_from_dma_area);
EXPORT_SYMBOL(vibe_os_allocate_memory);
EXPORT_SYMBOL(vibe_os_free_memory);
EXPORT_SYMBOL(vibe_os_grant_access);
EXPORT_SYMBOL(vibe_os_revoke_access);
EXPORT_SYMBOL(vibe_os_create_region);
EXPORT_SYMBOL(vibe_os_destroy_region);
EXPORT_SYMBOL(vibe_os_allocate_bpa2_dma_area);
EXPORT_SYMBOL(vibe_os_allocate_kernel_dma_area);
EXPORT_SYMBOL(vibe_os_allocate_dma_area);
EXPORT_SYMBOL(vibe_os_free_dma_area);
EXPORT_SYMBOL(vibe_os_get_dma_channel);
EXPORT_SYMBOL(vibe_os_release_dma_channel);
EXPORT_SYMBOL(vibe_os_stop_dma_channel);
EXPORT_SYMBOL(vibe_os_create_dma_transfer);
EXPORT_SYMBOL(vibe_os_delete_dma_transfer);
EXPORT_SYMBOL(vibe_os_start_dma_transfer);

EXPORT_SYMBOL(vibe_os_create_resource_lock);
EXPORT_SYMBOL(vibe_os_delete_resource_lock);
EXPORT_SYMBOL(vibe_os_lock_resource);
EXPORT_SYMBOL(vibe_os_unlock_resource);

EXPORT_SYMBOL(vibe_os_create_semaphore);
EXPORT_SYMBOL(vibe_os_delete_semaphore);
EXPORT_SYMBOL(vibe_os_down_semaphore);
EXPORT_SYMBOL(vibe_os_down_trylock_semaphore);
EXPORT_SYMBOL(vibe_os_up_semaphore);

EXPORT_SYMBOL(vibe_os_create_mutex);
EXPORT_SYMBOL(vibe_os_is_locked_mutex);
EXPORT_SYMBOL(vibe_os_delete_mutex);
EXPORT_SYMBOL(vibe_os_lock_mutex);
EXPORT_SYMBOL(vibe_os_trylock_mutex);
EXPORT_SYMBOL(vibe_os_unlock_mutex);

EXPORT_SYMBOL(vibe_os_release_firmware);
EXPORT_SYMBOL(vibe_os_request_firmware);
EXPORT_SYMBOL(vibe_os_snprintf);
EXPORT_SYMBOL(vibe_os_call_bios);
EXPORT_SYMBOL(vibe_os_div64);
EXPORT_SYMBOL(vibe_os_rational_reduction);

EXPORT_SYMBOL(vibe_os_create_work_queue);
EXPORT_SYMBOL(vibe_os_destroy_work_queue);
EXPORT_SYMBOL(vibe_os_allocate_workerdata);
EXPORT_SYMBOL(vibe_os_delete_workerdata);
EXPORT_SYMBOL(vibe_os_queue_workerdata);

EXPORT_SYMBOL(vibe_os_create_message_thread);
EXPORT_SYMBOL(vibe_os_destroy_message_thread);
EXPORT_SYMBOL(vibe_os_allocate_message);
EXPORT_SYMBOL(vibe_os_delete_message);
EXPORT_SYMBOL(vibe_os_queue_message);

EXPORT_SYMBOL(vibe_os_allocate_queue_event);
EXPORT_SYMBOL(vibe_os_release_queue_event);
EXPORT_SYMBOL(vibe_os_get_wait_queue_data);
EXPORT_SYMBOL(vibe_os_wake_up_queue_event);
EXPORT_SYMBOL(vibe_os_wait_queue_event);

EXPORT_SYMBOL(vibe_os_enable_irq);
EXPORT_SYMBOL(vibe_os_disable_irq_nosync);
EXPORT_SYMBOL(vibe_os_disable_irq);
EXPORT_SYMBOL(vibe_os_in_interrupt);

EXPORT_SYMBOL(vibe_os_gpio_get_value);
EXPORT_SYMBOL(vibe_os_get_chip_version);

EXPORT_SYMBOL(vibe_break);
EXPORT_SYMBOL(vibe_printf);
EXPORT_SYMBOL(vibe_kpprintf);
EXPORT_SYMBOL(vibe_trace);
EXPORT_SYMBOL(trcmask);

EXPORT_SYMBOL(vibe_os_clk_enable);
EXPORT_SYMBOL(vibe_os_clk_disable);
EXPORT_SYMBOL(vibe_os_clk_resume);
EXPORT_SYMBOL(vibe_os_clk_suspend);
EXPORT_SYMBOL(vibe_os_clk_set_parent);
EXPORT_SYMBOL(vibe_os_clk_get_parent);
EXPORT_SYMBOL(vibe_os_clk_set_rate);
EXPORT_SYMBOL(vibe_os_clk_get_rate);
