/***********************************************************************
 *
 * File: soc/sti5206/sti5206device.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI5206DEVICE_H
#define _STI5206DEVICE_H

#ifdef __cplusplus

#include <Gamma/GenericGammaDevice.h>


class CSTmSDVTG;
class CSTmFSynthType1;
class CSTmTVOutDENC;
class CSTi5206MainMixer;
class CSTi5206AuxMixer;
class CSTi5206BDisp;
class CGammaVideoPlug;
class CSTmHDFormatterAWG;
class CSTmTVOutTeletext;
class CSTi5206ClkDivider;

class CSTi5206Device : public CGenericGammaDevice
{
public:
  CSTi5206Device(void);
  virtual ~CSTi5206Device(void);

  bool Create(void);

protected:
  CSTmFSynthType1        *m_pFSynthHD;
  CSTmFSynthType1        *m_pFSynthSD;
  CSTmSDVTG              *m_pVTG1;
  CSTmSDVTG              *m_pVTG2;
  CSTmTVOutDENC          *m_pDENC;
  CSTi5206MainMixer      *m_pMainMixer;
  CSTi5206AuxMixer       *m_pAuxMixer;
  CGammaVideoPlug        *m_pVideoPlug1;
  CGammaVideoPlug        *m_pVideoPlug2;
  CSTmHDFormatterAWG     *m_pAWGAnalog;
  CSTi5206BDisp          *m_pBDisp;
  CSTmTVOutTeletext      *m_pTeletext;
  CSTi5206ClkDivider     *m_pClkDivider;

  virtual bool CreateInfrastructure(void);
  virtual bool CreatePlanes(void);
  virtual bool CreateOutputs(void);
  virtual bool CreateGraphics(void);

private:
  CSTi5206Device(const CSTi5206Device&);
  CSTi5206Device& operator=(const CSTi5206Device&);
};

#endif /* __cplusplus */


#define STi5206_OUTPUT_IDX_VDP0_MAIN      ( 0)
#define STi5206_OUTPUT_IDX_VDP0_HDMI      (-1)
#define STi5206_OUTPUT_IDX_DVO0           ( 2)
#define STi5206_OUTPUT_IDX_VDP1_MAIN      ( 1)

#define STi5206_BLITTER_IDX_KERNEL        ( 0)
#define STi5206_BLITTER_IDX_VDP0_MAIN     ( 1)
#define STi5206_BLITTER_IDX_VDP1_MAIN     ( 2)

#define STi5206_BLITTER_AQ_KERNEL         ( 1)
#define STi5206_BLITTER_AQ_VDP0_MAIN      ( 2)
#define STi5206_BLITTER_AQ_VDP1_MAIN      ( 3)


#endif // _STI5206DEVICE_H
