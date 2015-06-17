/***********************************************************************
 *
 * File: STMCommon/stmiqi.h
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_IQI_H
#define _STM_IQI_H


enum IQICellRevision
{
  IQI_CELL_REV1, /* not supported */
  IQI_CELL_REV2
};


enum IQIStrength {
  /* Peaking: No clipping        */ /* HMMS (Horizontal Min-Max Search): prefilter is off */
  IQISTRENGTH_NONE,
  /* Peaking: -3dB soft clipping */ /* HMMS: prefilter is in Weak mode */
  IQISTRENGTH_WEAK,
  /* Peaking: -6dB soft clipping */ /* HMMS: prefilter is in strong mode */
  IQISTRENGTH_STRONG,

  IQISTRENGTH_LAST = IQISTRENGTH_STRONG
};

/* coeff used by peaking */
enum IQIPeakingHorVertGain {
  IQIPHVG_N6_0DB, /* v == 0 */
  IQIPHVG_N5_5DB, /* v == 0 */
  IQIPHVG_N5_0DB, /* v == 0 */
  IQIPHVG_N4_5DB, /* v == 0 */
  IQIPHVG_N4_0DB, /* v == 0 */ /* h == IQIPHG_N4_5DB */
  IQIPHVG_N3_5DB, /* v == 0 */
  IQIPHVG_N3_0DB, /* v == 0 */ /* h == IQIPHG_N3_5DB */
  IQIPHVG_N2_5DB, /* v == 0 */
  IQIPHVG_N2_0DB, /* h == IQIPHG_N2_5DB */
  IQIPHVG_N1_5DB,
  IQIPHVG_N1_0DB, /* h == IQIPHG_N1_5DB */
  IQIPHVG_N0_5DB,
  IQIPHVG_0DB,    /* v == IQIPVG_N0_5DB */
  IQIPHVG_P0_5DB, /* v == IQIPVG_0DB */
  IQIPHVG_P1_0DB,
  IQIPHVG_P1_5DB, /* v == IQIPVG_P1_0DB */
  IQIPHVG_P2_0DB,
  IQIPHVG_P2_5DB, /* v == IQIPVG_P2_0DB */
  IQIPHVG_P3_0DB,
  IQIPHVG_P3_5DB, /* v == IQIPVG_P3_0DB */
  IQIPHVG_P4_0DB,
  IQIPHVG_P4_5DB,
  IQIPHVG_P5_0DB,
  IQIPHVG_P5_5DB,
  IQIPHVG_P6_0DB,
  IQIPHVG_P6_5DB,
  IQIPHVG_P7_0DB,
  IQIPHVG_P7_5DB,
  IQIPHVG_P8_0DB, /* v == IQIPVG_P7_5DB */
  IQIPHVG_P8_5DB, /* v == IQIPVG_P8_0DB */
  IQIPHVG_P9_0DB, /* v == IQIPVG_P8_5DB */
  IQIPHVG_P9_5DB, /* v == IQIPVG_P9_0DB */
  IQIPHVG_P10_0DB, /* v == IQIPVG_P9_5DB */
  IQIPHVG_P10_5DB, /* v == IQIPVG_P10_0DB */
  IQIPHVG_P11_0DB, /* v == IQIPVG_P10_5DB */
  IQIPHVG_P11_5DB, /* v == IQIPVG_P11_0DB */
  IQIPHVG_P12_0DB, /* v == IQIPVG_P11_5DB */ /* IQIPHG_P11_5DB */

  IQIPHVG_LAST = IQIPHVG_P12_0DB
};

enum IQIPeakingFilterFrequency {
  /* for these frequencies extended mode is used  */
  IQIPFF_0_15_FsDiv2, /* corresponds to 1.0MHZ  for 1H signal (1.0/6.75 = 0.148)  */
  IQIPFF_0_18_FsDiv2, /* corresponds to 1.25MHZ for 1H signal (1.25/6.75 = 0.185) */
  IQIPFF_0_22_FsDiv2, /* corresponds to 1.5MHZ  for 1H signal (1.5/6.75 = 0.222)  */
  IQIPFF_0_26_FsDiv2, /* corresponds to 1.75MHZ for 1H signal (1.75/6.75 = 0.259) */
  IQIPFF_EXTENDED_SIZE_LIMIT = IQIPFF_0_26_FsDiv2,
  /* for these frequencies extended mode is NOT set */
  IQIPFF_0_30_FsDiv2, /* corresponds to 2.0MHZ  for 1H signal (2.0/6.75 = 0.296)  */
  IQIPFF_0_33_FsDiv2, /* corresponds to 2.25MHZ for 1H signal (2.25/6.75 = 0.333) */
  IQIPFF_0_37_FsDiv2, /* corresponds to 2.5MHZ  for 1H signal (2.5/6.75 = 0.370)  */
  IQIPFF_0_41_FsDiv2, /* corresponds to 2.75MHZ for 1H signal (2.75/6.75 = 0.407) */
  IQIPFF_0_44_FsDiv2, /* corresponds to 3.0MHZ  for 1H signal (3.0/6.75 = 0.444)  */
  IQIPFF_0_48_FsDiv2, /* corresponds to 3.25MHZ for 1H signal (3.25/6.75 = 0.481) */
  IQIPFF_0_52_FsDiv2, /* corresponds to 3.5MHZ  for 1H signal (3.5/6.75 = 0.519)  */
  IQIPFF_0_56_FsDiv2, /* corresponds to 3.75MHZ for 1H signal (3.75/6.75 = 0.556) */
  IQIPFF_0_59_FsDiv2, /* corresponds to 4.0MHZ  for 1H signal (4.0/6.75 = 0.593)  */
  IQIPFF_0_63_FsDiv2, /* corresponds to 4.25MHZ for 1H signal (4.25/6.75 = 0.627) */

