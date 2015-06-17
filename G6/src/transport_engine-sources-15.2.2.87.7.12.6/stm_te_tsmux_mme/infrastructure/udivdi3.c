/*
 * Simple __udivdi3 function which doesn't use FPU.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>

extern u64 __xdiv64_32(u64 n, u32 d);

u64 __udivdi3(u64 n, u64 d)
{
	if (unlikely(d & 0xffffffff00000000ULL))
		panic("Need true 64-bit/64-bit division");
	return __xdiv64_32(n, (u32)d);
}
