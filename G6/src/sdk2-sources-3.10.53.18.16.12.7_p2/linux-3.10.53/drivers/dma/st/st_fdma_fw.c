/*
 * STMicroelectronics FDMA dmaengine driver firmware functions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This code borrows heavily from drivers/stm/fdma.c!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/sched.h>


#include <linux/platform_data/dma-st-fdma.h>
#include "st_fdma.h"


static int st_fdma_fw_check_header(struct st_fdma_device *fdev,
		struct ELF32_info *elfinfo)
{
	int i, loadable = 0;

	/* Check the firmware ELF header */
	if (elfinfo->header->e_type != ET_EXEC) {
		dev_err(fdev->dev, "Firmware invalid ELF type\n");
		return -EINVAL;
	}

	if (elfinfo->header->e_machine != EM_SLIM) {
		dev_err(fdev->dev, "Firmware invalid ELF machine\n");
		return -EINVAL;
	}

	if (elfinfo->header->e_flags != EF_SLIM_FDMA) {
		dev_err(fdev->dev, "Firmware invalid ELF flags\n");
		return -EINVAL;
	}

	/* We expect the firmware to contain 2 loadable segments.  We allow 1
	 * in case they've been combined.
	 * In fact it would be permissible to have more segments...we're just
	 * checking expectations.
	 */
	for (i = 0; i < elfinfo->header->e_phnum; ++i) {
		if (elfinfo->progbase[i].p_type == PT_LOAD)
			loadable++;
	}
	if (loadable < 1 || loadable > ST_FDMA_FW_SEGMENTS) {
		dev_err(fdev->dev,
			"Firmware has %d loadable segments (1 to %d expected)\n",
			loadable, ST_FDMA_FW_SEGMENTS);
		return -EINVAL;
	}

	return 0;
}

int st_fdma_fw_load(struct st_fdma_device *fdev, struct ELF32_info *elfinfo)
{
	int result;
	struct ELF32_LoadParams load_params = ELF_LOADPARAMS_INIT;

	BUG_ON(!fdev);
	BUG_ON(!elfinfo);

	/* Check the ELF file header */
	result = st_fdma_fw_check_header(fdev, elfinfo);
	if (result) {
		dev_err(fdev->dev, "Firmware header check failed\n");
		return result;
	}

	/* First ensure that the FDMA is disabled */
	st_fdma_hw_disable(fdev);

	/* Load the firmware segments to the FDMA */
	load_params.numAllowedRanges = 2;
	load_params.allowedRanges = kmalloc(sizeof(struct ELF32_MemRange) * 2,
					   GFP_KERNEL);
	if (!load_params.allowedRanges) {
		dev_err(fdev->dev, "Out of memory loading FDMA\n");
		return -ENOMEM;
	}
	load_params.allowedRanges[0].base =
			(uint32_t)fdev->io_res->start + fdev->hw->dmem.offset;
	load_params.allowedRanges[0].top =
			(uint32_t)fdev->io_res->start +	fdev->hw->dmem.offset +
			fdev->hw->dmem.size;
	load_params.allowedRanges[1].base =
			(uint32_t)fdev->io_res->start + fdev->hw->imem.offset;
	load_params.allowedRanges[1].top =
			(uint32_t)fdev->io_res->start + fdev->hw->imem.offset +
			fdev->hw->imem.size;
#ifdef CONFIG_ARM
	/* ARM does not allow multiple mappings of memory with different
	 * attributes.  Pass in the existing ioremapped range.
	 */
	load_params.numExistingMappings = 1;
	load_params.existingMappings = kmalloc(
			sizeof(struct ELF32_IORemapMapping), GFP_KERNEL);
	if (!load_params.existingMappings) {
		dev_err(fdev->dev, "Out of memory loading FDMA\n");
		ELF_LOADPARAMS_FREE(&load_params);
		return -ENOMEM;
	}
	load_params.existingMappings[0].physBase = fdev->io_res->start;
	load_params.existingMappings[0].vIOBase = fdev->io_base;
	load_params.existingMappings[0].size = resource_size(fdev->io_res);
	load_params.existingMappings[0].cached = false;
#endif /* CONFIG_ARM */
	result = ELF32_physLoad(elfinfo, &load_params, NULL);
	ELF_LOADPARAMS_FREE(&load_params);
	if (result) {
		dev_err(fdev->dev, "Failed to load FDMA\n");
		return result;
	}

	/* Now enable the FDMA */
	st_fdma_hw_enable(fdev);

	/* Enable all channels */
	result = st_fdma_hw_channel_enable_all(fdev);
	if (!result) {
		dev_err(fdev->dev, "Failed to enable all channels\n");
		return -ENODEV;
	}

	return 0;
}

