/***********************************************************************
 *
 * File: soc/sti7111/sti7111bdisp.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7111BDISP_H
#define STi7111BDISP_H

#include <STMCommon/stmbdisp.h>


class CSTi7111BDisp: public CSTmBDisp
{
public:
  /* Number of queues:
       - 4 AQs
       - 2 CQs
     Valid draw and copy operations the BDisp on 7111 can handle:
       - Disable destination colour keying
       - Enable BD-RLE decode */
  CSTi7111BDisp (ULONG *pBDispBaseAddr): CSTmBDisp (pBDispBaseAddr,
                                                    _STM_BLITTER_VERSION_7200c2_7111_7141_7105,
                                                    BDISP_VALID_DRAW_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                                                    ((BDISP_VALID_COPY_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY)
                                                     | STM_BLITTER_FLAGS_RLE_DECODE),
                                                    4,
                                                    2) {}

private:
  void ConfigurePlugs (void);
};


#endif // STi7111BDISPAQ_H
