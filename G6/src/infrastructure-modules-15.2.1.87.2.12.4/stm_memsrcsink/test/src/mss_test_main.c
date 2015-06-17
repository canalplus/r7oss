#include <linux/module.h>

#include "mss_test.h"
#include "mss_test_consumer.h"
#include "mss_test_user_app_interface.h"

MODULE_AUTHOR("Nidhi Chadha");
MODULE_DESCRIPTION("Test module for memory source & sink");
MODULE_LICENSE("GPL");

static int __init init_mss_test(void)
{
	struct dentry *d_memsrcsink;

	/*** Initialise the module ***/
	d_memsrcsink = mss_test_create_dbgfs();
	mss_test_consumer_init();
	if (mss_register_char_driver() < 0)
		return -1;
	return mss_test_create_srcsink(d_memsrcsink);
	/* return 0; */

}

module_init(init_mss_test);

static void __exit exit_mss_test(void)
{
	/* TODO : Add error check */
	pr_info("%s\n", __FUNCTION__);

	mss_test_delete_srcsink();
	mss_test_consumer_term();
	mss_unregister_char_driver();
	mss_test_delete_dbgfs();
}
module_exit(exit_mss_test);
