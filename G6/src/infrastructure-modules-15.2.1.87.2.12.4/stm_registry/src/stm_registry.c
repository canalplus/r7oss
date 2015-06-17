
/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   stm_registry.c
 @brief
*/
#include "registry_internal.h"
#include "registry_datatype.h"
#include "registry_iterator.h"
#include "stm_registry.h"
#include "registry_sysfs.h"

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_object                                                                     */
/*  Parameters:                                                                                 */
/*  [in]   parent       The parent object                                                       */
/*  [in]   tag          Tag for the new object                                                  */
/*  [in]   object       Object to add                                                           */
/*                                                                                              */
/*  Description:                                                                                */
/*  Adds an object to the registry database.                                                    */
/*  The object will be a child of the specified parent object.                                  */
/*  If NULL is specified for the tag parameter, the registry will form a                        */
/*  tag by appending the hex value of object to the tag of the parent object.                   */
/*  The object type will be NULL.                                                               */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0                 No errors                                                                 */
/*  -EEXIST      Object already added                                                           */
/*  -EINVAL      any bad parameter passed                                                       */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_add_object(stm_object_h parent,
			    const char *tag,
			    stm_object_h object)
{
	int error = 0;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	error = stm_registry_add_instance(parent, NULL, tag, object);
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_instance                                                                   */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]   parent       The parent object                                                       */
/*  [in]   type_object  Type object handle                                                      */
/*  [in]   tag          Tag for the new object                                                  */
/*  [in]   object       Object to add                                                           */
/*                                                                                              */
/*  Description:                                                                                */
/*  Adds an component instance to the registry database.                                        */
/*  The object will be a child of the specified parent object.                                  */
/*  The object being added should be an instance of the object type specified                   */
/*  in the object_type parameter. It's tag will be a combination of the tag of the              */
/*  type object and the tag specified in this call. If NULL is specified for the tag parameter, */
/*  the registry will form a tag by appending the hex value of object to                        */
/*  the tag of the type object.                                                                 */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0                 No errors                                                                 */
/*  -EEXIST      Object already added                                                           */
/*  -EINVAL      any bad parameter passed                                                       */
/*  -ENOMEM      memory allocation failed                                                       */
/*  -ENODEV      parent not present in registry                                                 */
/************************************************************************************************/
int stm_registry_add_instance(stm_object_h parent,
			      stm_object_h type_object,
			      const char *tag,
			      stm_object_h object)
{
	int			error = 0;
	struct registrynode		*registry_node_p = NULL;
	struct registrynode		*registry_node_obj_type_p = NULL;
	struct objecttype_info_s	object_type;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (object == NULL) {
		error = -EINVAL;
		goto error_return;
	}

	if (WARN_ON(parent == STM_REGISTRY_ROOT || parent == NULL))
		REGISTRY_ERROR_MSG("Wrong parent Type\n");


	if (tag) {
		if (strlen(tag) >= STM_REGISTRY_MAX_TAG_SIZE) {
			REGISTRY_ERROR_MSG("size of tag (%s) is %d for (%p), must be <= %d\n",
				      tag,
				      strlen(tag),
				      object,
				      STM_REGISTRY_MAX_TAG_SIZE);
			error = -EINVAL;
			goto error_return;
		}
	}

	/* The Object should not be present in the registry*/
	error = registry_internal_find_node(object, &registry_node_p);
	if (error == 0) {
		REGISTRY_ERROR_MSG("Same object present %x\n",
			      (uint32_t) object);
		error = -EEXIST;
		goto error_return;
	}

	if (type_object != NULL) {
		error = registry_internal_find_node(type_object, &registry_node_obj_type_p);
		if (error != 0) {
			REGISTRY_ERROR_MSG("Object Type Not Present %x\n",
				      (uint32_t) type_object);

			error = -ENODEV;
			goto error_return;
		}
	}

	error = registry_internal_alloc_node(&registry_node_p);

	if (error != 0) {
		REGISTRY_ERROR_MSG("Memory alloaction failed %x\n",
			      (uint32_t) type_object);
		goto error_return;
	}

	error = registry_internal_add_object(registry_node_p, object, parent);

	if (error != 0) {
		registry_internal_dealloc_node(registry_node_p);
		goto error_return;
	}

	object_type.object_type_h = type_object;

	if (registry_node_obj_type_p)
		strlcpy(object_type.objecttypetag_p, registry_node_obj_type_p->tag_p, (STM_REGISTRY_MAX_TAG_SIZE));


	take_regnode_lock(registry_node_p);

	error = registry_internal_add_node_info(registry_node_p, REGISTRY_TAG, (void *) tag);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Add Node Info of type REGISTRY_TAG failed !!!\n");
		goto remove_node;
	}

	error = registry_internal_add_node_info(registry_node_p, REGISTRY_PARENT, (void *) parent);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Add Node Info of type REGISTRY_PARENT failed !!!\n");
		goto remove_node;
	}
	error = registry_internal_add_node_info(registry_node_p, REGISTRY_OBJECT_TYPE, (void *) &object_type);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Add Node Info of type REGISTRY_OBJECT_TYPE failed !!!\n");
		goto remove_node;
	}

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	error = registry_sysfs_add_kobject(registry_node_p->tag_p,
		      registry_node_p->Parent_p->kobj,
		      &registry_node_p->kobj);

	if (error != 0) {
		REGISTRY_ERROR_MSG("registry_sysfs_add_kobject Failed !!!\n");
		goto remove_node;
	}

	if (registry_node_obj_type_p && error == 0) {
		error = registry_internal_create_link(registry_node_obj_type_p, registry_node_p);
		if (error != 0) {
			REGISTRY_ERROR_MSG("registry_internal_create_link Failed !!!\n");
			goto remove_node;
		}

	}
