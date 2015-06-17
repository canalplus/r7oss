#ifndef __MXLDVB_H__
#define __MXLDVB_H__

/*!
 *
 * @mainpage MXLDVB index page
 * @section intro_sec Introduction
 *
 * This is the introduction.
 * etc...
 *
 */


/**
 *
 * @defgroup SignalParameters Signal parameters
 * @{
 */

/**
 *
 * @brief DVB type definition 
 *
 */
typedef enum
{
  MXLDVB_DVB_S =    0x0001,
  MXLDVB_DVB_S2 =   0x0002,
  MXLDVB_DVB_T =    0x0004,
  MXLDVB_DVB_T2 =   0x0008,
  MXLDVB_DVB_H =    0x0010,
  MXLDVB_DVB_C =    0x0020,
  MXLDVB_DVB_C2 =   0x0040
} MXLDVB_DVB_E;  


/**
 *
 * @brief  FEC definition
 *
 */
typedef enum
{
  MXLDVB_FEC_AUTO,      //!< Recognize FEC automatically 
  
  MXLDVB_FEC_1_2,
  MXLDVB_FEC_3_5,
  MXLDVB_FEC_2_3,
  MXLDVB_FEC_3_4,
  MXLDVB_FEC_4_5
  MXLDVB_FEC_5_6,
  MXLDVB_FEC_6_7,
  MXLDVB_FEC_7_8,
  MXLDVB_FEC_8_9,
  MXLDVB_FEC_9_10
} MXLDVB_FEC_E;

/**
 *
 * @brief  Modulation definition
 *
 */
typedef enum
{
  MXLDVB_MODULATION_AUTO,   //!< Recognize modulation automatically

  MXLDVB_MODULATION_QPSK,
  MXLDVB_MODULATION_8PSK,
  MXLDVB_MODULATION_16QAM,
  MXLDVB_MODULATION_32QAM,
  MXLDVB_MODULATION_64QAM,
  MXLDVB_MODULATION_128QAM,
  MXLDVB_MODULATION_256QAM,
  MXLDVB_MODULATION_1024QAM
} MXLDVB_MODULATION_E;

/**
 *
 * @brief Signal bandwidth definition
 *
 */
typedef enum
{
  MXLDVB_BANDWIDTH_AUTO,    //!< Detect signal bandwidth automatically

  MXLDVB_BANDWIDTH_6MHz,   
  MXLDVB_BANDWIDTH_7MHz,
  MXLDVB_BANDWIDTH_8MHz
} MXLDVB_BANDWIDTH_E;  

/**
 *
 * @brief FFT mode definition 
 *
 */
typedef enum
{
  MXLDVB_FFT_AUTO,

  MXLDVB_FFT_2k,
  MXLDVB_FFT_4k,
  MXLDVB_FFT_8k
} MXLDVB_FFT_E;

/**
 *
 * @brief Guard interval definition 
 *
 */
typedef enum
{
  MXLDVB_GUARD_INTERVAL_AUTO,   //!< Detect guard interval automatically

  MXLDVB_GUARD_INTERVAL_1_32,
  MXLDVB_GUARD_INTERVAL_1_16,
  MXLDVB_GUARD_INTERVAL_1_8,
  MXLDVB_GUARD_INTERVAL_1_4
} MXLDVB_GUARD_INTERVAL_E;


/**
 *
 * @brief Hierarchical reception definition 
 *
 */
typedef enum
{
  MXLDVB_HIERARCHY_AUTO,      //!< Detect hierarchical mode automatically. Lock to any signal

  MXLDVB_HIERARCHY_DISABLED,
  MXLDVB_HIERARCHY_ALPHA_1,
  MXLDVB_HIERARCHY_ALPHA_2,
  MXLDVB_HIERARCHY_ALPHA_4
} MXLDVB_HIERARCHY_E;

/**
 *
 * @brief Setting for signal priority in hierarchical reception 
 *
 */
typedef enum
{
  MXLDVB_STREAM_PRIORITY_HIGH,  
  MXLDVB_STREAM_PRIORITY_LOW
} MXLDVB_STREAM_PRIORITY_E;

/**
 *
 * @brief  Roll-off definition
 *
 */
typedef enum
{
  MXLDVB_ROLLOFF_AUTO,      //!< Recognize roll-off automatically

  MXLDVB_ROLLOFF_0_20,
  MXLDVB_ROLLOFF_0_25,
  MXLDVB_ROLLOFF_0_35  
} MXLDVB_ROLLOFF_E;

/**
 *
 * @brief  Pilots definition
 *
 */
typedef enum
{
  MXLDVB_PILOTS_AUTO,      //!< Recognize pilots automatically

  MXLDVB_PILOTS_ON,
  MXLDVB_PILOTS_OFF,
} MXLDVB_PILOTS_E;

/**
 *
 * @brief   Spectrum inversion definition
 *
 */
typedef enum
{
  MXLDVB_SPECTRUM_INVERSION_AUTO,   //!< Recognize spectrum inversion automatically

  MXLDVB_SPECTRUM_INVERSION_ON,
  MXLDVB_SPECTRUM_INVERSION_OFF
} MXLDVB_SPECTRUM_INVERSION_E;


/**
 *
 * @brief Annex definition
 *
 */
typedef enum
{
  MXLDVB_ANNEX_AUTO,

  MXLDVB_ANNEX_A,
  MXLDVB_ANNEX_B
} MXLDVB_ANNEX_E;  


/**
 * @}
 */

/** 
 * 
 * @defgroup LNB LNB related definitions
 * @{
 */

/**
 *
 * @brief   LNB polarization definition
 *
 */
typedef enum
{
  MXLDVB_LNB_POL_NONE = 0,
  MXLDVB_LNB_POL_HORIZONTAL,
  MXLDVB_LNB_POL_VERTICAL
} MXLDVB_LNB_POL_E;    

/**
 *
 * @brief   LNB band definition
 *
 */
typedef enum
{
  MXLDVB_LNB_BAND_NONE = 0,
  MXLDVB_LNB_BAND_LOW,
  MXLDVB_LNB_BAND_HIGH
} MXLDVB_LNB_BAND_E;

/**
 *
 * @brief   LNB Power state 
 *
 */
typedef enum
{
  MXLDVB_LNB_POWER_OFF,   //!< LNB power supply disabled. Use this option if you do not want to provide any voltage to RF line (not to damage laboratory equipment)
  MXLDVB_LNB_POWER_ON,    //!< LNB power supply enabled. Use MXLDVB_LNB_POL_E to set output voltage
  MXLDVB_LNB_POWER_ON_BOOST   //!< LNB power supply enabled with additional line drop compensation (i.e. add 1V). Not that some LNB controllers may not be able to provide additional voltage
} MXLDVB_LNB_POWER_E;
/**
 *
 * @}
 */
  
/** 
 * 
 * @defgroup DiSEqC DiSEqC related definitions
 * @{
 */

/**
 *
 * @brief DiSEqC tone burst definition 
 *
 */
typedef enum
{
  MXLDVB_TONE_BURST_NONE,  //!< No tone burst is required

  MXLDVB_TONE_BURST_SA,    //!< Tone burst SA (satellite A) also known as unmodulated tone burst
  MXLDVB_TONE_BURST_SB     //!< Tone burst SB (satellite B) also known as modulated tone burst
} MXLDVB_DISEQC_TONE_BURST_E;

/**
 *
 * @}
 */


#endif


