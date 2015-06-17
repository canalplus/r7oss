/***********************************************************************
 *
 * File: HDTVOutFormatter/stmauxtvoutput.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_AUX_TVOUTPUT_H
#define _STM_AUX_TVOUTPUT_H

#include <STMCommon/stmmasteroutput.h>

class CDisplayDevice;
class CDisplayMixer;
class CSTmFSynth;
class CSTmSDVTG;
class CSTmTVOutDENC;
class CSTmHDFormatterAWG;
class CSTmHDFormatterOutput;

class CSTmAuxTVOutput: public CSTmMasterOutput
{
public:
  CSTmAuxTVOutput(CDisplayDevice        *pDev,
                  ULONG                  ulMainTVOut,
                  ULONG                  ulAuxTVOut,
                  ULONG                  ulTVFmt,
                  CSTmSDVTG             *pVTG,
                  CSTmTVOutDENC         *pDENC,
                  CDisplayMixer         *pMixer,
                  CSTmFSynth            *pFSynth,
                  CSTmHDFormatterAWG    *pAWG,
                  CSTmHDFormatterOutput *pHDFOutput);

  bool Start(const stm_mode_line_t*, ULONG tvStandard);

  ULONG GetCapabilities(void) const;

  const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;

  void  Resume(void);

protected:
  ULONG m_ulMainTVOut;
  ULONG m_ulAuxTVOut;
  ULONG m_ulTVFmt;

  ULONG m_ulDENCSyncOffset;

  CSTmHDFormatterAWG    *m_pAWG;
  CSTmHDFormatterOutput *m_pHDFOutput;

  bool m_bHasSeparateCbCrRescale;

  virtual void StartClocks(const stm_mode_line_t*)  = 0;
  virtual void SetAuxClockToHDFormatter(void) = 0;

  bool SetOutputFormat(ULONG);

  void WriteMainTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulMainTVOut + reg)>>2), val); }
  ULONG ReadMainTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulMainTVOut + reg)>>2)); }

  void WriteAuxTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulAuxTVOut + reg)>>2), val); }
  ULONG ReadAuxTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulAuxTVOut + reg)>>2)); }

  void WriteTVFmtReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulTVFmt + reg)>>2), val); }
  ULONG ReadTVFmtReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulTVFmt + reg)>>2)); }

private:
  CSTmAuxTVOutput(const CSTmAuxTVOutput&);
  CSTmAuxTVOutput& operator=(const CSTmAuxTVOutput&);
};


#endif //_STi7111AUXOUTPUT_H
