/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cectest/cec_test.c
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*
   @file     cec_test.h
   @brief

 */

#ifndef CEC_TEST_H_
#define CEC_TEST_H_

#include <vibe_debug.h>

#define CEC_TEST_ERROR	1
#define CEC_TEST_DEBUG	0
#define CEC_TEST_INFO	1

#define CEC_TEST_API	1
#define ENABLE_HDMI_API	0

#if CEC_TEST_ERROR
#define cec_test_etrace(enable, fmt, ...)			\
do {								\
	if (1) {						\
		printk("\033[0;%dm<%s:%d>: ", RED, __func__, __LINE__);	\
		printk(fmt,  ##__VA_ARGS__);			\
		printk("\033[0m");				\
	}							\
} while (0)

#else
#define cec_test_etrace(enable, fmt, ...)		while (0)
#endif

#if CEC_TEST_DEBUG
#define cec_test_dtrace(enable, fmt, ...)			\
do {								\
	if (enable) {						\
		printk("\033[1;%dm<%s:%d>: ", CYAN, __func__, __LINE__);\
		printk(fmt,  ##__VA_ARGS__);			\
		printk("\033[0m");				\
	}							\
} while (0)

#else
#define cec_test_dtrace(enable, fmt, ...)		while (0)
#endif

#if CEC_TEST_INFO
#define cec_test_itrace(enable, fmt, ...)			\
do {								\
	if (1) {						\
		printk("<%s:%d>: ", __func__, __LINE__);	\
		printk(fmt,  ##__VA_ARGS__);			\
		printk("\033[0m");				\
	}							\
} while (0)
#else
#define cec_test_itrace(enable, fmt, ...)		while (0)
#endif


void cec_test_modprobe_func(void);

#endif
