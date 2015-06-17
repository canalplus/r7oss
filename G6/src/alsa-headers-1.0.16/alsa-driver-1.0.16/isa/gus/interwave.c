#include "adriver.h"
#include "../../alsa-kernel/isa/gus/interwave.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "interwave.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
