/***********************************************************************
 *
 * File: soc/sti7200/sti7200denc.h
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7200DENC_H
#define _STi7200DENC_H

#include <HDTVOutFormatter/stmtvoutdenc.h>

class CDisplayDevice;
class COutput;
class CSTmTVOutTeletext;

/*
 * Both DENCs on Cut1 and the remote output DENC on Cut2 use this, it only
 * supports Y/C-CVBS or YPrPb on a single set of DAC outputs.
 */
class CSTi7200DENC : public CSTmTVOutDENC
{
public:
  CSTi7200DENC(CDisplayDevice    *pDev,
               ULONG              regoffset,
               CSTmTVOutTeletext *pTeletext = 0): CSTmTVOutDENC(pDev, regoffset, pTeletext) {}

  bool SetMainOutputFormat(ULONG);
  bool SetAuxOutputFormat(ULONG);

private:
  void ProgramOutputFormats(void);

  CSTi7200DENC(const CSTi7200DENC&);
  CSTi7200DENC& operator=(const CSTi7200DENC&);
};


#endif // _STi7200DENC_H
