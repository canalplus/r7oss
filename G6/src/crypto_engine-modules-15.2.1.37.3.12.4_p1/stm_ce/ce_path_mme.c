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

Source file name : ce_path_mme.c

Implements tango HAL MME host interface and data marshalling
************************************************************************/

#include "ce_path.h"
#include "stm_ce_objects.h"
#include "stm_ce_mme_share.h"
#include "ce_path_mme.h"
#include "mme.h"
#include "stm_ce_fuse.h"
#include <linux/signal.h>

static void cb(MME_Event_t Event,
		MME_Command_t *CallbackData,
		void *UserData)
{
	CE_ENTRY();
}

/** Initialise the Multicom MME transformer. */
int ce_path_mme_init_transformer(MME_TransformerHandle_t *mme_transformer)
{
	MME_ERROR mme_error;
	MME_TransformerInitParams_t mme_init_params;
	MME_TransformerCapability_t mme_caps = {
		.StructSize = sizeof(MME_TransformerCapability_t),
		.TransformerInfoSize = 0,
		.TransformerInfo_p = NULL,
	};

	CE_ENTRY();

	/* The transformer version specifies the CE MME interface.
	 * We check the interface version matches that supported by the
	 * driver. Else error */
	mme_error = MME_GetTransformerCapability(STM_CE_MME_TRANSFORMER,
			&mme_caps);
	if (MME_SUCCESS != mme_error) {
		CE_ERROR("MME_GetTransformerCapability failed: %d\n",
				mme_error);
		return -ENODEV;
	}
	if (mme_caps.Version != STM_CE_MME_INTERFACE_VERSION) {
		CE_ERROR("MME interface mismatch Host=%d Slave=%d\n",
				STM_CE_MME_INTERFACE_VERSION,
				mme_caps.Version);
		return -ENODEV;
	}
	CE_INFO("STM CE MME interface version: %u\n", mme_caps.Version);

	/** Struct size - required for separately linked users of data. */
	mme_init_params.StructSize = sizeof(MME_TransformerInitParams_t);
	/** We're the only RT user, so priority unimportant. */
	mme_init_params.Priority = MME_PRIORITY_NORMAL;
	/** Notify mechanism unused. */
	mme_init_params.Callback = cb;
	/** N/A */
	mme_init_params.CallbackUserData = NULL;
	/** N/A */
	mme_init_params.TransformerInitParamsSize = 0;
	/** N/A */
	mme_init_params.TransformerInitParams_p = NULL;

	mme_error = MME_InitTransformer(STM_CE_MME_TRANSFORMER,
			&mme_init_params,
			mme_transformer);

	if (MME_SUCCESS != mme_error) {
		CE_ERROR("MME_InitTransformer(%d) failed", mme_error);
		return -ENODEV;
	}




	CE_EXIT("MME HOST: Transformer initialized.");

	return 0;
}

/** De-initialise the Multicom MME transformer. */
int ce_path_mme_term_transformer(MME_TransformerHandle_t mme_transformer)
{
	MME_ERROR mme_error;

	CE_ENTRY();

	mme_error = MME_TermTransformer(mme_transformer);
	if (MME_SUCCESS != mme_error)
		return -EBUSY;

	CE_EXIT();

	return 0;
}

/**
 * Initialise the Multicom MME command structure.
 *
 * Command structure is complex structure with fields used by MME during
 * command processing. The structure is zeroed and initial parameters set
 * for each command request.
 *
 * The command parameters structure is assigned to the command structure,
 * bidirectional parameter element.
 *
 * @param mme_command[in] Pointer to mme command structure.
 * @param command_params[in] Pointer to ce_path command parameters.
 */
static int ce_path_mme_init_params(MME_Command_t *mme_command,
		struct ce_mme_params_s *command_params)
{
	CE_ENTRY();

	/** Initialise generic command parameters. */

	/** Belts and braces. */
	memset(mme_command, 0, sizeof(MME_Command_t));

