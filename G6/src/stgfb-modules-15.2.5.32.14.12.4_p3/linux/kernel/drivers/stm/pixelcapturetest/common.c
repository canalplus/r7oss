/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/common.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>   /* Needed for kmalloc and kfree */

#include <linux/bpa2.h>
#include <linux/fb.h>
#include <linux/io.h>

/*
 * This test builds against the version of stmfb.h in this source tree, rather
 * than the one that is shipped as part of the kernel headers package for
 * consistency. Normal user applications should use <linux/stmfb.h>
 */
#include <stm_display.h>
#include <stmfb.h>

#include "common.h"

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
#include "pattern_yuv.h"
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */

#if defined(CONFIG_ARM)
/* ARM has mis-named this interface relative to all the other platforms that
   define it (including x86). */
#  define ioremap_cache ioremap_cached
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

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)  (((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)  ((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)

#ifndef min
#define min( a, b )                     (((a)<(b)) ? (a) : (b))
#endif
#ifndef max
#define max( a, b )                     (((a)>(b)) ? (a) : (b))
#endif

/*************************************************************/
/**                                                         **/
/**  Get Default Capture Input Windows size                 **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
int capture_get_default_window_rect(stm_pixel_capture_h pixel_capture,
                                          stm_pixel_capture_input_params_t input_params,
                                          stm_pixel_capture_rect_t *rect)
{
	stm_pixel_capture_input_window_capabilities_t input_window_capability;
	int ret;

  memset(rect, 0, sizeof(stm_pixel_capture_rect_t));

	ret =
	    stm_pixel_capture_get_input_window_capabilities(pixel_capture,
						&input_window_capability);
	if (ret) {
		printk(KERN_ERR "%s: failed to get the IO window capabilities(%d)\n",
			__func__, ret);
		goto end;
	}

	rect->x = rect->y = 0;
	rect->width = min(input_window_capability.max_input_window_area.width,
				input_params.active_window.width);
	rect->height = min(input_window_capability.max_input_window_area.height,
				input_params.active_window.height);

end:
	return ret;
}


/*************************************************************/
/**                                                         **/
/**  Push the captured buffer to the framebuffer display    **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
void capture_update_display(struct fb_info *info, stm_pixel_capture_buffer_descr_t *buffer_descr)
{
  int ret;
  struct stmfbio_planeinfo plane_info;
  unsigned long buffer_add;

  if (info->fbops->fb_ioctl)
  {
    long unsigned int p_info = (long unsigned int)&plane_info;
    mm_segment_t oldfs = get_fs();

    set_fs(KERNEL_DS);

    ret = info->fbops->fb_ioctl(info, STMFBIO_GET_PLANEMODE, p_info);
    if (ret < 0)
      printk("Get Plane Info failed err = %d!\n", ret);

    /* update to the new captured buffer */
    switch (buffer_descr->cap_format.format) {
      case STM_PIXEL_FORMAT_RGB565:
        plane_info.config.format   = SURF_RGB565;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->rgb_address;
        break;
      case STM_PIXEL_FORMAT_ARGB1555:
        plane_info.config.format   = SURF_ARGB1555;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->rgb_address;
        break;
      case STM_PIXEL_FORMAT_ARGB4444:
        plane_info.config.format   = SURF_ARGB4444;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->rgb_address;
        break;
      case STM_PIXEL_FORMAT_RGB888:
        plane_info.config.format   = SURF_RGB888;
        plane_info.config.bitdepth = 24;
        buffer_add = buffer_descr->rgb_address;
        break;
      case STM_PIXEL_FORMAT_ARGB8565:
        plane_info.config.format   = SURF_ARGB8565;
        plane_info.config.bitdepth = 24;
        buffer_add = buffer_descr->rgb_address;
        break;
      case STM_PIXEL_FORMAT_ARGB8888:
        plane_info.config.format   = SURF_ARGB8888;
        plane_info.config.bitdepth = 32;
        buffer_add = buffer_descr->rgb_address;
        break;

      case STM_PIXEL_FORMAT_YUV_NV12:
        plane_info.config.format   = SURF_YCBCR422MB;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->luma_address;
        break;
      case STM_PIXEL_FORMAT_YUV_NV16:
        plane_info.config.format   = SURF_YCBCR422MB;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->luma_address;
        break;
      case STM_PIXEL_FORMAT_YUV:
        plane_info.config.format   = SURF_YCBCR422MB;
        plane_info.config.bitdepth = 16;
        buffer_add = buffer_descr->luma_address;
        break;
      default:
        plane_info.config.format   = SURF_ARGB8888;
        plane_info.config.bitdepth = 32;
        buffer_add = buffer_descr->rgb_address;
        break;
    }

    plane_info.config.baseaddr   = buffer_add;
    plane_info.config.source.w   = buffer_descr->cap_format.width;
    plane_info.config.source.h   = buffer_descr->cap_format.height;
    plane_info.config.dest.dim.w = buffer_descr->cap_format.width;
    plane_info.config.dest.dim.h = buffer_descr->cap_format.height;
    plane_info.config.dest.x     = 0;
    plane_info.config.dest.y     = 0;
    plane_info.config.pitch      = buffer_descr->cap_format.stride;

    ret = info->fbops->fb_ioctl(info, STMFBIO_SET_PLANEMODE, p_info);
    if (ret < 0)
      printk("Set Plane Info failed err = %d!\n", ret);

    set_fs(oldfs);
  }
}


/*************************************************************/
/**                                                         **/
/**  Pan the display buffer to the new captured buffer      **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
void capture_pan_display(struct fb_info *info, stm_pixel_capture_buffer_descr_t *buffer_descr)
{
  int ret;
  struct stmfbio_plane_pan pan;
  unsigned long buffer_add;

  switch (buffer_descr->cap_format.format) {
    case STM_PIXEL_FORMAT_RGB565:
    case STM_PIXEL_FORMAT_RGB888:
    case STM_PIXEL_FORMAT_ARGB1555:
    case STM_PIXEL_FORMAT_ARGB8565:
    case STM_PIXEL_FORMAT_ARGB8888:
    case STM_PIXEL_FORMAT_ARGB4444:
      buffer_add = buffer_descr->rgb_address;
      break;
    case STM_PIXEL_FORMAT_YUV_NV12:
    case STM_PIXEL_FORMAT_YUV_NV16:
    case STM_PIXEL_FORMAT_YCbCr422R:
    case STM_PIXEL_FORMAT_YUV:
    default:
      buffer_add = buffer_descr->luma_address;
      break;
  }

  pan.layerid  = 0;
  pan.activate = 0; /* immediate */
  pan.baseaddr = buffer_add;

  if (info->fbops->fb_ioctl)
  {
    long unsigned int pan_info = (long unsigned int)&pan;
    mm_segment_t oldfs = get_fs();

    set_fs(KERNEL_DS);

    ret = info->fbops->fb_ioctl(info, STMFBIO_PAN_PLANE, pan_info);
    if (ret < 0)
      printk("Pan display failed err = %d!\n", ret);

    set_fs(oldfs);
  }
}


