#define __NO_VERSION__
#include "adriver.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#include "pmac_old.c"
#else
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
/* hack for machine_is(powermac) */
static int _machine_is(void);
#define machine_is(x)	_machine_is()
#endif
#include "ppc-prom-hack.h"
#include "../alsa-kernel/ppc/pmac.c"
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)
static int _machine_is(void) { return _machine == _MACH_Pmac; }
#endif
#endif
