/***********************************************************************
 *
 * File: linux/drivers/media/video/stmvout_buffers.c
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
 ***********************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <media/v4l2-dev.h>

#if defined(CONFIG_BPA2)
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <asm/cacheflush.h>
#include <asm/semaphore.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/div64.h>

#include <stmdisplay.h>

#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "stmvout.h"

static void
bufferFinishedCallback (void                                  *pUser,
                        const stm_buffer_presentation_stats_t *stats);

static void timestampCallback (void   *pDevId,
                               TIME64  vsyncTime);


/* Map from SURF_FORMAT_... to V4L2 format descriptions */
typedef struct {
  struct v4l2_fmtdesc fmt;
  int    depth;
} format_info;

/* +1 to make sure the array covers all the SURF_FMT enumerations */
static const format_info g_capfmt[SURF_END+1] =
{
  /*
   * This isn't strictly correct as the V4L RGB565 format has red
   * starting at the least significant bit. Unfortunately there is no V4L2 16bit
   * format with blue starting at the LSB. It is recognised in the V4L2 API
   * documentation that this is strange and that drivers may well lie for
   * pragmatic reasons.
   */
  [SURF_RGB565]     = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "RGB-16 (5-6-5)", V4L2_PIX_FMT_RGB565 },    16},

  [SURF_ARGB1555]   = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "RGBA-16 (5-5-5-1)", V4L2_PIX_FMT_BGRA5551 }, 16},

  [SURF_ARGB4444]   = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "RGBA-16 (4-4-4-4)", V4L2_PIX_FMT_BGRA4444 }, 16},

  [SURF_RGB888]     = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "RGB-24 (B-G-R)", V4L2_PIX_FMT_BGR24 },     24},

  [SURF_ARGB8888]   = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "ARGB-32 (8-8-8-8)", V4L2_PIX_FMT_BGR32 },  32}, /* Note that V4L2 does
                                                         * not define the alpha
                                                         * channel */

  [SURF_BGRA8888]   = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "BGRA-32 (8-8-8-8)", V4L2_PIX_FMT_RGB32 },  32}, /* Bigendian BGR as BTTV
                                                         * driver, not as V4L2
                                                         * spec */

  [SURF_YCBCR422R]  = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:2 (U-Y-V-Y)", V4L2_PIX_FMT_UYVY }, 16},

  [SURF_YUYV]       = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:2 (Y-U-Y-V)", V4L2_PIX_FMT_YUYV }, 16},

  [SURF_YCBCR422MB] = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:2MB", V4L2_PIX_FMT_STM422MB },     8}, /* bpp for Luma only */

  [SURF_YCBCR420MB] = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:0MB", V4L2_PIX_FMT_STM420MB },     8}, /* bpp for Luma only */

  [SURF_YUV420]     = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:0 (YUV)", V4L2_PIX_FMT_YUV420 },   8}, /* bpp for Luma only */

  [SURF_YVU420]     = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:0 (YVU)", V4L2_PIX_FMT_YVU420 },   8}, /* bpp for Luma only */

  [SURF_YUV422P]    = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "YUV 4:2:2 (YUV)", V4L2_PIX_FMT_YUV422P },  8}, /* bpp for Luma only */

  [SURF_CLUT2]      = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "CLUT2 (RGB)", V4L2_PIX_FMT_CLUT2 },  2},

  [SURF_CLUT4]      = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "CLUT4 (RGB)", V4L2_PIX_FMT_CLUT4 },  4},

  [SURF_CLUT8]      = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "CLUT8 (RGB)", V4L2_PIX_FMT_CLUT8 },  8},

  [SURF_ACLUT88]    = {{ 0, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
			 "CLUT8 with Alpha (RGB)", V4L2_PIX_FMT_CLUTA8 },  16},
};


/*******************************************************************************
 *
 */
void
stmvout_init_buffer_queues (stvout_device_t * const device)
{
  if (!device->queues_inited) {
    int i;

    rwlock_init (&device->pendingStreamQ.lock);
    INIT_LIST_HEAD (&device->pendingStreamQ.list);

    rwlock_init (&device->completeStreamQ.lock);
    INIT_LIST_HEAD (&device->completeStreamQ.list);

    for (i = 0; i < MAX_USER_BUFFERS; ++i)
      INIT_LIST_HEAD (&device->userBuffers[i].node);

    device->queues_inited = 1;
  }
}


/******************************************************************************
 *  stmvout_send_next_buffer_to_display
 *
 *  Push a frame from the V4L2 queue to the display driver's queue
 */
void
stmvout_send_next_buffer_to_display (stvout_device_t *pDev)
{
  streaming_buffer_t *pStreamBuffer, *tmp;
  unsigned long       flags;
  int                 ret;

  read_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  list_for_each_entry_safe (pStreamBuffer,
                            tmp, &pDev->pendingStreamQ.list, node) {
    read_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

    ret = stm_display_plane_queue_buffer (pDev->pPlane,
					  &pStreamBuffer->bufferHeader);

    if (ret < 0) {
      /* There is a signal pending or the hardware queue is full. In either
         case do not take the streaming buffer off the pending queue,
         because it hasn't been written yet. */
      debug_msg ("%s: buffer queueing failed\n", __PRETTY_FUNCTION__);
      return;
    }

    write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
    list_del_init (&pStreamBuffer->node);
    write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

    read_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  }
  read_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);
}


/************************************************
 * Callback routines for buffer displayed/buffer
 * complete events from the device driver.
 ************************************************/
static void
timestampCallback (void   *pUser,
                   TIME64  vsyncTime)
{
  streaming_buffer_t *pStreamBuffer = (streaming_buffer_t *)pUser;
  UTIME64 tmp       = 0;
  TIME64  seconds   = 0;

  /*
   * We need to cope with negative time, but we cannot use ns_to_timeval
   * because that iterates in a loop counting seconds!!!! But we can do it
   * instead with some careful use of unsigned 64bit divides.
   */
  if(unlikely(vsyncTime < 0))
    tmp = (UTIME64)(-vsyncTime);
  else
    tmp = (UTIME64)vsyncTime;

  /*
   * do_div modifies tmp, which is ok becuase the expression
   * is tmp = tmp/oneSecond
   */
  do_div(tmp,USEC_PER_SEC);

  if(unlikely(vsyncTime < 0))
    seconds = -((TIME64)tmp) - 1;
  else
    seconds = (TIME64)tmp;

  pStreamBuffer->vidBuf.timestamp.tv_sec  = seconds;
  pStreamBuffer->vidBuf.timestamp.tv_usec = vsyncTime - (seconds*USEC_PER_SEC);
}


static void
bufferFinishedCallback (void                                  *pUser,
                        const stm_buffer_presentation_stats_t *stats)
{
  streaming_buffer_t *pStreamBuffer = (streaming_buffer_t *)pUser;
  stvout_device_t    *pDev	    = pStreamBuffer->pDev;
  unsigned long       flags;

  if(pDev->isStreaming)
  {
    pStreamBuffer->vidBuf.flags |= V4L2_BUF_FLAG_DONE;

    write_lock_irqsave (&pDev->completeStreamQ.lock, flags);
    list_add_tail (&pStreamBuffer->node, &pDev->completeStreamQ.list);
    write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

    wake_up_interruptible(&pDev->wqBufDQueue);
  }
}


