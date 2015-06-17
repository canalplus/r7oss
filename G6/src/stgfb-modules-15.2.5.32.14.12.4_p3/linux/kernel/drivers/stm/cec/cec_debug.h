/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cec/cec_debug.h
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
/*
 @File   cec_debug.h
 @brief



*/

#ifndef _CEC_DEBUG_H_
#define _CEC_DEBUG_H_

#include <vibe_debug.h>

#define CEC_ERROR	1
#define CEC_DEBUG	0


#define CEC_API		1
#define CEC_UTILS	0
#define CEC_HW		0

typedef enum {
	CEC_API_RUNTIME=0,
	CEC_UTILS_RUNTIME,
	CEC_HW_RUNTIME,
	CEC_RUNTIME_ID_MAX
}cec_debug_runtime_t;

#if CEC_ERROR
#define cec_error_trace(enable, fmt, ...)		do { \
								if(enable){ \
									printk("\033[0;%dm<%s:%d>:: ",RED,__FUNCTION__, __LINE__); \
									printk(fmt,  ##__VA_ARGS__); printk("\033[0m");\
								} \
							} while(0)
#else
#define cec_error_trace(enable, fmt, ...)		while(0)
#endif

#if CEC_DEBUG
#define cec_debug_trace(enable, fmt, ...)		do { \
								if(enable){ \
									printk("\033[1;%dm<%s:%d>:: ",CYAN,__FUNCTION__, __LINE__); \
									printk(fmt,  ##__VA_ARGS__); printk("\033[0m");\
								} \
							} while(0)
#else
#define cec_debug_trace(enable, fmt, ...)		while(0)
#endif

#endif
