/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
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
 @File   stm_common.h
 @brief



*/


#ifndef _STM_COMMON_H
#define _STM_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void			*stm_object_h;
typedef unsigned long        	stm_time_t;
/* The Linux headers are not C++ compatible and so we must hide them from
 * C++ compilers.
 */
#if defined(__KERNEL__) && !defined(__cplusplus)
#include "linux/compiler.h"
#include "linux/types.h"
#include <linux/wait.h>
typedef wait_queue_head_t			stm_event_wait_queue_t;
#else
#ifdef __GNUC__
#define __must_check __attribute__((warn_unused_result))
#else
#define __must_check
#endif
/* Dummy definition to ensure other infrastructure headers can be parsed */
typedef void            			stm_event_wait_queue_t;
typedef unsigned int				size_t;
#endif

typedef void (*stm_error_handler) (void *ctxt, stm_object_h bad_boy);

#ifdef __cplusplus
}
#endif

#endif /*_STM_COMMON_H*/
