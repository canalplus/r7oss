/***********************************************************************
 *
 * File: soc/stb7100/stb7100device.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7100DEVICE_H
#define STb7100DEVICE_H

#include <Gamma/GenericGammaDevice.h>

class CGAL;
class CGammaMixer;
class CSTb7100VTG1;
class CSTb7100VTG2;
class CSTb7100DENC;
class CSTmFSynthType1;
class CGammaVideoPlug;
class CSTb7100AWGAnalog;
class CGDPBDispOutput;
class CSTb7109BDisp;

class CSTb7100Device: public CGenericGammaDevice  
{
public:
  CSTb7100Device(void);
  ~CSTb7100Device(void);

  bool Create(void);

  CDisplayPlane *GetPlane(stm_plane_id_t planeID) const;

  void UpdateDisplay(COutput *);

private:  
  CSTb7100VTG1      *m_pVTG1;     // HD/Main Video path VTG
  CSTb7100VTG2      *m_pVTG2;     // SD/Aux  Video path VTG
  CSTb7100DENC      *m_pDENC;     // SD Analogue output
  CGammaMixer       *m_pMixer1;   // HD/Main Video path Mixer
  CGammaMixer       *m_pMixer2;   // SD/Aux  Video path Mixer
  CSTmFSynthType1   *m_pHDFSynth; // Frequency Synthesizer for the Main HD display
  CSTmFSynthType1   *m_pSDFSynth; // Frequency Synthesizer for the Aux SD display and broadcast clock recovery
  CGammaVideoPlug   *m_pVideoPlug1;
  CGammaVideoPlug   *m_pVideoPlug2;
  CSTb7100AWGAnalog *m_pAWGAnalog;
  CGDPBDispOutput   *m_pGDPBDispOutput;
  CSTb7109BDisp     *m_pBDisp;

  bool m_bIs7109;
  int  m_chipVersion;

  bool CreatePlanes();
  bool Create7100Planes();
  bool Create7109Cut2Planes();
  bool Create7109Cut3Planes();
  bool CreateOutputs(stm_plane_id_t sharedPlane);
  bool CreateGraphics();

  void WriteDevReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pGammaReg + (reg>>2), val); }
  ULONG ReadDevReg(ULONG reg) const { return g_pIOS->ReadRegister(m_pGammaReg + (reg>>2)); }
  
  CSTb7100Device(const CSTb7100Device&);
  CSTb7100Device& operator=(const CSTb7100Device&);
};

#endif // _STb7100DEVICE_H
