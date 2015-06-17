/***********************************************************************
 *
 * File: soc/sti7200/sti7200gdp.h
 * Copyright (c) 2007,2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7200GDP_H
#define _STi7200GDP_H

#include <Gamma/GammaCompositorGDP.h>

class CSTi7200LocalGDP: public CGammaCompositorGDP
{
public:
  CSTi7200LocalGDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTi7200LocalGDP(const CSTi7200LocalGDP&);
  CSTi7200LocalGDP& operator=(const CSTi7200LocalGDP&);
};


class CSTi7200Cut2LocalGDP: public CSTi7200LocalGDP
{
public:
  CSTi7200Cut2LocalGDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTi7200Cut2LocalGDP(const CSTi7200Cut2LocalGDP&);
  CSTi7200Cut2LocalGDP& operator=(const CSTi7200Cut2LocalGDP&);
};


class CSTi7200RemoteGDP: public CSTi7200LocalGDP
{
public:
  CSTi7200RemoteGDP(stm_plane_id_t GDPid, ULONG baseAddr);

private:
  CSTi7200RemoteGDP(const CSTi7200RemoteGDP&);
  CSTi7200RemoteGDP& operator=(const CSTi7200RemoteGDP&);
};

#endif // _STi7200GDP_H
