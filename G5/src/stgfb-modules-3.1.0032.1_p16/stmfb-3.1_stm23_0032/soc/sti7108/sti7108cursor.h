/***********************************************************************
 *
 * File: soc/sti7108/sti7108cursor.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7108CURSOR_H
#define STi7108CURSOR_H

#include <Gamma/GammaCompositorCursor.h>
#include <Gamma/GenericGammaReg.h>

class CSTi7108Cursor: public CGammaCompositorCursor
{
public:
  CSTi7108Cursor(ULONG baseAddr): CGammaCompositorCursor(baseAddr)
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

  ~CSTi7108Cursor() {}

private:
  CSTi7108Cursor(const CSTi7108Cursor &);
  CSTi7108Cursor& operator= (const CSTi7108Cursor &);
};

#endif // STi7108CURSOR_H
