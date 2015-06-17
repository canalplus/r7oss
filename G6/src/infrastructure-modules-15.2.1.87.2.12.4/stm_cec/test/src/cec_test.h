/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/*
   @file     cec_test.h
   @brief

 */

#ifndef CEC_TEST_H_
#define CEC_TEST_H_

#define CEC_TEST_ERROR	1
#define CEC_TEST_DEBUG	0
#define CEC_TEST_INFO	1

#define CEC_TEST_API	1
#define ENABLE_HDMI_API	0

#if CEC_TEST_ERROR
#define cec_test_etrace(enable, fmt, ...)			\
do {								\
	if (enable) {						\
		pr_err("%s<%s:%d>: ", RED, __func__, __LINE__);	\
		pr_err(fmt,  ##__VA_ARGS__);			\
		pr_err("%s", NONE);				\
	}							\
} while (0)

#else
#define cec_test_etrace(enable, fmt, ...)		while (0)
#endif

#if CEC_TEST_DEBUG
#define cec_test_dtrace(enable, fmt, ...)			\
do {								\
	if (enable) {						\
		pr_info("%s<%s:%d>: ", CYAN, __func__, __LINE__);\
		pr_info(fmt,  ##__VA_ARGS__);			\
		pr_info("%s", NONE);				\
	}							\
} while (0)

#else
#define cec_test_dtrace(enable, fmt, ...)		while (0)
#endif

#if CEC_TEST_INFO
#define cec_test_itrace(enable, fmt, ...)			\
do {								\
	if (enable) {						\
		pr_info("<%s:%d>: ", __func__, __LINE__);	\
		pr_info(fmt,  ##__VA_ARGS__);			\
		pr_info("%s", NONE);				\
	}							\
} while (0)
#else
#define cec_test_itrace(enable, fmt, ...)		while (0)
#endif


void cec_test_modprobe_func(void);

#endif
