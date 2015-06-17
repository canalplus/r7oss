/*******************************************************************************
 *
 * File: display/ip/vdp/VdpFilter.cpp
 * Copyright (c) 2005-2007,2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ******************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "STRefVdpFilters.h"
#include "VdpFilter.h"


CVdpFilter::CVdpFilter(void)
{
  vibe_os_zero_memory( &m_lumaVC1RangeMapping, sizeof( DMA_Area ));
  vibe_os_zero_memory( &m_chromaVC1RangeMapping, sizeof( DMA_Area ));
  vibe_os_zero_memory( m_horizontalLumaChroma, sizeof( m_horizontalLumaChroma ));
  vibe_os_zero_memory( m_verticalLumaChroma, sizeof( m_verticalLumaChroma ));
  m_filterSet = FILTER_SET_MEDIUM;
  m_maxVerticalLumaCoef = COEF_L;
  m_horizontalBoundaries = 0;
  m_verticalBoundaries = 0;
  m_nbLumaVerticalTables = 0;
  m_nbChromaVerticalTables = 0;
}


CVdpFilter::~CVdpFilter(void)
{
  vibe_os_free_dma_area( &m_lumaVC1RangeMapping );
  vibe_os_free_dma_area( &m_chromaVC1RangeMapping );
}


bool CVdpFilter::Create(void)
{
  // by default, use medium filters
  SelectFilterSet( FILTER_SET_MEDIUM );
  return true;
}


bool CVdpFilter::SelectFilterSet( stm_plane_filter_set_t filterSet )
{
  switch ( filterSet )
    {
    case FILTER_SET_LEGACY :
      TRC( TRC_ID_VDP, "LEGACY filter set selected" );
      m_horizontalBoundaries = LegacyHorizontalBoundaries;
      m_horizontalLumaChroma[COEF_X2].chroma = LegacyHorizontalCoefficientsChromaZoomX2;
      m_horizontalLumaChroma[COEF_A].luma = LegacyHorizontalCoefficientsA;
      m_horizontalLumaChroma[COEF_A].chroma = LegacyHorizontalCoefficientsA;
      m_horizontalLumaChroma[COEF_B].luma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_B].chroma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_C].luma = LegacyHorizontalCoefficientsLumaC;
      m_horizontalLumaChroma[COEF_C].chroma = LegacyHorizontalCoefficientsChromaC;
      m_horizontalLumaChroma[COEF_D].luma = LegacyHorizontalCoefficientsLumaD;
      m_horizontalLumaChroma[COEF_D].chroma = LegacyHorizontalCoefficientsChromaD;
      m_horizontalLumaChroma[COEF_E].luma = LegacyHorizontalCoefficientsLumaE;
      m_horizontalLumaChroma[COEF_E].chroma = LegacyHorizontalCoefficientsChromaE;
      m_horizontalLumaChroma[COEF_F].luma = LegacyHorizontalCoefficientsLumaF;
      m_horizontalLumaChroma[COEF_F].chroma = LegacyHorizontalCoefficientsChromaF;
      m_verticalBoundaries = LegacyVerticalBoundaries;
      m_verticalLumaChroma[COEF_X2].chroma = LegacyVerticalCoefficientsChromaZoomX2;
      m_verticalLumaChroma[COEF_A].luma = LegacyVerticalCoefficientsA;
      m_verticalLumaChroma[COEF_A].chroma = LegacyVerticalCoefficientsA;
      m_verticalLumaChroma[COEF_B].luma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_B].chroma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_C].luma = LegacyVerticalCoefficientsLumaC;
      m_verticalLumaChroma[COEF_C].chroma = LegacyVerticalCoefficientsChromaC;
      m_verticalLumaChroma[COEF_D].luma = LegacyVerticalCoefficientsLumaD;
      m_verticalLumaChroma[COEF_D].chroma = LegacyVerticalCoefficientsChromaD;
      m_verticalLumaChroma[COEF_E].luma = LegacyVerticalCoefficientsLumaE;
      m_verticalLumaChroma[COEF_E].chroma = LegacyVerticalCoefficientsChromaE;
      m_verticalLumaChroma[COEF_F].luma = LegacyVerticalCoefficientsLumaF;
      m_verticalLumaChroma[COEF_F].chroma = LegacyVerticalCoefficientsChromaF;
      m_verticalLumaChroma[COEF_G].luma = 0;
      m_verticalLumaChroma[COEF_H].luma = 0;
      m_verticalLumaChroma[COEF_I].luma = 0;
      m_verticalLumaChroma[COEF_J].luma = 0;
      m_verticalLumaChroma[COEF_K].luma = 0;
      m_verticalLumaChroma[COEF_L].luma = 0;
      m_maxVerticalLumaCoef = COEF_F;
      break;

    case FILTER_SET_MEDIUM :
      TRC( TRC_ID_VDP, "MEDIUM filter set selected" );
      m_horizontalBoundaries = MediumHorizontalBoundaries;
      m_horizontalLumaChroma[COEF_X2].chroma = 0;
      m_horizontalLumaChroma[COEF_A].luma = MediumHorizontalCoefficientsLumaA;
      m_horizontalLumaChroma[COEF_A].chroma = MediumHorizontalCoefficientsChromaA;
      m_horizontalLumaChroma[COEF_B].luma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_B].chroma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_C].luma = MediumHorizontalCoefficientsLumaC;
      m_horizontalLumaChroma[COEF_C].chroma = MediumHorizontalCoefficientsChromaC;
      m_horizontalLumaChroma[COEF_D].luma = MediumHorizontalCoefficientsLumaD;
      m_horizontalLumaChroma[COEF_D].chroma = MediumHorizontalCoefficientsChromaD;
      m_horizontalLumaChroma[COEF_E].luma = MediumHorizontalCoefficientsLumaE;
      m_horizontalLumaChroma[COEF_E].chroma = MediumHorizontalCoefficientsChromaE;
      m_horizontalLumaChroma[COEF_F].luma = MediumHorizontalCoefficientsLumaF;
      m_horizontalLumaChroma[COEF_F].chroma = MediumHorizontalCoefficientsChromaF;
      m_verticalBoundaries = MediumVerticalBoundaries;
      m_verticalLumaChroma[COEF_X2].chroma = 0;
      m_verticalLumaChroma[COEF_A].luma = MediumVerticalCoefficientsLumaA;
      m_verticalLumaChroma[COEF_A].chroma = MediumVerticalCoefficientsChromaA;
      m_verticalLumaChroma[COEF_B].luma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_B].chroma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_C].luma = MediumVerticalCoefficientsLumaC;
      m_verticalLumaChroma[COEF_C].chroma = MediumVerticalCoefficientsChromaC;
      m_verticalLumaChroma[COEF_D].luma = MediumVerticalCoefficientsLumaD;
      m_verticalLumaChroma[COEF_D].chroma = MediumVerticalCoefficientsChromaD;
      m_verticalLumaChroma[COEF_E].luma = MediumVerticalCoefficientsLumaE;
      m_verticalLumaChroma[COEF_E].chroma = MediumVerticalCoefficientsChromaE;
      m_verticalLumaChroma[COEF_F].luma = MediumVerticalCoefficientsLumaF;
      m_verticalLumaChroma[COEF_F].chroma = MediumVerticalCoefficientsChromaF;
      m_verticalLumaChroma[COEF_G].luma = MediumVerticalCoefficientsLumaG;
      m_verticalLumaChroma[COEF_H].luma = MediumVerticalCoefficientsLumaH;
      m_verticalLumaChroma[COEF_I].luma = MediumVerticalCoefficientsLumaI;
      m_verticalLumaChroma[COEF_J].luma = MediumVerticalCoefficientsLumaJ;
      m_verticalLumaChroma[COEF_K].luma = MediumVerticalCoefficientsLumaK;
      m_verticalLumaChroma[COEF_L].luma = MediumVerticalCoefficientsLumaL;
      m_maxVerticalLumaCoef = COEF_L;
      break;

    case FILTER_SET_SHARP :
      TRC( TRC_ID_VDP, "SHARP filter set selected" );
      m_horizontalBoundaries = SharpHorizontalBoundaries;
      m_horizontalLumaChroma[COEF_X2].chroma = 0;
      m_horizontalLumaChroma[COEF_A].luma = SharpHorizontalCoefficientsLumaA;
      m_horizontalLumaChroma[COEF_A].chroma = SharpHorizontalCoefficientsChromaA;
      m_horizontalLumaChroma[COEF_B].luma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_B].chroma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_C].luma = SharpHorizontalCoefficientsLumaC;
      m_horizontalLumaChroma[COEF_C].chroma = SharpHorizontalCoefficientsChromaC;
      m_horizontalLumaChroma[COEF_D].luma = SharpHorizontalCoefficientsLumaD;
      m_horizontalLumaChroma[COEF_D].chroma = SharpHorizontalCoefficientsChromaD;
      m_horizontalLumaChroma[COEF_E].luma = SharpHorizontalCoefficientsLumaE;
      m_horizontalLumaChroma[COEF_E].chroma = SharpHorizontalCoefficientsChromaE;
      m_horizontalLumaChroma[COEF_F].luma = SharpHorizontalCoefficientsLumaF;
      m_horizontalLumaChroma[COEF_F].chroma = SharpHorizontalCoefficientsChromaF;
      m_verticalBoundaries = SharpVerticalBoundaries;
      m_verticalLumaChroma[COEF_X2].chroma = 0;
      m_verticalLumaChroma[COEF_A].luma = SharpVerticalCoefficientsLumaA;
      m_verticalLumaChroma[COEF_A].chroma = SharpVerticalCoefficientsChromaA;
      m_verticalLumaChroma[COEF_B].luma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_B].chroma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_C].luma = SharpVerticalCoefficientsLumaC;
      m_verticalLumaChroma[COEF_C].chroma = SharpVerticalCoefficientsChromaC;
      m_verticalLumaChroma[COEF_D].luma = SharpVerticalCoefficientsLumaD;
      m_verticalLumaChroma[COEF_D].chroma = SharpVerticalCoefficientsChromaD;
      m_verticalLumaChroma[COEF_E].luma = SharpVerticalCoefficientsLumaE;
      m_verticalLumaChroma[COEF_E].chroma = SharpVerticalCoefficientsChromaE;
      m_verticalLumaChroma[COEF_F].luma = SharpVerticalCoefficientsLumaF;
      m_verticalLumaChroma[COEF_F].chroma = SharpVerticalCoefficientsChromaF;
      m_verticalLumaChroma[COEF_G].luma = SharpVerticalCoefficientsLumaG;
      m_verticalLumaChroma[COEF_H].luma = SharpVerticalCoefficientsLumaH;
      m_verticalLumaChroma[COEF_I].luma = SharpVerticalCoefficientsLumaI;
      m_verticalLumaChroma[COEF_J].luma = SharpVerticalCoefficientsLumaJ;
      m_verticalLumaChroma[COEF_K].luma = SharpVerticalCoefficientsLumaK;
      m_verticalLumaChroma[COEF_L].luma = SharpVerticalCoefficientsLumaL;
      m_maxVerticalLumaCoef = COEF_L;
      break;

    case FILTER_SET_SMOOTH :
      TRC( TRC_ID_VDP, "SMOOTH filter set selected" );
      m_horizontalBoundaries = SmoothHorizontalBoundaries;
      m_horizontalLumaChroma[COEF_X2].chroma = 0;
      m_horizontalLumaChroma[COEF_A].luma = SmoothHorizontalCoefficientsLumaA;
      m_horizontalLumaChroma[COEF_A].chroma = SmoothHorizontalCoefficientsChromaA;
      m_horizontalLumaChroma[COEF_B].luma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_B].chroma = HorizontalCoefficientsB;
      m_horizontalLumaChroma[COEF_C].luma = SmoothHorizontalCoefficientsLumaC;
      m_horizontalLumaChroma[COEF_C].chroma = SmoothHorizontalCoefficientsChromaC;
      m_horizontalLumaChroma[COEF_D].luma = SmoothHorizontalCoefficientsLumaD;
      m_horizontalLumaChroma[COEF_D].chroma = SmoothHorizontalCoefficientsChromaD;
      m_horizontalLumaChroma[COEF_E].luma = SmoothHorizontalCoefficientsLumaE;
      m_horizontalLumaChroma[COEF_E].chroma = SmoothHorizontalCoefficientsChromaE;
      m_horizontalLumaChroma[COEF_F].luma = SmoothHorizontalCoefficientsLumaF;
      m_horizontalLumaChroma[COEF_F].chroma = SmoothHorizontalCoefficientsChromaF;
      m_verticalBoundaries = SmoothVerticalBoundaries;
      m_verticalLumaChroma[COEF_X2].chroma = 0;
      m_verticalLumaChroma[COEF_A].luma = SmoothVerticalCoefficientsLumaA;
      m_verticalLumaChroma[COEF_A].chroma = SmoothVerticalCoefficientsChromaA;
      m_verticalLumaChroma[COEF_B].luma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_B].chroma = VerticalCoefficientsB;
      m_verticalLumaChroma[COEF_C].luma = SmoothVerticalCoefficientsLumaC;
      m_verticalLumaChroma[COEF_C].chroma = SmoothVerticalCoefficientsChromaC;
      m_verticalLumaChroma[COEF_D].luma = SmoothVerticalCoefficientsLumaD;
      m_verticalLumaChroma[COEF_D].chroma = SmoothVerticalCoefficientsChromaD;
      m_verticalLumaChroma[COEF_E].luma = SmoothVerticalCoefficientsLumaE;
      m_verticalLumaChroma[COEF_E].chroma = SmoothVerticalCoefficientsChromaE;
      m_verticalLumaChroma[COEF_F].luma = SmoothVerticalCoefficientsLumaF;
      m_verticalLumaChroma[COEF_F].chroma = SmoothVerticalCoefficientsChromaF;
      m_verticalLumaChroma[COEF_G].luma = SmoothVerticalCoefficientsLumaG;
      m_verticalLumaChroma[COEF_H].luma = SmoothVerticalCoefficientsLumaH;
      m_verticalLumaChroma[COEF_I].luma = SmoothVerticalCoefficientsLumaI;
      m_verticalLumaChroma[COEF_J].luma = SmoothVerticalCoefficientsLumaJ;
      m_verticalLumaChroma[COEF_K].luma = SmoothVerticalCoefficientsLumaK;
      m_verticalLumaChroma[COEF_L].luma = SmoothVerticalCoefficientsLumaL;
      m_maxVerticalLumaCoef = COEF_L;
      break;
    }

  if ( not CreateVC1RangeMapping( filterSet ))
    {
      TRC( TRC_ID_ERROR, "SelectFilterSet %d KO", filterSet );
      return false;
    }

  m_filterSet = filterSet;

  TRC( TRC_ID_MAIN_INFO, "SelectFilterSet %d OK", filterSet );
  return true;
}


bool CVdpFilter::GetFilterSet( stm_plane_filter_set_t * filterSet ) const
{
  *filterSet = m_filterSet;
  return true;
}


static void modifyVC1Tables( DMA_Area *vc1RangeMapping, uint8_t nbVerticalTables )
{
  uint8_t range, i;
  uint32_t set_offset;

  for ( range = 0; range < NB_VC1_RANGE_MAP; range++ )
    {
      set_offset = VFC_NB_COEFS * nbVerticalTables * range;

      for ( i = 0; i < nbVerticalTables; i++ )
        {
          uint32_t *coeffs = (uint32_t *)( vc1RangeMapping->pData + set_offset + ( VFC_NB_COEFS * i ));
          uint32_t div = ( coeffs[20] >> 25 ) & 0x3;
          int32_t offset_0_4, offset_1_3, offset_2;

          offset_0_4 = coeffs[21] & 0x1ff;

          if ( coeffs[21] & 0x200 )
            {
              offset_0_4 |= 0xfffffe00;
            }

          offset_1_3 = ( coeffs[21] >> 11 ) & 0x1ff;

          if ( coeffs[21] & 0x00100000 )
            {
              offset_1_3 |= 0xfffffe00;
            }

          offset_2 = ( coeffs[21] >> 22 ) & 0x1ff;

          if ( coeffs[21] & 0x80000000 )
            {
              offset_2 |= 0xfffffe00;
            }

          if ( div == 0 )
            {
              // Scale up offset by range factor. This only works if the offsets in
              // the filters are small and don't go out of range when multiplied
              // by two. Thankfully all the filters with a div == 0 meet this
              // requirement, except for the filter for no scaling, hence the
              // adjustment on the offset for the center filter tap (2).
              offset_0_4 = ( offset_0_4 * ( range * 64 + 576 )) / 512;
              offset_1_3 = ( offset_1_3 * ( range * 64 + 576 )) / 512;
              offset_2 = ( offset_2 * ( range * 64 + 576 )) / 512;

              if ( offset_2 > 511 )
                {
                  offset_2 = 511; /* Make sure we don't overflow in the hardware */
                }

              // Use the shift to multiply the coefficients by two
              coeffs[20] |= 0x01000000;
              coeffs[21] = ( offset_0_4 & 0x3ff ) | (( offset_1_3 & 0x3ff ) << 11 ) | (( offset_2 & 0x3ff ) << 22 ) | 0x00200400;
            }
          else
            {
              /*
               * We can use the divide to implicitly multiply both the coefficients
               * and offset by 2
               */
              coeffs[20] &= ~( 0x3 << 25 );
              coeffs[20] |= ( div - 1 ) << 25;
              /*
               * So we now scale the offsets by 1/2 the range factor. This means they can't go out of range.
               */
              offset_0_4 = ( offset_0_4 * ( range * 64 + 576 )) / 1024;
              offset_1_3 = ( offset_1_3 * ( range * 64 + 576 )) / 1024;
              offset_2 = ( offset_2 * ( range * 64 + 576 )) / 1024;;

              coeffs[21] = ( offset_0_4 & 0x3ff ) | (( offset_1_3 & 0x3ff ) << 11 ) | (( offset_2 & 0x3ff ) << 22 );
            }

          /*
           * Now scale down all the coefficients by 1/2 the range factor
           */
          for ( int j = 0; j < 21; j++ )
            {
              int32_t k1, k2, k3, k4;

              k1 = (signed char)( coeffs[j] & 0xff );
              k2 = (signed char)(( coeffs[j] >> 8 ) & 0xff );
              k3 = (signed char)(( coeffs[j] >> 16 ) & 0xff );
              k4 = (signed char)(( coeffs[j] >> 24 ) & 0xff );

              k1 = ( k1 * ( range * 64 + 576 )) / 1024;
              k2 = ( k2 * ( range * 64 + 576 )) / 1024;
              k3 = ( k3 * ( range * 64 + 576 )) / 1024;

              if ( j != 20 )
                {
                  k4 = ( k4 * ( range * 64 + 576 )) / 1024;
                }

              coeffs[j] = (( k4 & 0xff ) << 24 ) | (( k3 & 0xff ) << 16 ) | (( k2 & 0xff ) << 8 ) | ( k1 & 0xff );
            }
        }
    }
}


