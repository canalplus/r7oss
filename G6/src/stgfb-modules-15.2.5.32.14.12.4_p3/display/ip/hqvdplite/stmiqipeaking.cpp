/***********************************************************************
 *
 * File: display/ip/stmiqi.cpp
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


#include "stmiqipeaking.h"


#define PEAKING_COEFFX_MASK          (0x000003ff)





static const uint8_t VDP_IQI_PeakingHorGain[IQIPHVG_LAST + 1] = {
  /* IQIPHVG_N6_0DB  */  0,
  /* IQIPHVG_N5_5DB  */  1,
  /* IQIPHVG_N5_0DB  */  2,
  /* IQIPHVG_N4_5DB  */  3,
  /* IQIPHVG_N4_0DB  */  3,
  /* IQIPHVG_N3_5DB  */  4,
  /* IQIPHVG_N3_0DB  */  4,
  /* IQIPHVG_N2_5DB  */  5,
  /* IQIPHVG_N2_0DB  */  5,
  /* IQIPHVG_N1_5DB  */  6,
  /* IQIPHVG_N1_0DB  */  6,
  /* IQIPHVG_N0_5DB  */  7,
  /* IQIPHVG_0DB     */  8,
  /* IQIPHVG_P0_5DB  */  9,
  /* IQIPHVG_P1_0DB  */ 10,
  /* IQIPHVG_P1_5DB  */ 11,
  /* IQIPHVG_P2_0DB  */ 12,
  /* IQIPHVG_P2_5DB  */ 13,
  /* IQIPHVG_P3_0DB  */ 14,
  /* IQIPHVG_P3_5DB  */ 15,
  /* IQIPHVG_P4_0DB  */ 16,
  /* IQIPHVG_P4_5DB  */ 17,
  /* IQIPHVG_P5_0DB  */ 18,
  /* IQIPHVG_P5_5DB  */ 19,
  /* IQIPHVG_P6_0DB  */ 20,
  /* IQIPHVG_P6_5DB  */ 21,
  /* IQIPHVG_P7_0DB  */ 22,
  /* IQIPHVG_P7_5DB  */ 23,
  /* IQIPHVG_P8_0DB  */ 24,
  /* IQIPHVG_P8_5DB  */ 25,
  /* IQIPHVG_P9_0DB  */ 26,
  /* IQIPHVG_P9_5DB  */ 27,
  /* IQIPHVG_P10_0DB */ 28,
  /* IQIPHVG_P10_5DB */ 29,
  /* IQIPHVG_P11_0DB */ 30,
  /* IQIPHVG_P11_5DB */ 31,
  /* IQIPHVG_P12_0DB */ 31
};

static const uint8_t VDP_IQI_PeakingVertGain[IQIPHVG_LAST + 1] = {
  /* IQIPHVG_N6_0DB  */  0,
  /* IQIPHVG_N5_5DB  */  0,
  /* IQIPHVG_N5_0DB  */  0,
  /* IQIPHVG_N4_5DB  */  0,
  /* IQIPHVG_N4_0DB  */  0,
  /* IQIPHVG_N3_5DB  */  0,
  /* IQIPHVG_N3_0DB  */  0,
  /* IQIPHVG_N2_5DB  */  0,
  /* IQIPHVG_N2_0DB  */  1,
  /* IQIPHVG_N1_5DB  */  2,
  /* IQIPHVG_N1_0DB  */  3,
  /* IQIPHVG_N0_5DB  */  4,
  /* IQIPHVG_0DB     */  4,
  /* IQIPHVG_P0_5DB  */  4,
  /* IQIPHVG_P1_0DB  */  5,
  /* IQIPHVG_P1_5DB  */  5,
  /* IQIPHVG_P2_0DB  */  6,
  /* IQIPHVG_P2_5DB  */  6,
  /* IQIPHVG_P3_0DB  */  7,
  /* IQIPHVG_P3_5DB  */  7,
  /* IQIPHVG_P4_0DB  */  8,
  /* IQIPHVG_P4_5DB  */  9,
  /* IQIPHVG_P5_0DB  */ 10,
  /* IQIPHVG_P5_5DB  */ 11,
  /* IQIPHVG_P6_0DB  */ 12,
  /* IQIPHVG_P6_5DB  */ 13,
  /* IQIPHVG_P7_0DB  */ 14,
  /* IQIPHVG_P7_5DB  */ 15,
  /* IQIPHVG_P8_0DB  */ 15,
  /* IQIPHVG_P8_5DB  */ 15,
  /* IQIPHVG_P9_0DB  */ 15,
  /* IQIPHVG_P9_5DB  */ 15,
  /* IQIPHVG_P10_0DB */ 15,
  /* IQIPHVG_P10_5DB */ 15,
  /* IQIPHVG_P11_0DB */ 15,
  /* IQIPHVG_P11_5DB */ 15,
  /* IQIPHVG_P12_0DB */ 15
};

