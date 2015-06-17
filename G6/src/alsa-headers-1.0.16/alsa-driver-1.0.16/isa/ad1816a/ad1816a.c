#include "adriver.h"
#include "../../alsa-kernel/isa/ad1816a/ad1816a.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "ad1816a.isapnp"
#endif
EXPORT_NO_SYMBOLS;
#endif