bool CVdpFilter::CreateVC1RangeMapping( stm_plane_filter_set_t filterSet )
{
  uint8_t i, range, iLuma, iChroma;
  uint32_t setOffsetLuma, setOffsetChroma;

  // free vc1 range mapping tables if already allocated
  if ( m_lumaVC1RangeMapping.pMemory != 0 )
    {
      vibe_os_free_dma_area( &m_lumaVC1RangeMapping );
    }

  if ( m_chromaVC1RangeMapping.pMemory != 0 )
    {
      vibe_os_free_dma_area( &m_chromaVC1RangeMapping );
    }

  // compute vertical table number
  m_nbLumaVerticalTables = 0;
  m_nbChromaVerticalTables = 0;

  for ( i = 0; i <= m_maxVerticalLumaCoef; i++ )
    {
      if ( m_verticalLumaChroma[i].luma != 0 )
        {
          m_nbLumaVerticalTables++;
        }

      if ( m_verticalLumaChroma[i].chroma != 0 )
        {
          m_nbChromaVerticalTables++;
        }
    }

  // allocate vc1 range mapping tables
  vibe_os_allocate_dma_area( &m_lumaVC1RangeMapping, VFC_NB_COEFS * m_nbLumaVerticalTables * NB_VC1_RANGE_MAP, 0, SDAAF_NONE );

  if ( m_lumaVC1RangeMapping.pMemory == 0 )
    {
      return false;
    }

  vibe_os_allocate_dma_area( &m_chromaVC1RangeMapping, VFC_NB_COEFS * m_nbChromaVerticalTables * NB_VC1_RANGE_MAP, 0, SDAAF_NONE );

  if ( m_chromaVC1RangeMapping.pMemory == 0 )
    {
      return false;
    }

  // copy tables
  for ( range = 0; range < NB_VC1_RANGE_MAP; range++ )
    {
      setOffsetLuma = VFC_NB_COEFS * m_nbLumaVerticalTables * range;
      setOffsetChroma = VFC_NB_COEFS * m_nbChromaVerticalTables * range;
      iLuma = 0;
      iChroma = 0;

      for ( int i = 0; i <= m_maxVerticalLumaCoef; i++ )
        {
          if ( m_verticalLumaChroma[i].luma != 0 )
            {
              vibe_os_memcpy_to_dma_area( &m_lumaVC1RangeMapping, setOffsetLuma + ( VFC_NB_COEFS * iLuma ), m_verticalLumaChroma[i].luma, VFC_NB_COEFS );
              iLuma++;
            }

          if ( m_verticalLumaChroma[i].chroma != 0 )
            {
              vibe_os_memcpy_to_dma_area( &m_chromaVC1RangeMapping, setOffsetChroma + ( VFC_NB_COEFS * iChroma ), m_verticalLumaChroma[i].chroma, VFC_NB_COEFS );
              iChroma++;
            }
        }
    }

  // modify tables according to vc1 range
  modifyVC1Tables( &m_lumaVC1RangeMapping, m_nbLumaVerticalTables );
  modifyVC1Tables( &m_chromaVC1RangeMapping, m_nbChromaVerticalTables );
  return true;
}