	/** Struct size - required for separately linked users of data. */
	mme_command->StructSize = sizeof(MME_Command_t);
	/** stm_ce commands are passed as a parameter. */
	mme_command->CmdCode = MME_SET_GLOBAL_TRANSFORM_PARAMS;
	/** We wait for the RT notification to complete. */
	mme_command->CmdEnd = MME_COMMAND_END_RETURN_WAKE;
	/** N/A */
	mme_command->DueTime = 0;
	/** No RO buffers. */
	mme_command->NumberInputBuffers = 0;
	/** No RW buffer. */
	mme_command->NumberOutputBuffers = 0;
	/** No IO Data */
	mme_command->DataBuffers_p = NULL;
	/** Note "AdditionalInfo..." used because it's bi-directional. */
	/** Size of path command parameters. */
	mme_command->CmdStatus.AdditionalInfoSize =
			sizeof(struct ce_mme_params_s);
	/** Path command parameters and returned results. */
	mme_command->CmdStatus.AdditionalInfo_p = command_params;
	/** Not used. Not bi-directional. */
	mme_command->ParamSize = 0;
	/** Not used. Not bi-directional. */
	mme_command->Param_p = NULL;

	CE_EXIT();

	return 0;
}

/**
 * Execute ce_path command over Multicom MME interface.
 *
 * Command processing is synchronous. We wait for a command to complete before
 * continuing.
 */
static int ce_path_mme_send_command(MME_TransformerHandle_t mme_transformer,
		MME_Command_t *mme_command,
		struct ce_mme_params_s *command_params,
		MME_DataBuffer_t **buffers, uint32_t nb_in_buffers,
		uint32_t nb_out_buffers)
{
	MME_ERROR mme_error;
	MME_Event_t event;
	int err;
	sigset_t allset, oldset;

	CE_ENTRY();

	/* Block all signals during the WaitCommand sleep
	   (i.e. work correctly when called via ^C) */

	sigfillset(&allset);
	sigprocmask(SIG_BLOCK, &allset, &oldset);

	ce_path_mme_init_params(mme_command, command_params);

	if (buffers) {
		mme_command->DataBuffers_p = buffers;
		mme_command->NumberInputBuffers = nb_in_buffers;
		mme_command->NumberOutputBuffers = nb_out_buffers;
	}

	mme_error = MME_SendCommand(mme_transformer,
			mme_command);
	if (MME_SUCCESS != mme_error) {
		CE_ERROR("MME_SendCommand (%d)", mme_error);
		/* Restore the original signal mask */
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		return -EINVAL;
	}

	mme_error = MME_WaitCommand(mme_transformer,
			mme_command->CmdStatus.CmdId,
			&event,
			MME_TIMEOUT_INFINITE);

	/* Restore the original signal mask */
	sigprocmask(SIG_SETMASK, &oldset, NULL);

	if (MME_SUCCESS != mme_error) {
		CE_ERROR("MME_WaitCommand (%d)", mme_error);
		return -EINVAL;
	}

	if (MME_COMMAND_COMPLETED_EVT != event) {
		CE_ERROR("Not MME_COMMAND_COMPLETED_EVT is (%d)", event);
		return -EINVAL;
	}

	if (MME_COMMAND_COMPLETED != mme_command->CmdStatus.State) {
		CE_ERROR("Not MME_COMMAND_COMPLETED is (%d)", \
				mme_command->CmdStatus.State);
		return -EINVAL;
	}

	/* Return ce error code from MME command (translate from CE MME
	 * errorcode) */
	switch (command_params->err) {
	case CE_MME_SUCCESS:
		err = 0;
		break;
	case CE_MME_EINVAL:
		err = -EINVAL;
		break;
	case CE_MME_ENOTSUP:
		err = -ENOSYS;
		break;
	case CE_MME_ENOMEM:
		err = -ENOMEM;
		break;
	case CE_MME_EIO:
	default:
		err = -EIO;
		break;
	}
	if (err) {
		CE_ERROR("Slave error (%d): HAL code=%d (0x%x); Info: %s\n",
				command_params->err,
				command_params->u.error.hal_code,
				command_params->u.error.hal_code,
				command_params->u.error.text);
	}

	return err;
}