#endif

	release_regnode_lock(registry_node_p);
	goto error_return;

remove_node:
	release_regnode_lock(registry_node_p);
	error = stm_registry_remove_object(object);
	error = -ENODEV;
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_remove_object                                                                  */
/*  Parameters:                                                                                 */
/*  [in]   object       Object to remove                                                        */
/*                                                                                              */
/*  Description:                                                                                */
/*  Remove an component instance to the registry database.                                      */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0                 No errors                                                                 */
/*  -ENODEV      Object not present in registry                                                 */
/*  -EINVAL Any bad parameter passed                                                            */
/************************************************************************************************/
int stm_registry_remove_object(stm_object_h object)
{
	int		error = 0;
	struct registrynode	*registry_node_p = NULL;

	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (object == NULL) {
		error = -EINVAL;
		goto error_return;
	}

	error = registry_internal_entry_report(object, true, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}


	error = registry_internal_exit_report(registry_node_p);

	error = registry_internal_remove_object(registry_node_p, object);
	if (error != 0)
		REGISTRY_ERROR_MSG("registry_internal_remove_object Failed !!!\n");


error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_object_tag                                                                 */
/*  Parameters:                                                                                 */
/*  [in]   parent       The parent object                                                       */
/*  [in]   object       The object whose tag to get.                                            */
/*  [in]   tag          Place to store tag for the object                                       */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets an tag for the object from the registry database.                                      */
/*  The object will be a child of the specified parent object.                                  */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0                 No errors                                                                 */
/*  -ENODEV      Particular element not in registry                                             */
/*  -EINVAL any bad parameter passed                                                            */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_get_object_tag(stm_object_h object,
				const char *tag)
{
	int		error = 0;
	struct registrynode	*registry_node_p = NULL;

	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL || tag == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		return -ENODEV;
	}

	take_regnode_lock(registry_node_p);

	error = registry_internal_get_node_info(registry_node_p, REGISTRY_TAG, (void *) tag);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Get Node Info failed !!! %x\n",
			      (uint32_t) object);
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_object                                                                     */
/*  Parameters:                                                                                 */
/*  [in]   parent       The parent object                                                       */
/*  [in]   tag          Tag for the new object                                                  */
/*  [out]  p_object     Place to store the object.                                              */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets an object from the registry database.                                                  */
/*  The object will be a child of the specified parent object.                                  */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Particular element not in registry                                             */
/*  -EINVAL      any bad parameter passed                                                       */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_get_object(stm_object_h parent,
			    const char *tag,
			    stm_object_h *p_object)
{
	int		error = 0;
	struct registrynode	*registry_node_p = NULL;
	struct object_info_s	object_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (parent == NULL || tag == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(parent, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) parent);
		return -ENODEV;
	}

	take_regnode_lock(registry_node_p);

	object_info.object_p = p_object;
	object_info.tag_p = (char *) tag;
	error = registry_internal_get_node_info(registry_node_p,
		      REGISTRY_OBJECT, (void *) &object_info);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Get Node Info Failed !!! %s\n",
			      tag);
		goto release_lock;
	}


