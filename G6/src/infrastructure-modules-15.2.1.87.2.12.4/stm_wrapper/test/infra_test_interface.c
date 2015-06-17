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
   @file     infra_test_interface.c
   @brief

 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "infra_os_wrapper.h"
#include "infra_test_interface.h"
#include "infra_debug.h"
#include "infra_token.h"

MODULE_AUTHOR("stmicroelectronics");
MODULE_LICENSE("GPL");


static ssize_t
infra_test_sysfs_show(struct kobject *,
			struct attribute * ,
			char *);
static ssize_t
infra_test_sysfs_store(struct kobject *,
			struct attribute * ,
			const char *,
			size_t );

static infra_test_interface_q_t *interface_param_q_head_p=NULL;
static infra_test_interface_q_t *interface_param_data_q_head_p=NULL;

static infra_os_mutex_t lock_test_q_param_p;

int infra_test_deregister_cmd(infra_test_reg_param_t *cmd_param_p)
{
	int			error = INFRA_NO_ERROR;
	infra_test_interface_q_t		*interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;

	infra_test_debug(">>> %s@%d >>> \n", __func__, __LINE__);
	infra_os_mutex_lock(lock_test_q_param_p);
	error = infra_q_remove_node(
		&(interface_param_q_head_p),
		(uint32_t)(cmd_param_p->interface_func_p),
		&interface_q_param_p);
	cur_interface_param_p = INFRA_Q_GET_DATA(
		interface_q_param_p,
		infra_test_interface_param_t);
	infra_os_free(cur_interface_param_p);
	infra_os_mutex_unlock(lock_test_q_param_p);
	infra_test_debug("<<< %s@%d <<< \n", __func__, __LINE__);

	return error;
}


EXPORT_SYMBOL(infra_test_deregister_cmd);


int infra_test_register_cmd(infra_test_reg_param_t *cmd_param_p)
{
	infra_test_interface_param_t			*interface_param_p;

	infra_test_debug(">>> %s@%d >>> \n", __func__, __LINE__);
	interface_param_p = (infra_test_interface_param_t*)
				infra_os_malloc(sizeof(*interface_param_p));
	if(NULL==interface_param_p){
		infra_error_print("%s@%d: Error in command registration\n",
				__func__,
				__LINE__);
		return -ENOMEM;
	}

	memset((void*)interface_param_p->func_str,
			'\0',
			INFRA_TEST_MAX_STR_LEN);
	memset((void*)interface_param_p->info,
			'\0',
			INFRA_TEST_MAX_CMD_DESC_LEN);
	strncpy(interface_param_p->func_str ,
			cmd_param_p->func_str,
			(INFRA_TEST_MAX_STR_LEN-1));
	strncpy(interface_param_p->info,
			cmd_param_p->info,
			(INFRA_TEST_MAX_CMD_DESC_LEN-1));
	interface_param_p->interface_func_p = cmd_param_p->interface_func_p;
	interface_param_p->user_data_p = cmd_param_p->user_data_p;

	infra_test_debug("%15s:%d - %s\n",
			interface_param_p->func_str,
			strlen(interface_param_p->func_str),
			interface_param_p->info);

	infra_os_mutex_lock(lock_test_q_param_p);
	infra_q_insert_node(&(interface_param_q_head_p),
			&(interface_param_p->interface_q_node),
			(uint32_t)interface_param_p->interface_func_p,
			(void*)interface_param_p);
	infra_os_mutex_unlock(lock_test_q_param_p);
	infra_test_debug("<<< %s@%d <<< \n", __func__, __LINE__);

	return INFRA_NO_ERROR;

}

EXPORT_SYMBOL(infra_test_register_cmd);


