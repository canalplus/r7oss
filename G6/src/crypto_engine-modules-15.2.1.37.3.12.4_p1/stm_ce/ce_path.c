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

Source file name : ce_path.c

Defines tango HAL management functions
************************************************************************/
#include "ce_hal.h"
#include "ce_path.h"
#include "ce_path_mme.h"

#include <linux/delay.h>

/* Linux firmware loading interface used for ktd loading */
#include <linux/firmware.h>

static void dummy_release(struct device *dev) { }

int ce_path_hal_register(struct ce_hal *hal)
{
	int err;
	struct ce_hal *temp_hal = NULL;

	/* Allocate HAL data */
	hal->hal_data = OS_calloc(sizeof(struct ce_hal_data));
	if (NULL == hal->hal_data)
		return -ENOMEM;

	/* Add tango transform/session type objects to registry */
	err = stm_registry_add_object(&stm_ce_global->session_type,
			STM_CE_PATH_SESSION_TYPE_NAME,
			&hal->session_type);
	if (err != 0) {
		CE_ERROR("Failed to add type %s to registry (%d)\n",
				STM_CE_PATH_SESSION_TYPE_NAME, err);
		goto clean;
	}
	err = stm_registry_add_object(&stm_ce_global->transform_type,
			STM_CE_PATH_TRANSFORM_TYPE_NAME,
			&hal->transform_type);
	if (err != 0) {
		CE_ERROR("Failed to add type %s to registry (%d)\n",
				STM_CE_PATH_TRANSFORM_TYPE_NAME, err);
		goto clean;
	}

	/* Add path id interface to tango transform type */
	err = stm_registry_add_attribute(&hal->transform_type,
			STM_PATH_ID_INTERFACE, STM_REGISTRY_ADDRESS,
			&ce_path_interface, sizeof(ce_path_interface));
	if (err != 0) {
		CE_ERROR("Failed to add interface %s (%d)\n",
				STM_PATH_ID_INTERFACE, err);
		goto clean;
	}

	/* Add data push interface to tango transform type */
	err = stm_registry_add_attribute(&hal->transform_type,
			STM_DATA_INTERFACE_PUSH, STM_REGISTRY_ADDRESS,
			&ce_data_push_interface,
			sizeof(ce_data_push_interface));
	if (err != 0) {
		CE_ERROR("Failed to add interface %s (%d)\n",
				STM_DATA_INTERFACE_PUSH, err);
		goto clean;
	}

	/* Register ktd platform device */
	hal->hal_data->ktd_pdev.name = STM_CE_KTD_DEV_NAME;
	hal->hal_data->ktd_pdev.id = -1;
	hal->hal_data->ktd_pdev.dev.release = &dummy_release;
	err = platform_device_register(&hal->hal_data->ktd_pdev);
	if (0 != err) {
		CE_ERROR("Error registering platform device (%d)\n", err);
		goto clean;
	}

	/* FIXME:
	 * We initialize the path hal at registration time, since the
	 * initialization currently is the method for loading the Tango SC
	 * firmware. The Transport Engine HAL requires that Tango SC firmware
	 * is loaded.
	 * In future, when Tango SC firmware is loaded externally, this will
	 * no longer be necessary */
	err = ce_hal_get_initialized(hal->id, &temp_hal);
	if (err)
		goto clean;

	return 0;
clean:
	ce_path_hal_unregister(hal);
	return err;
}

int ce_path_hal_unregister(struct ce_hal *hal)
{
	int err = 0;
	if (hal->hal_data) {
		platform_device_unregister(&hal->hal_data->ktd_pdev);
		err = stm_registry_remove_object(&hal->transform_type);
		if (0 != err)
			CE_ERROR("Failed to remove transform object err %d\n",
						err);
		err = stm_registry_remove_object(&hal->session_type);
		if (0 != err)
			CE_ERROR("Failed to remove Session object  err %d\n",
						err);
	}
	OS_free(hal->hal_data);

	return 0;
}

#define MME_INIT_RETRY_CNT	10
#define MME_INIT_RETRY_SLEEP	10

int ce_path_hal_init(struct ce_hal *hal)
{
	int err = 0;
	unsigned int i = 0;

#if defined(CONFIG_CRYPTO_ENGINE_FIRMWARE_ENABLED)
	while (i++ < MME_INIT_RETRY_CNT) {
		/* Initialize Host-side MME layer */
		err = ce_path_mme_init_transformer(
					&hal->hal_data->mme_transformer);
		if (!err)
			break;

		msleep(MME_INIT_RETRY_SLEEP);
	}

	/* Request KTD using kernel firmware loading interface (if
	 * available) */
	if (0 == err) {
		int ktd_err;
		const struct firmware *fw = NULL;

		ktd_err = request_firmware(&fw, "sttkd",
				&hal->hal_data->ktd_pdev.dev);
		if (0 == ktd_err) {
			CE_INFO("KTD found: %d bytes\n", fw->size);
			err = ce_path_mme_send_data(
					hal->hal_data->mme_transformer,
					CE_MME_DATA_STTKD_KTD, fw->data,
					fw->size, 1, 0);
			if (0 != err) {
				CE_ERROR("Failed to send KTD data to slave "\
						"(%d)\n", err);
			}
			release_firmware(fw);
		}
	}
	/* Send initialize command via MME */
	if (0 == err)
		err = ce_path_mme_init(hal->hal_data->mme_transformer);

	/* Workaround for bug 28251 - Set T3 req filter range registers for
	 * LMI0/1 boundary
	 * TODO: Remove this once sec bug 4367 has been fixed (STTKD driver
	 * sets registers itself) */
#if defined(CONFIG_MACH_STM_STIH415)
	{
		/* Force T3 req filter boundaries to 0x80000000 */
		uint32_t *regs = ioremap_nocache(0xfd5ad06c, 0xc);
		CE_INFO("Setting T3 req filter range workaround\n");
		regs[0] = 0x80000000;
		regs[1] = 0x80000000;
		regs[2] = 0x80000000;
		iounmap(regs);
	}
#endif

#else
	CE_INFO("CE MME layer initalisation bypassed\n");
#endif

	if (err)
		goto mme_term;

	CE_INFO("Initialized path HAL\n");
	return err;

mme_term:
	CE_ERROR("Initializing path HAL failed (%d)\n", err);

	/* Terminate Host-side MME layer */
	if (ce_path_mme_term(hal->hal_data->mme_transformer))
		CE_ERROR("ce_path_mme_term failed\n");

	return err;
}

int ce_path_hal_term(struct ce_hal *hal)
{
	int err;

	/* Send terminate command via MME (ignore return code - we are going
	 * to terminate the MME layer anyway) */
	err = ce_path_mme_term(hal->hal_data->mme_transformer);
	if (0 != err)
		CE_ERROR("ce_path_mme_term returned %d\n", err);

	/* Terminate Host-side MME layer */
	err = ce_path_mme_term_transformer(hal->hal_data->mme_transformer);
	if (0 != err)
		CE_ERROR("ce_path_mme_term_transformer returned %d\n", err);

	/* Mark HAL as "terminated" */
	hal->initialized = 0;

	CE_INFO("Terminated path HAL\n");

	return 0;
}

int ce_path_hal_get_version(struct ce_hal *hal,
		stm_ce_version_t *versions, uint32_t max_versions,
		uint32_t *n_versions)
{
	int err = 0;
	*n_versions = 0;

	if (hal->initialized) {
		err = ce_path_mme_get_version(hal->hal_data->mme_transformer,
				versions, max_versions, n_versions);
	}

	return err;
}