/** Marshal key buffers for protecting keys, decryption keys, IV's, etc.  */
static int marshal_key(struct keys_mme_s *keys_out,
		const uint32_t rule_id,
		const stm_ce_buffer_t *keys_in,
		const uint32_t num_buffers,
		const uint32_t key_num)
{
	int err = 0;
	int i;

	CE_ENTRY();

	/* Too many buffers? */
	if (num_buffers > MME_MAX_KEY_BUFS) {
		CE_ERROR("Too many key buffers (%d > %d)\n", num_buffers,
				MME_MAX_KEY_BUFS);
		return -EINVAL;
	}

	keys_out->rule_id = rule_id;
	keys_out->num_buffers = num_buffers;
	keys_out->key_num = key_num;

	/* Key reset/ invalidate, identified via keys_in set to NULL.
	 * In HAL, identified by 0 == num_buffers.
	 */
	if (NULL == keys_in)
		keys_out->num_buffers = 0;
	else {

		/* Copy key buffers into MME buffer */
		for (i = 0; i < num_buffers; i++) {
			if (keys_in[i].size > MME_MAX_KEY_LEN) {
				CE_ERROR("Key[%d] too big (%d > %d)\n", i,
						keys_in[i].size,
						MME_MAX_KEY_LEN);
				return -EINVAL;
			}
			memcpy(&(keys_out->buf[i].data),
					keys_in[i].data,
					keys_in[i].size);
			keys_out->buf[i].size = keys_in[i].size;
		}
	}
	CE_EXIT();

	return err;
}

/** Marshal key buffers for protecting keys, decryption keys, IV's, etc.  */
static int marshal_key_Ex(struct keys_mme_s *keys_out,
		const uint32_t rule_id,
		const stm_ce_buffer_t *keys_in,
		const uint32_t num_buffers,
		const uint32_t key_num,
		const stm_ce_key_operation_t keyop)
{
	int err = 0;
	int i;

	CE_ENTRY();

	/* Too many buffers? */
	if (num_buffers > MME_MAX_KEY_BUFS) {
		CE_ERROR("Too many key buffers (%d > %d)\n", num_buffers,
				MME_MAX_KEY_BUFS);
		return -EINVAL;
	}

	keys_out->rule_id = rule_id;
	keys_out->num_buffers = num_buffers;
	keys_out->key_num = key_num;
	keys_out->keyop = keyop;

	/* Key reset/ invalidate, identified via keys_in set to NULL.
	 * In HAL, identified by 0 == num_buffers.
	 */
	if (NULL == keys_in)
		keys_out->num_buffers = 0;
	else {

		/* Copy key buffers into MME buffer */
		for (i = 0; i < num_buffers; i++) {
			if (keys_in[i].size > MME_MAX_KEY_LEN) {
				CE_ERROR("Key[%d] too big (%d > %d)\n", i,
						keys_in[i].size,
						MME_MAX_KEY_LEN);
				return -EINVAL;
			}
			memcpy(&(keys_out->buf[i].data),
					keys_in[i].data,
					keys_in[i].size);
			keys_out->buf[i].size = keys_in[i].size;
		}
	}
	CE_EXIT();

	return err;
}

