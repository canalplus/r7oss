/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided "AS IS", WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   registry_internal.c
 @brief
*/

#include "registry_internal.h"
#include "registry_sysfs.h"
#include "registry_rbtree.h"

/*******************************************************************/
/******* DON'T CALL INTERNAL API FROM ANOTHER INTERNAL API *********/
/*******************************************************************/


extern struct rb_root registry_tree;
extern infra_os_semaphore_t g_registry_sem_lock; /* global semaphore */

int	take_regnode_lock(struct registrynode	*regnode)
{
	return	 infra_os_sema_wait(regnode->sem_lock);
}

int	release_regnode_lock(struct registrynode *regnode)
{
	return	 infra_os_sema_signal(regnode->sem_lock);
}



int registry_internal_add_object(struct registrynode *Registry,
				 stm_object_h Object,
				 stm_object_h Parent)

{
	struct registrynode	*reg_node = NULL;

	REGISTRY_DEBUG_MSG("object : %x , parent : %x\n", (uint32_t) Object, (uint32_t) Parent);

	infra_os_sema_wait(g_registry_sem_lock);

	/* call registry search tree for parent
	* if parent found ,call insert tree with Object ptr
	* if parent  not found ; signal semaphore and return ENODEV error ;
	*/

	reg_node = registry_rbtree_search(&registry_tree, Parent);
	if (reg_node == NULL) {
		infra_os_sema_signal(g_registry_sem_lock);
		REGISTRY_ERROR_MSG("Parent(%x) does not exist\n", (uint32_t) Parent);
		return -ENODEV;
	}

	Registry->connection_np = NULL;
	Registry->attribute_np = NULL;
	Registry->children_np = NULL;
	Registry->Parent_p = reg_node; /*Parent;*/
	Registry->object_h = Object;
	Registry->object_type.object_type_h = NULL;
	Registry->object_type.objecttypetag_p[0] = '\0';
	Registry->connected_obj_count = 0;
	Registry->usr_cnt = 0;
	Registry->deletion_started = false;

	/* Deallocate the tag in registry_interanl_clean_node*/
	/*TODO:: we can use the array of character instead of malloc*/
	Registry->tag_p = (char *) infra_os_malloc(STM_REGISTRY_MAX_TAG_SIZE);
	if (!Registry->tag_p) {
		infra_os_sema_signal(g_registry_sem_lock);
		return -ENOMEM;
	}

	registry_rbtree_insert(&registry_tree, Registry);


	infra_os_sema_initialize(&Registry->sem_lock, 1);
	infra_os_sema_initialize(&Registry->no_active_usr_sem_p, 0);
	infra_os_sema_signal(g_registry_sem_lock);

	return 0;

}

int registry_internal_clean_node(struct registrynode *Registry_p)
{
	int err = 0;

	struct registrynode_ctx *parent_children_np;
	struct registrynode_ctx *parent_prevchild_np;
	struct registrynode *registry_parent_p;


	REGISTRY_DEBUG_MSG("Registry node : 0x%x\n", (uint32_t) Registry_p);

	/*Remove the attribute of this object */
	/*Registrynode->attribute_np gets updated */
	/* from the registry_internal_remove_node_info*/
	/* We always delete first element in every interation*/
	while (Registry_p->attribute_np != NULL) {
		err = registry_internal_remove_node_info(Registry_p,
			      REGISTRY_ATTRIBUTE,
			      (void *) Registry_p->attribute_np->attribute_info.tag_p);
		if (err != 0)
			REGISTRY_ERROR_MSG("registry_internal_remove_node_info failed\n");

	}


	registry_parent_p = Registry_p->Parent_p;
	parent_prevchild_np = parent_children_np = registry_parent_p->children_np;

	take_regnode_lock(Registry_p->Parent_p);
	/*Remove the object to be deleted form the parent's registry entry*/
	while (parent_children_np != NULL) {
		/* remove the object to be deleted form the parent's registry entry*/
		if (parent_children_np->registry_p->object_h == Registry_p->object_h) {
			if (parent_children_np == registry_parent_p->children_np)
				registry_parent_p->children_np = parent_children_np->node_next_np;
			else
				parent_prevchild_np->node_next_np = parent_children_np->node_next_np;

			/*deallocate the child */
			infra_os_free((void *) parent_children_np);
			break;
		}
		parent_prevchild_np = parent_children_np;
		parent_children_np = parent_children_np->node_next_np;
	}

	release_regnode_lock(Registry_p->Parent_p);
	return err;
}

