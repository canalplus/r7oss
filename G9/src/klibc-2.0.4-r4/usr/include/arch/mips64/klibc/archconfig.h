/*
 * include/arch/mips64/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

/* MIPS has nonstandard socket definitions */
#define _KLIBC_HAS_ARCHSOCKET_H 1

/* We can use RT signals on MIPS */
#define _KLIBC_USE_RT_SIG 1

#endif				/* _KLIBC_ARCHCONFIG_H */
