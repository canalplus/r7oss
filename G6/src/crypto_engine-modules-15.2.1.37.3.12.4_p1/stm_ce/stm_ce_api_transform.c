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

Source file name : stm_ce_api_transform.c

Defines API-level functions for stm_ce transforn objects
************************************************************************/

#include "stm_ce.h"
#include "stm_ce_objects.h"
#include "ce_hal.h"
#include "stm_ce_osal.h"
#include "stm_registry.h"
#include "helpers.h"

int stm_ce_transform_new(const stm_ce_handle_t session,
		const stm_ce_transform_type_t type,
		const uint32_t n_stages,
		stm_ce_handle_t *transform)
{
	struct ce_session *session_obj;
	struct ce_transform *transform_obj = NULL;
	stm_ce_transform_config_t *config = NULL;
	int err, error_registry;
	int i = 0;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == transform || type >= STM_CE_TRANSFORM_TYPE_LAST ||
			n_stages < 1) {
		return -EINVAL;
	}

	/* Get the parent session object (also check for validity) */
	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Get parent session write lock */
	OS_wlock(session_obj->lock);

	/* Allocate transform object */
	CE_INFO("Creating transform: (%p)\n", transform_obj);
	transform_obj = OS_calloc(sizeof(struct ce_transform));
	if (NULL == transform_obj)
		return -ENOMEM;

	/* Allocate n_stages config data */
	CE_INFO("Creating config: (%p)\n", config);
	config = OS_calloc(sizeof(stm_ce_transform_config_t) * n_stages);
	if (NULL == config) {
		OS_free(transform_obj);
		OS_wunlock(session_obj->lock);
		return -ENOMEM;
	}

	/* Initialize transform object */
	transform_obj->magic = STM_CE_TRANSFORM_MAGIC;
	transform_obj->self = transform_obj;
	transform_obj->parent = session_obj;
	transform_obj->api = session_obj->hal->transform_api;
	transform_obj->type = type;
	snprintf(transform_obj->tag, STM_REGISTRY_MAX_TAG_SIZE,
			"%s_%u",
			STM_CE_TRANSFORM_INSTANCE_NAME,
			OS_atomic_inc_return(
				stm_ce_global->transform_type.count));
	OS_lock_init(transform_obj->lock);

	/* Initialize n_stages of config data */
	for (i = 0; i < n_stages; i++) {
		config[i].key_rule_id = STM_CE_KEY_RULES_NONE;
		config[i].iv_rule_id = STM_CE_KEY_RULES_NONE;
	}

	transform_obj->stages = n_stages;
	transform_obj->config = config;
	transform_obj->delete = false;

	CE_INFO("Creating transform: %s (%p)\n", transform_obj->tag,
			transform_obj);

	/* Register the new transform object as a child of the parent
	 * session's session_list */
	err = stm_registry_add_instance(&session_obj->transform_list,
			&session_obj->hal->transform_type,
			transform_obj->tag,
			transform_obj);
	if (0 != err) {
		CE_ERROR("Failed to add transform %s (%p)to session "\
				"list (%d)\n", transform_obj->tag,
				transform_obj, err);
		goto cleanup_OS_objects;
	}

	/* Add registry debug handler */
	err = stm_registry_add_attribute(transform_obj,
			STM_CE_TRANSFORM_PRINT_NAME,
			STM_CE_TRANSFORM_PRINT_TYPE,
			&transform_obj, sizeof(transform_obj));
	if (0 != err) {
		CE_ERROR("Failed to add attribute to transform (%p) - (%d)\n",
			   transform_obj, err);
		goto cleanup_registry;
	}

	/* Initialize the transform HAL object */
	err = (*transform_obj->api->new)(transform_obj, type, n_stages);
	if (0 != err)
		goto cleanup_registry;

	/* If this is a memory transform, initialise the additional m2m
	 * parameters */
	if ((transform_obj->type == STM_CE_TRANSFORM_TYPE_MEMORY) ||
		(transform_obj->type == STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR)) {
		mutex_init(&transform_obj->mem.wlock);
		init_completion(&transform_obj->mem.dest_writeable);
		init_completion(&transform_obj->mem.dest_readable);
	}

	/* Unlock session object */
	OS_wunlock(session_obj->lock);

	/* Set returned transform object handle */
	*transform = (stm_ce_handle_t)transform_obj;

	CE_API_EXIT("%d\n", 0);
	return 0;

