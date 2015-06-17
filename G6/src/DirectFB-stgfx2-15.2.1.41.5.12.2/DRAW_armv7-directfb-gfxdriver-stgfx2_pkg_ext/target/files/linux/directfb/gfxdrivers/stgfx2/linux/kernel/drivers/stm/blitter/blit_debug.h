#ifndef __STM_BLIT_DEBUG_H__
#define __STM_BLIT_DEBUG_H__


#ifdef __KERNEL__
#include <linux/bug.h>
#  ifndef STM_BLITTER_ASSERT
#    define STM_BLITTER_ASSERT(cond)   BUG_ON(!(cond))
#  endif
#endif /* __KERNEL__ */

#define stm_blitter_magic(str) \
	(0                                                                   \
	 | ((((str)[sizeof(str) - 1]) | (((str)[sizeof(str) / 2]))) << 24)   \
	 | ((((str)[sizeof(str) / 3]) | (((str)[sizeof(str) / 4]))) << 16)   \
	 | ((((str)[sizeof(str) / 5]) | (((str)[sizeof(str) / 6]))) <<  8)   \
	 | ((((str)[sizeof(str) / 7]) | (((str)[sizeof(str) / 8]))) <<  0)   \
	)

#define stm_blitter_magic_assert(ptr, mag) \
	({ \
		STM_BLITTER_ASSERT((ptr) != NULL);                           \
		STM_BLITTER_ASSERT((ptr)->magic == stm_blitter_magic(#mag)); \
	})

#define stm_blitter_magic_set(ptr, mag) \
	({ \
		STM_BLITTER_ASSERT((ptr) != NULL);      \
		(ptr)->magic = stm_blitter_magic(#mag); \
	})

#define stm_blitter_magic_clear(ptr) \
	({ \
		STM_BLITTER_ASSERT((ptr) != NULL);      \
		STM_BLITTER_ASSERT((ptr)->magic != 0) ; \
		(ptr)->magic = 0;                       \
	})


#endif /* __STM_BLIT_DEBUG_H__ */
