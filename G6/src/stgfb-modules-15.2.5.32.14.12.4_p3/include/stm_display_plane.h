/***********************************************************************
 *
 * File: include/stm_display_plane.h
 * Copyright (c) 2000-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_PLANE_H
#define _STM_DISPLAY_PLANE_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file stm_display_plane.h
 *  \brief C interface to display plane objects
 */


/*! \enum stm_plane_feature_t
 *
 *  \brief This type provides the list of plane features.
 *         Each of the features might or might not be present for a given plane
 *         of the device..
 */
typedef enum stm_plane_feature_e
{
    PLANE_FEAT_VIDEO_ACC,
    PLANE_FEAT_VIDEO_ACM,
    PLANE_FEAT_VIDEO_AFM,
    PLANE_FEAT_VIDEO_CCS,
    PLANE_FEAT_VIDEO_DNR,
    PLANE_FEAT_GLOBAL_COLOR,
    PLANE_FEAT_VIDEO_MADI,
    PLANE_FEAT_VIDEO_MCTI,
    PLANE_FEAT_VIDEO_MNR,
    PLANE_FEAT_VIDEO_SCALING,
    PLANE_FEAT_VIDEO_SHARPNESS,
    PLANE_FEAT_PROCESSING_COMPRESSION,
    PLANE_FEAT_VIDEO_TNR,
    PLANE_FEAT_WINDOW_MODE,
    PLANE_FEAT_LATENCY,
    PLANE_FEAT_FLIP,
    PLANE_FEAT_COLOR_FILL,
    PLANE_FEAT_3D,
    PLANE_FEAT_FRC,
    PLANE_FEAT_FLICKER_FILTER,
    PLANE_FEAT_VIDEO_XVP,
    PLANE_FEAT_VIDEO_FMD,
    PLANE_FEAT_VIDEO_IQI, //TODO: remove this in V4L layer first
    PLANE_FEAT_SRC_COLOR_KEY,
    PLANE_FEAT_DST_COLOR_KEY,
    PLANE_FEAT_TRANSPARENCY,
    PLANE_FEAT_VIDEO_IQI_PEAKING,
    PLANE_FEAT_VIDEO_IQI_LE,
    PLANE_FEAT_VIDEO_IQI_CTI
}stm_plane_feature_t ;

/*! \brief IQI configuration setups
 *  \ctrlarg CTRL_IQI_CONFIG
 */
typedef enum PlaneCtrlIQIConfiguration {
  PCIQIC_FIRST,           /*!< this is the default config used by the driver */

  PCIQIC_BYPASS = PCIQIC_FIRST,       /*!< IQI hardware is bypassed */

  PCIQIC_ST_DEFAULT,                  /*!< */
  PCIQIC_ST_SOFT = PCIQIC_ST_DEFAULT, /*!< */
  PCIQIC_ST_MEDIUM,                 /*!< */
  PCIQIC_ST_STRONG,                 /*!< */

  PCIQIC_COUNT                        /*!< */
} PlaneCtrlIQIConfiguration_e;


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


/*! \enum stm_plane_filter_set_t
 *
 *  \brief This type provides the list of video filter sets
 */
typedef enum stm_plane_filter_set_e
  {
    FILTER_SET_LEGACY,
    FILTER_SET_MEDIUM,
    FILTER_SET_SHARP,
    FILTER_SET_SMOOTH
  } stm_plane_filter_set_t;


typedef struct _IQIPeakingConf {
  /* Undershoot factor */
  enum IQIPeakingOvershootUndershootFactor undershoot;
  /* Overshoot factor */
  enum IQIPeakingOvershootUndershootFactor overshoot;
  /* If set (1) the peaking is done in chroma adaptive coring mode,
     if reset (0) it is done in manual coring mode */
  uint8_t coring_mode;
  /* peaking coring value: applies to both V&H. 0 ... 63 */
  uint8_t coring_level;
  /* Vertical peaking enabled(1)/disabled(0) */
  uint8_t vertical_peaking;
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
} stm_iqi_peaking_conf_t;


typedef struct _IQILtiConf {
  enum IQILtiHecGradientOffset selective_edge;
  enum IQIStrength hmms_prefilter;
  uint8_t anti_aliasing; /* boolean 0..1 */
  uint8_t vertical;      /* boolean 0..1 */
  uint8_t ver_strength;  /* 0 ... 15     */
  uint8_t hor_strength;  /* 0 ... 31     */
} stm_iqi_lti_conf_t;


typedef struct _IQICtiConf {
  enum IQICtiStrength strength1;
  enum IQICtiStrength strength2;
} stm_iqi_cti_conf_t;


typedef struct _IQILEConfFixedCurveParams {
  uint16_t BlackStretchInflexionPoint; /* Black Stretch Inflexion Point (10 bits: 0..1023) */
  uint16_t BlackStretchLimitPoint;     /* Black Stretch Limit Point (10 bits: 0..1023)     */
  uint16_t WhiteStretchInflexionPoint; /* White Stretch Inflexion Point (10 bits: 0..1023) */
  uint16_t WhiteStretchLimitPoint;     /* White Stretch Limit Point (10 bits: 0..1023)     */
  uint8_t  BlackStretchGain;           /* Black Stretch Gain            (% : 0..100)       */
  uint8_t  WhiteStretchGain;           /* White Stretch Gain            (% : 0..100)       */
} stm_iqi_le_fixed_curve_params_t;


typedef struct _IQILEConf {
  uint8_t csc_gain;    /* Chroma Saturation Compensation Gain 0 ... 31 */
  uint8_t fixed_curve; /* If TRUE the contrast enhancer fixed curve is enabled */

  stm_iqi_le_fixed_curve_params_t fixed_curve_params;

  const int16_t *custom_curve; /* Custom curve contrast enhancer. Size of data is 128
                                  elements, centered around zero. If NULL, then the
                                  custom curve is unused. */
} stm_iqi_le_conf_t;

typedef enum stm_display_iqi_control_e
{
  CTRL_IQI_CONFIG=1<<8,        /*!< \values ::PlaneCtrlIQIConfiguration */
  CTRL_IQI_DEMO,          /*!< IQI demo on/off, default: off  */
  CTRL_IQI_ENABLES,       /*!< \values ::PlaneCtrlIQIEnables */
  CTRL_IQI_PEAKING,       /*!< IQI peaking */
  CTRL_IQI_LTI,           /*!< IQI Luma Transient Improvement */
  CTRL_IQI_CTI,           /*!< IQI Chroma Transient Improvement */
  CTRL_IQI_LE,            /*!< IQI Luma Enhancer */
} stm_display_iqi_control_t;

/*! \brief IQI processing block enables
 * \ctrlarg CTRL_IQI_ENABLES
 */
typedef enum PlaneCtrlIQIEnables {
  PCIQIE_NONE                   = 0x00000000, /*!< */
  PCIQIE_ENABLESINXCOMPENSATION = 0x00000001, /*!< */
  PCIQIE_ENABLE_PEAKING         = 0x00000002, /*!< */
  PCIQIE_ENABLE_LTI             = 0x00000004, /*!< */
  PCIQIE_ENABLE_CTI             = 0x00000008, /*!< */
  PCIQIE_ENABLE_LE              = 0x00000010, /*!< */

  PCIQIE_ALL                    = 0x0000001f  /*!< */
} PlaneCtrlIQIEnables_e;