/* highpass filter coefficient for 9 taps peaking filter */
static const short VDP_IQI_HFilterCoefficientHP[IQIPFF_COUNT][5]=
{
#if 0
  { 358,-120, -55,  -8,   4  },  /* Correspond to 1.0MHZ   for 1H signal   (1.0/6.75  = 0.15) */
  { 326,-134, -40,   6,   5  },  /* Correspond to 1.25MHZ  for 1H signal   (1.25/6.75 = 0.18) */
  { 286,-147, -19,  19,   4  },  /* Correspond to 1.5MHZ   for 1H signal   (1.5/6.75  = 0.22) */
  { 246,-148,   6,  20,  -1  },  /* Correspond to 1.75MHZ  for 1H signal   (1.75/6.75 = 0.26) */
  /* extended size limit */
  { 358,-120, -55,  -8,   4  },  /* Correspond to 2.0MHZ   for 1H signal   (2.0/6.75  = 0.30) */
  { 342,-128, -48,   0,   5  },  /* Correspond to 2.25MHZ  for 1H signal   (2.25/6.75 = 0.33) */
  { 326,-134, -40,   6,   5  },  /* Correspond to 2.5MHZ   for 1H signal   (2.5/6.75  = 0.37) */
  { 304,-144, -31,  16,   7  },  /* Correspond to 2.75MHZ  for 1H signal   (2.75/6.75 = 0.40) */
  { 286,-147, -19,  19,   4  },  /* Correspond to 3.0MHZ   for 1H signal   (3.0/6.75  = 0.44) */
  { 266,-149,  -6,  21,   1  },  /* Correspond to 3.25MHZ  for 1H signal   (3.25/6.75 = 0.48) */
  { 246,-148,   6,  20,  -1  },  /* Correspond to 3.5MHZ   for 1H signal   (3.5/6.75  = 0.51) */
  { 226,-145,  18,  17,  -3  },  /* Correspond to 3.75MHZ  for 1H signal   (3.75/6.75 = 0.55} */
  { 206,-141,  30,  13,  -5  },  /* Correspond to 4.0MHZ   for 1H signal   (4.0/6.75  = 0.58) */
  { 188,-135,  40,   7,  -6  },  /* Correspond to 4.25MHZ  for 1H signal   (4.25/6.75 = 0.63) */
#else
  /* Filter coefficients to give 0.8 gain */
  { 288,  -96, -44,  -7,   3  },  /* Correspond to 1.0MHZ   for 1H signal   (1.0/6.75  = 0.15) */
  { 260, -107, -31,   5,   3  },  /* Correspond to 1.25MHZ  for 1H signal   (1.25/6.75 = 0.18) */
  { 230, -119, -16,  16,   4  },  /* Correspond to 1.5MHZ   for 1H signal   (1.5/6.75  = 0.22) */
  { 198, -119,   5,  16,  -1  },  /* Correspond to 1.75MHZ  for 1H signal   (1.75/6.75 = 0.26) */
  /* extended size limit */
  { 288,  -96, -44,  -7,   3  },  /* Correspond to 2.0MHZ   for 1H signal   (2.0/6.75  = 0.30) */
  { 274, -102, -38,   0,   3  },  /* Correspond to 2.25MHZ  for 1H signal   (2.25/6.75 = 0.33) */
  { 260, -107, -31,   5,   3  },  /* Correspond to 2.5MHZ   for 1H signal   (2.5/6.75  = 0.37) */
  { 244, -116, -27,  14,   7  },  /* Correspond to 2.75MHZ  for 1H signal   (2.75/6.75 = 0.40) */
  { 230, -119, -16,  16,   4  },  /* Correspond to 3.0MHZ   for 1H signal   (3.0/6.75  = 0.44) */
  { 214, -120,  -6,  18,   1  },  /* Correspond to 3.25MHZ  for 1H signal   (3.25/6.75 = 0.48) */
  { 198, -119,   5,  16,  -1  },  /* Correspond to 3.5MHZ   for 1H signal   (3.5/6.75  = 0.51) */
  { 182, -118,  15,  16,  -4  },  /* Correspond to 3.75MHZ  for 1H signal   (3.75/6.75 = 0.55} */
  { 166, -114,  25,  12,  -6  },  /* Correspond to 4.0MHZ   for 1H signal   (4.0/6.75  = 0.58) */
  { 150, -109,  33,   7,  -6  },  /* Correspond to 4.25MHZ  for 1H signal   (4.25/6.75 = 0.63) */
#endif
};
/* bandpass filter coefficient for 9 taps peaking filter */
static const short VDP_IQI_HFilterCoefficientBP[IQIPFF_COUNT][5]=
{
#if 0
  { 154,  87, -35, -85, -44  },  /* Correspond to 1.0MHZ   for 1H signal   (1.0/6.75  = 0.15) */
  { 192,  70, -92, -73,  -1  },  /* Correspond to 1.25MHZ  for 1H signal   (1.25/6.75 = 0.18) */
  { 222,  34,-124, -31,  10  },  /* Correspond to 1.5MHZ   for 1H signal   (1.5/6.75  = 0.22) */
  { 246, -12,-125,   8,   6  },  /* Correspond to 1.75MHZ  for 1H signal   (1.75/6.75 = 0.26) */
  /* extended size limit */
  { 154,  87, -35, -85, -44  },  /* Correspond to 2.0MHZ   for 1H signal   (2.0/6.75  = 0.30) */
  { 176,  82, -65, -85, -20  },  /* Correspond to 2.25MHZ  for 1H signal   (2.25/6.75 = 0.33) */
  { 192,  70, -92, -73,  -1  },  /* Correspond to 2.5MHZ   for 1H signal   (2.5/6.75  = 0.37) */
  { 208,  54,-112, -53,   7  },  /* Correspond to 2.75MHZ  for 1H signal   (2.75/6.75 = 0.40) */
  { 222,  34,-124, -31,  10  },  /* Correspond to 3.0MHZ   for 1H signal   (3.0/6.75  = 0.44) */
  { 234,  12,-129,  -9,   9  },  /* Correspond to 3.25MHZ  for 1H signal   (3.25/6.75 = 0.48) */
  { 246, -12,-125,   8,   6  },  /* Correspond to 3.5MHZ   for 1H signal   (3.5/6.75  = 0.51) */
  { 246, -36,-117,  24,   6  },  /* Correspond to 3.75MHZ  for 1H signal   (3.75/6.75 = 0.55) */
  { 220, -56,-109,  48,   7  },  /* Correspond to 4.0MHZ   for 1H signal   (4.0/6.75  = 0.58) */
  { 212, -75, -92,  61,   0  },  /* Correspond to 4.25MHZ  for 1H signal   (4.25/6.75 = 0.63) */
#else
  /* Filter coefficients to give 0.8 gain in passband */
  { 124,  70,  -28, -68, -36 },  /* Correspond to 1.0MHZ   for 1H signal   (1.0/6.75  = 0.15) */
  { 154,  56,  -74, -58,  -1 },  /* Correspond to 1.25MHZ  for 1H signal   (1.25/6.75 = 0.18) */
  { 178,  27,  -99, -24,   7 },  /* Correspond to 1.5MHZ   for 1H signal   (1.5/6.75  = 0.22) */
  { 190,  -9, -101,   7,   8 },  /* Correspond to 1.75MHZ  for 1H signal   (1.75/6.75 = 0.26) */
   /* extended size limit */
  { 124,  70,  -28, -68, -36 },  /* Correspond to 2.0MHZ   for 1H signal   (2.0/6.75  = 0.30) */
  { 140,  65,  -52, -68, -15 },  /* Correspond to 2.25MHZ  for 1H signal   (2.25/6.75 = 0.33) */
  { 154,  56,  -74, -58,  -1 },  /* Correspond to 2.5MHZ   for 1H signal   (2.5/6.75  = 0.37) */
  { 166,  43,  -89, -42,   5 },  /* Correspond to 2.75MHZ  for 1H signal   (2.75/6.75 = 0.40) */
  { 178,  27,  -99, -24,   7 },  /* Correspond to 3.0MHZ   for 1H signal   (3.0/6.75  = 0.44) */
  { 188,   9, -103,  -7,   7 },  /* Correspond to 3.25MHZ  for 1H signal   (3.25/6.75 = 0.48) */
  { 190,  -9, -101,   7,   8 },  /* Correspond to 3.5MHZ   for 1H signal   (3.5/6.75  = 0.51) */
  { 172, -26,  -98,  26,  12 },  /* Correspond to 3.75MHZ  for 1H signal   (3.75/6.75 = 0.55) */
  { 176, -44,  -87,  38,   5 },  /* Correspond to 4.0MHZ   for 1H signal   (4.0/6.75  = 0.58) */
  { 174, -61,  -72,  47,  -1 },  /* Correspond to 4.25MHZ  for 1H signal   (4.25/6.75 = 0.63) */
#endif
};




