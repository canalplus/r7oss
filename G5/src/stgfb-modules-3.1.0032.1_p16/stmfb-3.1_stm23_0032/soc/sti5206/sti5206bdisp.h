/***********************************************************************
 *
 * File: soc/sti5206/sti5206bdisp.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi5206_BDISP_H
#define _STi5206_BDISP_H

#include <STMCommon/stmbdisp.h>


class CSTi5206BDisp: public CSTmBDisp
{
public:
  /* Number of queues:
       - 4 AQs
       - 2 CQs
     Valid draw and copy operations the BDisp on 5206 can handle:
       - Disable destination colour keying
   */
  CSTi5206BDisp (ULONG *pBDispBaseAddr): CSTmBDisp (pBDispBaseAddr,
                                                    _STM_BLITTER_VERSION_5206,
                                                    BDISP_VALID_DRAW_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                                                    BDISP_VALID_COPY_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                                                    4,
                                                    2,
                                                    720) {}

private:
  void ConfigurePlugs (void);
};


#endif // _STi5206_BDISP_H
