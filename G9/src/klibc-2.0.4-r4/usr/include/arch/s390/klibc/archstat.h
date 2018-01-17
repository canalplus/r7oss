#ifndef _KLIBC_ARCHSTAT_H
#define _KLIBC_ARCHSTAT_H

#include <klibc/stathelp.h>

#define _STATBUF_ST_NSEC

#ifndef __s390x__

/* This matches struct stat64 in glibc2.1, hence the absolutely
 * insane amounts of padding around dev_t's.
 */
struct stat {
	__stdev64	(st_dev);
	unsigned int	__pad1;
#define STAT64_HAS_BROKEN_ST_INO	1
	unsigned long	__st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned long	st_uid;
	unsigned long	st_gid;
	__stdev64	(st_rdev);
	unsigned int	__pad3;
	long long	st_size;
	unsigned long	st_blksize;
	unsigned char	__pad4[4];
	unsigned long	__pad5;     /* future possible st_blocks high bits */
	unsigned long	st_blocks;  /* Number 512-byte blocks allocated. */
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	unsigned long long	st_ino;
};

#else /* __s390x__ */

struct stat {
	__stdev64	(st_dev);
	unsigned long	st_ino;
	unsigned long	st_nlink;
	unsigned int	st_mode;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	__pad1;
	__stdev64	(st_rdev);
	unsigned long	st_size;
	struct timespec	st_atim;
	struct timespec	st_mtim;
	struct timespec	st_ctim;
	unsigned long	st_blksize;
	long		st_blocks;
	unsigned long	__unused[3];
};

#endif /* __s390x__ */
#endif