static int st_fdma_fw_request(struct st_fdma_device *fdev)
{
	const struct firmware *fw = NULL;
	struct ELF32_info *elfinfo = NULL;
	int result = 0;
	int fw_major, fw_minor;
	int hw_major, hw_minor;
	uint8_t *fw_data;

	BUG_ON(!fdev);

	if (!fdev->fw_name) {
		dev_err(fdev->dev,"firware not defined!");
		result = -EINVAL;
		goto error_no_fw;
	}

	dev_notice(fdev->dev, "Requesting firmware: %s\n", fdev->fw_name);

	/* Request the FDMA firmware */
	result = request_firmware(&fw, fdev->fw_name, fdev->dev);
	if (result || !fw) {
		dev_err(fdev->dev, "Failed request firmware: not present?\n");
		result = -ENODEV;
		goto error_no_fw;
	}

	/* Set pointer to the firmware data */
	fw_data = (uint8_t *) fw->data;

	/* Initialise firmware as an in-memory ELF file */
	elfinfo = (struct ELF32_info *)ELF32_initFromMem(fw_data, fw->size, 0);
	if (elfinfo == NULL) {
		dev_err(fdev->dev, "Failed to initialise in-memory ELF file\n");
		result = -ENOMEM;
		goto error_elf_init;
	}

	/* Attempt to load the ELF file */
	result = st_fdma_fw_load(fdev, elfinfo);
	if (result) {
		dev_err(fdev->dev, "Failed to load firmware\n");
		goto error_elf_load;
	}

	/* Retrieve the hardware and firmware versions */
	st_fdma_hw_get_revisions(fdev, &hw_major, &hw_minor, &fw_major,
			&fw_minor);
	dev_notice(fdev->dev, "SLIM hw %d.%d, FDMA fw %d.%d\n",
			hw_major, hw_minor, fw_major, fw_minor);

	/* Indicate firmware loaded */
	fdev->fw_state = ST_FDMA_FW_STATE_LOADED;

	/* Free ELF descriptor */
	ELF32_free(elfinfo);

	/* Wake up the wait queue */
	wake_up(&fdev->fw_load_q);

	/* Release the firmware */
	release_firmware(fw);

	return 0;

error_elf_load:
	ELF32_free(elfinfo);
error_elf_init:
	if (fw_data && fw_data != fw->data)
		devm_kfree(fdev->dev, fw_data);
	release_firmware(fw);
error_no_fw:
	fdev->fw_state = ST_FDMA_FW_STATE_ERROR;
	return result;
}

int st_fdma_fw_check(struct st_fdma_device *fdev)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdev->lock, irqflags);

	switch (fdev->fw_state) {
	case ST_FDMA_FW_STATE_INIT:
		/* Firmware is not loaded, so start the process */
		fdev->fw_state = ST_FDMA_FW_STATE_LOADING;
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return st_fdma_fw_request(fdev);

	case ST_FDMA_FW_STATE_LOADING:
		/* Firmware is loading, so wait until state changes */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		wait_event_interruptible(fdev->fw_load_q,
				fdev->fw_state != ST_FDMA_FW_STATE_LOADING);
		return fdev->fw_state == ST_FDMA_FW_STATE_LOADED ? 0 : -ENODEV;

	case ST_FDMA_FW_STATE_LOADED:
		/* Firmware has loaded */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return 0;

	case ST_FDMA_FW_STATE_ERROR:
		/* Firmware error */
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		return -ENODEV;

	default:
		spin_unlock_irqrestore(&fdev->lock, irqflags);
		dev_err(fdev->dev, "Invalid firmware state: %d\n",
			fdev->fw_state);
		return -ENODEV;
	}

	return 0;
}