  IQIPFF_LAST = IQIPFF_0_63_FsDiv2,
  IQIPFF_COUNT
};


enum IQIPeakingOvershootUndershootFactor {
  IQIPOUF_100,
  IQIPOUF_075,
  IQIPOUF_050,
  IQIPOUF_025,

  IQIPOUF_LAST = IQIPOUF_025
};

enum IQILtiHecGradientOffset {
  IQILHGO_0,
  IQILHGO_4,
  IQILHGO_8,

  IQILHGO_COUNT
};

enum IQICtiStrength {
  IQICS_NONE,
  IQICS_MIN,
  IQICS_MEDIUM,
  IQICS_STRONG,

  IQICS_COUNT
};


struct _IQIPeakingConf {
  /* Undershoot factor */
  enum IQIPeakingOvershootUndershootFactor undershoot;
  /* Overshoot factor */
  enum IQIPeakingOvershootUndershootFactor overshoot;
  /* If set the peaking is done in chroma adaptive coring mode,
     if reset it is done in manual coring mode */
  bool coring_mode;
  /* peaking coring value: applies to both V&H. 0 ... 63 */
  UCHAR coring_level;
  /* Vertical peaking enabled(1)/disabled(0) */
  bool vertical_peaking;
  /* Vertical gain in dB in the range [-2.5, +7.5] */
  enum IQIPeakingHorVertGain ver_gain;
  /* Iqi peaking clipping mode */
  enum IQIStrength clipping_mode;

  /* peaking filter cutoff frequency */
  enum IQIPeakingFilterFrequency highpass_filter_cutofffreq;
  /* peaking filter center frequency */
  enum IQIPeakingFilterFrequency bandpass_filter_centerfreq;
  /* Horizontal gain in dB in the range [-6, +12] for the highpass filter,
     step is 0.5 db */
  enum IQIPeakingHorVertGain highpassgain;
  /* Horizontal gain in dB in the range [-6, +12] for the bandpass filter,
     step is 0.5 db */
  enum IQIPeakingHorVertGain bandpassgain;
};

struct _IQILtiConf {
  enum IQILtiHecGradientOffset selective_edge;
  enum IQIStrength hmms_prefilter;
  bool anti_aliasing;
  bool vertical;
  UCHAR ver_strength; /* 0 ... 15 */
  UCHAR hor_strength; /* 0 ... 31 */
};

struct _IQICtiConf {
  enum IQICtiStrength strength1;
  enum IQICtiStrength strength2;
};


struct _IQILEConfFixedCurveParams {
  USHORT BlackStretchInflexionPoint; /* Black Stretch Inflexion Point (10 bits: 0..1023) */
  USHORT BlackStretchLimitPoint;     /* Black Stretch Limit Point (10 bits: 0..1023)     */
  USHORT WhiteStretchInflexionPoint; /* White Stretch Inflexion Point (10 bits: 0..1023) */
  USHORT WhiteStretchLimitPoint;     /* White Stretch Limit Point (10 bits: 0..1023)     */
  UCHAR  BlackStretchGain;           /* Black Stretch Gain            (% : 0..100)       */
  UCHAR  WhiteStretchGain;           /* White Stretch Gain            (% : 0..100)       */
};

struct _IQILEConf {
  UCHAR csc_gain; /* Chroma Saturation Compensation Gain 0 ... 31 */
  bool  fixed_curve; /* If TRUE the contrast enhancer fixed curve is enabled */
  struct _IQILEConfFixedCurveParams fixed_curve_params;

  const SHORT *custom_curve; /* Custom curve contrast enhancer. Size of data is 128
                                elements, centered around zero. If NULL, then the
                                custom curve is unused. */
};


struct _IQIConfigurationParameters {
  UCHAR iqi_conf; /* IQI_CONF register -> don't put demo mode in here! */
  struct _IQIPeakingConf peaking;
  struct _IQILtiConf lti;
  struct _IQICtiConf cti;
  struct _IQILEConf le;
};


