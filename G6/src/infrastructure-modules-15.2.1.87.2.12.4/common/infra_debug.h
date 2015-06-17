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
 @File   infra_debug.h
 @brief



*/

#ifndef INFRA_DEBUG_H_
#define INFRA_DEBUG_H_

#define INFRA_ERROR_TRACE_COMPILE	1
#define INFRA_ERROR_TRACE_RUNTIME	0
#define INFRA_DEBUG_TRACE_COMPILE	0
#define INFRA_DEBUG_TRACE_RUNTIME	0
#define INFRA_TEST_ERROR_TRACE		1
#define INFRA_TEST_DEBUG_TRACE		1

typedef enum {
	INFRA_WRAPPER_DEBUG_ID = 0,
	INFRA_EVENT_DEBUG_ID ,
	INFRA_REGISTRY_DEBUG_ID,
	INFRA_MSS_DEBUG_ID,
	INFRA_SCR_DEBUG_ID,
	INFRA_CEC_DEBUG_ID,
	INFRA_DEBUG_ID_MAX
}infra_debug_id_t;

static inline __attribute__ ((format (printf, 1, 2)))
int no_print(const char *s, ...)
{
        return 0;
}

#define infra_trace_compile(enable, fmt, ...)	do { \
							if(enable){ \
								printk("<%s:%d>:: ",__FUNCTION__, __LINE__); \
								printk(fmt,  ##__VA_ARGS__); \
							} \
						} while(0)

#define infra_trace_runtime(id, enable, fmt, ...)	do { \
							if(infra_arr_debug[id][##enable##_RUNTIME]) {\
								printk("<%s:%d>:: ",__FUNCTION__, __LINE__); \
								printk(fmt,  ##__VA_ARGS__); \
							} \
						} while(0)


#if INFRA_ERROR_TRACE_COMPILE
#define infra_error_trace(id, enable, fmt, ...)	infra_trace_compile(enable, fmt, ...)
#elif INFRA_ERROR_TRACE_RUNTIME
#define infra_error_trace(id, enable, fmt, ...)	infra_trace_runtime(id, enable, fmt, ...)
#else
#define infra_error_trace(id, enable, fmt, ...) 	while(0)
#endif



#if INFRA_DEBUG_TRACE_COMPILE
#define infra_debug_trace(id, enable, fmt, ...)		infra_trace_compile(enable, fmt, ...)
#elif INFRA_ERROR_TRACE_RUNTIME
#define infra_debug_trace(id, enable, fmt, ...)		infra_trace_runtime(id, enable, fmt, ...)
#else
#define infra_debug_trace(id, enable, fmt, ...) 	while(0)
#endif

#if INFRA_TEST_ERROR_TRACE
#define infra_test_error_trace(id, enable, fmt, ...)	infra_trace_compile(enable, fmt, ...)
#else
#define infra_test_error_trace(id, enable, fmt, ...) 	while(0)
#endif

#if INFRA_TEST_DEBUG_TRACE
#define infra_test_debug_trace(id, enable, fmt, ...)	infra_trace_compile(enable, fmt, ...)
#else
#define infra_test_debug_trace(id, enable, fmt, ...) 	while(0)
#endif


#endif /*INFRA_DEBUG_H_*/
