#include "adriver.h"
#include "../alsa-kernel/isa/azt2320.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "azt2320.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