const uint32_t *CVdpFilter::SelectHorizontalLumaFilter(uint32_t scaleFactor, uint32_t phase)
{
  const void *luma;

  if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_E_F] )
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_F");
      luma = m_horizontalLumaChroma[COEF_F].luma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_D_E] )
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_E");
      luma = m_horizontalLumaChroma[COEF_E].luma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_C_D] )
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_D");
      luma = m_horizontalLumaChroma[COEF_D].luma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_A_C] )
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_C");
      luma = m_horizontalLumaChroma[COEF_C].luma;
    }
  else if (( scaleFactor == m_horizontalBoundaries[BOUNDARY_A_C] ) && ( phase == 0 )) // No scaling or subpixel pan/scan
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_B");
      luma = m_horizontalLumaChroma[COEF_B].luma;
    }
  else
    {
      TRC( TRC_ID_MAIN_INFO, "HY filter: COEF_A");
      luma = m_horizontalLumaChroma[COEF_A].luma;       // zoom in or subpixel pan/scan at 1x
    }

  return (const uint32_t *)luma;
}


const uint32_t *CVdpFilter::SelectHorizontalChromaFilter(uint32_t scaleFactor, uint32_t phase)
{
  const void *chroma;

  if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_E_F] )
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_F");
      chroma = m_horizontalLumaChroma[COEF_F].chroma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_D_E] )
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_E");
      chroma = m_horizontalLumaChroma[COEF_E].chroma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_C_D] )
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_D");
      chroma = m_horizontalLumaChroma[COEF_D].chroma;
    }
  else if ( scaleFactor > m_horizontalBoundaries[BOUNDARY_A_C] )
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_C");
      chroma = m_horizontalLumaChroma[COEF_C].chroma;
    }
  else if (( scaleFactor == m_horizontalBoundaries[BOUNDARY_A_C] ) && ( phase == 0 )) // No scaling or subpixel pan/scan
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_B");
      chroma = m_horizontalLumaChroma[COEF_B].chroma;
    }
  else
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_A");
      chroma = m_horizontalLumaChroma[COEF_A].chroma;       // zoom in or subpixel pan/scan at 1x
    }

  // specific to legacy filter set
  if (( m_filterSet == FILTER_SET_LEGACY ) and ( scaleFactor == ZOOMx2 ))
    {
      TRC( TRC_ID_MAIN_INFO, "HC filter: COEF_X2");
      chroma = m_horizontalLumaChroma[COEF_X2].chroma;
    }

  return (const uint32_t *)chroma;
}