/*************************************************************/
/**                                                         **/
/**  List available Inputs                                  **/
/**                                                         **/
/*************************************************************/
void capture_list_inputs (stm_pixel_capture_h pixel_capture)
{
  uint32_t input_idx=0;
  const char *name;

  while (stm_pixel_capture_enum_inputs(pixel_capture, input_idx, &name) == 0)
  {
    printk ("pixel_capture: Found input '%s' \tw/ idx %d\n", name, input_idx);
    ++input_idx;
  }
}


/*************************************************************/
/**                                                         **/
/**  Set Input By Index                                     **/
/**                                                         **/
/*************************************************************/
int capture_set_input_by_index(stm_pixel_capture_h pixel_capture, uint32_t input_idx)
{
  const char *name;

  /* check if the input exists
    Note: The value of it is not yet defined, check further aproach
  */

  /* Input/Output Method Negotiation */
  if(stm_pixel_capture_enum_inputs(pixel_capture, input_idx, &name) != 0)
    return -1;
  else
  {
    printk("input selected is %s\n", name);

    /* selects the first input */
    return stm_pixel_capture_set_input( pixel_capture, input_idx );
  }
}


/*************************************************************/
/**                                                         **/
/**  Set Input By Name                                      **/
/**                                                         **/
/*************************************************************/
int capture_set_input_by_name (stm_pixel_capture_h pixel_capture, const char * const name)
{
  uint32_t input_idx=0;
  const char *input_name;

  while (stm_pixel_capture_enum_inputs(pixel_capture, input_idx, &input_name) == 0)
  {
    if(strcmp(input_name, name) == 0)
    {
      printk ("pixel_capture: Input Selected '%s' w/ idx %d\n", input_name, input_idx);
      break;
    }
    ++input_idx;
  }

  return capture_set_input_by_index (pixel_capture, input_idx);
}