/** MME Command session and transform DIRECT ACCESS */
int ce_path_mme_direct_access(struct ce_session *session,
		struct ce_transform *transform,	void *args)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param = NULL;
	stm_ce_buffer_t *buf;

	CE_ENTRY();

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param) {
		err = -ENOMEM;
		goto clean;
	}

	mme_trans = session->hal->hal_data->mme_transformer;

	/* Tx data */
	buf = (stm_ce_buffer_t *) args;

	if (sizeof(struct direct_access_mme_s) < buf->size) {
		CE_ERROR("Direct access buffer too big: Max(%d) Actual(%d)\n",
				sizeof(struct direct_access_mme_s), buf->size);
		err = -EINVAL;
		goto clean;
	}

	param->ce_mme_command = MME_CE_DIRECT_ACCESS;
	param->session = (stm_ce_handle_t)session;
	param->transform = (stm_ce_handle_t)transform;

	memcpy(param->u.da, buf->data, buf->size);

	err = ce_path_mme_send_command(mme_trans, &mme_command, param, NULL,
			0, 0);

	/* Rx data */
	memcpy(buf->data, param->u.da, buf->size);

clean:
	if (param)
		OS_free(param);

	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command session and transform FUSE COMMAND */
int ce_path_mme_fuse_command(struct ce_session *session,
		struct ce_transform *transform,	void *args)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param = NULL;
	struct stm_ce_fuse_op *buf = NULL;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;
	mme_trans = session->hal->hal_data->mme_transformer;

	/* input data */
	buf = (struct stm_ce_fuse_op *) args;

	param->ce_mme_command = MME_CE_FUSE_COMMAND;
	param->session = (stm_ce_handle_t)session;
	param->transform = (stm_ce_handle_t)transform;
	memcpy(param->u.fc, buf, sizeof(struct stm_ce_fuse_op));

	err = ce_path_mme_send_command(mme_trans, &mme_command, param, NULL,
			0, 0);

	/* output data */
	memcpy(buf, param->u.fc, sizeof(struct stm_ce_fuse_op));

	OS_free(param);
	CE_EXIT("%d", err);
	return err;
}

/** MME Command INIT. */
int ce_path_mme_init(MME_TransformerHandle_t mme_transformer)
{
	int err = 0;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	param->ce_mme_command = MME_CE_INIT;
	err = ce_path_mme_send_command(mme_transformer, &mme_command, param,
			NULL, 0, 0);

	OS_free(param);
	CE_EXIT("%d", err);
	return err;
}