const uint32_t *CVdpFilter::SelectVerticalLumaFilter(uint32_t scaleFactor, uint32_t phase)
{
  const void *luma = 0;

  if ( m_maxVerticalLumaCoef == COEF_L ) // only for medium, sharp or smooth
    {
      if ( scaleFactor > m_verticalBoundaries[BOUNDARY_K_L] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_L");
          luma = m_verticalLumaChroma[COEF_L].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_J_K] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_K");
          luma = m_verticalLumaChroma[COEF_K].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_I_J] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_J");
          luma = m_verticalLumaChroma[COEF_J].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_H_I] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_I");
          luma = m_verticalLumaChroma[COEF_I].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_G_H] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_H");
          luma = m_verticalLumaChroma[COEF_H].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_F_G] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_G");
          luma = m_verticalLumaChroma[COEF_G].luma;
        }
    }

  if ( luma == 0 ) // not set before
    {
      if ( scaleFactor > m_verticalBoundaries[BOUNDARY_E_F] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_F");
          luma = m_verticalLumaChroma[COEF_F].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_D_E] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_E");
          luma = m_verticalLumaChroma[COEF_E].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_C_D] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_D");
          luma = m_verticalLumaChroma[COEF_D].luma;
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_A_C] )
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_C");
          luma = m_verticalLumaChroma[COEF_C].luma;
        }
      else if (( scaleFactor == m_verticalBoundaries[BOUNDARY_A_C] ) && ( phase == 0x1000 )) // No scaling or subpixel pan/scan
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_B");
          luma = m_verticalLumaChroma[COEF_B].luma;
        }
      else
        {
          TRC( TRC_ID_MAIN_INFO, "VY filter: COEF_A");
          luma = m_verticalLumaChroma[COEF_A].luma; // zoom in or subpixel pan/scan at 1x
        }
    }

  return (const uint32_t *)luma;
}


