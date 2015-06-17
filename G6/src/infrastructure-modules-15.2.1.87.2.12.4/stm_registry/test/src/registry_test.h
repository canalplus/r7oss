#ifndef _REGISTRY_TEST_H
#define _REGISTRY_TEST_H

#include "stm_registry.h"

#define  REGISTRY_TEST_PRINTS				1
#define  REGISTRY_TEST_DEBUG				0

#if  REGISTRY_TEST_PRINTS
    #define  registry_test_print(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#else
    #define registry_test_print(fmt, ...)  do {} while (0)
#endif

#if  REGISTRY_TEST_DEBUG
    #define registry_test_debug(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#else
    #define registry_test_debug(fmt, ...) do {} while (0)
#endif

/********Functionality test*****************/

int functionality_object_add_remove_test(void);
int functionality_attribute_add_remove_test(void);
int functionality_delete_object_with_attribute_and_children(void);
int functionality_connection_add_and_remove(void);
int functionality_get_info_test(void);
int functionality_cleanup(void);
int functionality_get_attribute_test(void);
int functionality_backlink_test(void);
int functionality_attribute_test(void);
int functionality_remove_dynamic_test(void);
int functionality_change_dynamic_variable(void);
int functionality_attribute_dynamic_test(void);
int functionality_attribute_store_test(void);
/*************Iterator test ********/

int iterator_create_and_delete(void);


/*************Memory leak test***********/

void Memoryleak_object_add_remove_test(void);
void Memoryleak_attribute_add_remove_test(void);

void Memoryleak_connection_add_remove_test(void);
void Memoryleak_child_add_remove_test(void);
void Memoryleak_reverse_remove_object_test(void);
void Memoryleak_reverse_remove_instance_test(void);


/*****************Stress test **********************/
void registry_stress_test(void);

/****************data type test *******************/
void datatype_add_remove_test(void);

void registry_test_modprobe_func(void);

#endif
