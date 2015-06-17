#include <linux/config.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,15)
#include "vxpocket_old.c"
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16)
#include "vxpocket-2.6.16.c"
#else
#include "adriver.h"
#include "../../alsa-kernel/pcmcia/vx/vxpocket.c"
#endif

