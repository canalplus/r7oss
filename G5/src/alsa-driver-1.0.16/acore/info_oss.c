#define __NO_VERSION__
#include "adriver.h"
#include <linux/version.h>
#ifndef CONFIG_HAVE_INIT_UTSNAME
#define init_utsname()	(&system_utsname)
#endif
#include "../alsa-kernel/core/info_oss.c"
