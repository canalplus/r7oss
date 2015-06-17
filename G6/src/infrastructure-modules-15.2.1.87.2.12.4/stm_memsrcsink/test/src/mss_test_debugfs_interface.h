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
 *    @file     mss_test.h
 *       @brief
 *
 *
 *
 */

#ifndef MSS_TEST_H_
#define MSS_TEST_H_

#include "mss_test_debugfs.h"

#include "infra_os_wrapper.h"
#include "stm_memsrc.h"
#include "stm_memsink.h"
#include "stm_registry.h"

#if 0

#define MSS_TEST_ERROR	1
#define MSS_TEST_DEBUG	0

#define MSS_TEST_API			0

#define ENABLE_HDMI_API			0

#if MSS_TEST_ERROR
#define mss_test_etrace(enable, fmt, ...)		do { \
								if (enable) { \
									pr_err("%s<%s:%d>:: ", RED, __FUNCTION__, __LINE__); \
									pr_err(fmt,  ##__VA_ARGS__); pr_err("%s", NONE);\
								} \
							} while (0)
#else
#define mss_test_etrace(enable, fmt, ...)		do {} while (0)
#endif

#if MSS_TEST_DEBUG
#define mss_test_dtrace(enable, fmt, ...)		do { \
															    1, 1		  Top

#endif


/* struct dentry * memsrcsink_create_dbgfs(void); */
/* void memsrcsink_delete_dbgfs(void); */
#endif


#endif
