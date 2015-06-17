/*
 * include/arch/sparc64/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

#define _KLIBC_USE_RT_SIG 1	/* Use rt_* signals */
#define _KLIBC_NEEDS_SA_RESTORER 1 /* Need a restorer function */

#endif				/* _KLIBC_ARCHCONFIG_H */
