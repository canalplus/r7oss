#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#ifndef __NR_stat

int stat(const char *path, struct stat *buf)
{
	return fstatat(AT_FDCWD, path, buf, 0);
}

#endif /* __NR_stat */
