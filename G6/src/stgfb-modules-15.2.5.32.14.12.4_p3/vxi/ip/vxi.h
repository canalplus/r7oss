/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#ifndef VXI_H
#define VXI_H

#include <vibe_os.h>
#include <vibe_debug.h>
#include "linux/kernel/drivers/stm/vxi/platform.h"
#include "stm_vxi.h"

class CVxiDevice;
class CVxi;

class CVxi
{
public:
    CVxi(const char *name, uint32_t id, const CVxiDevice *pDev, stm_vxi_platform_data_t *data);
    virtual ~CVxi();

    const char           *GetName(void)    const { return m_name; }
    uint32_t              GetID(void)      const { return m_ID; }

    bool                  Create(void);
    int                   GetCapability(stm_vxi_capability_t *capability);
    int                   GetInputCapability(stm_vxi_input_capability_t *inputCapability);
    virtual int           SetControl(stm_vxi_ctrl_t ctrl, uint32_t value);
    virtual int           GetControl(stm_vxi_ctrl_t ctrl, uint32_t *value);
    int                   Freeze();
    int                   Suspend();
    int                   Resume();

protected:
  const char                * m_name;
  uint32_t                    m_ID;
  const CVxiDevice          * m_pVxiDevice;
  void                      * m_lock;
  uint32_t                  * m_pDevRegs;
  stm_vxi_capability_t        m_ulCapabilities;
  stm_vxi_input_capability_t  m_inputCapabilities[3];
  stm_vxi_platform_data_t   * m_platformData;
  uint32_t                    m_uCurrentInterface;
  uint32_t                    m_uCurrentInput;
  bool                        m_bIsSuspended;
  bool                        m_bIsFreezed;

  /* VXI Read/Write methodes */
  uint32_t ReadVXIReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (reg)); }
  void WriteVXIReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (reg), val); }
private:


}; /* CVxi */

#endif /* VXI_H */
