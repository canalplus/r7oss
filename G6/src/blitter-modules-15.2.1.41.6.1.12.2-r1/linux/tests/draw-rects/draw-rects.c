/*
 * draw-test.c
 *
 * Copyright (C) STMicroelectronics Limited 2011. All rights reserved.
 *
 * Test to exercise Draw Rectangles Operations using the blitter hw
 */


#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/slab.h>   /* Needed for kmalloc and kfree */
#include <linux/io.h>

#include <linux/stm/blitter.h>
#include <linux/bpa2.h>
#include <linux/fb.h>

#include "gam_utils/gam_utils.h"
#include "gam_utils/surf_utils.h"


/* Test module parameters */
static int x      = 100;
static int y      = 100;
static int w      = 320;
static int h     = 240;

module_param(x, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(x, "Position X");

module_param(y, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(y, "Position Y");

module_param(w, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(w, "Test iteration number");

module_param(h, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(h, "Rectangle width");

static int __init draw_rects_init(void)
{
	stm_blitter_t *blitter;
	stm_blitter_rect_t dst_rect  = { { x, y }, { w, h } };
	stm_blitter_serial_t serial;
	stm_blitter_color_t color;
	bitmap_t Dest;
	struct blit_fb_bpa2_test_surface *dest;
	int res;

	printk(KERN_INFO "Starting Draw Rectangle tests\n");

	/* Create a new surface for destination */
	Dest.size.w = 1024;
	Dest.size.h = 768;
	dest = get_fb_or_bpa2_surf(&Dest.size, &Dest.buffer_address,
				   &Dest.buffer_size, &Dest.pitch,
				   &Dest.format);
	if (IS_ERR(dest)) {
		res = PTR_ERR(dest);
		printk(KERN_ERR "Failed to get a create new surface for "
		       "dest: %d!\n", res);
		dest = NULL;
		return res;
	}
	Dest.memory = phys_to_virt(Dest.buffer_address.base);

	/* Get a blitter handle */
	blitter = stm_blitter_get(0);
	if (IS_ERR(blitter)) {
		res = PTR_ERR(blitter);
		printk(KERN_ERR "Failed to get a blitter device: %d!\n",
		       res);
		blitter = NULL;
		goto out_dest;
	}

	/* Setup drawing flags in some particular use case
	   (permultiplied surf maybe?) */
	res = stm_blitter_surface_set_drawflags(dest->surf,
						STM_BLITTER_SDF_NONE);
	if (res < 0) {
		printk(KERN_ERR "Failed to setup drawing flags for "
		       "the surface: %d!\n", res);
		goto out_blitter;
	}

	printk(KERN_INFO "Starting Draw Rectangle tests ..\n");
	color.a = 0xff;
	color.r = 0x00;
	color.g = 0x00;
	color.b = 0x80;
	stm_blitter_surface_clear(blitter, dest->surf, &color);

	color.a = 0xFF;
	color.r = 0xFF;
	color.g = 0x80;
	color.b = 0x10;

	printk(KERN_INFO "rect: %ld %ld %ld %ld\n",
	       dst_rect.position.x, dst_rect.position.y,
	       dst_rect.size.w, dst_rect.size.h);

	/* Setup the color fill value */
	res = stm_blitter_surface_set_color(dest->surf, &color);
	if (res < 0) {
		printk(KERN_ERR "Failed to clear the created surface "
		       "using the blitter device: %d!\n", res);
		goto out_blitter;
	}

	/* Fill ops */
	res = stm_blitter_surface_draw_rect(blitter, dest->surf, &dst_rect);
	if (res < 0) {
		printk(KERN_ERR "DrawRectangle ops failed: %d!\n", res);
		goto out_blitter;
	}

	/* wait for operations to finish on this surface. */
	stm_blitter_surface_get_serial(dest->surf, &serial);
	stm_blitter_wait(blitter, STM_BLITTER_WAIT_SERIAL, serial);

	save_gam_file("/root/dumped_draw_rect_dest.gam", &Dest);

out_blitter:
	stm_blitter_put(blitter);
out_dest:
	free_fb_bpa2_surf(dest);

	if (!res)
		printk(KERN_INFO "Draw Rectangle test finished.\n");
	else
		printk(KERN_INFO "Draw Rectangle test failed!\n");

	return res;
}

static void __exit draw_rects_exit(void)
{
	printk(KERN_INFO "Draw Rectangles tests module removed.\n");
}

module_init(draw_rects_init);
module_exit(draw_rects_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akram BEN BELGACEM <akram.ben-belgacem@st.com>");
MODULE_DESCRIPTION("A sample draw rectangle test operation using the blitter driver");
