#include <linux/module.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */
#include "registry_test.h"
#include "infra_queue.h"
#include "infra_os_wrapper.h"
#include "infra_test_interface.h"

MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics registry driver test");
MODULE_LICENSE("GPL");

static infra_error_code_t registry_test_exit(void);

void registry_test_modprobe_func(void)
{
	pr_info("This is INFRA's Registry TEST Module\n");
}
EXPORT_SYMBOL(registry_test_modprobe_func);

infra_error_code_t registry_functionality_test(void *data_p, void *usr_data)
{

	functionality_object_add_remove_test();
	functionality_attribute_add_remove_test();
	/* functionality_delete_object_with_attribute_and_children(); */
	functionality_connection_add_and_remove();
	functionality_get_info_test();
	functionality_get_attribute_test();
	functionality_cleanup();
	functionality_backlink_test();

       /* functionality_attribute_store_test(); */
	return 0;
}

infra_error_code_t registry_iterator_test(void *data_p, void *usr_data)
{
	iterator_create_and_delete();
	return 0;
}

infra_error_code_t registry_memory_leak_test(void *data_p, void *usr_data)
{
	Memoryleak_object_add_remove_test();
	Memoryleak_attribute_add_remove_test();

	Memoryleak_connection_add_remove_test();
	Memoryleak_child_add_remove_test();
	Memoryleak_reverse_remove_object_test();
	Memoryleak_reverse_remove_instance_test();
	return 0;
}

infra_error_code_t registry_stress_test_test(void *data_p, void *usr_data)
{
	registry_stress_test();
	return 0;
}

infra_error_code_t registry_data_type_test(void *data_p, void *usr_data)
{
	datatype_add_remove_test();
	return 0;
}

infra_error_code_t registry_test_entry(void)
{
	infra_test_reg_param_t		cmd_param;
	INFRA_TEST_REG_CMD(cmd_param, "REG_FUNCT_TEST", registry_functionality_test, "Functionality test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_ITERATOR_TEST", registry_iterator_test, "Iterator test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_MEMORYLEAK_TEST", registry_memory_leak_test, "Memory leak test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_STRESS_TEST", registry_stress_test_test, "Stress test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_DATATYPE_TEST", registry_data_type_test, "Data type test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_DYN_TEST_START", functionality_attribute_dynamic_test, "Data DYNAMIC test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_CHANGE_TEST", functionality_change_dynamic_variable, "Data DYNAMIC test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_DYN_TEST_END", functionality_remove_dynamic_test, "Data DYNAMIC test");
	return INFRA_NO_ERROR;
}

static int registry_test_exit(void)
{
	infra_test_reg_param_t		cmd_param;
	INFRA_TEST_REG_CMD(cmd_param, "REG_FUNCT_TEST", registry_functionality_test, "Functionality test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_ITERATOR_TEST", registry_iterator_test, "Iterator test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_MEMORYLEAK_TEST", registry_memory_leak_test, "Memory leak test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_STRESS_TEST", registry_stress_test_test, "Stress test");
	INFRA_TEST_REG_CMD(cmd_param, "REG_DATATYPE_TEST", registry_data_type_test, "Data type test");

	return INFRA_NO_ERROR;
}

static int __init registry_init_module(void)
{
	registry_test_print("Load module stm_registry_test by %s (pid %i)\n", current->comm, current->pid);
	registry_test_entry();

	return 0;
}
static void __exit registry_exit_module(void)
{
	registry_test_print("Unload module stm_registry_test by %s (pid %i)\n", current->comm, current->pid);
	registry_test_exit();
}

module_init(registry_init_module);
module_exit(registry_exit_module);
