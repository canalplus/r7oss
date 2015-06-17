/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestovp.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Header file name : displaytestovp.h
 *
 * Access to all platform specific display test information etc
 *
 ************************************************************************/
/******************************************************************************************
 * Exports CRC/MISR from CoreDipslay Test module to User space
 ******************************************************************************************/
#ifndef _DISPLAYTESTOVP_H_
#define _DISPLAYTESTOVP_H_

struct ovpDisplayTest * __init create_ovp(struct stmcore_display_pipeline_data *pPlatformData, uint32_t pipelineNo);
void ovp_exit(struct ovpDisplayTest *pOvpTest);
void get_ovp_capability(void);

#endif
