/***********************************************************************
 *
 * File: soc/sti7111/sti7111cursor.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7111CURSOR_H
#define STi7111CURSOR_H

#include <Gamma/GammaCompositorCursor.h>
#include <Gamma/GenericGammaReg.h>

class CSTi7111Cursor: public CGammaCompositorCursor
{
public:
  CSTi7111Cursor(ULONG baseAddr): CGammaCompositorCursor(baseAddr)
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

  ~CSTi7111Cursor() {}

private:
  CSTi7111Cursor(const CSTi7111Cursor &);
  CSTi7111Cursor& operator= (const CSTi7111Cursor &);
};

#endif // STi7111CURSOR_H