static void
allocateBuffer (unsigned long       size,
                streaming_buffer_t *buf)
{
  static const char *partnames[] = { "bigphysarea", "v4l2-video-buffers" };
  int                partname_idx;
  struct bpa2_part  *part;
  unsigned long      nPages, clutOffset;

  if((buf->pDev->planeConfig->flags & STMCORE_PLANE_MEM_ANY)
     == STMCORE_PLANE_MEM_VIDEO)
    partname_idx = 1;
  else
    partname_idx = 0;

#define CLUT_GDP_ALIGNMENT  16
#define CLUT_SIZE           (256 * (sizeof (u32)))
  /* GDP CLUTs need to be aligned to 16 bytes */
  clutOffset = _ALIGN_UP (size, CLUT_GDP_ALIGNMENT);
  size = clutOffset + CLUT_SIZE;
  nPages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

restart:
  buf->partname = partnames[partname_idx];
  part = bpa2_find_part(buf->partname);
  if(!part)
  {
    buf->partname = NULL;
    buf->physicalAddr = 0;
    buf->cpuAddr = NULL;
    return;
  }

  buf->physicalAddr = bpa2_alloc_pages(part, nPages, 1, GFP_KERNEL);
  if (!buf->physicalAddr)
  {
    /* try to alloc from the other partition */
    if(partname_idx == 0
       && ((buf->pDev->planeConfig->flags & STMCORE_PLANE_MEM_ANY)
           == STMCORE_PLANE_MEM_ANY))
    {
      partname_idx++;
      info_msg ("bpa2 part '%s' out of memory, retrying from '%s'\n",
                buf->partname, partnames[partname_idx]);
      goto restart;
    }
    else
    {
      info_msg ("bpa2 part '%s' out of memory\n", buf->partname);
      return;
    }
  }

  /* most buffers may not cross a 64MB boundary */
#define PAGE64MB_SIZE  (1<<26)
  if ((buf->physicalAddr & ~(PAGE64MB_SIZE - 1))
      != ((buf->physicalAddr + size) & ~(PAGE64MB_SIZE - 1)))
  {
    printk ("%s: %8.lx + %lu (%lu pages) crosses a 64MB boundary, retrying!\n",
            __FUNCTION__, buf->physicalAddr, size, nPages);

    /* FIXME: this is a hack until the point we get an updated BPA2 driver.
       Will be removed again! */
    /* free and immediately reserve, hoping nobody requests bpa2 memory in
       between... */
    bpa2_free_pages (part, buf->physicalAddr);
    buf->physicalAddr = bpa2_alloc_pages (part, nPages,
                                          PAGE64MB_SIZE / PAGE_SIZE,
                                          GFP_KERNEL);
    printk ("%s: new start: %.8lx\n", __FUNCTION__, buf->physicalAddr);
  }

  if ((buf->physicalAddr & ~(PAGE64MB_SIZE - 1))
      != ((buf->physicalAddr + size) & ~(PAGE64MB_SIZE - 1)))
  {
    /* can only happen, if somebody else requested bpa2 memory while we were
       busy here */
    printk ("%s: %8.lx + %lu (%lu pages) crosses a 64MB boundary again! "
            "Ignoring this time...\n",
            __FUNCTION__, buf->physicalAddr, size, nPages);
  }

  BUG_ON (buf->physicalAddr == 0);

  buf->cpuAddr = ioremap_cache(buf->physicalAddr, nPages*PAGE_SIZE);
  buf->clut_phys = buf->physicalAddr + clutOffset;
  buf->clut_virt = buf->cpuAddr + clutOffset;
}


static void
freeBuffer (streaming_buffer_t *buf)
{
  struct bpa2_part *part;

  if (!buf->partname)
    return;

  part = bpa2_find_part(buf->partname);

  iounmap(buf->cpuAddr);
  bpa2_free_pages(part, buf->physicalAddr);
  buf->partname = NULL;
  buf->physicalAddr = 0;
  buf->cpuAddr = NULL;

  buf->clut_phys = 0;
  buf->clut_virt = NULL;
}


/******************************************************************************
 *
 */
int
stmvout_allocate_mmap_buffers (stvout_device_t            *pDev,
                               struct v4l2_requestbuffers *req)
{
  int           i;
  unsigned long bufLen;

  /* set the buffer length to a multiple of the page size */
  bufLen = ((pDev->bufferFormat.fmt.pix.sizeimage + PAGE_SIZE - 1)
            & ~(PAGE_SIZE - 1));

  debug_msg ("%s: %d buffers of 0x%x bytes\n", __FUNCTION__,req->count,
             (unsigned int) bufLen);

  /* now allocate the buffers */
  BUG_ON (pDev->n_streamBuffers != 0);
  BUG_ON (pDev->streamBuffers != NULL);

  pDev->streamBuffers = vmalloc (req->count * sizeof (streaming_buffer_t));
  if (!pDev->streamBuffers)
    return 0;
  /* FIXME: that's pretty expensive! */
  memset (pDev->streamBuffers, 0, req->count * sizeof (streaming_buffer_t));
  pDev->n_streamBuffers = req->count;

  for (i = 0; i < req->count; ++i)
  {
    pDev->streamBuffers[i].pDev = pDev;

    allocateBuffer(bufLen, &pDev->streamBuffers[i]);

    if (pDev->streamBuffers[i].physicalAddr != 0)
    {
      debug_msg ("%s: Allocated buffer %d at 0x%08lx.\n",__FUNCTION__, i,
                 (unsigned long) pDev->streamBuffers[i].physicalAddr);

      INIT_LIST_HEAD (&pDev->streamBuffers[i].node);

      pDev->streamBuffers[i].bufferHeader.src.ulVideoBufferAddr
        = pDev->streamBuffers[i].physicalAddr;
      memset(&(pDev->streamBuffers[i].vidBuf), 0, sizeof(struct v4l2_buffer));

      pDev->streamBuffers[i].vidBuf.index  = i;
      pDev->streamBuffers[i].vidBuf.type   = req->type;
      pDev->streamBuffers[i].vidBuf.field  = pDev->bufferFormat.fmt.pix.field;

      pDev->streamBuffers[i].vidBuf.memory = V4L2_MEMORY_MMAP;
      /*
       * Warning using the physical address as the mmap offset will break on
       * big 32bit address spaces.
       */
      pDev->streamBuffers[i].vidBuf.m.offset
        = pDev->streamBuffers[i].bufferHeader.src.ulVideoBufferAddr;
      pDev->streamBuffers[i].vidBuf.length = bufLen;

      /* Clear the buffer to prevent data leaking leaking into userspace */
      memset(pDev->streamBuffers[i].cpuAddr, 0, bufLen);
      dma_cache_wback(pDev->streamBuffers[i].cpuAddr, bufLen);

      pDev->streamBuffers[i].bufferHeader.src.ulVideoBufferSize = bufLen;

      /* This is only applicable for YUV buffers with separate luma and chroma */
      pDev->streamBuffers[i].bufferHeader.src.chromaBufferOffset
        = pDev->bufferFormat.fmt.pix.priv;

      pDev->streamBuffers[i].bufferHeader.src.ulColorFmt = pDev->planeFormat;
      pDev->streamBuffers[i].bufferHeader.src.ulPixelDepth
        = g_capfmt[pDev->planeFormat].depth;

      pDev->streamBuffers[i].bufferHeader.src.ulStride
        = pDev->bufferFormat.fmt.pix.bytesperline;
      pDev->streamBuffers[i].bufferHeader.src.ulTotalLines
        = pDev->bufferFormat.fmt.pix.height;
      pDev->streamBuffers[i].bufferHeader.src.Rect.x
        = pDev->streamBuffers[i].bufferHeader.src.Rect.y
        = 0;
      pDev->streamBuffers[i].bufferHeader.src.Rect.width
        = pDev->bufferFormat.fmt.pix.width;
      pDev->streamBuffers[i].bufferHeader.src.Rect.height
        = pDev->bufferFormat.fmt.pix.height;
      pDev->streamBuffers[i].bufferHeader.src.ulFlags
        = ((pDev->bufferFormat.fmt.pix.colorspace == V4L2_COLORSPACE_REC709)
           ? STM_PLANE_SRC_COLORSPACE_709
           : 0);

      pDev->streamBuffers[i].bufferHeader.src.ulClutBusAddress
        = pDev->streamBuffers[i].clut_phys;

      /*
       * Specify that the buffer is persistent, this allows us to pause video by
       * simply not queuing any more buffers and the last frame stays on screen.
       */
      pDev->streamBuffers[i].bufferHeader.info.ulFlags
        = STM_PLANE_PRESENTATION_PERSISTENT;
      pDev->streamBuffers[i].bufferHeader.info.pDisplayCallback
        = &timestampCallback;
      pDev->streamBuffers[i].bufferHeader.info.pCompletedCallback
        = &bufferFinishedCallback;
      pDev->streamBuffers[i].bufferHeader.info.pUserData
        = &(pDev->streamBuffers[i]);
    }
    else
    {
      debug_msg ("%s: Maximum number of buffers allocated.\n", __FUNCTION__);

      // We've allocated as many buffers as we can
      req->count = i;
      break;
    }
  }