/*! \brief Control the FlexVP XP70 processing mode
 *  \ctrlarg PLANE_CTRL_XVP_CONFIG
 */
enum PlaneCtrlxVPConfiguration {
  PCxVPC_FIRST,                 /*!< default mode used by the driver          */

  PCxVPC_BYPASS = PCxVPC_FIRST, /*!< FlexVP is bypassed completely            */
  PCxVPC_FILMGRAIN,             /*!< Film grain addition                      */
  PCxVPC_TNR,                   /*!< Temporal Noise Reduction                 */
  PCxVPC_TNR_BYPASS,            /*!< Process buffers as if TNR was active,
                                     keeping the latency of TNR, but disable the
                                     actual video processing                  */
  PCxVPC_TNR_NLEOVER,           /*!< TNR enabled but allow override of the
                                     NLE parameter for debugging              */

  PCxVPC_COUNT                  /*!< */
};


/*! \brief Control video pipeline de-interlacer mode
 *  \ctrlarg PLANE_CTRL_DEI_MODE
 */
enum PlaneCtrlDEIConfiguration {
  PCDEIC_FIRST,                   /*!< default mode used by the driver        */

  PCDEIC_3DMOTION = PCDEIC_FIRST, /*!< Use motion based interpolation         */
  PCDEIC_DISABLED,                /*!< Disable hardware de-interlacer         */
  PCDEIC_MEDIAN,                  /*!< Use median (spatial) interpolation     */

  PCDEIC_COUNT                    /*!< */
};


typedef struct stm_fmd_params_s
{
  uint32_t ulH_block_sz;  /*!< Horizontal block size for BBD */
  uint32_t ulV_block_sz;  /*!< Vertical block size for BBD */
  uint32_t count_miss;    /*!< Delay for a film mode detection */
  uint32_t count_still;   /*!< Delay for a still mode detection */
  uint32_t t_noise;       /*!< Noise threshold */
  uint32_t k_cfd1;        /*!< Consecutive field difference factor 1 */
  uint32_t k_cfd2;        /*!< Consecutive field difference factor 2 */
  uint32_t k_cfd3;        /*!< Consecutive field difference factor 3 */
  uint32_t t_mov;         /*!< Moving pixel threshold */
  uint32_t t_num_mov_pix; /*!< Moving block threshold */
  uint32_t t_repeat;      /*!< Threshold on BBD for a field repetition */
  uint32_t t_scene;       /*!< Threshold on BBD for a scene change  */
  uint32_t k_scene;       /*!< Percentage of blocks with BBD > t_scene */
  uint8_t  d_scene;       /*!< Scene change detection delay (1,2,3 or 4) */
} stm_fmd_params_t;

typedef struct stm_iqi_peaking_tuning_data_s
{
/*
    stm_display_iqi_control_t control;
    union
    {
        PlaneCtrlIQIConfiguration_e config;
        uint32_t demo;
        PlaneCtrlIQIEnables_e enables;
        stm_iqi_peaking_conf_t peaking;
        stm_iqi_lti_conf_t lti;
        stm_iqi_cti_conf_t cti;
        stm_iqi_le_conf_t le;
    } data;
*/
    stm_iqi_peaking_conf_t conf;
} stm_iqi_peaking_tuning_data_t;

typedef struct stm_iqi_lti_tuning_data_s
{
    stm_iqi_lti_conf_t conf;
} stm_iqi_lti_tuning_data_t;

typedef struct stm_iqi_cti_tuning_data_s
{
    stm_iqi_cti_conf_t conf;
} stm_iqi_cti_tuning_data_t;

typedef struct stm_iqi_le_tuning_data_s
{
    stm_iqi_le_conf_t conf;
} stm_iqi_le_tuning_data_t;



typedef struct stm_madi_tuning_data_s
{
    enum PlaneCtrlDEIConfiguration config;
} stm_madi_tuning_data_t;

typedef struct stm_fmd_tuning_data_s
{
    stm_fmd_params_t params;
} stm_fmd_tuning_data_t;

typedef struct stm_xvp_tuning_data_s
{
    enum PlaneCtrlxVPConfiguration config;
} stm_xvp_tuning_data_t;

