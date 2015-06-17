/***********************************************************************
 *
 * File: soc/stb7100/stb7109bdisp.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb7109BDISP_H
#define STb7109BDISP_H

#include <STMCommon/stmbdisp.h>


class CSTb7109BDisp: public CSTmBDisp
{
public:
  /* Number of queues:
       - 1 AQ
       - 2 CQ
     Valid draw and copy operations on the BDisp on 7109:
     - disable destination color keying. */
  CSTb7109BDisp (ULONG *pBDispBaseAddr): CSTmBDisp (pBDispBaseAddr,
                                                    _STM_BLITTER_VERSION_7109c3,
                                                    BDISP_VALID_DRAW_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                                                    BDISP_VALID_COPY_OPS & ~(STM_BLITTER_FLAGS_DST_COLOR_KEY
                                                                             | STM_BLITTER_FLAGS_3BUFFER_SOURCE),
                                                    1,
                                                    2) {}
};


#endif // STb7109BDISP_H
