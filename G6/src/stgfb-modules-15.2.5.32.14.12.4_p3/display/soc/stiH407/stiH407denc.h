/***********************************************************************
 *
 * File: display/soc/stiH407/sti407denc.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STIH407DENC_H
#define STIH407DENC_H

#include <display/ip/tvout/stmtvoutdenc.h>

class CSTiH407DENC : public CSTmTVOutDENC
{
public:
  CSTiH407DENC(CDisplayDevice* pDev, uint32_t baseAddr, CSTmTVOutTeletext *pTeletext);
  CSTiH407DENC(void);

  bool SetMainOutputFormat(uint32_t);

private:
  CSTiH407DENC(const CSTiH407DENC&);
  CSTiH407DENC& operator=(const CSTiH407DENC&);
};

#endif // STIH407DENC_H