int registry_internal_remove_object(struct registrynode *Registrynode, stm_object_h Object)
{


	/* search rbtree for this object .
	* if present , clean node
	* remove object from rbtree
	*/

	struct registrynode *reg_node = NULL;


	REGISTRY_DEBUG_MSG("registry_node 0x%x , Object : 0x%x\n", (uint32_t) Registrynode, (uint32_t) Object);


	infra_os_sema_wait(g_registry_sem_lock);

	reg_node = registry_rbtree_search(&registry_tree, Object);
	if (reg_node == NULL) {
		REGISTRY_ERROR_MSG("Object not found\n");
		infra_os_sema_signal(g_registry_sem_lock);
		return -ENODEV;
	}

	registry_rbtree_remove_node(&registry_tree, Object);

	if (Registrynode->deletion_started == true && Registrynode->usr_cnt == 0)
		infra_os_sema_signal(g_registry_sem_lock);
	else {
		infra_os_sema_signal(g_registry_sem_lock);
		infra_os_sema_wait(Registrynode->no_active_usr_sem_p);
	}

	take_regnode_lock(reg_node);
	registry_internal_clean_node(reg_node);

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	registry_internal_delete_link(reg_node);

	registry_sysfs_remove_kobject(&reg_node->kobj); /* This function retur void */
#endif

	/*TODO : if we use the char array then remove this */


	infra_os_free(reg_node->tag_p);

	release_regnode_lock(reg_node);
	infra_os_sema_terminate(reg_node->sem_lock);
	infra_os_sema_terminate(reg_node->no_active_usr_sem_p);

	infra_os_free(reg_node); /* allocated in alloc_node*/


	return 0;
}

int registry_internal_add_node_info(struct registrynode *Registrynode,
				    Info_type_flag Infoflag,
				    void *Info_p)
{
	int err = 0;


	REGISTRY_DEBUG_MSG("Regsitrynode (%x)\n", (uint32_t) Registrynode);

	switch (Infoflag) {
	case REGISTRY_TAG:
		if ((char *) Info_p) {
			strlcpy(Registrynode->tag_p,
				      (char *) Info_p,
				      STM_REGISTRY_MAX_TAG_SIZE);
		} else {
			registry_intenal_string_create(Registrynode->tag_p,
				      Registrynode->Parent_p->tag_p,
				      Registrynode->object_h);
		}
		break;

	case REGISTRY_PARENT:
	{
		struct registrynode_ctx *parentchild_np;
		struct registrynode_ctx *child_np;

		child_np = (struct registrynode_ctx *) infra_os_malloc(sizeof(struct registrynode_ctx));
		child_np->registry_p = Registrynode;
		child_np->node_next_np = NULL;


		/*add the parent handle to the child node*/
		Registrynode->parent_h = (stm_object_h) Info_p;

		/* add the object to the child list of the parent*/
		if (Registrynode->Parent_p) {
			take_regnode_lock(Registrynode->Parent_p);
			parentchild_np = Registrynode->Parent_p->children_np;
			if (Registrynode->Parent_p->children_np == NULL) {
				Registrynode->Parent_p->children_np = child_np;
				release_regnode_lock(Registrynode->Parent_p);
				break;
			}


			while (parentchild_np->node_next_np)
				parentchild_np = parentchild_np->node_next_np;

			parentchild_np->node_next_np = child_np;
			release_regnode_lock(Registrynode->Parent_p);
		} else {
			REGISTRY_ERROR_MSG(" <%s>:<%d> Parent not Valid\n", __FUNCTION__, __LINE__);
			infra_os_free((void *) child_np);
			err = -EINVAL;
		}
	}
		break;


	case REGISTRY_OBJECT_TYPE:
	{
		struct objecttype_info_s *object_type;
		object_type = (struct objecttype_info_s *) Info_p;
		Registrynode->object_type.object_type_h = object_type->object_type_h;

		if (Registrynode->object_type.object_type_h) {
			strlcpy(Registrynode->object_type.objecttypetag_p,
				      object_type->objecttypetag_p, STM_REGISTRY_MAX_TAG_SIZE);
		}
	}
		break;

	case REGISTRY_OBJECT:
		/* nothing to be done */
		/* Object addtion to be handled by seperate API*/
		break;

	case REGISTRY_CONNECTION:
	{
		struct connection_info_s *connectioninfo_p;
		struct connection_node_s *connect_fortrvs_np = NULL; /* for traversing the link list*/
		struct connection_node_s *connection_np; /* node to be stored in the link list*/
		struct connection_node_s *connect_prev_np = NULL;

		/* Info of the node to be stored*/
		connectioninfo_p = (struct connection_info_s *) Info_p;

		connect_prev_np = connect_fortrvs_np = Registrynode->connection_np;

		while (connect_fortrvs_np != NULL) {
			if (!strcmp(connect_fortrvs_np->connect_info.tag_p, connectioninfo_p->tag_p)) {
				REGISTRY_ERROR_MSG(" <%s>:<%d> connection Already Present %s\n",
					      __FUNCTION__, __LINE__, connect_fortrvs_np->connect_info.tag_p);
				return -EEXIST;
			}

			connect_prev_np = connect_fortrvs_np;
			connect_fortrvs_np = connect_fortrvs_np->connection_next_np;
		}

		err = registry_internal_fill_connection_node(&connection_np,
			      connectioninfo_p);
		if (err != 0)
			break;


		err = registry_internal_create_connection_symblink(
			      Registrynode,
			      connectioninfo_p);
		if (err != 0) {
			REGISTRY_ERROR_MSG(" <%s>:<%d> Symblink Failed %s\n",
				      __FUNCTION__, __LINE__, connectioninfo_p->tag_p);
			infra_os_free((void *) connection_np);
			connection_np = NULL;
			break;
		}

		if (connect_fortrvs_np == Registrynode->connection_np)
			Registrynode->connection_np = connection_np;
		else
			connect_prev_np->connection_next_np = connection_np;

	}
		break;

	case REGISTRY_ATTRIBUTE:
	{
		struct attribute_node_s *attr_np; /* node to be stored in the link list*/
		struct attribute_node_s *attr_prev_np = NULL;
		struct attribute_info_s *attrinfo_p = (struct attribute_info_s *) Info_p; /* Info of the node to be stored*/
		struct attribute_node_s *attr_fortrvs_np = NULL; /* for traversing the link list*/


		REGISTRY_DEBUG_MSG("node info is REGISTRY_ATTRIBUTE\n");
		attr_prev_np = attr_fortrvs_np = Registrynode->attribute_np;

		while (attr_fortrvs_np != NULL)		{
			if (!strcmp(attr_fortrvs_np->attribute_info.tag_p, attrinfo_p->tag_p)) {
				REGISTRY_ERROR_MSG(" Attribute Already present %s\n",
					      attrinfo_p->tag_p);
				return -EEXIST;
			}
			attr_prev_np = attr_fortrvs_np;
			attr_fortrvs_np = attr_fortrvs_np->attribute_next_np;
		}
		err = registry_internal_fill_attribute_node(
			      &attr_np,
			      attrinfo_p,
			      Registrynode);
		if (err != 0)
			break;

		if (attr_fortrvs_np == Registrynode->attribute_np)
			Registrynode->attribute_np = attr_np;
		else
			attr_prev_np->attribute_next_np = attr_np;


#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
		err = registry_sysfs_add_attribute(
			      attr_np,
			      Registrynode->kobj,
			      attr_np->attribute_info.tag_p);
		if (err != 0) {
			REGISTRY_ERROR_MSG(" <%s>:<%d> Attribute Sysfs failed %s\n",
				      __FUNCTION__, __LINE__, attrinfo_p->tag_p);
			err = -EINVAL;
		}
#endif
	}
		break;


	default:
		REGISTRY_ERROR_MSG("<%s>:<%d> Invalid case ", __FUNCTION__, __LINE__);
		err = -EINVAL;

	}

	return err;
}

