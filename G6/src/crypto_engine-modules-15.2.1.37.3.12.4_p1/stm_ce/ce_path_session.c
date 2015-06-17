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

Source file name : ce_path_session.c

Defines tango HAL functions for session objects
************************************************************************/
#include "stm_ce_objects.h"
#include "ce_path.h"
#include "ce_path_mme.h"

/*
 * Retrieves a pointer to the required key rule, if valid for this session
 */
static int get_rule(struct ce_session *s, uint32_t id,
		struct stm_ce_rule_s **rule)
{
	int i;
	for (i = 0; i < s->rules.num_rules; i++) {
		if (id == s->rules.rules[i].id) {
			*rule = &s->rules.rules[i];
			return 0;
		}
	}
	return -EINVAL;
}

static int ce_path_session_new(struct ce_session *session)
{
	int err = 0;

	CE_ENTRY("session=%p\n", session);

	/* Allocate model-specific data struct */
	session->data = OS_malloc(sizeof(struct ce_session_data));
	if (NULL == session->data) {
		CE_ERROR("Failed to allocate session data\n");
		return -ENOMEM;
	}

	/* Call the HAL layer to create a new session */
	err = ce_path_mme_session_new(session);
	if (0 != err)
		goto clean;

	CE_EXIT("%d\n", err);
	return 0;

clean:
	OS_free(session->data);
	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_session_del(struct ce_session *session)
{
	int err = 0;

	CE_ENTRY("session %p\n", session);

	/* Call the HAL layer to delete this session */
	err = ce_path_mme_session_del(session);

	/* Free model-specific data struct */
	OS_free(session->data);

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_path_session_get_control(struct ce_session *session,
		const stm_ce_session_ctrl_t selector,
		uint32_t *value)
{
	return 0;
}

static int ce_path_session_set_control(struct ce_session *session,
		const stm_ce_session_ctrl_t selector,
		const uint32_t value)
{
	return 0;
}

static int ce_path_session_set_compound_control(
		struct ce_session *session,
		const stm_ce_session_cmpd_ctrl_t selector,
		void *args)
{
	int err;

	CE_ENTRY();

	switch (selector) {
	case STM_CE_SESSION_CMPD_CTRL_DIRECT_ACCESS:
		err = ce_path_mme_direct_access(session, NULL, args);
		break;
	case STM_CE_SESSION_CMPD_CTRL_FUSE_OP:
		err = ce_path_mme_fuse_command(session, NULL, args);
		break;
	default:
		err = -EINVAL;
	};

	CE_EXIT();
	return err;
}

static int ce_path_session_get_caps(struct ce_session *session,
		stm_ce_session_caps_t *caps)
{
	return 0;
}

static int ce_path_session_set_rule_data(struct ce_session *session,
		const int32_t key_rule_id,
		const stm_ce_buffer_t *buffers)
{
	int err;
	struct stm_ce_rule_s *rule;

	/* Check the rule is valid */
	err = get_rule(session, key_rule_id, &rule);
	if (0 != err) {
		CE_ERROR("Session %p: Invalid rule id: %d\n", session,
				key_rule_id);
		return -EINVAL;
	}

	/* Call the MME layer to set the ladder */
	/* set all but last protecting key */
	return ce_path_mme_session_set_rule_data(session, key_rule_id,
			buffers,
			rule->num_derivations-1);
}

static int ce_path_session_print(struct ce_session *session,
		char *print_buffer, uint32_t buffer_size)
{
	/* Currently the path HAL prints no additional session information */
	return 0;
}

/*
 * Exported ce session API
 */
struct ce_session_api ce_path_session_api = {
	.new = &ce_path_session_new,
	.del = &ce_path_session_del,
	.get_control = &ce_path_session_get_control,
	.set_control = &ce_path_session_set_control,
	.set_compound_control = &ce_path_session_set_compound_control,
	.get_capabilities = &ce_path_session_get_caps,
	.set_rule_data = &ce_path_session_set_rule_data,
	.print = &ce_path_session_print,
};