typedef enum stm_display_plane_control_e
{
    /* A plane video control that configures the brightness parameter.*/
    PLANE_CTRL_BRIGHTNESS=1,
    /* A plane video control that configures the saturation parameter.*/
    PLANE_CTRL_SATURATION,
    /* A plane video control that configures the contrast parameter.*/
    PLANE_CTRL_CONTRAST,
    /* A plane video control that configures the tint parameter. */
    PLANE_CTRL_TINT,
    /* A plane video control that configures the state of the Adaptive Contrast and Control (ACC).*/
    PLANE_CTRL_VIDEO_ACC_STATE,
    /* A plane video control that configures the state of the Active Color Management (ACM).*/
    PLANE_CTRL_VIDEO_ACM_STATE,
    /* A plane video control that configures the state of the Advanced Film Mode (AFM).*/
    PLANE_CTRL_VIDEO_AFM_STATE,
    /* A plane video control that configures the state of the Cross Color Suppression (CCS).*/
    PLANE_CTRL_VIDEO_CCS_STATE,
    /* A plane video control that configures the state of the Digital Noise Reduction (DNR).*/
    PLANE_CTRL_VIDEO_DNR_STATE,
    /* A plane video control that configures the state of the Motion Adaptive De-interlacing (MADi).*/
    PLANE_CTRL_VIDEO_MADI_STATE,
    /* A plane video control that configures the mode of the Motion Adaptive De-interlacing (MADi).*/
    PLANE_CTRL_VIDEO_MADI_MODE,
    /* A plane video control that configures the state of the MCTi<99> (Motion Adaptive De-interlacing).*/
    PLANE_CTRL_VIDEO_MCTI_STATE,
    /* A plane video control that configures the input telecine.*/
    PLANE_CTRL_VIDEO_INPUT_TELECINE,
    /* A plane video control that configures the state of the Mosquito Noise Reduction (MNR).*/
    PLANE_CTRL_VIDEO_MNR_STATE,
    /* A plane video control that configures the mode of the scaler.*/
    PLANE_CTRL_VIDEO_SCALER_MODE,
    /* A plane video control that configures the state of the Sharpness.*/
    PLANE_CTRL_VIDEO_SHARPNESS_STATE,
    /* A plane video control that configures the sharpness value.*/
    PLANE_CTRL_VIDEO_SHARPNESS_VALUE,
    /* A plane video control that configures the state of the Temporal Noise Reduction (TNR).*/
    PLANE_CTRL_VIDEO_TNR_STATE,
    /* A plane video control that configures the mode of the TNR.*/
    PLANE_CTRL_VIDEO_TNR_MODE,
    /* A plane video control that configures the luma compression mode.*/
    PLANE_CTRL_VIDEO_LUMA_COMPRESSION,
    /* A plane video control that configures the RGB compression mode.*/
    PLANE_CTRL_VIDEO_RGB_COMPRESSION,
    /* A plane video control that configures the chroma compression mode.*/
    PLANE_CTRL_VIDEO_CHROMA_COMPRESSION,
    /* A plane video control that configures the source compression mode.*/
    PLANE_CTRL_VIDEO_SOURCE_COMPRESSION,
    /* A plane video control that configures the DNR compression mode by reducing data path resolution.*/
    PLANE_CTRL_VIDEO_DNR_COMPRESSION,
    /* A plane video control that configures the TNR luma compression.*/
    PLANE_CTRL_VIDEO_TNR_LUMA_COMPRESSION,
    /* A plane video control that configures the red color component gain parameter.*/
    PLANE_CTRL_RED_GAIN,
    /* A plane video control that configures the red color component offset parameter.*/
    PLANE_CTRL_RED_OFFSET,
    /* A plane video control that configures the green color component gain parameter.*/
    PLANE_CTRL_GREEN_GAIN,
    /* A plane video control that configures the green color component offset parameter.*/
    PLANE_CTRL_GREEN_OFFSET,
    /* A plane video control that configures the blue component gain parameter.*/
    PLANE_CTRL_BLUE_GAIN,
    /* A plane video control that configures the blue color component offset parameter.*/
    PLANE_CTRL_BLUE_OFFSET,
    /* A plane control that configures the central region liniar percentage for a panoramic scaling.*/
    PLANE_CTRL_CENTRAL_REGION,
    /* A plane control that configures the input window mode.*/
    PLANE_CTRL_INPUT_WINDOW_MODE,
    /* A plane control that configures the input window value.*/
    PLANE_CTRL_INPUT_WINDOW_VALUE,
    /* A plane control that configures the output window mode.*/
    PLANE_CTRL_OUTPUT_WINDOW_MODE,
    /* A plane control that configures the output window value.*/
    PLANE_CTRL_OUTPUT_WINDOW_VALUE,
    /* A plane control that configures the output background window parameters.*/
    PLANE_CTRL_OUTPUT_BKGND_WINDOW,
    /* A plane compound control that indicates the minimum plane latency (= best case)*/
    PLANE_CTRL_MIN_VIDEO_LATENCY,
    /* A plane compound control that indicates the maximum plane latency (= worse case)*/
    PLANE_CTRL_MAX_VIDEO_LATENCY,
    /* A plane control that configures the flip state: horizontal/vertical*/
    PLANE_CTRL_FLIP,
    /* A plane control that configures the color fill mode.*/
    PLANE_CTRL_COLOR_FILL_MODE,
    /* A plane control that configures the color fill state.*/
    PLANE_CTRL_COLOR_FILL_STATE,
    /* A plane control that configures the color fill value.*/
    PLANE_CTRL_COLOR_FILL_VALUE,
    /* A plane control that configures background color.*/
    PLANE_CTRL_BKGND_COLOR_VALUE,
    /* A plane control that configures the 3D depth.*/
    PLANE_CTRL_3D_DEPTH,
    /* A plane control that configures the 3D polarity.*/
    PLANE_CTRL_3D_SWAP,
    /* A plane video control that configures the state of the flicker filter.*/
    PLANE_CTRL_FLICKER_FILTER_STATE,
    /* A plane video control that configures the mode of the flicker filter.*/
    PLANE_CTRL_FLICKER_FILTER_MODE,
    /* A plane video control that configures the state of the XVP.*/
    PLANE_CTRL_VIDEO_XVP_STATE,
    /* A plane video control that configures the mode of the XVP.*/
    PLANE_CTRL_VIDEO_XVP_MODE,
    /* A plane video control that configures the state of the FMD.*/
    PLANE_CTRL_VIDEO_FMD_STATE,
    /* A plane video control that configures the mode of the FMD.*/
    PLANE_CTRL_VIDEO_FMD_MODE,
    /* A plane video control that configures the state of the IQI PK ALGO.*/
    PLANE_CTRL_VIDEO_IQI_PEAKING_STATE,
    /* A plane video control that configures the state of the IQI CTI ALGO */
    PLANE_CTRL_VIDEO_IQI_CTI_STATE,
    /* A plane video control that configures the state of the IQI LE ALGO */
    PLANE_CTRL_VIDEO_IQI_LE_STATE,

    /* A plane video control that configures the mode of the IQI.*/
    PLANE_CTRL_VIDEO_IQI_MODE,
    /* A plane control that configures the state of the source color key.*/
    PLANE_CTRL_SRC_COLOR_STATE,
    /* A plane control that configures the source color key value.*/
    PLANE_CTRL_SRC_COLOR_VALUE,
    /* A plane control that configures the state of the destination color key */
    PLANE_CTRL_DST_COLOR_STATE,
    /* A plane control that configures the destination color key value. */
    PLANE_CTRL_DST_COLOR_VALUE,
    /* A plane control that configures the state of the Transparency.*/
    PLANE_CTRL_TRANSPARENCY_STATE, /*!< \value 'CONTROL_ON' means transparency is set
                                    by the plane control PLANE_CTRL_TRANSPARENCY_VALUE.
                                    Otherwise ('CONTROL_OFF') transparency can be set
                                    buffer per buffer thanks to both source flag
                                    'STM_BUFFER_SRC_CONST_ALPHA' and "ulConstAlpha"
                                    value of the buffer descriptor. */
    /* A plane control that configures the transparency value. */
    PLANE_CTRL_TRANSPARENCY_VALUE, /*!< \values 0xXXXXXXaa where:
                                   * XX = dont care,
                                   * aa = global alpha range 0-255,
                                   * * aa = 0 means full transparent plane.
                                   * * aa = 255 means full opaque plane.
                                   * * default value is 255 (full opaque).
                                   */
    /* A plane control that configures the global gain value.*/
    PLANE_CTRL_GLOBAL_GAIN_VALUE,
    /* A plane control that configures the aspect ratio conversion used when
       the source and output don’t have the same aspect ratio. (Only active,
       when input and output windows are in Automatic mode). */
    PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE,
    /* This is a plane control that selects the hiding mode policy. */
    PLANE_CTRL_HIDE_MODE_POLICY,


    // TEMPORARY controls: these controls need to be ported to match the new STKPI API
    // Until then they are added here and handled like previously in the code
    PLANE_CTRL_SCREEN_XY,         /*!< \values (signed short)y<<16 | (signed short)x
                                 *   \caps PLANE_CTRL_CAPS_SCREEN_XY
                                 */
    PLANE_CTRL_COLOR_KEY,         /*!< \values pointer to ::stm_color_key_config_s
                                   'immediate' activation, unless a queued
                                   buffer has something set, in which case it
                                   overrides this configuration. */
    PLANE_CTRL_BUFFER_ADDRESS,    /*!< \values physical buffer address when buffers are
                                   marked with STM_PLANE_SRC_DIRECT_BUFFER_ADDR */

    PLANE_CTRL_ALPHA_RAMP,        /*!< \values 0xXXXXbbaa where:
                                 * XX = dont care,
                                 * aa = alpha0 range 0-255, default 0
                                 * bb = alpha1 range 0-255, default 255
                                 *
                                 * \caps PLANE_CTRL_CAPS_ALPHA_RAMP
                                 */
    // End of temporary controls

    /* returns the average latency measured for the last 10 output frames. */
    PLANE_CTRL_VIDEO_LATENCY_AVERAGE_10,

    /* control used to set the desired processing type */
    PLANE_CTRL_PROCESSING_TYPE,

    /* control used to set the desired latency profile */
    PLANE_CTRL_VIDEO_LATENCY_MODE,

    /* set the filter set of video plane */
    PLANE_CTRL_FILTER_SET,

    /* A plane control that connect a plane to a source.
       We now have a STKPI function and a control with similar behaviors but there is a noticeable difference:
       - stm_display_plane_connect_to_source() is applied immediately.
       - PLANE_CTRL_CONNECT_TO_SOURCE is applied at next VSync.It allows to do synchronous connections/disconnections for the Swap use case.
       The value passed to this control should contain the handle of the source (stm_display_source_h)
    */
    PLANE_CTRL_CONNECT_TO_SOURCE,

    /* A plane control that disconnect a plane from a source.
       We now have a STKPI function and a control with similar behaviors but there is a noticeable difference:
       - stm_display_plane_disconnect_from_source() is applied immediately.
       - PLANE_CTRL_DISCONNECT_FROM_SOURCE is applied at next VSync.It allows to do synchronous connections/disconnections for the Swap use case.
       The value passed to this control should contain the handle of the source (stm_display_source_h)
    */
    PLANE_CTRL_DISCONNECT_FROM_SOURCE,

    /* A plane control to set the DEPTH of a plane.
       We now have a STKPI function and a control with similar behaviors but there is a noticeable difference:
       - stm_display_plane_set_depth() immediately programs the HW registers so the change is visible on TV at next VSync.
       - PLANE_CTRL_DEPTH is applied at next VSync to program the HW registers and the change is visible on TV at next-next VSync.
       The value passed to this control should contain the depth of the plane.
    */
    PLANE_CTRL_DEPTH,

    /* A plane control to test if a DEPTH is valid for a plane.
       It has the same behavior as stm_display_plane_set_depth() with "activate" set to false.
       The value passed to this control should contain the proposed depth for this plane.
    */
    PLANE_CTRL_TEST_DEPTH,

    /* A plane control to set the visibility of a plane.
       We now have a STKPI function and a control with similar behaviors but there is a noticeable difference:
       - stm_display_plane_show() immediately programs the HW registers so the change is visible on TV at next VSync.
       - PLANE_CTRL_VISIBILITY is applied at next VSync to program the HW registers and the change is visible on TV at next-next VSync.
       The value passed to this control should contain "PLANE_NOT_VISIBLE" or "PLANE_VISIBLE".
    */
    PLANE_CTRL_VISIBILITY

} stm_display_plane_control_t;