int registry_internal_get_node_info(struct registrynode *Registrynode,
				    Info_type_flag Infoflag, void *Info_p)
{
	int err = 0;

	switch (Infoflag) {
	case REGISTRY_OBJECT_TYPE:
		*((stm_object_h *) Info_p) = Registrynode->object_type.object_type_h;
		break;

	case REGISTRY_TAG:
		strlcpy((char *) Info_p, Registrynode->tag_p, STM_REGISTRY_MAX_TAG_SIZE);
		break;
	case REGISTRY_PARENT:
		*((stm_object_h *) Info_p) = Registrynode->Parent_p->object_h;
		break;
	case REGISTRY_OBJECT:
	{
		struct registrynode_ctx *children_np;
		struct object_info_s *objectinfo = (struct object_info_s *) Info_p;

		children_np = Registrynode->children_np;
		while (children_np != NULL) {
			take_regnode_lock(children_np->registry_p);
			if (!strcmp(objectinfo->tag_p, children_np->registry_p->tag_p)) {
				*objectinfo->object_p = children_np->registry_p->object_h;
				release_regnode_lock(children_np->registry_p);
				break;
			}
			release_regnode_lock(children_np->registry_p);
			children_np = children_np->node_next_np;
		}

		if (children_np == NULL) {
			REGISTRY_ERROR_MSG("<%s>:<%d> REGISTRY_OBJECT Not present\n", __FUNCTION__, __LINE__);
			err = -ENODEV;
		}
	}
		break;
	case REGISTRY_CONNECTION:
	{
		struct connection_node_s *connectnode_np;
		struct connection_info_s *connectinfo_p = (struct connection_info_s *) Info_p;

		connectnode_np = Registrynode->connection_np;
		while (connectnode_np != NULL) {
			if (!strcmp(connectinfo_p->tag_p, connectnode_np->connect_info.tag_p)) {
				connectinfo_p->connected_object =
					connectnode_np->connect_info.connected_object;
				break;
			}
			connectnode_np = connectnode_np->connection_next_np;
		}

		if (connectnode_np == NULL) {
			REGISTRY_DEBUG_MSG(" %s connection does not exist\n",
				      connectinfo_p->tag_p);
			err = -ENODEV;
		}
	}
		break;

	case REGISTRY_ATTRIBUTE:
	{
		struct attribute_node_s *attrnode_np;
		struct attribute_info_s *attrinfo_p;

		attrinfo_p = (struct attribute_info_s *) Info_p;
		attrnode_np = Registrynode->attribute_np;
		while (attrnode_np != NULL) {
			if (!strcmp(attrnode_np->attribute_info.tag_p, attrinfo_p->tag_p)) {

				if (attrinfo_p->buffer_p == NULL) { /* if the buffer_p is NULL then */ /*this call is made to get the attribute size*/
					goto copysize;
				}

				if (attrinfo_p->usize < attrnode_np->attribute_info.usize) { /* if the buffer size is smaller then */ /* return -ENOMEM along with the actual size */
					err = -ENOMEM;
					goto copysize;
				}

				if (attrinfo_p->usize > attrnode_np->attribute_info.usize) {
					memcpy(attrinfo_p->buffer_p,
						      attrnode_np->attribute_info.buffer_p,
						      attrnode_np->attribute_info.usize);
				} else {
					memcpy(attrinfo_p->buffer_p,
						      attrnode_np->attribute_info.buffer_p,
						      attrinfo_p->usize);
				}

				strlcpy(attrinfo_p->datatypetag_p,
					      attrnode_np->attribute_info.datatypetag_p,
					      STM_REGISTRY_MAX_TAG_SIZE);
copysize:
				attrinfo_p->usize = attrnode_np->attribute_info.usize;
				break;
			}
			attrnode_np = attrnode_np->attribute_next_np;
		}


		if (attrnode_np == NULL) {
			REGISTRY_DEBUG_MSG("<%s>:<%d> Attribute Not present\n", __FUNCTION__, __LINE__);
			err = -ENODEV;
		}
	}
		break;

	default:
		REGISTRY_ERROR_MSG(" <%s>:<%d> Invalid case\n ", __FUNCTION__, __LINE__);
		err = -EINVAL;
	}

	return err;
}

