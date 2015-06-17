#define __NO_VERSION__
/* to be in alsa-driver-specfici code */
#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#define vmalloc_32(x) vmalloc_nocheck(x)
#endif

#include "adriver.h"
#include "../../alsa-kernel/drivers/vx/vx_pcm.c"

