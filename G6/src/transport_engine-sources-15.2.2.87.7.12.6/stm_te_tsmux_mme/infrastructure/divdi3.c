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

	if (unlikely(d & 0xffffffff00000000LL))
		panic("Need true 64-bit/64-bit division");

	res = __xdiv64_32(n, (u32)d);
	if (c)
		res = -res;

	return res;
}
