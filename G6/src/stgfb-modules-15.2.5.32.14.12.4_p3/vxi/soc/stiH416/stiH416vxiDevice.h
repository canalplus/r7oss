/***********************************************************************
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH416VXIDEVICE_H
#define _STIH416VXIDEVICE_H

#include "linux/kernel/drivers/stm/vxi/platform.h"
#include <vxi/generic/VxiDevice.h>

class CSTiH416vxiDevice : public CVxiDevice
{
public:
  CSTiH416vxiDevice(stm_vxi_platform_data_t *data);
  virtual ~CSTiH416vxiDevice(void);

  bool Create(void);

protected:
  bool CreateVxis(void);
  stm_vxi_platform_data_t m_platformData;

  CSTiH416vxiDevice(const CSTiH416vxiDevice&);
  CSTiH416vxiDevice& operator=(const CSTiH416vxiDevice&);
};

#define STiH416_VXI_INSTANCES_COUNT      (1)

#endif // _STIH416VXIDEVICE_H