int infra_test_register_param(infra_test_param_t *int_param_p)
{
	infra_test_interface_param_t	*interface_param_p;

	infra_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
	interface_param_p = (infra_test_interface_param_t*)infra_os_malloc(sizeof(*interface_param_p));
	if(NULL==interface_param_p){
		infra_error_print("%s@%d: Error in Parameter registration\n",__func__,__LINE__);
		return -ENOMEM;
	}

	memset((void*)interface_param_p->func_str,
			'\0',
			INFRA_TEST_MAX_STR_LEN);
	memset((void*)interface_param_p->info,
			'\0',
			INFRA_TEST_MAX_CMD_DESC_LEN);
	strncpy(interface_param_p->func_str ,
			int_param_p->param_str,
			(INFRA_TEST_MAX_STR_LEN-1));
	interface_param_p->user_data_p = (int *)infra_os_malloc(sizeof(int));

	*(int *)(interface_param_p->user_data_p) = *(int *)(int_param_p->user_data_p);

	infra_test_debug("\nInserting %15s Length :%d - Value : %d\n",
			interface_param_p->func_str,
			strlen(interface_param_p->func_str),
			*(int *)interface_param_p->user_data_p);



	infra_os_mutex_lock(lock_test_q_param_p);
	infra_q_insert_node(&(interface_param_data_q_head_p),
			&(interface_param_p->interface_q_node),
			(uint32_t)&(interface_param_p->interface_q_node),
			(void*)interface_param_p);
	infra_os_mutex_unlock(lock_test_q_param_p);

	infra_test_debug("\n<<< %s@%d <<< \n\n", __func__, __LINE__);

	return INFRA_NO_ERROR;

}

EXPORT_SYMBOL(infra_test_register_param);


int infra_test_deregister_param(infra_test_param_t *int_param_p)
{
	int error = INFRA_NO_ERROR;
	infra_test_interface_q_t *cur_interface_q_param_p;
	infra_test_interface_param_t *cur_interface_param_p;
	uint8_t cmp_result=0;
	uint8_t	node_no = 0;

	infra_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
	cur_interface_q_param_p = interface_param_data_q_head_p;

	infra_os_mutex_lock(lock_test_q_param_p);

	while(cur_interface_q_param_p != NULL){

		cur_interface_param_p = INFRA_Q_GET_DATA(
				cur_interface_q_param_p,
				infra_test_interface_param_t);

		cmp_result = strcmp(int_param_p->param_str,
				cur_interface_param_p->func_str);

		if(!(cmp_result)){

			error = infra_q_remove_node(
					&(interface_param_data_q_head_p),
					(uint32_t)&(cur_interface_param_p->interface_q_node),
					&cur_interface_q_param_p);

			infra_test_debug("%15s:%d - %s Error : %d \n",
					cur_interface_param_p->func_str,
					strlen(cur_interface_param_p->func_str),
					cur_interface_param_p->info, error);

			infra_os_free(cur_interface_param_p->user_data_p);

			infra_os_free(cur_interface_param_p);

			break;
		}

		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node))
				);
	}

	cur_interface_q_param_p = interface_param_data_q_head_p;

	while(cur_interface_q_param_p != NULL){

		node_no++;
		cur_interface_param_p = INFRA_Q_GET_DATA(
				cur_interface_q_param_p,
				infra_test_interface_param_t);

		infra_test_debug("Node (%u) : %15s \n",
				node_no,cur_interface_param_p->func_str);
		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node)));
	}

	infra_os_mutex_unlock(lock_test_q_param_p);

	infra_test_debug("\n<<< %s@%d <<< \n", __func__, __LINE__);

	return error;

}

EXPORT_SYMBOL(infra_test_deregister_param);