typedef struct stm_asynch_ctrl_status_s {
    int ctrl_id; /**< id of the control for which the error code is reported (stm_display_plane_control_t) for a plane*/
    int error_code; /**< error code value for the control */
} stm_asynch_ctrl_status_t;


typedef struct stm_ctrl_listener_s {
    void *data; /**< listener private data */
    void (*notify) (void *data,
                    const stm_time64_t vsync_time,
                    const stm_asynch_ctrl_status_t *status,
                    int status_cnt);
} stm_ctrl_listener_t;


/*! \struct stm_display_plane_control_range_s
 *  \brief This type defines the control range given for a plane.
 */
typedef struct stm_display_plane_control_range_s
{
    int32_t  min_val;     /* Minimum meaningful value that a control is providing.  */
    int32_t  max_val;     /* Maximum meaningful value that a control is providing.  */
    int32_t  default_val; /* Default value at init time that a control is providing.*/
    int32_t  step;        /* The step unit that a control is providing. */
} stm_display_plane_control_range_t;


/*! \struct stm_compound_control_range_t
 *  \brief This type defines the compound control range given for a plane.
 */
typedef struct stm_compound_control_range_s
{
    stm_rect_t  min_val; /* Minimum meaningful value that a window control is providing. */
    stm_rect_t  max_val; /* Maximum meaningful value that a window control is providing. */
    stm_rect_t  default_val; /* Default value at init time that a window control is providing.*/
    stm_rect_t  step;    /* The step that would make a visible change. */
} stm_compound_control_range_t;


/*! \struct stm_compound_control_latency_s
 *  \brief This type defines the structure used to retrieve the min and max plane latency.
 */
typedef struct stm_compound_control_latency_s
{
    uint32_t  nb_input_periods;
    uint32_t  nb_output_periods;
} stm_compound_control_latency_t;


/*! \enum stm_display_plane_tuning_data_control_t
 *
 *  \brief This type defines plane tuning data types of controls.
 */
typedef enum stm_display_plane_tuning_data_control_e
{
PLANE_CTRL_VIDEO_ACC_TUNING_DATA,
PLANE_CTRL_VIDEO_ACM_TUNING_DATA,
PLANE_CTRL_VIDEO_AFM_TUNING_DATA,
PLANE_CTRL_VIDEO_CCS_TUNING_DATA,
PLANE_CTRL_VIDEO_DNR_TUNING_DATA,
PLANE_CTRL_VIDEO_MADI_TUNING_DATA,
PLANE_CTRL_VIDEO_MCTI_TUNING_DATA,
PLANE_CTRL_VIDEO_MNR_TUNING_DATA,
PLANE_CTRL_VIDEO_SCALER_TUNING_DATA,
PLANE_CTRL_VIDEO_SHARPNESS_TUNING_DATA,
PLANE_CTRL_VIDEO_TNR_TUNING_DATA,
PLANE_CTRL_FLICKER_FILTER_TUNING_DATA,
PLANE_CTRL_VIDEO_XVP_TUNING_DATA,
PLANE_CTRL_VIDEO_FMD_TUNING_DATA,
PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA,
PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA,
PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA

} stm_display_plane_tuning_data_control_t;

/*! \enum stm_plane_ctrl_processing_type_e+ *
*  \brief This type defines the possible modes for video processing.
*/
typedef enum stm_plane_ctrl_processing_type_e
{
    PLANE_PROCESSING_TYPE_UNKNOWN =0,
    PLANE_PROCESSING_TYPE_GRAPHICS,
    PLANE_PROCESSING_TYPE_PHOTO,
    PLANE_PROCESSING_TYPE_CINEMA,
    PLANE_PROCESSING_TYPE_GAME
} stm_plane_ctrl_processing_type_t;

/*! \enum stm_plane_ctrl_latency_mode_e+ *
*  \brief This type defines the possible modes for video latency.
*/
typedef enum stm_plane_ctrl_latency_mode_e
{
    PLANE_LATENCY_CONSTANT =0,
    PLANE_LATENCY_FE_ONLY,
    PLANE_LATENCY_LOW,
} stm_plane_ctrl_latency_mode_t;

/*! \struct stm_tuning_data_t
 *  \brief This type defines the data description used for CoreDisplay tuning data configuration.
 */
typedef struct stm_tuning_data_s
{
    uint32_t            size;     /* Size of the complete tuning data structure including payload.*/
    stm_plane_feature_t feature;  /* This is a unique identification of the type of the data configuration table based. */
    uint32_t            revision; /* Version of the tuning data. */
} stm_tuning_data_t;

#define GET_PAYLOAD_POINTER(tuning_data) ((uint8_t *)tuning_data + sizeof(stm_tuning_data_t))
#define GET_PAYLOAD_SIZE(tuning_data) (tuning_data->size-sizeof(stm_tuning_data_t))

typedef enum stm_plane_mode_e
{
    MANUAL_MODE,
    AUTO_MODE
} stm_plane_mode_t;

typedef enum stm_plane_ff_state_e{
    PLANE_FLICKER_FILTER_DISABLE,
    PLANE_FLICKER_FILTER_ENABLE
} stm_plane_ff_state_t;

