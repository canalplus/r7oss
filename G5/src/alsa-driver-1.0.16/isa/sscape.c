#include "adriver.h"
#include "../alsa-kernel/isa/sscape.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "sscape.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
