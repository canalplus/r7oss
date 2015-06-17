/***********************************************************************
 *
 * File: soc/sti7111/sti7111mixer.h
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7111MIXER_H
#define STi7111MIXER_H

#include <Gamma/GammaMixer.h>

class CDisplayDevice;

class CSTi7111MainMixer: public CGammaMixer
{
public:
  CSTi7111MainMixer(CDisplayDevice *pDev, ULONG offset);

private:
  CSTi7111MainMixer(const CSTi7111MainMixer&);
  CSTi7111MainMixer& operator=(const CSTi7111MainMixer&);
};


class CSTi7111AuxMixer: public CGammaMixer
{
public:
  CSTi7111AuxMixer(CDisplayDevice *pDev, ULONG offset);

private:
  CSTi7111AuxMixer(const CSTi7111AuxMixer&);
  CSTi7111AuxMixer& operator=(const CSTi7111AuxMixer&);
};

#endif /* STi7111MIXER_H */
