/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ce_path_transform.c

Defines tango HAL functions for transform objects
************************************************************************/
#include "ce_path.h"
#include "ce_path_mme.h"
#include "stm_ce.h"
#include "stm_ce_osal.h"
#include "stm_registry.h"
#include "stm_te_if_ce.h"

/*
 * Local helper functions
 */

/**
 * Creates and initializes a new object to represent the connection of a te
 * PID filter to a transform object
 */
static int ce_te_conn_new(struct ce_transform *t, stm_object_h pf)
{
	struct ce_te_conn *c;
	struct list_head *lptr;
	int err;
	stm_object_h te_type;
	char te_tag[STM_REGISTRY_MAX_TAG_SIZE];
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t if_size;

	CE_ENTRY("transform=%p, pf=%p\n", t, pf);

	/* Check if connection object already exists */
	list_for_each(lptr, &t->data->te_conn_list) {
		c = list_entry(lptr, struct ce_te_conn, lh);
		if (pf == c->te_object) {
			CE_ERROR("TE object %p is already connected to "\
					"transform %p\n", pf, t);
			return -EALREADY;
		}
	}

	/* Allocate and initialize the new connection object */
	c = OS_calloc(sizeof(struct ce_te_conn));
	if (NULL == c)
		return -ENOMEM;
	c->te_object = pf;

	/* Get Pid filter tag and interface */
	err = stm_registry_get_object_tag(pf, te_tag);
	if (0 != err) {
		CE_ERROR("Failed to get tag for TE object %p (%d)\n", pf,
				err);
		goto clean;
	}
	err = stm_registry_get_object_type(pf, &te_type);
	if (0 != err) {
		CE_ERROR("Failed to get type object for TE object %p (%d)\n",
				pf, err);
		goto clean;
	}
	err = stm_registry_get_attribute(te_type, STM_PID_FILTER_INTERFACE,
			type_tag, sizeof(c->te_if), &c->te_if, &if_size);
	if (0 != err) {
		CE_ERROR("Failed to get PID filter interface for TE object "\
				"%p (%d)\n", pf, err);
	}

	/* Add registry connection to represent connection */
	snprintf(c->tag, STM_REGISTRY_MAX_TAG_SIZE, "conn.%s", te_tag);
	err = stm_registry_add_connection(t, c->tag, pf);
	if (0 != err) {
		CE_ERROR("Failed to add connection %p -> %p\n", t, pf);
		goto clean;
	}

	/* Add connection object to transform's connection list */
	list_add_tail(&c->lh, &t->data->te_conn_list);

	CE_EXIT("0\n");
	return 0;
clean:
	OS_free(c);
	CE_EXIT("%d\n", err);
	return err;
}

/**
 * Deletes a connection between a TE Pid filter and CE transform
 */
static int ce_te_conn_del(struct ce_transform *t, stm_object_h pf)
{
	struct ce_te_conn *c = NULL;
	struct ce_te_conn *ctmp;
	struct list_head *lptr;
	int err;

	CE_ENTRY("transform=%p, pf=%p\n", t, pf);

	/* Get the connection object */
	list_for_each(lptr, &t->data->te_conn_list) {
		ctmp = list_entry(lptr, struct ce_te_conn, lh);
		if (pf == ctmp->te_object) {
			c = ctmp;
			break;
		}
	}
	if (NULL == c) {
		CE_ERROR("No connection found %p -> %p\n", pf, t);
		return -EINVAL;
	}


	/* Remove from transform's connection list */
	list_del(&c->lh);

	/* Delete registry connection (continue on error) */
	err = stm_registry_remove_connection(t, c->tag);
	if (0 != err) {
		CE_ERROR("Remove connection %s from object %p (%d)\n", c->tag,
				t, err);
	}

	/* Free connection object */
	OS_free(c);

	CE_EXIT("0\n");
	return 0;
}

/**
 * ce_path_transform_disconnect_all - Cleanly disconnects all connections to a
 * transform
 * @transform:	Transform object to disconnect
 */
