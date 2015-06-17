/*
 * conversion-test.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Conversion Operations using the blitter hw
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/stm/blitter.h>
#include <linux/bpa2.h>
#include <linux/fb.h>

#include <gam_utils.h>

#define BLIT_TEST_AUTHOR "Akram BEN BELGACEM <akram.ben-belgacem@st.com>"
#define BLIT_TEST_DESC   "A sample blit rectangles test operations using the blitter driver"

struct fb_info       *info;

/* Test module parameters */
static char *file_name1 = "/root/bateau_YCbCr_422r.gam";
static char *file_name3 = "/root/ftb_420_R2B.gam";

module_param(file_name1, charp, 0000);
MODULE_PARM_DESC(file_name1, "Source1 GAM File Name");

module_param(file_name3, charp, 0000);
MODULE_PARM_DESC(file_name3, "Destination GAM File Name");

#define TEST_BLIT_RECT_WIDHT       120
#define TEST_BLIT_RECT_HEIGHT       60

static int __init conversion_test_init(void)
{
#ifndef LOAD_DESTINATION_GAMFILE
  int res;
  struct bpa2_part                 *bpa2 = NULL;
#endif
  stm_blitter_t                    *blitter = NULL;
  stm_blitter_surface_t            *src1    = NULL;
  stm_blitter_surface_t            *dest    = NULL;
  stm_blitter_surface_t            *dest2   = NULL;
  stm_blitter_surface_blitflags_t   flags   = STM_BLITTER_SBF_NONE;
  stm_blitter_rect_t                src_rects   = { { 100, 50 }, { 200, 200 } };
  stm_blitter_rect_t                dst_rects   = { { 100, 50 }, { 200, 200 } };
  stm_blitter_point_t               dst_points  = { 0, 0 };
  stm_blitter_serial_t              serial;
  stm_blitter_rect_t               *blit_rects  = NULL;
  stm_blitter_point_t              *blit_points = NULL;
  stm_blitter_color_t              *color_p     = NULL;
  bitmap_t Src1;
  bitmap_t Dest;
  bitmap_t Dest2;
  int                               i=0, k=0;

  printk(KERN_INFO "Starting Conversion tests\n");

  printk(KERN_INFO "Loading Source1 Gam file...\n");
  if(load_gam_file( file_name1, &Src1) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }

#ifdef LOAD_DESTINATION_GAMFILE
  printk(KERN_INFO "Loading Destination Gam file...\n");
  if(load_gam_file( file_name3, &Dest) < 0)
  {
    printk(KERN_ERR "Failed to load GAM file memory!\n");
    return -ENXIO;
  }
#else
  for (res = 0; res < num_registered_fb; ++res)
  {
    if (registered_fb[res])
      {
        info = registered_fb[res];
        printk ("found a registered framebuffer @ %p\n", info);
        break;
      }
  }

  Dest.size.w = 1280;
  Dest.size.h = 720;

  Dest2.size.w = 1280;
  Dest2.size.h = 720;

  if (info)
  {
    struct module *owner = info->fbops->owner;

    if (try_module_get (owner))
      {
        printk ("try_module_get\n");
        if (info->fbops->fb_open && !info->fbops->fb_open (info, 0))
          {
            switch (info->var.bits_per_pixel)
              {
              case  8: Dest.format = STM_BLITTER_SF_LUT8; break;
              case 16: Dest.format = STM_BLITTER_SF_RGB565/*STM_BLITTER_SF_UYVY*/; break;
              case 24: Dest.format = STM_BLITTER_SF_RGB24; break;
              case 32: Dest.format = STM_BLITTER_SF_ARGB; break;
              default:
                if (info->fbops->fb_release)
                  info->fbops->fb_release (info, 0);
                module_put (owner);
                info = NULL;
                break;
              }

            Dest.buffer_address.base = info->fix.smem_start;
            /*Dest.buffer_size = info->fix.smem_len;*/
            Dest.pitch = info->var.xres * info->var.bits_per_pixel / 8;
            Dest.size.w = info->var.xres;
            Dest.size.h = info->var.yres_virtual;
            Dest.buffer_size = Dest.pitch * Dest.size.h;
            printk ("fb: %lx %zu %ld\n", Dest.buffer_address.base, Dest.buffer_size, Dest.pitch);
          }
        else
          {
            module_put (owner);
            printk ("no fb_open()\n");
            info = NULL;
          }
      }
    else
      {
        printk ("failed try_module_get\n");
        info = NULL;
      }
  }

  bpa2 = bpa2_find_part ("bigphysarea");
  if (!bpa2)
  {
    printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
    res = -ENOENT;
    return res;
  }

  printk ("using memory from BPA2\n");
  Dest2.format = STM_BLITTER_SF_YUYV;
  Dest2.pitch = (Dest2.size.w + 5) * 4;
  Dest2.buffer_size = Dest2.pitch * (Dest2.size.h + 7);
  Dest2.buffer_address.base = bpa2_alloc_pages (bpa2,
                                               ((Dest2.buffer_size
                                                 + PAGE_SIZE - 1)
                                                / PAGE_SIZE),
                                               PAGE_SIZE, GFP_KERNEL);
  if (!Dest2.buffer_address.base)
  {
    printk ("%s: could not allocate %zu bytes from bpa2 partition\n",
            __func__, Dest2.buffer_size);
    res = -ENOMEM;
    return res;
  }
  printk ("%s: surface @ %.8lx\n", __func__, Dest2.buffer_address.base);

  Dest2.memory = phys_to_virt(Dest2.buffer_address.base);
#endif


  /* Allocate memory for color data */
  color_p = (stm_blitter_color_t *)kmalloc((size_t)sizeof(stm_blitter_color_t), GFP_KERNEL);
  if(!color_p)
  {
    printk(KERN_ERR "Failed to allocate memory!\n");
    return -ENXIO; /* No such device or address */
  }

  /* Get a blitter handle */
  if((blitter = stm_blitter_get(0)) == NULL)
  {
    printk(KERN_ERR "Failed to get a blitter device!\n");
    return -ENXIO; /* No such device or address */
  }

  /* Create a new surface for source1 */
  if((src1 = stm_blitter_surface_new_preallocated(Src1.format,Src1.colorspace,&Src1.buffer_address,Src1.buffer_size,&Src1.size,Src1.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for src1!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Create a new surface for destination */
  if((dest = stm_blitter_surface_new_preallocated(Dest.format,Dest.colorspace,&Dest.buffer_address,Dest.buffer_size,&Dest.size,Dest.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for dest!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Create a new surface for destination */
  if((dest2 = stm_blitter_surface_new_preallocated(Dest2.format,Dest2.colorspace,&Dest2.buffer_address,Dest2.buffer_size,&Dest2.size,Dest2.pitch)) == NULL)
  {
    printk(KERN_ERR "Failed to get a create new surface for dest!\n");
    stm_blitter_put(blitter);
    goto out_surface;
  }

  /* Setup blit flags in some particular use case (permultiplied surf maybe?) */
  if(stm_blitter_surface_set_blitflags(dest, flags) < 0)
    goto out_surface;
  if(stm_blitter_surface_set_blitflags(dest2, flags) < 0)
    goto out_surface;
  if(stm_blitter_surface_set_blitflags(src1, flags) < 0)
    goto out_surface;

  {
	  color_p->a = 0xff;
	  color_p->r = 0xff;
	  color_p->g = 0xff;
	  color_p->b = 0xff;
	  stm_blitter_surface_clear (blitter, dest2, color_p);
	  stm_blitter_surface_clear (blitter, dest, color_p);

	  printk(KERN_INFO "Starting Conversion tests..\n");

	  src_rects.position.x  = dst_points.x = 0;
	  src_rects.position.y  = dst_points.y = 0;
	  src_rects.size.w  = Src1.size.w;
	  src_rects.size.h  = Src1.size.h;
	  /* Simple Blit ops UYVY => YUYV => Display */
	  if(stm_blitter_surface_blit(blitter, src1, &src_rects, dest2, &dst_points, 1) < 0)
		goto out_surface;
	  if(stm_blitter_surface_blit(blitter, dest2, &src_rects, dest, &dst_points, 1) < 0)
		goto out_surface;

      /* Strech Blit ops, UYVY => YUYV should fails */
	  if(stm_blitter_surface_stretchblit(blitter, src1, &src_rects, dest2, &dst_rects, 1) >= 0)
		goto out_surface;
      /* Strech Blit ops, YUYV => UYVY should fails */
	  if(stm_blitter_surface_stretchblit(blitter, dest2, &src_rects, src1, &dst_rects, 1) >= 0)
		goto out_surface;

	  /* Simple Blit ops YUYV => UYVY => Display */
	  if(stm_blitter_surface_blit(blitter, dest2, &src_rects, src1, &dst_points, 1) < 0)
		goto out_surface;
	  src_rects.position.x  = dst_points.x = Src1.size.w;
	  src_rects.position.y  = dst_points.y = 0;
	  src_rects.size.w  = Dest2.size.w - Src1.size.w;
	  src_rects.size.h  = Src1.size.h;
	  if(stm_blitter_surface_blit(blitter, src1, &src_rects, dest, &dst_points, 1) < 0)
		goto out_surface;

	  /* wait for blitter to have finished all operations on this surface. */
	  stm_blitter_surface_get_serial (dest, &serial);
	  stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

      /* Compare to check swap */
      for(i=0;i<Src1.size.h;i++)
        for(k=0;k<Src1.pitch/4;k++)
          {
            unsigned int word2 = ((unsigned int *)Dest2.memory)[i*Dest2.pitch/4 + k];
            unsigned int word1 = ((unsigned int *)Src1.memory)[i*Src1.pitch/4 + k];

            if( ((word2&0x000000FF)>> 0) != ((word1&0x0000FF00)>> 8) ||
                ((word2&0x0000FF00)>> 8) != ((word1&0x000000FF)>> 0) ||
                ((word2&0x00FF0000)>>16) != ((word1&0xFF000000)>>24) ||
                ((word2&0xFF000000)>>24) != ((word1&0x00FF0000)>>16)   )
              {
                printk("Error pixel value did change 0x%x  0x%x\n",word2,word1);
              }

		  }

	  save_gam_file("/root/dumped_dest_t1.gam", &Dest);
  }

  /* Free the rects memory */
  kfree(blit_rects);
  kfree(blit_points);

  /* Release the surface */
  stm_blitter_surface_put(src1);
  stm_blitter_surface_put(dest);
  stm_blitter_surface_put(dest2);

  /* Release the blitter */
  stm_blitter_put(blitter);

  free_gam_file(&Src1);
#ifdef LOAD_DESTINATION_GAMFILE
  free_gam_file(&Dest);
#endif

    bpa2_free_pages (bpa2, Dest2.buffer_address.base);
  /* Free the color data memory */
  if(!color_p)
    kfree((void *)color_p);

  printk(KERN_INFO "Conversion tests finished.\n");
  return 0;

out_surface:
#ifndef LOAD_DESTINATION_GAMFILE
  if (!info && bpa2 && Dest.buffer_address.base)
    bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif
  printk(KERN_INFO "Conversion tests failed!\n");
  return -1;
}

static void __exit conversion_test_exit(void)
{
	printk(KERN_INFO " Conversion tests module removed.\n");
}

module_init(conversion_test_init);
module_exit(conversion_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(BLIT_TEST_AUTHOR);
MODULE_DESCRIPTION(BLIT_TEST_DESC);

