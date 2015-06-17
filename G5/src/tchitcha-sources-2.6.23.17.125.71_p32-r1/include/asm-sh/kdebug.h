#ifndef __ASM_SH_KDEBUG_H
#define __ASM_SH_KDEBUG_H

#include <linux/notifier.h>

/*
 * These are only here because kprobes.c wants them to implement a
 * blatant layering violation.  Will hopefully go away soon once all
 * architectures are updated.
 */
static inline int register_page_fault_notifier(struct notifier_block *nb)
{
        return 0;
}
static inline int unregister_page_fault_notifier(struct notifier_block *nb)
{
        return 0;
}

/* Grossly misnamed. */
enum die_val {
	DIE_TRAP,
};

#endif /* __ASM_SH_KDEBUG_H */
