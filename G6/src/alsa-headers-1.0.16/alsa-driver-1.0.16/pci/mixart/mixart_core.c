#define __NO_VERSION__

#include "adriver.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#ifdef __i386__
#define __raw_readl(x) readl(x)
#define __raw_writel(x,y) writel(x,y)
#else
#warning This architecture will not work correctly!
#endif
#endif

#include "../../alsa-kernel/pci/mixart/mixart_core.c"
