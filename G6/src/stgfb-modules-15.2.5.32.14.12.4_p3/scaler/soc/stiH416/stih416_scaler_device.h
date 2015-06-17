/***********************************************************************
 *
 * File: scaler/soc/stiH416/stih416_scaler_device.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STIH416SCALERDEVICE_H
#define STIH416SCALERDEVICE_H

#ifdef __cplusplus

#include "scaler_device.h"

#define STiH416_SCALER_COUNT 1

class CStiH416ScalerDevice : public CScalerDevice
{
public:
  CStiH416ScalerDevice();
  virtual ~CStiH416ScalerDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  bool Create();

protected:
  bool CreateScalers(void);

}; /* CStiH416ScalerDevice */

#endif /* __cplusplus */

#endif /* STIH416SCALERDEVICE_H */
