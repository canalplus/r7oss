#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include "ioctl32_old.c"
#else

#if defined(CONFIG_SPARC64) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 8)
#ifdef copy_in_user
#undef copy_in_user
#endif
size_t hack_copy_in_user(void __user *to, const void __user *from, size_t size)
{
	char tmp[64];
	while (size) {
		size_t s = sizeof(tmp) < size ? sizeof(tmp) : size;
		if (copy_from_user(tmp, from, s) || copy_to_user(to, tmp, s))
			break;
		size -= s;
		from += s;
		to += s;
	}
	return size;
}
#define copy_in_user hack_copy_in_user
#endif /* SPARC64 && < 2.6.8 */

#include "ioctl32_new.c"

#endif /* 2.6.0 */

