/***********************************************************************
 *
 * File: STMCommon/stmfdvo.h
 * Copyright (c) 2008-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFDVO_H
#define _STMFDVO_H

#include <Generic/Output.h>

#include "stmawg.h"

class CDisplayDevice;

class CSTmFDVOAWG: public CAWG
{
public:
  CSTmFDVOAWG (CDisplayDevice* pDev, ULONG ulRegOffset);
  virtual ~CSTmFDVOAWG (void);

  virtual void Enable  (void);
  virtual void Disable (void);

private:
  ULONG *m_pFDVOAWG;

  virtual bool Enable  (stm_awg_mode_t mode);

  void  WriteReg (ULONG reg, ULONG value)  { g_pIOS->WriteRegister (m_pFDVOAWG + (reg>>2), value); }
  ULONG ReadReg  (ULONG reg)               { return g_pIOS->ReadRegister (m_pFDVOAWG + (reg>>2)); }

  CSTmFDVOAWG(const CSTmFDVOAWG&);
  CSTmFDVOAWG& operator=(const CSTmFDVOAWG&);
};


class CSTmFDVO: public COutput
{
public:
  CSTmFDVO(CDisplayDevice *, ULONG ulFDVOOffset, COutput *);
  virtual ~CSTmFDVO();

  ULONG GetCapabilities(void) const;

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  bool CanShowPlane(stm_plane_id_t planeID);
  bool ShowPlane   (stm_plane_id_t planeID);
  void HidePlane   (stm_plane_id_t planeID);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

  bool HandleInterrupts(void);

protected:
  ULONG *m_pFDVORegs;

  COutput *m_pMasterOutput;
  CSTmFDVOAWG m_AWG;

  bool   m_bEnable422ChromaFilter;
  bool   m_bInvertDataClock;

  virtual bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

  void WriteFDVOReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pFDVORegs + (reg>>2), val); }
  ULONG ReadFDVOReg(ULONG reg)            { return g_pIOS->ReadRegister(m_pFDVORegs + (reg>>2)); }

private:
  CSTmFDVO(const CSTmFDVO&);
  CSTmFDVO& operator=(const CSTmFDVO&);
};


#endif /* _STMFDVO_H */
