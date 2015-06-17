/***********************************************************************
 *
 * File: display/ip/tvout/stmauxtvoutput.h
 * Copyright (c) 2009-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_AUX_TVOUTPUT_H
#define _STM_AUX_TVOUTPUT_H

#include <display/ip/stmmasteroutput.h>

class CDisplayDevice;
class CDisplayMixer;
class CSTmHDFormatter;
class CSTmVTG;
class CSTmDENC;

class CSTmAuxTVOutput: public CSTmMasterOutput
{
public:
  CSTmAuxTVOutput(const char            *name,
                  uint32_t               id,
                  CDisplayDevice        *pDev,
                  CSTmVTG               *pVTG,
                  CSTmDENC              *pDENC,
                  CDisplayMixer         *pMixer,
                  CSTmHDFormatter *m_pHDFormatter);

  virtual ~CSTmAuxTVOutput();

  OutputResults Start(const stm_display_mode_t*);
  bool Stop(void);

  void  Resume(void);

  virtual bool SetFilterCoefficients(const stm_display_filter_setup_t *);

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal);
  uint32_t GetCompoundControl(stm_output_control_t ctrl, void *val) const;

  virtual const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;


protected:
  CSTmHDFormatter *m_pHDFormatter;

  uint32_t m_uExternalSyncShift;
  bool     m_bInvertExternalSyncs;
  uint32_t m_uDENCSyncOffset;

  virtual bool SetOutputFormat(uint32_t);

  virtual bool SetVTGSyncs(const stm_display_mode_t *mode) = 0;
  virtual bool ConfigureOutput(uint32_t format) = 0;
  virtual void SetAuxClockToHDFormatter(void) = 0;
  virtual void ConfigureDisplayClocks(const stm_display_mode_t *mode) = 0;
  virtual void DisableClocks(void) { }

  virtual void PowerDownDACs(void) = 0;

private:
  bool RestartDENC(uint32_t);

  CSTmAuxTVOutput(const CSTmAuxTVOutput&);
  CSTmAuxTVOutput& operator=(const CSTmAuxTVOutput&);
};


#endif //_STM_AUX_TVOUTPUT_H
