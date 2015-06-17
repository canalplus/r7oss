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

Source file name : ce_hal_manager.c

Implements stm_ce HAL management functions
************************************************************************/
#include "ce_hal.h"
#include "stm_ce_key_rules.h"
#include "ce_path.h"

/* List of available HALs */
static struct ce_hal hal_list[] = {
	{
		STM_CE_HAL_TANGO,
		"Tango/STTKD",
		0,
		&ce_path_hal_register,
		&ce_path_hal_unregister,
		&ce_path_hal_init,
		&ce_path_hal_term,
		&ce_path_hal_get_version,
		&ce_path_session_api,
		&ce_path_transform_api,
	},
};
#define N_HAL (sizeof(hal_list)/sizeof(struct ce_hal))

/* Registers each HAL known to the HAL manager
 *
 * Registering a HAL typically involves registering a type object for the HAL
 * in the registry
 */
int ce_hal_register_all(void)
{
	int err = 0;
	int i;

	for (i = 0; i < N_HAL; i++) {
		err = (*hal_list[i].reg)(&hal_list[i]);
		if (0 != err) {
			CE_ERROR("Registering CE HAL: %s (%d)\n",
					hal_list[i].name, err);
			goto clean;
		}
		CE_INFO("Registered CE HAL: %s\n", hal_list[i].name);
	}

	return 0;
clean:
	/* Unregister registered HALs on failure */
	for (--i; i >= 0; i--)
		(*hal_list[i].unreg)(&hal_list[i]);

	return err;
}

/* Returns the specified HAL object (if available).
 *
 * The HAL object will be returned initialized. I.e. the HAL will be
 * initialized if not already
 */
int ce_hal_get_initialized(uint32_t id, struct ce_hal **hal)
{
	int err = 0;
	int i;

	for (i = 0; i < N_HAL; i++) {
		if (id == hal_list[i].id)
			*hal = &hal_list[i];
	}
	if (NULL == *hal) {
		CE_ERROR("HAL id %d not known\n", id);
		return -ENOSYS;
	}

	if (!(*hal)->initialized) {
		err = (*(*hal)->init)(*hal);
		if (0 != err) {
			CE_ERROR("Initializing HAL %s return %d\n",
					(*hal)->name, err);
		} else {
			(*hal)->initialized = 1;
			CE_INFO("Initialized HAL: %s\n", (*hal)->name);
		}
	}

	return err;
}

/* Unregisters each HAL known to the HAL manager
 *
 * This will terminate each HAL and then undo any state set-up by
 * ce_register_hals
 */
int ce_hal_unregister_all(void)
{
	int err;
	int i;

	for (i = 0; i < N_HAL; i++) {

		/* Terminate, then unregister each HAL */
		err = (*hal_list[i].term)(&hal_list[i]);
		if (0 != err) {
			CE_ERROR("Term CE HAL: %s (%d)\n",
					hal_list[i].name, err);
		} else {
			CE_INFO("Terminated CE HAL: %s\n", hal_list[i].name);
		}

		err = (*hal_list[i].unreg)(&hal_list[i]);
		if (0 != err) {
			CE_ERROR("Unregister CE HAL: %s (%d)\n",
					hal_list[i].name, err);
		} else {
			CE_INFO("Unregistered CE HAL: %s\n", hal_list[i].name);
		}
	}

	return 0;
}

/* Gets the version info for all registered HALs */
int ce_hal_get_version(stm_ce_version_t *version, uint32_t max_versions,
		uint32_t *n_versions)
{
	int i;
	int err;

	*n_versions = 0;

	for (i = 0 ; i < N_HAL; i++) {
		err = (*hal_list[i].get_version)(&hal_list[i],
				&version[*n_versions],
				max_versions - *n_versions, n_versions);
	}

	return err;
}

