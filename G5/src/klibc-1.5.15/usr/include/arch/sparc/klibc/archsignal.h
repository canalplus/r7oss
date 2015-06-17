/*
 * arch/sparc/include/klibc/archsignal.h
 *
 * Architecture-specific signal definitions
 *
 */

#ifndef _KLIBC_ARCHSIGNAL_H
#define _KLIBC_ARCHSIGNAL_H

#define __WANT_POSIX1B_SIGNALS__
#include <asm/signal.h>

struct sigaction {
	__sighandler_t	sa_handler;
	unsigned long	sa_flags;
	void		(*sa_restorer)(void);	/* Not used by Linux/SPARC */
	sigset_t	sa_mask;
};

/* Not actually used by the kernel... */
#define SA_RESTORER	0x80000000

#endif