static int ce_path_transform_disconnect_all(struct ce_transform *t)
{
	struct list_head *lptr, *lptr2;
	struct ce_te_conn *conn;
	int err;

	if (!t || !t->data)
		return -EINVAL;

	CE_ENTRY("transform=%p\n", t);

	/* Iterate through all connections for this transform, for each
	 * connection, call the TE PID filter's disconnect function to cleanly
	 * perform the connection from the TE side */
	list_for_each_safe(lptr, lptr2, &t->data->te_conn_list) {
		conn = list_entry(lptr, struct ce_te_conn, lh);

		if (conn->te_if.disconnect) {
			/* Ignore connection deletion errors here */
			err = (*conn->te_if.disconnect)(conn->te_object, t);
			if (0 != err) {
				CE_ERROR("TE disconnect call failed. TE pf=%p,"\
						" transform=%p\n",
						conn->te_object, t);
			}
		}
	}

	CE_EXIT("0\n");
	return 0;
}

/**
 * ce_path_transform_new - Initializes internal data for a transform object
 * @transform:	Pointer to the new transfom object to set-up
 */
static int ce_path_transform_new(struct ce_transform *transform,
		stm_ce_transform_type_t type, const uint32_t n_stages)
{
	int err = 0;

	CE_ENTRY("transform=%p\n", transform);

	/* Allocate HAL-specific data struct */
	transform->data = OS_calloc(sizeof(struct ce_transform_data));
	if (NULL == transform->data)
		return -ENOMEM;

	/* Initialise HAL-specific data for transform */
	INIT_LIST_HEAD(&transform->data->te_conn_list);
	transform->data->path_id = CE_PATH_ID_NONE;

	/* Send MME command */
	err = ce_path_mme_transform_new(transform, type, n_stages,
			&transform->data->path_id);

	/* Don't deallocate ->data on error. Deallocated later by clean: del fn.
	 */

	if (!err) {
		CE_INFO("New transform %p, path_id=0x%x\n", transform,
				transform->data->path_id);
	}

	CE_EXIT("%d\n", err);
	return err;
}

/**
 * ce_path_transform_del - Deletes internal data of a transform object
 * @transform:	Pointer to the transform object to unsetup
 */
