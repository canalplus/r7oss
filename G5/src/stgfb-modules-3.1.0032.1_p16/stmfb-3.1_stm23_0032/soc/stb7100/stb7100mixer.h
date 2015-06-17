/***********************************************************************
 *
 * File: soc/stb7100/stb7100mixer.h
 * Copyright (c) 2005,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7100MIXER_H
#define STb7100MIXER_H

#include <Gamma/GammaMixer.h>
#include "stb7100reg.h"

class CDisplayDevice;

/*
 * Mixer classes for both 7100 and 7109Cut2
 */
class CSTb7100MainMixer: public CGammaMixer
{
public:
  CSTb7100MainMixer(CDisplayDevice* pDev);

private:
  CSTb7100MainMixer(const CSTb7100MainMixer&);
  CSTb7100MainMixer& operator=(const CSTb7100MainMixer&);
};


class CSTb7100AuxMixer: public CGammaMixer
{
public:
  CSTb7100AuxMixer(CDisplayDevice* pDev):
    CGammaMixer(pDev, STb7100_MIXER2_OFFSET+STb7100_COMPOSITOR_BASE)
  {
    m_validPlanes = OUTPUT_GDP2 | OUTPUT_VID2;
    m_bHasYCbCrMatrix = false;
  }

  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

private:
  CSTb7100AuxMixer(const CSTb7100AuxMixer&);
  CSTb7100AuxMixer& operator=(const CSTb7100AuxMixer&);
};


/*
 * Mixer classes for 7109Cut3
 */
class CSTb7109Cut3MainMixer: public CGammaMixer
{
public:
  CSTb7109Cut3MainMixer(CDisplayDevice* pDev);

private:
  CSTb7109Cut3MainMixer(const CSTb7109Cut3MainMixer&);
  CSTb7109Cut3MainMixer& operator=(const CSTb7109Cut3MainMixer&);
};


class CSTb7109Cut3AuxMixer: public CGammaMixer
{
public:
  CSTb7109Cut3AuxMixer(CDisplayDevice* pDev);

private:
  CSTb7109Cut3AuxMixer(const CSTb7109Cut3AuxMixer&);
  CSTb7109Cut3AuxMixer& operator=(const CSTb7109Cut3AuxMixer&);
};

#endif /* STb7100MIXER_H */
