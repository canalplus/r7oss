/***********************************************************************
 *
 * File: Gamma/GenericGammaDevice.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GENERICGAMMA_H
#define _GENERICGAMMA_H

#include <Generic/DisplayDevice.h>

#define CGAMMADEVICE_MAX_PLANES       10
#define CGAMMADEVICE_MAX_ACCELERATORS 4

class CGammaCompositorPlane;
class CGenericGammaCompositor;

class CGenericGammaDevice : public CDisplayDevice
{
public:
  CGenericGammaDevice(void);
  virtual ~CGenericGammaDevice();

  virtual bool Create();

  void* GetCtrlRegisterBase() const { return m_pGammaReg; }

  CDisplayPlane *GetPlane(stm_plane_id_t num) const;
  CGAL          *GetGAL(ULONG id) const;

  void UpdateDisplay(COutput *);
  void UnlockPlanes(COutput *);

protected:
  // This represents each hardware plane on the device
  CGammaCompositorPlane* m_hwPlanes[CGAMMADEVICE_MAX_PLANES];
  unsigned               m_numPlanes;

  CGAL *m_graphicsAccelerators[CGAMMADEVICE_MAX_ACCELERATORS];
  unsigned m_numAccelerators;

  // IO mapped pointer to the start of the Gamma register block
  ULONG* m_pGammaReg;

  void WriteDevReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pGammaReg + (reg>>2), val); }
  ULONG ReadDevReg(ULONG reg) { return g_pIOS->ReadRegister(m_pGammaReg + (reg>>2)); }
};

#endif // _GENERICGAMMA_H
