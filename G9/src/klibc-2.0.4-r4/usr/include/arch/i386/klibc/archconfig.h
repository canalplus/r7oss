/*
 * include/arch/i386/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

/* The i386 <asm/signal.h> is still not clean enough for this... */
#define _KLIBC_USE_RT_SIG 0

/* We have __libc_arch_init() */
#define _KLIBC_HAS_ARCHINIT 1

#endif				/* _KLIBC_ARCHCONFIG_H */
