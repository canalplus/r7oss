/***********************************************************************
 *
 * File: STMCommon/stmiqi.cpp
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>


#include "stmiqi.h"


#define IQI_CONF                      0x00
/* for IQI config */
#  define IQI_BYPASS                   (1 << 0)
#  define IQI_ENABLESINXCOMPENSATION   (1 << 1)
#  define IQI_ENABLE_PEAKING           (1 << 2)
#  define IQI_ENABLE_LTI               (1 << 3)
#  define IQI_ENABLE_CTI               (1 << 4)
#  define IQI_ENABLE_LE                (1 << 5)
#  define IQI_ENABLE_CSC               (1 << 6)
#  define IQI_ENABLE_ALL               (0 /* IQI_BYPASS */           \
                                        | IQI_ENABLESINXCOMPENSATION \
                                        | IQI_ENABLE_PEAKING         \
                                        | IQI_ENABLE_LTI             \
                                        | IQI_ENABLE_CTI             \
                                        | IQI_ENABLE_LE              \
                                        | IQI_ENABLE_CSC             \
                                       )
#  define IQI_DEMO_LE                  (1 << 16)
#  define IQI_DEMO_PEAKING             (1 << 17)
#  define IQI_DEMO_LTI                 (1 << 18)
#  define IQI_DEMO_CTI                 (1 << 20)
#  define IQI_DEMO_ALL                 (IQI_DEMO_LE        \
                                        | IQI_DEMO_PEAKING \
                                        | IQI_DEMO_LTI     \
                                        | IQI_DEMO_CTI     \
                                       )

#define IQI_DEMO_WINDOW               0x04

#define IQI_PEAKING_CONF              0x08
#  define PEAKING_RANGEMAX_MASK        (0x0000f000)
#  define PEAKING_RANGEMAX_SHIFT       (12)
#  define PEAKING_UNDERSHOOT_MASK      (0x00000c00)
#  define PEAKING_UNDERSHOOT_SHIFT     (10)
#  define PEAKING_OVERSHOOT_MASK       (0x00000300)
#  define PEAKING_OVERSHOOT_SHIFT      (8)
#  define PEAKING_RANGE_GAIN_LUT_INIT  (1 << 4)
#  define PEAKING_V_PK_EN              (1 << 3)
#  define PEAKING_CORING_MODE          (1 << 2)
#  define PEAKING_ENV_DETECT           (1 << 1)
#  define PEAKING_EXTENDED_SIZE        (1 << 0)
#  define IQI_PEAKING_CONF_ALL_MASK    (PEAKING_RANGEMAX_MASK \
                                        | PEAKING_UNDERSHOOT_MASK \
                                        | PEAKING_OVERSHOOT_MASK \
                                        | PEAKING_RANGE_GAIN_LUT_INIT \
                                        | PEAKING_V_PK_EN \
                                        | PEAKING_CORING_MODE \
                                        | PEAKING_ENV_DETECT \
                                        | PEAKING_EXTENDED_SIZE)

#define IQI_PEAKING_COEFX0            0x0c
#define IQI_PEAKING_COEFX1            0x10
#define IQI_PEAKING_COEFX2            0x14
#define IQI_PEAKING_COEFX3            0x18
#define IQI_PEAKING_COEFX4            0x1c
   /* can also convert signed char to "10,t,9,0" format */
#  define PEAKING_COEFFX_MASK          (0x000003ff)

#define IQI_PEAKING_RANGE_GAIN_LUT_BA 0x20

#define IQI_PEAKING_GAIN              0x24
#  define PEAKING_GAIN_SINC_MASK       (0x0000f000)
#  define PEAKING_GAIN_SINC_SHIFT      (12)
#  define PEAKING_GAIN_VERT_GAIN_MASK  (0x00000f00)
#  define PEAKING_GAIN_VERT_GAIN_SHIFT (8)
#  define PEAKING_GAIN_HOR_GAIN_MASK   (0x0000001f)
#  define PEAKING_GAIN_HOR_GAIN_SHIFT  (0)
#  define IQI_PEAKING_GAIN_ALL_MASK    (PEAKING_GAIN_SINC_MASK \
                                        | PEAKING_GAIN_VERT_GAIN_MASK \
                                        | PEAKING_GAIN_HOR_GAIN_MASK)

#define IQI_PEAKING_CORING_LEVEL      0x28
#  define PEAKING_CORING_LEVEL_MASK    (0x0000003f)
#  define PEAKING_CORING_LEVEL_SHIFT   (0)
#  define IQI_PEAKING_CORING_LEVEL_ALL_MASK (PEAKING_CORING_LEVEL_MASK)

#define IQI_LTI_CONF                  0x2c
#  define LTI_CONF_HMMS_PREFILTER_MASK  (0x00030000)
#  define LTI_CONF_HMMS_PREFILTER_SHIFT (16)
#  define LTI_CONF_V_LTI_STRENGTH_MASK  (0x0000f000)
#  define LTI_CONF_V_LTI_STRENGTH_SHIFT (12)
#  define LTI_CONF_HEC_HEIGHT_MASK      (0x00000800)
#  define LTI_CONF_HEC_HEIGHT_SHIFT     (11)
#  define LTI_CONF_HEC_GRADIENT_MASK    (0x00000600)
#  define LTI_CONF_HEC_GRADIENT_SHIFT   (9)
#  define LTI_CONF_H_LTI_STRENGTH_MASK  (0x000001f0)
#  define LTI_CONF_H_LTI_STRENGTH_SHIFT (4)
#  define LTI_CONF_VERTICAL_LTI         (1 << 3)
#  define LTI_CONF_ANTIALIASING         (1 << 1)
#  define LTI_CONF_SELECTIVE_EDGE       (1 << 0)

#define IQI_LTI_THRESHOLD             0x30
#  define LTI_CORNERTEST_MASK                (0xff000000)
#  define LTI_CORNERTEST_SHIFT               (24)
#  define LTI_VERTDIFF_MASK                  (0x00ff0000)
#  define LTI_VERTDIFF_SHIFT                 (16)
#  define LTI_MINCORRELATION_MASK            (0x0000ff00)
#  define LTI_MINCORRELATION_SHIFT           (8)
#  define LTI_DISTURBANCE_MASK               (0x000000ff)
#  define LTI_DISTURBANCE_SHIFT              (0)
#    define LTI_DEFAULT_CORNERTEST     (0x20) /* VDP_IQI_LTI_DEFAULT_CORNERTEST */
#    define LTI_DEFAULT_VERTDIFF       (0x20) /* VDP_IQI_LTI_DEFAULT_VERTDIFF */
#    define LTI_DEFAULT_MINCORRELATION (0x10) /* VDP_IQI_LTI_DEFAULT_MINCORRELATION */
#    define LTI_DEFAULT_DISTURBANCE    (0x04) /* VDP_IQI_LTI_DEFAULT_DISTURBANCE */

#define IQI_LTI_DELTA_SLOPE           0x34
#  define LTI_DELTA_SLOPE_MASK         (0x000000ff)
#  define LTI_DELTA_SLOPE_SHIFT        (0)

/* revision 2 CTI */
#define IQIr2_CTI_CONF                  0x38
#  define CTIr2_COR_MASK                       (0x000f0000)
#  define CTIr2_COR_SHIFT                      (16)
#  define CTIr2_EXTENDED_MASK                  (0x00004000)
#  define CTIr2_EXTENDED_SHIFT                 (14)
#  define CTIr2_STRENGTH2_MASK                 (0x00003000)
#  define CTIr2_STRENGTH2_SHIFT                (12)
#  define CTIr2_STRENGTH1_MASK                 (0x00000c00)
#  define CTIr2_STRENGTH1_SHIFT                (10)
#  define CTIr2_MED_MASK                       (0x000003f0)
#  define CTIr2_MED_SHIFT                      (4)
#  define CTIr2_MON_MASK                       (0x0000000f)
#  define CTIr2_MON_SHIFT                      (0)
#    define CTIr2_DEFAULT_COR                  (0x01) /* VDP_IQI_CTI_DEFAULT_COR */
#    define CTIr2_DEFAULT_STRENGTH2            (IQICS_NONE)
#    define CTIr2_DEFAULT_STRENGTH1            (IQICS_NONE)
#    define CTIr2_DEFAULT_MED                  (0x1C) /* VDP_IQI_CTI_DEFAULT_MED */
#    define CTIr2_DEFAULT_MON                  (0x04) /* VDP_IQI_CTI_DEFAULT_MON */
#  define IQIr2_CTI_ALL_MASK                   (CTIr2_COR_MASK \
                                                | CTIr2_STRENGTH2_MASK \
                                                | CTIr2_STRENGTH1_MASK \
                                                | CTIr2_MED_MASK \
                                                | CTIr2_MON_MASK)

