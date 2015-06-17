/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/devfbOS.c
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#if defined(CONFIG_BPA2)
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/cpu/timer.h>
#include <asm/processor.h>
#include <asm/clock.h>
#include <asm/atomic.h>
#include <asm/div64.h>

#if defined(CONFIG_STM_DMA)
#include <linux/stm/stm-dma.h>
#endif

#if defined(HAVE_BIOS)
#include <linux/stm/st_bios.h>
#endif

static const int dmaarea_debug =0;

static const ULONG botguard0  = 0x12345678;
static const ULONG botguard1  = 0x87654321;
static const ULONG topguard0  = 0x89abcdef;
static const ULONG topguard1  = 0xfedcba98;
static const ULONG guardfreed = 0x000f3eed;

static DECLARE_WAIT_QUEUE_HEAD(g_BlitterWaitQueue);
static DECLARE_WAIT_QUEUE_HEAD(g_BlitEngineQueue);

typedef struct _RESOURCE_LOCK
{
  spinlock_t    theLock;
  unsigned long lockFlags;
} RESOURCE_LOCK;


struct stm_dma_transfer_handle
{
  int channel;
  struct stm_dma_params *params;
};


#ifdef CONFIG_FW_LOADER
typedef struct
{
  struct list_head fw_list;
  STMFirmware      firmware;
  char             name[FIRMWARE_NAME_MAX + 1];

  atomic_t         users;
} STMFirmware_private;

struct firmware_data {
  struct list_head fw_cache;
};

static struct firmware_data firmware_data = {
  .fw_cache = LIST_HEAD_INIT (firmware_data.fw_cache),
};

static struct device firmware_device;
static int firmware_device_registered;
#endif

static int dma_req_lines[SDTP_LAST_ENTRY];

static void determine_dma_req_lines(void)
{
  switch(boot_cpu_data.type)
  {
#if defined(CONFIG_STM_DMA)
    case CPU_STX7105:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = 2;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 42;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
#if 0
    case CPU_STX7106:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 42;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
    case CPU_STX7108:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 32;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
#endif
    case CPU_STB7109:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = 1;
      dma_req_lines[SDTP_DENC1_TELETEXT] = -1;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
    case CPU_STX7111:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = 2;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 34;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
    case CPU_STX7141:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = 46;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 49;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
    case CPU_STX7200:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = 2;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 41;
      dma_req_lines[SDTP_DENC2_TELETEXT] = 42;
      break;
#if 0
    case CPU_STX5206:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
      dma_req_lines[SDTP_DENC1_TELETEXT] = 17;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
#endif
    case CPU_STB7100:
#endif /* CONFIG_STM_DMA */
    default:
      dma_req_lines[SDTP_HDMI1_IFRAME]   = -1;
      dma_req_lines[SDTP_DENC1_TELETEXT] = -1;
      dma_req_lines[SDTP_DENC2_TELETEXT] = -1;
      break;
  }
}

static int Init(void)
{
#ifdef CONFIG_FW_LOADER
  int failure;

  failure = device_register (&firmware_device);
  if (unlikely (failure))
    dev_err (&firmware_device,
             "Device registration [%s] failed: %d\n",
             firmware_device.bus_id, failure);
  firmware_device_registered = !failure;
#endif

  determine_dma_req_lines();

  return 0;
}


static void Release(void)
{
#ifdef CONFIG_FW_LOADER
  if (likely (firmware_device_registered))
    device_unregister (&firmware_device);
#endif
}


static wait_queue_head_t* GetWaitQueue(WAIT_QUEUE queue)
{
  switch(queue)
  {
    case BLIT_ENGINE_QUEUE:
      return &g_BlitEngineQueue;

    case BLIT_NODE_QUEUE:
      return &g_BlitterWaitQueue;

    default:
      return NULL;
  }
}


static int SleepOnQueue(WAIT_QUEUE queue,
                         volatile unsigned long* var,
                         unsigned long value,
                         int cond)
{
  wait_queue_head_t* pQueue = GetWaitQueue(queue);

  if(!pQueue)
    panic("IOS::WakeQueue invalid queue ID\n");

  if(!var)
  {
    // Unconditional sleep
    interruptible_sleep_on(pQueue);
    if(signal_pending(current))
      return -ERESTARTSYS;
  }
  else
  {
    // Two conditions, wait either until *var == value or *var != value
    return wait_event_interruptible(*pQueue,(cond?(*var == value):(*var != value)));
  }
  return 0;
}


