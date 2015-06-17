
#ifndef _MSS_DEBUGFS_H
#define _MSS_DEBUGFS_H

#include "stm_data_interface.h"
#include "stm_memsrc.h"
#include "stm_memsink.h"
#include <linux/debugfs.h>



struct io_descriptor {
	size_t size;
	size_t size_allocated;
	void *data;
};


extern size_t consumer_size;
extern size_t producer_size;

extern struct io_descriptor push;
extern struct io_descriptor pull;


int		mss_test_create_srcsink(struct dentry *d_memsrcsink);
int		mss_test_delete_srcsink(void);

struct dentry *mss_test_create_dbgfs(void);
void mss_test_delete_dbgfs(void);



#endif
