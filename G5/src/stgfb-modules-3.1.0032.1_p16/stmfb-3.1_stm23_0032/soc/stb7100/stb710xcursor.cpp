/***********************************************************************
 *
 * File: soc/stb7100/stb710xcursor.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Gamma/GenericGammaReg.h>

#include "stb710xcursor.h"


CSTb7100Cursor::CSTb7100Cursor(ULONG baseAddr): CGammaCompositorCursor(baseAddr)
{
  DENTRY();

  WriteCurReg(GDPn_PKZ_OFFSET ,0);

  DEXIT();
}

CSTb7100Cursor::~CSTb7100Cursor() {}


CSTb7109Cut3Cursor::CSTb7109Cut3Cursor(ULONG baseAddr): CGammaCompositorCursor(baseAddr)
{
  DENTRY();

  /*
   * Cursor plug registers are at the same offset as GDPs
   */
  WriteCurReg(GDPn_PAS ,2);
  WriteCurReg(GDPn_MAOS,5);
  WriteCurReg(GDPn_MIOS,3);
  WriteCurReg(GDPn_MACS,0);
  WriteCurReg(GDPn_MAMS,3);

  DEXIT();
}


CSTb7109Cut3Cursor::~CSTb7109Cut3Cursor() {}
