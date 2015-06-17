/*
 * include/arch/i386/klibc/archconfig.h
 *
 * See include/klibc/sysconfig.h for the options that can be set in
 * this file.
 *
 */

#ifndef _KLIBC_ARCHCONFIG_H
#define _KLIBC_ARCHCONFIG_H

/* On i386, only half the signals are accessible using the legacy calls. */
#define _KLIBC_USE_RT_SIG 1

#endif				/* _KLIBC_ARCHCONFIG_H */
