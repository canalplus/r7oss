/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2bdisp.cpp
 * Copyright (c) 2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7200bdisp.h"


/* Number of queues:
     - 4 AQs
     - 2 CQs
   Valid draw and copy operations the BDisp on 7200 can handle:
     - Disable destination colour keying
     - Enable BD-RLE decode
   At some point we need to enable H2 and H8 RLE decode. */
CSTi7200BDisp::CSTi7200BDisp (ULONG *pBDispBaseAddr): 
                   CSTmBDisp (pBDispBaseAddr,
                              _STM_BLITTER_VERSION_7200c2_7111_7141_7105,
                              BDISP_VALID_DRAW_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY,
                              ((BDISP_VALID_COPY_OPS & ~STM_BLITTER_FLAGS_DST_COLOR_KEY)
                               | STM_BLITTER_FLAGS_RLE_DECODE),
                              4,
                              2)
{
  DEBUGF2 (4, (FENTRY ", (cut2) self: %p, BDisp base: %p\n",
               __PRETTY_FUNCTION__, this, pBDispBaseAddr));
  DEBUGF2 (4, (FEXIT ", self: %p\n", __PRETTY_FUNCTION__, this));
};