/** MME Command TERM */
int ce_path_mme_term(MME_TransformerHandle_t mme_transformer)
{
	int err = 0;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	param->ce_mme_command = MME_CE_TERM;
	err = ce_path_mme_send_command(mme_transformer, &mme_command, param,
			NULL, 0, 0);

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command GET_VERSION */
int ce_path_mme_get_version(MME_TransformerHandle_t mme_transformer,
		stm_ce_version_t *versions, uint32_t max_versions,
		uint32_t *n_versions)
{
	int err = 0;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;
	int i;

	/* Store version info returned from slave in a static area so it can
	 * be referenced elsewhere */
	static struct ce_mme_version_s mme_ver[CE_MME_MAX_VERSIONS];

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	param->ce_mme_command = MME_CE_GET_VERSION;
	err = ce_path_mme_send_command(mme_transformer, &mme_command, param,
			NULL, 0, 0);

	memcpy(&mme_ver, param->u.version_info.v,
			param->u.version_info.n_versions *
			sizeof(struct ce_mme_version_s));

	*n_versions = (uint32_t)min(param->u.version_info.n_versions,
			max_versions);
	if (*n_versions != param->u.version_info.n_versions) {
		CE_ERROR("Slave returned too many versions (%d)\n",
				param->u.version_info.n_versions);
	}
	CE_INFO("Received %d versions from slave\n", *n_versions);

	/* Copy version info into ce version structs */
	for (i = 0; i < *n_versions; i++) {
		versions[i].name = mme_ver[i].name;
		versions[i].major = mme_ver[i].major;
		versions[i].minor = mme_ver[i].minor;
		versions[i].patch = mme_ver[i].patch;
	}

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command SEND_DATA */
int ce_path_mme_send_data(MME_TransformerHandle_t mme_transformer,
		uint32_t id, const uint8_t *data, uint32_t size,
		uint32_t ro, uint32_t rw)
{
	int err;
	MME_Command_t mme_command;
	MME_DataBuffer_t *buf = NULL;
	struct ce_mme_params_s *param;
	MME_ERROR mme_error;

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	/* Allocate MME buffer and copy data into it (slower, but avoids
	 * having to deal with non-contiguous buffers) */
	mme_error = MME_AllocDataBuffer(mme_transformer,
			size, MME_ALLOCATION_PHYSICAL, &buf);
	if (MME_SUCCESS != mme_error) {
		CE_ERROR("Failed to allocate MME buffer (sz=%d) err=%d\n",
				size, mme_error);
		err = -ENOMEM;
		goto cleanup;
	}
	memcpy(buf->ScatterPages_p[0].Page_p, data, size);

	param->ce_mme_command = MME_CE_SEND_DATA;
	param->u.send_data.id = id;
	param->u.send_data.size = size;
	err = ce_path_mme_send_command(mme_transformer, &mme_command, param,
			&buf, ro, rw);

cleanup:
	if (buf)
		MME_FreeDataBuffer(buf);
	if (param)
		OS_free(param);

	CE_EXIT("%d\n", err);
	return err;
}


/** MME Command SESSION NEW */
int ce_path_mme_session_new(struct ce_session *session)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	MME_DataBuffer_t *rules_buffer = NULL;
	struct ce_mme_params_s *param;
	MME_ERROR mme_error;

	CE_ENTRY();

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = session->hal->hal_data->mme_transformer;

	/* Rules structure may be large, so we use an MME buffer to send it */
	mme_error = MME_AllocDataBuffer(mme_trans,
			sizeof(stm_ce_rule_set_t),
			MME_ALLOCATION_PHYSICAL,
			&rules_buffer);
	if (MME_SUCCESS != mme_error) {
		CE_ERROR("Failed to allocate MME buffer for rules (err=%d)\n",
				mme_error);
		err = -ENOMEM;
		goto cleanup;
	}
	memcpy(rules_buffer->ScatterPages_p[0].Page_p, &session->rules,
			sizeof(stm_ce_rule_set_t));

	param->ce_mme_command = MME_CE_SESSION_NEW;
	param->session = (stm_ce_handle_t)session;
	err = ce_path_mme_send_command(mme_trans, &mme_command, param,
			&rules_buffer, 1, 0);

cleanup:
	if (rules_buffer)
		MME_FreeDataBuffer(rules_buffer);
	if (param)
		OS_free(param);

	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command SESSION DELETE */
int ce_path_mme_session_del(struct ce_session *session)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = session->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_SESSION_DEL;
	param->session = (stm_ce_handle_t)session;
	err = ce_path_mme_send_command(mme_trans, &mme_command, param, NULL,
			0, 0);

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}


/** MME Command SET RULE DATA*/
int ce_path_mme_session_set_rule_data(struct ce_session *session,
		const uint32_t rule_id,
		const stm_ce_buffer_t *buffers,
		uint32_t num_buffers)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = session->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_SESSION_SET_RULE_DATA;
	param->session = (stm_ce_handle_t)session;

	err = marshal_key(&param->u.keys, rule_id, buffers,
			num_buffers, 0);
	if (0 == err) {
		err = ce_path_mme_send_command(mme_trans, &mme_command, param,
				NULL, 0, 0);
	}

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM NEW */
int ce_path_mme_transform_new(struct ce_transform *transform,
		stm_ce_transform_type_t type, uint32_t n_stages,
		uint32_t *path_id)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_TRANSFORM_NEW;
	param->transform = (stm_ce_handle_t)transform;
	param->transform_type = type;
	param->session = (stm_ce_handle_t)transform->parent;
	param->stage = n_stages;
	err = ce_path_mme_send_command(mme_trans, &mme_command, param,
					NULL, 0, 0);

	if (0 == err)
		*path_id = param->u.path_id;

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM DELETE */
int ce_path_mme_transform_del(struct ce_transform *transform)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_TRANSFORM_DEL;
	param->transform = (stm_ce_handle_t)transform;
	param->session = (stm_ce_handle_t)transform->parent;
	err = ce_path_mme_send_command(mme_trans, &mme_command, param, NULL,
			0, 0);

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM SET KEY */
int ce_path_mme_transform_set_key(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *key,
		const uint32_t key_num,
		const int key_not_iv)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	if (key_not_iv)
		param->ce_mme_command = MME_CE_TRANSFORM_SET_KEY;
	else
		param->ce_mme_command = MME_CE_TRANSFORM_SET_IV;
	param->session = (stm_ce_handle_t)transform->parent;
	param->transform = (stm_ce_handle_t)transform;
	param->stage = stage;

	err = marshal_key(&param->u.keys, STM_CE_KEY_RULES_NONE, key, 1,
			key_num);
	if (0 == err) {
		err = ce_path_mme_send_command(mme_trans, &mme_command, param,
				NULL, 0, 0);
	}

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM SET KEY EXTENDED */
int ce_path_mme_transform_set_key_Ex(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *key,
		const uint32_t key_num,
		const int key_not_iv,
		const stm_ce_key_operation_t keyop)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	if (key_not_iv)
		param->ce_mme_command = MME_CE_TRANSFORM_SET_KEY_EX;
	else
		param->ce_mme_command = MME_CE_TRANSFORM_SET_IV;
	param->session = (stm_ce_handle_t)transform->parent;
	param->transform = (stm_ce_handle_t)transform;
	param->stage = stage;

	err = marshal_key_Ex(&param->u.keys, STM_CE_KEY_RULES_NONE, key, 1,
			key_num, keyop);
	if (0 == err) {
		err = ce_path_mme_send_command(mme_trans, &mme_command, param,
				NULL, 0, 0);
	}

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM SET IV */
int ce_path_mme_transform_set_iv(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *iv,
		const uint32_t key_num)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_TRANSFORM_SET_IV;
	param->session = (stm_ce_handle_t)transform->parent;
	param->transform = (stm_ce_handle_t)transform;
	param->stage = stage;

	err = marshal_key(&param->u.keys, STM_CE_KEY_RULES_NONE, iv, 1,
			key_num);
	if (0 == err) {
		err = ce_path_mme_send_command(mme_trans, &mme_command, param,
				NULL, 0, 0);
	}

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

/** MME Command TRANSFORM SET CONFIG */
int ce_path_mme_transform_set_config(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_transform_config_t *config)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();
	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	mme_trans = transform->parent->hal->hal_data->mme_transformer;
	param->ce_mme_command = MME_CE_TRANSFORM_SET_CONFIG;
	param->transform = (stm_ce_handle_t)transform;
	param->session = (stm_ce_handle_t)transform->parent;
	param->stage = stage;
	memcpy(&param->u.config, config, sizeof(stm_ce_transform_config_t));

	err = ce_path_mme_send_command(mme_trans, &mme_command, param, NULL,
			0, 0);

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}

