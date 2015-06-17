/***********************************************************************
 *
 * File: soc/stb7100/stb710xcursor.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STb710XCURSOR_H
#define STb710XCURSOR_H

#include <Gamma/GammaCompositorCursor.h>

class CSTb7100Cursor: public CGammaCompositorCursor
{
public:
    CSTb7100Cursor(ULONG baseAddr);
    ~CSTb7100Cursor();
};


class CSTb7109Cut3Cursor: public CGammaCompositorCursor
{
public:
    CSTb7109Cut3Cursor(ULONG baseAddr);
    ~CSTb7109Cut3Cursor();
};

#endif // STb710XCURSOR_H