struct _IQIPeakingInitParameters {
  struct _Peaking {
    /* Peaking */
    uint8_t range_max;                            /* start Value for Range Gain non linear curve */
    bool    coring_mode;                           /* if set the peaking is done in chroma adaptive coring mode, if reset it is done in manual coring mode */
    uint8_t coring_level;                         /* peaking coring value */
    enum IQIStrength clipping_mode;             /* clipping mode */
  } peaking;
};



static const struct _IQIPeakingInitParameters IQI_PK_initDefaults = {
  peaking : { range_max : 4,
              coring_mode : false,
              coring_level : 8,
              clipping_mode : IQISTRENGTH_NONE,
            },
};


static const struct _IQIPeakingConf IQI_PK_ConfigParams[PCIQIC_COUNT] = {
  /* disable */
 {              undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_100,
                coring_mode      : false,
                coring_level     : 0,
                vertical_peaking : false,
                ver_gain         : IQIPHVG_0DB,
                clipping_mode    : IQISTRENGTH_NONE,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_0DB,
                bandpassgain     : IQIPHVG_0DB
  },
  /* soft */
  {             undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P1_0DB,
                bandpassgain     : IQIPHVG_P1_0DB
  },

  /* medium */
  {             undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P3_0DB,
                bandpassgain     : IQIPHVG_P3_0DB
  },

  /* strong */
  {             undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P5_0DB,
                bandpassgain     : IQIPHVG_P5_0DB
  }
};




