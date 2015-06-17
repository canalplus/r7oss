#include "adriver.h"
#include "../alsa-kernel/isa/es18xx.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "es18xx.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
