/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2remotedevice.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200CUT2REMOTEDEVICE_H
#define _STI7200CUT2REMOTEDEVICE_H

#include "sti7200device.h"

class CSTmSDVTG;
class CSTmFSynthType2;
class CSTi7200DENC;
class CSTi7200Cut2RemoteMixer;
class CGammaVideoPlug;
class CSTmTVOutTeletext;

class CSTi7200Cut2RemoteDevice : public CSTi7200Device
{
public:
  CSTi7200Cut2RemoteDevice(void);
  virtual ~CSTi7200Cut2RemoteDevice(void);

  bool Create(void);

private:
  CSTmFSynthType2         *m_pFSynth;
  CSTmSDVTG               *m_pVTG;
  CSTi7200DENC            *m_pDENC;
  CSTi7200Cut2RemoteMixer *m_pMixer;
  CGammaVideoPlug         *m_pVideoPlug;
  CSTmTVOutTeletext       *m_pTeletext;

  CSTi7200Cut2RemoteDevice(const CSTi7200Cut2RemoteDevice&);
  CSTi7200Cut2RemoteDevice& operator=(const CSTi7200Cut2RemoteDevice&);

};

#endif // _STI7200CUT2REMOTEDEVICE_H
