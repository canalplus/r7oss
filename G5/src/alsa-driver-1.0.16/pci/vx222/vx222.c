#include "adriver.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#define CONFIG_USE_VXLOADER
#endif
#include "../../alsa-kernel/pci/vx222/vx222.c"
EXPORT_NO_SYMBOLS;
