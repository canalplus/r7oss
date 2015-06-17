/*
 * draw-test.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Draw Rectangles Operations using the blitter hw
 */


#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/slab.h>		/* Needed for kmalloc and kfree */
#include <asm/io.h>

#include <linux/stm/blitter.h>
#include <linux/bpa2.h>
#include <linux/fb.h>
#include <gam_utils.h>

#define DRAW_TEST_AUTHOR "Akram BEN BELGACEM <akram.ben-belgacem@st.com>"
#define DRAW_TEST_DESC   "A sample draw rectangles test operation using the blitter driver"

#define DONT_ALLOCATE_DESTINATION_BUFFER

/* Test module parameters */
static int number_of_loops = 256;

module_param(number_of_loops, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(number_of_loops, "Test iteration number");

/* use consts so they are visible in the debugger */
static const int BITMAP_WIDTH       = 1280;
static const int BITMAP_HEIGHT      = 720;
static const int BITMAP_DEPTH       = 32;
static const int BITMAP_PITCH       = (1280*(32>>3));
static const int BITMAP_SIZE        = ((1280*(32>>3))*720);
static const stm_blitter_surface_format_t BITMAP_PIXFMT = STM_BLITTER_SF_ARGB;

static int __init draw_test_init(void)
{
#ifdef DONT_ALLOCATE_DESTINATION_BUFFER
	int res;
	struct bpa2_part                 	*bpa2 = NULL;
	struct fb_info       							*info = NULL;
#endif
	stm_blitter_t *blitter = NULL;
	stm_blitter_surface_t *surface = NULL;
	stm_blitter_surface_drawflags_t     flags 			= STM_BLITTER_SDF_NONE /*STM_BLITTER_SDF_BLEND | STM_BLITTER_SDF_DST_PREMULITPLY*/;
	stm_blitter_surface_format_t        format			= BITMAP_PIXFMT;
	stm_blitter_surface_colorspace_t    colorspace		= STM_BLITTER_SCS_RGB;
	unsigned long                       buffer_size		= BITMAP_SIZE;
	stm_blitter_dimension_t             size 			= { BITMAP_WIDTH, BITMAP_HEIGHT };
	unsigned long                       pitch 			= BITMAP_PITCH;
	stm_blitter_rect_t                  dst_rect		= { { 0, 0 }, { BITMAP_WIDTH, BITMAP_HEIGHT } };
	stm_blitter_rect_t 				   *draw_rects		= NULL;
	int									i				= 0;
	stm_blitter_surface_address_t 	    buffer_address;
	unsigned char                      *fbuffer;
	stm_blitter_serial_t     			serial;
	stm_blitter_color_t                *color_p = NULL;
	bitmap_t Dest;

	printk(KERN_INFO "Starting Draw Rectangle tests\n");

#ifdef DONT_ALLOCATE_DESTINATION_BUFFER
  for (res = 0; res < num_registered_fb; ++res)
    {
      if (registered_fb[res])
        {
          info = registered_fb[res];
          printk ("found a registered framebuffer @ %p\n", info);
          break;
        }
    }

  size.w = 1024;
  size.h = 768;

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
                case  8: format = STM_BLITTER_SF_LUT8; break;
                case 16: format = STM_BLITTER_SF_RGB565; break;
                case 24: format = STM_BLITTER_SF_RGB24; break;
                case 32: format = STM_BLITTER_SF_ARGB; break;
                default:
                  if (info->fbops->fb_release)
                    info->fbops->fb_release (info, 0);
                  module_put (owner);
                  info = NULL;
                  break;
                }

              buffer_address.base = info->fix.smem_start;
              /*buffer_size = info->fix.smem_len;*/
              pitch = info->var.xres * info->var.bits_per_pixel / 8;
              size.w = info->var.xres;
              size.h = info->var.yres_virtual;
              buffer_size = pitch * size.h;
              printk ("fb: %lx %ld %ld %ld %ld\n", buffer_address.base, buffer_size, pitch, size.w, size.h);
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

  if (!info)
    {
      bpa2 = bpa2_find_part ("bigphysarea");
      if (!bpa2)
        {
          printk ("%s: no bpa2 partition 'bigphysarea'\n", __func__);
          res = -ENOENT;
          return res;
        }

      printk ("using memory from BPA2\n");
      format = STM_BLITTER_SF_ARGB;
      pitch = (size.w + 5) * 4;
      buffer_size = pitch * (size.h + 7);
      buffer_address.base = bpa2_alloc_pages (bpa2,
                                              ((buffer_size
                                                + PAGE_SIZE - 1)
                                               / PAGE_SIZE),
                                              PAGE_SIZE, GFP_KERNEL);
      if (!buffer_address.base)
        {
          printk ("%s: could not allocate %lu bytes from bpa2 partition\n",
                  __func__, buffer_size);
          res = -ENOMEM;
          return res;
        }
      printk ("%s: surface @ %.8lx\n", __func__, buffer_address.base);
    }
	fbuffer = phys_to_virt(buffer_address.base);
#else
	/* Allocate memory for surface buffer */
	fbuffer = kmalloc((size_t)buffer_size, GFP_KERNEL);
	if(!fbuffer)
	{
		printk(KERN_ERR "Failed to allocate memory!\n");
		return -ENXIO; /* No such device or address */
	}
	buffer_address.base = virt_to_phys(fbuffer);
#endif
	/* Setup Destination Bitmap for dump... */
	Dest.format         = format;
	Dest.colorspace     = colorspace;
	Dest.buffer_size    = buffer_size;
	Dest.size           = size;
	Dest.pitch          = pitch;
	Dest.buffer_address = buffer_address;
	Dest.memory         = fbuffer;

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
		kfree((void *)fbuffer);
		goto out_surface;
	}

	/* Create a new surface */
	if((surface = stm_blitter_surface_new_preallocated(format,colorspace,&buffer_address,buffer_size,&size,pitch)) == NULL)
	{
		printk(KERN_ERR "Failed to get a blitter device!\n");
		stm_blitter_put(blitter);
		kfree((void *)fbuffer);
		goto out_surface;
	}

	/* Setup drawing flags in some particular use case (permultiplied surf maybe?) */
	if(stm_blitter_surface_set_drawflags(surface, flags) < 0)
	{
		printk(KERN_ERR "Failed to setup drawing flags for the surface !\n");
		stm_blitter_surface_put(surface);
		stm_blitter_put(blitter);
		kfree((void *)fbuffer);
		goto out_surface;
	}

	printk(KERN_INFO "Starting Draw Rectangle tests ..\n");
	color_p->a = 0xff;
	color_p->r = 0x00;
	color_p->g = 0x00;
	color_p->b = 0x80;
	stm_blitter_surface_clear (blitter, surface, color_p);

	for (i = 0; i < number_of_loops; i++)
	{
		color_p->a = i;
		color_p->r = ((i+(2*i))%255);
		color_p->g = ((i*2)%255);
		color_p->b = i;

		dst_rect.size.w = 400;
		dst_rect.size.h = 300;
		dst_rect.position.x = i + ((i * dst_rect.size.w)%BITMAP_HEIGHT);
		dst_rect.position.y = i + ((i * dst_rect.size.h)%BITMAP_WIDTH);
		printk ("rect: %d %d %d %d\n", (int)dst_rect.position.x, (int)dst_rect.position.y,
			  (int)dst_rect.size.w, (int)dst_rect.size.h);

		/* Setup the color fill value */
		if(stm_blitter_surface_set_color (surface, color_p) < 0)
		{
			printk(KERN_ERR "Failed to clear the created surface using the blitter device!\n");
			stm_blitter_surface_put(surface);
			stm_blitter_put(blitter);
			kfree((void *)buffer_address.base);
			goto out_surface;
		}

		/* Fill ops */
		if(stm_blitter_surface_draw_rect(blitter, surface, &dst_rect) < 0)
		{
			printk(KERN_ERR "DrawRectangle ops failed!\n");
			stm_blitter_surface_put(surface);
			stm_blitter_put(blitter);
			kfree((void *)fbuffer);
			goto out_surface;
		}
	}

	/* wait for operations to finish on this surface. */
	stm_blitter_surface_get_serial (surface, &serial);
	stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

	save_gam_file("/root/dumped_draw_t1_dest.gam", &Dest);

	printk(KERN_INFO "Starting Draw Rectangles tests (Single API call)..\n");
	color_p->a = 0xff;
	color_p->r = 0xff;
	color_p->g = 0xff;
	color_p->b = 0xff;
	stm_blitter_surface_clear (blitter, surface, color_p);

	/* Allocate memory for surface buffer */
	draw_rects = kmalloc(256*sizeof(stm_blitter_rect_t), GFP_KERNEL);
	if(!draw_rects)
	{
		printk(KERN_ERR "Failed to allocate memory!\n");
		goto out_surface;
	}

	for (i = 0; i < number_of_loops; i++)
	{
		draw_rects[i].size.w = 400;
		draw_rects[i].size.h = 300;
		draw_rects[i].position.x = i + ((i * draw_rects[i].size.w)%BITMAP_HEIGHT);
		draw_rects[i].position.y = i + ((i * draw_rects[i].size.h)%BITMAP_WIDTH);
		printk ("rect: %d %d %d %d\n", (int)draw_rects[i].position.x, (int)draw_rects[i].position.y,
			  (int)draw_rects[i].size.w, (int)draw_rects[i].size.h);
	}

	/* Setup the color fill value */
	color_p->a = 0x00;
	color_p->r = 0x00;
	color_p->g = 0x00;
	color_p->b = 0x80;
	if(stm_blitter_surface_set_color (surface, color_p) < 0)
	{
		printk(KERN_ERR "Failed to clear the created surface using the blitter device!\n");
		stm_blitter_surface_put(surface);
		stm_blitter_put(blitter);
		kfree((void *)buffer_address.base);
		goto out_surface;
	}

	/* Fill ops */
	if(stm_blitter_surface_draw_rects(blitter, surface, draw_rects, i-1) < 0)
	{
		printk(KERN_ERR "DrawRectangles ops failed!\n");
		stm_blitter_surface_put(surface);
		stm_blitter_put(blitter);
		kfree((void *)fbuffer);
		kfree(draw_rects);
		goto out_surface;
	}

	/* wait for operations to finish on this surface. */
	stm_blitter_surface_get_serial (surface, &serial);
	stm_blitter_wait (blitter, STM_BLITTER_WAIT_SERIAL, serial);

	save_gam_file("/root/dumped_draw_t2_dest.gam", &Dest);

	/* Free the rects memory */
	kfree(draw_rects);

	/* Release the surface */
	stm_blitter_surface_put(surface);
	/* Release the blitter */
	stm_blitter_put(blitter);

	/* Free the buffer memory */
	if(!buffer_address.base)
		kfree((void *)fbuffer);

	/* Free the color data memory */
	if(!color_p)
		kfree((void *)color_p);

	printk(KERN_INFO " Draw Rectangles tests finished\n");

	return 0;

out_surface:
#ifdef DONT_ALLOCATE_DESTINATION_BUFFER
	if (!info && bpa2 && Dest.buffer_address.base)
	bpa2_free_pages (bpa2, Dest.buffer_address.base);
#endif
	printk(KERN_INFO "Fill Rectangles tests failed!\n");
	return -1;
}

static void __exit draw_test_exit(void)
{
	printk(KERN_INFO " Draw Rectangles tests module removed.\n");
}

module_init(draw_test_init);
module_exit(draw_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRAW_TEST_AUTHOR);
MODULE_DESCRIPTION(DRAW_TEST_DESC);
