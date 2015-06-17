/***********************************************************************
 *
 * File: display/ip/stmiqile.cpp
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>


#include "stmiqile.h"


/* LUMA LUT */
#define LUMA_LUT_SIZE     (128 * sizeof (int16_t)) /* maximum size of IQI Luma LUT enhancer */
#define VDP_IQI_BLACKD1   ( 64)    /* Default value for AutoCurve LE algo */
#define VDP_IQI_WHITED1   (940)    /* Default value for AutoCurve LE algo */
#define VDP_IQI_LUT_10BITS_SIZE               (1024)  /* Compute LUT on 10 bits         */
#define VDP_IQI_LUT_7BITS_SIZE                ( 128)  /* Compute LUT on 7 bits (for HW) */
#define VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR  (   8)  /* Scale from VDP_IQI_LUT_10BITS_SIZE
                                                         to VDP_IQI_LUT_7BITS_SIZE      */


static const int16_t CustomCurveGamma1_02[LUMA_LUT_SIZE / sizeof (int16_t)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -2, -2, -2, -3, -3, -3, -4, -4, -4, -4, -4, -5,
-5, -5, -5, -5, -5, -5, -5, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6,
-6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6,
-6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -4, -4, -4,
-4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2,
-1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static const int16_t CustomCurveGamma1_03[LUMA_LUT_SIZE / sizeof (int16_t)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -3, -4, -4, -4, -5, -5, -6, -6, -6, -6, -7,
-7, -7, -7, -8, -8, -8, -8, -8, -8, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
-9, -9, -10, -10, -10, -10, -10, -10, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
-9, -9, -9, -9, -9, -9, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7,
-7, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -3, -3, -3, -3,
-3, -2, -2, -2, -2, -2, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static const int16_t CustomCurveGamma1_04[LUMA_LUT_SIZE / sizeof (int16_t)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -5, -5, -6, -6, -7, -7, -8, -8, -9, -9,
-9, -10, -10, -10, -10, -11, -11, -11, -11, -11, -12, -12, -12, -12, -12, -12, -12,
-12, -12, -12, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -12, -12,
-12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -11, -11, -11, -11, -11, -11, -11,
-11, -10, -10, -10, -10, -10, -9, -9, -9, -9, -9, -8, -8, -8, -8, -8, -7, -7, -7,
-7, -6, -6, -6, -6, -5, -5, -5, -5, -4, -4, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
-1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };




static const struct _IQILEConf IQI_LE_ConfigParams[PCIQIC_COUNT] = {
  /* disable */
  {
           csc_gain    : 0,
           fixed_curve : false,
           fixed_curve_params : { BlackStretchInflexionPoint : 0,
                                  BlackStretchLimitPoint     : 0,
                                  WhiteStretchInflexionPoint : 0,
                                  WhiteStretchLimitPoint     : 0,
                                  BlackStretchGain           : 0,
                                  WhiteStretchGain           : 0 },
           custom_curve : 0
   },

  /* soft */
  {
           csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  10,
                                  WhiteStretchGain           :  10 },
           custom_curve : 0
  },

  /* medium */
  {
           csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  15,
                                  WhiteStretchGain           :  15 },
           custom_curve : CustomCurveGamma1_02
  },

  /* strong */
  {
           csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  20,
                                  WhiteStretchGain           :  20 },
           custom_curve : CustomCurveGamma1_03
  }
};




CSTmIQILe::CSTmIQILe (void)
{
  m_LumaLUT = (uint16_t*)NULL;
  m_preset = PCIQIC_FIRST;
  m_current_config = IQI_LE_ConfigParams[PCIQIC_FIRST];
}


CSTmIQILe::~CSTmIQILe (void)
{
  vibe_os_free_memory (m_LumaLUT);
}


bool
CSTmIQILe::Create (void)
{


  m_LumaLUT = (uint16_t*)vibe_os_allocate_memory (LUMA_LUT_SIZE);
  if (UNLIKELY (!m_LumaLUT))
    {
      TRC( TRC_ID_ERROR, " 'out of memory'");
      return false;
    }

  m_current_config = IQI_LE_ConfigParams[PCIQIC_FIRST];
  m_preset = PCIQIC_FIRST;

  return true;
}



