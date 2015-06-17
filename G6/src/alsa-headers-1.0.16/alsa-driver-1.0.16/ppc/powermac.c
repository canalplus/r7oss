#include "adriver.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#define CONFIG_ADB_CUDA
#define CONFIG_ADB_PMU
#endif

#include "../alsa-kernel/ppc/powermac.c"
EXPORT_NO_SYMBOLS;