typedef enum stm_plane_ff_mode_e{
    PLANE_FLICKER_FILTER_SIMPLE,
    PLANE_FLICKER_FILTER_ADAPTIVE
} stm_plane_ff_mode_t;


typedef enum stm_plane_control_state_e
{
    CONTROL_OFF,
    CONTROL_ON,
    CONTROL_SMOOTH,
    CONTROL_MEDIUM,
    CONTROL_SHARP,
    CONTROL_ONLY_LEFT_ON,
    CONTROL_ONLY_RIGHT_ON,
} stm_plane_control_state_t;


/*! \enum stm_plane_telecine_t
 *
 *  \brief This type defines the telecine mode.
 */
typedef enum stm_plane_telecine_e
{
    TELECINE_UNKNOWN,  /*!< Don't know */
    TELECINE_2_2,      /*!< 2:2 format */
    TELECINE_3_2       /*!< 3:2 format */
} stm_plane_telecine_t;


/*! \enum stm_plane_scaler_mode_t
 *
 *  \brief This type defines the scaler mode.
 */
typedef enum stm_plane_scaler_mode_e
{
    SCALER_NORMAL_MODE,    /*!< Scaler in normal mode */
    SCALER_PANORAMIC_MODE  /*!< Scaler in panoramic mode */
} stm_plane_scaler_mode_t;


/*! \enum stm_plane_luma_compression_mode_t
 *
 *  \brief This type defines the luma compression modes.
 */
typedef enum stm_plane_luma_compression_mode_e
{
    LUMA_NO_COMPRESSION,                /*!< No compression */
    LUMA_REMPEG_BASE_COMPRESSION,       /*!< Luma base compression */
    LUMA_REMPEG_AGGRESSIVE_COMPRESSION  /*!< Luma aggressive compression */
} stm_plane_luma_compression_mode_t;


/*! \enum stm_plane_rgb_compression_mode_t
 *
 *  \brief This type defines the RGB compression modes.
 */
typedef enum stm_plane_rgb_compression_mode_e
{
    RGB_NO_COMPRESSION,                /*!< No compression */
    RGB_REMPEG_COMPRESSION,            /*!< RGB base compression */
    RGB_REMPEG_AGGRESSIVE_COMPRESSION, /*!< RGB aggressive compression */
    RGB_565_COMPRESSION                /*!< RGB 565 compression */
} stm_plane_rgb_compression_mode_t;


/*! \enum stm_plane_chroma_compression_mode_t
 *
 *  \brief This type defines the chroma compression modes.
 */
typedef enum stm_plane_chroma_compression_mode_e
{
    CHROMA_NO_COMPRESSION,   /*!< No compression */
    CHROMA_420_COMPRESSION   /*!< Chroma 4:2:0 compression the entire path */
} stm_plane_chroma_compression_mode_t;


/*! \enum stm_plane_source_compression_mode_t
 *
 *  \brief This type defines the source compression modes.
 */
typedef enum stm_plane_source_compression_mode_e
{
    SOURCE_NO_COMPRESSION,   /*!< No compression */
    SOURCE_8BIT_COMPRESSION  /*!< Obtain 8 bit data through dropping LSB bits */
} stm_plane_source_compression_mode_t;


/*! \enum stm_plane_dnr_compression_mode_t
 *
 *  \brief This type defines the DNR compression modes.
 */
typedef enum stm_plane_dnr_compression_mode_e
{
    DNR_NO_COMPRESSION,  /*!< DNR compression is disabled */
    DNR_COMPRESSION      /*!< DNR compression is enabled */
} stm_plane_dnr_compression_mode_t;


/*! \enum stm_plane_tnr_compression_mode_t
 *
 *  \brief This type defines the TNR compression modes.
 */
typedef enum stm_plane_tnr_compression_mode_e
{
    TNR_NO_COMPRESSION,  /*!< TNR luma mode is disabled */
    TNR_LUMA_COMPRESSION /*!< TNR luma mode is enabled */
} stm_plane_tnr_compression_mode_t;


/*! \enum stm_plane_aspect_ratio_conversion_mode_t
 *
 *  \brief This type defines the possible aspect ratio conversions.
 */
typedef enum stm_plane_aspect_ratio_conversion_mode_e
{
        ASPECT_RATIO_CONV_LETTER_BOX,   /*!< Default: some black bars will be
                                             displayed */
        ASPECT_RATIO_CONV_PAN_AND_SCAN, /*!< Part of the source picture will be
                                             cropped */
        ASPECT_RATIO_CONV_COMBINED,     /*!< Intermediate solution between the
                                             Letter-Box and Pan&Scan solutions:
                                             Part of the source picture is
                                             cropped and some little black bars
                                             are added. */
        ASPECT_RATIO_CONV_IGNORE        /*!< The source picture is stretched to
                                             fill the full output window. The
                                             aspect ratio is NOT preserved so
                                             this mode is mainly for testing
                                             purpose. */
} stm_plane_aspect_ratio_conversion_mode_t;


/*! \enum stm_plane_ctrl_hide_mode_policy_t
 *
 *  \brief This type defines the possible modes for video plane when hidden.
 */
typedef enum stm_plane_ctrl_hide_mode_policy_e {
  PLANE_HIDE_BY_MASKING,   /*!< The mixer is still pulling the data from the plane (so plane processing are done) but doesn’t blend them. */
  PLANE_HIDE_BY_DISABLING  /*!< The plane is disabled so cannot be blent, no data pulled from the plane. */
} stm_plane_ctrl_hide_mode_policy_t;


/*! \enum stm_display_control_mode_t
 *
 *  \brief This type defines plane control modes.
 */
typedef enum stm_display_control_mode_e
{
    /* A plane control mode that configures each control to be operated right away. */
    CONTROL_IMMEDIATE_MODE,
    /* A plane control mode that configures a set of controls to be processable
     together in an atomic manner through a synchronized commit. */
    CONTROL_SYNC_MODE
} stm_display_control_mode_t;

/*! \enum stm_display_visibility_t
 *
 *  \brief This type defines plane visibility.
 */
typedef enum stm_display_visibility_e
{
    PLANE_NOT_VISIBLE, /*!< The plane is not visible. */
    PLANE_VISIBLE      /*!< The plane is visible. */
} stm_display_visibility_t;


#define STM_DISPLAY_NEW_LATENCY_EVT (1 << 0)

/*****************************************************************************
 * C interface to display planes
 *
 */

/*!
 * Get the name of a plane.
 *
 * \param p     Plane to query.
 * \param name  A pointer to the output's name.
 *
 * /pre Have the plane handle retrieved and available.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL The plane handle was invalid
 * \returns -EFAULT The name output parameter pointer is an invalid address
 */
extern int stm_display_plane_get_name(stm_display_plane_h p, const char **name);

/* Capabilities */
/*!
 * Populates the plane capabilities structure pointed to by "c". The
 * contents of the structure may vary when the plane is connected
 * to a running output, depending on the display mode.
 *
 * \param p    Plane to query.
 * \param caps Pointer to a capabilities structure to populate.
 *
 * \returns -1 if the device lock cannot be obtained, otherwise 0.
 *
 */
extern int stm_display_plane_get_capabilities(stm_display_plane_h p, stm_plane_capabilities_t *caps);

