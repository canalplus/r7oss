/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2localdevice.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200CUT2LOCALDEVICE_H
#define _STI7200CUT2LOCALDEVICE_H

#include "sti7200device.h"

class CSTmSDVTG;
class CSTmFSynthType2;
class CSTmTVOutDENC;
class CSTi7200LocalMainMixer;
class CSTi7200LocalAuxMixer;
class CGammaVideoPlug;
class CSTmHDFormatterAWG;
class CGDPBDispOutput;
class CSTmTVOutTeletext;

class CSTi7200Cut2LocalDevice : public CSTi7200Device
{
public:
  CSTi7200Cut2LocalDevice(void);
  virtual ~CSTi7200Cut2LocalDevice(void);

  bool Create(void);

  CDisplayPlane *GetPlane(stm_plane_id_t planeID) const;

  void UpdateDisplay(COutput *);

private:
  CSTmFSynthType2        *m_pFSynthHD0;
  CSTmFSynthType2        *m_pFSynthSD;
  CSTmFSynthType2        *m_pFSynth_IC_177;
  CSTmFSynthType2        *m_pFSynth_FVP_166;
  CSTmFSynthType2        *m_pFSynth_PIPE;
  CSTmSDVTG              *m_pVTG1;
  CSTmSDVTG              *m_pVTG2;
  CSTmTVOutDENC          *m_pDENC;
  CSTi7200LocalMainMixer *m_pMainMixer;
  CSTi7200LocalAuxMixer  *m_pAuxMixer;
  CGammaVideoPlug        *m_pVideoPlug1;
  CGammaVideoPlug        *m_pVideoPlug2;
  CSTmHDFormatterAWG     *m_pAWGAnalog;
  CGDPBDispOutput        *m_pGDPBDispOutput;
  CSTmTVOutTeletext      *m_pTeletext;

  CSTi7200Cut2LocalDevice(const CSTi7200Cut2LocalDevice&);
  CSTi7200Cut2LocalDevice& operator=(const CSTi7200Cut2LocalDevice&);
};

#endif // _STI7200CUT1LOCALDEVICE_H
