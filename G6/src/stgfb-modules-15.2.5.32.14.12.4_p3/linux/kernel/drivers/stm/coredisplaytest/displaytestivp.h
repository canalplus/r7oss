/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestivp.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Header file name : displaytestivp.h
 *
 * Access to all platform specific display test information etc
 *
 ************************************************************************/
/******************************************************************************************
 * The kernel thread which exports source CRC from CoreDipslay Test module to User space
 ******************************************************************************************/
#ifndef _DISPLAYTESTIVP_H_
#define _DISPLAYTESTIVP_H_

struct ivpDisplayTest* __init create_ivp(struct stmcore_display_pipeline_data *pPlatformData, uint32_t sourceNo, uint32_t pipelineNo);
void ivp_exit(struct ivpDisplayTest *pIvpTest);
void get_ivp_capability(void);

#endif