  return req->count;
}


int
stmvout_allocate_clut (struct _stvout_device *device)
{
  struct bpa2_part * const part = bpa2_find_part ("bigphysarea");
  unsigned long     nPages, size;

  if (device->userClut_phys == 0)
    BUG_ON (device->userClut_virt != NULL);
  if (device->userClut_phys != 0)
    BUG_ON (device->userClut_virt == NULL);

  if (device->userClut_phys != 0)
    return 0;

  if (!part)
    return -ENODEV;

  size = CLUT_SIZE * MAX_USER_BUFFERS;
  nPages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  device->userClut_phys = bpa2_alloc_pages (part, nPages, 1, GFP_KERNEL);
  if (!device->userClut_phys) {
    info_msg ("bigphysarea: out of memory\n");
    return -ENOMEM;
  }

  /* CLUT may not cross a 64MB boundary */
#define PAGE64MB_SIZE  (1<<26)
  if ((device->userClut_phys & ~(PAGE64MB_SIZE - 1))
      != ((device->userClut_phys + size) & ~(PAGE64MB_SIZE - 1)))
  {
    printk ("%s: %8.lx + %lu (%lu pages) crosses a 64MB boundary, retrying!\n",
	    __FUNCTION__, device->userClut_phys, size, nPages);

    /* FIXME: this is a hack until the point we get an updated BPA2 driver.
       Will be removed again! */
    /* free and immediately reserve, hoping nobody requests bpa2 memory in
       between... */
    bpa2_free_pages (part, device->userClut_phys);
    device->userClut_phys = bpa2_alloc_pages (part, nPages,
                                              PAGE64MB_SIZE / PAGE_SIZE,
                                              GFP_KERNEL);
    printk ("%s: new start: %.8lx\n", __FUNCTION__, device->userClut_phys);
  }

  if ((device->userClut_phys & ~(PAGE64MB_SIZE - 1))
      != ((device->userClut_phys + size) & ~(PAGE64MB_SIZE - 1)))
  {
    /* can only happen, if somebody else requested bpa2 memory while we were
       busy here */
    printk ("%s: %8.lx + %lu (%lu pages) crosses a 64MB boundary again! "
            "Ignoring this time...\n",
	    __FUNCTION__, device->userClut_phys, size, nPages);
  }

  device->userClut_virt = ioremap_nocache (device->userClut_phys, size);

  return 0;
}

void
stmvout_deallocate_clut (struct _stvout_device *device)
{
  if (device->userClut_phys) {
    struct bpa2_part * const part = bpa2_find_part ("bigphysarea");

    iounmap (device->userClut_virt);
    bpa2_free_pages (part, device->userClut_phys);

    device->userClut_phys = 0;
    device->userClut_virt = NULL;
  }
}

/******************************************************************************
 *
 */
int
stmvout_deallocate_mmap_buffers (stvout_device_t *pDev)
{
  int i;

  debug_msg("%s\n",__PRETTY_FUNCTION__);

  for(i = 0; i < pDev->n_streamBuffers; i++)
  {
    debug_msg("%s: system %d (%p): phys/count: %8.lx/%d\n",
              __PRETTY_FUNCTION__,
              i, &pDev->streamBuffers[i],
              pDev->streamBuffers[i].physicalAddr,
              pDev->streamBuffers[i].mapCount);

    if (pDev->streamBuffers[i].physicalAddr != 0
        && (pDev->streamBuffers[i].mapCount > 0))
      return 0;
  }

  for(i = 0; i < pDev->n_streamBuffers; i++)
  {
    if (pDev->streamBuffers[i].physicalAddr != 0)
    {
      debug_msg("%s: de-allocating system buffer %d.\n",__FUNCTION__, i);

      freeBuffer(&pDev->streamBuffers[i]);
    }
  }

  for(i = 0; i < MAX_USER_BUFFERS; ++i)
  {
    if (pDev->userBuffers[i].isAllocated)
    {
      debug_msg("%s: de-allocating user buffer %d.\n",__FUNCTION__, i);

      pDev->userBuffers[i].isAllocated                        = 0;
      pDev->userBuffers[i].vidBuf.m.offset                    = 0xFFFFFFFF;
      pDev->userBuffers[i].vidBuf.flags                       = 0;
      pDev->userBuffers[i].bufferHeader.src.ulVideoBufferAddr = 0;
      pDev->userBuffers[i].bufferHeader.src.ulVideoBufferSize = 0;
    }
  }

  vfree (pDev->streamBuffers);
  pDev->streamBuffers = NULL;
  pDev->n_streamBuffers = 0;

  debug_msg("%s\n",__PRETTY_FUNCTION__);
  return 1;
}


static unsigned long
getPhysicalContiguous (unsigned long ptr,
                       size_t        size)
{
  struct mm_struct *mm = current->mm;
  unsigned virt_base =  (ptr / PAGE_SIZE) * PAGE_SIZE;
  unsigned phys_base = 0;

  pgd_t *pgd;
  pmd_t *pmd;
  pte_t *ptep, pte;

  debug_msg ("User pointer = 0x%.8lx\n", ptr);

  spin_lock (&mm->page_table_lock);

  pgd = pgd_offset(mm, virt_base);
  if (pgd_none (*pgd) || pgd_bad (*pgd))
    goto out;

  pmd = pmd_offset ((pud_t*) pgd, virt_base);
  if (pmd_none (*pmd) || pmd_bad (*pmd))
    goto out;

  ptep = pte_offset_map (pmd, virt_base);
  if (!ptep)
    goto out;

  pte = *ptep;

  if (pte_present (pte)) {
    phys_base = __pa (page_address (pte_page (pte)));
  }

  if (!phys_base)
    goto out;

  spin_unlock (&mm->page_table_lock);
  return phys_base + (ptr - virt_base);

out:
  spin_unlock (&mm->page_table_lock);
  return 0;
}


