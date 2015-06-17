/***********************************************************************
 *
 * File: soc/sti7200/sti7200bdisp.h
 * Copyright (c) 2007/2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STi7200BDISP_H
#define STi7200BDISP_H

#include <STMCommon/stmbdisp.h>


class CSTi7200BDisp: public CSTmBDisp
{
public:
  CSTi7200BDisp (ULONG *pBDispBaseAddr);
};


#endif // STi7200BDISP_H
