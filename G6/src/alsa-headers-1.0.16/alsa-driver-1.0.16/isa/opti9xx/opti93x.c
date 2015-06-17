#include "adriver.h"
#include "../../alsa-kernel/isa/opti9xx/opti93x.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "opti93x.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
