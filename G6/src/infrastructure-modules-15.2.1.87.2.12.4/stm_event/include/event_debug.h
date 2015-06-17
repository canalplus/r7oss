/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   event_debug.h
 @brief



*/

#ifndef _EVENT_DEBUG_H_
#define _EVENT_DEBUG_H_

#include "infra_debug.h"

#define EVENT_ERROR	1
#define EVENT_DEBUG	0


#define EVENT_API		1
#define EVENT_SUBSCRIBER	0
#define EVENT_SIGNALER		0
#define EVENT_ASYNC_MANAGER	0

typedef enum {
	EVENT_API_RUNTIME = 0,
	EVENT_UTILS_RUNTIME,
	EVENT_HW_RUNTIME,
	EVENT_RUNTIME_ID_MAX
} evt_debug_runtime_t;

#define RED   "\033[0;31m"
#define CYAN  "\033[1;36m"
#define GREEN "\033[4;32m"
#define BLUE  "\033[9;34m"
#define NONE   "\033[0m"
#define BROWN  "\033[0;33m"
#define MAGENTA  "\033[0;35m"

/*
#define evt_error_trace(enable, fmt, ...)		infra_error_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
#define evt_debug_trace(enable, fmt, ...)		infra_debug_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
#define evt_test_error_trace(enable, fmt, ...)		infra_test_error_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
#define evt_test_debug_trace(enable, fmt, ...)		infra_test_debug_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
*/

#if EVENT_ERROR
#define evt_error_trace(enable, fmt, ...)		do { \
								if (enable) { \
									pr_err("%s<%s:%d>:: ", RED, __FUNCTION__, __LINE__); \
									pr_err(fmt,  ##__VA_ARGS__); pr_err("%s", NONE);\
								} \
							} while (0)
#else
#define evt_error_trace(enable, fmt, ...)		do {} while (0)
#endif

#if EVENT_DEBUG
#define evt_debug_trace(enable, fmt, ...)		do { \
								if (enable) { \
									printk("%s<%s:%d>:: ", CYAN, __FUNCTION__, __LINE__); \
									printk(fmt,  ##__VA_ARGS__); printk("%s", NONE);\
								} \
							} while (0)
#else
#define evt_debug_trace(enable, fmt, ...)		do {} while (0)
#endif

#define evt_test_error_trace(enable, fmt, ...)		infra_test_error_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)
#define evt_test_debug_trace(enable, fmt, ...)		infra_test_debug_trace(INFRA_EVENT_DEBUG_ID, enable, fmt, ##__VA_ARGS__)

#endif