/*************************************************************/
/**                                                         **/
/**  Set the format                                         **/
/**                                                         **/
/*************************************************************/
int capture_set_format(stm_pixel_capture_h pixel_capture, stm_pixel_capture_buffer_format_t format)
{
  bool supported;

  /*
  Pixel Format Negotiation
  */
  stm_pixel_capture_try_format( pixel_capture, format, &supported);

  if(supported == true)
    return stm_pixel_capture_set_format( pixel_capture, format);
  else
    return -1;
}


/*************************************************************/
/**                                                         **/
/**  Performs input cropping                                **/
/**                                                         **/
/*************************************************************/
int capture_set_crop(stm_pixel_capture_h pixel_capture, stm_pixel_capture_rect_t input_rectangle)
{
  int ret;
  stm_pixel_capture_input_window_capabilities_t input_caps;

  ret = stm_pixel_capture_get_input_window_capabilities(pixel_capture, &input_caps);

  if(ret != 0)
    return ret;

  if((input_rectangle.width == 0) || (input_rectangle.height == 0))
    input_rectangle = input_caps.default_input_window_area;

  printk ("CROP : input_rectangle: %d-%d@%dx%d\n", input_rectangle.x, input_rectangle.y,
            input_rectangle.width, input_rectangle.height);

  return stm_pixel_capture_set_input_window(pixel_capture, input_rectangle);
}


/*******************************************************************************/
/**                                                                           **/
/**  Helper for buffer allocation from bpa2 partition                         **/
/**                                                                           **/
/*******************************************************************************/
int
stm_capture_allocate_bpa2 (size_t                      size,
        uint32_t                    align,
        bool                        cached,
        struct stm_capture_dma_area *area)
{
  static const char *partnames[] = { "capture", "vid-raw-input", "gfx-memory", "bigphysarea", NULL };
  unsigned int       idx; /* index into partnames[] */
  int                pages;

  BUG_ON (area == NULL);

  area->part = NULL;
  for (idx = 0; idx < ARRAY_SIZE (partnames); ++idx) {
    area->part = bpa2_find_part (partnames[idx]);
    if (area->part)
      break;
    printk (KERN_INFO "BPA2 partition '%s' not found!\n",
         partnames[idx]);
  }
  if (!area->part) {
    printk (KERN_ERR "No BPA2 partition found!\n");
    goto out;
  }
  printk (KERN_INFO "Using BPA2 partition '%s'\n", partnames[idx]);

  /* round size to pages */
  pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  area->size = pages * PAGE_SIZE;

  area->base = bpa2_alloc_pages (area->part,
               pages, align / PAGE_SIZE, GFP_KERNEL);
  if (!area->base) {
    printk (KERN_ERR "Couldn't allocate %d pages from BPA2 "
         "partition '%s'\n", pages, partnames[idx]);
    goto out;
  }

