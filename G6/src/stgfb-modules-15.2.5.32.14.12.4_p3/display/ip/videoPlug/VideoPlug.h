/*******************************************************************************
 *
 * File: display/ip/videoPlug/VideoPlug.h
 * Copyright (c) 2006 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ******************************************************************************/

#ifndef _VIDEOPLUG_H
#define _VIDEOPLUG_H

#include <display/ip/stmviewport.h>
#include <display/generic/DisplayDevice.h>

struct VideoPlugSetup
{
  uint32_t        VIDn_CTL;
  uint32_t        VIDn_VPO;
  uint32_t        VIDn_VPS;
  uint32_t        VIDn_ALP;
  uint32_t        VIDn_KEY1;
  uint32_t        VIDn_KEY2;
  const uint32_t *mpr;
};


class CVideoPlug
{
 public:
  CVideoPlug(CDisplayDevice *pDev, uint32_t plugOffset, bool bEnablePSI, bool bUseMPR);
  virtual ~CVideoPlug(void);

  bool CreatePlugSetup(VideoPlugSetup                  &plugSetup,
                       const stm_display_buffer_t    * const pFrame,
                       const stm_viewport_t          &viewport,
                       stm_pixel_format_t            src_color_fmt,
                       uint32_t                      transparency);

  void WritePlugSetup(const VideoPlugSetup &, bool);

  bool SetControl(stm_display_plane_control_t, uint32_t);
  bool GetControl(stm_display_plane_control_t, uint32_t *) const;

 protected:
  uint32_t *m_plugBaseAddr;

  bool     m_bEnablePSI;
  bool     m_bUseMPR;

  uint32_t m_ulBrightness;
  uint32_t m_ulSaturation;
  uint32_t m_ulContrast;
  uint32_t m_ulTint;

  uint32_t m_VIDn_CSAT;
  uint32_t m_VIDn_TINT;
  uint32_t m_VIDn_BC;

  stm_color_key_config_t m_ColorKeyConfig;

  void updateColorKeyState (stm_color_key_config_t       * const dst,
                            const stm_color_key_config_t * const src) const;

  void  CreatePSISetup(void);

  void  WritePLUGReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_plugBaseAddr, reg, val); }
};

#endif // _VIDEOPLUG_H
