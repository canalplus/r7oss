/*
 * sh4cache.c
 *
 * Copyright (C) STMicroelectronics Limited 2005. All rights reserved.
 *
 * Report violations of assertions made about cache behavior
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/addrspace.h>

#define SET_SZ 512
#define LINE_SZ 32

static unsigned int *data = 0;

static void sh4_cache_snoop_set(void *set, void *p, unsigned int len)
{
	unsigned int i, base, end;
	int listed_bounds = 0;

	/* read the tags (and status bits) disturbing the cache as little
	 * as possible
	 */
	for (i=0; i<SET_SZ; i++) {
		data[i] = *((volatile unsigned int *) (((char *) set) + (i<<5)));
	}

	/* make p a strictly physical address */
	base = ((unsigned int) p) & 0x1fffffff;
	end = base + len;

	/* now process the tags searching for address within the range */
	for (i=0; i<SET_SZ; i++) {
		unsigned int index = i << 5;
		unsigned int field = data[i];
		unsigned int tag = field & ~0x3ff;
		unsigned int valid = field & 0x1;
		unsigned int dirty = field & 0x2;

		unsigned int phys = (tag | (index & 0x3ff)) & 0x1fffffff;
		unsigned int virt = (tag & ~0x3fff) | index;

		if (valid && ((phys+LINE_SZ) > base) && (phys < end)) {
			/*printk("Found unexpected cache data: "
			       "phys 0x%08x    virt 0x%08x    %s\n",
			       phys, virt, dirty ? "dirty" : "clean" );*/
			if (!listed_bounds) {
				printk("Bounds: set 0x%08x    base 0x%08x    end 0x%08x\n",
				       (unsigned) set, (unsigned) base, end);
				listed_bounds = 1;
			}

			printk ("%03d: field 0x%08x    phys 0x%08x    "
				"virt 0x%08x    %s %s\n",
				i, field, phys, virt,
				(valid ? "valid" : "invalid"),
				(dirty ? "dirty" : "clean"));
		}
	}
}

void sh4_cache_snoop(void *p, unsigned int len)
{
	unsigned int MM_OC_ADDRESS_ARRAY = 0xf4000000;
	void *set1 = (void *) (MM_OC_ADDRESS_ARRAY            );
	void *set2 = (void *) (MM_OC_ADDRESS_ARRAY + (1 << 14));

	if (!data) {
		data = kmalloc(512 * 4, GFP_KERNEL);
	}

	sh4_cache_snoop_set(set1, p, len);
	sh4_cache_snoop_set(set2, p, len);
}
