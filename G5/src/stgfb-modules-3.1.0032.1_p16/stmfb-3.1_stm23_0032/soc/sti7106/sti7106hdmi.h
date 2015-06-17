/***********************************************************************
 *
 * File: soc/sti7106/sti7106hdmi.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7106HDMI_H
#define _STI7106HDMI_H

#include <soc/sti7111/sti7111hdmi.h>

class CSTi7106HDMI: public CSTi7111HDMI
{
public:
  CSTi7106HDMI(CDisplayDevice *pDev, COutput *pMainOutput, CSTmSDVTG *pVTG);
  virtual ~CSTi7106HDMI(void);

  bool Create(void);
  bool Stop(void);

protected:
  bool SetInputSource(ULONG  source);
  bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard);
  bool PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard);

private:
  bool SetupRejectionPLL(const stm_mode_line_t*, ULONG tvStandard, ULONG divide);

  CSTi7106HDMI(const CSTi7106HDMI&);
  CSTi7106HDMI& operator=(const CSTi7106HDMI&);
};

#endif //_STI7106HDMI_H
