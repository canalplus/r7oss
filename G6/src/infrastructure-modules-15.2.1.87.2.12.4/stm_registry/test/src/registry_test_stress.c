#include "stm_registry.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include "infra_os_wrapper.h"
static stm_object_h object_data[] = {
	(stm_object_h)0x123450,
	(stm_object_h)0x123451,
	(stm_object_h)0x123452,
	(stm_object_h)0x123453,
	(stm_object_h)0x123454,
	(stm_object_h)0x123455,
	(stm_object_h)0x123456,
	(stm_object_h)0x123457,
	(stm_object_h)0x123458,
	(stm_object_h)0x123459,
};

char *attr_tag[4] = {
	"test_buffer_tag",
	"test_buffer_tag1",
	"test_buffer_tag2",
	"test_buffer_tag3",
};

/* #define NODE_STRESS */
#define ATTR_STRESS

#ifdef	NODE_STRESS
static void intermediatenode_add_remove_test(int startnode);
#endif

static void attribute_add_remove_test(int test_object);

void thread1_entry(void *parameters)
{
	int i = 1000;
	while (i-- > 0) {
		/* Sleep */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(((22 * HZ) / 1000) + 1);
		pr_info(" <%s>\n", __FUNCTION__);
    #ifdef NODE_STRESS
		intermediatenode_add_remove_test(0);
    #endif

    #ifdef ATTR_STRESS
		attribute_add_remove_test(0);
    #endif
	}
}

void thread2_entry(void *parameters)
{
	int i = 1000;
	while (i-- > 0) {
		/* Sleep */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(((13 * HZ) / 1000) + 1);

		pr_info(" <%s>\n", __FUNCTION__);
    #ifdef NODE_STRESS
		intermediatenode_add_remove_test(4);
    #endif

    #ifdef ATTR_STRESS
		attribute_add_remove_test(1);
    #endif
	}
}

void registry_stress_test(void)
{
	infra_os_thread_t *thread1 = kmalloc(sizeof(infra_os_thread_t), GFP_KERNEL);
	infra_os_thread_t *thread2 = kmalloc(sizeof(infra_os_thread_t), GFP_KERNEL);
	infra_os_task_priority_t task_priority[2] = {SCHED_RR, 15};

	infra_os_thread_create(thread1,
		      thread1_entry,
		      NULL,
		      "INF-T-RegStress1",
		      task_priority);
	infra_os_thread_create(thread2,
		      thread2_entry,
		      NULL,
		      "INF-T-RegStress2",
		      task_priority);

}


#ifdef	NODE_STRESS
static void intermediatenode_add_remove_test(int start)
{
	int ErrorCode = 0;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);

	{
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[start+0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+0]);
		ErrorCode = stm_registry_add_object(object_data[start+0], NULL, object_data[start+1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+1]);
		ErrorCode = stm_registry_add_object(object_data[start+1], NULL, object_data[start+2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+1]);
		ErrorCode = stm_registry_add_object(object_data[start+2], NULL, object_data[start+3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+1]);


		ErrorCode = stm_registry_remove_object(object_data[start+3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+3]);

		ErrorCode = stm_registry_remove_object(object_data[start+2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+2]);

		ErrorCode = stm_registry_remove_object(object_data[start+1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+1]);

		ErrorCode = stm_registry_remove_object(object_data[start+0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[start+0]);
	}
	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}
#endif

void attribute_add_remove_test(int test_object)
{
	int ErrorCode = 0;
	static int first_time;
	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);
	if (first_time == 0) {
		first_time = 1;
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
	}
	ErrorCode = stm_registry_add_attribute(object_data[0],
		      attr_tag[test_object], STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x  for %s\n", (int) object_data[0], attr_tag[test_object]);

	ErrorCode = stm_registry_add_attribute(object_data[0],
		      attr_tag[test_object+2], STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x for %s\n", (int) object_data[0], attr_tag[test_object+2]);

	ErrorCode = stm_registry_remove_attribute(object_data[0], attr_tag[test_object]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_attribute failed for object %x errorcode %d  %s\n", (int) object_data[0], ErrorCode, attr_tag[test_object]);

	ErrorCode = stm_registry_remove_attribute(object_data[0], attr_tag[test_object+2]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_attribute failed for object %x errorcode %d  %s\n", (int) object_data[0], ErrorCode, attr_tag[test_object+2]);

	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}
