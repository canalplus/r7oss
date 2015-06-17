#include "stm_registry.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include "infra_os_wrapper.h"
static stm_object_h object_data[] = {
	(stm_object_h)0x00123450,
	(stm_object_h)0x00123451,
	(stm_object_h)0x00123452,
	(stm_object_h)0x00123453,
	(stm_object_h)0x00123454,
	(stm_object_h)0x00123455,
	(stm_object_h)0x00123456,
	(stm_object_h)0x00123457,
	(stm_object_h)0x00123458,
	(stm_object_h)0x00123459,
};

void Memoryleak_object_add_remove_test(void)
{
	int ErrorCode = 0;
	int i = 10;
	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);
	while (i--) {
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		ErrorCode = stm_registry_add_instance(STM_REGISTRY_INSTANCES, object_data[0], NULL, object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);
		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object%x\n ", ErrorCode, (int) object_data[0]);
		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object%x\n ", ErrorCode, (int) object_data[0]);

	}
	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}

void Memoryleak_attribute_add_remove_test(void)
{
	int ErrorCode = 0;
	int i = 10;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);

	while (i--) {
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);

		ErrorCode = stm_registry_add_attribute(object_data[0],
			      "test_buffer_tag", STM_REGISTRY_INT32,
			      "test buffer content", sizeof("test buffer content"));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);

		ErrorCode = stm_registry_add_attribute(object_data[0],
			      "test_buffer_tag1", STM_REGISTRY_INT32,
			      "test buffer content", sizeof("test buffer content"));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);

		ErrorCode = stm_registry_add_attribute(object_data[0],
			      "test_buffer_tag2", STM_REGISTRY_INT32,
			      "test buffer content", sizeof("test buffer content"));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);

		ErrorCode = stm_registry_add_attribute(object_data[0],
			      "test_buffer_tag3", STM_REGISTRY_INT32,
			      "test buffer content", sizeof("test buffer content"));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);


		ErrorCode = stm_registry_add_attribute(object_data[0],
			      "test_buffer_tag4", STM_REGISTRY_INT32,
			      "test buffer content", sizeof("test buffer content"));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);

		ErrorCode = stm_registry_remove_attribute(object_data[0], "test_buffer_tag");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_attribute failed for object %x errorcode %d\n", (int) object_data[0], ErrorCode);

		ErrorCode = stm_registry_remove_attribute(object_data[0], "test_buffer_tag1");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_attribute failed for object %x errorcode %d\n", (int) object_data[0], ErrorCode);

		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);

	}

	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}

void Memoryleak_connection_add_remove_test(void)
{
	int ErrorCode = 0;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);
	{
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, NULL, object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);


		ErrorCode = stm_registry_add_connection(object_data[0], "connect0-1", object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %x for connection %s  error code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);

		ErrorCode = stm_registry_add_connection(object_data[1], "connect1-0", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %xfor connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[1], "connect1-0");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[0], "connect0-1");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);



		ErrorCode = stm_registry_add_connection(object_data[0], "connect0-1", object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %x for connection %s  error code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);

		ErrorCode = stm_registry_add_connection(object_data[1], "connect1-0", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %xfor connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[1], "connect1-0");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[0], "connect0-1");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);



		ErrorCode = stm_registry_add_connection(object_data[0], "connect0-1", object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %x for connection %s  error code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);

		ErrorCode = stm_registry_add_connection(object_data[1], "connect1-0", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_connection failed for %xfor connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[1], "connect1-0");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[1], "connect1-0", ErrorCode);

		ErrorCode = stm_registry_remove_connection(object_data[0], "connect0-1");
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_connection failed for %x for connection %s code %d\n ", (int) object_data[0], "connect0-1", ErrorCode);



		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);
		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
	}

	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}

void Memoryleak_child_add_remove_test(void)
{
	int ErrorCode = 0;
	int i = 10;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);

	while (i--) {
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);

		ErrorCode = stm_registry_add_object(object_data[0], NULL, object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);

		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);

		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
	}

	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}

void Memoryleak_reverse_remove_object_test(void)
{
	int ErrorCode = 0;
	int i = 10;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);

	while (i--) {
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
		ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_add_object(object_data[0], NULL, object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_add_object(object_data[1], NULL, object_data[2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[2]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_add_object(object_data[2], NULL, object_data[3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[3]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		stm_registry_dumptheregistry();
		ErrorCode = stm_registry_remove_object(object_data[3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[2]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[3]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

	}

	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}
void Memoryleak_reverse_remove_instance_test(void)
{
	int ErrorCode = 0;
	int i = 10;

	pr_info(" $$$$$$$$$$$$$ %s start$$$$$$$$$$$\n", __FUNCTION__);

	while (i--) {
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
		ErrorCode = stm_registry_add_object(stm_registry_types, NULL, object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_add_instance(stm_registry_types, object_data[0], NULL, object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_add_object(object_data[1], NULL, object_data[2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[2]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);


		ErrorCode = stm_registry_add_object(object_data[2], NULL, object_data[3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n ", ErrorCode, (int) object_data[3]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		stm_registry_dumptheregistry();

		ErrorCode = stm_registry_remove_object(object_data[3]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[0]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[2]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[2]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[1]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n ", ErrorCode, (int) object_data[3]);
		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);

		stm_registry_dumptheregistry();

		pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
	}
	pr_info(" $$$$$$$$$$$$$ %s end$$$$$$$$$$$\n", __FUNCTION__);
}
