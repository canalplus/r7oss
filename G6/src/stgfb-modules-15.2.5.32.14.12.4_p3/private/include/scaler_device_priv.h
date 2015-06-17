/***********************************************************************
 *
 * File: private/include/scaler_device_priv.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALER_DEVICE_PRIV_H
#define SCALER_DEVICE_PRIV_H

#if defined(__cplusplus)
extern "C" {
#endif

extern int thread_vib_scaler[];

extern int  stm_scaler_device_open(void);
extern void stm_scaler_device_close(void);

extern int stm_scaler_device_open_scaler(uint32_t scalerId, stm_scaler_h *scaler_handle, const stm_scaler_config_t *scaler_config);
extern int stm_scaler_device_close_scaler(stm_scaler_h scaler_handle);

#if defined(__cplusplus)
}
#endif

#endif
