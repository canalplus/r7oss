/***********************************************************************
 *
 * File: display/ip/tvout/stmmaintvoutput.h
 * Copyright (c) 2008-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_MAINTVOUTPUT_H
#define _STM_MAINTVOUTPUT_H

#include <display/ip/stmmasteroutput.h>

class CSTmVTG;
class CSTmHDFormatter;


class CSTmMainTVOutput: public CSTmMasterOutput
{
public:
  CSTmMainTVOutput(const char *name,
                   uint32_t id,
                   CDisplayDevice *,
                   CSTmVTG *,
                   CSTmDENC *,
                   CDisplayMixer *,
                   CSTmHDFormatter *);

  virtual ~CSTmMainTVOutput();

  OutputResults Start(const stm_display_mode_t*);
  bool Stop(void);

  void Resume(void);

  const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;

  bool     SetFilterCoefficients(const stm_display_filter_setup_t *);
  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal);
  uint32_t GetCompoundControl(stm_output_control_t ctrl, void *val) const;

protected:
  CSTmHDFormatter         *m_pHDFormatter;

  bool     m_bDacHdPowerDisabled;
  uint32_t m_uExternalSyncShift;
  bool     m_bInvertExternalSyncs;
  uint32_t m_uDENCSyncOffset;

  virtual bool SetOutputFormat(uint32_t format);

  virtual bool SetVTGSyncs(const stm_display_mode_t *mode) = 0;
  virtual bool ConfigureOutput(uint32_t format) = 0;
  virtual void SetMainClockToHDFormatter(void) = 0;
  virtual void ConfigureDisplayClocks(const stm_display_mode_t *mode) = 0;
  virtual void DisableClocks(void) { }

  virtual void EnableDACs(void)  = 0;
  virtual void DisableDACs(void) = 0;
  virtual void PowerDownHDDACs(void) = 0;
  virtual void PowerDownDACs(void) = 0;

private:
  bool RestartDENC(uint32_t);

  CSTmMainTVOutput(const CSTmMainTVOutput&);
  CSTmMainTVOutput& operator=(const CSTmMainTVOutput&);
};


#endif //_STM_MAINTVOUTPUT_H
