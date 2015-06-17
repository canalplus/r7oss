#ifndef _MSS_TEST_H
#define _MSS_TEST_H


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/current.h>

#include "mss_test_debugfs.h"

#include "infra_os_wrapper.h"
#include "stm_memsrc.h"
#include "stm_memsink.h"
#include  "stm_data_interface.h"
#include "stm_registry.h"
#include "stm_event.h"


#define RED   "\033[0;31m"
#define CYAN  "\033[1;36m"
#define GREEN "\033[4;32m"
#define BLUE  "\033[9;34m"
#define NONE   "\033[0m"
#define BROWN  "\033[0;33m"
#define MAGENTA  "\033[0;35m"

#define PUSH_BYTE	10
#define PULL_BYTE	20

/* TODO : Add proper debug traces mechanism */

#define MSS_TEST_ERROR	1
#define MSS_TEST_DEBUG	0

#define MSS_TEST_CONSUMER		1
#define MSS_TEST_PRODUCER		1
#define MSS_TEST_MODULE			1
#define MSS_TEST_USER_INTERFACE		1

#ifdef MSS_TEST_ERROR
#define mss_test_etrace(enable, fmt, ...)	       do { \
								if (enable) { \
									pr_err("%s<%s:%d>:: ", NONE, __FUNCTION__, __LINE__); \
									pr_err(fmt,  ##__VA_ARGS__); pr_err("%s", NONE);\
								} \
							} while (0)
#else
#define mss_test_etrace(enable, fmt, ...)		do {} while (0)
#endif


#ifdef MSS_TEST_DEBUG
#define mss_test_dtrace(enable, fmt, ...)		do { \
								if (enable) { \
									pr_info("%s<%s:%d>:: ", NONE, __FUNCTION__, __LINE__); \
									pr_info(fmt,  ##__VA_ARGS__); pr_info("%s", NONE);\
								} \
							} while (0)
#else
#define mss_test_dtrace(enable, fmt, ...)		do {} while (0)
#endif


int create_srcsinkobjects_i(int i, struct dentry *d_memsrcsink);
int create_producer_sink_objects(stm_object_h *producer_p, stm_memsink_h *snk_p, stm_memory_iomode_t snk_mode, char *snk_name, int api);
int create_consumer_src_objects(stm_object_h *consumer_p, stm_memsrc_h *src_p, stm_memory_iomode_t src_mode, char *src_name, int api);
int delete_srcsinkobjects_i(int i);
int test_iomode_sink_objects(stm_object_h snk_p);

int delete_producer_sink_objects(stm_object_h producer_p, stm_memsink_h snk_p, int api);
int delete_consumer_src_objects(stm_object_h consumer_p, stm_memsrc_h src_p, int api);

void mss_test_modprobe_func(void);
int memsink_vmalloc_test(void);
int memsrc_vmalloc_test(void);
#endif
