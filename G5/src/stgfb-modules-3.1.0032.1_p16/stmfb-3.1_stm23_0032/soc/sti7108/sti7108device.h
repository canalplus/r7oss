/***********************************************************************
 *
 * File: soc/sti7108/sti7108device.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7108DEVICE_H
#define _STI7108DEVICE_H

#ifdef __cplusplus

#include <Gamma/GenericGammaDevice.h>


class CSTmSDVTG;
class CSTmFSynthType1;
class CSTmTVOutDENC;
class CSTi7108MainMixer;
class CSTi7108AuxMixer;
class CSTi7108BDisp;
class CGammaVideoPlug;
class CSTmHDFormatterAWG;
class CSTmTVOutTeletext;
class CSTi7108ClkDivider;

class CSTi7108Device : public CGenericGammaDevice
{
public:
  CSTi7108Device(void);
  virtual ~CSTi7108Device(void);

  bool Create(void);

protected:
  CSTmFSynthType1        *m_pFSynthHD;
  CSTmFSynthType1        *m_pFSynthSD;
  CSTmSDVTG              *m_pVTG1;
  CSTmSDVTG              *m_pVTG2;
  CSTmTVOutDENC          *m_pDENC;
  CSTi7108MainMixer      *m_pMainMixer;
  CSTi7108AuxMixer       *m_pAuxMixer;
  CGammaVideoPlug        *m_pVideoPlug1;
  CGammaVideoPlug        *m_pVideoPlug2;
  CSTmHDFormatterAWG     *m_pAWGAnalog;
  CSTi7108BDisp          *m_pBDisp;
  CSTmTVOutTeletext      *m_pTeletext;
  CSTi7108ClkDivider     *m_pClkDivider;

  virtual bool CreateInfrastructure(void);
  virtual bool CreatePlanes(void);
  virtual bool CreateOutputs(void);
  virtual bool CreateHDMIOutput(void);
  virtual bool CreateGraphics(void);

private:
  CSTi7108Device(const CSTi7108Device&);
  CSTi7108Device& operator=(const CSTi7108Device&);
};

#endif /* __cplusplus */


#define STi7108_OUTPUT_IDX_VDP0_MAIN      ( 0)
#define STi7108_OUTPUT_IDX_VDP0_HDMI      ( 2)
#define STi7108_OUTPUT_IDX_DVO0           ( 3)
#define STi7108_OUTPUT_IDX_VDP1_MAIN      ( 1)

#define STi7108_BLITTER_IDX_KERNEL        ( 0)
#define STi7108_BLITTER_IDX_VDP0_MAIN     ( 1)
#define STi7108_BLITTER_IDX_VDP1_MAIN     ( 2)

#define STi7108_BLITTER_AQ_KERNEL         ( 1)
#define STi7108_BLITTER_AQ_VDP0_MAIN      ( 2)
#define STi7108_BLITTER_AQ_VDP1_MAIN      ( 3)


#endif // _STI7108DEVICE_H