/******************************************************************************
 *
 */
int
stmvout_queue_buffer (stvout_device_t    *pDev,
                      struct v4l2_buffer *pVidBuf)
{
  const stm_mode_line_t* pCurrentMode;
  stm_plane_caps_t       planeCaps;
  streaming_buffer_t    *pBuf;
  unsigned long          standard;
  stm_display_buffer_t  * __restrict hdr;

  if (pVidBuf->memory == V4L2_MEMORY_USERPTR)
  {
    int i;
    for (pBuf = NULL, i = 0; i < MAX_USER_BUFFERS; ++i)
    {
      if ((pDev->userBuffers[i].vidBuf.flags & V4L2_BUF_FLAG_QUEUED) == 0)
      {
        unsigned long phys = getPhysicalContiguous (pVidBuf->m.userptr, 1);

        if (unlikely (!phys))
        {
          err_msg("User space pointer is a bit wonky\n");
          return -EINVAL;
        }

        debug_msg ("Found the physical pointer 0x%.8lx\n", phys);

        pBuf = &pDev->userBuffers[i];
        pBuf->isAllocated = 1;

        pBuf->pDev = pDev;
        pBuf->vidBuf = *pVidBuf;
        pBuf->vidBuf.m.offset = phys;

        hdr = &pBuf->bufferHeader;
        hdr->src.ulVideoBufferAddr = phys;
        hdr->src.ulVideoBufferSize = pVidBuf->length;

        /* This is only applicable for YUV buffers with separate luma
           and chroma */
        hdr->src.chromaBufferOffset = pDev->bufferFormat.fmt.pix.priv;

        info_msg ("%x %d\n",
                  pDev->planeFormat,g_capfmt[pDev->planeFormat].depth);
        hdr->src.ulColorFmt = pDev->planeFormat;
        hdr->src.ulPixelDepth = g_capfmt[pDev->planeFormat].depth;

        info_msg ("%d bytes, %d lines\n",
                  pDev->bufferFormat.fmt.pix.bytesperline,
                  pDev->bufferFormat.fmt.pix.height);
        hdr->src.ulStride = pDev->bufferFormat.fmt.pix.bytesperline;
        hdr->src.ulTotalLines = pDev->bufferFormat.fmt.pix.height;

        hdr->src.ulFlags
          = ((pDev->bufferFormat.fmt.pix.colorspace == V4L2_COLORSPACE_REC709)
             ? STM_PLANE_SRC_COLORSPACE_709
             : 0);

        /* Specify that the buffer is persistent, this allows us to pause
           video by simply not queuing any more buffers and the last frame
           stays on screen.*/
        hdr->info.ulFlags = STM_PLANE_PRESENTATION_PERSISTENT;

        hdr->info.pDisplayCallback = &timestampCallback;
        hdr->info.pCompletedCallback = &bufferFinishedCallback;
        hdr->info.pUserData = pBuf;

        info_msg ("clut = %p + %x\n", pDev->userClut_virt, (CLUT_SIZE * i));
        pBuf->clut_virt = pDev->userClut_virt + (CLUT_SIZE * i);

        hdr->src.ulClutBusAddress = pDev->userClut_phys + (CLUT_SIZE * i);
        break;
      }
    }

    if (!pBuf)
    {
      err_msg ("%s: too many queued user buffers, please dequeue one first\n",
               __FUNCTION__);
      return -EIO;
    }

    /* FIXME: cache writeback needed?? */
  }
  else // if (pVidBuf->memory == V4L2_MEMORY_MMAP)
  {
    if (pVidBuf->index >= pDev->n_streamBuffers)
    {
      err_msg ("%s: buffer index %d is out of range\n",
               __FUNCTION__, pVidBuf->index);
      return -EINVAL;
    }

    pBuf = &pDev->streamBuffers[pVidBuf->index];

    if (pBuf->vidBuf.flags & V4L2_BUF_FLAG_QUEUED)
    {
      err_msg ("%s: buffer %d is already queued\n",
               __FUNCTION__, pVidBuf->index);
      return -EIO;
    }

    hdr = &pBuf->bufferHeader;
    dma_cache_wback (pBuf->cpuAddr, pBuf->vidBuf.length);
  }

  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
    return -EIO;

  if(stm_display_output_get_current_tv_standard(pDev->pOutput, &standard)<0)
    return -ERESTARTSYS;

  if(stm_display_plane_get_capabilities(pDev->pPlane, &planeCaps)<0)
    return -ERESTARTSYS;

  if (!(planeCaps.ulCaps & PLANE_CAPS_RESIZE))
  {
    /*
     * We need to check this here because the buffer format and output
     * window size can change independently, only at the point of queuing
     * the buffer can we decide if the configuration is valid, which
     * isn't ideal.
     */
    if(pDev->srcCrop.c.width != pDev->outputCrop.c.width)
    {
      debug_msg("%s: failed, HW doesn't support resizing\n", __FUNCTION__);
      return -EINVAL;
    }

    if(pDev->srcCrop.c.height != pDev->outputCrop.c.height)
    {
      debug_msg("%s: failed, HW doesn't support resizing\n", __FUNCTION__);
      return -EINVAL;
    }
  }

  hdr->src.Rect.x      = pDev->srcCrop.c.left;
  hdr->src.Rect.y      = pDev->srcCrop.c.top;
  hdr->src.Rect.width  = pDev->srcCrop.c.width;
  hdr->src.Rect.height = pDev->srcCrop.c.height;

  hdr->dst.Rect.x      = pDev->outputCrop.c.left;
  hdr->dst.Rect.y      = pDev->outputCrop.c.top;
  hdr->dst.Rect.width  = pDev->outputCrop.c.width;
  hdr->dst.Rect.height = pDev->outputCrop.c.height;

  /* For V4L2_MEMORY_USERPTR buffers the CLUT must be specified in the
     'reserved' field of the buffer structure.

     V4L2_MEMORY_MMAP buffers contain the CLUT as part of the actual buffer,
     so in case the application was intelligent enough to write the CLUT
     directly, we don't have to do anything. In that case, 'reserved' shall be
     NULL. This is new behaviour and old applications don't know about this
     possibility.
     Therefore, we also allow a CLUT to be specified in 'reserved' for
     V4L2_MEMORY_MMAP buffers. In that case, we will have to copy the CLUT
     from userspace to the expected address (same as with V4L2_MEMORY_USERPTR
     buffers).
     The API addition has the advantage of avoiding a copy, obviously. */
  switch (pDev->bufferFormat.fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_CLUTA8:
    case V4L2_PIX_FMT_CLUT8:
    case V4L2_PIX_FMT_CLUT4:
    case V4L2_PIX_FMT_CLUT2:
      if (!pVidBuf->reserved)
        printk (KERN_WARNING "%s: no CLUT given for %s but we need one for LUT "
                             "buffers!\n",
                __FUNCTION__,
                (pVidBuf->memory == V4L2_MEMORY_USERPTR) ? "USER" : "MMAP");
      break;

    default:
      if (pVidBuf->reserved)
        printk (KERN_WARNING "%s: CLUT given for non-LUT surface!\n",
                __FUNCTION__);
      break;
    }

