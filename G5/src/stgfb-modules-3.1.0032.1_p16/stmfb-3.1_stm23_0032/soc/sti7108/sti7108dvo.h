/***********************************************************************
 *
 * File: soc/sti7108/sti7108dvo.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7108DVO_H
#define _STI7108DVO_H

#include <HDTVOutFormatter/stmhdfdvo.h>

class CSTi7108ClkDivider;

class CSTi7108DVO: public CSTmHDFDVO
{
public:
  CSTi7108DVO(CDisplayDevice     *pDev,
              COutput            *pMasterOutput,
              CSTi7108ClkDivider *pClkDivider,
              ULONG               ulDVOBase,
              ULONG               ulHDFormatterBase);

protected:
  bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

private:
  CSTi7108ClkDivider *m_pClkDivider;

  CSTi7108DVO(const CSTi7108DVO&);
  CSTi7108DVO& operator=(const CSTi7108DVO&);
};


#endif /* _STI7108DVO_H */