const uint32_t *CVdpFilter::SelectVerticalChromaFilter(uint32_t scaleFactor, uint32_t phase)
{
  const void *chroma;

  if ( scaleFactor > m_verticalBoundaries[BOUNDARY_E_F] )
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_F");
      chroma = m_verticalLumaChroma[COEF_F].chroma;
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_D_E] )
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_E");
      chroma = m_verticalLumaChroma[COEF_E].chroma;
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_C_D] )
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_D");
      chroma = m_verticalLumaChroma[COEF_D].chroma;
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_A_C] )
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_C");
      chroma = m_verticalLumaChroma[COEF_C].chroma;
    }
  else if (( scaleFactor == m_verticalBoundaries[BOUNDARY_A_C] ) && ( phase == 0x1000 )) // No scaling or subpixel pan/scan
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_B");
      chroma = m_verticalLumaChroma[COEF_B].chroma;
    }
  else
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_A");
      chroma = m_verticalLumaChroma[COEF_A].chroma;       // zoom in or subpixel pan/scan at 1x
    }

  // specific to legacy filter set
  if (( m_filterSet == FILTER_SET_LEGACY ) and ( scaleFactor == ZOOMx2 ))
    {
      TRC( TRC_ID_MAIN_INFO, "VC filter: COEF_X2");
      chroma = m_verticalLumaChroma[COEF_X2].chroma;
    }

  return (const uint32_t *)chroma;
}


