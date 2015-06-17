/***********************************************************************
 *
 * File: scaler/generic/scaler_device.h
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef SCALERDEVICE_H
#define SCALERDEVICE_H

#ifdef __cplusplus

#include <stm_scaler.h>
#include "scaler.h"

class CScaler;
class CScalerDevice;

/*
 * Scaler registry types.
 */
extern const char  *stm_scaler_tag;
extern unsigned     stm_scaler_type;


struct stm_scaler_device_s
{
  CScalerDevice * handle;
  void          * lock;
};


class CScalerDevice
{
public:
  CScalerDevice(int32_t nScalers);
  virtual ~CScalerDevice();

  CScaler  *GetScaler(uint32_t index) const { return (index<m_numScalers)?m_pScalers[index]:0; }

  // Create Device specific resources after initial mappings have
  // been completed.
  virtual bool  Create();

protected:
  void RegistryInit(void);
  void RegistryTerm(void);

  bool AddScaler(CScaler *pScaler);
  void RemoveScalers(void);

  uint32_t     m_numScalers;
  CScaler    **m_pScalers;
}; /* CScalerDevice */

/*
 * There will be one device specific implementation of this function in each
 * valid driver configuration. This is how you get hold of the device object
 */
extern CScalerDevice* AnonymousCreateScalerDevice(void);


extern "C" {
    extern bool stm_scaler_handle_valid(const stm_scaler_h scaler_handle);
}

#endif /* __cplusplus */

#endif /* SCALERDEVICE_H */
