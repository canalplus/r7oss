#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include "seq32_old.c"
#else

#if defined(CONFIG_SPARC64) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 8)
#ifdef copy_in_user
#undef copy_in_user
#endif
size_t hack_copy_in_user(void __user *to, const void __user *from, size_t size);
#define copy_in_user hack_copy_in_user
#endif

#include "seq32_new.c"

#endif
