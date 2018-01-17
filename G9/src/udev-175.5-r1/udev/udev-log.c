#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "udev.h"

void udev_klog(const char *format,...)
{
	va_list args;

	va_start(args, format);
	udev_vklog(format, args);
	va_end(args);
}

void udev_vklog(const char *format, va_list args)
{
	int klog;
	char msg[1024];
	int size;

	klog = open("/dev/kmsg", O_WRONLY);
	if (klog < 0)
		return;

	size = vsnprintf(msg, sizeof(msg), format, args);
	if (size > 0)
    /* write() will return 0 if the kernel has no printk support. This causes
     * stdio functions to block, so we cannot use them. As we cannot rely on
     * the return value, we simply ignore it. */
		UDEV_IGNORE_VALUE(write(klog, msg, size));

	(void)close(klog);
}
