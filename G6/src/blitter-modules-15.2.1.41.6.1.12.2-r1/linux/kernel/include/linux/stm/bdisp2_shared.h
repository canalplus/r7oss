#ifndef __BDISP2_SHARED_H__
#define __BDISP2_SHARED_H__

#include <linux/types.h>


enum stm_blitter_device_type {
	STM_BLITTER_VERSION_7109c3                = 3,
	STM_BLITTER_VERSION_7200c1                = 4,
	STM_BLITTER_VERSION_7200c2_7111_7141_7105 = 6,
	STM_BLITTER_VERSION_7106_7108             = 8,
	STM_BLITTER_VERSION_5206                  = 9,
	STM_BLITTER_VERSION_FLI75x0               = 11,

	STM_BLITTER_VERSION_STiH415_FLI7610       = 12,
	STM_BLITTER_VERSION_STiH416               = 13,
	STM_BLITTER_VERSION_STiH407               = 15,
	STM_BLITTER_VERSION_STiH205               = 16,
};


/* This structure is used to describe hardware configuration parameter
   Along with device implementation. */
struct stm_plat_blit_data {
	/* 100 */
	enum stm_blitter_device_type  device_type; /* BDisp implementation,
						      keep first for old lib
						      compatibility */
	/* 104 */
	unsigned int  nb_aq;                /* Nb AQ available for this host */
	/* 108 */
	unsigned int  nb_cq;                /* Nb CQ available for this host */
	/* 112 */
	unsigned int  line_buffer_length;   /* resize/filtering spans size*/
	/* 116 */
	unsigned int  mb_buffer_length;     /* mb line:should be equal to line
					       delay unless H/W bug (h416c1) */
	/* 120 */
	unsigned int  rotate_buffer_length; /* rotation spans max width */

	/* 124 */
	unsigned int  clk_ip_freq;

	/* 128 */
	unsigned char device_name[20];      /* /dev/"device_name" that will
					       be created */
};

#ifdef __KERNEL__
#  define const
#else
typedef struct {
	volatile int counter;
} atomic_t;
#endif

/* This structure is internally padded to be PAGE_SIZE bytes long, therefore
   it occupies the first PAGE_SIZE bytes of the shared area. We do this so as
   to be able to mmap() this first page cached and the remaining pages
   uncached. */
struct stm_bdisp2_shared_area {
	/* 0 */
	const unsigned long nodes_phys;

	/* 4 */
	unsigned long last_free; /* offset of last free node */

	/* 8,12,14,16 */
	unsigned long        bdisp_running; /* No more needed as completely relying
					       on STA, but kept for compatibility
					       with old lib */
	const unsigned short aq_index; /* which AQ is associated w/ this area,
					  first is 0 */
	unsigned short       updating_lna; /* No more needed as completely relying
					      on STA, but kept for compatibility
					      with old lib */
	unsigned long        prev_set_lna; /* previously set LNA */

	/* 20,24,28 */
	unsigned int  num_irqs; /* total number of IRQs (not only LNA + NODE) */
	unsigned int  num_lna_irqs;  /* total LNA IRQs */
	unsigned int  num_node_irqs; /* total node IRQs */

	/**** cacheline ****/

	/* 0x20 32 */
	unsigned int  num_idle;

	/* 36 */
	unsigned long next_free;

	/* 40,44 */
	unsigned int  num_wait_idle;
	unsigned int  num_wait_next;

	/* 48,52,56 */
	unsigned int  num_starts;
	unsigned int  num_ops_hi;
	unsigned int  num_ops_lo;

	/* 60 */
	const unsigned long nodes_size;

	/**** cacheline ****/

	/* 0x40 64 */
	unsigned long last_lut;

	/* 68,76 */
	unsigned long long bdisp_ops_start_time;
	unsigned long long ticks_busy;

	/* 84,88,89,90,92 */
	atomic_t      lock; /* a lock for concurrent access -> use with
			       atomic_cmpxchg() in kernel and
			       __sync_bool_compare_and_swap() in userspace */
	unsigned char locked_by; /* 0 == none, 2 == userspace, 3 == kernel,
				    1 == old version of this struct, it had
				    the version information here */
	unsigned char bdisp_suspended;  /* power status,
	                                   0 = Device is running
	                                   1 = Device is suspended by user
	                                   2 = device is suspended by RPM */
	unsigned char num_pending_requests;  /* pad 90 */
	unsigned char dummy;
	unsigned long next_ip;

	/**** cacheline ****/

	/* 0x60 96,100 */
	const unsigned long version; /* version of this structure */
	struct stm_plat_blit_data plat_blit; /* BDisp configuration params */
};

#ifdef __KERNEL__
#  undef const
#endif


#endif /* __BDISP2_SHARED_H__ */
