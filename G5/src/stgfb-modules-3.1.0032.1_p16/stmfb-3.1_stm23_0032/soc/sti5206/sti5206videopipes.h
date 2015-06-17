/***********************************************************************
 *
 * File: soc/sti5206/sti5206videopipes.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STI5206_VIDEOPIPES_H
#define _STI5206_VIDEOPIPES_H

#include <Gamma/DEIVideoPipeV2.h>

class CSTi5206DEI: public CDEIVideoPipeV2
{
public:
  CSTi5206DEI (stm_plane_id_t   GDPid,
               CGammaVideoPlug *plug,
               ULONG            baseAddr);

  bool Create(void);

private:
  CSTi5206DEI (const CSTi5206DEI &);
  CSTi5206DEI& operator= (const CSTi5206DEI &);
};


class CSTi5206VDP: public CDEIVideoPipe
{
public:
  CSTi5206VDP(stm_plane_id_t   planeID,
              CGammaVideoPlug *plug,
              ULONG            vdpBaseAddr);

  bool Create(void);

private:
  CSTi5206VDP (const CSTi5206VDP &);
  CSTi5206VDP& operator= (const CSTi5206VDP &);
};

#endif // _STI5206_VIDEOPIPES_H