const uint32_t *CVdpFilter::SelectVC1VerticalLumaFilter(uint32_t scaleFactor, uint32_t phase, uint32_t vc1type)
{
  const void *luma = 0;
  char *pData;
  uint8_t i;

  if ( vc1type > STM_BUFFER_SRC_VC1_RANGE_MAP_MAX )
    {
      return SelectVerticalLumaFilter( scaleFactor, phase );
    }

  pData = m_lumaVC1RangeMapping.pData + ( VFC_NB_COEFS * m_nbLumaVerticalTables * vc1type );
  i = m_nbLumaVerticalTables;

  if ( m_maxVerticalLumaCoef == COEF_L ) // only for medium, sharp or smooth
    {
      if ( scaleFactor > m_verticalBoundaries[BOUNDARY_K_L] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 1 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_J_K] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 2 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_I_J] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 3 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_H_I] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 4 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_G_H] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 5 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_F_G] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 6 )));
        }
      else
        {
          i = i - 6;
        }
    }

  if ( luma == 0 ) // not set before
    {
      if ( scaleFactor > m_verticalBoundaries[BOUNDARY_E_F] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 1 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_D_E] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 2 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_C_D] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 3 )));
        }
      else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_A_C] )
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 4 )));
        }
      else if (( scaleFactor == m_verticalBoundaries[BOUNDARY_A_C] ) && ( phase == 0x1000 )) // No scaling or subpixel pan/scan
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 5 )));
        }
      else
        {
          luma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 6 ))); // zoom in or subpixel pan/scan at 1x
        }
    }

  return (const uint32_t *)luma;
}


