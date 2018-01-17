#ifndef _KLIBC_ARCHSTAT_H
#define _KLIBC_ARCHSTAT_H

#include <klibc/stathelp.h>

#define _STATBUF_ST_NSEC

struct stat {
	__stdev64	(st_dev);
	unsigned long	st_ino;
	unsigned long	st_nlink;

	unsigned int	st_mode;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	__pad0;

	__stdev64 (st_rdev);
	long		st_size;
	long		st_blksize;
	long		st_blocks;

	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;

	unsigned long __unused[3];
};

#endif
