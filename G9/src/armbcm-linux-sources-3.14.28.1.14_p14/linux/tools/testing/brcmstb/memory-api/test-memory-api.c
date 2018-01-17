#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/brcmstb/memory_api.h>

static struct brcmstb_memory bm;

static int __init test_init(void)
{
	struct brcmstb_range *range;
	int ret;
	int i, j;

	ret = brcmstb_memory_get(&bm);
	if (ret) {
		pr_err("could not get memory struct\n");
		return ret;
	}

	/* print ranges */
	pr_info("Range info:\n");
	for (i = 0; i < MAX_BRCMSTB_MEMC; ++i) {
		pr_info(" memc%d\n", i);
		for (j = 0; j < bm.memc[i].count; ++j) {
			if (j >= MAX_BRCMSTB_RANGE) {
				pr_warn("  Need to increase MAX_BRCMSTB_RANGE!\n");
				break;
			}
			range = &bm.memc[i].range[j];
			pr_info("  %llu MiB at %#016llx\n",
					range->size / SZ_1M, range->addr);
		}
	}

	pr_info("lowmem info:\n");
	for (i = 0; i < bm.lowmem.count; ++i) {
		if (i >= MAX_BRCMSTB_RANGE) {
			pr_warn(" Need to increase MAX_BRCMSTB_RANGE!\n");
			break;
		}
		range = &bm.lowmem.range[i];
		pr_info(" %llu MiB at %#016llx\n",
				range->size / SZ_1M, range->addr);
	}

	pr_info("bmem info:\n");
	for (i = 0; i < bm.bmem.count; ++i) {
		if (i >= MAX_BRCMSTB_RANGE) {
			pr_warn(" Need to increase MAX_BRCMSTB_RANGE!\n");
			break;
		}
		range = &bm.bmem.range[i];
		pr_info(" %llu MiB at %#016llx\n",
				range->size / SZ_1M, range->addr);
	}

	pr_info("cma info:\n");
	for (i = 0; i < bm.cma.count; ++i) {
		if (i >= MAX_BRCMSTB_RANGE) {
			pr_warn(" Need to increase MAX_BRCMSTB_RANGE!\n");
			break;
		}
		range = &bm.cma.range[i];
		pr_info(" %llu MiB at %#016llx\n",
				range->size / SZ_1M, range->addr);
	}

	pr_info("reserved info:\n");
	for (i = 0; i < bm.reserved.count; ++i) {
		if (i >= MAX_BRCMSTB_RESERVED_RANGE) {
			pr_warn(" Need to increase MAX_BRCMSTB_RESERVED_RANGE!\n");
			break;
		}
		range = &bm.reserved.range[i];
		pr_info(" %#016llx-%#016llx\n",
				range->addr, range->addr + range->size);
	}

	return -EINVAL;
}

static void __exit test_exit(void)
{
	pr_info("Goodbye world\n");
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregory Fong (Broadcom Corporation)");