CSTmIQIPeaking::CSTmIQIPeaking (void)
{
  m_Revision = IQI_CELL_REV2;
  vibe_os_zero_memory( &m_current_config, sizeof( m_current_config ));
  m_preset = PCIQIC_BYPASS;
}


CSTmIQIPeaking::~CSTmIQIPeaking (void)
{

}


bool
CSTmIQIPeaking::Create (void)
{

  m_current_config = IQI_PK_ConfigParams[PCIQIC_FIRST];
  m_preset = PCIQIC_FIRST;

  return true;
}





void
CSTmIQIPeaking::PeakingGainSet (struct IQIPeakingSetup *  setup,
                         enum IQIPeakingHorVertGain     horGainHighPass,
                         enum IQIPeakingHorVertGain     horGainBandPass,
                         enum IQIPeakingFilterFrequency highPassFilterCutoffFrequency,
                         enum IQIPeakingFilterFrequency bandPassFilterCenterFrequency)
{
  //uint32_t reg;
  uint32_t gain, gain_divider;
  int32_t  coeff0, coeff1, coeff2, coeff3, coeff4;
  //uint32_t coeff0_, coeff1_, coeff2_, coeff3_, coeff4_;

  /* if both are positive */
  if (horGainHighPass >= IQIPHVG_0DB
      && horGainBandPass >= IQIPHVG_0DB)
    {
      /* find max gain from HP & BP */
      if (horGainHighPass >= horGainBandPass)
        {
          /* calculate the relative gain */
          gain = 16 * (horGainBandPass - IQIPHVG_0DB);
          gain_divider = horGainHighPass - IQIPHVG_0DB;

          if (gain_divider != 0)
            gain /= gain_divider;
          else
            gain = 0;

          setup->peaking.horizontal_gain = VDP_IQI_PeakingHorGain[horGainHighPass];

          coeff0 = (VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][0]
                    + ((VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][0]
                        * ((short) gain))
                       / 16));
          coeff1 = (VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][1]
                    + ((VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][1]
                        * ((short) gain))
                       / 16));
          coeff2 = (VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][2]
                    + ((VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][2]
                        * ((short) gain))
                       / 16));
          coeff3 = (VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][3]
                    + ((VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][3]
                        * ((short) gain))
                       / 16));
          coeff4 = (VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][4]
                    + ((VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][4]
                        * ((short) gain))
                       / 16));
        }
      else
        {
          /* calculate relative gain */
          gain = 16 * (horGainHighPass - IQIPHVG_0DB);
          gain_divider = horGainBandPass - IQIPHVG_0DB;
          gain /= gain_divider;

          setup->peaking.horizontal_gain = VDP_IQI_PeakingHorGain[horGainBandPass];
          coeff0 = (VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][0]
                    + ((VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][0]
                        * ((short) gain))
                       / 16));
          coeff1 = (VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][1]
                    + ((VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][1]
                        * ((short) gain))
                       / 16));
          coeff2 = (VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][2]
                    + ((VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][2]
                        * ((short) gain))
                       / 16));
          coeff3 = (VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][3]
                    + ((VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][3]
                        * ((short) gain))
                       / 16));
          coeff4 = (VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][4]
                    + ((VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][4]
                        * ((short) gain))
                       / 16));
        }
    }
  else
    {
      /* at least one of them is negative */
      if (horGainHighPass < horGainBandPass)
        {

          setup->peaking.horizontal_gain = VDP_IQI_PeakingHorGain[horGainHighPass];

          coeff0 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][0];
          coeff1 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][1];
          coeff2 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][2];
          coeff3 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][3];
          coeff4 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][4];
        }
      else
        {

          setup->peaking.horizontal_gain = VDP_IQI_PeakingHorGain[horGainBandPass];

          coeff0 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][0];
          coeff1 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][1];
          coeff2 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][2];
          coeff3 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][3];
          coeff4 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][4];
        }
    }

  setup->peaking.coeff0_ = (uint16_t) coeff0 & (uint16_t) PEAKING_COEFFX_MASK;
  setup->peaking.coeff1_ = (uint16_t) coeff1 & (uint16_t) PEAKING_COEFFX_MASK;
  setup->peaking.coeff2_ = (uint16_t) coeff2 & (uint16_t) PEAKING_COEFFX_MASK;
  setup->peaking.coeff3_ = (uint16_t) coeff3 & (uint16_t) PEAKING_COEFFX_MASK;
  setup->peaking.coeff4_ = (uint16_t) coeff4 & (uint16_t) PEAKING_COEFFX_MASK;

  TRC( TRC_ID_HQVDPLITE, "setup->peaking.horizontal_gain %d", setup->peaking.horizontal_gain);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.coeff0_ %d", setup->peaking.coeff0_);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.coeff1_ %d", setup->peaking.coeff1_);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.coeff2_ %d", setup->peaking.coeff2_);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.coeff3_ %d", setup->peaking.coeff3_);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.coeff4_ %d", setup->peaking.coeff4_);

}


