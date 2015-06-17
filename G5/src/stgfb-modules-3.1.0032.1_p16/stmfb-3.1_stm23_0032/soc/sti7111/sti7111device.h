/***********************************************************************
 *
 * File: soc/sti7111/sti7111device.h
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7111DEVICE_H
#define _STI7111DEVICE_H

#ifdef __cplusplus

#include <Gamma/GenericGammaDevice.h>


class CSTmSDVTG;
class CSTmFSynthType1;
class CSTmTVOutDENC;
class CSTi7111MainMixer;
class CSTi7111AuxMixer;
class CSTi7111BDisp;
class CGammaVideoPlug;
class CSTmHDFormatterAWG;
class CGDPBDispOutput;
class CSTmTVOutTeletext;
class CGammaCompositorPlane;
class CDisplayMixer;

class CSTi7111Device : public CGenericGammaDevice
{
public:
  CSTi7111Device(void);
  virtual ~CSTi7111Device(void);

  bool Create(void);

  CDisplayPlane *GetPlane(stm_plane_id_t planeID) const;

  void UpdateDisplay(COutput *);

protected:
  CSTmFSynthType1        *m_pFSynthHD;
  CSTmFSynthType1        *m_pFSynthSD;
  CSTmSDVTG              *m_pVTG1;
  CSTmSDVTG              *m_pVTG2;
  CSTmTVOutDENC          *m_pDENC;
  CDisplayMixer          *m_pMainMixer;
  CDisplayMixer          *m_pAuxMixer;
  CGammaVideoPlug        *m_pVideoPlug1;
  CGammaVideoPlug        *m_pVideoPlug2;
  CSTmHDFormatterAWG     *m_pAWGAnalog;
  CGDPBDispOutput        *m_pGDPBDispOutput;
  CSTi7111BDisp          *m_pBDisp;
  CSTmTVOutTeletext      *m_pTeletext;

  stm_plane_id_t         m_sharedVideoPlaneID;
  stm_plane_id_t         m_sharedGDPPlaneID;

  virtual bool CreateInfrastructure(void);
  virtual bool CreateMixers(void);
  virtual bool CreatePlanes(void);
  virtual bool CreateVideoPlanes(CGammaCompositorPlane **vid1,CGammaCompositorPlane **vid2);
  virtual bool CreateOutputs(void);
  virtual bool CreateHDMIOutput(void);
  virtual bool CreateGraphics(void);

private:
  CSTi7111Device(const CSTi7111Device&);
  CSTi7111Device& operator=(const CSTi7111Device&);
};

#endif /* __cplusplus */


#define STi7111_OUTPUT_IDX_VDP0_MAIN      ( 0)
#define STi7111_OUTPUT_IDX_VDP0_HDMI      ( 2)
#define STi7111_OUTPUT_IDX_DVO0           ( 4)
#define STi7111_OUTPUT_IDX_VDP1_MAIN      ( 1)
#define STi7111_OUTPUT_IDX_GDP            ( 3)

#define STi7111_BLITTER_IDX_KERNEL        ( 0)
#define STi7111_BLITTER_IDX_VDP0_MAIN     ( 1)
#define STi7111_BLITTER_IDX_VDP1_MAIN     ( 2)

#define STi7111_BLITTER_AQ_KERNEL         ( 1)
#define STi7111_BLITTER_AQ_VDP0_MAIN      ( 2)
#define STi7111_BLITTER_AQ_VDP1_MAIN      ( 3)


#endif // _STI7111DEVICE_H
