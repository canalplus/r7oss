/* see stlinux bug #20559 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/bpa2.h>

#include <linux/stm/blitter.h>


static int __init
test_init(void)
{
	long res = 0;
	stm_blitter_t *blitter;

	struct bpa2_part *bpa2;
	unsigned long buf_size;
	stm_blitter_dimension_t size;
	unsigned long           stride;

	stm_blitter_surface_t         *src1;
	stm_blitter_surface_address_t  src1_buf_address;
	stm_blitter_surface_t         *src2;
	stm_blitter_surface_address_t  src2_buf_address;
	stm_blitter_surface_t         *dst1;
	stm_blitter_surface_address_t  dst1_buf_address;
	stm_blitter_surface_t         *dst2;
	stm_blitter_surface_address_t  dst2_buf_address;

	static const stm_blitter_color_t white = { .a = 0xff,
						   .r = 0xff,
						   .g = 0xff,
						   .b = 0xff };
	static const stm_blitter_color_t red = { .a = 0xff,
						 .r = 0xff,
						 .g = 0x00,
						 .b = 0x00 };
	static const stm_blitter_color_t green = { .a = 0xff,
						   .r = 0x00,
						   .g = 0xff,
						   .b = 0x00 };
	static const stm_blitter_color_t blue = { .a = 0xff,
						  .r = 0x00,
						  .g = 0x00,
						  .b = 0xff };


	pr_info("%s\n", __func__);

	bpa2 = bpa2_find_part("blitter");
	if (!bpa2) {
		pr_err("no bpa2 partition 'blitter'\n");
		return -ENOENT;
	}
	size.w = 20;
	size.h = 20;
	stride = 40;
	buf_size = stride * size.h;
	src1_buf_address.base = bpa2_alloc_pages(bpa2,
						 ((buf_size + PAGE_SIZE - 1)
						  / PAGE_SIZE),
						 PAGE_SIZE,
						 GFP_KERNEL);
	src2_buf_address.base = bpa2_alloc_pages(bpa2,
						 ((buf_size + PAGE_SIZE - 1)
						  / PAGE_SIZE),
						 PAGE_SIZE,
						 GFP_KERNEL);
	dst1_buf_address.base = bpa2_alloc_pages(bpa2,
						 ((buf_size + PAGE_SIZE - 1)
						  / PAGE_SIZE),
						 PAGE_SIZE,
						 GFP_KERNEL);
	dst2_buf_address.base = bpa2_alloc_pages(bpa2,
						 ((buf_size + PAGE_SIZE - 1)
						  / PAGE_SIZE),
						 PAGE_SIZE,
						 GFP_KERNEL);
	if (!src1_buf_address.base || !src2_buf_address.base
	    || !dst1_buf_address.base || !dst2_buf_address.base) {
		res = -ENOMEM;
		goto out_bpa2;
	}


	blitter = stm_blitter_get(0);
	if (IS_ERR(blitter)) {
		res = PTR_ERR(blitter);
		blitter = NULL;
		pr_err("could not get blitter: %lld\n", (long long) res);
		goto out_blitter;
	}

	/* allocate some memory */


	src1 = stm_blitter_surface_new_preallocated(STM_BLITTER_SF_RGB565,
						    STM_BLITTER_SCS_RGB,
						    &src1_buf_address,
						    buf_size,
						    &size,
						    stride);
	if (IS_ERR(src1)) {
		res = PTR_ERR(src1);
		goto out_src1;
	}
	stm_blitter_surface_clear(blitter, src1, &red);

	src2 = stm_blitter_surface_new_preallocated(STM_BLITTER_SF_RGB565,
						    STM_BLITTER_SCS_RGB,
						    &src2_buf_address,
						    buf_size,
						    &size,
						    stride);
	if (IS_ERR(src2)) {
		res = PTR_ERR(src2);
		goto out_src2;
	}
	stm_blitter_surface_clear(blitter, src1, &green);

	dst1 = stm_blitter_surface_new_preallocated(STM_BLITTER_SF_RGB565,
						    STM_BLITTER_SCS_RGB,
						    &dst1_buf_address,
						    buf_size,
						    &size,
						    stride);
	if (IS_ERR(dst1)) {
		res = PTR_ERR(dst1);
		goto out_dst1;
	}
	stm_blitter_surface_clear(blitter, src1, &blue);

	dst2 = stm_blitter_surface_new_preallocated(STM_BLITTER_SF_RGB565,
						    STM_BLITTER_SCS_RGB,
						    &dst2_buf_address,
						    buf_size,
						    &size,
						    stride);
	if (IS_ERR(dst2)) {
		res = PTR_ERR(dst2);
		goto out_dst2;
	}
	stm_blitter_surface_clear(blitter, src1, &white);


	stm_blitter_surface_stretchblit(blitter, src1, NULL, dst1, NULL, 1);
	stm_blitter_surface_stretchblit(blitter, src1, NULL, dst2, NULL, 1);
	stm_blitter_surface_stretchblit(blitter, src2, NULL, dst1, NULL, 1);
	stm_blitter_surface_blit(blitter, src2, NULL, dst2, NULL, 1);
	stm_blitter_surface_blit(blitter, src1, NULL, dst2, NULL, 1);
	stm_blitter_surface_blit(blitter, dst1, NULL, dst2, NULL, 1);

	stm_blitter_surface_put(dst2);
out_dst2:
	stm_blitter_surface_put(dst1);
out_dst1:
	stm_blitter_surface_put(src2);
out_src2:
	stm_blitter_surface_put(src1);
out_src1:

	stm_blitter_put(blitter);
out_blitter:

out_bpa2:
	if (dst2_buf_address.base)
		bpa2_free_pages(bpa2, dst2_buf_address.base);
	if (dst1_buf_address.base)
		bpa2_free_pages(bpa2, dst1_buf_address.base);
	if (src2_buf_address.base)
		bpa2_free_pages(bpa2, src2_buf_address.base);
	if (src1_buf_address.base)
		bpa2_free_pages(bpa2, src1_buf_address.base);

	if (!res) {
		pr_debug("multiple blit tests finished.\n");
		printk("multiple blit tests finished.\n");
		res = -ENODEV;
	} else
		pr_err("multiple blit tests failed!\n");

       printk("exit\n");
	return res;
}


module_init (test_init);

MODULE_AUTHOR("André Draszik <andre.draszik@st.com>");
MODULE_LICENSE("GPL");
