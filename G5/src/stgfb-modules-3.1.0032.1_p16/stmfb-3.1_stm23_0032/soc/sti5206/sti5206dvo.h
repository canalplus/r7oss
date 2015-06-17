/***********************************************************************
 *
 * File: soc/sti5206/sti5206dvo.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI5206DVO_H
#define _STI5206DVO_H

#include <HDTVOutFormatter/stmhdfdvo.h>

class CSTi5206DVO: public CSTmHDFDVO
{
public:
  CSTi5206DVO(CDisplayDevice *pDev,
              COutput        *pMasterOutput,
              ULONG           ulDVOBase,
              ULONG           ulHDFormatterBase);

protected:
  bool SetOutputFormat(ULONG format, const stm_mode_line_t* pModeLine);

private:
  CSTi5206DVO(const CSTi5206DVO&);
  CSTi5206DVO& operator=(const CSTi5206DVO&);
};


#endif /* _STI5206DVO_H */