release_lock:
	release_regnode_lock(registry_node_p);

	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_object_parent                                                              */
/*  Parameters:                                                                                 */
/*  [in]   object       Object whose parent to retrieve                                         */
/*  [in]   tag          Tag for the new object                                                  */
/*  [out]  p_object     Place to store the object.                                              */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets the parent of an object from the registry database.                                    */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Particular element not in registry                                             */
/*  -EINVAL      any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_get_object_parent(stm_object_h object,
				   stm_object_h *p_parent)
{
	int		error = 0;
	struct registrynode	*registry_node_p = NULL;

	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL) {
		error = -EINVAL;
		REGISTRY_ERROR_MSG("input object is NULL\n");
		goto error_return;
	}

	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		return -ENODEV;
	}
	take_regnode_lock(registry_node_p);

	error = registry_internal_get_node_info(registry_node_p,
		      REGISTRY_PARENT,
		      (void *) p_parent);

	if (error != 0) {
		REGISTRY_ERROR_MSG("Get Node Info Failed !!! %x\n",
			      (uint32_t) p_parent);
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);

	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_object_type                                                                */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]   object         Object whose object type to retrieve                                  */
/*  [out]  p_type_object  Place to store the object type.                                       */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets object type of an object from the registry database.                                   */
/*  If the object was added with the stm_registry_add_object                                    */
/*  function the type returned will be NULL.                                                    */
/*  If it was added with the stm_registry_add_instance function the type will                   */
/*  be the object passed in the type_object parameter of the call to that function.             */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Particular element not in registry                                             */
/*  -EINVAL      any bad parameter passed                                                       */
/*  -EEXIST      Attribute already present in the registry                                      */
/************************************************************************************************/
int stm_registry_get_object_type(stm_object_h object,
				 stm_object_h *p_type_object)
{
	int	error = 0;
	struct registrynode *registry_node_p = NULL;

	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	error = registry_internal_get_node_info(registry_node_p,
		      REGISTRY_OBJECT_TYPE,
		      (void *) p_type_object);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Get Node Info Failed !!! %x\n",
			      (uint32_t) p_type_object);
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_attribute                                                                  */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  object   Object handle                                                                */
/*  [in]  tag      Tag identifying the data to add  < STM_REGISTRY_MAX_TAG_SIZE                */
/*  [in]  type_tag Tag indicating the type of data (used for sysfs) < STM_REGISTRY_MAX_TAG_SIZE*/
/*  [in]  buffer   Buffer containing the data to store                                          */
/*  [in]  size     Size of data in bytes                                                        */
/*                                                                                              */
/*  Description:                                                                                */
/*  Stores attribute data associated with the specified object with the specified tag           */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/*  -EEXIST      Attribute already present in the registry                                      */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_add_attribute(stm_object_h object,
			       const char *tag, /*attribute identifier*/
			       const char *type_tag,
			       void *buffer,
			       int size)
{
	int			error = 0;
	struct registrynode		*registry_node_p = NULL;
	struct attribute_info_s	attribute_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");
	REGISTRY_DEBUG_MSG("Buffer : %d  , size : %d\n", *((int32_t *) buffer), size);


	if (object == NULL || tag == NULL ||
		(strlen(tag) >= STM_REGISTRY_MAX_TAG_SIZE) ||
		(strlen(type_tag) >= STM_REGISTRY_MAX_TAG_SIZE) || (size == 0)) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}


	take_regnode_lock(registry_node_p);

	/* check again for deletion status before adding */
	if (registry_node_p->deletion_started == true) {
		REGISTRY_ERROR_MSG("Deletion started !!!\n");
		goto release_lock;
	}
	REGISTRY_DEBUG_MSG("after find node : regnode : %x\n", (uint32_t) registry_node_p);

	attribute_info.usize = size;
	attribute_info.buffer_p = buffer;
	strlcpy(attribute_info.tag_p, tag, STM_REGISTRY_MAX_TAG_SIZE);
	strlcpy(attribute_info.datatypetag_p, type_tag, STM_REGISTRY_MAX_TAG_SIZE);

	REGISTRY_DEBUG_MSG("Attribute_info.buffer_p : %d  , size : %d\n", *((int32_t *) attribute_info.buffer_p), size);
	error = registry_internal_add_node_info(registry_node_p,
		      REGISTRY_ATTRIBUTE, (void *) &attribute_info);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Add Node Info failed !!!\n");
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_remove_attribute                                                               */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  object  Object handle                                                                 */
/*  [in]  tag     Tag identifying the data to remove                                            */
/*                                                                                              */
/*  Description:                                                                                */
/*  Removes attribute data associated with the specified object with the specified tag.         */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_remove_attribute(stm_object_h object,
				  const char *tag)
{
	int error = 0;
	struct registrynode *registry_node_p = NULL;


	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (object == NULL || tag == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		return -ENODEV;
	}

	take_regnode_lock(registry_node_p);

	error = registry_internal_remove_node_info(registry_node_p,
		      REGISTRY_ATTRIBUTE,
		      (void *) tag);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Remove Node Info Failed !!! %s\n", tag);
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);

	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_attribute                                                                  */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*                                                                                              */