cleanup_registry:
	error_registry = stm_registry_remove_object(transform_obj);
	if (0 != error_registry)
		CE_ERROR("Failed to remove transform object error %d\n",
					error_registry);
cleanup_OS_objects:
	OS_free(config);
	OS_lock_del(transform_obj->lock);
	OS_free(transform_obj);
	OS_wunlock(session_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_new);

int stm_ce_transform_delete(const stm_ce_handle_t transform)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	CE_INFO("Deleting transform: %s (%p)\n", transform_obj->tag,
			transform_obj);

	/* Lock transform object */
	OS_wlock(transform_obj->lock);

	/* Set delete field - avoid deadlock with callback functions, since we
	 * know this transform has been locked for deletion */
	transform_obj->delete = true;

	/* Call HAL delete api */
	err = (*transform_obj->api->del)(transform_obj);

	/* Delete object from registry */
	err = stm_registry_remove_object(transform_obj);
	if (0 != err)
		CE_ERROR("Failed to remove the transform err %d\n", err);

	/* Poison transform object "self" field to mark as invalid */
	transform_obj->self = NULL;
	OS_wunlock(transform_obj->lock);
	OS_lock_del(transform_obj->lock);

	/* Free transform object and config */
	if (transform_obj->config)
		OS_free(transform_obj->config);
	OS_free(transform_obj);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_delete);

int stm_ce_transform_get_control(const stm_ce_handle_t transform,
		const stm_ce_transform_ctrl_t selector,
		uint32_t *value)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == value || selector >= STM_CE_TRANSFORM_CTRL_LAST)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Call internal get_control api */
	OS_rlock(transform_obj->lock);
	err = (*transform_obj->api->get_control)(transform_obj, selector,
			value);
	OS_runlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_get_control);

int stm_ce_transform_set_control(const stm_ce_handle_t transform,
		const stm_ce_transform_ctrl_t selector,
		const uint32_t value)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (selector >= STM_CE_TRANSFORM_CTRL_LAST)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Call internal set_control api */
	OS_wlock(transform_obj->lock);
	err = (*transform_obj->api->set_control)(transform_obj, selector,
			value);
	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_control);

/* TODO Ow! args are bi-directional for Direct Access. Discuss. */
int stm_ce_transform_set_compound_control(
		const stm_ce_handle_t transform,
		const stm_ce_transform_cmpd_ctrl_t selector,
		const void *value)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();

	err = ce_check_initialized();
	if (err)
		goto clean;

	if (NULL == value) {
		err = -EINVAL;
		goto clean;
	}

	/* Basic parameter checking */
	switch (selector) {
	case STM_CE_TRANSFORM_CMPD_CTRL_DIRECT_ACCESS:
		break;
	default:
		err = -EINVAL;
		goto clean;
	}

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Call internal set_control api */
	OS_wlock(transform_obj->lock);
	err = (*transform_obj->api->set_compound_control)(transform_obj,
			(const uint32_t) selector, (void *) value);
	OS_wunlock(transform_obj->lock);

clean:
	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_compound_control);

int stm_ce_transform_get_capabilities(const stm_ce_handle_t transform,
		stm_ce_transform_caps_t *caps)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == caps)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Call internal get_capabilities api */
	OS_rlock(transform_obj->lock);
	err = (*transform_obj->api->get_capabilities)(transform_obj, caps);
	OS_runlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_get_capabilities);

