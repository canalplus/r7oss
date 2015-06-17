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
 @File   stm_gearbox_debug.h
 @brief
*/
#ifndef STM_GEARBOX_DEBUG_H
#define STM_GEARBOX_DEBUG_H

#include "infra_debug.h"
#include "infra_os_wrapper.h"

#define STM_GEARBOX_ERROR 1
#define STM_GEARBOX_DEBUG 0

#if STM_GEARBOX_ERROR
#define gearbox_etrace(fmt, ...)  \
    pr_err("\n<%s:%d>::Error: " fmt "\n", __FUNCTION__, __LINE__,##__VA_ARGS__);

#else
#define gearbox_etrace(fmt, ...)   while(0)
#endif

#if STM_GEARBOX_DEBUG
#define gearbox_dtrace(fmt, ...)   \
    pr_info("\n<%s:%d>:: " fmt "\n", __FUNCTION__, __LINE__,##__VA_ARGS__);

#else
#define gearbox_dtrace(fmt, ...)   while(0)
#endif

#define CHECK_CALL_RETURN(x) \
do { \
	result = x; \
	if (result != 0) { \
		gearbox_dtrace("Failed:%d", result);\
		return result; \
	} \
} while(0)

#define CHECK_CALL_BREAK(x) \
do{ \
	result = x; \
	if (result != 0) { \
		gearbox_dtrace("Failed:%d", result);\
		break; \
	} \
} while(0)

#endif
