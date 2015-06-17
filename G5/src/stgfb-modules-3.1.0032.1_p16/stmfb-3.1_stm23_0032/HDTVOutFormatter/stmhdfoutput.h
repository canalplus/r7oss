/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfoutput.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDFOUTPUT_H
#define _STM_HDFOUTPUT_H

#include <STMCommon/stmmasteroutput.h>

class CSTmSDVTG;
class CSTmHDFormatterAWG;


class CSTmHDFormatterOutput: public CSTmMasterOutput
{
public:
  CSTmHDFormatterOutput(CDisplayDevice *,
                        ULONG ulMainTVOut,
                        ULONG ulAuxTVOut,
                        ULONG ulTVFmt,
                        CSTmSDVTG *,
                        CSTmSDVTG *,
                        CSTmDENC *,
                        CDisplayMixer *,
                        CSTmFSynth *,
                        CSTmHDFormatterAWG *);

  virtual ~CSTmHDFormatterOutput();

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  ULONG GetCapabilities(void) const;

  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;

  void SetSlaveOutputs(COutput *dvo, COutput *hdmi);

  virtual void SetUpsampler(int multiple, ULONG format);

  bool  SetFilterCoefficients(const stm_display_filter_setup_t *);

  void  Resume(void);

protected:
  CSTmSDVTG          *m_pVTG2; // Slave VTG for SD modes on HD display outputs
  COutput            *m_pDVO;
  COutput            *m_pHDMI;
  CSTmHDFormatterAWG *m_pAWGAnalog;

  bool  m_bHasSeparateCbCrRescale;
  bool  m_bDacHdPowerDisabled;
  ULONG m_ulAWGLock;

  ULONG m_ulMainTVOut;
  ULONG m_ulAuxTVOut;
  ULONG m_ulTVFmt;
  bool  m_bUseAlternate2XFilter;
  stm_display_hdf_filter_setup_t m_filters[6];

  bool StartHDDisplay(const stm_mode_line_t*);
  bool StartSDInterlacedDisplay(const stm_mode_line_t*, ULONG tvStandard);
  bool StartSDProgressiveDisplay(const stm_mode_line_t*, ULONG tvStandard);

  bool SetOutputFormat(ULONG);

  virtual void StartHDClocks(const stm_mode_line_t*)            = 0;
  virtual void StartSDInterlacedClocks(const stm_mode_line_t*)  = 0;
  virtual void StartSDProgressiveClocks(const stm_mode_line_t*) = 0;

  virtual void SetMainClockToHDFormatter(void) = 0;

  virtual void EnableDACs(void)  = 0;
  virtual void DisableDACs(void) = 0;

  void WriteAuxTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulAuxTVOut + reg)>>2), val); }
  ULONG ReadAuxTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulAuxTVOut + reg)>>2)); }

  void WriteMainTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulMainTVOut + reg)>>2), val); }
  ULONG ReadMainTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulMainTVOut + reg)>>2)); }

  void WriteTVFmtReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulTVFmt + reg)>>2), val); }
  ULONG ReadTVFmtReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulTVFmt + reg)>>2)); }

private:
  CSTmHDFormatterOutput(const CSTmHDFormatterOutput&);
  CSTmHDFormatterOutput& operator=(const CSTmHDFormatterOutput&);
};


#endif //_STM_HDFOUTPUT_H
