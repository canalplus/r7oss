/***********************************************************************
 *
 * File: soc/sti7111/sti7111bdisp.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7111bdisp.h"


void CSTi7111BDisp::ConfigurePlugs (void)
{
  DEBUGF2 (2, (FENTRY ": %p\n", __PRETTY_FUNCTION__, this));

  /* on 7111, we have a different bus architecture, allowing us to use more
     bandwidth on the blitter, so we set up the bus plugs a bit differently.
     This results in about 20% more speed at least when doing fill
     rectangles in DirectFB. */
  WriteBDispReg (BDISP_S1_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S1_CHUNK,0x3);
  WriteBDispReg (BDISP_S1_MESSAGE,0x0);
  WriteBDispReg (BDISP_S1_PAGE,0x2);

  WriteBDispReg (BDISP_S2_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S2_CHUNK,0x3);
  WriteBDispReg (BDISP_S2_MESSAGE,0x0);
  WriteBDispReg (BDISP_S2_PAGE,0x2);

  WriteBDispReg (BDISP_S3_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_S3_CHUNK,0x3);
  WriteBDispReg (BDISP_S3_MESSAGE,0x0);
  WriteBDispReg (BDISP_S3_PAGE,0x2);

  WriteBDispReg (BDISP_TARGET_MAX_OPCODE,0x5);
  WriteBDispReg (BDISP_TARGET_CHUNK,0x3);
  WriteBDispReg (BDISP_TARGET_MESSAGE,0x0);
  WriteBDispReg (BDISP_TARGET_PAGE,0x2);

  DEBUGF2 (2, (FEXIT ": %p\n", __PRETTY_FUNCTION__, this));
}