/*!
 * Gets a pointer to a list of supported image formats and places it in "f".
 * The list itself is owned by the plane and must not be modified.
 *
 * \param p       Plane to query.
 * \param formats Pointer to the list pointer variable to fill in.
 *
 * \returns the number of formats in the returned list on success or -1 if
 *          the device lock cannot be obtained.
 *
 */
extern int stm_display_plane_get_image_formats(stm_display_plane_h p, const stm_pixel_format_t **formats);

/* Plane controls */
/*!
 * This function sets the specified control to the given value.
 * The value is commited right away if it is set in direct mode; if it is in
 * synced mode, then it will be cached until the set of operations is prepared
 * prior to being commited to the lower level.
 *
 * \param p       Plane to set the control on.
 * \param ctrl    Control identifier to change
 * \param new_val The new control value
 *
 * \pre The plane object is already used by the application and its
 *      capabilities and contextual information is checked to establish if the
 *      specific control is still meaningful. More details for each of the
 *      control cases are found in the controls usage chapter.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -ENOTSUP The control is not supported.
 * \returns -ERANGE  The value set is invalid.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_set_control(stm_display_plane_h p, stm_display_plane_control_t ctrl, uint32_t new_val);

/*!
 * This function gets the value of the specified control.
 * The value depends on the control mode; if it is direct mode, then it will be
 * closer to what is at the lower level; if it is synced mode, then it will
 * retrieve the values that were previously cached.
 *
 * \param p           Plane to get the control from.
 * \param ctrl        Control identifier to get.
 * \param current_val Pointer to the control value variable to be filled in.
 *
 * \pre The plane object is already used by the application. More details for
 *      each of the control cases are found in the controls usage chapter.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EFAULT  The current_val parameter pointer is an invalid address.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_get_control(stm_display_plane_h p, stm_display_plane_control_t ctrl, uint32_t *current_val);

/*!
 * Get the current plane status flags, an Or'd value of STM_STATUS_*
 *
 * \note This call cannot fail, does not take the device lock and may be called
 *       from interrupt context.
 *
 * \param p Plane to query
 * \param s Pointer to the status variable to fill in
 *
 * \returns 0        Success
 * \returns -EFAULT  The status parameter pointer is an invalid address.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */
extern int stm_display_plane_get_status(stm_display_plane_h p, uint32_t *s);

/* Connections */
/*!
 * Get the numeric ID of a plane's parent device.
 *
 * \note For a given plane, there is a single device to which it is associated,
 * and for a given device there might be more than one plane.
 *
 * \param p  Plane to query.
 * \param id Pointer to the variable to fill in.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EFAULT The id parameter pointer is an invalid address
 * \returns -EINVAL Plane reference passed as parameter was not
 *                  provided previously by the CoreDisplay.
 */
extern int stm_display_plane_get_device_id(stm_display_plane_h p, uint32_t *id);

/*!
 * Connect the plane to the given output.
 *
 * \note If successful, buffers queued on the plane will be processed and
 * displayed on display being generated by the output.
 *
 * \pre The plane and output are already known, their handle is ready to be
 * used further, and their capabilities are ready to be used.

 * \param p Plane to connect
 * \param o Output to connect to
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL Output or Plane reference passed as parameter was not
 *                  provided previously by the CoreDisplay.
 * \returns -EBUSY  The connection failed to be established.
 *
 */
extern int stm_display_plane_connect_to_output(stm_display_plane_h p, stm_display_output_h o);

/*!
 * Disconnect the plane from the output.
 *
 * The plane will no longer contribute to the output's display and all hardware
 * processing on the plane will be stopped.
 *
 * \note If the plane is not currently connected to the given output the
 *       call has no effect, this is not considered an error.
 *
 * \param p Plane to disconnect
 * \param o Output to disconnect from
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL Output or Plane reference passed as parameter was not
 *                  provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_disconnect_from_output(stm_display_plane_h p, stm_display_output_h o);

/*!
 * This function gets the numeric IDs of connected output to this plane; this list
 * might not be sorted.
 * This function can be used to query the actual number of connected planes.
 * This is done by setting a NULL pointer as id. In that case, max_ids can be
 * null also.
 *
 * \param p      Plane to query.
 * \param number Number of expected number of connected output IDs list.
 * \param id     Pointer to the connected output IDs list.
 *
 * \returns 0 or any
 *          number>0 Number of outputs connected.
 * \returns 0        Success if the id is not NULL.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  Output or Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -ERANGE  Index is out of range.
 */
extern int stm_display_plane_get_connected_output_id(stm_display_plane_h p, uint32_t *id, uint32_t number );

/*!
 * Connect the plane to the given source.
 *
 * /note A plane should not be previously connected to the source that is
 *       already connected to a different source; it should go through a
 *       disconnection before connecting again.
 *
 * \param p Plane to connect
 * \param s Source to connect to
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  Plane or Source reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -ENOTSUP The connection is not supported.
 * \returns -EBUSY   The connection failed to be established.
 *
 */
extern int stm_display_plane_connect_to_source(stm_display_plane_h p, stm_display_source_h s);

/*!
 * Disconnect the plane from the source.
 *
 * \note If the plane is not currently connected to the given source the
 *       call has no effect, this is not considered an error.
 *       Prior to a source disconnection to a source, the output should be put
 *       in a stable state to avoid transitory states.
 *
 * \param p Plane to disconnect
 * \param s Source to disconnect from
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL Output or Source reference passed as parameter was not
 *                  provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_disconnect_from_source(stm_display_plane_h p, stm_display_source_h s);

/*!
 * This function gets the numeric IDs of available sources this plane is allowed
 * to connect to. The available sources might not be sorted.
 * This function can be used to query the actual number of connected planes.
 * This is done by setting a NULL pointer as id. In that case, max_ids can be
 * null also.
 *
 * \param p      Plane to query.
 * \param id     Pointer to the source IDs list.
 * \param number Number of expected number of available IDs list.
 *
 * \returns 0 or any
 *          number>0 Number of sources available.
 * \returns 0        Success if the id is not NULL.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -ERANGE  Index is out of range.
 */
extern int stm_display_plane_get_available_source_id(stm_display_plane_h p, uint32_t *id, uint32_t number);

/*!
 * Get the numeric ID of the source this plane is connected to
 *
 * \note Planes can only be connected to one source at a time.
 *
 * \param p    Plane to query.
 * \param id   Pointer to the source IDs list.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock
 * \returns -EINVAL Plane reference passed as parameter was not
 *                  provided previously by the CoreDisplay.
 * \returns -EFAULT The id parameter pointer is an invalid address
 */
extern int stm_display_plane_get_connected_source_id(stm_display_plane_h p, uint32_t *id);

