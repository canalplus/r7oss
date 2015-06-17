
#include <linux/fs.h>
#include <linux/string.h>
#include "infra_os_wrapper.h"
#include "stm_memsrc.h"
#include "stm_memsink.h"
#include "stm_data_interface.h"
#include "stm_registry.h"

#include "mss_test_producer.h"

#ifndef MSS_MAGIC_NO
#define MSS_MAGIC_NO   'l'
#endif

enum {
	IOCTL_SINK_CONNECT = 0x1,
	IOCTL_SINK_DISCONNECT
};

#define MSS_SINK_CONNECT	 _IOWR(MSS_MAGIC_NO, IOCTL_SINK_CONNECT, uint32_t)
#define MSS_SINK_DISCONNECT	 _IOWR(MSS_MAGIC_NO, IOCTL_SINK_DISCONNECT, uint32_t)


#define MEMSRC_NAME "mss_src0"
#define MEMSINK_NAME "mss_sink0"

#define MSS_DEBUG 1
#define MSS_ERROR  1

#if MSS_ERROR
#define mss_error_print(fmt, ...)	  pr_err(fmt,  ##__VA_ARGS__)
#else
#define mss_error_print(fmt, ...)	  do {} while (0)
#endif

#if MSS_DEBUG
#define mss_debug_print(fmt, ...)		pr_info(fmt,  ##__VA_ARGS__)
#else
#define mss_debug_print(fmt, ...)		do {} while (0)
#endif

int32_t mss_register_char_driver(void);
void mss_unregister_char_driver(void);


