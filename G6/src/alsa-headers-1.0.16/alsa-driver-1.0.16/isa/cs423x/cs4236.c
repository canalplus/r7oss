#include "adriver.h"
#include "../../alsa-kernel/isa/cs423x/cs4236.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "cs4236.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
