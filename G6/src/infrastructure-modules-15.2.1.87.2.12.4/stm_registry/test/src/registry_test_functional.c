#include "stm_registry.h"
#include <linux/kernel.h>
#include <linux/slab.h>
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

typedef struct {
	int (*connect_handler)(
			       stm_object_h object,
			       stm_object_h sink);
	int (*disconnect_handler)(
				  stm_object_h object,
				  stm_object_h sink);
	int (*push_data_handler)(
				 stm_object_h object,
				 stm_object_h data_block);
} stm_sink_interface_t;


int test_connect_handler(stm_object_h object,
			 stm_object_h sink)
{
	return 0;
}
int test_disconnect_handler(stm_object_h object,
			    stm_object_h sink)
{
	return 0;
}

int test_push_data_handler(stm_object_h object,
			   stm_object_h data_block)
{
	return 0;
}


static stm_sink_interface_t demux_sink_interface = {
	test_connect_handler,
	test_disconnect_handler,
	test_push_data_handler
};

int functionality_object_add_remove_test(void)
{
	int ErrorCode = 0;
	int count;

	ErrorCode = stm_registry_add_object(STM_REGISTRY_TYPES, "childofroot", object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);

	ErrorCode = stm_registry_remove_object(object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);

	for (count = 0; count < 5; count++) {
		ErrorCode = stm_registry_add_object(STM_REGISTRY_TYPES, NULL, object_data[count]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[count]);
	}
	for (count = 0; count < 5; count++) {
		ErrorCode = stm_registry_remove_object(object_data[count]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x\n", ErrorCode, (int) object_data[count]);
	}
	for (count = 0; count < 5; count++) {
		ErrorCode = stm_registry_add_object(STM_REGISTRY_TYPES, NULL, object_data[count]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[count]);
	}
	ErrorCode = stm_registry_remove_object(object_data[3]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[3]);

	ErrorCode = stm_registry_add_object(STM_REGISTRY_TYPES, NULL, object_data[3]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[3]);

	ErrorCode = stm_registry_add_instance(STM_REGISTRY_INSTANCES, object_data[3], "obj88", object_data[8]);
	if (ErrorCode != 0) {
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[4]);
		pr_err(" This should failed for dupilicate entry\n");
	}

	ErrorCode = stm_registry_add_instance(STM_REGISTRY_INSTANCES, object_data[3], "obj99", object_data[9]);
	if (ErrorCode != 0) {
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[4]);
		pr_err(" This should failed for dupilicate entry\n");
	}

	ErrorCode = stm_registry_remove_object(object_data[8]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed !!!! %x\n", (unsigned int) object_data[8]);

	ErrorCode = stm_registry_add_object(object_data[3], "child", object_data[7]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x for parent %x\n", ErrorCode, (int) object_data[7], (int) object_data[3]);

	pr_info("<<<<  Sucessfully functionality_object_add_remove_test  For STM REGISTY is done  <<<<<");
	return 0;
}


#define UPDATED_VALUE 0x200
int buffer;
int buffer1;
int functionality_attribute_add_remove_test(void) /*Asssuming the object list already there */
{
	int ErrorCode = 0;
	ErrorCode = stm_registry_add_attribute(object_data[4],
		      "test_buffer_tag", STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[4]);

	ErrorCode = stm_registry_remove_attribute(object_data[4], "test_buffer_tag");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_attribute failed for object %x errorcode %d\n", (int) object_data[4], ErrorCode);

	ErrorCode = stm_registry_add_attribute(object_data[4],
		      "test_attribute", STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[4]);

	ErrorCode = stm_registry_add_attribute(object_data[4],
		      "test_attribute4", STM_REGISTRY_INT32,
		      "test buffer content 1", sizeof("test buffer content 1"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[4]);

	ErrorCode = stm_registry_add_attribute(object_data[4],
		      "test_attribute41", STM_REGISTRY_INT32,
		      "test buffer content 1", sizeof("test buffer content 1"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[4]);

	ErrorCode = stm_registry_remove_attribute(object_data[4], "test_attribute");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_attribute failed for object %x test_attribute\n", (int) object_data[4]);

	ErrorCode = stm_registry_add_attribute(object_data[7],
		      "test_attribute7", STM_REGISTRY_INT32,
		      "test buffer content 1", sizeof("test buffer content 1"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[7]);
	{
		buffer = 0x100;
		ErrorCode = stm_registry_add_attribute(object_data[9],
			      "int_attr", STM_REGISTRY_UINT32,
			      (void *) &buffer, sizeof(int));
		if (ErrorCode != 0)
			pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[9]);

		buffer1 = UPDATED_VALUE;
		ErrorCode = stm_registry_update_attribute(object_data[9],
			      "int_attr", STM_REGISTRY_UINT32,
			      (void *) &buffer1, sizeof(int));
		if (ErrorCode != 0)
			pr_err("stm_registry_update_attribute failed for object %x\n", (int) object_data[9]);
	}
	ErrorCode = stm_registry_add_attribute(object_data[9],
		      "demux_test", STM_REGISTRY_ADDRESS,
		      &demux_sink_interface,
		      sizeof(stm_sink_interface_t));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[9]);
	pr_info("<<<<  Sucessfully  functionality_attribute_add_remove_test For STM REGISTY is done  <<<<<");
	return 0;

}

void functionality_get_attribute_test(void)
{
	int ErrorCode = 0;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE]; /*returned for the get attribute*/
	char buffer[100]; /* Buffer to hold the attribute data*/
	char user_buf[800]; /* Buffer to hold the user data */
	int p_actual_size; /* Actuel data rtetunred by the get attribute*/
	stm_registry_type_def_t def; /* Print handler returned by the get data type*/
	int formatted_size; /* formatted buffer size returned by the print handler*/

	ErrorCode = stm_registry_get_attribute(object_data[9], "int_attr", type_tag, 100, buffer, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute failed for object %x\n", (int) object_data[9]);

	ErrorCode = stm_registry_get_data_type(type_tag, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_get_data_type failed %d for datatype %s\n", ErrorCode, type_tag);


	formatted_size = def.print_handler(object_data[9], buffer, p_actual_size, user_buf, 100);
	if (formatted_size == 0)
		pr_err("def.print_handler failed for datatype %s\n", type_tag);

	pr_info("%s\n", user_buf);

	ErrorCode = stm_registry_get_attribute(object_data[9], "demux_test", type_tag, 100, buffer, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute failed for object %x\n", (int) object_data[9]);

	ErrorCode = stm_registry_get_data_type(type_tag, &def);
	if (ErrorCode != 0)
		pr_err("registry_internal_get_data_type failed %d for datatype %s\n", ErrorCode, type_tag);


	formatted_size = def.print_handler(object_data[9], buffer, p_actual_size, user_buf, 800);
	if (formatted_size == 0)
		pr_err("def.print_handler failed for datatype %s\n", type_tag);

	pr_info("%s\n", user_buf);

	pr_info("data formatted by the rpint handler %d\n", formatted_size);
	pr_info("<<<<  Sucessfully  functionality_get_attribute_test For STM REGISTY is done  <<<<<");

}

int functionality_connection_add_and_remove(void)
{
	int ErrorCode = 0;

	ErrorCode = stm_registry_add_connection(object_data[0], "connect0-3", object_data[3]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_connection failed for %x for connection %s\n", (int) object_data[0], "connect0-3");

	ErrorCode = stm_registry_add_connection(object_data[3], "connect3-0", object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_connection failed for %x for connection %s\n", (int) object_data[3], "connect3-0");
	pr_info("<<<<  Sucessfully  functionality_connection_add_and_remove For STM REGISTY is done  <<<<<");

	return 0;
}

int functionality_get_info_test(void)
{

	int ErrorCode = 0;
	char *tag = kmalloc(30, GFP_KERNEL);
	char *type_tag = kmalloc(30, GFP_KERNEL);
	stm_object_h p_parent;
	stm_object_h p_object;
	stm_object_h p_connected_object;
	char buffer[30];
	int p_actual_size;
	int int_var;

	stm_registry_dumptheregistry();

	ErrorCode = stm_registry_get_object_tag(object_data[9], tag);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_object_tag failed !!! %x  error %d\n", (unsigned int) object_data[9], ErrorCode);
	else
		pr_info("Object tag  %s\n", tag);

	ErrorCode = stm_registry_get_object_parent(object_data[9], &p_parent);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_object_parent failed !!!%x error %d\n", (unsigned int) object_data[9], ErrorCode);
	else
		pr_info(" parent of object %x  is %x\n", (unsigned int) object_data[9], (unsigned int) p_parent);

	ErrorCode = stm_registry_get_object(object_data[7], tag, &p_object);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_object failed !!! %x  errorcode %d\n", (unsigned int) object_data[3], ErrorCode);
	else
		pr_info(" object of parent %x is %x\n", (unsigned int) object_data[3], (unsigned int) p_object);

	ErrorCode = stm_registry_get_attribute(object_data[9], "int_attr", type_tag, 30, (void *) &int_var, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute failed !!! %x for tag %s  error %d\n", (int) p_object, "test_attribute9", ErrorCode);
	else
		pr_info(" attribute data %x and the size returned %d, expected data is %x\n", int_var, p_actual_size, (int)UPDATED_VALUE);

	ErrorCode = stm_registry_get_attribute(object_data[9], "demux_test", type_tag, 30, (void *) NULL, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute for NULL failed !!! %x for tag %s  error %d\n", (unsigned int) p_object, "test_attribute9", ErrorCode);
	else
		pr_info(" stm_registry_get_attribute for NULL attribute data %s and the size returned %d\n", buffer, p_actual_size);


	ErrorCode = stm_registry_get_attribute(object_data[9], "demux_test", type_tag, 10, (void *) buffer, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute for less memory failed !!! %x for tag %s  error %d actual size %d\n",
			      (int) p_object, "test_attribute9", ErrorCode, p_actual_size);
	else
		pr_info(" stm_registry_get_attribute for less memory  attribute data %s and the size returned %d\n", buffer, p_actual_size);

	ErrorCode = stm_registry_get_connection(object_data[3], "connect3-0", &p_connected_object);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_connection failed !!! %x for tag %s error %d\n", (int) p_parent, "connect3-0", ErrorCode);
	else
		pr_info("%x is connected to %x\n", (int) object_data[3], (unsigned int) p_connected_object);
	pr_info("<<<<  Sucessfully  functionality_get_info_test  For STM REGISTY is done  <<<<<");
	return 0;
}

int functionality_cleanup(void)
{
	int ErrorCode = 0;
	int i = 0;

	ErrorCode = stm_registry_remove_connection(object_data[0], "connect0-3");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_connection failed for %x for connection %s error %d\n ", (unsigned int) object_data[0], "connect0-3", ErrorCode);

	ErrorCode = stm_registry_remove_connection(object_data[3], "connect3-0");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_connection failed for %x for connection %s error %d\n ", (unsigned int) object_data[0], "connect0-3", ErrorCode);
	ErrorCode = stm_registry_remove_object(object_data[9]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x error %d\n ", ErrorCode, (unsigned int) object_data[9], ErrorCode);

	ErrorCode = stm_registry_remove_object(object_data[7]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x error %d\n ", ErrorCode, (unsigned int) object_data[9], ErrorCode);

	ErrorCode = stm_registry_remove_object(object_data[4]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x error %d\n ", ErrorCode, (unsigned int) object_data[4], ErrorCode);

	for (i = 0; i < 4; i++) {
		ErrorCode = stm_registry_remove_object(object_data[i]);
		if (ErrorCode != 0)
			pr_err("stm_registry_remove_object failed %d for object %x error %d\n ", ErrorCode, (unsigned int) object_data[i], ErrorCode);
	}
	pr_info("<<<<  Sucessfully  functionality_cleanup  For STM REGISTY is done  <<<<<");
	return 0;

}

int functionality_backlink_test(void)
{
	int ErrorCode = 0, i = 10;
	while (i--) {
		ErrorCode = stm_registry_add_object(
			      STM_REGISTRY_INSTANCES,
			      NULL,
			      object_data[0]);
		if (ErrorCode != 0)
			pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);
		ErrorCode = stm_registry_add_connection(
			      object_data[0],
			      "connect",
			      object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s Expected !!!!!!!\n ",
				      __LINE__, (int) object_data[1], "connect");
		else
			pr_err("<%d>stm_registry_add_connection should failed for %x for connection %s\n",
				      __LINE__, (int) object_data[0], "connect");

		ErrorCode = stm_registry_add_object(
			      STM_REGISTRY_INSTANCES,
			      NULL,
			      object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_object failed %d for object %x\n", __LINE__, ErrorCode, (int) object_data[1]);

		ErrorCode = stm_registry_add_object(
			      STM_REGISTRY_INSTANCES,
			      NULL,
			      object_data[2]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_object failed %d for object %x\n", __LINE__, ErrorCode, (int) object_data[2]);

		ErrorCode = stm_registry_add_connection(
			      object_data[2],
			      "connect",
			      object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s\n",
				      __LINE__, (int) object_data[2], "connect");
		ErrorCode = stm_registry_add_connection(
			      object_data[0],
			      "connect",
			      object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s\n",
				      __LINE__, (int) object_data[0], "connect");

		ErrorCode = stm_registry_remove_connection(
			      object_data[0],
			      "connect");
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_connection failed for %x for connection %s error %d\n ",
				      __LINE__, (unsigned int) object_data[0], "connect", ErrorCode);

		ErrorCode = stm_registry_add_connection(
			      object_data[0],
			      "connect",
			      object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s\n",
				      __LINE__, (int) object_data[0], "connect");
		ErrorCode = stm_registry_add_connection(
			      object_data[1],
			      "connect",
			      object_data[0]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s\n",
				      __LINE__, (int) object_data[1], "connect");
		ErrorCode = stm_registry_add_connection(
			      object_data[1],
			      "connect",
			      object_data[2]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_add_connection failed for %x for connection %s Expected !!!!\n ",
				      __LINE__, (int) object_data[1], "connect");
		else
			pr_info("<%d>stm_registry_add_connection should fail for %x for connection %s\n",
				      __LINE__, (int) object_data[1], "connect");

		ErrorCode = stm_registry_remove_connection(
			      object_data[1],
			      "connect");
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_connection failed for %x for connection %s error %d\n ",
				      __LINE__, (unsigned int) object_data[1], "connect", ErrorCode);
		ErrorCode = stm_registry_remove_object(object_data[1]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_object failed %d for object %x error %d\n ",
				      __LINE__, ErrorCode, (unsigned int) object_data[1], ErrorCode);

		ErrorCode = stm_registry_remove_connection(
			      object_data[0],
			      "connect");
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_connection failed for %x for connection %s error %d\n ",
				      __LINE__, (unsigned int) object_data[0], "connect", ErrorCode);
		ErrorCode = stm_registry_remove_connection(
			      object_data[2],
			      "connect");
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_connection failed for %x for connection %s error %d\n ",
				      __LINE__, (unsigned int) object_data[0], "connect", ErrorCode);
		ErrorCode = stm_registry_remove_object(object_data[0]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_object failed %d for object %x error %d\n ",
				      __LINE__, ErrorCode, (unsigned int) object_data[0], ErrorCode);
		ErrorCode = stm_registry_remove_object(object_data[2]);
		if (ErrorCode != 0)
			pr_err("<%d>stm_registry_remove_object failed %d for object %x error %d\n ",
				      __LINE__, ErrorCode, (unsigned int) object_data[2], ErrorCode);
	}

	return 0;
}
int dynamic_test_variable = 1;
int functionality_attribute_dynamic_test(void)
{
	int ErrorCode;
	char type_tag[100];
	int test_buffer;
	int p_actual_size;

	ErrorCode = stm_registry_add_object(
		      STM_REGISTRY_INSTANCES,
		      NULL,
		      object_data[1]);
	if (ErrorCode != 0)
		pr_err("<%d>stm_registry_add_object failed %d for object %x\n", __LINE__, ErrorCode, (int) object_data[1]);

	ErrorCode = stm_registry_add_object(
		      STM_REGISTRY_INSTANCES,
		      NULL,
		      object_data[2]);
	if (ErrorCode != 0)
		pr_err("<%d>stm_registry_add_object failed %d for object %x\n", __LINE__, ErrorCode, (int) object_data[2]);

	ErrorCode = stm_registry_add_attribute(object_data[2],
		      "test_buffer_tag", STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[1]);

	ErrorCode = stm_registry_add_attribute(object_data[1],
		      "demux_test", STM_REGISTRY_ADDRESS,
		      &demux_sink_interface,
		      sizeof(stm_sink_interface_t));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[1]);

	ErrorCode = stm_registry_add_attribute(object_data[1],
		      "dynamic_test", STM_REGISTRY_INT32,
		      &dynamic_test_variable,
		      sizeof(int));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[1]);

	ErrorCode = stm_registry_get_attribute(object_data[1], "dynamic_test", type_tag, sizeof(int), &test_buffer, &p_actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute failed for object %x\n", (int) object_data[9]);
	pr_info("<<<<  Sucessfully  functionality_attribute_dynamic_test  For STM REGISTY is done  <<<<<");
	return 0;
}




int functionality_change_dynamic_variable(void)
{

	dynamic_test_variable++;
	return 0;
}

int functionality_remove_dynamic_test(void)
{
	int ErrorCode;
	ErrorCode = stm_registry_remove_object(object_data[2]);
	if (ErrorCode != 0) {
		pr_err("<%d>stm_registry_remove_object failed %d for object %x error %d\n ",
			      __LINE__, ErrorCode, (unsigned int) object_data[2], ErrorCode);
	}

	ErrorCode = stm_registry_remove_object(object_data[1]);
	if (ErrorCode != 0) {
		pr_err("<%d>stm_registry_remove_object failed %d for object %x error %d\n ",
			      __LINE__, ErrorCode, (unsigned int) object_data[1], ErrorCode);
	}
	pr_info("<<<<  Sucessfully  functionality_remove_dynamic_test  For STM REGISTY is done  <<<<<");
	return 0;
}
/**************************************************************************************************r
****************************Iterator testing *****************************************************/
/**************************************************************************************************/

int iterator_create_and_delete(void)
{
	stm_registry_iterator_h p_iter;
	int ErrorCode = 0;
	char *tag = kmalloc(30, GFP_KERNEL);
	char *buffer = kmalloc(30, GFP_KERNEL);
	char *type_tag = kmalloc(30, GFP_KERNEL);
	int actual_size;
	stm_registry_member_type_t child_type;
	ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, "childofroot", object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);
	ErrorCode = stm_registry_add_attribute(object_data[0],
		      "test_buffer_tag", STM_REGISTRY_INT32,
		      "test buffer content", sizeof("test buffer content"));
	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[0]);

	ErrorCode = stm_registry_add_object(object_data[0], NULL, object_data[3]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[1]);

	ErrorCode = stm_registry_add_object(STM_REGISTRY_INSTANCES, NULL, object_data[1]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_object failed %d for object %x\n", ErrorCode, (int) object_data[1]);

	ErrorCode = stm_registry_add_connection(object_data[0], "connect0-1", object_data[1]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_connection failed for %d for connection %s\n", (int) object_data[0], "connect0-1");
	ErrorCode = stm_registry_add_connection(object_data[1], "connect1-0", object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_add_connection failed for %d for connection %s\n", (int) object_data[1], "connect1-0");

	ErrorCode = stm_registry_get_attribute(object_data[0], "test_buffer_tag", type_tag, 30, buffer, &actual_size);
	if (ErrorCode != 0)
		pr_err("stm_registry_get_attribute failed for %d for connection %s\n", (int) object_data[0], "test_buffer_tag");

	ErrorCode = stm_registry_new_iterator(object_data[0],
		      STM_REGISTRY_MEMBER_TYPE_ATTRIBUTE | STM_REGISTRY_MEMBER_TYPE_CONNECTION,
		      &p_iter);
	if (ErrorCode != 0)
		pr_err("stm_registry_new_iterator failed for object %d\n", (int) object_data[0]);

	do {
		ErrorCode = stm_registry_iterator_get_next(p_iter, tag, &child_type);
		if (ErrorCode != 0)
			pr_err("stm_registry_iterator_get_next done object %x  errorcode %d\n", (int) object_data[0], ErrorCode);
		else
			pr_info(" iterator data tag %s of type %d\n", tag, child_type);
	} while (ErrorCode == 0);

	kfree(tag);

	ErrorCode = stm_registry_delete_iterator(p_iter);
	if (ErrorCode != 0)
		pr_err("stm_registry_delete_iterator failed for object %d\n", (int) object_data[0]);

	ErrorCode = stm_registry_remove_connection(object_data[1], "connect1-0");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_connection failed for %d for connection %s\n", (int) object_data[1], "connect1-0");

	ErrorCode = stm_registry_remove_connection(object_data[0], "connect0-1");
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_connection failed for %d for connection %s\n", (int) object_data[0], "connect0-1");

	ErrorCode = stm_registry_remove_object(object_data[1]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x\n", ErrorCode, (int) object_data[1]);
	ErrorCode = stm_registry_remove_object(object_data[0]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x\n", ErrorCode, (int) object_data[0]);

	ErrorCode = stm_registry_remove_object(object_data[3]);
	if (ErrorCode != 0)
		pr_err("stm_registry_remove_object failed %d for object %x\n", ErrorCode, (int) object_data[3]);
	pr_info("<<<<  Sucessfully  functionality_remove_dynamic_test  For STM REGISTY is done  <<<<<");
	return 0;

}


int functionality_attribute_store_test(void)
{
	int ErrorCode;
	static int test = 20;

	ErrorCode = stm_registry_add_object(
		      STM_REGISTRY_INSTANCES,
		      NULL,
		      object_data[1]);
	if (ErrorCode != 0)
		pr_err("<%d>stm_registry_add_object failed %d for object %x\n", __LINE__, ErrorCode, (int) object_data[1]);


	ErrorCode = stm_registry_add_attribute(object_data[1],
		      "test_store_attribute", STM_REGISTRY_INT32,
		      (void *) &test, sizeof(int));

	if (ErrorCode != 0)
		pr_err("stm_registry_add_attribute failed for object %x\n", (int) object_data[1]);

	return 0;
}
