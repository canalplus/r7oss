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

Source file name : stm_ce_api_session.c

Defines API-level functions for stm_ce session objects
************************************************************************/

#include "stm_ce.h"
#include "stm_ce_objects.h"
#include "stm_ce_key_rules.h"
#include "ce_hal.h"
#include "stm_ce_osal.h"
#include "stm_registry.h"
#include "helpers.h"

/* defines default stm_ce_sp */
#include <stm_ce_sp/scheme.h>

/* Linux firmware loading interface used for key rule loading */
#include <linux/firmware.h>
/* Linux PM Runtime API */
#include <linux/pm_runtime.h>

/*
 * Local function prototypes
 */
static int registry_session_add(struct ce_session *s);
static int registry_session_del(struct ce_session *s);

/* Dummy device release function */
static void ce_dummy_release(struct device *dev) { }

/**
 * Adds stm_ce_session object to registry
 *
 * Adds instance of ce_session, with transform_list child object and debug
 * print handler
 *
 * On failure, cleans-up any registry objects added.
 *
 * Returns 0 on success, negative errno on failure.
 */
static int registry_session_add(struct ce_session *s)
{
	int err;

	err = stm_registry_add_instance(&stm_ce_global->session_list,
			&s->hal->session_type, s->tag, s);
	if (0 != err)
		goto clean;

	err = stm_registry_add_object(s, STM_CE_TRANSFORM_INSTANCE_NAME,
			&s->transform_list);
	if (0 != err)
		goto clean;

	err = stm_registry_add_attribute(s, STM_CE_SESSION_PRINT_NAME,
			STM_CE_SESSION_PRINT_TYPE,
			&s, sizeof(s));

	return 0;
clean:
	registry_session_del(s);
	return err;
}

/**
 * Removes stm_ce_session object from the registry
 *
 * Removes instance and all attributes added by registry_session_del
 */
static int registry_session_del(struct ce_session *s)
{
	int err;

	err = stm_registry_remove_attribute(s, STM_CE_SESSION_PRINT_NAME);
	if (0 != err)
		CE_ERROR("Failed to remove attribute %s from session %p "\
				"(%d)\n", STM_CE_SESSION_PRINT_NAME, s, err);

	err = stm_registry_remove_object(&s->transform_list);
	if (0 != err)
		CE_ERROR("Failed to remove transform list from session %p "\
				"(%d)\n", s, err);

	err = stm_registry_remove_object(s);
	if (0 != err)
		CE_ERROR("Failed to remove session %p (%d)\n", s, err);

	return err;
}

/*
 * Load session rules
 * Get the session rules and copy it inside the stm_ce_global rules
 */
#define RULES_SUFFIX	".kr"
static int load_rules(const char *scheme, struct ce_session *session_obj)
{
	int err = 0;
	const struct firmware *fw = NULL;
	char rules_fw_name[RULES_NAME_MAX_LEN+4] = {0};

	/* Concatenate the session name with the suffix */
	strlcpy(rules_fw_name, scheme, sizeof(rules_fw_name));
	strlcat(rules_fw_name, RULES_SUFFIX, sizeof(rules_fw_name));

	/* Load session profile data using Linux firmware loader */
	err = request_firmware(&fw, rules_fw_name, &session_obj->pdev.dev);
	if (0 != err) {
		CE_ERROR("Failed to load session profile %s (%d)\n",
				rules_fw_name,
				err);
		err = -EINVAL;
		goto out;
	}

	CE_INFO("Loaded session profile %s (%d bytes)\n", scheme, fw->size);
	if (fw->size != sizeof(stm_ce_rule_set_t)) {
		CE_ERROR("Invalid profile (size=%d, expected %d)\n",
				fw->size, sizeof(stm_ce_rule_set_t));
		err = -EINVAL;
	} else {
		memcpy(&stm_ce_global->session_rules.rules, fw->data,
				sizeof(stm_ce_rule_set_t));
		strncpy(stm_ce_global->session_rules.rules_name,
			scheme, RULES_NAME_MAX_LEN);
	}

	release_firmware(fw);

out:
	return err;
}

/*
 * stm_ce session API functions
 */
int stm_ce_session_new(const char *scheme, stm_ce_handle_t *session)
{
	int err = 0;
	struct ce_session *session_obj = NULL;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == session)
		return -EINVAL;

	/* Create the new session object */
	session_obj = OS_calloc(sizeof(struct ce_session));
	if (NULL == session_obj)
		return -ENOMEM;

	/* Get global stm_ce lock, while creating session */
	OS_wlock(stm_ce_global->lock);

	/* If no scheme name is passed, use the default scheme name */
	if (NULL == scheme) {
		CE_INFO("### No session profile set, "\
				"setting to default for current SP\n");
		scheme = get_default_session_name(stm_ce_global->pdev);

	}
	CE_INFO("### session profile now %s\n", scheme);

#ifdef CONFIG_PM_RUNTIME
	/* Ensure the device is woken up */
	pm_runtime_get_sync(&stm_ce_global->pdev->dev);
