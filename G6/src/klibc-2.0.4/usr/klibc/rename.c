#include <fcntl.h>
#include <stdio.h>

#ifndef __NR_rename

int rename(const char *oldpath, const char *newpath)
{
	return renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

#endif /* __NR_rename */