/*!
 * Configure the video mixer bypass behavior when a plane is connected to a
 * specific output.
 *
 * Where supported by the mixer hardware in the specified output, a plane’s
 * data can be routed through a secondary channel while optionally being mixed
 * at the same time with the data from other planes. The given output handle
 * must be for an output object that the given plane can be connected to; thus
 * the output must support a mixing capability.
 * This behavior can be configured before the plane is actually connected to
 * the output. In this case, any previous bypass configuration will be
 * immediately overwritten by this call and there will be no data produced by
 * the bypass channel until the specified plane is connected to the output.
 * Also see OUTPUT_CTRL_VIDEO_SOURCE_SELECT, which allow outputs (that support
 * the feature) to select if they process the mixed or bypassed data channel
 * from a particular video mixer.
 *
 * \param p          Plane to set as the mixer bypass source.
 * \param o          Output whose mixer bypass configuration should be changed.
 * \param exclusive  Should the plane be exclusively routed to the bypass or
 *                   should it also be mixed as normal with other planes.
 *
 * \pre  The plane and output are already known, their handle is ready to be
 *       used further, and their capabilities are ready to be used.
 *
 * \returns 0        Success
 * \returns -EINTR   Device lock cannot be obtained or the plane has already
 *                   been locked for use by another plane handle.
 * \returns -ENOTSUP This plane is not supported as the mixer bypass source
 *                   for the given output.
 * \returns -EINVAL  Output handle passed as parameter was not previously
 *                   provided by the CoreDisplay.
 */
extern int stm_display_plane_set_mixer_bypass(stm_display_plane_h  p,
                                              stm_display_output_h o,
                                              int32_t              exclusive);


/* Mixer Visibility */
/*!
 * Set a plane's depth in the plane stack of the specified output.
 *
 * If a plane can be attached to multiple outputs then the position
 * can be different on each output. The plane does not have to be currently
 * connected to the output to change it's depth. If the activate parameter is
 * 0 (false) then the change is only tested for validity, the actual plane
 * order is not changed. Planes at depth N will appear in front of all planes
 * with a depth < N.
 *
 * Depth 0 represents the background and cannot be occupied by a plane; an
 * attempt to set a depth of zero will result in an attempt to set the plane
 * to depth 1 instead rather than return an error. The maximum depth is
 * implementation dependent, an attempt to set a depth higher than the highest
 * supported depth will result in an attempt to set the plane to the highest
 * supported depth.
 *
 * When a plane changes its depth, the other planes are shifted up or down in
 * the obvious way to make room for the plane at the new depth, depending on
 * the whether the plane being moved is currently below or above the new
 * position. If the plane is currently below the new position (it is being moved
 * up) then the plane already located at that position will be pushed
 * below the moved plane. If the plane is currently above the new position (it
 * is being moved down) then the plane already located at that position will
 * pushed above the moved plane.
 *
 * \param   p        Plane whose depth is to be changed.
 * \param   o        The output on which the depth is to be changed.
 * \param   depth    The new depth.
 * \param   activate Should the new configuration be activated now, or just tested for validity
 *
 * \returns 0        Success
 * \returns -EINVAL  Plane or Ouput reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -ENOTSUP The plane is not connected to the given output.
 *
 */
extern int stm_display_plane_set_depth(stm_display_plane_h p, stm_display_output_h o, int32_t depth, int32_t activate);

/*!
 * Get a plane's depth in the plane stack of the specified output.
 *
 * If a plane can be attached to multiple outputs then the position
 * can be different on each output.
 *
 * \param p     Plane to query.
 * \param o     Output to query.
 * \param depth Pointer to the depth variable to fill in.
 *
 * \returns 0        Success
 * \returns -EINVAL  Plane or Ouput reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EFAULT  The depth parameter pointer is an invalid address
 * \returns -ENOTSUP The plane is not connected to the given output.
 *
 */
extern int stm_display_plane_get_depth(stm_display_plane_h p, stm_display_output_h o, int32_t *depth);

/*!
 * Hide plane from display on all outputs it is connected to.
 *
 * \note All the plane processing is still active as per the current
 *       configurations.
 *
 * \param p Plane to hide
 *
 * \returns 0        Success
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EDEADLK The plane was not currently active.
 *
 */
extern int stm_display_plane_hide(stm_display_plane_h p);

/*!
 * Make plane visible on all outputs it is connected to.
 *
 * If the plane is currently active then make sure it is visible on the
 * display for all outputs it is connected to.
 *
 * \param p Plane to make visible
 *
 * \returns 0        Success
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EDEADLK The plane was not currently active.
 *
 */
extern int stm_display_plane_show(stm_display_plane_h p);

/*!
 * Pause a plane.
 *
 * \pre   The plane object is already used by application.
 *
 * \param p Plane to pause.
 *
 * \returns 0        Success
 * \returns -ENOTSUP The feature is not supportedt.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_pause(stm_display_plane_h p);

/*!
 * Resume a plane.
 *
 * \pre   The plane object is already used by application.
 *
 * \param p Plane to resume processing.
 *
 * \returns 0        Success
 * \returns -ENOTSUP The feature is not supported.
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 *
 */
extern int stm_display_plane_resume(stm_display_plane_h p);

/*!
 * This function closes the given plane handle. Calling this function with a
 * NULL handle is allowed and will not cause an error or system failure.
 *
 * If this handle has locked the underlying plane's buffer queue, then
 * this lock is released and the queue flushed.
 *
 * \note The plane handle is invalid after this call completes.
 *
 * \param p Plane handle to release
 *
 *
 */
extern void stm_display_plane_close(stm_display_plane_h p);

/*!
 *  This function is a query to retrieve which is the features list for the
 *  specified plane. This list depends on the hardware implementation version
 *  and present product fuses. This function can be used to query the actual
 *  number existent plane features. This is done by setting a NULL pointer
 *  as list. In that case, number can be null also.
 *
 * \param P       Plane to query.
 * \param list    Pointer to the plane feature list.
 * \param number  Max number of features for the specified plane.
 *
 * /pre    The plane handler is obtained.
 *
 * \returns 0       Success
 * \returns -EINTR  The call was interrupted while obtaining the device lock.
 * \returns -EINVAL The device handle was invalid.
 * \returns -ERANGE Number is out of range.
 *
 */
extern int stm_display_plane_get_list_of_features(stm_display_plane_h p,
                                                  stm_plane_feature_t* list,
                                                  uint32_t number );

/*!
 *  This function queries for a given plane feature if is applicable for the
 *  given context, the driver having the domain knowledge of the features
 *  dependencies, which could be given by the platform characteristics.
 *
 * \param p          Plane to query.
 * \param feature    Feature to query.
 * \param applicable Pointer to a boolean variable set to true if applicable
 *                     and false if is not applicable.
 *
 * /pre The plane handler is obtained, the plane is connected, and the feature
 *      list is obtained.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -EINVAL  The device handle was invalid.
 * \returns -ENOTSUP The queried function is not supported.
 */
extern int stm_display_plane_is_feature_applicable(stm_display_plane_h p,
                                                   stm_plane_feature_t feature,
                                                   bool *applicable );