int infra_test_get_param_data(char *param_str_p,
		infra_type_t mytype,
		void *data_p,
		uint8_t size)
{
	int error = INFRA_NO_ERROR;
	infra_test_interface_q_t *cur_interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;
	uint8_t					cmp_result=0;
	uint32_t				len;
	char					*data;

	infra_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
	len = strlen(param_str_p);
	data = infra_os_malloc(size);
	infra_test_debug("\nReceived param_str : %s\n", param_str_p);
	cur_interface_q_param_p = interface_param_data_q_head_p;

	while(cur_interface_q_param_p != NULL){

		cur_interface_param_p = INFRA_Q_GET_DATA(
				cur_interface_q_param_p,
				infra_test_interface_param_t);

		cmp_result = strcmp(param_str_p,
				cur_interface_param_p->func_str);

		infra_test_debug("\n%s@%d \n \tArg: %s Parameter :%s Value : %d\n",
				__func__,
				__LINE__,
				param_str_p,
				cur_interface_param_p->func_str,
				*(int *)cur_interface_param_p->user_data_p);

		if(!(cmp_result)){

			infra_test_debug("\nParameter Match !!!\n");

			switch(mytype){

				case INTEGER:
					*(int *)data_p =
					*(int *)cur_interface_param_p->user_data_p;
					break;
				case STRING:
					strncpy(
					(char *)data_p,
					(char *)cur_interface_param_p->user_data_p,
					size);
					break;
				case HEX:
					*(int *)data_p =
					*(int *)cur_interface_param_p->user_data_p;
					break;
				case UNKNOWN:
					return -EINVAL;
				default:
					return -EINVAL;
			}
			break;
		}

		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node))
				);

	}

	infra_os_free(data);

	return error;

}

EXPORT_SYMBOL(infra_test_get_param_data);


static ssize_t
infra_test_sysfs_show(struct kobject *kobj,
		struct attribute *attr,
		char *buf)
{

	infra_test_interface_q_t		*cur_interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;
	uint8_t					local_len = 0;
	uint32_t				len =0;

	infra_test_debug( "Reading Data from Attribute \n");

	cur_interface_q_param_p = interface_param_q_head_p;

	while(cur_interface_q_param_p != NULL) {

		cur_interface_param_p = INFRA_Q_GET_DATA(
				cur_interface_q_param_p,
				infra_test_interface_param_t);
		local_len = sprintf(buf , "%25s:\n ",
				cur_interface_param_p->func_str);
		infra_test_debug("%25s - %s\n",
				cur_interface_param_p->func_str,
				cur_interface_param_p->info);
		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node))
				);

		len += local_len;
		buf += local_len;
	}

	return (len);

}

static ssize_t
infra_test_sysfs_store(struct kobject *kobj,
		struct attribute *attr,
		const char *buf,
		size_t len)
{
	infra_test_interface_q_t		*cur_interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;
	infra_test_func_t			interface_func_p = NULL;

	char *dummy_p=NULL;
	uint8_t cmp_result=0;
	uint32_t mylen;
	char  *test_case;
	char delimiters[]= " ";

	infra_test_debug(" %s>>>@ %d>>>  ",__func__, __LINE__);

	dummy_p = (char *)buf;
	dummy_p[len-1] = '\0';
	test_case = strsep(&dummy_p,delimiters);
	mylen = len;
	cur_interface_q_param_p = interface_param_q_head_p;

	infra_test_debug( "Writing Data [%s of length %u ]to Attribute\n",
			test_case,
			mylen);
	while(cur_interface_q_param_p != NULL){

		cur_interface_param_p = INFRA_Q_GET_DATA(
				cur_interface_q_param_p,
				infra_test_interface_param_t);
		cmp_result = strcmp(test_case, cur_interface_param_p->func_str);

		infra_test_debug("Arg:%s:%d List:%s\n",
				dummy_p,
				len ,
				cur_interface_param_p->func_str);

		if(!(cmp_result)){
			interface_func_p =
				(infra_test_func_t)cur_interface_param_p->interface_func_p;
			break;
		}

		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node))
				);
	}

	if(interface_func_p) {
		infra_debug_trace(">>> %s@ %d >>> \n", __func__, __LINE__);
		interface_func_p(dummy_p, cur_interface_param_p->user_data_p);
	}

	return mylen;

}

struct infra_test_attr_s {
	struct attribute attr;
};

static struct infra_test_attr_s infra_test_attr = {
	.attr.name="test",
	.attr.mode = 0666,
};

static struct attribute * infra_test_kobj_attr[] = {
	&infra_test_attr.attr,
	NULL
};

struct kobject *infra_test_kobj_p;

static struct sysfs_ops infra_test_sysfs_ops;

static struct kobj_type infra_test_kobj_type;