  if (pVidBuf->reserved) {
    int clutSize = 0;
    u32 color;
    int i;

    switch (pDev->bufferFormat.fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_CLUTA8:
    case V4L2_PIX_FMT_CLUT8:
      clutSize += (256 - 16);
      /* fall through */

    case V4L2_PIX_FMT_CLUT4:
      clutSize += (16 - 4);
      /* fall through */

    case V4L2_PIX_FMT_CLUT2:
      clutSize += 4;
      if (access_ok (VERIFY_READ, (u8 *) pVidBuf->reserved, (clutSize * 4))) {
        for (i = 0; i < clutSize; ++i) {
          __get_user (color, &((u32 *) pVidBuf->reserved)[i]);
          ((u32 *) pBuf->clut_virt)[i] = (0
                                          | (color & 0x00ffffff)
                                          | ((((color >> 24) + 1) / 2) << 24)
                                          );
        }
      }
      /* V4L2_MEMORY_MMAP have the CLUT ioremap()ed cached, need to flush
         the cache...
         V4L2_MEMORY_USERPTR have a separate (uncached) clut associated with
         them. */
      if (pVidBuf->memory == V4L2_MEMORY_MMAP) {
#if 0
        /* for now this doesn't work yet... */
        static int once = 0;
        if (unlikely (!once)) {
          once = 1;
          printk ("%s: you should convert your CLUT code to the NEW API\n",
                  __FUNCTION__);
        }
#endif
        dma_cache_wback (pBuf->clut_virt, clutSize * 4);
      }
      break;

    default:
      break;
    }
  }

  if (pDev->state.ckey.flags & SCKCF_ENABLE
      && pDev->state.ckey.enable & VCSCEF_ACTIVATE_BUFFER) {
    hdr->src.ColorKey = pDev->state.ckey;
    hdr->src.ColorKey.enable &= ~VCSCEF_ACTIVATE_BUFFER;
  } else
    hdr->src.ColorKey.flags = SCKCF_NONE;

  if(pDev->globalAlpha < 255)
  {
    hdr->src.ulConstAlpha = pDev->globalAlpha;
    hdr->src.ulFlags |= STM_PLANE_SRC_CONST_ALPHA;
  }
  else
  {
    hdr->src.ulFlags &= ~STM_PLANE_SRC_CONST_ALPHA;
  }

  if(pDev->bufferFormat.fmt.pix.field != V4L2_FIELD_ANY &&
     pVidBuf->field != pDev->bufferFormat.fmt.pix.field)
  {
    err_msg("%s: field option does not match buffer format\n", __FUNCTION__);
    return -EINVAL;
  }

  pBuf->vidBuf.field = pVidBuf->field;

  switch(pBuf->vidBuf.field)
  {
    case V4L2_FIELD_NONE:
      hdr->info.nfields = 1;
      hdr->src.ulFlags &= ~STM_PLANE_SRC_INTERLACED;
      break;

    case V4L2_FIELD_INTERLACED:
      hdr->info.nfields = 2;
      hdr->src.ulFlags |= STM_PLANE_SRC_INTERLACED;
      /*
       * It is slightly odd to determine the output field order by the
       * display standard, but that is what the API says to do. In reality
       * the source order should be specified by the next two cases below.
       */
      if(standard & (STM_OUTPUT_STD_NTSC_M
                     | STM_OUTPUT_STD_NTSC_J
                     | STM_OUTPUT_STD_NTSC_443))
        hdr->src.ulFlags |= STM_PLANE_SRC_BOTTOM_FIELD_FIRST;

      break;
    case V4L2_FIELD_INTERLACED_TB:
      hdr->info.nfields = 2;
      hdr->src.ulFlags |= STM_PLANE_SRC_INTERLACED;
      break;
    case V4L2_FIELD_INTERLACED_BT:
      hdr->info.nfields = 2;
      hdr->src.ulFlags |= (STM_PLANE_SRC_INTERLACED
                           | STM_PLANE_SRC_BOTTOM_FIELD_FIRST);
      break;
    default:
      err_msg("%s: field option not supported\n", __FUNCTION__);
      return -EINVAL;
  }

  if((hdr->src.ulFlags & STM_PLANE_SRC_INTERLACED) &&
     (pCurrentMode->ModeParams.ScanType           == SCAN_P)     &&
     (planeCaps.ulCaps & PLANE_CAPS_DEINTERLACE) == 0)
  {
    err_msg ("%s: de-interlaced content on a progressive display not "
             "supported\n", __FUNCTION__);
    return -EINVAL;
  }

  hdr->info.presentationTime = (((TIME64) pVidBuf->timestamp.tv_sec
                                 * USEC_PER_SEC)
                                + (TIME64) pVidBuf->timestamp.tv_usec);

  if(pVidBuf->flags & V4L2_BUF_FLAG_REPEAT_FIRST_FIELD)
  {
    pBuf->vidBuf.flags |= V4L2_BUF_FLAG_REPEAT_FIRST_FIELD;
    hdr->src.ulFlags   |= STM_PLANE_SRC_REPEAT_FIRST_FIELD;
  }
  else
  {
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_REPEAT_FIRST_FIELD;
    hdr->src.ulFlags   &= ~STM_PLANE_SRC_REPEAT_FIRST_FIELD;
  }

  if(pVidBuf->flags & V4L2_BUF_FLAG_INTERPOLATE_FIELDS)
  {
    pBuf->vidBuf.flags |= V4L2_BUF_FLAG_INTERPOLATE_FIELDS;
    hdr->src.ulFlags   |= STM_PLANE_SRC_INTERPOLATE_FIELDS;
  }
  else
  {
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_INTERPOLATE_FIELDS;
    hdr->src.ulFlags   &= ~STM_PLANE_SRC_INTERPOLATE_FIELDS;
  }

  if(pVidBuf->flags & V4L2_BUF_FLAG_NON_PREMULTIPLIED_ALPHA)
  {
    /*
     * Note: this V4L2 flag is the reverse of the core driver flag so the
     * default is premultiplied.
     */
    pBuf->vidBuf.flags |= V4L2_BUF_FLAG_NON_PREMULTIPLIED_ALPHA;
    hdr->src.ulFlags   &= ~STM_PLANE_SRC_PREMULTIPLIED_ALPHA;
  }
  else
  {
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_NON_PREMULTIPLIED_ALPHA;
    hdr->src.ulFlags   |= STM_PLANE_SRC_PREMULTIPLIED_ALPHA;
  }

  if(pVidBuf->flags & V4L2_BUF_FLAG_RESCALE_COLOUR_TO_VIDEO_RANGE)
  {
    /*
     * Note: this flag is on the destination description flags not the source
     * unlike all the other flags set above.
     */
    pBuf->vidBuf.flags |= V4L2_BUF_FLAG_RESCALE_COLOUR_TO_VIDEO_RANGE;
    hdr->dst.ulFlags   |= STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE;
  }
  else
  {
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_RESCALE_COLOUR_TO_VIDEO_RANGE;
    hdr->dst.ulFlags   &= ~STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE;
  }

  if(pVidBuf->flags & V4L2_BUF_FLAG_GRAPHICS)
  {
    pBuf->vidBuf.flags |= V4L2_BUF_FLAG_GRAPHICS;
    hdr->info.ulFlags  |= STM_PLANE_PRESENTATION_GRAPHICS;
  }
  else
  {
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_GRAPHICS;
    hdr->info.ulFlags  &= ~STM_PLANE_PRESENTATION_GRAPHICS;
  }

  pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_DONE;
  pBuf->vidBuf.flags |= V4L2_BUF_FLAG_QUEUED;

  /*
   * The API spec states that the flags are reflected in the structure the
   * user passed in, but nothing else is.
   */
  pVidBuf->flags = pBuf->vidBuf.flags;

  {
  unsigned long flags;

  BUG_ON (!list_empty (&pBuf->node));
  write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  list_add_tail (&pBuf->node, &pDev->pendingStreamQ.list);
  write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);
  }

  if (pDev->isStreaming)
  {
    stmvout_send_next_buffer_to_display(pDev);
  }

  return 0;
}


/******************************************************************************
 *
 */
int
stmvout_dequeue_buffer (stvout_device_t    *pDev,
                        struct v4l2_buffer *pVidBuf)
{
  streaming_buffer_t *pBuf;
  unsigned long       flags;

  write_lock_irqsave (&pDev->completeStreamQ.lock, flags);
  list_for_each_entry (pBuf, &pDev->completeStreamQ.list, node)
  {
    list_del_init (&pBuf->node);
    write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

    pBuf->vidBuf.flags &= ~(V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE);

    *pVidBuf = pBuf->vidBuf;

    return 1;
  }
  write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

  return 0;
}


/******************************************************************************
 *  stmvout_set_buffer_format - ioctl hepler function for S_FMT
 */