static int ce_condense_buffers(const stm_ce_buffer_t *in_bufs,
		stm_ce_buffer_t *out_bufs, const uint32_t n_in_bufs)
{
	int i;
	int out_index = 0;

	out_bufs[0].data = in_bufs[0].data;
	out_bufs[0].size = in_bufs[0].size;

	for (i = 1; i < n_in_bufs; i++) {
		if (in_bufs[i].data == out_bufs[out_index].data +
				out_bufs[out_index].size) {
			/* Contiguous */
			out_bufs[out_index].size += in_bufs[i].size;
		} else {
			out_index++;
			out_bufs[out_index].data = in_bufs[i].data;
			out_bufs[out_index].size = in_bufs[i].size;
		}
	}
	if (out_index + 1 < n_in_bufs) {
		CE_INFO("Condensed buffer list from %d -> %d pages\n",
				n_in_bufs, out_index);
	}

	return out_index + 1;
}

static void print_ce_buffers(const stm_ce_buffer_t *src,
		const uint32_t n_src,
		const stm_ce_buffer_t *dst,
		const uint32_t n_dst)
{
	int i;
	CE_INFO("Src Buffers\t\t Dst Buffers\n");
	for (i = 0; i < max(n_src, n_dst); i++) {
		CE_INFO("0x%p:0x%p\t0x%p:0x%p\n",
				i >= n_src ? NULL : src[i].data,
				i >= n_src ? NULL : src[i].data + src[i].size,
				i >= n_dst ? NULL : dst[i].data,
				i >= n_dst ? NULL : dst[i].data + dst[i].size);
	}
}

