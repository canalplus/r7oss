/***********************************************************************
 *
 * File: Gamma/GammaCompositorCursor.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_COMPOSITOR_CURSOR_H
#define _GAMMA_COMPOSITOR_CURSOR_H

#include "GammaCompositorPlane.h"


struct GammaCursorSetup
{
  ULONG CTL;
  ULONG VPO;
  ULONG PML;
  ULONG PMP;
  ULONG SIZE;
  ULONG CML;
};


class CGammaCompositorCursor: public CGammaCompositorPlane
{
  public:

    CGammaCompositorCursor(ULONG baseAddr);
    ~CGammaCompositorCursor();

    virtual bool QueueBuffer(const stm_display_buffer_t * const pBuffer,
                             const void                 * const user);

    virtual void UpdateHW (bool isDisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const TIME64 &vsyncTime);

    virtual stm_plane_caps_t GetCapabilities(void) const;

    virtual bool SetControl(stm_plane_ctrl_t control, ULONG value);
    virtual bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

  protected:
    void writeSetup(const GammaCursorSetup *setup);

    void EnableHW (void);

    ULONG m_CursorBaseAddr;
    short m_ScreenX;
    short m_ScreenY;
    ULONG m_GlobalVPO;

    void  WriteCurReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister((volatile ULONG*)m_CursorBaseAddr + (reg>>2), val); }
    ULONG ReadCurReg (ULONG reg)            { return g_pIOS->ReadRegister((volatile ULONG*)m_CursorBaseAddr + (reg>>2)); }

  private:
    CGammaCompositorCursor(const CGammaCompositorCursor&);
    CGammaCompositorCursor& operator=(const CGammaCompositorCursor&);
};


#endif // _GAMMA_COMPOSITOR_CURSOR_H
