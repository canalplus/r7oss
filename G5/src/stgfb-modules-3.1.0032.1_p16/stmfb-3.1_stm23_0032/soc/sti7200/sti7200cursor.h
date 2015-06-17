/***********************************************************************
 *
 * File: soc/sti7200/sti7200cursor.h
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7200CURSOR_H
#define STi7200CURSOR_H

#include <Gamma/GammaCompositorCursor.h>
#include <Gamma/GenericGammaReg.h>

class CSTi7200Cursor: public CGammaCompositorCursor
{
public:
  CSTi7200Cursor(ULONG baseAddr): CGammaCompositorCursor(baseAddr)
  {
    DENTRY();

    /*
     * Cursor plug registers are at the same offset as GDPs
     */
    WriteCurReg(GDPn_PAS ,6);
    WriteCurReg(GDPn_MAOS,5);
    WriteCurReg(GDPn_MIOS,3);
    WriteCurReg(GDPn_MACS,0);
    WriteCurReg(GDPn_MAMS,3);

    DEXIT();
  }

  ~CSTi7200Cursor() {}

private:
  CSTi7200Cursor(const CSTi7200Cursor &);
  CSTi7200Cursor& operator= (const CSTi7200Cursor &);
};

#endif // STi7200CURSOR_H
