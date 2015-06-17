/***********************************************************************
 *
 * File: soc/sti7200/sti7200remoteoutput.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7200REMOTEOUTPUT_H
#define _STi7200REMOTEOUTPUT_H

#include <STMCommon/stmmasteroutput.h>

class CGammaMixer;
class CSTi7200Cut2RemoteDevice;
class CSTmFSynthType2;
class CSTmSDVTG;
class CSTi7200Cut2RemoteMixer;
class CSTi7200DENC;


class CSTi7200RemoteOutput: public CSTmMasterOutput
{
public:
  CSTi7200RemoteOutput(CSTi7200Device *,
                       ULONG ulTVOutOffset,
                       CSTmSDVTG *,
                       CSTi7200DENC *,
                       CGammaMixer *,
                       CSTmFSynthType2 *);

  virtual ~CSTi7200RemoteOutput();

  ULONG GetCapabilities(void) const;

  const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;

protected:
  ULONG m_ulTVOutOffset;

  bool SetOutputFormat(ULONG);
  void EnableDACs(void);
  void DisableDACs(void);

  void WriteTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((m_ulTVOutOffset + reg)>>2), val); }
  ULONG ReadTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((m_ulTVOutOffset + reg)>>2)); }

  void WriteSysCfgReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7200_SYSCFG_BASE + reg)>>2), val); }
  ULONG ReadSysCfgReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7200_SYSCFG_BASE +reg)>>2)); }

  void WriteClkReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + ((STi7200_CLKGEN_BASE + reg)>>2), val); }
  ULONG ReadClkReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + ((STi7200_CLKGEN_BASE +reg)>>2)); }


private:
  CSTi7200RemoteOutput(const CSTi7200RemoteOutput&);
  CSTi7200RemoteOutput& operator=(const CSTi7200RemoteOutput&);
};


class CSTi7200Cut2RemoteOutput: public CSTi7200RemoteOutput
{
public:
  CSTi7200Cut2RemoteOutput(CSTi7200Cut2RemoteDevice *,
                           CSTmSDVTG *,
                           CSTi7200DENC *,
                           CSTi7200Cut2RemoteMixer *,
                           CSTmFSynthType2 *);

  virtual ~CSTi7200Cut2RemoteOutput();

private:
  CSTi7200Cut2RemoteOutput(const CSTi7200Cut2RemoteOutput&);
  CSTi7200Cut2RemoteOutput& operator=(const CSTi7200Cut2RemoteOutput&);
};

#endif //_STI7200REMOTEOUTPUT_H
