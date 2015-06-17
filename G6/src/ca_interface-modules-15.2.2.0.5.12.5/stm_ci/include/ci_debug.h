/*****************************************************************************/
/* COPYRIGHT(C)2011 STMicroelectronics -All Rights Reserved		     */
/* ST makes no warranty	express	or implied including but not limited to,     */
/* any warranty	of							     */
/*									     */
/*   (i)  merchantability or fitness for a particular purpose and/or	     */
/*   (ii) requirements,	for a particular purpose in relation to	the LICENSED */
/*	  MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*	  represent or warrant that the	LICENSED MATERIALS provided here     */
/*	  under	is free	of infringement	of any third party patents,	     */
/*	  copyrights,trade secrets or other intellectual property rights.    */
/*	  ALL WARRANTIES, CONDITIONS OR	OTHER TERMS IMPLIED BY LAW ARE	     */
/*	  EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW		     */
/*									     */
/*****************************************************************************/
/**
 @File	 ca_debug.h
 @brief



*/

#ifndef	_CI_DEBUG_H_
#define	_CI_DEBUG_H_

#include "ca_debug.h"

#define	CI_ERROR	1
#define	CI_DEBUG	0

typedef	enum {
	CI_API_RUNTIME = 1,
	CI_UTILS_RUNTIME,
	CI_INTERNAL_RUNTIME,
	CI_HAL_RUNTIME,
	CI_PROTOCOL_RUNTIME,
	CI_RUNTIME_ID_MAX
} ci_debug_runtime_t;

#define	RED   "\033[0;31m"
#define	CYAN  "\033[1;36m"
#define	GREEN "\033[4;32m"
#define	BLUE  "\033[9;34m"
#define	NONE   "\033[0m"
#define	BROWN  "\033[0;33m"
#define	MAGENTA	 "\033[0;35m"

#if CI_ERROR
#define	ci_error_trace(enable, fmt, ...)		\
	do { \
		if (enable) { \
			printk("%s<%s:%d>:", RED, __FUNCTION__, __LINE__); \
			printk(fmt,  ##__VA_ARGS__); printk("%s", NONE);\
		} \
	} while(0)
#else
#define	ci_debug_trace(enable, fmt, ...)		while(0)
#endif

#if CI_DEBUG
#define	ci_debug_trace(enable, fmt, ...)		\
	do { \
		if (enable) { \
			printk("%s<%s:%d>:", CYAN, __FUNCTION__, __LINE__); \
			printk(fmt,  ##__VA_ARGS__); printk("%s", NONE);\
		} \
	} while (0)
#else
#define	ci_debug_trace(enable, fmt, ...)		while(0)
#endif

#define	ci_test_error_trace(enable, fmt, ...)	ca_test_error_trace(INFRA_CI_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
#define	ci_test_debug_trace(enable, fmt, ...)	ca_test_debug_trace(INFRA_CI_DEBUG_ID, enable, fmt, ##__VA_ARGS__)

#endif