#define IQI_LE_CONF                   0x3c
#  define LE_WEIGHT_GAIN_MASK          (0x0000007c)
#  define LE_WEIGHT_GAIN_SHIFT         (2)
#  define LE_601                       (1 << 1)
#  define LE_LUT_INIT                  (1 << 0)
#  define IQI_LE_ALL_MASK              (LE_WEIGHT_GAIN_MASK \
                                        | LE_601 \
                                        | LE_LUT_INIT)

#define IQI_LE_LUT_BA                 0x40

#define IQI_CONBRI                    0x4c
#  define CONBRI_CONBRI_OFFSET_MASK    (0x0fff0000)
#  define CONBRI_CONBRI_OFFSET_SHIFT   (16)
#  define CONBRI_CONTRAST_GAIN_MASK    (0x000001ff)
#  define CONBRI_CONTRAST_GAIN_SHIFT   (0)
#    define CONBRI_MIN_CONTRAST_GAIN   (  0)
#    define CONBRI_MAX_CONTRAST_GAIN   (256)
#  define IQI_CONBRI_ALL_MASK          (CONBRI_CONBRI_OFFSET_MASK \
                                        | CONBRI_CONTRAST_GAIN_MASK)

#define IQI_SAT                       0x50
#  define SAT_SAT_GAIN_MASK            (0x000001ff)
#  define SAT_SAT_GAIN_SHIFT           (0)
#    define SAT_SAT_GAIN_ONE             (256 << SAT_SAT_GAIN_SHIFT)
#  define IQI_SAT_ALL_MASK             (SAT_SAT_GAIN_MASK)





