#ifndef __BLITTER_OS_H__
#define __BLITTER_OS_H__

#ifdef __KERNEL__

#include <linux/slab.h>

#define stm_blitter_allocate_mem(size) kmalloc(size, GFP_KERNEL)
#define stm_blitter_free_mem(ptr)      kfree(ptr)

#else /* __KERNEL__ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#define stm_blitter_allocate_mem(size) malloc(size)
#define stm_blitter_free_mem(ptr)      free(ptr)

#endif /* __KERNEL__ */


#endif /* __BLITTER_OS_H__ */
