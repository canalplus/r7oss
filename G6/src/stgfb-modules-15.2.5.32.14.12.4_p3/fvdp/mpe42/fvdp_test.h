/***********************************************************************
 *
 * File: fvdp_test.h
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#if FVDP_IOCTL

/* Define to prevent recursive inclusion */
#ifndef __FVDP_TEST_H
#define __FVDP_TEST_H

/* FVDP Test API Prototype */
FVDP_Result_t FVDP_ReadRegister(uint32_t addr, uint32_t* value);
FVDP_Result_t FVDP_WriteRegister(uint32_t addr, uint32_t value);
FVDP_Result_t FVDP_GetBufferCallback(uint32_t* AddrScalingComplete, uint32_t* AddrBufferDone);

#endif /* #ifndef __FVDP_TEST_H */

#endif // #if FVDP_IOCTL
