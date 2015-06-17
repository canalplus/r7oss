#include "adriver.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define HDSP_USE_HWDEP_LOADER
#endif
#include "../../alsa-kernel/pci/rme9652/hdsp.c"
EXPORT_NO_SYMBOLS;