static void WakeQueue(WAIT_QUEUE queue)
{
  wait_queue_head_t* pQueue = GetWaitQueue(queue);

  if(!pQueue)
    panic("IOS::WakeQueue invalid queue ID\n");

  wake_up_interruptible(pQueue);
}


static ULONG ReadRegister(volatile ULONG *reg)
{
  return readl((ULONG)reg);
}


static void WriteRegister(volatile ULONG *reg,ULONG val)
{
  writel(val, (ULONG)reg);
}


static UCHAR ReadByteRegister(volatile UCHAR *reg)
{
  return readb((ULONG)reg);
}


static void WriteByteRegister(volatile UCHAR *reg,UCHAR val)
{
  writeb(val, (ULONG)reg);
}


static TIME64 GetSystemTime(void)
{
  ktime_t currenttime = ktime_get();

  return ktime_to_us(currenttime);
}


static TIME64 GetOneSecond(void)
{
  return USEC_PER_SEC;
}


static void StallExecution(ULONG ulDelay)
{
  ULONG ulms = ulDelay/1000;
  ULONG ulus = ulDelay%1000;

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


static void* MapMemory(ULONG ulBaseAddr, ULONG ulSize)
{
  return ioremap(ulBaseAddr, ulSize);
}


static void UnMapMemory(void* pVMem)
{
  iounmap(pVMem);
}


static void ZeroMemory(void* pMem, unsigned long count)
{
  memset(pMem, 0, count);
}


static int CheckDMAAreaGuards(DMA_Area *mem)
{
  ULONG *guard,*topguard;

  if(!mem->pMemory)
    return 0;

  if(((ULONG)mem->pMemory & 0x3) != 0)
  {
    printk("CheckDMAAreaGuards: unaligned mem = %p pMemory = %p\n",mem,mem->pMemory);
    return -1;
  }

  guard = (ULONG*)mem->pMemory;
  if((guard[0] != botguard0) || (guard[1] != botguard1))
  {
    printk("CheckDMAAreaGuards: bad guard before data mem = %p pMemory = %p guard[0] = %lu guard[1] = %lu\n",mem,mem->pMemory,guard[0],guard[1]);
    return -1;
  }
  else
  {
    topguard = (ULONG *)(mem->pData+mem->ulDataSize);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      printk("CheckDMAAreaGuards: bad guard after data mem = %p pMemory = %p guard[0] = %lu guard[1] = %lu\n",mem,mem->pMemory,topguard[0],topguard[1]);
      return -1;
    }
  }

  return 0;
}


