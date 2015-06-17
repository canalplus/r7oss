/***********************************************************************
 *
 * File: scaler/soc/stiH407/stih407_scaler_device.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STIH407SCALERDEVICE_H
#define STIH407SCALERDEVICE_H

#ifdef __cplusplus

#include "scaler_device.h"

#define STiH407_SCALER_COUNT 1
#define STiH407_BLITTER_USED_FOR_SCALING 0

class CStiH407ScalerDevice : public CScalerDevice
{
public:
  CStiH407ScalerDevice();
  virtual ~CStiH407ScalerDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  bool Create();

protected:
  bool CreateScalers(void);

}; /* CStiH407ScalerDevice */

#endif /* __cplusplus */

#endif /* STIH407SCALERDEVICE_H */