#define FREQUENCY_FROM_IQIPFF(x)  (100 * (100 + 25 * (x)) / 675)

IQIPeakingFilterFrequency
CSTmIQIPeaking::GetZoomCorrectedFrequency ( CDisplayInfo             *pqbi,
                                    enum IQIPeakingFilterFrequency  PeakingFrequency)
{
  int32_t                   ZoomInRatioBy100;
  int32_t                   IndexBy100;
  IQIPeakingFilterFrequency CorrectedPeakingFrequency;

  /* We need to do divide the peaking freq by the zoom ratio: F2 = F1/z */
  /* As we use an enum indexed by n specifying the frequency, we have
     F1 = (n * 0.25 + 1) / 6.75 */
  /* When developping F2 = F1/z this evaluates to n2 = (n1 + 4 - 4*z) / z */
  /* To work in fixed point: z = Z/100 => n2 = (100 * n1 + 400 - 4*Z) / Z */

  /* ZoomInRatioBy100 =
       (LayerCommonData_p->ViewPortParams.OutputRectangle.Width * 100)
       / LayerCommonData_p->ViewPortParams.InputRectangle.Width; */
  ZoomInRatioBy100 = (pqbi->m_dstFrameRect.width * 100) / pqbi->m_primarySrcFrameRect.width;

  /* Use 100*Index for rounding purposes */
  /* IndexBy100 =
       (100 * (100 * (S32) PeakingFrequency + 400 - 4*ZoomInRatioBy100))
       / ZoomInRatioBy100; */
  IndexBy100 = ((100 * (100 * (int32_t) PeakingFrequency
                       + 400 - 4 * ZoomInRatioBy100))
                / ZoomInRatioBy100);

  if (IndexBy100 < 0)
    {
      TRC( TRC_ID_HQVDPLITE, "IQI peaking zoom correction: clipping peaking frequency "
                   "from 0.%d to min 0.%d",
                   FREQUENCY_FROM_IQIPFF (PeakingFrequency),
                   FREQUENCY_FROM_IQIPFF (0));

      IndexBy100 = 0;
    }

  /* round correctly */
  CorrectedPeakingFrequency = (IQIPeakingFilterFrequency)
    ((IndexBy100 / 100) + (((IndexBy100 % 100) >= 50) ? 1 : 0));