#ifdef __cplusplus

#include <stmdisplayplane.h>
#include <Generic/IOS.h>


struct IQISetup
{
  bool is_709_colorspace;
  bool lti_allowed;
  bool cti_extended;
  struct {
    bool env_detect_3_lines;
    bool vertical_peaking_allowed;
    bool extended_size;
    enum IQIPeakingFilterFrequency highpass_filter_cutoff_freq;
    enum IQIPeakingFilterFrequency bandpass_filter_center_freq;
  } peaking;
};

class CSTmIQI
{
public:
  CSTmIQI (enum IQICellRevision revision,
           ULONG                baseAddr);
  virtual ~CSTmIQI (void);

  bool Create (void);

  bool SetControl (stm_plane_ctrl_t control,
                   ULONG            value);
  bool GetControl (stm_plane_ctrl_t  control,
                   ULONG            *value) const;

  virtual ULONG GetCapabilities (void) const; /* PLANE_CTRL_CAPS_IQIxxx */

  void CalculateSetup (const stm_display_buffer_t * const pFrame,
                       bool                        bIsDisplayInterlaced,
                       bool                        bIsSourceInterlaced,
                       struct IQISetup            * const setup) const __attribute__((nonnull(1,2)));

  void UpdateHW (const struct IQISetup * const setup,
                 ULONG                  width);


private:
  enum IQICellRevision m_Revision;

  ULONG    m_baseAddress;
  DMA_Area m_PeakingLUT;
  DMA_Area m_LumaLUT;

  bool  m_bIqiDemo;
  bool  m_bIqiDemoParamsChanged;
  short m_IqiDemoLastStart;
  short m_IqiDemoMoveDirection; /* 1: ltr, -1: rtl */
  UCHAR m_IqiPeakingLutLoadCycles;
  UCHAR m_IqiLumaLutLoadCycles;

  struct _IQIConfigurationParameters m_current_config;

  USHORT m_contrast;
  USHORT m_brightness;

  bool SetDefaults (void);

  bool SetConfiguration (const struct _IQIConfigurationParameters * const param) __attribute__((nonnull));

  void SetContrastBrightness (USHORT contrast, USHORT brightness);
  USHORT GetSaturation (void) const;
  void SetSaturation (USHORT attenuation);

  bool LoadPeakingLut (enum IQIStrength               clipping_mode) __attribute__((warn_unused_result));
  void PeakingGainSet (enum IQIPeakingHorVertGain     gain_highpass,
                       enum IQIPeakingHorVertGain     gain_bandpass,
                       enum IQIPeakingFilterFrequency cutoff_freq,
                       enum IQIPeakingFilterFrequency center_freq);
  void DoneLoadPeakingLut (void);
  void DoneLoadLumaLut    (void);

  void SetConfReg (UCHAR conf);

  IQIPeakingFilterFrequency
    GetZoomCorrectedFrequency (const stm_display_buffer_t     * const pFrame,
                               enum IQIPeakingFilterFrequency  PeakingFrequency) const __attribute__((nonnull(1)));

  bool SetPeaking (const struct _IQIPeakingConf * const peaking) __attribute__((nonnull,warn_unused_result));
  bool SetLTI (const struct _IQILtiConf * const lti) __attribute__((nonnull,warn_unused_result));
  bool SetCTI (const struct _IQICtiConf * const cti) __attribute__((nonnull,warn_unused_result));

  bool GenerateFixedCurveTable (const struct _IQILEConfFixedCurveParams * const params,
                                SHORT                                   * const curve) const __attribute__((nonnull,warn_unused_result));
  bool GenerateLeLUT (const struct _IQILEConf * const le,
                      USHORT                  *LutToPrg) const __attribute__((nonnull,warn_unused_result));
  bool SetLE (const struct _IQILEConf * const le) __attribute__((nonnull,warn_unused_result));

  void Demo (USHORT start_pixel,
             USHORT end_pixel); /* if start == end -> demo off*/

  void  WriteIQIReg (ULONG reg,
                     ULONG value) { DEBUGF2 (9, ("w %p <- %#.8lx\n",
                                                 (volatile ULONG *) m_baseAddress + (reg>>2),
                                                 value));
                                    g_pIOS->WriteRegister ((volatile ULONG *) m_baseAddress + (reg>>2),
                                                               value); }
  ULONG ReadIQIReg (ULONG reg) const { const ULONG value = g_pIOS->ReadRegister ((volatile ULONG *) m_baseAddress + (reg>>2));
                                       DEBUGF2 (9, ("r %p -> %#.8lx\n",
                                                    (volatile ULONG *) m_baseAddress + (reg>>2),
                                                    value));
                                       return value; }
};

#endif /* __cplusplus */


#endif /* _STM_IQI_H */