bool
CSTmIQILe::GenerateFixedCurveTable (const struct _IQILEConfFixedCurveParams * const params,
                                  int16_t                                 * const curve) const
{
  /* B&W Limits */
  uint32_t BSLim;
  uint32_t WSLim;
  uint32_t BlackD1;
  uint32_t WhiteD1;
  /* Black Clip setup */
  uint32_t xClipBS;
  uint32_t yClipBS;
  /* Black Limit */
  int32_t  SlopeBSLim;
  int32_t  OriginBSLim;
  /* White X&Y Clip */
  uint32_t xClipWS;
  int32_t  yClipWS;
  /* White Limit */
  int32_t  SlopeWSLim;

  /* From Black to Black inflection */
  /* new algo from application team (CR5)
     values like 128, 880, etc. are not replaced by defines (to keep the
     algo clear) */

  /* B & W limits */
  BSLim   = params->BlackStretchLimitPoint;
  WSLim   = params->WhiteStretchLimitPoint;
  BlackD1 = VDP_IQI_BLACKD1;
  WhiteD1 = VDP_IQI_WHITED1;

  { /* black clip setup */
  uint32_t xClipBS1 =
    (uint32_t) (BSLim
               + (int32_t) (((uint32_t) (params->BlackStretchGain * 8)
                            * (uint32_t) params->BlackStretchInflexionPoint)
                           / 1024));
  uint32_t xClipBS2 = (uint32_t) ((1024 + (uint32_t) (params->BlackStretchGain * 8)));

  xClipBS = (uint32_t) (xClipBS1 * 1024) / xClipBS2;
  yClipBS = xClipBS - BSLim;
  }

  { /* Black Limit */
  uint32_t SlopeBSLim1 = (uint32_t) (yClipBS * 1024);
  int32_t  SlopeBSLim2 = (int32_t) (BlackD1 - xClipBS);
  SlopeBSLim = ((SlopeBSLim2 == 0)
                ? 1
                : (int32_t) (SlopeBSLim1) / SlopeBSLim2);
  OriginBSLim = (-BlackD1 * SlopeBSLim / 1024);
  }

  { /* White X&Y Clip */
  uint32_t xClipWS1 =
    (uint32_t) (WSLim
               + (int32_t) (((uint32_t) (params->WhiteStretchGain * 8)
                            * (uint32_t) params->WhiteStretchInflexionPoint)
                           / 1024));
  uint32_t xClipWS2 = (uint32_t) ((1024 + (uint32_t) (params->WhiteStretchGain * 8)));

  xClipWS = (uint32_t) (xClipWS1 * 1024) / xClipWS2;
  yClipWS = (int32_t) (WSLim - xClipWS);
  }

  { /* White Limit */
  uint32_t SlopeWSLim1 = (uint32_t) (yClipWS * 1024);
  int32_t  SlopeWSLim2 = (int32_t) (xClipWS - WhiteD1);

  SlopeWSLim = ((SlopeWSLim2 == 0)
                ? 1
                : (int32_t) (SlopeWSLim1) / SlopeWSLim2);
  }

  /* First generate Fixed curve */

  /* --- Check parameters...  */
  if (BlackD1 <= xClipBS
      && xClipBS <= params->BlackStretchInflexionPoint
      && params->BlackStretchInflexionPoint <= params->WhiteStretchInflexionPoint
      && params->WhiteStretchInflexionPoint <= xClipWS
      && xClipWS <= WhiteD1
      && WhiteD1 <= VDP_IQI_LUT_10BITS_SIZE)
    {
      int32_t Point;

      /* A loop on all values*/
      for (uint32_t Loop = 0;
           Loop < VDP_IQI_LUT_10BITS_SIZE;
           Loop += VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR)
        {
          if (Loop <= BlackD1)
            /* From 0 to BlackD1 => Linear LUT*/
            curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;
          else if (Loop < xClipBS)
            {
              /* From BlackD1 to xClipBS */
              Point = (int32_t) ((int32_t) Loop * (int32_t) SlopeBSLim / 1024
                                + (int32_t) OriginBSLim);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (int16_t) Point;
            }
          else if (Loop < params->BlackStretchInflexionPoint)
            {
              /* From xClipBS to Black inflection */
              Point = (int32_t) ((((int32_t) Loop
                                  - (int32_t) params->BlackStretchInflexionPoint)
                                 * (int32_t) (params->BlackStretchGain * 8)
                                 + 512) / 1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (int16_t) Point;
            }
          else if (Loop < params->WhiteStretchInflexionPoint)
            {
              /* From Black inflection to White inflection */
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;
            }
          else if (Loop < xClipWS)
            {
              /* From White inflection to xClipWS */
              Point = (int32_t) ((((int32_t) Loop
                                  - (int32_t) params->WhiteStretchInflexionPoint)
                                 * (int32_t) (params->WhiteStretchGain * 8)
                                 + 512) / 1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (int16_t) Point;
            }
          else if (Loop < WhiteD1)
            {
              /* From xClipWS to WhiteD1 */
              Point = (int32_t) (yClipWS
                                + SlopeWSLim * ((int32_t) Loop
                                              - (int32_t) xClipWS)
                                /1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (int16_t) Point;
            }
          else
            {
              /* From WhiteD1 to full white */
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;
            }

        }
    }
  else
    {
      /* Clean LUT table (which must be done to avoid visual effects when
         values are wrong) */
      for (uint32_t Loop = 0;
           Loop < VDP_IQI_LUT_10BITS_SIZE;
           Loop += VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR)
        curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;

      return false;
    }

  return true;
}

bool
CSTmIQILe::GenerateLeLUT (const struct _IQILEConf * const le,
                        uint16_t                       *LutToPrg) const
{
  bool   success = true;
  int16_t  LutGainHL[LUMA_LUT_SIZE / sizeof (int16_t)];
  uint8_t  Index;
  uint16_t IdentityValue;

  /* First generate fixed curve table if needed */
  if (le->fixed_curve)
    success = GenerateFixedCurveTable (&le->fixed_curve_params, LutGainHL);



  /* Now, sum fixed and gamma curves to the identity curve */
  if (success)
    {
      for (unsigned int i = 0;
           i < VDP_IQI_LUT_10BITS_SIZE;
           i += VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR)
        {
          Index = i / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR;
          /* Ranges are on 10 bits (1024) */
          IdentityValue = (uint16_t) ((Index * (VDP_IQI_LUT_10BITS_SIZE - 1))
                                      / (VDP_IQI_LUT_7BITS_SIZE - 1));

          /* identity curve */
          LutToPrg[Index] = IdentityValue;

          if (le->fixed_curve)
            /* Fixed curve is centered aroud zero */
            LutToPrg[Index] += LutGainHL[Index];

          if (le->custom_curve)
            /* custom curve is also centered around zero */
            LutToPrg[Index] += le->custom_curve[Index];

          /* Manage clipping */
          if (LutToPrg[Index] >= (VDP_IQI_LUT_10BITS_SIZE))
            LutToPrg[Index] = VDP_IQI_LUT_10BITS_SIZE - 1;
        }
    }

  return success;
}

void
CSTmIQILe::CalculateSetup ( CDisplayNode * node, struct IQILeSetup * setup)
{

   setup->le.is_709_colorspace
    = (node->m_bufferDesc.src.flags & STM_BUFFER_SRC_COLORSPACE_709) ? true : false;

   setup->le.lce_lut_init = true;

   setup->le.enable_csc =  (m_current_config.csc_gain) ? true : false;
}


void CSTmIQILe::CalculateParams(struct IQILeSetup * setup, HQVDPLITE_IQI_Params_t * params)
{
    params->LeConfig = ((setup->le.is_709_colorspace << IQI_LE_CONFIG_D1_601_NOT_709_SHIFT ) &  IQI_LE_CONFIG_D1_601_NOT_709_MASK)|
                         ((setup->le.lce_lut_init << IQI_LE_CONFIG_LCE_LUT_INIT_SHIFT) &  IQI_LE_CONFIG_LCE_LUT_INIT_MASK) |
                         ((m_current_config.csc_gain << IQI_LE_CONFIG_WEIGHT_GAIN_SHIFT) &  IQI_LE_CONFIG_WEIGHT_GAIN_MASK);

    for(int j=0; j<64; j++)
        params->LeLut[j] = ((m_LumaLUT[2*j] << IQI_LE_LUT_LUT_COEF_EVEN_SHIFT ) & IQI_LE_LUT_LUT_COEF_EVEN_MASK) |
                           ((m_LumaLUT[2*j+1] << IQI_LE_LUT_LUT_COEF_ODD_SHIFT ) & IQI_LE_LUT_LUT_COEF_ODD_MASK);
}





/* to be called on SetControl */
bool
CSTmIQILe::SetLeParams ( struct _IQILEConf * le)
{

  bool res=false;
  uint16_t *LutToPrg = reinterpret_cast <uint16_t *> (m_LumaLUT);

  if (!le
      || le->csc_gain > 31
      || (le->fixed_curve
          && (le->fixed_curve_params.BlackStretchInflexionPoint > VDP_IQI_LUT_10BITS_SIZE-1
              || le->fixed_curve_params.BlackStretchGain > 100
              || le->fixed_curve_params.BlackStretchLimitPoint > VDP_IQI_LUT_10BITS_SIZE-1
              || le->fixed_curve_params.WhiteStretchInflexionPoint > VDP_IQI_LUT_10BITS_SIZE-1
              || le->fixed_curve_params.WhiteStretchGain > 100
              || le->fixed_curve_params.WhiteStretchLimitPoint > VDP_IQI_LUT_10BITS_SIZE-1)))
    {
      TRC( TRC_ID_ERROR, "invalid IQI LE parameters");
      return false;
    }

    TRC( TRC_ID_HQVDPLITE, "le->csc_gain = %d", le->csc_gain );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve = %d", le->fixed_curve );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.BlackStretchInflexionPoint = %d", le->fixed_curve_params.BlackStretchInflexionPoint );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.BlackStretchGain = %d", le->fixed_curve_params.BlackStretchGain );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.BlackStretchLimitPoint = %d", le->fixed_curve_params.BlackStretchLimitPoint );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.WhiteStretchInflexionPoint = %d", le->fixed_curve_params.WhiteStretchInflexionPoint );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.WhiteStretchGain = %d", le->fixed_curve_params.WhiteStretchGain );
    TRC( TRC_ID_HQVDPLITE, "le->fixed_curve_params.WhiteStretchLimitPoint = %d", le->fixed_curve_params.WhiteStretchLimitPoint);

  /* we need to create a LUT */
  res = GenerateLeLUT (le, LutToPrg);
  if (res != false ) {

    //vibe_os_flush_dma_area (&m_LumaLUT, 0, LUMA_LUT_SIZE); //what to do with this?
    /* copy in m_current config */
    m_current_config = *le;
  }
  return res;

}

void CSTmIQILe::GetLeParams(struct _IQILEConf *  le)
{
    *le = m_current_config;
}



bool CSTmIQILe::SetLePreset(PlaneCtrlIQIConfiguration preset)
{

    uint16_t *LutToPrg = reinterpret_cast <uint16_t *> (m_LumaLUT);
    bool res=false;

    if (preset >=  PCIQIC_COUNT)
    {
      TRC( TRC_ID_ERROR, "invalid IQI cti preset" );
      return false;
    }
    m_current_config =  IQI_LE_ConfigParams[preset];
    m_preset = preset;
    TRC( TRC_ID_HQVDPLITE, "LE Preset = %d", preset );

    res = GenerateLeLUT (&m_current_config, LutToPrg);

    return res;
}

void CSTmIQILe::GetLePreset( enum PlaneCtrlIQIConfiguration * preset) const
{
    *preset = m_preset;

}

