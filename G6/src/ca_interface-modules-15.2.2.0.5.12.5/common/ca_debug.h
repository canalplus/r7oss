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
 @File   ca_debug.h
 @brief



*/

#ifndef CA_DEBUG_H_
#define CA_DEBUG_H_

#define CA_ERROR_TRACE_COMPILE	1
#define CA_DEBUG_TRACE_COMPILE	0
#define CA_TEST_ERROR_TRACE	1
#define CA_TEST_DEBUG_TRACE	1


static inline __attribute__ ((format (printf, 1, 2)))
int no_print(const char *s, ...)
{
        return 0;
}

#define ca_trace_compile(enable, fmt, ...) \
do { \
	if(enable){ \
		printk("<%s:%d>:: ",__FUNCTION__, __LINE__); \
		printk(fmt,  ##__VA_ARGS__); \
	} \
} while(0)



#if CA_ERROR_TRACE_COMPILE
#define ca_error_trace(id, enable, fmt, ...) \
	ca_trace_compile(enable, fmt, ...)
#else
#define ca_error_trace(id, enable, fmt, ...) 	while(0)
#endif



#if CA_DEBUG_TRACE_COMPILE
#define ca_debug_trace(id, enable, fmt, ...) \
	ca_trace_compile(enable, fmt, ...)
#else
#define ca_debug_trace(id, enable, fmt, ...) 	while(0)
#endif

#if CA_TEST_ERROR_TRACE
#define ca_test_error_trace(id, enable, fmt, ...) \
	ca_trace_compile(enable, fmt, ...)
#else
#define ca_test_error_trace(id, enable, fmt, ...) 	while(0)
#endif

#if CA_TEST_DEBUG_TRACE
#define ca_test_debug_trace(id, enable, fmt, ...) \
	ca_trace_compile(enable, fmt, ...)
#else
#define ca_test_debug_trace(id, enable, fmt, ...) 	while(0)
#endif

#endif /*CA_DEBUG_H_*/
