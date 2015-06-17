#include "stm_registry.h"
#include <linux/kernel.h>
#include <linux/slab.h>

#define DATATYPE_TEST_TAG1  "test_tag1"
#define DATATYPE_TEST_TAG2  "test_tag2"

int test_tag_handler1(
		      stm_object_h object,
		      char *buf,
		      size_t size,
		      char *user_buf,
		      size_t user_size)
{
	pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
	return 0;
}

int test_tag_handler2(
		      stm_object_h object,
		      char *buf,
		      size_t size,
		      char *user_buf,
		      size_t user_size)
{
	pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
	return 0;
}

int test_tag_store_handler1(
			    stm_object_h object,
			    char *buf,
			    size_t size,
			    const char *user_buf,
			    size_t user_size)
{
	pr_info("<%s> :: <%d>\n", __FUNCTION__, __LINE__);
	return 0;
}
void datatype_add_remove_test(void)
{
	int ErrorCode = 0;
	stm_registry_type_def_t def;
	int user_size = 0;
	char buffer[100];
	char store_buf[100];
	def.print_handler = test_tag_handler1;
	def.store_handler = test_tag_store_handler1;

	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_add_data_type(DATATYPE_TEST_TAG1, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_add_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG1);
	def.print_handler = test_tag_handler2;

	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_add_data_type(DATATYPE_TEST_TAG2, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_add_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG2);
	def.print_handler = NULL;
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_get_data_type(DATATYPE_TEST_TAG1, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_get_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG1);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	def.print_handler((stm_object_h)0x12345, "test", 32, buffer, user_size);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	def.store_handler((stm_object_h)0x12345, store_buf, user_size, "test", 32);


	ErrorCode = stm_registry_remove_data_type(DATATYPE_TEST_TAG1);
	if (ErrorCode != 0)
		pr_err("registry_internal_remove_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG1);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_remove_data_type(STM_REGISTRY_INT32); /*for negative testing*/
	if (ErrorCode != 0) {
		pr_err("registry_internal_remove_data_type failed %d for datatype %s\n", ErrorCode, STM_REGISTRY_INT32);
		pr_err(" THIS SHOULD GET FAILED ");
	}
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_remove_data_type(DATATYPE_TEST_TAG2);
	if (ErrorCode != 0)
		pr_err("registry_internal_remove_data_type failed %d for datatype %s\n ", ErrorCode, DATATYPE_TEST_TAG2);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);


	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);
	ErrorCode = stm_registry_add_data_type(DATATYPE_TEST_TAG1, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_add_data_type failed %d for datatype %s\n ", ErrorCode, DATATYPE_TEST_TAG1);
	def.print_handler = test_tag_handler2;

	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	ErrorCode = stm_registry_add_data_type(DATATYPE_TEST_TAG2, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_add_data_type failed %d for datatype %s\n ", ErrorCode, DATATYPE_TEST_TAG2);


	ErrorCode = stm_registry_get_data_type(DATATYPE_TEST_TAG1, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_get_data_type failed %d for datatype %s\n ", ErrorCode, DATATYPE_TEST_TAG1);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	def.print_handler((stm_object_h)0x12345, "test", 32, buffer, user_size);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	ErrorCode = stm_registry_remove_data_type(DATATYPE_TEST_TAG1);
	if (ErrorCode != 0)
		pr_err("registry_internal_remove_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG1);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);

	ErrorCode = stm_registry_remove_data_type(DATATYPE_TEST_TAG2);
	if (ErrorCode != 0)
		pr_err("registry_internal_remove_data_type failed %d for datatype %s\n", ErrorCode, DATATYPE_TEST_TAG2);
	pr_info(" <%s> :: <%d>\n", __FUNCTION__, __LINE__);


}