  TRC( TRC_ID_HQVDPLITE, "IQI peaking zoom correction: Freq 0.%d has been changed to "
               "0.%d for a zoom-in ratio of %d.%d",
               FREQUENCY_FROM_IQIPFF (PeakingFrequency),
               FREQUENCY_FROM_IQIPFF (CorrectedPeakingFrequency),
               ZoomInRatioBy100 / 100, ZoomInRatioBy100 % 100);

  if (CorrectedPeakingFrequency > IQIPFF_LAST)
    {
     TRC( TRC_ID_HQVDPLITE, "IQI peaking zoom correction: clipping peaking frequency "
                   "from 0.%d to max 0.%d",
                   FREQUENCY_FROM_IQIPFF (PeakingFrequency),
                   FREQUENCY_FROM_IQIPFF (IQIPFF_LAST));
      CorrectedPeakingFrequency = IQIPFF_LAST;
    }

  return CorrectedPeakingFrequency;
}

void
CSTmIQIPeaking::CalculateSetup ( CDisplayInfo         *pqbi,
                        bool                isDisplayInterlaced,
                         struct IQIPeakingSetup            * setup)
{

  /* peaking */
  if (isDisplayInterlaced)
    {
      /* We need to disable these if the output is interlaced to avoid
         artifacts */
      setup->peaking.env_detect_3_lines = false;
      setup->peaking.vertical_peaking_allowed = false;
    }
  else
    {
      setup->peaking.env_detect_3_lines = true;
      setup->peaking.vertical_peaking_allowed = true;
    }

  /* peaking cutoff */
  enum IQIPeakingFilterFrequency HighPassFilterCutoffFrequency
    = GetZoomCorrectedFrequency (pqbi,
                                 m_current_config.highpass_filter_cutofffreq);
  enum IQIPeakingFilterFrequency BandPassFilterCenterFrequency
    = GetZoomCorrectedFrequency (pqbi,
                                 m_current_config.bandpass_filter_centerfreq);

  /* If extended size is required to reach required frequencies... */
  if (HighPassFilterCutoffFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT
      || BandPassFilterCenterFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT)
    {
      /* ...then we have to multiply the frequency by 2 because extended
         size on means all frequencies are divided by 2 */
      /* Compensate HighPass filter for extended mode if extended was not
        required for it */
      if (HighPassFilterCutoffFrequency > IQIPFF_EXTENDED_SIZE_LIMIT)
        /* HighPass is at least 0.30 if not extended so with extended on it
           is at least 0.60 */
        HighPassFilterCutoffFrequency = IQIPFF_0_63_FsDiv2;

      /* Note that the BandPass frequency is always lower than the HighPass
         frequency so HighPass extended and BandPass not extended makes no
         sense */

      /* extended size is not allowed for zoom-in < 2.0 */
      /* FIXME - is a real rescale meant here, or does it include (de-)
         interlacing as well??
      if(((100*LayerCommonData_p->ViewPortParams.OutputRectangle.Width)/LayerCommonData_p->ViewPortParams.InputRectangle.Width) < 200)
      */
      if (((0x100 * pqbi->m_dstFrameRect.width) / pqbi->m_primarySrcFrameRect.width) < 0x200)
        {
          TRC( TRC_ID_HQVDPLITE,"Warning! IQI Peaking: extended size needed but "
                       "zoom-in is < 2.0!");

          if (BandPassFilterCenterFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT)
            {
              /* clip BandPass to minimum value without extended size */
              TRC( TRC_ID_HQVDPLITE,"Warning! IQI Peaking: Clipping BandPass filter "
                           "freq 0.%d to minimum freq non extended 0.%d\n",
                           FREQUENCY_FROM_IQIPFF (BandPassFilterCenterFrequency),
                           FREQUENCY_FROM_IQIPFF (IQIPFF_EXTENDED_SIZE_LIMIT + 1));
              BandPassFilterCenterFrequency = static_cast <IQIPeakingFilterFrequency> (
                (static_cast <unsigned int> (IQIPFF_EXTENDED_SIZE_LIMIT)) + 1);
            }

          if (HighPassFilterCutoffFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT)
            {
              /* clip HighPass to minimum value without extended size */
              TRC( TRC_ID_HQVDPLITE, "Warning! IQI Peaking: Clipping HighPass filter "
                           "freq 0.%d to minimum freq non extended 0.%d\n",
                           FREQUENCY_FROM_IQIPFF (HighPassFilterCutoffFrequency),
                           FREQUENCY_FROM_IQIPFF (IQIPFF_EXTENDED_SIZE_LIMIT + 1));
              HighPassFilterCutoffFrequency = static_cast <IQIPeakingFilterFrequency> (
                (static_cast <unsigned int> (IQIPFF_EXTENDED_SIZE_LIMIT)) + 1);
            }

          setup->peaking.extended_size = false;
        }
      else
        setup->peaking.extended_size = true;
    }
  else
    setup->peaking.extended_size = false;

