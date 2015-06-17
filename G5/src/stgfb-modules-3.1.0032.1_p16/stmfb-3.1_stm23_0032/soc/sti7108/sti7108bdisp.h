/***********************************************************************
 *
 * File: soc/sti7108/sti7108bdisp.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7108_BDISP_H
#define _STi7108_BDISP_H

#include <STMCommon/stmbdisp.h>


class CSTi7108BDisp: public CSTmBDisp
{
public:
  /* Number of queues:
       - 4 AQs
       - 2 CQs
     Valid draw and copy operations the BDisp on 7108 can handle:
       - Disable destination colour keying
       - Enable BD-RLE decode
     Increased line buffer size
   */
  CSTi7108BDisp (ULONG *pBDispBaseAddr): CSTmBDisp (pBDispBaseAddr,
                                                    _STM_BLITTER_VERSION_7106_7108,
                                                    BDISP_VALID_DRAW_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                                                    ((BDISP_VALID_COPY_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY)
                                                     | STM_BLITTER_FLAGS_RLE_DECODE),
                                                    4,
                                                    2,
                                                    720) {}

private:
  void ConfigurePlugs (void);
};


#endif // _STi7108_BDISP_H
