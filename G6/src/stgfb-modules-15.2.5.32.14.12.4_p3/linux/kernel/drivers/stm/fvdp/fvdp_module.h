/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/fvdp/fvdp_module.h
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_MODULE_H
#define FVDP_MODULE_H

extern const char* fvdp_driver_name;

int  fvdp_platform_device_register(void);
void fvdp_platform_device_unregister(void);

void fvdp_hw_power_up(struct platform_device *pdev);
void fvdp_hw_power_down(struct platform_device *pdev);
int fvdp_clocks_enable(struct platform_device *pdev, bool enable);

int fvdp_clocks_initialize(struct platform_device *pdev);
void fvdp_clocks_uninitialize(struct platform_device *pdev);

int fvdp_configure_pads(struct platform_device *pdev, bool enable);

int fvdp_claim_sysconf(struct platform_device *pdev);
void fvdp_release_sysconf(struct platform_device *pdev);

int fvdp_driver_probe(struct platform_device *pdev);
int fvdp_driver_remove(struct platform_device *pdev);

#endif /* FVDP_MODULE_H */
