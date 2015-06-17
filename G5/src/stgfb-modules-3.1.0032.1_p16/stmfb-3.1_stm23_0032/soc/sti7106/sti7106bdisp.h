/***********************************************************************
 *
 * File: soc/sti7106/sti7106bdisp.h
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STi7106_BDISP_H
#define _STi7106_BDISP_H

#include <soc/sti7111/sti7111bdisp.h>


class CSTi7106BDisp: public CSTi7111BDisp
{
public:
  /* Identical to 7111, except for an increased line buffer size (as on 7108)
   */
  CSTi7106BDisp (ULONG *pBDispBaseAddr): CSTi7111BDisp (pBDispBaseAddr)
  {
    m_eBDispDevice = _STM_BLITTER_VERSION_7106_7108;
    m_nLineBufferLength = 720;
  }
};


#endif // _STi7106_BDISP_H