/*  [in]  object        Object handle                                                           */
/*  [in]  tag           Tag identifying the data to get                                         */
/*  [out] type_tag      Tag identifying the data type for the attribute                        */
/*  [in]  buffer_size   Size of buffer in bytes                                                 */
/*  [out] buffer        Buffer to receive the data retrieved                                    */
/*  [out] p_actual_size Place to store the actual number of bytes retrieved                     */
/*                                                                                              */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets data associated with the specified object with the specified tag                       */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_get_attribute(stm_object_h	object,
			       const char		*tag,
			       char		*type_tag,
			       int		buffer_size,
			       void		*buffer,
			       int *p_actual_size)
{
	int error = 0;
	struct registrynode *registry_node_p = NULL;
	struct attribute_info_s attribute_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL || tag == NULL || p_actual_size == NULL) {
		error = -EINVAL;
		goto error_return;
	}

	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	attribute_info.buffer_p = buffer;
	attribute_info.usize = buffer_size;
	strlcpy(attribute_info.tag_p, tag, STM_REGISTRY_MAX_TAG_SIZE);
	error = registry_internal_get_node_info(registry_node_p,
		      REGISTRY_ATTRIBUTE, (void *) &attribute_info);
	if (error != 0) {
		/*	REGISTRY_DEBUG_MSG("<%s>: <%d> Get Node Info Failed !!! %s\n",
		 *		__FUNCTION__, __LINE__, tag);
		 */
		goto release_lock;
	}

	strlcpy(type_tag, attribute_info.datatypetag_p, STM_REGISTRY_MAX_TAG_SIZE);
	*p_actual_size = attribute_info.usize;

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_connection                                                                 */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in] source_object     Object handle                                                        */
/*  [in] tag               Tag indicating the type of connection                                */
/*  [in] connected_object  Object to connect                                                    */
/*                                                                                              */
/*                                                                                              */
/*  Description:                                                                                */
/*  Adds a connection between the specified objects                                             */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/*  -EEXIST      Attribute already present in the registry                                      */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_add_connection(stm_object_h object,
				const char *tag,
				stm_object_h connected_object)
{
	int		error_code = 0;
	struct registrynode	*registry_node_p = NULL;
	struct connection_info_s connect_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL || tag == NULL || (connected_object == NULL)) {
		error_code = -EINVAL;
		goto error_return;
	}

	error_code = registry_internal_entry_report(object, false, &registry_node_p);

	if (error_code != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error_code,
			      (uint32_t) object);
		error_code = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	/* check again for deletion status before adding */
	if (registry_node_p->deletion_started == true) {
		REGISTRY_ERROR_MSG("Deletion started !!!\n");
		goto release_lock;
	}
	strlcpy(connect_info.tag_p, tag, STM_REGISTRY_MAX_TAG_SIZE);
	connect_info.connected_object = connected_object;

	error_code = registry_internal_add_node_info(
		      registry_node_p,
		      REGISTRY_CONNECTION,
		      (void *) &connect_info);
	if (error_code != 0) {
		REGISTRY_ERROR_MSG("\n");
		goto release_lock;
	}