#endif

	/* Get/initialize HAL for this session */
	err = ce_hal_get_initialized(session_obj->rules.hal,
			&session_obj->hal);
	if (err != 0)
		goto failed_hal_init;

	/* Initialize new session object (increment session count) */
	session_obj->magic = STM_CE_SESSION_MAGIC;
	session_obj->self = session_obj;
	session_obj->api = session_obj->hal->session_api;
	snprintf(session_obj->tag, STM_REGISTRY_MAX_TAG_SIZE, "%s_%u-%s",
			STM_CE_SESSION_INSTANCE_NAME,
			OS_atomic_inc_return(stm_ce_global->session_type.count),
			scheme);
	OS_lock_init(session_obj->lock);

	/* Register session platform device */
	snprintf(session_obj->pdev_name, STM_REGISTRY_MAX_TAG_SIZE,
			STM_CE_SESSION_PDEV_NAME ".%u",
			OS_atomic_read(stm_ce_global->session_type.count));
	session_obj->pdev.name = session_obj->pdev_name;
	session_obj->pdev.id = -1;
	session_obj->pdev.dev.release = &ce_dummy_release;
	err = platform_device_register(&session_obj->pdev);
	if (0 != err) {
		CE_ERROR("Error registering platform device (%d)\n", err);
		goto failed_platform_init;
	}

	/* Get the session key rules */
	if (strncmp(stm_ce_global->session_rules.rules_name, scheme,
			RULES_NAME_MAX_LEN)){
		/* Rules do not match - load a new one */
		err = load_rules(scheme, session_obj);
		if (err) {
			CE_ERROR("Failed to load session profile %s (%d)\n",
					scheme, err);
			goto failed_request_firmware;
		}
	}

	/* Copy the key rules into the session */
	memcpy(&session_obj->rules, &stm_ce_global->session_rules.rules,
		sizeof(stm_ce_rule_set_t));

	CE_INFO("Created session: %s (%p)\n\tRules: %s (v=%d, rules=%d)\n",
			session_obj->tag, session_obj,
			session_obj->rules.name,
			session_obj->rules.version,
			session_obj->rules.num_rules);

	/* Add the session object to the registry */
	err = registry_session_add(session_obj);
	if (0 != err)
		goto failed_request_firmware;

	/* Initialize the session HAL object */
	err = (*session_obj->api->new)(session_obj);
	if (0 != err)
		goto failed_cefw_new;

	/* Release global stm_ce lock */
	OS_wunlock(stm_ce_global->lock);

	/* Set returned session object */
	*session = (stm_ce_handle_t)session_obj;

	CE_API_EXIT("%d\n", 0);
	return 0;

failed_cefw_new:
	/* Clean-up registry */
	registry_session_del(session_obj);
failed_request_firmware:
	/* Unregister platform device */
	platform_device_unregister(&session_obj->pdev);
failed_platform_init:
	/* Free the session object */
	OS_lock_del(session_obj->lock);
failed_hal_init:
#ifdef CONFIG_PM_RUNTIME
	/* We don't need anymore the device */
	pm_runtime_put(&stm_ce_global->pdev->dev);
#endif
	/* Release global stm_ce lock */
	OS_wunlock(stm_ce_global->lock);
	OS_free(session_obj);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_new);

int stm_ce_session_delete(const stm_ce_handle_t session)
{
	int err = 0;
	struct ce_session *session_obj;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Check session is valid */
	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Get session lock */
	OS_wlock(session_obj->lock);

	CE_INFO("Deleting session: %s (%p)\n", session_obj->tag, session_obj);

	/* Delete all child transforms */
	err = foreach_child_object(&session_obj->transform_list,
			(int (*)(void *))&stm_ce_transform_delete);
	if (0 != err) {
		CE_API_EXIT("%d\n", err);
		return err;
	}

	/* Clean-up HAL data */
	(*session_obj->api->del)(session_obj);

	/* Unregister platform device */
	platform_device_unregister(&session_obj->pdev);

	/* Clean-up registry */
	registry_session_del(session_obj);

#ifdef CONFIG_PM_RUNTIME
	/* We don't need anymore the device */
	pm_runtime_put(&stm_ce_global->pdev->dev);
#endif

	/* Poison session object "self" field to stop re-use of deleted
	 * handles */
	session_obj->self = NULL;
	OS_wunlock(session_obj->lock);
	OS_lock_del(session_obj->lock);

	/* Free the session object */
	OS_free(session_obj);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_delete);

int stm_ce_session_get_control(const stm_ce_handle_t session,
		const stm_ce_session_ctrl_t selector,
		uint32_t *value)
{
	struct ce_session *session_obj;
	int err;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == value || selector >= STM_CE_SESSION_CTRL_LAST)
		return -EINVAL;

	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Call internal get_control api */
	OS_rlock(session_obj->lock);
	err = (*session_obj->api->get_control)(session_obj, selector, value);
	OS_runlock(session_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_get_control);