/*!
 * This function queries the control range for a given plane. The contents of the
 * structure for some of the features may vary when the plane is connected to
 * a running output, depending on the display mode.
 *
 * \param p                Plane to query.
 * \param control_selector Control to query.
 * \param range            Pointer to a range structure to populate.
 *
 * /pre The plane handler is obtained, the plane is connected, and the feature
 *      list is obtained.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EFAULT  The range parameter pointer is an invalid address.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_get_control_range(stm_display_plane_h p,
                                               stm_display_plane_control_t control_selector,
                                               stm_display_plane_control_range_t *range);

/*!
 * This function gets the value of the specified compound control.
 * The value depends on the control mode; if it is direct mode, then it will be
 * closer to what is at the lower level; if it is synced mode, then it will
 * retrieve the values that were previously cached.
 *
 * \param p            Plane to query.
 * \param ctrl         Control identifier to get.
 * \param current_val  Pointer to the window value to be filled in.
 *
 * /pre The plane object is already used by application.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -EFAULT  The current_val parameter pointer is an invalid address
 * \returns -ENOTSUP The control is not supported.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_get_compound_control (stm_display_plane_h p,
                                                   stm_display_plane_control_t ctrl,
                                                   void *current_val);

/*!
 * This function sets the specified control to the given compound value.
 * The compound value is commited right away if it is set in direct mode;
 * if it is in synced mode, then it will be cached until the set of operations
 * is prepared prior to being commited to the lower level.
 *
 * \param p        Plane to query.
 * \param ctrl     Control identifier to change.
 * \param new_val  The new control value.
 *
 * /pre The plane object is already used by the application and its capabilities
 *      and contextual information is checked to establish if the specifc control
 *      is still meaningful.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -RANGE   The value set is invalid.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_set_compound_control (stm_display_plane_h p,
                                                   stm_display_plane_control_t  ctrl,
                                                   void *new_val);

/*!
 * This function queries the compound control range for a given plane.
 * The contents of the structure for some of the features may vary when the
 * plane is connected to a running output, depending on the display mode.
 *
 * \param p                 Plane to query.
 * \param control_selector  Capability to query.
 * \param range             Pointer to a range structure to populate.
 *
 * /pre The plane handler is obtained, the plane is connected, and the feature
 *      list is obtained.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EFAULT  The range parameter pointer is an invalid address
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_get_compound_control_range (stm_display_plane_h p,
                                                         stm_display_plane_control_t control_selector,
                                                         stm_compound_control_range_t *range);

/*!
 * Get a unique implementation dependent non-zero numeric identifier for the
 * video timing generator pacing this plane.
 *
 * \param p          Plane to query.
 * \param timingID   The returned timing identifier
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock
 * \returns -EINVAL  The plane handle was invalid
 * \returns -EFAULT  The timingID pointer is an invalid address
 * \returns -ENOTSUP The plane is not connected to a video timing generator (happens when the plane is not connected to an output)
 *
 */
extern int stm_display_plane_get_timing_identifier(stm_display_plane_h p, uint32_t *timingID);


/*!
 * This function gets the current supported revision of the specified tuning data
 * control.
 *
 * \param p          Plane to query.
 * \param ctrl       Control identifier to get.
 * \param Revision   Pointer to the tuning data control supported revision
 *                   to be filled in.
 *
 * /pre The plane object is already used by the application.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EFAULT  The revision parameter pointer is an invalid address
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_get_tuning_data_revision (stm_display_plane_h p,
                                                       stm_display_plane_tuning_data_control_t ctrl,
                                                       uint32_t *revision );

/*!
 * This function gets the value of the specified tuning data control.
 * The value depends on the control mode; if it is direct mode, then it will be
 * closer to what is at the lower level; if it is synced mode, then it will
 * retrieve the values that were previously cached.
 *
 * \param p           Plane to query.
 * \param ctrl        Control identifier to get.
 * \param current_val Pointer to the control value variable to be filled in.
 *
 * /pre The plane object is already used by the application.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EFAULT  The current_val parameter pointer is an invalid address
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_get_tuning_data_control (stm_display_plane_h p,
                                                      stm_display_plane_tuning_data_control_t ctrl,
                                                      stm_tuning_data_t *current_val);

/*!
 * This function sets the specified tuning data control to the given tuning data.
 * The value is commited right away if it is set in direct mode; if it is in
 * synced mode, then it will be cached until the set of operations is
 * prepared prior to being commited to the lower level.
 *
 * \param p          Plane set the control on.
 * \param ctrl       Control identifier to change.
 * \param new_val    The new control value.
 *
 * /pre The plane object is already used by the application and its capabilities
 *      and contextual information is checked to establish if the specific
 *      control is still meaningful.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -ERANGE  The value set is invalid.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */

extern int stm_display_plane_set_tuning_data_control (stm_display_plane_h p,
                                                      stm_display_plane_tuning_data_control_t ctrl,
                                                      stm_tuning_data_t * new_val);

/*!
 * This function gets the present control mode.
 *
 * \param p          Plane to query.
 * \param m          Control mode value.
 *
 * /pre First make use of the stm_display_device_open_plane.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EFAULT  The mode parameter pointer is an invalid address
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */
extern int stm_display_plane_get_control_mode(stm_display_plane_h p, stm_display_control_mode_t *mode);

/*!
 * This function sets plane control mode, which will condition the mode of
 * operations of the control, compound control, and tuning data tuning.
 *
 * \param p          Plane to set the control mode.
 * \param m          Control mode value.
 *
 * /pre First make use of the stm_display_device_open_plane.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -ERANGE  The value set is invalid.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */
extern int stm_display_plane_set_control_mode(stm_display_plane_h p, stm_display_control_mode_t mode);

/*!
 * This function is a synchronized set control to the given value.
 * This operation could be used only if the stm_display_plane_set_control_mode()
 * sets the corresponding sync control mode.
 *
 * \param p          Plane to apply the controls.
 *
 * /pre First make use of the stm_display_device_open_plane and make use of
 *      the sync control mode configuring stm_display_plane_set_control_mode.
 *
 * \returns 0        Success
 * \returns -EINTR   The call was interrupted while obtaining the device lock.
 * \returns -ENOTSUP The control is not supported.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */
extern int stm_display_plane_apply_sync_controls(stm_display_plane_h p);

/*!
 * This function applies the mode setup signal to the end of the
 * reconfiguration of the entire CoreDisplay after a transition from unstable
 * to stable source, or from a stop start output.
 *
 * \param p  Plane set the control mode.
 *
 * /pre First make use of the stm_display_source_set_status to indicate a
 *      transition from unstable to stable source, or from stopped to start
 *      output. Also make use of the input and output configuration parameters
 *      prior to finalizing this complete reconfiguration with
 *      stm_display_plane_apply_modesetup.
 *
 * \returns 0        Success
 * \returns -ENOTSUP The control is not supported.
 * \returns -EINVAL  Plane reference passed as parameter was not
 *                   provided previously by the CoreDisplay.
 */
extern int stm_display_plane_apply_modesetup(stm_display_plane_h p);

/*!
 * Set a control listener. The listener is notified upon vsync event on the
 * status of the controls that were effectively applied upon this vsync.
 *
 * \param p  Plane handle the listener is registering.
 * \param listener  a pointer on the structure describing the listener.
 * \param listener_id  the id associated to this listener by the plane
 *
 * \returns 0        Success
 * \returns -ENOTSUP
 * \returns -EINVAL
 * \returns -ERANGE
 * \returns -EAGAIN
 * \returns -EINTR
 */
extern int stm_display_plane_set_asynch_ctrl_listener(stm_display_plane_h p, const stm_ctrl_listener_t *listener, int *listener_id);

/*!
 * Remove listener_id from the listeners on the plane vsync events.
 *
 * \param listener_id  the id associated to this listener by the plane
 *
 * \returns 0        Success
 * \returns -ENOTSUP
 * \returns -EINVAL
 * \returns -ERANGE
 * \returns -EAGAIN
 * \returns -EINTR
 */
extern int stm_display_plane_unset_asynch_ctrl_listener(stm_display_plane_h plane, int listener_id);



#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_PLANE_H */
