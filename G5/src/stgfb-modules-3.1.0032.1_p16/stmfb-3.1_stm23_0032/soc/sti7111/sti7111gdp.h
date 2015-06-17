/***********************************************************************
 *
 * File: soc/sti7111/sti7111gdp.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7111GDP_H
#define _STi7111GDP_H

#include <Gamma/GammaCompositorGDP.h>

class CSTi7111GDP: public CGammaCompositorGDP
{
public:
  CSTi7111GDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTi7111GDP(const CSTi7111GDP&);
  CSTi7111GDP& operator=(const CSTi7111GDP&);
};

#endif // _STi7111GDP_H