int registry_internal_remove_node_info(struct registrynode *Registrynode,
				       Info_type_flag InfoFlag, void *Info_p)
{
	int err = 0;

	REGISTRY_DEBUG_MSG("registrynode %d\n", (uint32_t) Registrynode);

	switch (InfoFlag) {
	case REGISTRY_TAG: /* Nothig to be done for TAG*/
		err = -EINVAL;
		break;
	case REGISTRY_PARENT: /* Nothing to be done for PARENT*/
		err = -EINVAL;
		break;
	case REGISTRY_OBJECT: /* Nothing to be done for OBJECT*/
		err = -EINVAL;
		break;
	case REGISTRY_CONNECTION:
	{
		struct connection_node_s *connection_fortrvs_np; /* for traversing the llnk list*/
		struct connection_node_s *connection_prev_np;

		connection_fortrvs_np = connection_prev_np = Registrynode->connection_np;
		while (connection_fortrvs_np != NULL) {
			if (!strcmp(connection_fortrvs_np->connect_info.tag_p, (char *) Info_p)) {
				if (connection_fortrvs_np == Registrynode->connection_np) /*  For the first node*/
					Registrynode->connection_np = connection_fortrvs_np->connection_next_np;
				else
					connection_prev_np->connection_next_np =
						connection_fortrvs_np->connection_next_np;

				registry_internal_del_connection_symblink(
					      Registrynode,
					      &connection_fortrvs_np->connect_info);

				infra_os_free((void *) connection_fortrvs_np);
				return 0; /*every thing OK*/
			}

			/*save the previous node*/
			connection_prev_np = connection_fortrvs_np;
			/*move to next node*/
			connection_fortrvs_np =
				connection_fortrvs_np->connection_next_np;
		}
		REGISTRY_ERROR_MSG("<%s>:<%d> This connection does not exist\n",
			      __FUNCTION__, __LINE__);
		err = -ENODEV;
	}
		break;
	case REGISTRY_ATTRIBUTE:
	{
		struct attribute_node_s *attrnode_fortrvs_np;
		struct attribute_node_s *attrnode_prev_np;

		attrnode_prev_np =
			attrnode_fortrvs_np =
			Registrynode->attribute_np;
		while (attrnode_fortrvs_np != NULL) {
			if (!strcmp(attrnode_fortrvs_np->attribute_info.tag_p, (char *) Info_p)) {
				if (attrnode_fortrvs_np == Registrynode->attribute_np) /*  For the first node*/
					Registrynode->attribute_np = attrnode_fortrvs_np->attribute_next_np;
				else
					attrnode_prev_np->attribute_next_np = attrnode_fortrvs_np->attribute_next_np;

				attrnode_fortrvs_np->attribute_info.buffer_p = NULL;
				infra_os_free(attrnode_fortrvs_np);
#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
				registry_sysfs_remove_attribute(Registrynode->kobj, (char *) Info_p); /* This function return void */
#endif
				return err; /*every thing OK*/
			}
			attrnode_prev_np = attrnode_fortrvs_np; /*save the previous node*/
			attrnode_fortrvs_np =
				attrnode_fortrvs_np->attribute_next_np; /*move to next node*/
		}
		REGISTRY_ERROR_MSG("<%s>:<%d> This Attribute does not exist\n",
			      __FUNCTION__, __LINE__);
		err = -ENODEV;
	}
		break;

	case REGISTRY_OBJECT_TYPE: /*Nothing to be done OBJECT TYPE*/
		err = -EINVAL;
		break;
	default:
		REGISTRY_ERROR_MSG("<%s>:<%d> invalid case\n", __FUNCTION__, __LINE__);
		err = -EINVAL;
	}

	return err;
}

