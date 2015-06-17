/*
 * include/arch/arm/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

/* newer arm arch support bx instruction */
#if (!defined(__ARM_ARCH_2__) && !defined(__ARM_ARCH_3__) \
	&& !defined(__ARM_ARCH_3M__) && !defined(__ARM_ARCH_4__))
# define _KLIBC_ARM_USE_BX 1
#endif

/* Use rt_* signals */
#define _KLIBC_USE_RT_SIG 1

#endif				/* _KLIBC_ARCHCONFIG_H */