int stm_ce_session_set_control(const stm_ce_handle_t session,
		const stm_ce_session_ctrl_t selector,
		const uint32_t value)
{
	struct ce_session *session_obj;
	int err = 0;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (selector >= STM_CE_SESSION_CTRL_LAST)
		return -EINVAL;

	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Call internal set_control api */
	OS_wlock(session_obj->lock);
	err = (*session_obj->api->set_control)(session_obj, selector, value);
	OS_wunlock(session_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_set_control);

/* TODO Ow! args are bi-directional for Direct Access. Discuss. */
int stm_ce_session_set_compound_control(
		const stm_ce_handle_t session,
		const stm_ce_session_cmpd_ctrl_t selector,
		void *args)
{
	int err;
	struct ce_session *session_obj;

	CE_API_ENTRY();

	err = ce_check_initialized();
	if (err)
		goto clean;

	/* Basic parameter checking */

	if (NULL == args) {
		err = -EINVAL;
		goto clean;
	}

	switch (selector) {
	case STM_CE_SESSION_CMPD_CTRL_DIRECT_ACCESS:
		break;
	case STM_CE_SESSION_CMPD_CTRL_FUSE_OP:
		break;
	default:
		err = -EINVAL;
		goto clean;
	}

	/* Get session object (checks validity of handle) */
	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Call internal set_control api */
	OS_wlock(session_obj->lock);
	err = (*session_obj->api->set_compound_control)(session_obj,
			selector, args);
	OS_wunlock(session_obj->lock);
clean:

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_set_compound_control);

int stm_ce_session_get_capabilities(const stm_ce_handle_t session,
		stm_ce_session_caps_t *caps)
{
	struct ce_session *session_obj;
	int err = 0;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	/* Basic parameter checking */
	if (NULL == caps)
		return -EINVAL;

	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Call internal get_capabilities api */
	OS_rlock(session_obj->lock);
	err = (*session_obj->api->get_capabilities)(session_obj, caps);
	OS_runlock(session_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_get_capabilities);

int stm_ce_session_set_rule_data(stm_ce_handle_t session,
		const int32_t key_rule_id,
		const stm_ce_buffer_t *buffers)
{
	struct ce_session *session_obj;
	int err = 0;

	CE_API_ENTRY();
	err = ce_check_initialized();
	if (err)
		return err;

	if (0 != ce_session_from_handle(session, &session_obj))
		return -EINVAL;

	/* Call internal set_rule_data api */
	OS_wlock(session_obj->lock);
	err = (*session_obj->api->set_rule_data)(session_obj, key_rule_id,
			buffers);
	OS_wunlock(session_obj->lock);

	CE_API_EXIT("%d\n", err);
	return err;
}
EXPORT_SYMBOL(stm_ce_session_set_rule_data);

/*
 * Internal API-level session functions
 */

/**
 * ce_session_from_handle() - Retrieves a session object from a handle
 */
int ce_session_from_handle(stm_ce_handle_t session,
		struct ce_session **object)
{
	struct ce_hal *hal;
	stm_object_h type;
	int err;

	*object = (struct ce_session *)session;

	/* Check if object appears to be valid */
	if (NULL == *object) {
		CE_ERROR("Received invalid session handle %p\n", session);
		return -EINVAL;
	}

	/* Check if object appears to be valid (check type) */
	type = NULL;
	err = stm_registry_get_object_type(*object, &type);
	if (err) {
		CE_ERROR("Received invalid session handle %p: %d\n",
			 session, err);
		return -EINVAL;
	}
	if (type == NULL) {
		CE_ERROR("Received invalid session handle %p\n",
			 session);
		return -EINVAL;
	}

	/* Check if object appears to be valid (match type) */
	hal = NULL;
	err = ce_hal_get_initialized(0, &hal);
	if (err) {
		CE_ERROR("Cannot retrieve hal: %d\n", err);
		return -EINVAL;
	}
	if (type != &hal->session_type) {
		CE_ERROR("Received invalid session handle %p\n", session);
		return -EINVAL;
	}

	/* Check if object appears to be valid (check self field) */
	if ((*object)->self != *object ||
			STM_CE_SESSION_MAGIC != (*object)->magic) {
		CE_ERROR("Received invalid session handle: %p\n", session);
		return -EINVAL;
	}
	return 0;
}

/*
 * ce_session_print - API-level print handler for session objects
 */
int ce_session_print(stm_object_h object, char *buf, size_t size,
		char *user_buf, size_t user_size)
{
	int n = 0;
	int i;
	struct ce_session *s = *(struct ce_session **)buf;

	n += snprintf(user_buf + n, user_size - n,
			"Name: %s\nAddress: %p\nHal: %s (%d)\n",
			s->tag, s, s->hal->name, s->hal->id);

	/* Print key rules */
	n += snprintf(user_buf + n, user_size - n,
			"Rules: %s, version=%u (%u rules)\n",
			s->rules.name, s->rules.version, s->rules.num_rules);
	for (i = 0; i < s->rules.num_rules; i++) {
		stm_ce_rule_t *rule = &s->rules.rules[i];
		n += snprintf(user_buf + n, user_size - n,
				"\tID=%u: %s, %d derivation(s), leaf key size=%u\n",
				rule->id, rule->name, rule->num_derivations,
				rule->hal_leaf_key_size);
	}

	/* Print HAL-specific data */
	n += (*s->api->print)(s, user_buf + n, user_size - n);

	return n;
}

