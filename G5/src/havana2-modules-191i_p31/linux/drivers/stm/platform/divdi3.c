/*
 * Simple __divdi3 function which doesn't use FPU.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>

extern u64 __xdiv64_32(u64 n, u32 d);

s64 __divdi3(s64 n, s64 d)
{
	int c = 0;
	s64 res;

	if (n < 0LL) {
		c = ~c;
		n = -n;
	}
	if (d < 0LL) {
		c = ~c;
		d = -d;
	}

	if (unlikely(d & 0xffffffff00000000LL)) {
		printk("Approximating 64-bit/64-bit division : %llu / %llu\n", n, d);
		while (d & 0xffffffff00000000ULL) {
			n /= 2;
			d /= 2;
		}
	}

	res = __xdiv64_32(n, (u32)d);
	if (c) {
		res = -res;
	}
	return res;
}

#if defined (CONFIG_KERNELVERSION) /* STLinux 2.3 or later */
EXPORT_SYMBOL(__divdi3);
#endif

MODULE_DESCRIPTION("Player2 64Bit Divide Driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.9");
MODULE_LICENSE("GPL");
