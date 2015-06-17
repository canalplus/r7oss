#ifndef _ASM_SH_RESTART_H
#define _ASM_SH_RESTART_H

#ifdef __KERNEL__

#include <linux/list.h>

struct restart_prep_handler {
	struct list_head list;
	void (*prepare_restart)(void);
};

/* Register a function to be run immediately before a restart. */
void register_prepare_restart_handler(void (*prepare_restart)(void));

#endif /* __KERNEL__ */
#endif /* _ASM_SH_RESTART_H */
