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
   @file     evt_test.h
   @brief

 */

#ifndef EVT_TEST_H_
#define EVT_TEST_H_


#define EVT_TEST_ERROR				1
#define EVT_TEST_DEBUG				0

#define RED   "\033[0;31m"
#define CYAN  "\033[1;36m"
#define GREEN "\033[4;32m"
#define BLUE  "\033[9;34m"
#define NONE   "\033[0m"
#define BROWN  "\033[0;33m"
#define MAGENTA  "\033[0;35m"

#define EVT_TEST_L1			1
#define EVT_TEST_L2			1

#if EVT_TEST_ERROR
#define evt_test_etrace(enable, fmt, ...)		do { \
								if (enable) { \
									pr_err("%s<%s:%d>:: ", RED, __FUNCTION__, __LINE__); \
									pr_err(fmt,  ##__VA_ARGS__); pr_err("%s", NONE);\
								} \
							} while (0)
#else
#define evt_test_etrace(enable, fmt, ...)		do {} while (0)
#endif

#if EVT_TEST_DEBUG
#define evt_test_dtrace(enable, fmt, ...)		do { \
								if (enable) { \
									pr_info("%s<%s:%d>:: ", CYAN, __FUNCTION__, __LINE__); \
									pr_info(fmt,  ##__VA_ARGS__); pr_info("%s", NONE);\
								} \
							} while (0)
#else
#define evt_test_dtrace(enable, fmt, ...)		do {} while (0)
#endif

void evt_test_modprobe_func(void);

#endif