  /* remap physical */
  if (bpa2_low_part(area->part))
  {
    if (!cached)
    {
      printk (KERN_ERR "bpa2 low is allocated by kernel as cached memory\n");
      goto out_bpa2_free;
    }
    area->memory = phys_to_virt(area->base);
  }
  else if (cached)
    area->memory = ioremap_cache (area->base, area->size);
  else
    area->memory = ioremap_wc (area->base, area->size);
  if (!area->memory) {
    printk (KERN_ERR "ioremap of BPA2 memory failed\n");
    goto out_bpa2_free;
  }

  area->cached = cached;

  printk (KERN_INFO "Allocated memory BPA2 partition '%s'\n", partnames[idx]);
  return 0;

out_bpa2_free:
  bpa2_free_pages (area->part, area->base);
out:
  return -ENOMEM;
}


/*******************************************************************************/
/**                                                                           **/
/**  Helper for buffer deallocation from bpa2 partition                       **/
/**                                                                           **/
/*******************************************************************************/
void
stm_capture_free_bpa2 (struct stm_capture_dma_area *area)
{
  BUG_ON (area == NULL);

  if (!bpa2_low_part(area->part))
    iounmap ((void *) area->memory);
  bpa2_free_pages (area->part, area->base);

  area->part = NULL;
  area->base = 0;
  area->size = 0;
  area->memory = NULL;
}


/*******************************************************************************/
/**                                                                           **/
/**  Alloc buffers at application level and configure the shared memory area  **/
/**                                                                           **/
/*******************************************************************************/
int allocate_capture_buffers_memory(stm_pixel_capture_h pixel_capture,
                          uint32_t number_buffers,
                          stm_pixel_capture_buffer_format_t format,
                          stm_pixel_capture_buffer_descr_t *buffer_descr,
                          struct stm_capture_dma_area *memory_pool)
{
  int i,res;

  uint32_t buffer_rounded_size;
  uint32_t allocation_size;
  unsigned int page_size = PAGE_SIZE;
  uint32_t buffer_size = format.height * format.stride;
  uint32_t chroma_offset = buffer_size;

  if( (format.format == STM_PIXEL_FORMAT_YCbCr422R) || (format.format == STM_PIXEL_FORMAT_YUV)
    ||(format.format == STM_PIXEL_FORMAT_YUV_NV12) || (format.format == STM_PIXEL_FORMAT_YUV_NV16))
  {
    /* Allocate memory for both luma and chroma */
    buffer_size *= 2;
  }

  buffer_rounded_size = (buffer_size + page_size - 1) & ~(page_size - 1);
  allocation_size = buffer_rounded_size * number_buffers;

  /* Allocate the pool of buffers */
  res = stm_capture_allocate_bpa2 (allocation_size,
          (2*shm_align_mask) & ~shm_align_mask,
          false,
          memory_pool);
  if (res) {
    printk ("failed to allocate capture buffers\n");
    goto out_buffers;
  }

  /*
  Application initiate the buffer exchange framework
  */

