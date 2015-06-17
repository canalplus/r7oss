/***********************************************************************
 *
 * File: display/ip/tvout/stmpreformatter.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_REFORMATTER_H
#define _STM_REFORMATTER_H

#include <stm_display.h>

class CDisplayDevice;

typedef struct stm_vout_pf_mat_coef_s
{
  uint32_t ycbcr_to_rgb[2][8];
  uint32_t rgb_to_ycbcr[2][8];
} stm_vout_pf_mat_coef_t;


typedef enum stm_vout_pf_mat_direction_e
{
    VOUT_PF_CONV_YUV_TO_RGB
  , VOUT_PF_CONV_RGB_TO_YUV
} stm_vout_pf_mat_direction_t;

typedef enum stm_vout_pf_adaptive_decimation_filter_e
{
    VOUT_PF_ADAPTIVE_FILTER
  , VOUT_PF_LINEAR_FILTER
  , VOUT_PF_SAMPLE_DROP
} stm_vout_pf_adaptive_decimation_filter_t;


class CSTmPreformatter
{
public:
  CSTmPreformatter(CDisplayDevice               *pDev,
                   uint32_t                      ulPFRegs,
                   const stm_vout_pf_mat_coef_t *ulPFConversionMat);

  virtual ~CSTmPreformatter(void);

  bool SetColorSpaceConversion(stm_ycbcr_colorspace_t colorspaceMode, stm_vout_pf_mat_direction_t direction = VOUT_PF_CONV_YUV_TO_RGB);
  bool SetAdaptiveDecimationFilter(stm_vout_pf_adaptive_decimation_filter_t filter);

protected:
  uint32_t                     *m_pDevRegs;
  uint32_t                      m_ulPFRegOffset;
  const stm_vout_pf_mat_coef_t *m_pPFConversionMat;
  bool                          m_bUseAdaptiveDecimationFilter;

  void WriteReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_ulPFRegOffset + reg), val); }
  uint32_t ReadReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_ulPFRegOffset + reg)); }

private:
  CSTmPreformatter(const CSTmPreformatter&);
  CSTmPreformatter& operator=(const CSTmPreformatter&);
};


#endif //_STM_REFORMATTER_H