int
stmvout_set_buffer_format (stvout_device_t    *pDev,
                           struct v4l2_format *fmt,
                           int                 updateConfig)
{
  const SURF_FMT *formats;
  int num_formats;
  int index;
  int minbytesperline;

  stm_plane_caps_t       planeCaps;
  const stm_mode_line_t* pCurrentMode;

  if(stm_display_plane_get_capabilities(pDev->pPlane, &planeCaps)<0)
    return -ERESTARTSYS;


  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
  {
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  num_formats = stm_display_plane_get_image_formats(pDev->pPlane, &formats);
  if(signal_pending(current))
    return -ERESTARTSYS;

  /*
   * Setting buffer format should not fail when a requested format isn't
   * supported, but fall back to the closest the hardware can provide. The fmt
   * structure is both input and output (unlike in S_CROP) so must reflect the
   * actual format selected once we have finished.
   */
  switch(fmt->fmt.pix.field)
  {
    case V4L2_FIELD_ANY:
    case V4L2_FIELD_NONE:
      break;
    case V4L2_FIELD_INTERLACED:
    case V4L2_FIELD_INTERLACED_TB:
    case V4L2_FIELD_INTERLACED_BT:
    {
      if(pCurrentMode->ModeParams.ScanType == SCAN_P &&
         (planeCaps.ulCaps & PLANE_CAPS_DEINTERLACE) == 0)
      {
        err_msg ("%s: de-interlaced content on a progressive display not "
                 "supported on this plane", __FUNCTION__);
        fmt->fmt.pix.field = V4L2_FIELD_NONE;
      }
      break;
    }
    default:
    {
      fmt->fmt.pix.field = V4L2_FIELD_ANY;
      break;
    }
  }

  switch(fmt->fmt.pix.pixelformat)
  {
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_STM420MB:
    case V4L2_PIX_FMT_STM422MB:
      /*
       * The plane converts YUV to 10bit/component RGB internally for
       * mixing with the rest of the display, the mixed RGB output gets
       * converted back to the correct YUV colourspace for the display mode
       * automatically.
       * A source YUV buffer to be displayed on a plane can be specified as
       * being in either 601 (SMPTE170M) or 709; this specifies the maths used
       * for the internal conversion to RGB.
       */
      if(fmt->fmt.pix.colorspace != V4L2_COLORSPACE_SMPTE170M &&
         fmt->fmt.pix.colorspace != V4L2_COLORSPACE_REC709)
      {
      	/*
      	 * Pick a default based on the display standard. SD = 601, HD = 709
      	 */
        if((pCurrentMode->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK))
          fmt->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
        else
          fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
      }
      break;
    default:
      /*
       * For RGB surface formats sRGB content is assumed.
       */
      fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
      break;
  }

  for (index = 0; index < num_formats; index++)
  {
    if (g_capfmt[formats[index]].fmt.pixelformat == fmt->fmt.pix.pixelformat)
      break;
  }

  if (index == num_formats)
  {
      /*
       * If the plane doesn't support the requested format
       * pick the first format it can support.
       */
      index = 0;
      fmt->fmt.pix.pixelformat = g_capfmt[formats[index]].fmt.pixelformat;
  }

  /*
   * WARNING: Do not change "index" after this point as it is used at the end of
   *          the function - which by definition is way too long if it needs
   *          this comment.
   */

  /*
   * For interlaced buffers we need an even number of lines.
   * We will round up and try and provide more than the number of
   * lines requested, assuming this doesn't go over the maximum allowed.
   */
  if (fmt->fmt.pix.field != V4L2_FIELD_NONE)
  {
    fmt->fmt.pix.height += fmt->fmt.pix.height%2;
  }

  switch(fmt->fmt.pix.pixelformat)
  {
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_YUYV:
      /*
       * 422 Raster (interleaved luma and chroma), we need an even number
       * of luma samples (pixels) per line
       */
      fmt->fmt.pix.width  += fmt->fmt.pix.width % 2;
      break;
    case V4L2_PIX_FMT_STM420MB:
    case V4L2_PIX_FMT_STM422MB:
      /*
       * For planar macroblock formats we round to 1 macroblock (16pixels/lines
       * per macroblock). The actual physical size of the memory buffer has to
       * be larger as there is wastage due to the macroblock memory
       * organization, but that is dealt with below.
       */
      fmt->fmt.pix.width  = (fmt->fmt.pix.width  + 15) & ~15;
      fmt->fmt.pix.height = (fmt->fmt.pix.height + 15) & ~15;
      break;
  }

  /*
   * If the plane caps came back with 0 for the maximum width
   * or height then fallback to using the display mode values.
   */
  if(planeCaps.ulMaxWidth == 0)
    planeCaps.ulMaxWidth = pCurrentMode->TimingParams.PixelsPerLine;

  if(planeCaps.ulMaxHeight == 0)
    planeCaps.ulMaxHeight = pCurrentMode->ModeParams.ActiveAreaHeight;

  if (fmt->fmt.pix.width > planeCaps.ulMaxWidth)
    fmt->fmt.pix.width  = planeCaps.ulMaxWidth;

  if (fmt->fmt.pix.width < planeCaps.ulMinWidth)
    fmt->fmt.pix.width  = planeCaps.ulMinWidth;

  if (fmt->fmt.pix.height > planeCaps.ulMaxHeight)
    fmt->fmt.pix.height = planeCaps.ulMaxHeight;

  if (fmt->fmt.pix.height < planeCaps.ulMinHeight)
    fmt->fmt.pix.height = planeCaps.ulMinHeight;

  /*
   * We only honour the bytesperline set by the application for raster formats.
   * If the user specified value is less than the minimum required for the
   * requested width/height/format then we override the user value.
   *
   * For the ST planar macroblock formats we _always_ set this to the "virtual"
   * stride of the luma buffer, which is used to present the buffer correctly.
   */
  minbytesperline  = fmt->fmt.pix.width * (g_capfmt[formats[index]].depth >> 3);

  if (fmt->fmt.pix.bytesperline < minbytesperline        ||
      fmt->fmt.pix.pixelformat  == V4L2_PIX_FMT_STM420MB ||
      fmt->fmt.pix.pixelformat  == V4L2_PIX_FMT_STM422MB)
  {
    fmt->fmt.pix.bytesperline = minbytesperline;
  }

  /*
   * Now we have to sort out stride alignments for raster formats
   */
  switch(fmt->fmt.pix.pixelformat)
  {
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_YUYV:
      /*
       * Some hardware can read 422R in 32pixel chunks, which is 64bytes,
       * to get maximum efficiency on the memory interfaces; so we have to
       * align the stride to this.
       *
       * TODO: add this information to the plane's capabilities.
       */
      fmt->fmt.pix.bytesperline = (fmt->fmt.pix.bytesperline + 63) & ~63;
      break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YUV422P:
      /*
       * Hardware supporting these formats reads 8pixel chunks.
       */
      fmt->fmt.pix.bytesperline = (fmt->fmt.pix.bytesperline + 7) & ~7;
      break;
    case V4L2_PIX_FMT_STM420MB:
    case V4L2_PIX_FMT_STM422MB:
      break;
  }

  /*
   * Work out the physical size of the buffer, for normal formats it is simply
   * the stride * height. However for the two ST planar macroblock formats
   * we need to add the size of the chroma buffer to this.
   */
  switch(fmt->fmt.pix.pixelformat)
  {
    case V4L2_PIX_FMT_STM422MB:
    case V4L2_PIX_FMT_STM420MB:
    {
      int lumasize;
      int chromasize;
      int mbwidth    = fmt->fmt.pix.width  / 16;
      int mbheight   = fmt->fmt.pix.height / 16;
      /*
       * These buffer size calculations are taken directly from the datasheet.
       * However if we use them with an odd mbwidth (e.g. 720pixels->45MBs)
       * then we get video corruption when using the hardware MPEG decoder to
       * fill the buffers. So, although we don't understand why, we up the
       * size of the luma buffer by ensuring mbwidth is even. Sigh...
       */
      mbwidth += mbwidth % 2;

      lumasize   = mbwidth * ((mbheight+1)/2) * 512;
      chromasize = ((mbwidth+1)/2) * ((mbheight+1)/2) * 512;

      if(fmt->fmt.pix.pixelformat == V4L2_PIX_FMT_STM422MB)
        chromasize *= 2;

      fmt->fmt.pix.sizeimage = lumasize+chromasize;

      /*
       * This specifies the start offset in bytes of the Chroma from the
       * beginning of the buffer in planar macroblock YUV formats.
       */
      fmt->fmt.pix.priv = lumasize;

      break;
    }
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    {
      /*
       * 420 with separate luma and chroma buffers, the format is defined such
       * that the size of 2 chroma lines is the same as one luma line including
       * padding.
       */
      int lumasize = fmt->fmt.pix.bytesperline * fmt->fmt.pix.height;

      /*
       * Ensure the chroma buffers start on an 8 byte boundary
       */
      lumasize = (lumasize + 7) & ~7;

      fmt->fmt.pix.priv = lumasize;

      fmt->fmt.pix.sizeimage = lumasize + (lumasize/2);
      break;

    }
    case V4L2_PIX_FMT_YUV422P:
    {
      /*
       * 422 with separate luma and chroma buffers, the format is defined such
       * that the size of 2 chroma lines is the same as one luma line including
       * padding.
       */
      int lumasize = fmt->fmt.pix.bytesperline * fmt->fmt.pix.height;

      /*
       * Ensure the chroma buffers start on an 8 byte boundary
       */
      lumasize = (lumasize + 7) & ~7;

      fmt->fmt.pix.priv = lumasize;
      /*
       * The chroma buffers combined are the same size as the luma buffer in 422
       */
      fmt->fmt.pix.sizeimage = lumasize * 2;
      break;
    }
    default:
    {
      fmt->fmt.pix.sizeimage = fmt->fmt.pix.bytesperline * fmt->fmt.pix.height;
      break;
    }
  }


  /*
   * Change the driver state if requested by the caller.
   */
  if(updateConfig)
  {
    pDev->planeFormat  = formats[index];
    pDev->bufferFormat = *fmt;

    /*
     * Set the source crop to default to the whole buffer
     */
    pDev->srcCrop.type     = V4L2_BUF_TYPE_PRIVATE;
    pDev->srcCrop.c.top    = 0;
    pDev->srcCrop.c.left   = 0;
    pDev->srcCrop.c.width  = fmt->fmt.pix.width;
    pDev->srcCrop.c.height = fmt->fmt.pix.height;
    stmvout_set_buffer_crop(pDev, &pDev->srcCrop);
  }

  return 0;
}


/******************************************************************************
 *  stmvout_set_output_crop - ioctl helper function for S_CROP
 */
int
stmvout_set_output_crop (stvout_device_t  *pDev,
                         struct v4l2_crop *pCrop)
{
  const stm_mode_line_t* pCurrentMode;
  stm_plane_caps_t       planeCaps;
  struct v4l2_crop       origCrop;

  debug_msg ("%s: (%ld,%ld-%ldx%ld)\n",
             __FUNCTION__,
             (long) pCrop->c.left, (long) pCrop->c.top,
             (long) pCrop->c.width, (long) pCrop->c.height);

  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
  {
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  if(stm_display_plane_get_capabilities(pDev->pPlane, &planeCaps)<0)
    return -ERESTARTSYS;

  origCrop = *pCrop;

  /*
   * Make sure the width and height can be contained in the video active area
   * of the current display mode, adjusting them down while maintaining the
   * original requested aspect ratio.
   *
   * Note: that this does not produce an error if the application request
   * is outside the bounds of the display. The user code should query the actual
   * crop selected by doing a G_CROP IOCTL; it can use this to continue to
   * negotiate a mutually suitable output window with the driver.
   */
  if(pCrop->c.width > pCurrentMode->ModeParams.ActiveAreaWidth)
  {
    pCrop->c.width  = pCurrentMode->ModeParams.ActiveAreaWidth;
    pCrop->c.height = (pCrop->c.width * origCrop.c.height) / origCrop.c.width;
  }

  if(pCrop->c.height > pCurrentMode->ModeParams.ActiveAreaHeight)
  {
    pCrop->c.height = pCurrentMode->ModeParams.ActiveAreaHeight;
    pCrop->c.width = (pCrop->c.height * origCrop.c.width) / origCrop.c.height;
  }

  if(pCurrentMode->ModeParams.ScanType == SCAN_I)
  {
    /*
     * Round the height up to a multiple of two lines if needed, don't bother
     * to readjust the width, the aspect ratio will be close enough.
     */
    pCrop->c.height += pCrop->c.height%2;

    /*
     * Make sure top of the output is on the top field on an interlaced display,
     * we do this by shifting one line up if necessary.
     *
     * Note that the display driver translates (0,0) to the correct first active
     * top field line and pixel based on the current display mode; this code
     * doesn't need to deal with blanking lines etc..
     */
    pCrop->c.top -= pCrop->c.top%2;
  }

  /*
   * Now constrain the crop to within the screen bounds.
   */
  if (pCrop->c.left < 0)
    pCrop->c.left = 0; /* v4l2_rect structure contains signed values */

  if (pCrop->c.top < 0)
    pCrop->c.top = 0;

  if ((pCrop->c.left + pCrop->c.width)
      > pCurrentMode->ModeParams.ActiveAreaWidth)
  {
    pCrop->c.left = pCurrentMode->ModeParams.ActiveAreaWidth - pCrop->c.width;
  }

  if ((pCrop->c.top + pCrop->c.height)
      > pCurrentMode->ModeParams.ActiveAreaHeight)
  {
    pCrop->c.top = pCurrentMode->ModeParams.ActiveAreaHeight - pCrop->c.height;
  }

  pDev->outputCrop = *pCrop;

  return 0;
}


/******************************************************************************
 *  stmvout_set_buffer_crop - ioctl hepler function for S_CROP
 */
int
stmvout_set_buffer_crop (stvout_device_t  *pDev,
                         struct v4l2_crop *pCrop)
{
  struct v4l2_crop       origCrop = *pCrop;

  debug_msg ("%s: (%ld,%ld-%ldx%ld)\n",
             __FUNCTION__,
             (long) pCrop->c.left, (long) pCrop->c.top,
             (long) pCrop->c.width, (long) pCrop->c.height);

  /*
   * Make sure the width and height can be contained in the buffer rounding
   * them down while maintaining the original requested aspect ratio.
   *
   * As with setting output crop the application should use G_CROP to
   * find out the result and try S_CROP again if it wants to adjust the
   * selction futher.
   */
  if(pCrop->c.width > pDev->bufferFormat.fmt.pix.width)
  {
    pCrop->c.width  = pDev->bufferFormat.fmt.pix.width;
    pCrop->c.height = (pCrop->c.width * origCrop.c.height) / origCrop.c.width;
  }

  if(pCrop->c.height > pDev->bufferFormat.fmt.pix.height)
  {
    pCrop->c.height = pDev->bufferFormat.fmt.pix.height;
    pCrop->c.width  = (pCrop->c.height * origCrop.c.width) / origCrop.c.height;
  }

  if(pDev->bufferFormat.fmt.pix.field != V4L2_FIELD_NONE)
  {
    /*
     * Round the height up to a multiple of two lines for interlaced buffers
     */
    pCrop->c.height += pCrop->c.height%2;

    /*
     * Make sure the first line is the top field otherwise the fields will
     * be reversed on both interlaced and progressive displays.
     *
     */
    pCrop->c.top -= pCrop->c.top%2;
  }

  /*
   * Now constrain the crop to within the buffer extent.
   */
  if (pCrop->c.left < 0)
    pCrop->c.left = 0; /* v4l2_rect structure contains signed values */

  if (pCrop->c.top < 0)
    pCrop->c.top = 0;

  if ((pCrop->c.left + pCrop->c.width) > pDev->bufferFormat.fmt.pix.width)
  {
    pCrop->c.left = pDev->bufferFormat.fmt.pix.width - pCrop->c.width;
  }

  if ((pCrop->c.top + pCrop->c.height) > pDev->bufferFormat.fmt.pix.height)
  {
    pCrop->c.top = pDev->bufferFormat.fmt.pix.height - pCrop->c.height;
  }

  pDev->srcCrop = *pCrop;

  return 0;
}


/******************************************************************************
 *
 */
static void
delete_buffers_from_list (const struct list_head * const list)
{
  streaming_buffer_t *buffer, *tmp;

  list_for_each_entry_safe (buffer, tmp, list, node)
  {
    /* need to init the node as well, because the user might decide to start
       streaming again, without closing the device or calling VIDIOC_REQBUFS
       beforehand. In that case, we would not ever re init the list head,
       thus a BUG_ON() would trigger in stmvout_queue_buffer(). It's not
       correct to remove the BUG_ON(), though, because the node (and thus the
       list) will still have a bogus pointer! */
    list_del_init (&buffer->node);
    buffer->vidBuf.flags &= ~V4L2_BUF_FLAG_QUEUED;
    debug_msg ("%s: dequeueing/discarding buffer %d\n",
               __FUNCTION__, buffer->vidBuf.index);
  }
}

int
stmvout_streamoff (stvout_device_t *pDev)
{
  unsigned long flags;

  debug_msg("%s: pDev = %p\n", __FUNCTION__, pDev);

  if (pDev->isStreaming)
  {
    /* Discard the pending queue */
    debug_msg("%s: About to dequeue from pending queue\n", __FUNCTION__);
    write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
    delete_buffers_from_list (&pDev->pendingStreamQ.list);
    write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

    debug_msg("%s: About to flush plane = %p\n", __FUNCTION__, pDev->pPlane);

    /*
     * Unlock (and Flush) the display plane, this will cause all the completed
     * callbacks for each queued buffer to be called.
     */
    if(stm_display_plane_unlock(pDev->pPlane)<0)
      goto error;

    /*
     * Disconnect the plane from the current output, this allows us to
     * change output once we are no longer streaming.
     */
    if(stm_display_plane_disconnect_from_output(pDev->pPlane, pDev->pOutput)<0)
      goto error;

    /* dqueue all buffers from the complete queue */
    debug_msg("%s: About to discard completed queue\n", __FUNCTION__);
    write_lock_irqsave (&pDev->completeStreamQ.lock, flags);
    delete_buffers_from_list (&pDev->completeStreamQ.list);
    write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

    pDev->isStreaming=0;
    wake_up_interruptible(&(pDev->wqStreamingState));
  }

  debug_msg("%s\n", __FUNCTION__);
  return 0;

error:
  if(signal_pending(current))
    return -ERESTARTSYS;
  return -EIO;
}


/******************************************************************************
 *
 */
streaming_buffer_t *
stmvout_get_buffer_from_mmap_offset (stvout_device_t *pDev,
                                     unsigned long    offset)
{
  streaming_buffer_t *this;
  int	              i;

  debug_msg("%s: offset = 0x%08x\n",__FUNCTION__, (int)offset);

  for (i = 0, this = pDev->streamBuffers;
        i < pDev->n_streamBuffers;
        ++i, ++this)
  {
    if (offset >= this->vidBuf.m.offset &&
        offset <  (this->vidBuf.m.offset + this->vidBuf.length))
    {
      debug_msg("%s: buffer found. index = %d\n",__FUNCTION__, i);
      return this;
    }
  }

  debug_msg("%s: buffer not found\n",__FUNCTION__);

  return NULL;
}


int
stmvout_enum_buffer_format (stvout_device_t     *pDev,
                            struct v4l2_fmtdesc *f)
{
  int num_formats;
  const SURF_FMT *formats;
  __u32 index,count;

  num_formats = stm_display_plane_get_image_formats(pDev->pPlane, &formats);
  if(signal_pending(current))
    return -ERESTARTSYS;

  /*
   * Run through the surface formats available on the plane, enumerating
   * _only_ those that are supported by V4L2.
   */
  count = 0;
  for(index=0; index<num_formats; index++)
  {
    /* Is this a V4L2 supported format? */
    if(g_capfmt[formats[index]].fmt.pixelformat != 0)
      count++;

    /* Have we found the Nth format specified in the application request? */
    if(count == (f->index+1))
      break;
  }

  if (index >= num_formats)
    return -EINVAL;

  /* Copy the generic format descriptor */
  *f = g_capfmt[formats[index]].fmt;
  /* Fix up the index so it is the same as the input */
  f->index = count-1;

  return 0;
}