static const UCHAR VDP_IQI_PeakingHorGain[IQIPHVG_LAST + 1] = {
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

static const UCHAR VDP_IQI_PeakingVertGain[IQIPHVG_LAST + 1] = {
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

/* for converting h_lti_strenght to delta_slope */
static const UCHAR VDP_IQI_DeltaSlope[32] =
{
  0, 0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,
  /* SGP_PC_63739 - START */
#if 1
  0, 8, 17, 26, 37, 47, 59, 72, 85, 100, 116, 134, 154, 175, 199, 226
#else
  0, 0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,
#endif
  /* SGP_PC_63739 - END */
};

/* Coeff used by Clipping */
static const UCHAR VDP_IQI_ClippingCurves[IQISTRENGTH_LAST + 1][256] = {
  {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
  },
  {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    254,253,252,251,250,249,248,247,247,246,245,244,243,242,241,240,
    239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,
    223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,
    207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,
    191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,
    175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,
    159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,
    143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128
  },
  {
    255,247,240,233,227,220,215,209,204,199,194,190,185,181,177,173,
    170,166,163,160,157,154,151,148,145,143,140,138,136,133,131,129,
    127,125,123,121,119,117,116,114,113,111,110,108,107,105,104,103,
    101,100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86,
     85, 84, 83, 82, 82, 81, 80, 79, 78, 77, 76, 76, 75, 74, 73, 72,
     72, 71, 71, 70, 71, 70, 69, 68, 68, 67, 66, 65, 65, 64, 64, 63,
     63, 62, 62, 61, 61, 60, 60, 60, 59, 59, 58, 58, 58, 57, 57, 56,
     56, 56, 55, 55, 55, 54, 54, 54, 53, 53, 53, 52, 52, 52, 51, 51,
     51, 50, 50, 50, 49, 49, 49, 48, 48, 48, 47, 47, 47, 46, 46, 46,
     46, 45, 45, 45, 45, 44, 44, 44, 44, 43, 43, 43, 43, 42, 42, 42,
     42, 41, 41, 41, 41, 40, 40, 40, 40, 40, 39, 39, 39, 39, 39, 39,
     38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 36, 36, 36, 36, 36,
     36, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 33, 33,
     33, 33, 33, 33, 33, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31, 31,
     31, 31, 31, 31, 30, 30, 30, 30, 30, 30, 30, 30, 30, 29, 29, 29,
     29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28
  }
};

/* LUMA LUT */


#define LUMA_LUT_SIZE     (128 * sizeof (SHORT)) /* maximum size of IQI Luma LUT enhancer */
#define VDP_IQI_BLACKD1   ( 64)    /* Default value for AutoCurve LE algo */
#define VDP_IQI_WHITED1   (940)    /* Default value for AutoCurve LE algo */
#define VDP_IQI_LUT_10BITS_SIZE               (1024)  /* Compute LUT on 10 bits         */
#define VDP_IQI_LUT_7BITS_SIZE                ( 128)  /* Compute LUT on 7 bits (for HW) */
#define VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR  (   8)  /* Scale from VDP_IQI_LUT_10BITS_SIZE
                                                         to VDP_IQI_LUT_7BITS_SIZE      */


struct _IQIInitParameters {
  struct _LTI {
    /* LTI */
    UCHAR threshold_corner_test;     /* threshold corner test*/
    UCHAR threshold_vert_diff;       /* threshold vertical difference */
    UCHAR threshold_min_correlation; /* threshold minimum correlation */
    UCHAR threshold_disturbance;     /* threshold disturbance */
  } lti;
  struct _CTI {
    /* CTI */
    UCHAR tmon;   /* value of cti used in monotonicity checker */
    UCHAR tmed;   /* value of the cti threshold used in 3 points median filter */
    UCHAR coring; /* value of cti coring used in the peaking filter */
  } cti;
  struct _Peaking {
    /* Peaking */
    UCHAR range_max;                            /* start Value for Range Gain non linear curve */
    bool coring_mode;                           /* if set the peaking is done in chroma adaptive coring mode, if reset it is done in manual coring mode */
    UCHAR coring_level;                         /* peaking coring value */
    enum IQIStrength clipping_mode;             /* clipping mode */
  } peaking;
};



static const struct _IQIInitParameters IQIinitDefaults = {
  lti     : { threshold_corner_test : LTI_DEFAULT_CORNERTEST,
              threshold_vert_diff : LTI_DEFAULT_VERTDIFF,
              threshold_min_correlation : LTI_DEFAULT_MINCORRELATION,
              threshold_disturbance : LTI_DEFAULT_DISTURBANCE
            },
  cti     : { tmon : CTIr2_DEFAULT_MON,
              tmed : CTIr2_DEFAULT_MED,
              coring : CTIr2_DEFAULT_COR
            },
  peaking : { range_max : 4,
              coring_mode : false,
              coring_level : 8,
              clipping_mode : IQISTRENGTH_NONE,
            },
};


static const SHORT CustomCurveGamma1_02[LUMA_LUT_SIZE / sizeof (SHORT)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -2, -2, -2, -3, -3, -3, -4, -4, -4, -4, -4, -5,
-5, -5, -5, -5, -5, -5, -5, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6,
-6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6,
-6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -4, -4, -4,
-4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2,
-1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static const SHORT CustomCurveGamma1_03[LUMA_LUT_SIZE / sizeof (SHORT)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -3, -4, -4, -4, -5, -5, -6, -6, -6, -6, -7,
-7, -7, -7, -8, -8, -8, -8, -8, -8, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
-9, -9, -10, -10, -10, -10, -10, -10, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9, -9,
-9, -9, -9, -9, -9, -9, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7,
-7, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -3, -3, -3, -3,
-3, -2, -2, -2, -2, -2, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static const SHORT CustomCurveGamma1_04[LUMA_LUT_SIZE / sizeof (SHORT)] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -5, -5, -6, -6, -7, -7, -8, -8, -9, -9,
-9, -10, -10, -10, -10, -11, -11, -11, -11, -11, -12, -12, -12, -12, -12, -12, -12,
-12, -12, -12, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -13, -12, -12,
-12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -11, -11, -11, -11, -11, -11, -11,
-11, -10, -10, -10, -10, -10, -9, -9, -9, -9, -9, -8, -8, -8, -8, -8, -7, -7, -7,
-7, -6, -6, -6, -6, -5, -5, -5, -5, -4, -4, -4, -4, -3, -3, -3, -2, -2, -2, -1, -1,
-1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static const struct _IQIConfigurationParameters IQIConfigParams[PCIQIC_COUNT] = {
  /* disable */
  { iqi_conf : IQI_BYPASS,
    peaking : { undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_100,
                coring_mode      : false,
                coring_level     : 0,
                vertical_peaking : false,
                ver_gain         : IQIPHVG_0DB,
                clipping_mode    : IQISTRENGTH_NONE,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_0DB,
                bandpassgain     : IQIPHVG_0DB },
    lti: { selective_edge : IQILHGO_0,
           hmms_prefilter : IQISTRENGTH_NONE,
           anti_aliasing  : false,
           vertical       : false,
           ver_strength   : 0,
           hor_strength   : 0 },
    cti: { strength1 : IQICS_NONE,
           strength2 : IQICS_NONE },
    le: {  csc_gain    : 0,
           fixed_curve : false,
           fixed_curve_params : { BlackStretchInflexionPoint : 0,
                                  BlackStretchLimitPoint     : 0,
                                  WhiteStretchInflexionPoint : 0,
                                  WhiteStretchLimitPoint     : 0,
                                  BlackStretchGain           : 0,
                                  WhiteStretchGain           : 0 },
           custom_curve : 0 },
  },
  /* soft */
  { iqi_conf : IQI_ENABLE_PEAKING | IQI_ENABLE_LTI | IQI_ENABLE_CTI | IQI_ENABLE_LE,
    peaking : { undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P1_0DB,
                bandpassgain     : IQIPHVG_P1_0DB },
    lti: { selective_edge : IQILHGO_8,
           hmms_prefilter : IQISTRENGTH_STRONG,
           anti_aliasing  : true,
           vertical       : true,
           ver_strength   : 10,
           hor_strength   : 10 },
    cti: { strength1 : IQICS_NONE,
           strength2 : IQICS_MIN },
    le: {  csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  10,
                                  WhiteStretchGain           :  10 },
           custom_curve : 0 },
  },
  /* medium */
  { iqi_conf : IQI_ENABLE_PEAKING | IQI_ENABLE_LTI | IQI_ENABLE_CTI | IQI_ENABLE_LE,
    peaking : { undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P3_0DB,
                bandpassgain     : IQIPHVG_P3_0DB },
    lti: { selective_edge : IQILHGO_8,
           hmms_prefilter : IQISTRENGTH_STRONG,
           anti_aliasing  : true,
           vertical       : true,
           ver_strength   : 15,
           hor_strength   : 20 },
    cti: { strength1 : IQICS_NONE,
           strength2 : IQICS_STRONG },
    le: {  csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  15,
                                  WhiteStretchGain           :  15 },
           custom_curve : CustomCurveGamma1_02 },
  },
  /* strong */
  { iqi_conf : IQI_ENABLE_PEAKING | IQI_ENABLE_LTI | IQI_ENABLE_CTI | IQI_ENABLE_LE,
    peaking : { undershoot       : IQIPOUF_100,
                overshoot        : IQIPOUF_050,
                coring_mode      : false,
                coring_level     : 20,
                vertical_peaking : true,
                ver_gain         : IQIPHVG_P6_5DB,
                clipping_mode    : IQISTRENGTH_WEAK,
                highpass_filter_cutofffreq : IQIPFF_0_48_FsDiv2,
                bandpass_filter_centerfreq : IQIPFF_0_30_FsDiv2,
                highpassgain     : IQIPHVG_P5_0DB,
                bandpassgain     : IQIPHVG_P5_0DB },
    lti: { selective_edge : IQILHGO_4,
           hmms_prefilter : IQISTRENGTH_STRONG,
           anti_aliasing  : true,
           vertical       : true,
           ver_strength   : 15,
           hor_strength   : 25 },
    cti: { strength1 : IQICS_MIN,
           strength2 : IQICS_MEDIUM },
    le: {  csc_gain    : 8,
           fixed_curve : true,
           fixed_curve_params : { BlackStretchInflexionPoint : 344,
                                  BlackStretchLimitPoint     : 128,
                                  WhiteStretchInflexionPoint : 360,
                                  WhiteStretchLimitPoint     : 704,
                                  BlackStretchGain           :  20,
                                  WhiteStretchGain           :  20 },
           custom_curve : CustomCurveGamma1_03 },
  }
};



#ifdef DEBUG
static void
CheckInitParams (ULONG                caps,
                 enum IQICellRevision iqi_rev)
{
  const struct _IQIInitParameters * const init = &IQIinitDefaults;

#  define CHECK(val,shift,mask)                                          \
    ASSERTF((((val) << (shift)) & (mask)) == ((val) << (shift)),         \
            ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__))

  /* lti */
  CHECK ((ULONG) init->lti.threshold_corner_test, LTI_CORNERTEST_SHIFT, LTI_CORNERTEST_MASK);
  CHECK ((ULONG) init->lti.threshold_vert_diff, LTI_VERTDIFF_SHIFT, LTI_VERTDIFF_MASK);
  CHECK ((ULONG) init->lti.threshold_min_correlation, LTI_MINCORRELATION_SHIFT, LTI_MINCORRELATION_MASK);
  CHECK ((ULONG) init->lti.threshold_disturbance, LTI_DISTURBANCE_SHIFT, LTI_DISTURBANCE_MASK);
  /* cti */
  switch (iqi_rev)
    {
    case IQI_CELL_REV2:
      CHECK ((ULONG) init->cti.tmon, CTIr2_MON_SHIFT, CTIr2_MON_MASK);
      CHECK ((ULONG) init->cti.tmed, CTIr2_MED_SHIFT, CTIr2_MED_MASK);
      CHECK ((ULONG) init->cti.coring, CTIr2_COR_SHIFT, CTIr2_COR_MASK);
      break;

    default:
      ASSERTF (1 == 2, ("%s: should not be reached\n", __PRETTY_FUNCTION__));
      break;
    }
  /* peaking */
  CHECK ((ULONG) init->peaking.range_max, PEAKING_RANGEMAX_SHIFT, PEAKING_RANGEMAX_MASK);
  CHECK ((ULONG) init->peaking.coring_level, PEAKING_CORING_LEVEL_SHIFT, PEAKING_CORING_LEVEL_MASK);
  ASSERTF (init->peaking.clipping_mode <= IQISTRENGTH_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
#  undef CHECK
}

static void
CheckConfigParams (ULONG                                     caps,
                   enum IQICellRevision                      iqi_rev,
                   const struct _IQIConfigurationParameters * const param)
{
  /* peaking */
  ASSERTF (param->peaking.coring_level <= 63,
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.undershoot <= IQIPOUF_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.overshoot <= IQIPOUF_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.ver_gain <= IQIPHVG_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.clipping_mode <= IQISTRENGTH_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.bandpass_filter_centerfreq < IQIPFF_COUNT,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.highpass_filter_cutofffreq < IQIPFF_COUNT,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.bandpassgain <= IQIPHVG_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->peaking.highpassgain <= IQIPHVG_LAST,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  /* lti */
  ASSERTF (param->lti.selective_edge < IQILHGO_COUNT,
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->lti.hmms_prefilter <= IQISTRENGTH_LAST,
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->lti.hor_strength < N_ELEMENTS (VDP_IQI_DeltaSlope),
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  ASSERTF (param->lti.ver_strength <= 31,
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  /* cti */
  switch (iqi_rev)
    {
    case IQI_CELL_REV2:
      ASSERTF (param->cti.strength1 < IQICS_COUNT,
               ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
      ASSERTF (param->cti.strength2 < IQICS_COUNT,
               ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
      break;

    default:
      ASSERTF (1 == 2, ("%s: should not be reached!\n", __PRETTY_FUNCTION__));
      break;
    }
  /* le */
  ASSERTF (param->le.csc_gain <= 31,
           ("assertion failed: %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
}
#else
#  define CheckInitParams(caps,rev)            do { } while(0)
#  define CheckConfigParams(caps,rev,param)    do { } while(0)
#endif


CSTmIQI::CSTmIQI (enum IQICellRevision revision,
                  ULONG                baseAddr)
{
  DEBUGF2 (4, (FENTRY ", rev: %d, baseAddr: %lx\n", __PRETTY_FUNCTION__, revision, baseAddr));

  m_Revision    = revision;
  m_baseAddress = baseAddr;

  m_PeakingLUT.pMemory = 0;
  m_LumaLUT.pMemory    = 0;

  CheckInitParams (GetCapabilities (), m_Revision);
  for (ULONG config = (ULONG) PCIQIC_FIRST; config < PCIQIC_COUNT; ++config)
    CheckConfigParams (GetCapabilities (), m_Revision,
                       &IQIConfigParams[config]);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}


CSTmIQI::~CSTmIQI (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  g_pIOS->FreeDMAArea (&m_PeakingLUT);
  g_pIOS->FreeDMAArea (&m_LumaLUT);

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));
}


bool
CSTmIQI::Create (void)
{
  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  /* since we ignore the restrictions of the LUT (must be contained within
     one 64MB bank), we add an assertion to notice any change causing this
     to potentially happen. I.e. if sizeof (LUT) is < 4k, it will never cross
     a 64MB boundary because  */
  ASSERTF (sizeof (VDP_IQI_ClippingCurves) < 4096 - 8,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));
  /* same for Luma LUT */
  ASSERTF (LUMA_LUT_SIZE < 4096 - 8,
           ("assertion failed %s:%d\n", __PRETTY_FUNCTION__, __LINE__));

  g_pIOS->AllocateDMAArea (&m_PeakingLUT,
                           sizeof (VDP_IQI_ClippingCurves), 8, SDAAF_NONE);
  g_pIOS->AllocateDMAArea (&m_LumaLUT, LUMA_LUT_SIZE, 8, SDAAF_NONE);
  if (UNLIKELY (!m_PeakingLUT.pMemory
                || !m_LumaLUT.pMemory))
    {
      DEBUGF2 (1, ("%s 'out of memory'\n", __PRETTY_FUNCTION__));
      g_pIOS->FreeDMAArea (&m_PeakingLUT);
      g_pIOS->FreeDMAArea (&m_LumaLUT);
      return false;
    }
  g_pIOS->MemcpyToDMAArea (&m_PeakingLUT, 0, VDP_IQI_ClippingCurves,
                           sizeof (VDP_IQI_ClippingCurves));

  DEBUGF2 (2, ("%s: m_PeakingLUT: pMemory/pData/ulPhysical/size = %p/%p/%.8lx/%lu\n",
               __PRETTY_FUNCTION__, m_PeakingLUT.pMemory, m_PeakingLUT.pData,
               m_PeakingLUT.ulPhysical, m_PeakingLUT.ulDataSize));
  DEBUGF2 (2, ("%s: m_LumaLUT: pMemory/pData/ulPhysical/size = %p/%p/%.8lx/%lu\n",
               __PRETTY_FUNCTION__, m_LumaLUT.pMemory, m_LumaLUT.pData,
               m_LumaLUT.ulPhysical, m_LumaLUT.ulDataSize));

  /* initialize */
  m_contrast = 128 * 2;
  m_brightness = 128;
  SetDefaults ();
  m_current_config = IQIConfigParams[PCIQIC_FIRST];
  SetConfiguration (&m_current_config);

  m_IqiDemoMoveDirection  = 1;
  m_bIqiDemo              = false;
  m_bIqiDemoParamsChanged = true;

  DEBUGF2 (4, (FEXIT "\n", __PRETTY_FUNCTION__));

  return true;
}


ULONG
CSTmIQI::GetCapabilities (void) const
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));
  return (PLANE_CTRL_CAPS_IQI | PLANE_CTRL_CAPS_PSI_CONTROLS
          | ((m_Revision == IQI_CELL_REV1) ? PLANE_CTRL_CAPS_IQI_CE
                                           : 0));
}


bool
CSTmIQI::SetControl (stm_plane_ctrl_t control,
                     ULONG            value)
{
  bool retval = true;

  DEBUGF2 (8, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_IQI_CONFIG:
      {
      enum PlaneCtrlIQIConfiguration cfg =
        static_cast<enum PlaneCtrlIQIConfiguration>(value);

      if (PCIQIC_COUNT <= cfg)
        return false;

      m_current_config = IQIConfigParams[value];
      retval = SetConfiguration (&m_current_config);
      if (cfg != PCIQIC_BYPASS)
        break;
      }
      /* else fall through so demo state machinery gets disabled */
      value = false;

    case PLANE_CTRL_IQI_DEMO:
      /* order is important so we can avoid locking */
      m_bIqiDemo              = value;
      m_bIqiDemoParamsChanged = true;
      break;

    case PLANE_CTRL_IQI_ENABLES:
      {
      enum PlaneCtrlIQIEnables enables = static_cast<enum PlaneCtrlIQIEnables>(value);
      UCHAR                    iqi_conf = m_current_config.iqi_conf & IQI_ENABLE_CSC;

      if (enables & PCIQIE_ENABLESINXCOMPENSATION)
        iqi_conf |= IQI_ENABLESINXCOMPENSATION;
      if (enables & PCIQIE_ENABLE_PEAKING)
        iqi_conf |= IQI_ENABLE_PEAKING;
      if (enables & PCIQIE_ENABLE_LTI)
        iqi_conf |= IQI_ENABLE_LTI;
      if (enables & PCIQIE_ENABLE_CTI)
        iqi_conf |= IQI_ENABLE_CTI;
      if (enables & PCIQIE_ENABLE_LE)
        iqi_conf |= IQI_ENABLE_LE;
      if (!iqi_conf)
        iqi_conf |= IQI_BYPASS;
      m_current_config.iqi_conf = iqi_conf;
      /* the config is changed in the (next) VSync, as we might have to
         (dynamically) disallow e.g. the LTI depending on the incoming video */
      }
      break;

    case PLANE_CTRL_IQI_PEAKING:
    case PLANE_CTRL_IQI_LTI:
    case PLANE_CTRL_IQI_CTI:
    case PLANE_CTRL_IQI_LE:
      if (!value)
        {
          retval = false;
          break;
        }

      switch (control)
        {
        case PLANE_CTRL_IQI_PEAKING:
          {
          struct _IQIPeakingConf *peaking = reinterpret_cast<struct _IQIPeakingConf *>(value);
          retval = SetPeaking (peaking);
          if (retval)
            m_current_config.peaking = *peaking;
          }
          break;
        case PLANE_CTRL_IQI_LTI:
          {
          struct _IQILtiConf *lti = reinterpret_cast<struct _IQILtiConf *>(value);
          retval = SetLTI (lti);
          if (retval)
            m_current_config.lti = *lti;
          }
          break;
        case PLANE_CTRL_IQI_CTI:
          {
          struct _IQICtiConf *cti = reinterpret_cast<struct _IQICtiConf *>(value);
          retval = SetCTI (cti);
          if (retval)
            m_current_config.cti = *cti;
          }
          break;
        case PLANE_CTRL_IQI_LE:
          {
          struct _IQILEConf *le = reinterpret_cast<struct _IQILEConf *>(value);
          retval = SetLE (le);
          if (retval)
            m_current_config.le = *le;
          }
          break;

        default:
          /* will not be reached */
          retval = false;
          break;
        }
      break;

    case PLANE_CTRL_PSI_BRIGHTNESS:
      m_brightness = value;
      if (m_brightness > 256)
        m_brightness = 256;
      SetContrastBrightness (m_contrast, m_brightness);
      break;

    case PLANE_CTRL_PSI_CONTRAST:
      m_contrast = value * 2;
      if (m_contrast > CONBRI_MAX_CONTRAST_GAIN)
        m_contrast = CONBRI_MAX_CONTRAST_GAIN;
      SetContrastBrightness (m_contrast, m_brightness);
      break;

    case PLANE_CTRL_PSI_SATURATION:
      /* the API's input range is 0...255, whereas the range in the hardware
         is 0...511, with a gain of 1.0 == 128. Convert appropriately. */
      value *= 2;
      if (value > (SAT_SAT_GAIN_MASK >> SAT_SAT_GAIN_SHIFT))
        value = SAT_SAT_GAIN_MASK;
      value <<= SAT_SAT_GAIN_SHIFT;
      SetSaturation (value);
      break;

    default:
      retval = false;
      break;
    }

  return retval;
}


bool
CSTmIQI::GetControl (stm_plane_ctrl_t  control,
                     ULONG            *value) const
{
  bool retval = true;

  DEBUGF2 (4, (FENTRY "\n", __PRETTY_FUNCTION__));

  switch (control)
    {
    case PLANE_CTRL_IQI_CONFIG:
      /* doesn't make sense */
      retval = false;
      break;

    case PLANE_CTRL_IQI_DEMO:
      *value = m_bIqiDemo;
      break;

    case PLANE_CTRL_IQI_ENABLES:
      {
      ULONG enables = PCIQIE_NONE;

      if (!(m_current_config.iqi_conf & IQI_BYPASS))
        {
          if (m_current_config.iqi_conf & IQI_ENABLESINXCOMPENSATION)
            enables |= PCIQIE_ENABLESINXCOMPENSATION;
          if (m_current_config.iqi_conf & IQI_ENABLE_PEAKING)
            enables |= PCIQIE_ENABLE_PEAKING;
          if (m_current_config.iqi_conf & IQI_ENABLE_LTI)
            enables |= PCIQIE_ENABLE_LTI;
          if (m_current_config.iqi_conf & IQI_ENABLE_CTI)
            enables |= PCIQIE_ENABLE_CTI;
          if (m_current_config.iqi_conf & IQI_ENABLE_LE)
            enables |= PCIQIE_ENABLE_LE;
        }
      *value = enables;
      }
      break;

    case PLANE_CTRL_IQI_PEAKING:
    case PLANE_CTRL_IQI_LTI:
    case PLANE_CTRL_IQI_CTI:
    case PLANE_CTRL_IQI_LE:
      if (!value)
        {
          retval = false;
          break;
        }

      switch (control)
        {
        case PLANE_CTRL_IQI_PEAKING:
          {
          struct _IQIPeakingConf *peaking = reinterpret_cast<struct _IQIPeakingConf *>(value);
          *peaking = m_current_config.peaking;
          }
          break;
        case PLANE_CTRL_IQI_LTI:
          {
          struct _IQILtiConf *lti = reinterpret_cast<struct _IQILtiConf *>(value);
          *lti = m_current_config.lti;
          }
          break;
        case PLANE_CTRL_IQI_CTI:
          {
          struct _IQICtiConf *cti = reinterpret_cast<struct _IQICtiConf *>(value);
          *cti = m_current_config.cti;
          }
          break;
        case PLANE_CTRL_IQI_LE:
          {
          struct _IQILEConf *le = reinterpret_cast<struct _IQILEConf *>(value);
          *le = m_current_config.le;
          }
          break;

        default:
          /* will not be reached */
          retval = false;
          break;
        }
      break;

    case PLANE_CTRL_PSI_BRIGHTNESS:
      *value = m_brightness;
      break;
    case PLANE_CTRL_PSI_CONTRAST:
      *value = m_contrast / 2;
      break;
    case PLANE_CTRL_PSI_SATURATION:
      /* as above = map the value */
      *value = GetSaturation () / 2;
      break;

    default:
      retval = false;
      break;
    }

  return retval;
}


#define FREQUENCY_FROM_IQIPFF(x)  (100 * (100 + 25 * (x)) / 675)

IQIPeakingFilterFrequency
CSTmIQI::GetZoomCorrectedFrequency (const stm_display_buffer_t     * const pFrame,
                                    enum IQIPeakingFilterFrequency  PeakingFrequency) const
{
  LONG                      ZoomInRatioBy100;
  LONG                      IndexBy100;
  IQIPeakingFilterFrequency CorrectedPeakingFrequency;

  /* We need to do divide the peaking freq by the zoom ratio: F2 = F1/z */
  /* As we use an enum indexed by n specifying the frequency, we have
     F1 = (n * 0.25 + 1) / 6.75 */
  /* When developping F2 = F1/z this evaluates to n2 = (n1 + 4 - 4*z) / z */
  /* To work in fixed point: z = Z/100 => n2 = (100 * n1 + 400 - 4*Z) / Z */

  /* ZoomInRatioBy100 =
       (LayerCommonData_p->ViewPortParams.OutputRectangle.Width * 100)
       / LayerCommonData_p->ViewPortParams.InputRectangle.Width; */
  ZoomInRatioBy100 = (pFrame->dst.Rect.width * 100) / pFrame->src.Rect.width;

  /* Use 100*Index for rounding purposes */
  /* IndexBy100 =
       (100 * (100 * (S32) PeakingFrequency + 400 - 4*ZoomInRatioBy100))
       / ZoomInRatioBy100; */
  IndexBy100 = ((100 * (100 * (LONG) PeakingFrequency
                       + 400 - 4 * ZoomInRatioBy100))
                / ZoomInRatioBy100);

  if (IndexBy100 < 0)
    {
      DEBUGF2 (3, ("IQI peaking zoom correction: clipping peaking frequency "
                   "from 0.%d to min 0.%d\n",
                   FREQUENCY_FROM_IQIPFF (PeakingFrequency),
                   FREQUENCY_FROM_IQIPFF (0)));
      IndexBy100 = 0;
    }

  /* round correctly */
  CorrectedPeakingFrequency = (IQIPeakingFilterFrequency)
    ((IndexBy100 / 100) + (((IndexBy100 % 100) >= 50) ? 1 : 0));

  DEBUGF2 (3, ("IQI peaking zoom correction: Freq 0.%d has been changed to "
               "0.%d for a zoom-in ratio of %ld.%ld\n",
               FREQUENCY_FROM_IQIPFF (PeakingFrequency),
               FREQUENCY_FROM_IQIPFF (CorrectedPeakingFrequency),
               ZoomInRatioBy100 / 100, ZoomInRatioBy100 % 100));

  if (CorrectedPeakingFrequency > IQIPFF_LAST)
    {
      DEBUGF2 (3, ("IQI peaking zoom correction: clipping peaking frequency "
                   "from 0.%d to max 0.%d",
                   FREQUENCY_FROM_IQIPFF (PeakingFrequency),
                   FREQUENCY_FROM_IQIPFF (IQIPFF_LAST)));
      CorrectedPeakingFrequency = IQIPFF_LAST;
    }

  return CorrectedPeakingFrequency;
}

void
CSTmIQI::CalculateSetup (const stm_display_buffer_t * const pFrame,
                         bool                        bIsDisplayInterlaced,
                         bool                        bIsSourceInterlaced,
                         struct IQISetup            * const setup) const
{
  DENTRYn (8);

  setup->is_709_colorspace
    = (pFrame->src.ulFlags & STM_PLANE_SRC_COLORSPACE_709) ? true : false;

  /* when downscaling, we disable the LTI as it creates bad artifacts */
  setup->lti_allowed = (pFrame->src.Rect.width <= pFrame->dst.Rect.width
                        && pFrame->src.Rect.height <= pFrame->dst.Rect.height);

  /* this is what the reference code in v0.24.0 does (without comments) */
#if 0
  setup->cti_extended = (pFrame->src.Rect.width < pFrame->dst.Rect.width
                         && pFrame->src.Rect.height < pFrame->dst.Rect.height);
#else
  setup->cti_extended = false;
#endif

  /* peaking */
  if (bIsDisplayInterlaced)
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
    = GetZoomCorrectedFrequency (pFrame,
                                 m_current_config.peaking.highpass_filter_cutofffreq);
  enum IQIPeakingFilterFrequency BandPassFilterCenterFrequency
    = GetZoomCorrectedFrequency (pFrame,
                                 m_current_config.peaking.bandpass_filter_centerfreq);

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
      if (((0x100 * pFrame->dst.Rect.width) / pFrame->src.Rect.width) < 0x200)
        {
          DEBUGF2 (3, ("Warning! IQI Peaking: extended size needed but "
                       "zoom-in is < 2.0!"));

          if (BandPassFilterCenterFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT)
            {
              /* clip BandPass to minimum value without extended size */
              DEBUGF2 (3, ("Warning! IQI Peaking: Clipping BandPass filter "
                           "freq 0.%d to minimum freq non extended 0.%d\n",
                           FREQUENCY_FROM_IQIPFF (BandPassFilterCenterFrequency),
                           FREQUENCY_FROM_IQIPFF (IQIPFF_EXTENDED_SIZE_LIMIT + 1)));
              BandPassFilterCenterFrequency = static_cast <IQIPeakingFilterFrequency> (
                (static_cast <unsigned int> (IQIPFF_EXTENDED_SIZE_LIMIT)) + 1);
            }

          if (HighPassFilterCutoffFrequency <= IQIPFF_EXTENDED_SIZE_LIMIT)
            {
              /* clip HighPass to minimum value without extended size */
              DEBUGF2 (3, ("Warning! IQI Peaking: Clipping HighPass filter "
                           "freq 0.%d to minimum freq non extended 0.%d\n",
                           FREQUENCY_FROM_IQIPFF (HighPassFilterCutoffFrequency),
                           FREQUENCY_FROM_IQIPFF (IQIPFF_EXTENDED_SIZE_LIMIT + 1)));
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
      DEBUGF2 (3, ("Error! IQI Peaking: BandPass freq 0.%d is above HighPass "
                   "freq. 0.%d which is forbidden! "
                   "Forcing Bandpass = Highpass\n",
                   FREQUENCY_FROM_IQIPFF (BandPassFilterCenterFrequency),
                   FREQUENCY_FROM_IQIPFF (HighPassFilterCutoffFrequency)));
      BandPassFilterCenterFrequency = HighPassFilterCutoffFrequency;
  }

  setup->peaking.highpass_filter_cutoff_freq = HighPassFilterCutoffFrequency;
  setup->peaking.bandpass_filter_center_freq = BandPassFilterCenterFrequency;

  DEXITn (8);
}


void
CSTmIQI::UpdateHW (const struct IQISetup * const setup,
                   ULONG                  width)
{
  DENTRYn (9);

  ULONG reg;
  UCHAR new_iqi_conf = m_current_config.iqi_conf;

  if (!setup)
    goto out;

  /* LE. FIXME: this can race with SetLE(). At most we will be wrong by one
     VSync, though. */
  reg = ReadIQIReg (IQI_LE_CONF);
  if ((reg & LE_601 && setup->is_709_colorspace)
      || (!(reg & LE_601) && !setup->is_709_colorspace))
    DEBUGF (("%s: colorspace changed to %s\n", __PRETTY_FUNCTION__,
             setup->is_709_colorspace ? "709" : "601"));
  reg |= (setup->is_709_colorspace ? 0 : LE_601);
  WriteIQIReg (IQI_LE_CONF, reg);


  /* LTI */
  if (!setup->lti_allowed)
    new_iqi_conf &= ~IQI_ENABLE_LTI;
  /* CTI */
  reg = ReadIQIReg (IQIr2_CTI_CONF) & ~CTIr2_EXTENDED_MASK;
  //reg |= (setup->cti_extended ? setup->cti_extended_value << CTIr2_EXTENDED_SHIFT : 0;
  WriteIQIReg (IQIr2_CTI_CONF, reg);

  /* peaking */
  reg = ReadIQIReg (IQI_PEAKING_CONF) & ~(PEAKING_ENV_DETECT
                                          | PEAKING_V_PK_EN
                                          | PEAKING_EXTENDED_SIZE);
  reg |= (setup->peaking.env_detect_3_lines ? PEAKING_ENV_DETECT : 0);
  reg |= (setup->peaking.extended_size ? PEAKING_EXTENDED_SIZE : 0);
  if (setup->peaking.vertical_peaking_allowed)
    {
      reg |= (m_current_config.peaking.vertical_peaking
              ? PEAKING_V_PK_EN : 0);

      /* FIXME: vertical gain one does not depend on src/dst */
      ULONG reg2 = ReadIQIReg (IQI_PEAKING_GAIN) & ~PEAKING_GAIN_VERT_GAIN_MASK;
      WriteIQIReg (IQI_PEAKING_GAIN,
                   reg2
                   | (VDP_IQI_PeakingVertGain[m_current_config.peaking.ver_gain] << PEAKING_GAIN_VERT_GAIN_SHIFT));
    }

  /* Peaking Gain according to HP/BP */
  PeakingGainSet (m_current_config.peaking.highpassgain,
                  m_current_config.peaking.bandpassgain,
                  setup->peaking.highpass_filter_cutoff_freq,
                  setup->peaking.bandpass_filter_center_freq);

  WriteIQIReg (IQI_PEAKING_CONF, reg);

  SetConfReg (new_iqi_conf);

out:
  /* check demo mode */
  if (UNLIKELY (m_bIqiDemoParamsChanged))
    {
      if (LIKELY (m_bIqiDemo))
        {
          if (LIKELY (width))
            {
              USHORT start_pixel, end_pixel;

              ASSERTF (m_IqiDemoMoveDirection != 0,
                       ("%s\n", __PRETTY_FUNCTION__));

#if 1
              if ((m_IqiDemoLastStart + m_IqiDemoMoveDirection + width/3) > width)
                m_IqiDemoMoveDirection = -1;
              else if ((m_IqiDemoLastStart + m_IqiDemoMoveDirection) < 0)
                m_IqiDemoMoveDirection = 1;

              m_IqiDemoLastStart += m_IqiDemoMoveDirection;

              start_pixel = m_IqiDemoLastStart;
              end_pixel   = start_pixel + width / 3;
#else
              start_pixel = width / 2;
              end_pixel = width;
#endif
              Demo (start_pixel, end_pixel);
            }
        }
      else
        {
          Demo (0, 0);
          m_bIqiDemoParamsChanged = false;

          m_IqiDemoLastStart     = 0;
          m_IqiDemoMoveDirection = 1;
        }
    }

  /* we don't continuously reload the peaking & luma LUTs to save some
     bandwidth.
     LUTs are loaded _after_ VSYNC only, so have to wait one vsync for
     operation to complete */
  if (UNLIKELY (m_IqiPeakingLutLoadCycles))
    if (--m_IqiPeakingLutLoadCycles == 0)
      DoneLoadPeakingLut ();
  if (UNLIKELY (m_IqiLumaLutLoadCycles))
    if (--m_IqiLumaLutLoadCycles == 0)
      DoneLoadLumaLut ();

  DEXITn (9);
}


void
CSTmIQI::Demo (USHORT start_pixel,
               USHORT end_pixel)
{
  DEBUGF2 (4, ("%s start/end: %hu/%hu\n",
               __PRETTY_FUNCTION__, start_pixel, end_pixel));

  if (start_pixel != end_pixel)
    {
      WriteIQIReg (IQI_DEMO_WINDOW, end_pixel << 16 | start_pixel);
      WriteIQIReg (IQI_CONF, ReadIQIReg (IQI_CONF) | IQI_DEMO_ALL);
    }
  else
    {
      /* disable demo */
      WriteIQIReg (IQI_DEMO_WINDOW, 0x00000000);
      WriteIQIReg (IQI_CONF, ReadIQIReg (IQI_CONF) & ~IQI_DEMO_ALL);
    }
}


void
CSTmIQI::PeakingGainSet (enum IQIPeakingHorVertGain     horGainHighPass,
                         enum IQIPeakingHorVertGain     horGainBandPass,
                         enum IQIPeakingFilterFrequency highPassFilterCutoffFrequency,
                         enum IQIPeakingFilterFrequency bandPassFilterCenterFrequency)
{
  ULONG reg;
  ULONG gain, gain_divider;
  LONG  coeff0, coeff1, coeff2, coeff3, coeff4;
  ULONG coeff0_, coeff1_, coeff2_, coeff3_, coeff4_;

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

          reg = ReadIQIReg (IQI_PEAKING_GAIN) & ~PEAKING_GAIN_HOR_GAIN_MASK;
          WriteIQIReg (IQI_PEAKING_GAIN,
                       reg
                       | (VDP_IQI_PeakingHorGain[horGainHighPass] << PEAKING_GAIN_HOR_GAIN_SHIFT));
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

          if (gain_divider != 0)
            gain /= gain_divider;
          else
            gain = 0;

          reg = ReadIQIReg (IQI_PEAKING_GAIN) & ~PEAKING_GAIN_HOR_GAIN_MASK;
          WriteIQIReg (IQI_PEAKING_GAIN,
                       reg
                       | (VDP_IQI_PeakingHorGain[horGainBandPass] << PEAKING_GAIN_HOR_GAIN_SHIFT));
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
          reg = ReadIQIReg (IQI_PEAKING_GAIN) & ~PEAKING_GAIN_HOR_GAIN_MASK;
          WriteIQIReg (IQI_PEAKING_GAIN,
                       reg
                       | (VDP_IQI_PeakingHorGain[horGainHighPass] << PEAKING_GAIN_HOR_GAIN_SHIFT));
          coeff0 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][0];
          coeff1 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][1];
          coeff2 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][2];
          coeff3 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][3];
          coeff4 = VDP_IQI_HFilterCoefficientHP[highPassFilterCutoffFrequency][4];
        }
      else
        {
          reg = ReadIQIReg (IQI_PEAKING_GAIN) & ~PEAKING_GAIN_HOR_GAIN_MASK;
          WriteIQIReg (IQI_PEAKING_GAIN,
                       reg
                       | (VDP_IQI_PeakingHorGain[horGainBandPass] << PEAKING_GAIN_HOR_GAIN_SHIFT));
          coeff0 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][0];
          coeff1 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][1];
          coeff2 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][2];
          coeff3 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][3];
          coeff4 = VDP_IQI_HFilterCoefficientBP[bandPassFilterCenterFrequency][4];
        }
    }

  coeff0_ = (USHORT) coeff0 & (USHORT) PEAKING_COEFFX_MASK;
  coeff1_ = (USHORT) coeff1 & (USHORT) PEAKING_COEFFX_MASK;
  coeff2_ = (USHORT) coeff2 & (USHORT) PEAKING_COEFFX_MASK;
  coeff3_ = (USHORT) coeff3 & (USHORT) PEAKING_COEFFX_MASK;
  coeff4_ = (USHORT) coeff4 & (USHORT) PEAKING_COEFFX_MASK;
  WriteIQIReg (IQI_PEAKING_COEFX0, coeff0_);
  WriteIQIReg (IQI_PEAKING_COEFX1, coeff1_);
  WriteIQIReg (IQI_PEAKING_COEFX2, coeff2_);
  WriteIQIReg (IQI_PEAKING_COEFX3, coeff3_);
  WriteIQIReg (IQI_PEAKING_COEFX4, coeff4_);
}


