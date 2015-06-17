#include "adriver.h"
#include "../../alsa-kernel/isa/gus/interwave-stb.c"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#ifndef __isapnp_now__
#include "interwave-stb.isapnp"
#endif
#endif
