/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407hdmi.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407HDMI_H
#define _STIH407HDMI_H

#include <display/ip/hdmi/stmhdmi.h>

class COutput;
class CSTmClockLLA;
class CSTmVIP;
class CSTmVTG;

class CSTiH407HDMI: public CSTmHDMI
{
public:
  CSTiH407HDMI(CDisplayDevice      *pDev,
               COutput             *pMainOutput,
               CSTmVTG             *pVTG,
               CSTmVIP             *pHDMIVIP,
               CSTmClockLLA        *pClkDivider,
       const stm_display_sync_id_t *pSyncMap);

  virtual ~CSTiH407HDMI(void);

  bool Create(void);

  uint32_t SetControl(stm_output_control_t ctrl, uint32_t newVal);
  bool SetVTGSyncs(const stm_display_mode_t *);

private:
  uint32_t m_uAudioClkOffset;
  uint32_t m_uUniplayerHDMIOffset;

  CSTmClockLLA        *m_pClkDivider;
  CSTmVIP             *m_pVIP;
  CSTmVTG             *m_pVTG;

  const stm_display_sync_id_t *m_pTVOSyncMap;

  bool SetOutputFormat(uint32_t format);
  void SetSignalRangeClipping(void);
  bool SetAudioSource(stm_display_output_audio_source_t source);

  int  GetAudioFrequency(void);
  void GetAudioHWState(stm_hdmi_audio_state_t *);

  void DisableClocks(void);
  bool PreConfiguration(const stm_display_mode_t*);
  bool PostConfiguration(const stm_display_mode_t*);

  void WriteUniplayerReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uUniplayerHDMIOffset+reg), val); }
  uint32_t ReadUniplayerReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uUniplayerHDMIOffset+reg)); }

  void WriteAudioClkReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uAudioClkOffset+reg), val); }
  uint32_t ReadAudioClkReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uAudioClkOffset+reg)); }

  CSTiH407HDMI(const CSTiH407HDMI&);
  CSTiH407HDMI& operator=(const CSTiH407HDMI&);

};

#endif //_STIH407HDMI_H