int stm_ce_transform_set_config(const stm_ce_handle_t transform,
		const uint32_t stage,
		const stm_ce_transform_config_t *config)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == config)
		return -EINVAL;
	if (STM_CE_DIRECTION_LAST <= config->direction ||
			0 > config->direction ||
			STM_CE_CIPHER_LAST <= config->cipher ||
			0 > config->cipher ||
			STM_CE_CHAINING_LAST <= config->chaining ||
			0 > config->chaining ||
			STM_CE_RESIDUE_LAST <= config->residue ||
			0 > config->residue) {
		CE_ERROR("Config parameters out of range\n");
		return -EINVAL;
	}

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	if (stage >= transform_obj->stages)
		return -EINVAL;

	/* config->packet_size = Must stay clear
				  +
				 quantity of data to encrypt or encrypt */
	/* Must stay clear = 0
		says the whole packet is to be encrypted or decrypted */
	if ((STM_CE_TRANSFORM_TYPE_MEMORY == transform_obj->type) ||
		(transform_obj->type == STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR)) {

		if ((0 == config->packet_size) && (0 != config->msc)) {
			/* If config->packet_size  = 0
			   then config->msc ought to be zero */
			return -EINVAL;
		}
		if ((0 < config->packet_size) &&
		    (config->packet_size <= config->msc)) {
			/* If config->packet_size is greater than 0
			   then config->msc less than packet size */
			/* In this case config->msc can be even zero at same
			   time it can't be less or equal to packet size */
			return -EINVAL;
		}
	}

	/* Key rule parameter checks
	 * 1. Must have a valid key rule
	 * 2. Ciphers with a chaining mode must specify an IV rule */
	if (STM_CE_KEY_RULES_NONE == config->key_rule_id ||
			(STM_CE_KEY_RULES_NONE == config->iv_rule_id &&
			STM_CE_CHAINING_NONE != config->chaining))
		return -EINVAL;

	/* Copy the new config into the transform object */
	memcpy(&transform_obj->config[stage], config,
			sizeof(stm_ce_transform_config_t));

	/* Call the internal set_config function */
	OS_wlock(transform_obj->lock);
	err = (*transform_obj->api->set_config)(transform_obj, stage, config);
	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_config);

int stm_ce_transform_set_key(const stm_ce_handle_t transform,
		const uint32_t stage,
		const stm_ce_buffer_t *key,
		const int32_t key_number)
{
	struct ce_transform *transform_obj;
	int err;

	if (NULL == key) {
		CE_API_ENTRY("transform=%p, stage=%d, key=%p, key->size=%d,"\
				" key->data=%p, key_number=%d)\n",
			transform, stage, key, 0, NULL, key_number);
	} else {
		CE_API_ENTRY("transform=%p, stage=%d, key=%p, key->size=%d,"\
				" key->data=%p, key_number=%d)\n",
			transform, stage, key, key->size,
			key->data, key_number);
	}

	err = ce_check_initialized();
	if (err)
		return err;

	if (key && 0 == key->size)
		return -EINVAL;

	if (key && NULL == key->data)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* We need a write lock on the transform object and readlock on the
	 * parent session, since we use its ladders
	 * Take transform wlock first to avoid potential deadlock with
	 * stm_ce_transform_del */
	OS_wlock(transform_obj->lock);
	OS_rlock(transform_obj->parent->lock);

	/* Is this a valid stage for this transform? */
	if (stage >= transform_obj->stages)
		err = -EINVAL;

	/* Check the validity of setting the required key on this stage */
	if (!err && STM_CE_CIPHER_NONE == transform_obj->config[stage].cipher) {
		CE_ERROR("Transform %p, stage %d not configured\n", transform,
				stage);
		err = -EPERM;
	}

	/* Call the internal set_key function */
	if (!err)
		err = (*transform_obj->api->set_key)(transform_obj, stage, key,
				key_number);

	/* Release locks */
	OS_runlock(transform_obj->parent->lock);
	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_key);

int stm_ce_transform_set_key_Ex(const stm_ce_handle_t transform,
		const uint32_t stage,
		const stm_ce_buffer_t *key,
		const int32_t key_number,
		const stm_ce_key_operation_t keyop)
{
	struct ce_transform *transform_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	if (key && 0 == key->size)
		return -EINVAL;

	if (key && NULL == key->data)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* We need a write lock on the transform object and readlock on the
	 * parent session, since we use its ladders
	 * Take transform wlock first to avoid potential deadlock with
	 * stm_ce_transform_del */
	OS_wlock(transform_obj->lock);
	OS_rlock(transform_obj->parent->lock);

	/* Is this a valid stage for this transform? */
	if (stage >= transform_obj->stages)
		err = -EINVAL;

	/* Check the validity of setting the required key on this stage */
	if (!err && STM_CE_CIPHER_NONE == transform_obj->config[stage].cipher) {
		CE_ERROR("Transform %p, stage %d not configured\n", transform,
				stage);
		err = -EPERM;
	}

	/* Call the internal set_key function */
	if (!err) {
		err = (*transform_obj->api->set_key_Ex)(transform_obj,
							stage, key,
							key_number, keyop);
	}

	/* Release locks */
	OS_runlock(transform_obj->parent->lock);
	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_key_Ex);

