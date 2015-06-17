#ifndef __BDISP2_SHARED_IOCTL_H__
#define __BDISP2_SHARED_IOCTL_H__

#include <linux/ioctl.h>

#include <linux/stm/bdisp2_shared.h>

/* returns the cookie for mmap()ing a 'struct stm_bdisp2_shared_area',
   which is used for communication w/ the BDisp IRQ handler */
struct stm_bdisp2_shared_cookie {
	__u32  cookie;
	size_t size;
};


#define STM_BDISP2_GET_SHARED_AREA _IOR ('B', 0x18, struct stm_bdisp2_shared_cookie)
#define STM_BDISP2_WAIT_NEXT       _IO  ('B', 0x19) /* similar to
						       STM_BLITTER_SYNC, but
						       will wakeup upon next
						       node interrupt, not
						       necessarily LNA */

#define STM_BLITTER_GET_BLTLOAD    _IOR ('B', 0x1a, unsigned long)
#define STM_BLITTER_GET_BLTLOAD2   _IOR ('B', 0x1a, unsigned long long)
#define STM_BLITTER_SYNC           _IO  ('B', 0x5) /* wait for blitter idle */
#define STM_BLITTER_WAKEUP         _IO  ('B', 0x6) /* wakeup blitter */


#endif /* __BDISP2_SHARED_IOCTL_H__ */