static int ce_path_transform_del(struct ce_transform *transform)
{
	int err = 0;

	CE_ENTRY("transform %p\n", transform);

	/* Disconnect all connections */
	ce_path_transform_disconnect_all(transform);

	/* Send MME command to slave */
	err = ce_path_mme_transform_del(transform);

	/* Free model-specific data struct */
	if (transform->data)
		OS_free(transform->data);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_transform_get_control(struct ce_transform *transform,
		const stm_ce_transform_ctrl_t selector,
		uint32_t *value)
{
	CE_ENTRY();
	CE_EXIT();
	return 0;
}

static int ce_path_transform_set_control(struct ce_transform *transform,
		const stm_ce_transform_ctrl_t selector,
		const uint32_t value)
{
	CE_ENTRY();
	CE_EXIT();
	return 0;
}

static int ce_path_transform_set_compound_control(
		struct ce_transform *transform,
		const stm_ce_transform_cmpd_ctrl_t selector,
		void *args)
{
	int err;

	CE_ENTRY();

	switch (selector) {
	case STM_CE_TRANSFORM_CMPD_CTRL_DIRECT_ACCESS:
		err = ce_path_mme_direct_access(transform->parent, transform,
				args);
		break;
	default:
		err = -EINVAL;
	};

	CE_EXIT();
	return err;
}

static int ce_path_transform_get_caps(struct ce_transform *transform,
		stm_ce_transform_caps_t *caps)
{
	CE_ENTRY();
	CE_EXIT();
	return 0;
}

static int ce_path_transform_set_config(struct ce_transform *transform,
		const uint32_t stage, const stm_ce_transform_config_t *config)
{
	int err;

	CE_ENTRY();

	/* Call HAL layer to apply this config */
	err = ce_path_mme_transform_set_config(transform, stage, config);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_transform_set_key(struct ce_transform *transform,
		uint32_t stage, const stm_ce_buffer_t *key,
		const int32_t key_num)
{
	int err;

	CE_ENTRY();

	/* Call the HAL layer to set this key */
	err = ce_path_mme_transform_set_key(transform, stage, key, key_num, 1);

	CE_EXIT("%d\n", err);
	return err;
}
static int ce_path_transform_set_key_Ex(struct ce_transform *transform,
		uint32_t stage, const stm_ce_buffer_t *key,
		const int32_t key_num,
		const stm_ce_key_operation_t keyop)
{
	int err;

	CE_ENTRY();

	/* Call the HAL layer to set this key */
	err = ce_path_mme_transform_set_key_Ex(transform, stage, key,
						key_num, 1, keyop);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_transform_set_iv(struct ce_transform *transform,
		uint32_t stage, const stm_ce_buffer_t *iv,
		const int32_t key_num)
{
	int err;

	CE_ENTRY();

	/* Call the HAL layer to set this IV */
	err = ce_path_mme_transform_set_key(transform, stage, iv, key_num, 0);

	CE_EXIT("%d\n", err);
	return err;
}

/**
 * ce_path_transform_connect - Connects a transform object to a filter object
 * @transform:	Pointer to the transform object to connect
 * @target:	Object handle (reference) to the filter to connect to
 */
static int ce_path_transform_connect(stm_object_h transform,
		stm_object_h target, uint32_t *path_id)
{
	struct ce_transform *t;
	int err;

	CE_ENTRY("transform=%p, target=%p\n", transform, target);

	/* Get and lock the transform object */
	if (0 != ce_transform_from_handle(transform, &t))
		return -EINVAL;
	OS_wlock(t->lock);

	/* Create the new connection (error if connection already exists) */
	err = ce_te_conn_new(t, target);
	if (0 != err)
		goto clean;

	/* Set the path_id */
	*path_id = t->data->path_id;

	OS_wunlock(t->lock);
	CE_EXIT("0\n");
	return 0;
clean:
	OS_wunlock(t->lock);
	CE_EXIT("%d\n", err);
	return err;
}

/**
 * ce_path_transform_disconnect - Disconnects a transform object from a filter
 * object
 * @transform:	Pointer to the transform object to disconnect
 * @target:	Object handle of the filter to disconnect from
 */
static int ce_path_transform_disconnect(stm_object_h transform,
		stm_object_h target)
{
	struct ce_transform *t;
	int err;

	CE_ENTRY("transform=%p, target=%p\n", transform, target);

	/* Get and lock transform object */
	if (0 != ce_transform_from_handle(transform, &t))
		return -EINVAL;

	/* If transform is being deleted, do not attempt to get the
	 * transform's write lock again (will deadlock). This callback is a
	 * result of the  deletion */
	if (!t->delete)
		OS_wlock(t->lock);

	/* Delete the connection */
	err = ce_te_conn_del(transform, target);

	if (!t->delete)
		OS_wunlock(t->lock);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_transform_dma(struct ce_transform *transform,
		const stm_ce_buffer_t *src, const uint32_t n_src,
		const stm_ce_buffer_t *dst, const uint32_t n_dst)
{
	int err;
	CE_ENTRY("transform=%p\n", transform);

	err = ce_path_mme_transform_dma(transform, src, n_src, dst, n_dst);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_transform_print(struct ce_transform *transform,
		char *print_buffer, uint32_t buffer_size)
{
	int n = 0, err = 0;
	struct list_head *lptr;
	struct ce_te_conn *conn;
	char te_tag[STM_REGISTRY_MAX_TAG_SIZE];
	n += snprintf(print_buffer + n, buffer_size - n, "Path ID: 0x%x\n",
			transform->data->path_id);


	/* List PID-filter connections for stream transforms */
	if (STM_CE_TRANSFORM_TYPE_STREAM == transform->type) {
		n += snprintf(print_buffer + n, buffer_size - n,
				"PID filter connections:\n");
		list_for_each(lptr, &transform->data->te_conn_list) {
			conn = list_entry(lptr, struct ce_te_conn, lh);
			err = stm_registry_get_object_tag(conn->te_object,
				te_tag);
			if (0 != err) {
				CE_ERROR("Failed to get tag for TE "\
						"object %p (%d)\n",
						conn->te_object, n);
				return err;
			}
			n += snprintf(print_buffer + n, buffer_size - n,
					"\t%s (%p)\n", te_tag, conn->te_object);
		}
	}
	return n;
}

/*
 * Global exported variables
 */
stm_ce_path_interface_t ce_path_interface = {
	.connect = &ce_path_transform_connect,
	.disconnect = &ce_path_transform_disconnect,
};

/* Exported transform API */
struct ce_transform_api ce_path_transform_api = {
		.new = &ce_path_transform_new,
		.del = &ce_path_transform_del,
		.get_control = &ce_path_transform_get_control,
		.set_control = &ce_path_transform_set_control,
		.set_compound_control = &ce_path_transform_set_compound_control,
		.get_capabilities = &ce_path_transform_get_caps,
		.set_config = &ce_path_transform_set_config,
		.set_key = &ce_path_transform_set_key,
		.set_key_Ex = &ce_path_transform_set_key_Ex,
		.set_iv = &ce_path_transform_set_iv,
		.dma = &ce_path_transform_dma,
		.print = &ce_path_transform_print,
};
