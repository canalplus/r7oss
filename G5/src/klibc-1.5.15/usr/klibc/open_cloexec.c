/*
 * open_cloexec.c
 *
 * A quick hack to do an open() and set the cloexec flag
 */

#include <unistd.h>
#include <fcntl.h>

int open_cloexec(const char *path, int flags, mode_t mode)
{
	int fd = open(path, flags, mode);

	if (fd >= 0)
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	return fd;
}
