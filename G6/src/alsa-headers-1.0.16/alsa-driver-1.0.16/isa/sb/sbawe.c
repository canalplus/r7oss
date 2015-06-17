#include "adriver.h"
#include "../../alsa-kernel/isa/sb/sbawe.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "sbawe.isapnp"
#endif
#endif
