/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfawg.h
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDFAWG_H
#define _STMHDFAWG_H


#include <STMCommon/stmawg.h>

/*
 * AWG based analogue sync generation in the HDFormatter block common to
 * 7200Cut1/2, 7111, 7141 ...
 */
class CSTmHDFormatterAWG: public CAWG
{
public:
  CSTmHDFormatterAWG (CDisplayDevice* pDev, ULONG hdf_offset, ULONG ram_size);
  virtual ~CSTmHDFormatterAWG (void);

  virtual void Enable  (void);
  virtual void Disable (void);

private:
  ULONG *m_pHDFAWG;

  virtual bool Enable  (stm_awg_mode_t mode);

  void  WriteReg (ULONG reg,
                  ULONG value)  { DEBUGF2 (4, ("w %p <- %.8lx\n",m_pHDFAWG + (reg>>2),value));
                                  g_pIOS->WriteRegister (m_pHDFAWG + (reg>>2), value); }

  ULONG ReadReg  (ULONG reg)    { DEBUGF2 (4, ("r %p -> %.8lx\n", m_pHDFAWG + (reg>>2),
                                                                  g_pIOS->ReadRegister (m_pHDFAWG + (reg>>2))));
                                  return g_pIOS->ReadRegister (m_pHDFAWG + (reg>>2)); }

};

#endif /* _STMHDFAWG_H */