  /* Application indicates what empty buffers to be used for the capture device to have them ready to fill in */
  /* todo check how it will look the application for a mutli-plane usage */
  for (i = 0; i < number_buffers; i++)
  {
    buffer_descr[i].cap_format = format;
    buffer_descr[i].length = buffer_rounded_size;
    buffer_descr[i].bytesused = buffer_rounded_size;

    if( (format.format == STM_PIXEL_FORMAT_RGB888) || (format.format == STM_PIXEL_FORMAT_RGB565)
      ||(format.format == STM_PIXEL_FORMAT_ARGB8888) || (format.format == STM_PIXEL_FORMAT_ARGB1555)
      ||(format.format == STM_PIXEL_FORMAT_ARGB4444) || (format.format == STM_PIXEL_FORMAT_ARGB8565))
    {
      buffer_descr[i].rgb_address = memory_pool->base + (i * buffer_rounded_size);
      printk ("buffer_descr[%d] : %x %u %u %u %u\n", i, (unsigned int)buffer_descr[i].rgb_address, buffer_descr[i].bytesused, buffer_descr[i].cap_format.stride,
                buffer_descr[i].cap_format.width, buffer_descr[i].cap_format.height);
    }
    else if((format.format == STM_PIXEL_FORMAT_YCbCr422R) || (format.format == STM_PIXEL_FORMAT_YUV))
    {
      buffer_descr[i].luma_address  = memory_pool->base + (i * buffer_rounded_size);
      buffer_descr[i].chroma_offset = 0;
      printk ("buffer_descr[%d] : %x %u %u %u %u %u\n", i, (unsigned int)buffer_descr[i].luma_address, (unsigned int)buffer_descr[i].chroma_offset, buffer_descr[i].bytesused,
                buffer_descr[i].cap_format.stride, buffer_descr[i].cap_format.width, buffer_descr[i].cap_format.height);
    }
    else if((buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YUV_NV12) || (buffer_descr[i].cap_format.format == STM_PIXEL_FORMAT_YUV_NV16))
    {
      buffer_descr[i].luma_address  = memory_pool->base + (i * buffer_rounded_size);
      buffer_descr[i].chroma_offset = chroma_offset;     /* the driver will retreive chroma base address at its level */
      printk ("buffer_descr[%d] : %x %u %u %u %u %u\n", i, (unsigned int)buffer_descr[i].luma_address, (unsigned int)buffer_descr[i].chroma_offset, buffer_descr[i].bytesused,
                buffer_descr[i].cap_format.stride, buffer_descr[i].cap_format.width, buffer_descr[i].cap_format.height);
    }
    else
    {
      /* Error! Format not supported */
      stm_capture_free_bpa2(memory_pool);
      return -EINVAL;
    }
  }
  return 0;

out_buffers:
  return -ENOMEM;
}

int free_capture_buffers_memory(struct stm_capture_dma_area *memory_pool)
{
  printk ("free allocated capture buffers %p\n",memory_pool);
  stm_capture_free_bpa2(memory_pool);
  return 0;
}

#if defined(CONFIG_STM_VIRTUAL_PLATFORM)
/*************************************************************/
/**                                                         **/
/**  Generate Pattern for the Virtual Stream Injection      **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static int stream_create_rgb_test_pattern(struct stm_capture_dma_area *memory_pool, stm_pixel_capture_input_params_t *InputParams)
{
  int res=0;
  uint32_t allocation_size;

#ifdef USE_PICTURE_FROM_HEADER_FILE
  allocation_size = dvpdata_size;
#else
  uint32_t start_x = InputParams->active_window.x;
  uint32_t start_y = InputParams->active_window.y;
  uint32_t width = InputParams->active_window.width;
  uint32_t height = InputParams->active_window.height;
  uint32_t frame_rate = InputParams->src_frame_rate;
  uint32_t color_space = InputParams->color_space;
  uint32_t htotal = InputParams->htotal;
  uint32_t vtotal = InputParams->vtotal;

  unsigned char *bufferdata = NULL;
  uint32_t *header          = NULL;
  uint32_t *pixel           = NULL;
  uint32_t x,y,hsync,vsync,de_flag,parity;
  unsigned int page_size = PAGE_SIZE;
  uint32_t buffer_size = (htotal * vtotal * 2) + (4 * sizeof(uint32_t));
  allocation_size = (buffer_size + page_size - 1) & ~(page_size - 1);
#endif

  printk ("%s : generating %dx%d  RGB24 pattern...\n",__func__, InputParams->active_window.width, InputParams->active_window.height);

  /* Allocate the buffer memory */
  res = stm_capture_allocate_bpa2 (allocation_size,
          (2*shm_align_mask) & ~shm_align_mask,
          false,
          memory_pool);
  if (res) {
    printk ("failed to allocate pattern buffer\n");
    return res;
  }
  printk ("%s : hAllocated %d bytes !!\n",__func__, allocation_size);

#ifdef USE_PICTURE_FROM_HEADER_FILE
  memcpy((void *)memory_pool->memory, dvpdata, allocation_size);
