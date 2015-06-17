/*******************************************************************************
 *
 * File: Gamma/GammaVideoPlug.h
 * Copyright (c) 2006 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ******************************************************************************/

#ifndef _GAMMA_VIDEO_PLUG_H
#define _GAMMA_VIDEO_PLUG_H

struct VideoPlugSetup
{
    ULONG        VIDn_CTL;
    ULONG        VIDn_VPO;
    ULONG        VIDn_VPS;
    ULONG        VIDn_ALP;
    ULONG        VIDn_KEY1;
    ULONG        VIDn_KEY2;
    const ULONG *mpr;
};


class CGammaVideoPlug
{
  public:
    CGammaVideoPlug(ULONG baseAddr, bool bEnablePSI, bool bUseMPR);
    virtual ~CGammaVideoPlug(void);

    bool CreatePlugSetup(VideoPlugSetup                &plugSetup,
                         const stm_display_buffer_t    * const pFrame,
                         const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    void WritePlugSetup(const VideoPlugSetup &);

    bool SetControl(stm_plane_ctrl_t, ULONG);
    bool GetControl(stm_plane_ctrl_t, ULONG *) const;

    void Hide(void) { m_bVisible = false; }
    void Show(void) { m_bVisible = true; }

  protected:
    ULONG m_plugBaseAddr;
    bool  m_bEnablePSI;
    bool  m_bUseMPR;
    bool  m_bVisible;

    ULONG m_ulBrightness;
    ULONG m_ulSaturation;
    ULONG m_ulContrast;
    ULONG m_ulTint;

    ULONG m_VIDn_CSAT;
    ULONG m_VIDn_TINT;
    ULONG m_VIDn_BC;

    stm_color_key_config_t m_ColorKeyConfig;

    void updateColorKeyState (stm_color_key_config_t       * const dst,
			      const stm_color_key_config_t * const src) const;

    void  CreatePSISetup(void);

    void  WritePLUGReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister((volatile ULONG*)m_plugBaseAddr + (reg>>2), val); }
};

#endif // _GAMMA_VIDEO_PLUG_H