bool
CSTmIQI::SetPeaking (const struct _IQIPeakingConf * const peaking)
{
  ULONG reg;

  DENTRYn (2);

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
      DEBUGF ((" invalid IQI peaking parameters\n"));
      return false;
    }

  reg = ReadIQIReg (IQI_PEAKING_CONF) & ~(PEAKING_OVERSHOOT_MASK
                                          | PEAKING_UNDERSHOOT_MASK
                                          | PEAKING_CORING_MODE);
  WriteIQIReg (IQI_PEAKING_CONF,
               reg
               | (peaking->undershoot << PEAKING_UNDERSHOOT_SHIFT)
               | (peaking->overshoot << PEAKING_OVERSHOOT_SHIFT)
               | (peaking->coring_mode ? PEAKING_CORING_MODE : 0));

  reg = ReadIQIReg (IQI_PEAKING_CORING_LEVEL) & ~PEAKING_CORING_LEVEL_MASK;
  WriteIQIReg (IQI_PEAKING_CORING_LEVEL,
               reg
               | (peaking->coring_level << PEAKING_CORING_LEVEL_SHIFT));

  /* clipping */
  return LoadPeakingLut (peaking->clipping_mode);
}

bool
CSTmIQI::SetLTI (const struct _IQILtiConf * const lti)
{
  ULONG reg;

  DENTRYn (2);

  if (!lti
      || lti->selective_edge >= IQILHGO_COUNT
      || lti->hmms_prefilter > IQISTRENGTH_LAST
      || lti->ver_strength > 15
      || lti->hor_strength >= N_ELEMENTS (VDP_IQI_DeltaSlope))
    {
      DEBUGF ((" invalid IQI LTI parameters\n"));
      return false;
    }

  reg = ReadIQIReg (IQI_LTI_CONF) & ~(LTI_CONF_V_LTI_STRENGTH_MASK
                                      | LTI_CONF_H_LTI_STRENGTH_MASK
                                      | LTI_CONF_VERTICAL_LTI
                                      | LTI_CONF_ANTIALIASING
                                      | LTI_CONF_SELECTIVE_EDGE);
  WriteIQIReg (IQI_LTI_CONF,
               reg
               | (lti->ver_strength << LTI_CONF_V_LTI_STRENGTH_SHIFT)
               | (lti->hor_strength << LTI_CONF_H_LTI_STRENGTH_SHIFT)
               | (lti->vertical ? LTI_CONF_VERTICAL_LTI : 0)
               | (lti->anti_aliasing ? LTI_CONF_ANTIALIASING : 0)
               | (lti->selective_edge ? LTI_CONF_SELECTIVE_EDGE : 0));
  WriteIQIReg (IQI_LTI_DELTA_SLOPE, VDP_IQI_DeltaSlope[lti->hor_strength]);

  /* always zero hec height */
  reg = ReadIQIReg (IQI_LTI_CONF) & ~(LTI_CONF_HMMS_PREFILTER_MASK
                                      | LTI_CONF_HEC_HEIGHT_MASK
                                      | LTI_CONF_HEC_GRADIENT_MASK);
  WriteIQIReg (IQI_LTI_CONF,
               reg
               | (lti->selective_edge << LTI_CONF_HEC_GRADIENT_SHIFT)
               | (lti->hmms_prefilter << LTI_CONF_HMMS_PREFILTER_SHIFT));

  DEXITn (2);

  return true;
}

