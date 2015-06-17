/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407mixer.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STiH407_MIXER_H
#define _STiH407_MIXER_H

#include <display/gamma/GammaMixer.h>
#include <display/gamma/GenericGammaReg.h>
#include <display/gamma/GenericGammaDefs.h>

#include "stiH407reg.h"

class CDisplayDevice;
class CSTmPreformatter;

class CSTiH407Mixer: public CGammaMixer
{
public:
  CSTiH407Mixer(CDisplayDevice *pDev,
                uint32_t        mixerOffset,
                uint32_t        pfOffset,
          const stm_mixer_id_t *planeMap,
                int             planeMapSize,
                stm_mixer_id_t  VBILinkedPlane);

  ~CSTiH407Mixer(void);

  bool Create(void);

  void UpdateColorspace(const stm_display_mode_t *pModeLine);

private:
  CSTmPreformatter *m_pPreformatter;
  uint32_t          m_uPFOffset;

  CSTiH407Mixer(const CSTiH407Mixer&);
  CSTiH407Mixer& operator=(const CSTiH407Mixer&);
};


class CSTiH407MainMixer: public CSTiH407Mixer
{
public:
  CSTiH407MainMixer(
      CDisplayDevice *pDev,
const stm_mixer_id_t *planeMap,
      int             planeMapSize,
      stm_mixer_id_t  VBILinkedPlane): CSTiH407Mixer(pDev,
                                                     (STiH407_COMPOSITOR_BASE+STiH407_MIXER1_OFFSET),
                                                     STiH407_TVO_MAIN_PF_BASE,
                                                     planeMap,
                                                     planeMapSize,
                                                     VBILinkedPlane)
  {
    /*
     * Fix the default plane order to BKG -> VID1 -> GDP1 -> VID2 -> GDP2 -> GDP4 -> GDP3
     * VID1 = main video program
     * GDP1 = e.g. DVD subpicture or Subtitles/CC presentation for VID1 [Note non scalable]
     * VID2 = PIP second video program
     * GDP2 = "floating" extra plane between main and aux pipelines [Note non scalable]
     * GDP4 = "floating" extra plane between main and aux pipelines
     * GDP3 = primary framebuffer for application UI + VBI linked plane
     */
    uint32_t uCRBReg = (MIX_CRB_VID1 << MIX_CRB_DEPTH1_SHIFT) |
                       (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT) |
                       (MIX_CRB_VID2 << MIX_CRB_DEPTH3_SHIFT) |
                       (MIX_CRB_GDP2 << MIX_CRB_DEPTH4_SHIFT) |
                       (MIX_CRB_GDP4 << MIX_CRB_DEPTH5_SHIFT) |
                       (MIX_CRB_GDP3 << MIX_CRB_DEPTH6_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, uCRBReg);
    m_crossbarSize = 6;
  }

private:
  CSTiH407MainMixer(const CSTiH407MainMixer&);
  CSTiH407MainMixer& operator=(const CSTiH407MainMixer&);
};


class CSTiH407AuxMixer: public CSTiH407Mixer
{
public:
  CSTiH407AuxMixer(
      CDisplayDevice *pDev,
const stm_mixer_id_t *planeMap,
      int             planeMapSize): CSTiH407Mixer(pDev,
                                                   (STiH407_COMPOSITOR_BASE+STiH407_MIXER2_OFFSET),
                                                   STiH407_TVO_AUX_PF_BASE,
                                                   planeMap,
                                                   planeMapSize,
                                                   MIXER_ID_NONE)
  {
    uint32_t uCRBReg = (MIX_CRB_VID2 << MIX_CRB_DEPTH1_SHIFT) |
                       (MIX_CRB_GDP1 << MIX_CRB_DEPTH2_SHIFT) |
                       (MIX_CRB_GDP2 << MIX_CRB_DEPTH3_SHIFT) |
                       (MIX_CRB_GDP4 << MIX_CRB_DEPTH4_SHIFT);

    WriteMixReg(MXn_CRB_REG_OFFSET, uCRBReg);
    m_crossbarSize = 4;
  }

private:
  CSTiH407AuxMixer(const CSTiH407AuxMixer&);
  CSTiH407AuxMixer& operator=(const CSTiH407AuxMixer&);
};

#endif /* _STiH407_MIXER_H */