int registry_internal_check_node_for_deletion(struct registrynode *Registry_p)
{
	int err = 0;

	REGISTRY_DEBUG_MSG("Registry node : 0x%x\n", (uint32_t) Registry_p);
	if (Registry_p->connection_np != NULL) {
		REGISTRY_ERROR_MSG("Connection(%s) present Cannot Delete\n",
			      Registry_p->connection_np->connect_info.tag_p);

		return -EBUSY;
	}

	if (Registry_p->connected_obj_count != 0) {
		REGISTRY_ERROR_MSG("Object:%p is still connnected to %d Objects\n",
			      Registry_p->object_h,
			      Registry_p->connected_obj_count);
		return -EBUSY;
	}

	if (Registry_p->children_np != NULL) {
		REGISTRY_ERROR_MSG("<%s>: <%d> Child(%s) present Cannot Delete\n",
			      __FUNCTION__, __LINE__, Registry_p->children_np->registry_p->tag_p);
		return -EBUSY;
	}

	return err;
}


int registry_internal_entry_report(stm_object_h object, bool do_removal, struct registrynode **Registry_p)
{

	struct registrynode	*registry_node_p = NULL;
	int error = 0;
	int cnt = 0;

	infra_os_sema_wait(g_registry_sem_lock);

	*Registry_p = registry_rbtree_search(&registry_tree, object);

	registry_node_p = *Registry_p;

	if (registry_node_p) {

		take_regnode_lock(registry_node_p);

		if (registry_node_p->deletion_started == false) {
			registry_node_p->usr_cnt++;
			cnt = 1;
		} else
			error = -ECANCELED;

		if (do_removal == true) {
			if (registry_internal_check_node_for_deletion(registry_node_p) == -EBUSY) {
				error = -EBUSY;
				/* decrease the count if it get incremented */
				registry_node_p->usr_cnt -= cnt;
			} else
				registry_node_p->deletion_started = true;
		}
		release_regnode_lock(registry_node_p);
	} else
		error = -ENODEV;

	infra_os_sema_signal(g_registry_sem_lock);

	return error;
}


int registry_internal_exit_report(struct registrynode *Registrynode)
{
	int error = 0;

	take_regnode_lock(Registrynode);

	Registrynode->usr_cnt -= (Registrynode->usr_cnt > 0) ? 1 : 0;
	if (Registrynode->deletion_started == true && Registrynode->usr_cnt == 0)
		infra_os_sema_signal(Registrynode->no_active_usr_sem_p);

	release_regnode_lock(Registrynode);

	return error;
}

int registry_internal_update_attribute(struct registrynode *Registrynode,
				    struct attribute_info_s *attribute_info)
{
	int err = 0;
	struct attribute_node_s *attrnode_fortrvs_np;
	struct attribute_node_s *attrnode_prev_np;

	attrnode_prev_np = Registrynode->attribute_np;
	attrnode_fortrvs_np = Registrynode->attribute_np;

