/* to be in alsa-driver-specfici code */
#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#define spin_lock_bh spin_lock
#define spin_unlock_bh spin_unlock
#endif

#include "adriver.h"
#include "../../alsa-kernel/drivers/vx/vx_core.c"
