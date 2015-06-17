#ifndef _LINUX_ERR_H
#define _LINUX_ERR_H

/* on earlier kernels, adriver.h already gets this through linux/fs.h */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)

#define IS_ERR_VALUE(x) ((x) > (unsigned long)-1000L)

static inline void *ERR_PTR(long error)
{
	return (void *) error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline long IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

#endif

#endif /* _LINUX_ERR_H */