int ce_path_mme_transform_dma(struct ce_transform *transform,
		const stm_ce_buffer_t *src, const uint32_t n_src,
		const stm_ce_buffer_t *dst, const uint32_t n_dst)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_DataBuffer_t *dma_buffer;
	MME_Command_t mme_command;
	MME_ERROR mme_error;
	stm_ce_buffer_t *bufs;
	struct ce_mme_params_s *param;
	int n_merged_src;
	int n_merged_dst;

	CE_ENTRY();

	mme_trans = transform->parent->hal->hal_data->mme_transformer;

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (NULL == param)
		return -ENOMEM;

	/* The buffer structs should all point to physical addresses, so we
	 * only need to send the list of buffer structs for src and dst */
	mme_error = MME_AllocDataBuffer(mme_trans,
			sizeof(stm_ce_buffer_t) * (n_src + n_dst),
			MME_ALLOCATION_PHYSICAL, &dma_buffer);
	if (MME_SUCCESS != mme_error) {
		CE_ERROR("Failed to allocate MME buffer for dma list\n");
		err = -ENOMEM;
		goto cleanup;
	}
	bufs = (stm_ce_buffer_t *)dma_buffer->ScatterPages_p[0].Page_p;

	/* Condense ce buffer lists by merging physically-contiguous pages */
	n_merged_src = ce_condense_buffers(src, bufs, n_src);
	n_merged_dst = ce_condense_buffers(dst, &bufs[n_merged_src], n_dst);

	if (ce_trace_info)
		print_ce_buffers(bufs, n_merged_src, &bufs[n_merged_src],
				n_merged_dst);

	param->ce_mme_command = MME_CE_TRANSFORM_DMA;
	param->transform = (stm_ce_handle_t)transform;
	param->session = (stm_ce_handle_t)transform->parent;
	param->u.dma.n_src_buffers = n_merged_src;
	param->u.dma.n_dst_buffers = n_merged_dst;

	err = ce_path_mme_send_command(mme_trans, &mme_command, param,
			&dma_buffer, 1, 0);
	if (err) {
		pr_err("DMA error\n");
		print_ce_buffers(bufs, n_merged_src, &bufs[n_merged_src],
				n_merged_dst);
	}

cleanup:
	if (dma_buffer)
		MME_FreeDataBuffer(dma_buffer);
	if (param)
		OS_free(param);
	CE_EXIT("%d\n", err);
	return err;
}



int ce_path_mme_session_power_manage(struct ce_session *session,
		stm_ce_power_state_profile_t power_state_profile,
		const int power_down)
{
	int err = 0;
	MME_TransformerHandle_t mme_trans;
	MME_Command_t mme_command;
	struct ce_mme_params_s *param;

	CE_ENTRY();

	param = OS_calloc(sizeof(struct ce_mme_params_s));
	if (param == NULL)
		return -ENOMEM;

	mme_trans = session->hal->hal_data->mme_transformer;
	if (power_down)
		param->ce_mme_command = MME_CE_SESSION_POWER_STATE_SUSPEND;
	else
		param->ce_mme_command = MME_CE_SESSION_POWER_STATE_RESUME;
	param->session = (stm_ce_handle_t)session;
	param->u.power_state_type = power_state_profile;

	err = ce_path_mme_send_command(mme_trans, &mme_command, param,
				NULL, 0, 0);

	OS_free(param);
	CE_EXIT("%d\n", err);
	return err;

}