const uint32_t *CVdpFilter::SelectVC1VerticalChromaFilter(uint32_t scaleFactor,uint32_t phase, uint32_t vc1type)
{
  const void *chroma;
  char *pData;
  uint8_t i;

  if ( vc1type > STM_BUFFER_SRC_VC1_RANGE_MAP_MAX )
    {
      return SelectVerticalChromaFilter( scaleFactor, phase );
    }

  pData = m_chromaVC1RangeMapping.pData + ( VFC_NB_COEFS * m_nbChromaVerticalTables * vc1type );
  i = m_nbChromaVerticalTables;

  if ( scaleFactor > m_verticalBoundaries[BOUNDARY_E_F] )
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 1 )));
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_D_E] )
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 2 )));
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_C_D] )
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 3 )));
    }
  else if ( scaleFactor > m_verticalBoundaries[BOUNDARY_A_C] )
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 4 )));
    }
  else if (( scaleFactor == m_verticalBoundaries[BOUNDARY_A_C] ) && ( phase == 0x1000 )) // No scaling or subpixel pan/scan
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 5 )));
    }
  else
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 6 ))); // zoom in or subpixel pan/scan at 1x
    }

  // specific to legacy filter set
  if (( m_filterSet == FILTER_SET_LEGACY ) and ( scaleFactor == ZOOMx2 ))
    {
      chroma = (uint32_t *)( pData + ( VFC_NB_COEFS * ( i - 7 )));
    }

  return (const uint32_t *)chroma;
}
