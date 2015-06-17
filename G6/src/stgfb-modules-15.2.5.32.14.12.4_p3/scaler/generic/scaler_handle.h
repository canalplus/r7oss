/***********************************************************************
 *
 * File: scaler/generic/scaler_handle.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALER_HANDLE_H
#define SCALER_HANDLE_H

#if defined(__cplusplus)
extern "C" {
#endif

struct stm_scaler_s
{
    void               * lock;             // API semaphore
    CScalingCtxNode      scaling_ctx_node; // scaling context
};

#if defined(__cplusplus)
}
#endif

#endif
