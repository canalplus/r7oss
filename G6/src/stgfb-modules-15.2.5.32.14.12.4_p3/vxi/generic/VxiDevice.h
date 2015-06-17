/***********************************************************************
 *
 * File: vxi/generic/VXIDevice.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _VXI_DEVICE_H__
#define _VXI_DEVICE_H__

#include <vibe_os.h>
#include <vibe_debug.h>
#include "linux/kernel/drivers/stm/vxi/platform.h"

#define VALID_DEVICE_HANDLE       0x01230123

class CVxi;

struct stm_vxi_s
{
  uint32_t  handle;
  void     * lock;
  uint32_t  magic;
};

class CVxiDevice
{
public:
  CVxiDevice(uint32_t nVxis);
  virtual ~CVxiDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  virtual bool  Create();

  // Access to Vxi instances
  uint32_t  GetNumberOfVxis(void)     const { return m_numVxis; }
  CVxi     *GetVxi(uint32_t index)    const { return (index<m_numVxis)?m_pVxis[index]:0; }
  // Control registers information
  void* GetCtrlRegisterBase()         const { return m_pReg; }

protected:
  void RegistryInit(void);
  void RegistryTerm(void);

  bool AddVxi(CVxi *pVxi);
  void RemoveVxis(void);

  uint32_t     m_numVxis;  // Number of Vxis
  CVxi       **m_pVxis;    // Available Vxis objects

  // IO mapped pointer to the start of the register block
  uint32_t* m_pReg;

}; /* CVxiDevice */

/*
 * There will be one device specific implementation of this function in each
 * valid driver configuration. This is how you get hold of the device object
 */
extern CVxiDevice* AnonymousCreateVxiDevice(stm_vxi_platform_data_t *data);

extern "C" {
  extern int stm_vxi_device_open(const uint32_t id, stm_vxi_h *vxiHandle);
  extern int stm_vxi_device_close(const stm_vxi_h vxiHandle);
  extern bool stm_vxi_is_handle_valid(const stm_vxi_h vxiHandle);
}
#endif /* _VXI_DEVICE_H__ */