	while (attrnode_fortrvs_np != NULL) {
		if (!strcmp(attrnode_fortrvs_np->attribute_info.tag_p, (char *) attribute_info->tag_p)) {

			attrnode_fortrvs_np->attribute_info.buffer_p = attribute_info->buffer_p;
			attrnode_fortrvs_np->attribute_info.usize = attribute_info->usize;

			strlcpy(attrnode_fortrvs_np->attribute_info.datatypetag_p,
					      attribute_info->datatypetag_p,
					      STM_REGISTRY_MAX_TAG_SIZE);

			return err;
		}
		attrnode_prev_np = attrnode_fortrvs_np; /*save the previous node*/
		attrnode_fortrvs_np =
			attrnode_fortrvs_np->attribute_next_np; /*move to next node*/
	}
	REGISTRY_ERROR_MSG("<%s>:<%d> This Attribute does not exist\n",
			    __FUNCTION__, __LINE__);
	err = -ENODEV;

	return err;
}

/*****************All API below this are lock free **************************/

int registry_internal_find_node(stm_object_h Object, struct registrynode **Registry_p)
{


	infra_os_sema_wait(g_registry_sem_lock);

	*Registry_p = registry_rbtree_search(&registry_tree, Object);
	if (*Registry_p == NULL) {
		REGISTRY_DEBUG_MSG("regnode not found :for Object : %x , reg_node is %x\n", (uint32_t) Object, (uint32_t) (*Registry_p));
		infra_os_sema_signal(g_registry_sem_lock);
		return -ENODEV;
	}

	infra_os_sema_signal(g_registry_sem_lock);
	REGISTRY_DEBUG_MSG("for Object : %x , reg_node is %x\n", (uint32_t) Object, (uint32_t) (*Registry_p));
	return 0;
}


int registry_internal_alloc_node(struct registrynode **Registry_p)
{
	REGISTRY_DEBUG_MSG("Regsitry node %x\n", (uint32_t) *Registry_p);

	if (Registry_p == NULL) {
		/*REGISTRY_ERROR_MSG(fmt, args...)*/
		return -EINVAL;
	}

	*Registry_p = (struct registrynode *) infra_os_malloc(sizeof(struct registrynode));

	if (*Registry_p == NULL) {
		/*REGISTRY_ERROR_MSG(fmt, args...)*/
		return -ENOMEM;
	}

	memset(*Registry_p, 0x00, sizeof(struct registrynode));

	return 0;
}

int registry_internal_dealloc_node(struct registrynode *Registry)
{
	infra_os_free(Registry);
	return 0;
}

void registry_intenal_string_create(char *Destination_string, char *Tag_string, stm_object_h Object_h)
{
	char temp_str[24];
	if (strlen(Tag_string) > 23) {
		strlcpy(temp_str, Tag_string, 23);
		temp_str[23] = '\0';
		sprintf(Destination_string, "%s%x", temp_str, (int) Object_h);
	} else
		sprintf(Destination_string, "%s%x", Tag_string, (int) Object_h);

}

int registry_internal_fill_connection_node(struct connection_node_s **Connection_np,
					   struct connection_info_s *ConnectionInfo_p)
{
	struct connection_node_s *connection_np;

	connection_np = (struct connection_node_s *) infra_os_malloc(sizeof(struct connection_node_s));
	if (!connection_np)
		return -ENOMEM;


	/* filling up the node with info */
	connection_np->connection_next_np = NULL;
	connection_np->connect_info.connected_object = ConnectionInfo_p->connected_object;
	strlcpy(connection_np->connect_info.tag_p,
		      ConnectionInfo_p->tag_p,
		      STM_REGISTRY_MAX_TAG_SIZE);

	*Connection_np = connection_np;
	return 0;
}

int registry_internal_create_connection_symblink(struct registrynode *Registrynode,
						 struct connection_info_s *connectioninfo_p)
{
	struct registrynode *registry_connected;
	int	error_code = 0;
#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	char temp_tag[REG_CONNECTION_SYMLINK_TAG_MAX_LEN];
#endif

	registry_connected = registry_rbtree_search(&registry_tree, connectioninfo_p->connected_object);
	if (registry_connected == NULL) {
		REGISTRY_ERROR_MSG("Object not found\n");
		goto error;
	}


#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	error_code = registry_sysfs_add_symblink(
		      Registrynode->kobj,
		      registry_connected->kobj,
		      connectioninfo_p->tag_p);
	if (error_code != 0)
		goto error;
#endif

	/*Increase the user count by 1*/
	registry_connected->connected_obj_count += 1;

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	memset(temp_tag, 0x00, 100);
	snprintf(temp_tag, REG_CONNECTION_SYMLINK_TAG_MAX_LEN,
		      "backlink-%s-from-%s-%x",
		      connectioninfo_p->tag_p,
		      Registrynode->tag_p,
		      (int) Registrynode->object_h);
	error_code = registry_sysfs_add_symblink(
		      registry_connected->kobj,
		      Registrynode->kobj,
		      temp_tag);
	if (error_code != 0)
		goto error;
#endif

error:
	return error_code;
}

