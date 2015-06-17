#include "adriver.h"
#include "../alsa-kernel/isa/dt019x.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "dt019x.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
