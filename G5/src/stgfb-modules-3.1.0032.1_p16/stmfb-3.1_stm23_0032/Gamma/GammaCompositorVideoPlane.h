/***********************************************************************
 *
 * File: Gamma/GammaCompositorVideoPlane.h
 * Copyright (c) 2006 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _GAMMA_COMPOSITOR_VIDEO_PLANE_H
#define _GAMMA_COMPOSITOR_VIDEO_PLANE_H

#include "GammaCompositorPlane.h"
#include "GammaVideoPlug.h"


struct HVSRCState
{
    ULONG LumaHInc;
    ULONG LumaVInc;
    LONG  LumaHphase;
    LONG  LumaVphase;
    LONG  ChromaHphase;
    LONG  ChromaVphase;
    ULONG VC1flag;
    ULONG VC1LumaType;
    ULONG VC1ChromaType;
};


class CGammaCompositorVideoPlane: public CGammaCompositorPlane
{
public:
    CGammaCompositorVideoPlane(stm_plane_id_t GDPid, CGammaVideoPlug *, ULONG baseAddress);
    ~CGammaCompositorVideoPlane();

    virtual bool Create(void);

    virtual bool SetControl(stm_plane_ctrl_t control, ULONG value);
    virtual bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

protected:
    CGammaVideoPlug *m_videoPlug;

    void  WriteVideoReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister((volatile ULONG*)m_baseAddress + (reg>>2), val); }
    ULONG ReadVideoReg (ULONG reg)            { return g_pIOS->ReadRegister((volatile ULONG*)m_baseAddress + (reg>>2)); }

    static const int HFILTER_ENTRIES;
    static const int HFILTER_SIZE;
    static const int VFILTER_ENTRIES;
    static const int VFILTER_SIZE;

    DMA_Area m_HFilter;
    DMA_Area m_VFilter;

    HVSRCState m_HVSRCState;

    ULONG m_baseAddress;

    void ResetQueueBufferState(void);

};

#endif // _GAMMA_COMPOSITOR_VIDEO_PLANE_H