  if (BandPassFilterCenterFrequency > HighPassFilterCutoffFrequency)
  {
      TRC( TRC_ID_HQVDPLITE,"Error! IQI Peaking: BandPass freq 0.%d is above HighPass "
                   "freq. 0.%d which is forbidden! "
                   "Forcing Bandpass = Highpass\n",
                   FREQUENCY_FROM_IQIPFF (BandPassFilterCenterFrequency),
                   FREQUENCY_FROM_IQIPFF (HighPassFilterCutoffFrequency));
      BandPassFilterCenterFrequency = HighPassFilterCutoffFrequency;
  }

  setup->peaking.highpass_filter_cutoff_freq = HighPassFilterCutoffFrequency;
  setup->peaking.bandpass_filter_center_freq = BandPassFilterCenterFrequency;


  // was in updateHW
  if (setup->peaking.vertical_peaking_allowed)
  {
    setup->peaking.vertical_gain = VDP_IQI_PeakingVertGain[m_current_config.ver_gain];
  }

  TRC( TRC_ID_HQVDPLITE, "setup->peaking.env_detect_3_lines %d", setup->peaking.env_detect_3_lines);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.vertical_peaking_allowed %d", setup->peaking.vertical_peaking_allowed);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.extended_size %d",setup->peaking.extended_size);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.highpass_filter_cutoff_freq %d", setup->peaking.highpass_filter_cutoff_freq);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.bandpass_filter_center_freq %d",  setup->peaking.bandpass_filter_center_freq);
  TRC( TRC_ID_HQVDPLITE, "setup->peaking.vertical_gain %d", setup->peaking.vertical_gain);

  /* Peaking Gain according to HP/BP */
  PeakingGainSet ( setup,
                  m_current_config.highpassgain,
                  m_current_config.bandpassgain,
                  HighPassFilterCutoffFrequency,
                  BandPassFilterCenterFrequency);

}

void CSTmIQIPeaking::CalculateParams(struct IQIPeakingSetup * setup, HQVDPLITE_IQI_Params_t * params)
{
  params->PkConfig = setup->peaking.extended_size  << IQI_PK_CONFIG_EXTENDED_SIZE_SHIFT |
    setup->peaking.env_detect_3_lines << IQI_PK_CONFIG_ENV_DETECT_SHIFT        |
    m_current_config.coring_mode << IQI_PK_CONFIG_CORING_MODE_SHIFT    |
    setup->peaking.vertical_peaking_allowed << IQI_PK_CONFIG_V_PK_EN_SHIFT     |
    1 <<  IQI_PK_CONFIG_RANGE_GAIN_LUT_INIT_SHIFT                      | //hardcoded: LUT is loaded at each VSync
    m_current_config.overshoot <<  IQI_PK_CONFIG_OVERSHOOT_SHIFT       |
    m_current_config.undershoot <<  IQI_PK_CONFIG_UNDERSHOOT_SHIFT     |
    IQI_PK_initDefaults.peaking.range_max << IQI_PK_CONFIG_RANGE_MAX_SHIFT ;

  TRC( TRC_ID_HQVDPLITE, "PK Config register content : 0x%x", params->PkConfig);

  params->Coeff0Coeff1 = (( setup->peaking.coeff0_ << IQI_COEFF0_COEFF1_PK_COEFF0_SHIFT ) & IQI_COEFF0_COEFF1_PK_COEFF0_MASK ) |
    (( setup->peaking.coeff1_ << IQI_COEFF0_COEFF1_PK_COEFF1_SHIFT ) & IQI_COEFF0_COEFF1_PK_COEFF1_MASK );


  TRC( TRC_ID_HQVDPLITE, "PK Coeff0Coeff1 register content : 0x%x", params->Coeff0Coeff1);

  params->Coeff2Coeff3 = (( setup->peaking.coeff2_ << IQI_COEFF2_COEFF3_PK_COEFF2_SHIFT ) & IQI_COEFF2_COEFF3_PK_COEFF3_MASK ) |
    (( setup->peaking.coeff3_ << IQI_COEFF2_COEFF3_PK_COEFF3_SHIFT ) & IQI_COEFF2_COEFF3_PK_COEFF3_MASK );

 TRC( TRC_ID_HQVDPLITE, "PK Coeff2Coeff3 register content : 0x%x", params->Coeff2Coeff3);

  params->Coeff4 = ((setup->peaking.coeff4_ << IQI_COEFF4_PK_COEFF4_SHIFT) & IQI_COEFF4_PK_COEFF4_MASK );


  TRC( TRC_ID_HQVDPLITE, "PK Coeff4 register content : 0x%x", params->Coeff4);

  params->PkLut = m_current_config.clipping_mode << IQI_PK_LUT_SELECT_SHIFT;


  TRC( TRC_ID_HQVDPLITE, "PK PkLut register content : 0x%x", params->PkLut);

  params->PkGain = setup->peaking.horizontal_gain << IQI_PK_GAIN_PK_HOR_GAIN_SHIFT  |
    setup->peaking.vertical_gain << IQI_PK_GAIN_PK_VERT_GAIN_SHIFT;

  TRC( TRC_ID_HQVDPLITE, "PK PkGain register content : 0x%x", params->PkGain);


  params->PkCoringLevel = m_current_config.coring_level << IQI_PK_CORING_LEVEL_PK_CORING_LEVEL_SHIFT;

  TRC( TRC_ID_HQVDPLITE, "PK PkCoringLevel register content : 0x%x", params->PkCoringLevel);

}

