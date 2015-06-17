/*******************************************************************************
 *
 * File: display/ip/vdp/VdpFilter.h
 * Copyright (c) 2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ******************************************************************************/

#ifndef _VDPFILTER_H
#define _VDPFILTER_H

typedef enum coef_e
  {
    COEF_X2,
    COEF_A,
    COEF_B,
    COEF_C,
    COEF_D,
    COEF_E,
    COEF_F,
    COEF_G,
    COEF_H,
    COEF_I,
    COEF_J,
    COEF_K,
    COEF_L
  } coef_t;

typedef enum boundary_e
  {
    BOUNDARY_A_C,
    BOUNDARY_C_D,
    BOUNDARY_D_E,
    BOUNDARY_E_F,
    BOUNDARY_F_G,
    BOUNDARY_G_H,
    BOUNDARY_H_I,
    BOUNDARY_I_J,
    BOUNDARY_J_K,
    BOUNDARY_K_L
  } boundary_t;

typedef struct luma_chroma_s
{
  const void *luma;
  const void *chroma;
} luma_chroma_t;

#define ZOOMx2 0x1000
#define NB_VC1_RANGE_MAP (STM_BUFFER_SRC_VC1_RANGE_MAP_MAX + 1)

class CVdpFilter
{
 public:
  CVdpFilter(void);
  ~CVdpFilter(void);

  bool Create(void);
  bool SelectFilterSet( stm_plane_filter_set_t filterSet );
  bool GetFilterSet( stm_plane_filter_set_t * filterSet ) const;
  const uint32_t *SelectHorizontalLumaFilter( uint32_t scaleFactor, uint32_t phase );
  const uint32_t *SelectVerticalLumaFilter( uint32_t scaleFactor, uint32_t phase );
  const uint32_t *SelectHorizontalChromaFilter( uint32_t scaleFactor, uint32_t phase );
  const uint32_t *SelectVerticalChromaFilter (uint32_t scaleFactor, uint32_t phase );
  const uint32_t *SelectVC1VerticalLumaFilter( uint32_t scaleFactor, uint32_t phase, uint32_t vc1type );
  const uint32_t *SelectVC1VerticalChromaFilter(uint32_t scaleFactor,   uint32_t phase, uint32_t vc1type);

 private:
  bool CreateVC1RangeMapping( stm_plane_filter_set_t filterSet );

  DMA_Area m_lumaVC1RangeMapping;
  DMA_Area m_chromaVC1RangeMapping;
  stm_plane_filter_set_t m_filterSet;
  luma_chroma_t m_horizontalLumaChroma[COEF_F + 1];
  luma_chroma_t m_verticalLumaChroma[COEF_L + 1];
  coef_t m_maxVerticalLumaCoef;
  const uint32_t *m_horizontalBoundaries;
  const uint32_t *m_verticalBoundaries;
  uint8_t m_nbLumaVerticalTables;
  uint8_t m_nbChromaVerticalTables;
};

#endif // _VDPFILTER_H