int registry_internal_del_connection_symblink(struct registrynode *Registrynode,
					      struct connection_info_s *connectioninfo_p)
{
	struct registrynode *registry_connected = NULL;
#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	char temp_tag[REG_CONNECTION_SYMLINK_TAG_MAX_LEN];
#endif

	/* Get the connected object's registry node*/

	registry_connected = registry_rbtree_search(&registry_tree, connectioninfo_p->connected_object);
	if (registry_connected == NULL) {
		REGISTRY_ERROR_MSG("Object not found\n");
		return -ENODEV;
	}

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	registry_sysfs_remove_symblink(Registrynode->kobj, connectioninfo_p->tag_p);
#endif

	if (registry_connected) {
		/*decrease the user count by 1*/
		registry_connected->connected_obj_count -= 1;

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
		memset(temp_tag, 0x00, REG_CONNECTION_SYMLINK_TAG_MAX_LEN);
		snprintf(temp_tag, REG_CONNECTION_SYMLINK_TAG_MAX_LEN,
			      "backlink-%s-from-%s-%x",
			      connectioninfo_p->tag_p,
			      Registrynode->tag_p,
			      (int) Registrynode->object_h);

		registry_sysfs_remove_symblink(registry_connected->kobj, temp_tag);
#endif
	}

	return 0;
}

int registry_internal_fill_attribute_node(struct attribute_node_s **Attr_np,
					  struct attribute_info_s *Attrinfo_p,
					  struct registrynode *Registrynode)
{
	int err = 0;
	struct attribute_node_s *attr_np;

	REGISTRY_DEBUG_MSG("\n");
	attr_np = (struct attribute_node_s *) infra_os_malloc(sizeof(struct attribute_node_s));
	if (!attr_np) {
		REGISTRY_ERROR_MSG("");
		err = -ENOMEM;
		return err;
	}
	attr_np->attribute_info.buffer_p = Attrinfo_p->buffer_p;


	REGISTRY_DEBUG_MSG("attr_np->attribute_info.buffer_p : %d  , size : %d\n", *((int32_t *) attr_np->attribute_info.buffer_p), Attrinfo_p->usize);
	strlcpy(attr_np->attribute_info.tag_p,
		      Attrinfo_p->tag_p,
		      STM_REGISTRY_MAX_TAG_SIZE);

	strlcpy(attr_np->attribute_info.datatypetag_p,
		      Attrinfo_p->datatypetag_p,
		      STM_REGISTRY_MAX_TAG_SIZE);

	attr_np->attribute_info.usize = Attrinfo_p->usize;
	attr_np->attribute_next_np = NULL;
	attr_np->Registrynode = Registrynode;
	*Attr_np = attr_np;
	return err;
}


int registry_internal_clean_registry(void)
{

	struct rb_node *n, *prev;
	struct registrynode *registry_entry, *registry_node_p;
	int err = 0;

	REGISTRY_DEBUG_MSG("registry_internal_clean_registry\n");

	n = rb_first(&registry_tree);
	if (n == NULL)
		return err;

	n = rb_next(n);

	while (n) {

		registry_entry = rb_entry(n, struct registrynode, r_rbnode);

		while (registry_entry->connection_np) {
			err = registry_internal_remove_node_info(registry_entry, REGISTRY_CONNECTION,
				      registry_entry->connection_np->connect_info.tag_p);
			if (err != 0)
				return err;

		}

		prev = rb_next(n);

		err = registry_internal_entry_report(registry_entry->object_h, true, &registry_node_p);
		if (err != 0)
			return err;

		registry_internal_exit_report(registry_node_p);
		err = registry_internal_remove_object(registry_entry, registry_entry->object_h);
		if (err != 0)
			return err;

		n = prev;
	}

	return err;
}