/* to be called on SetControl */
bool
CSTmIQIPeaking::SetPeakingParams ( struct _IQIPeakingConf *  peaking)
{

  if (peaking->bandpassgain > IQIPHVG_LAST
      || peaking->highpassgain > IQIPHVG_LAST
      || peaking->ver_gain < IQIPHVG_N6_0DB
      || peaking->ver_gain > IQIPHVG_P12_0DB
      || peaking->coring_level > 63
      || peaking->overshoot > IQIPOUF_LAST
      || peaking->undershoot > IQIPOUF_LAST
      || peaking->clipping_mode > IQISTRENGTH_LAST
      || peaking->bandpass_filter_centerfreq >= IQIPFF_COUNT
      || peaking->highpass_filter_cutofffreq >= IQIPFF_COUNT
      || peaking->bandpass_filter_centerfreq > peaking->highpass_filter_cutofffreq)
    {

      TRC( TRC_ID_ERROR, "invalid IQI peaking parameters" );

      return false;
    }

    /* copy in m_current config */
    m_current_config = *peaking;

    TRC( TRC_ID_HQVDPLITE, "peaking->bandpassgain = %d", peaking->bandpassgain );
    TRC( TRC_ID_HQVDPLITE, "peaking->highpassgain = %d", peaking->highpassgain );
    TRC( TRC_ID_HQVDPLITE, "peaking->ver_gain = %d", peaking->ver_gain );
    TRC( TRC_ID_HQVDPLITE, "peaking->coring_level = %d", peaking->coring_level );
    TRC( TRC_ID_HQVDPLITE, "peaking->overshoot = %d", peaking->overshoot );
    TRC( TRC_ID_HQVDPLITE, "peaking->undershoot = %d", peaking->undershoot );
    TRC( TRC_ID_HQVDPLITE, "peaking->clipping_mode = %d", peaking->clipping_mode );
    TRC( TRC_ID_HQVDPLITE, "peaking->bandpass_filter_centerfreq = %d", peaking->bandpass_filter_centerfreq );
    TRC( TRC_ID_HQVDPLITE, "peaking->highpass_filter_cutofffreq = %d", peaking->highpass_filter_cutofffreq );

    return true;

}

void CSTmIQIPeaking::GetPeakingParams(struct _IQIPeakingConf *  peaking)
{
    *peaking = m_current_config;
}



bool CSTmIQIPeaking::SetPeakingPreset(PlaneCtrlIQIConfiguration preset)
{

    if (preset >=  PCIQIC_COUNT)
    {

      TRC( TRC_ID_ERROR, "invalid IQI peaking parameters" );
      return false;

    }
    m_current_config =  IQI_PK_ConfigParams[preset];
    m_preset = preset;

    TRC( TRC_ID_HQVDPLITE, "peaking preset = %d", preset );

    return true;
}

void CSTmIQIPeaking::GetPeakingPreset( enum PlaneCtrlIQIConfiguration * preset) const
{

    *preset = m_preset;

}

