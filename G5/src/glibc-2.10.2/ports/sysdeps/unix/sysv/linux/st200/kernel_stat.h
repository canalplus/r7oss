/* definition of "struct stat" from the kernel, version 2.5.66. */
/* The st_atim, st_mtim and st_ctim fields have
   different types compared to the kernel version, but their
   layout matches the kernel version.  This allows
   the "linux generic" code in xconvstat.c to be used.  */

struct kernel_stat {
	unsigned long	st_dev;
	unsigned long	st_ino;
	unsigned long	st_nlink;
	unsigned int	st_mode;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned int	__pad0;
	unsigned long	st_rdev;
	unsigned long	st_size;
        struct timespec st_atim;
        struct timespec st_mtim;
        struct timespec st_ctim;
	unsigned long	st_blksize;
	long		st_blocks;
	unsigned long	__unused[3];
};

#define _HAVE_STAT_NSEC
#define _HAVE_STAT64_NSEC
