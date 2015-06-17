/***********************************************************************
 *
 * File: soc/stb7100/stb7100AWGAnalog.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STB7100AWGANALOG_H
#define _STB7100AWGANALOG_H


#include <STMCommon/stmawg.h>


class CSTb7100AWGAnalog: public CAWG
{
public:
  CSTb7100AWGAnalog (CDisplayDevice* pDev);
  virtual ~CSTb7100AWGAnalog (void);

  virtual void Enable  (void);
  virtual void Disable (void);

private:
  ULONG *m_pAWGReg;

  virtual bool Enable  (stm_awg_mode_t mode);

  void  WriteReg (ULONG reg, ULONG value) { g_pIOS->WriteRegister(m_pAWGReg + (reg>>2), value); }
  ULONG ReadReg  (ULONG reg) { return g_pIOS->ReadRegister(m_pAWGReg + (reg>>2)); }
};

#endif /* _STB7100AWGANALOG_H */
