#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#include <pcmcia/cs_types.h>
static void cs_error(client_handle_t handle, int func, int ret);
#define CONFIG_USE_VXLOADER
#endif