int stm_ce_transform_set_iv(const stm_ce_handle_t transform,
		const uint32_t stage,
		const stm_ce_buffer_t *iv,
		const int32_t key_number)
{
	struct ce_transform *transform_obj;
	int err;

	if (NULL == iv) {
		CE_API_ENTRY("transform=%p, stage=%d, iv=%p, iv->size=%d, "\
			"iv->data=%p, key_number=%d)\n",
			transform, stage, iv, 0, NULL, key_number);
	} else {
		CE_API_ENTRY("transform=%p, stage=%d, iv=%p, iv->size=%d, "\
			"iv->data=%p, key_number=%d)\n",
			transform, stage, iv, iv->size, iv->data, key_number);
	}

	err = ce_check_initialized();
	if (err)
		return err;

/*	CE_INFO("Creating transform: %s (%p)\n", transform_obj->tag,*/

	if (iv && 0 == iv->size)
		return -EINVAL;

	if (iv && NULL == iv->data)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Is this a valid stage for this transform? */
	if (stage >= transform_obj->stages)
		return -EINVAL;

	/* We need a write lock on the transform object and readlock on the
	 * parent session, since we use its ladders
	 * Take transform wlock first to avoid deadlock with
	 * stm_ce_transform_del */
	OS_wlock(transform_obj->lock);
	OS_rlock(transform_obj->parent->lock);

	/* Call the internal set_iv api */
	err = (*transform_obj->api->set_iv)(transform_obj, stage, iv,
			key_number);

	/* Release locks */
	OS_runlock(transform_obj->parent->lock);
	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_iv);

int stm_ce_transform_attach(const stm_ce_handle_t transform,
		const stm_object_h target)
{
	int err;
	struct ce_transform *transform_obj;

	CE_API_ENTRY("transform=%p, target=%p\n", transform, target);
	err = ce_check_initialized();
	if (err)
		return err;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Check this is a memory transform */
	if ((STM_CE_TRANSFORM_TYPE_MEMORY != transform_obj->type) &&
		(transform_obj->type != STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR))
		return -EINVAL;

	OS_wlock(transform_obj->lock);

	/* Is this transform already attached to a sink? */
	if (transform_obj->mem.sink) {
		err = -EBUSY;
	} else {
		/* Attempt attach to pull sink or push sink */
		err = ce_transform_attach_pull_sink(transform_obj, target);
		if (err) {
			err = ce_transform_attach_push_sink(transform_obj,
					target);
		}
	}

	OS_wunlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_attach);

int stm_ce_transform_detach(const stm_ce_handle_t transform,
		const stm_object_h target)
{
	int err;
	struct ce_transform *transform_obj;

	CE_API_ENTRY("transform=%p, target=%p\n", transform, target);
	err = ce_check_initialized();
	if (err)
		return err;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Check this is a memory transform */
	if ((STM_CE_TRANSFORM_TYPE_MEMORY != transform_obj->type) &&
		(transform_obj->type != STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR))
		return -EINVAL;

	if (transform_obj->mem.sink != target) {
		CE_ERROR("Transform %p not attached to sink %p\n",
				transform_obj, target);
		return -ENODEV;
	}

	/* If there is a buffer, connected sink must be a push sink, otherwise
	 * it is a pull sink */
	if (transform_obj->mem.push_buffer)
		err = ce_transform_detach_push_sink(transform_obj, target);
	else
		err = ce_transform_detach_pull_sink(transform_obj, target);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_detach);

int stm_ce_transform_dma(const stm_ce_handle_t transform,
		const stm_ce_buffer_t *src, const stm_ce_buffer_t *dst)
{
	int err;

	CE_API_ENTRY("transform=%p, src=%p (sz=%d), dst=%p (sz=%d)\n",
			transform, src->data, src->size, dst->data, dst->size);

	/* Basic parameter checking */
	if (!src || !dst || !src->data || !dst->data || !src->size ||
			!dst->size)
		return -EINVAL;

	if (src->size != dst->size) {
		CE_ERROR("Source and destination sizes must be equal\n");
		return -EINVAL;
	}

	err = stm_ce_transform_dma_sg(transform, src, 1, dst, 1);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_dma);