static void MemsetDMAArea(DMA_Area *mem, ULONG offset, int val, unsigned long count)
{
  if(dmaarea_debug)
    printk("MemsetDMAArea: mem = %p, pMemory = %p, pData = %p, size = %lu offset = %lu count = %lu\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset+count) > mem->ulDataSize)
  {
    printk("MemsetDMAArea: access out of range, size = %lu offset = %lu count = %lu\n",mem->ulDataSize,offset,count);
  }

  memset(mem->pData+offset, val, count);
  CheckDMAAreaGuards(mem);

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    dma_cache_wback_inv(mem->pData+offset, count);
}


static void MemcpyToDMAArea(DMA_Area *mem, ULONG offset, const void* pSrc, unsigned long count)
{
  if(dmaarea_debug)
    printk("MemcpyToDMAArea: mem = %p, pMemory = %p, pData = %p, size = %lu offset = %lu count = %lu\n",mem,mem->pMemory,mem->pData,mem->ulDataSize,offset,count);

  if((offset+count) > mem->ulDataSize)
  {
    printk("MemcpyToDMAArea: access out of range, size = %lu offset = %lu count = %lu\n",mem->ulDataSize,offset,count);
  }

  memcpy(mem->pData+offset, pSrc, count);
  CheckDMAAreaGuards(mem);

  if (!(mem->ulFlags & SDAAF_UNCACHED))
    dma_cache_wback_inv(mem->pData+offset, count);
}


static void *AllocateMemory(ULONG size)
{
  char  *mem   = (char*)kmalloc(size+(sizeof(ULONG)*4), GFP_KERNEL);
  ULONG *guard = (ULONG *)mem;

  if(mem == 0)
  {
    printk("AllocateMemory: unable to kmalloc %lu bytes\n",size);
    return 0;
  }

  guard[0] = size;
  guard[1] = botguard1;

  guard = (ULONG *)(mem+size+(sizeof(ULONG)*2));

  guard[0] = topguard0;
  guard[1] = topguard1;

  return mem+(sizeof(ULONG)*2);
}


static void FreeMemory(void *mem)
{
  ULONG *guard;
  ULONG *topguard;
  ULONG size;

  if(mem == 0)
    return;

  guard = (ULONG *)((char*)mem - (sizeof(ULONG)*2));

  size  = guard[0];

  if(guard[1] != botguard1)
  {
    printk("FreeMemory: bad guard before data mem = %p guard[0] = %lu guard[1] = %lu\n",mem,size,guard[1]);
    printk("FreeMemory: freeing anyway\n");
  }
  else
  {
    topguard = (ULONG *)((char*)mem+size);
    if((topguard[0] != topguard0) || (topguard[1] != topguard1))
    {
      printk("FreeMemory: bad guard after data mem = %p topguard[0] = %lu topguard[1] = %lu\n",mem,topguard[0],topguard[1]);
      printk("FreeMemory: freeing anyway\n");
    }
  }

  guard[0] = guardfreed;
  guard[1] = guardfreed;

  kfree(guard);

  return;
}


static void AllocateDMAArea(DMA_Area *mem, ULONG size, ULONG align, STM_DMA_AREA_ALLOC_FLAGS flags)
{
  ULONG *guard;

  memset(mem, 0, sizeof(DMA_Area));

  if(align<8)
    align = 8; /* Allow room for top and bottom memory guards */

  mem->ulAllocatedSize = size+(align*2);
  mem->ulDataSize      = size;
  mem->ulFlags         = flags;

  if(mem->ulAllocatedSize > 128*1024 || (flags & (SDAAF_VIDEO_MEMORY | SDAAF_UNCACHED)))
  {
    ULONG             nPages = (mem->ulAllocatedSize + PAGE_SIZE - 1) / PAGE_SIZE;
    const char       *partname;
    struct bpa2_part *part;

    mem->ulAllocatedSize = nPages*PAGE_SIZE;

    if(flags & SDAAF_VIDEO_MEMORY)
      partname = "coredisplay-video";
    else
      partname = "bigphysarea";

    part = bpa2_find_part(partname);
    if(part)
      mem->ulPhysical = bpa2_alloc_pages(part, nPages, 1, GFP_KERNEL);
    else
      printk("%s: BPA2 partition '%s' not available in this kernel\n",
             __PRETTY_FUNCTION__, partname);
  }
  else
  {
    mem->pMemory = kmalloc(mem->ulAllocatedSize, GFP_KERNEL);
  }

  if((mem->pMemory == 0) && (mem->ulPhysical == 0))
  {
    memset(mem, 0, sizeof(DMA_Area));
    return;
  }

  if(mem->pMemory)
  {
    /* Align the data area in the standard fashion */
    mem->pData = (char*)(((ULONG)mem->pMemory + (align-1)) & ~(align-1));
    /* Now move it up to allow room for the bottom memory guard */
    mem->pData += align;

    mem->ulPhysical = virt_to_phys(mem->pData);
  }
  else
  {
    /*
     * Slightly more convoluted for memory returned from bpa2, as this is
     * from memory that isn't known about by the Linux virtual memory system.
     */
    mem->pMemory = (flags & SDAAF_UNCACHED)
                   ? ioremap_nocache(mem->ulPhysical, mem->ulAllocatedSize)
                   : ioremap_cache(mem->ulPhysical, mem->ulAllocatedSize);
    mem->pData = (char*)(((ULONG)mem->pMemory + (align-1)) & ~(align-1));
    mem->pData += align;
    mem->ulPhysical += (ULONG)mem->pData - (ULONG)mem->pMemory;
  }

  if(dmaarea_debug)
    printk("AllocateDMAArea: mem = %p, pMemory = %p (%s), pData = %p,\n\t allocated size = %lu data size = %lu\n",mem,mem->pMemory,(flags&SDAAF_UNCACHED)?"uncached":"cached",mem->pData,mem->ulAllocatedSize,mem->ulDataSize);

  guard = (ULONG*)mem->pMemory; /* Note: NOT pData!!! */
  guard[0] = botguard0;
  guard[1] = botguard1;

  guard = (ULONG*)(mem->pData+mem->ulDataSize);
  guard[0] = topguard0;
  guard[1] = topguard1;

  if (!(flags & SDAAF_UNCACHED))
    dma_cache_wback_inv(mem->pMemory, mem->ulAllocatedSize);
}


static void FreeDMAArea(DMA_Area *mem)
{
  ULONG *guard;

  if(dmaarea_debug)
    printk("FreeDMAArea: mem = %p, pMemory = %p, pData = %p, size = %lu\n",mem,mem->pMemory,mem->pData,mem->ulDataSize);

  if(!mem->pMemory)
    return;

  if(CheckDMAAreaGuards(mem)<0)
    printk("FreeDMAArea: ******************* freeing corrupted memory ***************\n");

  guard = (ULONG*)mem->pMemory;
  guard[0] = guardfreed;
  guard[1] = guardfreed;

  if ((mem->ulAllocatedSize > 128*1024) || (mem->ulFlags & (SDAAF_VIDEO_MEMORY | SDAAF_UNCACHED)))
  {
    const char       *partname;
    struct bpa2_part *part;

    if(mem->ulFlags & SDAAF_VIDEO_MEMORY)
      partname = "coredisplay-video";
    else
      partname = "bigphysarea";

    part = bpa2_find_part(partname);
    if(part)
    {
      mem->ulPhysical -= (ULONG)mem->pData - (ULONG)mem->pMemory;
      iounmap(mem->pMemory);
      bpa2_free_pages(part, mem->ulPhysical);
    }
  }
  else
    kfree(mem->pMemory);

  memset(mem, 0, sizeof(DMA_Area));
}

#if defined(CONFIG_STM_DMA)
static const char *fdmac_id[]    = { STM_DMAC_ID, NULL };
static const char *fdma_cap_lb[] = { STM_DMA_CAP_LOW_BW, NULL };

static DMA_Channel *GetDMAChannel(STM_DMA_TRANSFER_PACING pacing, ULONG bytes_per_req, STM_DMA_TRANSFER_FLAGS flags)
{

  DMA_Channel *chan = kzalloc(sizeof(DMA_Channel), GFP_KERNEL);

  if(!chan)
    return NULL;

  chan->channel_nr = request_dma_bycap(fdmac_id, fdma_cap_lb, "coredisplay");

  if(chan->channel_nr<0)
  {
    kfree(chan);
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


static void ReleaseDMAChannel(DMA_Channel *chan)
{
  if(!chan)
    return;

  if(chan->paced_request)
    dma_req_free(chan->channel_nr,(struct stm_dma_req *)chan->paced_request);

  free_dma(chan->channel_nr);
  kfree(chan);
}


static void StopDMAChannel(DMA_Channel *chan)
{
  if(!chan)
    return;

  dma_stop_channel(chan->channel_nr);
  dma_wait_for_completion(chan->channel_nr);
}


static void* CreateDMATransfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags)
{
  DMA_transfer *tmp = transfer_list;
  struct stm_dma_params *params = NULL,*tail = NULL;
  struct stm_dma_transfer_handle *handle;

  if(!transfer_list)
    return NULL;

  if(chan == NULL)
  {
    BUG();
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
    printk("%s: src =%p dst = %p offset = %lu size = %lu\n",
           __FUNCTION__, tmp->src, tmp->dst, tmp->src_offset, tmp->size);

    BUG_ON(CheckDMAAreaGuards(tmp->src)<0);
    BUG_ON(CheckDMAAreaGuards(tmp->dst)<0);
#endif

    if((tmp->src_offset+tmp->size) > tmp->src->ulDataSize)
    {
      printk(KERN_ERR "%s: src parameters out of range offset = %lu size = %lu dmaarea size = %lu\n",
                       __FUNCTION__, tmp->src_offset, tmp->size, tmp->src->ulDataSize);
      goto error_exit;
    }

    if(!(flags & SDTF_DST_IS_SINGLE_REGISTER) &&
        (tmp->dst_offset+tmp->size > tmp->dst->ulDataSize))
    {
      printk(KERN_ERR "%s: dst parameters out of range offset = %lu size = %lu dmaarea size = %lu\n",
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
      dma_params_comp_cb(node, tmp->completed_cb, tmp->cb_param, STM_DMA_CB_CONTEXT_ISR);

#ifdef DEBUG
    printk("%s: 0x%lx (%lu) -> 0x%lx\n",__FUNCTION__,node->sar,node->node_bytes,node->dar);
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
    kfree(params);
    params = tmp;
  }

  kfree(handle);

  return NULL;
}


static void  DeleteDMATransfer(void *handle)
{
  struct stm_dma_transfer_handle *transfer = (struct stm_dma_transfer_handle*)handle;
  struct stm_dma_params *params;

  if(!handle)
    return;

  params = transfer->params;
  if(params->req != NULL)
    dma_req_free(transfer->channel,params->req);

  /*
   * This doesn't actually free the list, it frees any internal memory
   * created when the list was compiled.
   */
  dma_params_free(params);

  /*
   * We have to free the list ourselves.
   */
  while(params)
  {
    struct stm_dma_params *tmp = params->next;
    kfree(params);
    params = tmp;
  }

  kfree(transfer);
}


static int StartDMATransfer(void *handle)
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
#else /* CONFIG_STM_DMA */
static DMA_Channel *GetDMAChannel(STM_DMA_TRANSFER_PACING pacing, ULONG bytes_per_req, STM_DMA_TRANSFER_FLAGS flags) { return NULL; }
static void ReleaseDMAChannel(DMA_Channel *chan) {}
static void StopDMAChannel(DMA_Channel *chan) {}
static void* CreateDMATransfer(DMA_Channel *chan, DMA_transfer *transfer_list, STM_DMA_TRANSFER_PACING pacing, STM_DMA_TRANSFER_FLAGS flags) { return NULL; }
static void  DeleteDMATransfer(void *handle) {}
static int StartDMATransfer(void *handle) { return -1; }
#endif /* CONFIG_STM_DMA */

static ULONG CreateResourceLock(void)
{
  RESOURCE_LOCK *pLock = kmalloc(sizeof(RESOURCE_LOCK),GFP_KERNEL);
  spin_lock_init(&(pLock->theLock));
  return (unsigned long)pLock;
}


static void DeleteResourceLock(ULONG ulHandle)
{
  kfree((void*)ulHandle);
}


static void LockResource(ULONG ulHandle)
{
  RESOURCE_LOCK *pLock = (RESOURCE_LOCK*)ulHandle;
  spin_lock_irqsave(&(pLock->theLock), pLock->lockFlags);
}


static void UnlockResource(ULONG ulHandle)
{
  RESOURCE_LOCK *pLock = (RESOURCE_LOCK*)ulHandle;
  spin_unlock_irqrestore(&(pLock->theLock),pLock->lockFlags);
}


static ULONG CreateSemaphore(int initVal)
{
  struct semaphore *pSema = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
  sema_init(pSema,initVal);
  return (unsigned long)pSema;
}


static void DeleteSemaphore(ULONG ulHandle)
{
  kfree((void*)ulHandle);
}


static int DownSemaphore(ULONG ulHandle)
{
  struct semaphore *pSema = (struct semaphore*)ulHandle;
  return down_interruptible(pSema);
}


static void UpSemaphore(ULONG ulHandle)
{
  struct semaphore *pSema = (struct semaphore*)ulHandle;
  up(pSema);
}


static void FlushCache(void *vaddr, int size)
{
  dma_cache_wback_inv(vaddr, size);
}



static void ReleaseFirmware (const STMFirmware *fw_pub)
{
#ifdef CONFIG_FW_LOADER
  STMFirmware_private * const fw  = container_of (fw_pub,
                                                  STMFirmware_private,
                                                  firmware);

  BUG_ON (fw_pub == NULL);

  if (atomic_dec_and_test (&fw->users))
    {
      /* no users left */
      list_del (&fw->fw_list);

      vfree (fw->firmware.pData);
      kfree (fw);
    }
#endif
}

#ifdef CONFIG_FW_LOADER
static void
firmware_dev_release (struct device * device)
{
  /* free firmware cache */
  struct firmware_data * const fwd = (struct firmware_data *) (device->driver_data);
  struct list_head *pos, *n;

  list_for_each_safe (pos, n, &fwd->fw_cache)
  {
    STMFirmware_private * const fw = list_entry (pos,
                                                 STMFirmware_private,
                                                 fw_list);
    BUG_ON (atomic_read (&fw->users) != 1);
    ReleaseFirmware (&fw->firmware);
  }
}

static struct device firmware_device = {
  .bus_id      = "stm_fw0",
  .driver_data = &firmware_data,
  .release     = firmware_dev_release,
};

static int CacheGetFirmware (const char                 * const name,
                             const STMFirmware_private **st_firmware_priv)
{
  struct list_head     *pos;
  struct firmware_data * const fwd = (struct firmware_data *) (firmware_device.driver_data);
  int                   error = ENOENT;

  BUG_ON (!st_firmware_priv);
  BUG_ON (*st_firmware_priv != NULL);

  /* first check if we have it cached already */
  list_for_each (pos, &fwd->fw_cache)
  {
    STMFirmware_private * const fw = list_entry (pos,
                                                 STMFirmware_private,
                                                 fw_list);
    if (!strcmp (fw->name, name))
    {
      /* yes - so use it */
      atomic_inc (&fw->users);

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
    struct firmware     *lnx_firmware;
    STMFirmware_private *fw;

    /* get some memory */
    fw = kzalloc (sizeof (*fw), GFP_KERNEL);
    if (!fw)
    {
      error = -ENOMEM;
      goto out;
    }

    /* get linux firmware */
    printk ("requesting frm: '%s', %p/%p\n",
            name, &firmware_device, &lnx_firmware);
    error = request_firmware ((const struct firmware **) (&lnx_firmware),
                              name, &firmware_device);
    if (unlikely (error))
    {
      kfree (fw);
      goto out;
    }

    printk ("frm '%s' successfully loaded\n", name);

    /* have the firmware */
    *st_firmware_priv = fw;

    /* remember fw data */
    strncpy (fw->name, name, FIRMWARE_NAME_MAX);
    fw->name[FIRMWARE_NAME_MAX] = '\0';
    atomic_set (&fw->users, 2); /* 2 users: requester & us */

    /* steal linux' firmware pointer */
    fw->firmware.pData = lnx_firmware->data;
    lnx_firmware->data = NULL;
    fw->firmware.ulDataSize = lnx_firmware->size;

    /* add to cache */
    list_add_tail (&fw->fw_list, &fwd->fw_cache);

    /* release linux firmware */
    release_firmware (lnx_firmware);
  }

out:
  if (unlikely (error))
    printk ("loading frm: '%s' failed: %d\n", name, error);
  return error;
}
#endif

static int RequestFirmware (const STMFirmware **firmware_p,
                            const char         * const name)
{
#ifdef CONFIG_FW_LOADER
  int                        error;
  const STMFirmware_private *st_firmware_priv = NULL;

  BUG_ON (!name);
  BUG_ON (*name == '\0');
  BUG_ON (!firmware_p);
  BUG_ON (*firmware_p != NULL);
  if (unlikely (!firmware_p
                || *firmware_p
                || !name
                || !*name))
  {
    return -EINVAL;
  }

  if (unlikely (!firmware_device_registered))
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

static int
__attribute__ ((format (printf, 3, 4)))
Snprintf(char * buf, ULONG size, const char * fmt, ...)

{
  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,size,fmt,args);
  va_end(args);
  return i;
}


static int
AtomicCmpXchg (ATOMIC *atomic, int oldval, int newval)
{
  return atomic_cmpxchg ((atomic_t *) atomic, oldval, newval);
}

static void
AtomicSet (ATOMIC *atomic, int val)
{
  atomic_set (atomic, val);
}

static void
Barrier (void)
{
  mb ();
}

static ULONG
CallBios (long         call,
          unsigned int n_args, ...)
{
#ifdef HAVE_BIOS
  va_list va_args;
  ULONG retval;
  LONG args[n_args];

  va_start (va_args, n_args);
  for (retval = 0; retval < n_args; ++retval)
    args[retval] = va_arg (va_args, LONG);
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

static IOS devFBIOS = {
  .Init               = Init,
  .Release            = Release,

  .ReadRegister       = ReadRegister,
  .WriteRegister      = WriteRegister,
  .ReadByteRegister   = ReadByteRegister,
  .WriteByteRegister  = WriteByteRegister,

  .GetSystemTime      = GetSystemTime,
  .GetOneSecond       = GetOneSecond,
  .StallExecution     = StallExecution,

  .MapMemory          = MapMemory,
  .UnMapMemory        = UnMapMemory,

  .ZeroMemory         = ZeroMemory,
  .MemsetDMAArea      = MemsetDMAArea,
  .MemcpyToDMAArea    = MemcpyToDMAArea,
  .CheckDMAAreaGuards = CheckDMAAreaGuards,

  .GetDMAChannel      = GetDMAChannel,
  .ReleaseDMAChannel  = ReleaseDMAChannel,
  .StopDMAChannel     = StopDMAChannel,
  .CreateDMATransfer  = CreateDMATransfer,
  .DeleteDMATransfer  = DeleteDMATransfer,
  .StartDMATransfer   = StartDMATransfer,

  .AllocateMemory     = AllocateMemory,
  .FreeMemory         = FreeMemory,

  .AllocateDMAArea    = AllocateDMAArea,
  .FreeDMAArea        = FreeDMAArea,

  .FlushCache         = FlushCache,

  .OSPageSize         = PAGE_SIZE,

  .CreateResourceLock = CreateResourceLock,
  .DeleteResourceLock = DeleteResourceLock,
  .LockResource       = LockResource,
  .UnlockResource     = UnlockResource,

  .CreateSemaphore    = CreateSemaphore,
  .DeleteSemaphore    = DeleteSemaphore,
  .DownSemaphore      = DownSemaphore,
  .UpSemaphore        = UpSemaphore,

  .WakeQueue          = WakeQueue,
  .SleepOnQueue       = SleepOnQueue,

  .RequestFirmware    = RequestFirmware,
  .ReleaseFirmware    = ReleaseFirmware,

  .snprintf           = Snprintf,

  .AtomicCmpXchg      = AtomicCmpXchg,
  .AtomicSet          = AtomicSet,
  .Barrier            = Barrier,

  .CallBios           = CallBios,
};

IOS *g_pIOS = &devFBIOS;

//////////////////////////////////////////////////////////////////////////////
// IDebug implementaiton
static int  IDebugInit(void)
{
  return 0;
}

static void IDebugRelease(void) {}

static void IDebugBreak(const char *cond, const char *file, unsigned int line)
{
  panic("\n%s:%u:FATAL ASSERTION FAILURE %s\n", file, line, cond);
}


static void IDebugPrintf(const char *fmt,...)
{
  // In windows CE, debug strings are all in unicode!
  // So, we have to sprintf our string into a char string,
  // and then convert that into a unicode string before passing
  // to the debug print function!

  char buffer[1024];
  va_list ap;
  va_start(ap,fmt);
  vsprintf(buffer,fmt,ap);
  printk(buffer);
  va_end(ap);
}

static IDebug devFBDebug = {
   .Init         = IDebugInit,
   .Release      = IDebugRelease,
   .IDebugPrintf = IDebugPrintf,
   .IDebugBreak  = IDebugBreak
};

IDebug *g_pIDebug = &devFBDebug;



//////////////////////////////////////////////////////////////////////////////
// Services required by C++ code. These are typically part of the C++ run time
// but we dont have a run time!

void *_Znwj(unsigned int size)
{
  if(size>0)
    return AllocateMemory(size);

  return NULL;
}

void *_Znaj(unsigned int size)
{
  if(size>0)
    return AllocateMemory(size);

  return NULL;
}

void* __builtin_new(size_t size)
{
  if(size>0)
    return AllocateMemory(size);

  return NULL;
}

void* __builtin_vec_new(size_t size)
{
  return __builtin_new(size);
}

void _ZdlPv(void *ptr)
{
  if(ptr)
    FreeMemory(ptr);
}

void _ZdaPv(void *ptr)
{
  if(ptr)
    FreeMemory(ptr);
}

void __builtin_delete(void *ptr)
{
  if(ptr)
    FreeMemory(ptr);
}

void __builtin_vec_delete(void* ptr)
{
  __builtin_delete(ptr);
}


void __pure_virtual(void)
{
}

void __cxa_pure_virtual(void)
{
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)

u64 __udivdi3(u64 n, u64 d)
{
  if (unlikely(d & 0xffffffff00000000ULL))
  {
    printk(KERN_WARNING "stmfb: 64-bit/64-bit division will lose precision due to 64bit divisor (0x%llx / 0x%llx)\n",n,d);
    BUG();
  }

  return div64_64(n, d);
}

#endif