bool
CSTmIQI::SetCTI (const struct _IQICtiConf * const cti)
{
  ULONG reg;

  DENTRYn (2);

  if (!cti
      || cti->strength1 >= IQICS_COUNT
      || cti->strength2 >= IQICS_COUNT)
    {
      DEBUGF ((" invalid IQI CTI parameters\n"));
      return false;
    }

  switch (m_Revision)
    {
    case IQI_CELL_REV2:
      reg = ReadIQIReg (IQIr2_CTI_CONF) & ~(CTIr2_STRENGTH1_MASK
                                            | CTIr2_STRENGTH2_MASK);
      WriteIQIReg (IQIr2_CTI_CONF,
                   (reg
                    | (cti->strength1 << CTIr2_STRENGTH1_SHIFT)
                    | (cti->strength2 << CTIr2_STRENGTH2_SHIFT)
                   ));
      break;

    default:
      /* nothing, should not be reached! */
      return false;
    }

  DEXITn (2);

  return true;
}

bool
CSTmIQI::GenerateFixedCurveTable (const struct _IQILEConfFixedCurveParams * const params,
                                  SHORT                                   * const curve) const
{
  /* B&W Limits */
  ULONG BSLim;
  ULONG WSLim;
  ULONG BlackD1;
  ULONG WhiteD1;
  /* Black Clip setup */
  ULONG xClipBS;
  ULONG yClipBS;
  /* Black Limit */
  LONG  SlopeBSLim;
  LONG  OriginBSLim;
  /* White X&Y Clip */
  ULONG xClipWS;
  LONG  yClipWS;
  /* White Limit */
  LONG  SlopeWSLim;

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
  ULONG xClipBS1 =
    (ULONG) (BSLim
             + (LONG) (((ULONG) (params->BlackStretchGain * 8)
                        * (ULONG) params->BlackStretchInflexionPoint)
                       / 1024));
  ULONG xClipBS2 = (ULONG) ((1024 + (ULONG) (params->BlackStretchGain * 8)));

  xClipBS = (ULONG) (xClipBS1 * 1024) / xClipBS2;
  yClipBS = xClipBS - BSLim;
  }

  { /* Black Limit */
  ULONG SlopeBSLim1 = (ULONG) (yClipBS * 1024);
  LONG  SlopeBSLim2 = (LONG) (BlackD1 - xClipBS);
  SlopeBSLim = ((SlopeBSLim2 == 0)
                ? 1
                : (LONG) (SlopeBSLim1) / SlopeBSLim2);
  OriginBSLim = (-BlackD1 * SlopeBSLim / 1024);
  }

  { /* White X&Y Clip */
  ULONG xClipWS1 =
    (ULONG) (WSLim
             + (LONG) (((ULONG) (params->WhiteStretchGain * 8)
                        * (ULONG) params->WhiteStretchInflexionPoint)
                       / 1024));
  ULONG xClipWS2 = (ULONG) ((1024 + (ULONG) (params->WhiteStretchGain * 8)));

  xClipWS = (ULONG) (xClipWS1 * 1024) / xClipWS2;
  yClipWS = (LONG) (WSLim - xClipWS);
  }

  { /* White Limit */
  ULONG SlopeWSLim1 = (ULONG) (yClipWS * 1024);
  LONG  SlopeWSLim2 = (LONG) (xClipWS - WhiteD1);

  SlopeWSLim = ((SlopeWSLim2 == 0)
                ? 1
                : (LONG) (SlopeWSLim1) / SlopeWSLim2);
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
      LONG Point;

      /* A loop on all values*/
      for (ULONG Loop = 0;
           Loop < VDP_IQI_LUT_10BITS_SIZE;
           Loop += VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR)
        {
          if (Loop <= BlackD1)
            /* From 0 to BlackD1 => Linear LUT*/
            curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;
          else if (Loop < xClipBS)
            {
              /* From BlackD1 to xClipBS */
              Point = (LONG) ((LONG) Loop * (LONG) SlopeBSLim / 1024
                              + (LONG) OriginBSLim);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (SHORT) Point;
            }
          else if (Loop < params->BlackStretchInflexionPoint)
            {
              /* From xClipBS to Black inflection */
              Point = (LONG) ((((LONG) Loop
                                - (LONG) params->BlackStretchInflexionPoint)
                               * (LONG) (params->BlackStretchGain * 8)
                               + 512) / 1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (SHORT) Point;
            }
          else if (Loop < params->WhiteStretchInflexionPoint)
            {
              /* From Black inflection to White inflection */
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;
            }
          else if (Loop < xClipWS)
            {
              /* From White inflection to xClipWS */
              Point = (LONG) ((((LONG) Loop
                                - (LONG) params->WhiteStretchInflexionPoint)
                               * (LONG) (params->WhiteStretchGain * 8)
                               + 512) / 1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (SHORT) Point;
            }
          else if (Loop < WhiteD1)
            {
              /* From xClipWS to WhiteD1 */
              Point = (LONG) (yClipWS
                              + SlopeWSLim * ((LONG) Loop
                                              - (LONG) xClipWS)
                              /1024);
              curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = (SHORT) Point;
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
      for (ULONG Loop = 0;
           Loop < VDP_IQI_LUT_10BITS_SIZE;
           Loop += VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR)
        curve[Loop / VDP_IQI_LUT_10BITS_7BITS_SCALEFACTOR] = 0;

      return false;
    }

  return true;
}

bool
CSTmIQI::GenerateLeLUT (const struct _IQILEConf * const le,
                        USHORT                  *LutToPrg) const
{
  bool   success = true;
  SHORT  LutGainHL[LUMA_LUT_SIZE / sizeof (SHORT)];
  UCHAR  Index;
  USHORT IdentityValue;

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
          IdentityValue = (USHORT) ((Index * (VDP_IQI_LUT_10BITS_SIZE - 1))
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

bool
CSTmIQI::SetLE (const struct _IQILEConf * const le)
{
  ULONG   reg;
  USHORT *LutToPrg = reinterpret_cast <USHORT *> (m_LumaLUT.pData);
  bool    success;

  DENTRYn (2);

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
      DEBUGF ((" invalid IQI LE parameters\n"));
      return false;
    }

  /* we need to create a LUT */
  if ((success = GenerateLeLUT (le, LutToPrg)) != false)
    {
      g_pIOS->FlushCache (LutToPrg, LUMA_LUT_SIZE);

      /* enable CSC if gain != 0 */
      m_current_config.iqi_conf &= ~IQI_ENABLE_CSC;
      m_current_config.iqi_conf |= le->csc_gain ? IQI_ENABLE_CSC : 0;

      /* load LCE LUT and set CSC gain */
      reg = ReadIQIReg (IQI_LE_CONF) & ~LE_WEIGHT_GAIN_MASK;
      reg |= LE_LUT_INIT;
      reg |= (((ULONG) le->csc_gain) << LE_WEIGHT_GAIN_SHIFT);
      WriteIQIReg (IQI_LE_CONF, reg);

      /* The LUT is reloaded on next VSync */
      m_IqiLumaLutLoadCycles = 1;
    }

  DEXITn (2);

  return success;
}

void
CSTmIQI::SetConfReg (UCHAR iqi_conf)
{
  ULONG reg;

  reg = ReadIQIReg (IQI_CONF) & ~(IQI_ENABLE_ALL
                                  | IQI_DEMO_ALL);
  reg &= ~IQI_BYPASS;
  WriteIQIReg (IQI_CONF, reg | iqi_conf);
}

bool
CSTmIQI::SetConfiguration (const struct _IQIConfigurationParameters * const param)
{
  /* peaking */
  if (SetPeaking (&param->peaking)
      /* LTI */
      && SetLTI (&param->lti)
      /* CTI */
      && SetCTI (&param->cti)
      /* LE */
      && SetLE (&param->le))
    {
      /* write enable conf */
      SetConfReg (param->iqi_conf);
      return true;
    }

  return false;
}



bool
CSTmIQI::LoadPeakingLut (enum IQIStrength clipping_mode)
{
  DENTRYn (4);

  if (clipping_mode > IQISTRENGTH_LAST)
    return false;

  ULONG reg = ReadIQIReg (IQI_PEAKING_CONF);
  reg &= ~PEAKING_RANGE_GAIN_LUT_INIT;
  WriteIQIReg (IQI_PEAKING_CONF, reg);

  switch (clipping_mode)
    {
    case IQISTRENGTH_NONE:
    case IQISTRENGTH_WEAK:
    case IQISTRENGTH_STRONG:
      WriteIQIReg (IQI_PEAKING_RANGE_GAIN_LUT_BA,
                   (m_PeakingLUT.ulPhysical
                    + clipping_mode*sizeof (VDP_IQI_ClippingCurves[0])));
      break;
    }

  reg |= PEAKING_RANGE_GAIN_LUT_INIT;
  WriteIQIReg (IQI_PEAKING_CONF, reg);

  m_IqiPeakingLutLoadCycles = 1;

  DEXITn (4);

  return true;
}

void
CSTmIQI::DoneLoadPeakingLut (void)
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

  ULONG reg = ReadIQIReg (IQI_PEAKING_CONF);
  reg &= ~PEAKING_RANGE_GAIN_LUT_INIT;
  WriteIQIReg (IQI_PEAKING_CONF, reg);
}


void
CSTmIQI::DoneLoadLumaLut (void)
{
  DEBUGF2 (4, ("%s\n", __PRETTY_FUNCTION__));

  ULONG reg = ReadIQIReg (IQI_LE_CONF);
  reg &= ~LE_LUT_INIT;
  WriteIQIReg (IQI_LE_CONF, reg);
}


void
CSTmIQI::SetContrastBrightness (USHORT contrast,
                                USHORT brightness)
{
  ULONG reg = ReadIQIReg (IQI_CONBRI) & ~IQI_CONBRI_ALL_MASK;

  long offset = (-2048 + (brightness * 16)
                 + (256 - contrast) * 8);

  /* can become end up out of bounds */
  if (offset > 2047)
    offset = 2047;
  else if (offset < -2048)
    offset = -2048;

  offset <<= 16;
  offset &= CONBRI_CONBRI_OFFSET_MASK;

  WriteIQIReg (IQI_CONBRI,
               (reg
                | offset
                | (contrast << CONBRI_CONTRAST_GAIN_SHIFT)));
}


USHORT
CSTmIQI::GetSaturation (void) const
{
  return ReadIQIReg (IQI_SAT) & IQI_SAT_ALL_MASK;
}

/* gain = attenuation / 256 */
void
CSTmIQI::SetSaturation (USHORT attenuation)
{
  ULONG reg = ReadIQIReg (IQI_SAT) & ~IQI_SAT_ALL_MASK;
  attenuation &= SAT_SAT_GAIN_MASK;
  WriteIQIReg (IQI_SAT, reg | attenuation);
}




bool
CSTmIQI::SetDefaults (void)
{
  const struct _IQIInitParameters * const init = &IQIinitDefaults;

  /* init LTI */
  WriteIQIReg (IQI_LTI_THRESHOLD,
               (0
                | (init->lti.threshold_corner_test << LTI_CORNERTEST_SHIFT)
                | (init->lti.threshold_vert_diff << LTI_VERTDIFF_SHIFT)
                | (init->lti.threshold_min_correlation << LTI_MINCORRELATION_SHIFT)
                | (init->lti.threshold_disturbance << LTI_DISTURBANCE_SHIFT)
               ));

  /* init CTI */
  switch (m_Revision)
    {
    case IQI_CELL_REV2:
      WriteIQIReg (IQIr2_CTI_CONF,
                   (0
                    | (init->cti.coring << CTIr2_COR_SHIFT)
                    | (init->cti.tmed << CTIr2_MED_SHIFT)
                    | (init->cti.tmon << CTIr2_MON_SHIFT)
                   ));
      break;

    default:
      ASSERTF (1 == 2, ("%s: should not be reached!\n", __PRETTY_FUNCTION__));
      return false;
    }

  /* set LE LUT BA */
  WriteIQIReg (IQI_LE_LUT_BA, m_LumaLUT.ulPhysical);

  /* conbri */
  SetContrastBrightness (m_contrast, m_brightness);

  /* iqi sat */
  SetSaturation (SAT_SAT_GAIN_ONE);

  /* init peaking */
  ULONG reg = ReadIQIReg (IQI_PEAKING_CORING_LEVEL) & ~IQI_PEAKING_CORING_LEVEL_ALL_MASK;
  WriteIQIReg (IQI_PEAKING_CORING_LEVEL,
               reg | (init->peaking.coring_level << PEAKING_CORING_LEVEL_SHIFT));

  reg = ReadIQIReg (IQI_PEAKING_CONF) & ~IQI_PEAKING_CONF_ALL_MASK;
  WriteIQIReg (IQI_PEAKING_CONF,
               reg
               | (init->peaking.range_max << PEAKING_RANGEMAX_SHIFT)
               | (init->peaking.coring_mode ? PEAKING_CORING_MODE : 0)
              );
  return LoadPeakingLut (init->peaking.clipping_mode);
}