static int __init infra_init_test_module(void)
{
	int error = -1;
	struct kset *kset_p;
	struct kobject *kobj_p;

	infra_os_mutex_initialize(&(lock_test_q_param_p));
	infra_test_sysfs_ops.show = infra_test_sysfs_show;
	infra_test_sysfs_ops.store = infra_test_sysfs_store;

	infra_test_kobj_type.sysfs_ops = &infra_test_sysfs_ops;
	infra_test_kobj_type.default_attrs = infra_test_kobj_attr;

	infra_test_print(">>> %s@%d MUTEX:0x%x \n",
			__func__,
			__LINE__,
			(uint32_t)lock_test_q_param_p);

	/*
	 * Create a kset with the name of "infrastructure",
	 * located under /sys/kernel/
	 */
	kset_p = kset_create_and_add("infrastructure", NULL, kernel_kobj);
	if (!kset_p)
		return -ENOMEM;


	kobj_p = kzalloc(sizeof(*kobj_p), GFP_KERNEL);

	/*
	 * As we have a kset for this kobject, we need to set it before calling
	 * the kobject core.
	 */

	kobj_p->kset = kset_p;

	/*
	 * Initialize and add the kobject to the kernel.  All the default files
	 * will be created here.
	 */

	if (kobj_p) {
		/*Initialize kobject*/
		kobject_init(kobj_p, &infra_test_kobj_type);
		if (kobject_add(kobj_p, NULL, "%s", "infra")) {
			error = -1;
			infra_test_print("Sysfs creation failed\n");
			kobject_put(kobj_p);
			kfree(kobj_p);
			kobj_p = NULL;
		}
		error = 0;
	}

	infra_test_kobj_p = kobj_p;
	infra_test_print("<<< %s@%d <<< \n", __func__, __LINE__);

	return error;

	/*As we have already specified a kset for this
	 * kobject, we don't have to set a parent for the kobject, the kobject
	 * will be placed beneath that kset automatically.
	 */
}

static void __exit term_infra_test_module(void)
{

	int error = INFRA_NO_ERROR;
	infra_test_interface_q_t *cur_interface_q_param_p;
	infra_test_interface_param_t *cur_interface_param_p;
	struct kset *kset_p;
	struct kobject *kobj_p = infra_test_kobj_p;


	infra_test_print(">>> %s@%d >>> \n", __func__, __LINE__);
	kset_p = kobj_p->kset;
	infra_os_mutex_lock(lock_test_q_param_p);

	while(interface_param_q_head_p != NULL){
		cur_interface_param_p = INFRA_Q_GET_DATA(
				interface_param_q_head_p,
				infra_test_interface_param_t);
		error = infra_q_remove_node(
				&(interface_param_q_head_p),
				(uint32_t)(cur_interface_param_p->interface_func_p),
				&cur_interface_q_param_p);
		infra_os_free(cur_interface_param_p);

	}

	while(interface_param_data_q_head_p != NULL){

		infra_test_debug("\nEntering Loop\n");
		cur_interface_param_p = INFRA_Q_GET_DATA(
				interface_param_data_q_head_p,
				infra_test_interface_param_t);

		error = infra_q_remove_node(
				&(interface_param_data_q_head_p),
				(uint32_t)&(cur_interface_param_p->interface_q_node),
				&cur_interface_q_param_p);

		infra_test_debug("%15s:%d - %s\n",
				cur_interface_param_p->func_str,
				strlen(cur_interface_param_p->func_str),
				cur_interface_param_p->info);

		infra_os_free(cur_interface_param_p->user_data_p);
		infra_os_free(cur_interface_param_p);

		cur_interface_q_param_p = INFRA_Q_GET_NEXT(
				(&(cur_interface_param_p->interface_q_node))
				);

	}

	infra_os_mutex_unlock(lock_test_q_param_p);
	infra_os_mutex_terminate(lock_test_q_param_p);

	kset_unregister(kset_p);
	if (kobj_p) {
		kobject_put(kobj_p);
		kfree(kobj_p);
	}

	infra_test_print("<<< %s@%d <<< \n", __func__, __LINE__);

}

module_init(infra_init_test_module);
module_exit(term_infra_test_module);