release_lock:
	release_regnode_lock(registry_node_p);

	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error_code;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_remove_connection                                                              */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  object  Object handle                                                                 */
/*  [in]  tag     Tag identifying the data to remove                                            */
/*                                                                                              */
/*  Description:                                                                                */
/*  Removes connection associated with the specified object with the specified tag.             */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_remove_connection(stm_object_h object,
				   const char *tag)
{
	int error = 0;
	struct registrynode *registry_node_p = NULL;


	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL || tag == NULL) {
		error = -EINVAL;
		goto error_return;
	}

	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	error = registry_internal_remove_node_info(registry_node_p,
		      REGISTRY_CONNECTION, (void *) tag);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Remove Node Info Failed !!! %s\n", tag);
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_connection                                                                 */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*                                                                                              */
/*  [in]  object                Object handle                                                   */
/*  [in]  tag                   Tag of connection to retrieve                                   */
/*  [out] p_connected_object    Object connected with the specified tag                         */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets data associated with the specified object with the specified tag                       */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_get_connection(stm_object_h object,
				const char			*tag,
				stm_object_h			*p_connected_object)
{
	int error = 0;
	struct registrynode *registry_node_p = NULL;
	struct connection_info_s connect_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (object == NULL || tag == NULL || p_connected_object == NULL) {
		error = -EINVAL;
		goto error_return;
	}


	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);


	strlcpy(connect_info.tag_p, tag, STM_REGISTRY_MAX_TAG_SIZE);
	connect_info.connected_object = p_connected_object;


	error = registry_internal_get_node_info(registry_node_p,
		      REGISTRY_CONNECTION, (void *) &connect_info);
	if (error != 0) {
		REGISTRY_DEBUG_MSG("Get Node Info Failed !!! %s\n", tag);
		goto release_lock;
	}

	if (error == 0)
		*p_connected_object = connect_info.connected_object;

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_dumptheregistry                                                                */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*                                                                                              */
/*  Description:                                                                                */
/*  This function is provided to dump the whole registry for debug purpose                      */
/*  and registry sanity checking.This function is also present in  procfs at                    */
/*  "cat /sys/kernel/debug/stm_registry/Dump_registry "                                         */
/*                                                                                              */
/*  Return Value :                                                                              */
/************************************************************************************************/
void stm_registry_dumptheregistry(void)
{
#if (defined SDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES)
	registry_internal_dump_registry();
#endif
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_data_type                                                                  */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  tag  Data type tag                                                                    */
/*  [in]  def  Definition of the new data type                                                  */
/*                                                                                              */
/*  Description:                                                                                */
/*  Adds a data type with the specified tag.                                                    */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0         No errors                                                                         */
/*  -EEXIST   Data type already added                                                           */
/*  -ENOMEM   memory allocation failed                                                          */
/*  -EINVAL   Any bad parameter passed                                                          */
/************************************************************************************************/
int stm_registry_add_data_type(const char *tag,
			       stm_registry_type_def_t *def)
{
	int	error = 0;
	REGISTRY_DEBUG_MSG("[API IN]\n");

	if (tag == NULL || def == NULL || (strlen(tag) >= STM_REGISTRY_MAX_TAG_SIZE)) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_add_data_type(tag, (void *) def);

error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_add_data_type                                                                  */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  tag  Data type tag                                                                    */
/*                                                                                              */
/*  Description:                                                                                */
/*  Remove a data type with the specified tag.                                                  */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -EEXIST      Data type already added                                                        */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_remove_data_type(const char *tag)
{
	int error = 0;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (tag == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_remove_data_type(tag);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_get_data_type                                                                  */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]   tag  Data type tag                                                                   */
/*  [out]  def  Place to store definition of the new data type                                  */
/*                                                                                              */
/*  Description:                                                                                */
/*  Get a data type with the specified tag.                                                     */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -EEXIST      Data type already added                                                        */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_get_data_type(const char *tag,
			       stm_registry_type_def_t *def)
{
	int error = 0;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (tag == NULL || def == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_get_data_type(tag, def);

error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}


/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_new_iterator                                                                   */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  object  Object over whose members to iterate                                          */
/*  [in]  types   ORed mask of member types to match                                            */
/*  [out] p_iter  Place to store the new iterator                                               */
/*                                                                                              */
/*  Description:                                                                                */
/*  Creates an iterator for the specified object that can iterate over the object's members.    */
/*  The types of members returned by the iterator is determined by the value of the types       */
/*  parameter which should be an ORed combination of values from stm_registry_member_type_t.    */
/*  Only members of the specified types will be returned. To get all members regardless         */
/*  of their types specify STM_REGISTRY_MEMBER_TYPE_ALL for types.                              */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -ENOMEM      memory allocation failed                                                       */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_new_iterator(stm_object_h object,
			      stm_registry_member_type_t types,
			      stm_registry_iterator_h *p_iter)
{
	int		error = 0;
	struct registrynode	*registry_node_p = NULL;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (!object || !types) {
		error = -EINVAL;
		goto error_return;
	}
	/* The Object should  be present in the registry*/
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	error = registry_internal_new_iterator(registry_node_p, types, p_iter);
	if (error != 0) {
		REGISTRY_ERROR_MSG("error in registry_internal_new_iterator\n");
		goto release_lock;
	}
	release_regnode_lock(registry_node_p);
	/* call registry_internal_exit_report() while deleting the iterator */
	return error;

release_lock:
	release_regnode_lock(registry_node_p);

	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_delete_iterator                                                                */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in] iter  iterator handle to be deleted                                                    */
/*                                                                                              */
/*  Description:                                                                                */
/*  Deletes an iterator                                                                         */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0          No errors                                                                        */
/*  -EINVAL    Any bad parameter passed                                                         */
/************************************************************************************************/
int stm_registry_delete_iterator(stm_registry_iterator_h iter)
{
	int error = 0;

	REGISTRY_DEBUG_MSG("[API IN]\n");
	/* Delete the usr cnt set in stm_registry_new_iterator() */
	registry_internal_exit_report(iter->registry_node_p);
	error = registry_internal_delete_iterator(iter);
	REGISTRY_DEBUG_MSG("[API OUT]\n");

	return error;
}
/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_iterator_get_next                                                              */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  iter         Iterator handle                                                          */
/*  [out] tag          Tag for matching member                                                  */
/*  [out] member_type  Type of member                                                           */
/*                                                                                              */
/*  Description:                                                                                */
/*  Gets the next matching member from an iterator and returns the member's tag                 */
/*  and member type. After getting the tag and member type of a matching member the             */
/*  actual member can be retrieved by using the corresponding get function.                     */
/*  For STM_REGISTRY_MEMBER_TYPE_OBJECT members this will be stm_registry_get_object,           */
/*  for STM_REGISTRY_MEMBER_TYPE_ATTRIBUTE members this will be stm_registry_get_attribute,     */
/*  and for STM_REGISTRY_MEMBER_TYPE_CONNECTION                                                 */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      No more member in th iterator                                                  */
/*  -EINVAL      Any bad parameter passed                                                       */
/************************************************************************************************/
int stm_registry_iterator_get_next(stm_registry_iterator_h iter,
				   char *tag,
				   stm_registry_member_type_t *p_child_type)
{
	int	error = 0;
	REGISTRY_DEBUG_MSG("[API IN]\n");
	if (tag == NULL || iter == NULL) {
		error = -EINVAL;
		goto error_return;
	}
	take_regnode_lock(iter->registry_node_p);
	error = registry_internal_get_next_iterator(iter, tag, p_child_type);
	release_regnode_lock(iter->registry_node_p);

error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}

/************************************************************************************************/
/*  Function :                                                                                  */
/*  stm_registry_update_attribute                                                               */
/*                                                                                              */
/*  Parameters:                                                                                 */
/*  [in]  object   Object handle                                                                */
/*  [in]  tag      Tag identifying the data to add  < STM_REGISTRY_MAX_TAG_SIZE                 */
/*  [in]  type_tag Tag indicating the type of data (used for sysfs) < STM_REGISTRY_MAX_TAG_SIZE */
/*  [in]  buffer   Buffer containing the data to store                                          */
/*  [in]  size     Size of data in bytes                                                        */
/*                                                                                              */
/*  Description:                                                                                */
/*  Updates attribute data associated with the specified object with the specified tag.         */
/*                                                                                              */
/*  Return Value :                                                                              */
/*  0            No errors                                                                      */
/*  -ENODEV      Object  not in registry                                                        */
/*  -EINVAL      Any bad parameter passed                                                       */
/*  -ENOMEM      memory allocation failed                                                       */
/************************************************************************************************/
int stm_registry_update_attribute(stm_object_h object,
			       const char *tag, /*attribute identifier*/
			       const char *type_tag,
			       void *buffer,
			       int size)
{
	int error = 0;
	struct registrynode *registry_node_p = NULL;
	struct attribute_info_s attribute_info;


	REGISTRY_DEBUG_MSG("[API IN]\n");
	REGISTRY_DEBUG_MSG("Buffer : %d  , size : %d\n", *((int32_t *) buffer), size);

	if (object == NULL || tag == NULL ||
		(strlen(tag) >= STM_REGISTRY_MAX_TAG_SIZE) ||
		(strlen(type_tag) >= STM_REGISTRY_MAX_TAG_SIZE) || (size == 0)) {
		error = -EINVAL;
		goto error_return;
	}
	error = registry_internal_entry_report(object, false, &registry_node_p);
	if (error != 0) {
		REGISTRY_ERROR_MSG("Error %d returned from Object %x\n", error,
			      (uint32_t) object);
		error = -ENODEV;
		goto error_return;
	}

	take_regnode_lock(registry_node_p);

	/* check again for deletion status before updating */
	if (registry_node_p->deletion_started == true) {
		REGISTRY_ERROR_MSG("Deletion started !!!\n");
		goto release_lock;
	}

	attribute_info.usize = size;
	attribute_info.buffer_p = buffer;
	strlcpy(attribute_info.tag_p, tag, STM_REGISTRY_MAX_TAG_SIZE);
	strlcpy(attribute_info.datatypetag_p, type_tag, STM_REGISTRY_MAX_TAG_SIZE);

	REGISTRY_DEBUG_MSG("Attribute_info.buffer_p : %d  , size : %d\n",
					    *((int32_t *) attribute_info.buffer_p), size);
	error = registry_internal_update_attribute(registry_node_p,
		       &attribute_info);

	if (error != 0) {
		REGISTRY_ERROR_MSG("Update Attribute failed !!!\n");
		goto release_lock;
	}

release_lock:
	release_regnode_lock(registry_node_p);
	registry_internal_exit_report(registry_node_p);
error_return:
	REGISTRY_DEBUG_MSG("[API OUT]\n");
	return error;
}