int registry_internal_add_root(const char *tag,
			       stm_object_h object)
{
	int err = 0;

	struct registrynode *Registrynode = NULL;
	struct objecttype_info_s object_type;

	REGISTRY_DEBUG_MSG("");
	err = registry_internal_alloc_node(&Registrynode);
	if (err != 0) {
		/* REGSITER_ERROR_MSG(REGISTRY_INTERNAL); */
		return err;
	}

	err = registry_internal_add_object(Registrynode, object, STM_REGISTRY_ROOT);
	if (err != 0) {
		/* REGSITER_ERROR_MSG(REGSITRY_INTERNAL); */
		registry_internal_dealloc_node(Registrynode);
		return err;
	}

	registry_internal_add_node_info(Registrynode, REGISTRY_TAG, (void *) tag);
	registry_internal_add_node_info(Registrynode, REGISTRY_PARENT, STM_REGISTRY_ROOT);
	object_type.object_type_h = NULL;
	registry_internal_add_node_info(Registrynode, REGISTRY_OBJECT_TYPE, (void *) &object_type);

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	err = registry_sysfs_add_kobject(Registrynode->tag_p,
		      Registrynode->Parent_p->kobj,
		      &Registrynode->kobj);
#endif
	if (err != 0) {
		err = registry_internal_entry_report(object, true, &Registrynode);
		if (err == 0) {
			registry_internal_exit_report(Registrynode);
			registry_internal_remove_object(Registrynode, object);
		}
		err = -ENODEV;
	}
	return err;

}

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
void registry_internal_delete_link(struct registrynode *reg_node)
{
	struct registrynode *reg_data_type_p = NULL;
	char tag[REG_SYSLINK_MAX_TAG_SIZE];

	reg_data_type_p = registry_rbtree_search(&registry_tree, reg_node->object_type.object_type_h);
	if (reg_data_type_p == NULL) {
		REGISTRY_ERROR_MSG("Object not found\n");
		return;
	}

	if (reg_node->object_type.objecttypetag_p[0] != '\0') {
		registry_sysfs_remove_symblink(reg_node->kobj,
			      reg_node->object_type.objecttypetag_p);
		snprintf(tag, REG_SYSLINK_MAX_TAG_SIZE, "%s_%s", "inst", reg_node->tag_p);
		registry_sysfs_remove_symblink(reg_data_type_p->kobj, tag);
	}
}


int registry_internal_create_link(struct registrynode *Registrynode_objtype, struct registrynode *Registrynode)
{
	int err = 0;
	char tag[REG_SYSLINK_MAX_TAG_SIZE];
	err = registry_sysfs_add_symblink(Registrynode->kobj,
		      Registrynode_objtype->kobj, Registrynode_objtype->tag_p);
	if (err != 0)
		return -ENODEV;

	snprintf(tag, REG_SYSLINK_MAX_TAG_SIZE, "%s_%s", "inst", Registrynode->tag_p);
	err = registry_sysfs_add_symblink(Registrynode_objtype->kobj,
		      Registrynode->kobj, tag);
	if (err != 0) {
		registry_sysfs_remove_symblink(Registrynode->kobj, Registrynode_objtype->tag_p);
		return -ENODEV;
	}
	return 0;
}
#endif


#if (defined SDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES)
/* This function is provided to dump the whole registry for debug purpose */
void registry_internal_dump_registry(void)
{


	struct rb_node *n;
	struct registrynode *registry_p;
	struct registrynode_ctx *children_np;
	struct attribute_node_s *attribute_np;
	struct connection_node_s *connection_np;

	pr_info("--------------------- Registry dump starts -----------------------\n");

	n = rb_first(&registry_tree);
	while (n != NULL) {


		registry_p = rb_entry(n, struct registrynode, r_rbnode);

		pr_info("     %s-------- Data for the object %x <%s> starts --------%s\n", RED, (int) registry_p->object_h,
			      registry_p->tag_p, NONE);


		pr_info("     Object         ->0x%x\n", (int) registry_p->object_h);
		pr_info("     Object Tag     ->%s\n", registry_p->tag_p);
		pr_info("     Object Parent  ->0x%x\n", (int) registry_p->parent_h);
		pr_info("     Object Type    ->0x%x\n", (int) registry_p->object_type.object_type_h);
		pr_info("     Object Type Tag->%s\n", registry_p->object_type.objecttypetag_p);

		children_np = registry_p->children_np;
		pr_info("         %s[Children]:\n", CYAN);
		while (children_np) {
			pr_info("          [Child]:\n");
			pr_info("                  Object ->0x%x\n", (int) children_np->registry_p->object_h);
			pr_info("                  Tag    ->%s\n", children_np->registry_p->tag_p);
			pr_info("                  Parent ->0x%x\n", (int) children_np->registry_p->parent_h);

			children_np = children_np->node_next_np;
		}

		attribute_np = registry_p->attribute_np;
		pr_info("         %s[Attribute]:\n", BROWN);

		while (attribute_np) {
			pr_info("           Tag        ->%s\n", attribute_np->attribute_info.tag_p);
			pr_info("           Size       ->%d\n", attribute_np->attribute_info.usize);
			attribute_np = attribute_np->attribute_next_np;
		}

		connection_np = registry_p->connection_np;
		pr_info("         %s[Connection]:\n", BLUE);

		while (connection_np) {
			pr_info("          Connection Tag   ->%s\n", connection_np->connect_info.tag_p);
			pr_info("          Connected Object ->0x%x\n", (unsigned int) connection_np->connect_info.connected_object);
			connection_np = connection_np->connection_next_np;
		}

		pr_info("    %s -------- Data for the object %x <%s> ends -------- %s\n", RED, (int) registry_p->object_h,
			      registry_p->tag_p, NONE);
		n = rb_next(n);
	}

	pr_info("------------------------ Registry dump ends -------------------------\n");
}
#endif
