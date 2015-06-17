/*
 * of_get_property() wrapper
 */

#include "adriver.h"
#include <asm/prom.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 21)
#define of_get_property		get_property
#endif