#else
  bufferdata = (unsigned char *)memory_pool->memory;
  header     = (uint32_t *)bufferdata;
  pixel      = (uint32_t *)bufferdata+(4 * sizeof(uint32_t));

  /* Write header file */
  header[0] = htotal;
  header[1] = vtotal;
  header[2] = frame_rate;
  header[3] = (color_space == STM_PIXEL_CAPTURE_RGB) ? 0:1;

  printk ("%s : header writen successfully !!\n",__func__);

#define HSYNC_POS     16
#define VSYNC_POS     17
#define DE_FLAG_POS   18
#define PARITY_POS    19

  /* For the first line hsync and vsync should be equal to 1 */
  hsync = vsync = 1;
  /* At least two blacking lines are required for DVP synchronization */
  de_flag       = 0;
  /* We generate a frame or Top field pattern */
  parity        = 0;

  /* Write Pixels data */
  for(y=0;y<vtotal;y++)
  {
    for(x=0;x<2*htotal;x+=2)
    {
      uint32_t red   = 0;
      uint32_t green = 0;
      uint32_t blue  = x%256;

      if(y<(height/2))
        red = 255;
      else
        green = 255;

      /* At least two blanking lines are required for DVP synchronization */
      if((y >= start_y) && (x >= start_x))
        de_flag = 1;
      else
        red = green = blue = 0;

      if(x <= 10 /* cycle */)
        vsync = 1;
      else
        vsync = 0;

      /* write first 32 bits from the wanted pixel value */
      *(pixel+x) = (green<<22) | (blue<<6);

      /* write second 32 bits from the wanted pixel value */
      *(pixel+(x+1)) = (hsync << HSYNC_POS) | (vsync << VSYNC_POS) | (de_flag << DE_FLAG_POS) | (parity << PARITY_POS) | (red<<8);

      /* clear hsync for the next pixel values */
      hsync = vsync = 1;
    }

    pixel += (2*htotal);
  }
  printk ("%s : Generated bytes size = %08x\n",__func__, (width*height*8));

#undef HSYNC_POS
#undef VSYNC_POS
#undef DE_FLAG_POS
#undef PARITY_POS
#endif
  return 0;
}

/*************************************************************/
/**                                                         **/
/**  Start Virtual Stream Injection for DVP Capture         **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
#define STREAM_INJ_IP_BASE    0x08500000
#define STREAM_LM_ADD         0x00
#define STREAM_LM_OFF         0x04
#define STREAM_LM_SIZE        0x08
#define STREAM_CONFIG         0x0C
#define STREAM_STATUS         0x10
#define STREAM_INJ_START      0x14
static   uint32_t* stream_base_add = NULL;

int stream_init_pattern_injection(stm_pixel_capture_input_params_t InputParams)
{
  int res;
  struct stm_capture_dma_area memory_pool;

  stream_base_add = (uint32_t*)ioremap(STREAM_INJ_IP_BASE, 0x1000);
  if(!stream_base_add)
  {
    printk ("%s : ERROR to map c8inj_stream base address!!!\n", __func__);
    return -1;
  }

  /* Allocate and generate pattern into the Stream buffer */
  memset(&memory_pool, 0, sizeof(struct stm_capture_dma_area));
  res = stream_create_rgb_test_pattern(&memory_pool, &InputParams);
  if(res)
    return -1;

  /* Program the c8inj_stream */
  writel(0x40000000, stream_base_add+(STREAM_LM_ADD>>2));
  writel(((uint32_t)memory_pool.base - 0x40000000), stream_base_add+(STREAM_LM_OFF>>2));
  writel(50000, stream_base_add+(STREAM_LM_SIZE>>2));

  return 0;
}

void stream_set_pattern_injection_status(bool start, int loop_count)
{
  if(start)
  {
    /* Reconfigure loop_count */
    writel(loop_count, stream_base_add+(STREAM_CONFIG>>2));
  }

  /* Start/Stop the injection */
  writel((int)start, stream_base_add+(STREAM_INJ_START>>2));
}
#endif /* CONFIG_STM_VIRTUAL_PLATFORM */