int stm_ce_transform_dma_sg(const stm_ce_handle_t transform,
			const stm_ce_buffer_t *src, const uint32_t n_src,
			const stm_ce_buffer_t *dst, const uint32_t n_dst)
{
	int err;
	struct ce_transform *transform_obj;

	CE_API_ENTRY("transform=%p, src=%p n_src=%d), dst=%p n_dst=%d)\n",
			transform, src, n_src, dst, n_dst);

	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (!src || !dst || n_src == 0 || n_dst == 0)
		return -EINVAL;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Check this is a memory transform */
	if ((STM_CE_TRANSFORM_TYPE_MEMORY != transform_obj->type) &&
		(transform_obj->type != STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR))
		return -EINVAL;

	/* Call the internal dma api */
	OS_rlock(transform_obj->lock);
	err = (*transform_obj->api->dma)(transform_obj, src, n_src, dst, n_dst);
	OS_runlock(transform_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_dma_sg);

int stm_ce_transform_get_digest(const stm_ce_handle_t transform,
		const stm_ce_buffer_t *buf)
{
	int err;
	struct ce_transform *transform_obj;

	CE_API_ENTRY("transform=%p, digest=%p (sz=%d)\n", transform,
			buf->data, buf->size);
	err = ce_check_initialized();
	if (err)
		return err;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Function not yet implemented */
	err = -ENOSYS;

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_get_digest);

int stm_ce_transform_set_hash_config(const stm_ce_handle_t transform,
		const stm_ce_transform_hash_t *config)
{
	int err;
	struct ce_transform *transform_obj;

	CE_API_ENTRY("transform=%p\n", transform);
	err = ce_check_initialized();
	if (err)
		return err;

	/* Get transform object (checks validity of handle) */
	if (0 != ce_transform_from_handle(transform, &transform_obj))
		return -EINVAL;

	/* Function not yet implemented */
	err = -ENOSYS;

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_transform_set_hash_config);

/*
 * Internal API-level transform functions
 */

/**
 * stm_ce_transform_from_handle() - Retrieves a transform object from a handle
 */
int ce_transform_from_handle(stm_ce_handle_t transform,
		struct ce_transform **object)
{
	struct ce_hal *hal;
	stm_object_h type;
	int err;

	*object = (struct ce_transform *)transform;

	/* Check if object appears to be valid */
	if (NULL == *object) {
		CE_ERROR("Received invalid transform handle %p\n", transform);
		return -EINVAL;
	}

	/* Check if object appears to be valid (check type) */
	type = NULL;
	err = stm_registry_get_object_type(*object, &type);
	if (err) {
		CE_ERROR("Received invalid transform handle %p: %d\n",
			 transform, err);
		return -EINVAL;
	}
	if (type == NULL) {
		CE_ERROR("Received invalid transform handle %p\n",
			 transform);
		return -EINVAL;
	}

	/* Check if object appears to be valid (match type) */
	hal = NULL;
	err = ce_hal_get_initialized(0, &hal);
	if (err) {
		CE_ERROR("Cannot retrieve hal: %d\n", err);
		return -EINVAL;
	}
	if (type != &hal->transform_type) {
		CE_ERROR("Received invalid transform handle %p\n", transform);
		return -EINVAL;
	}

	/* Check if object appears to be valid (check self field) */
	if ((*object)->self != *object ||
			STM_CE_TRANSFORM_MAGIC != (*object)->magic) {
		CE_ERROR("Received invalid transform handle: %p\n", transform);
		return -EINVAL;
	}
	return 0;
}

/**
 * ce_transform_get_session - retrieves the parent session for a transform
 * object
 */
int ce_transform_get_session(struct ce_transform *transform,
		struct ce_session **session)
{
	*session = transform->parent;
	return 0;
}


/*
 * API-level transform print-handler
 */
int ce_transform_print(stm_object_h object, char *buf, size_t size,
		char *user_buf, size_t user_size)
{
	int n = 0;
	uint8_t i;
	struct ce_transform *t = *(struct ce_transform **)buf;

	n += snprintf(user_buf + n, user_size - n,
			"Name: %s\nAddress: %p\n",
			t->tag, t);
	n += snprintf(user_buf + n, user_size - n, "Type: %u\n", t->type);

	/* Print config for each stage */
	n += snprintf(user_buf + n, user_size - n , "Stages: %d\n", t->stages);

	for (i = 0; i < t->stages; i++) {
		stm_ce_transform_config_t *c = &t->config[i];
		n += snprintf(user_buf + n, user_size - n ,
				"\tStage %d: key_rule=0x%x, iv_rule=0x%x, "
				"dir=%d\n"
				"\t\tcipher=%d, chaining=%d, residue=%d, "
				"msc=%d\n",
				i, c->key_rule_id, c->iv_rule_id,
				c->direction, c->cipher, c->chaining,
				c->residue, c->msc);
	}

	/* Print HAL-specific data */
	n += (*t->api->print)(t, user_buf + n, user_size - n);

	return n;
}

