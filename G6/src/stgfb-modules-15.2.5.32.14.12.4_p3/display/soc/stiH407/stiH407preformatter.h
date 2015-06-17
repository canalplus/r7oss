/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407preformatter.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH407PREFORMATTER
#define _STIH407PREFORMATTER

#include  <display/ip/tvout/stmpreformatter.h>

class CDisplayDevice;

class CSTiH407Preformatter: public CSTmPreformatter
{
public:
  CSTiH407Preformatter(CDisplayDevice               *pDev,
                       uint32_t                      ulPFRegs,
                       const stm_vout_pf_mat_coef_t *ulPFConversionMat):CSTmPreformatter(pDev, ulPFRegs, ulPFConversionMat) {
                             m_bUseAdaptiveDecimationFilter = true;
                           }

private:
  CSTiH407Preformatter(const CSTiH407Preformatter&);
  CSTiH407Preformatter& operator=(const CSTiH407Preformatter&);
};

#endif //_STIH407PREFORMATTER
