/***********************************************************************
 *
 * File: soc/sti7200/sti7200mixer.h
 * Copyright (c) 2007,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7200MIXER_H
#define STi7200MIXER_H

#include <Gamma/GammaMixer.h>
#include "sti7200reg.h"

class CDisplayDevice;

class CSTi7200LocalMainMixer: public CGammaMixer
{
public:
  CSTi7200LocalMainMixer(CDisplayDevice* pDev);

private:
  CSTi7200LocalMainMixer(const CSTi7200LocalMainMixer&);
  CSTi7200LocalMainMixer& operator=(const CSTi7200LocalMainMixer&);
};


class CSTi7200LocalAuxMixer: public CGammaMixer
{
public:
  CSTi7200LocalAuxMixer(CDisplayDevice* pDev);

private:
  CSTi7200LocalAuxMixer(const CSTi7200LocalAuxMixer&);
  CSTi7200LocalAuxMixer& operator=(const CSTi7200LocalAuxMixer&);
};


class CSTi7200Cut2RemoteMixer: public CGammaMixer
{
public:
  CSTi7200Cut2RemoteMixer(CDisplayDevice* pDev);

private:
  CSTi7200Cut2RemoteMixer(const CSTi7200Cut2RemoteMixer&);
  CSTi7200Cut2RemoteMixer& operator=(const CSTi7200Cut2RemoteMixer&);
};


#endif /* STi7200MIXER_H */
