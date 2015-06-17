/*******************************************************************************
 *
 * FILE NAME          : MxL101SF_PhyCtrlApi.h
 * 
 * AUTHOR             : Brenndon Lee
 *                      Mahendra Kondur - code seperation
 *
 * DATE CREATED       : 5/18/2009
 *                      10/22/2009 
 *
 * DESCRIPTION        : This file is header file for MxL101SF_PhyCtrlApi.cpp
 *
 *******************************************************************************
 *                Copyright (c) 2006, MaxLinear, Inc.
 ******************************************************************************/

#ifndef __MXL101SF_PHY_CTRL_API_H__
#define __MXL101SF_PHY_CTRL_API_H__

/******************************************************************************
    Include Header Files
    (No absolute paths - paths handled by make file)
******************************************************************************/
#include "MaxLinearDataTypes.h"

/******************************************************************************
    Macros
******************************************************************************/

//#define __FLOATING_POINT_STATISTICS__  

#ifdef __FLOATING_POINT_STATISTICS__
  #define CALCULATE_SNR(data)        (REAL32)((10 * (REAL32)data / 64) - 2.5)
  #define CALCULATE_BER(avg_errors, count)  (REAL32)(avg_errors * 4)/(count*64*188*8)
#else
  #define CALCULATE_SNR(data)       (((1563 * data) - 25000) / 10000)
  #define CALCULATE_BER(avg_errors, count)  (UINT32)(avg_errors * 4)/(count*64*188*8)
#endif

#define MXL_TUNER_MODE         0
#define MXL_SOC_MODE           1
#define MXL_DEV_MODE_MASK      0x01

/******************************************************************************
    User-Defined Types (Typedefs)
******************************************************************************/

/* Command Types  */
typedef enum
{
  // MxL Device configure command
  MXL_DEV_SOFT_RESET_CFG = 0,
  MXL_DEV_XTAL_SETTINGS_CFG,
  MXL_DEV_ID_VERSION_REQ,
  MXL_DEV_OPERATIONAL_MODE_CFG,
  MXL_DEV_GPO_PINS_CFG,
  MXL_DEV_CABLE_CFG,
  
  // MxL101SF Config specific commands
  MXL_DEV_101SF_OVERWRITE_DEFAULTS_CFG,
  MXL_DEV_101SF_POWER_MODE_CFG,
  MXL_DEV_MPEG_OUT_CFG,
  
  // Reset/Clear Demod status
  MXL_DEMOD_RESET_IRQ_CFG,
  MXL_DEMOD_RESET_PEC_CFG,
  MXL_DEMOD_TS_PRIORITY_CFG,
  
  // Demod Status and Info command
  MXL_DEMOD_SNR_REQ,
  MXL_DEMOD_BER_REQ,
  MXL_DEMOD_TPS_CODE_RATE_REQ,
  MXL_DEMOD_TPS_HIERARCHY_REQ,
  MXL_DEMOD_TPS_CONSTELLATION_REQ,
  MXL_DEMOD_TPS_FFT_MODE_REQ,
  MXL_DEMOD_TPS_HIERARCHICAL_ALPHA_REQ,
  MXL_DEMOD_TPS_GUARD_INTERVAL_REQ,
  MXL_DEMOD_TPS_CELL_ID_REQ,
  MXL_DEMOD_TPS_LOCK_REQ,
  MXL_DEMOD_PACKET_ERROR_COUNT_REQ,
  MXL_DEMOD_SYNC_LOCK_REQ,
  MXL_DEMOD_RS_LOCK_REQ,
  MXL_DEMOD_CP_LOCK_REQ,
  
  // Tuner Config commands
  MXL_TUNER_TOP_MASTER_CFG,
  MXL_TUNER_CHAN_TUNE_CFG,
  MXL_TUNER_IF_OUTPUT_FREQ_CFG, 

  // Tuner Status and Info command
  MXL_TUNER_LOCK_STATUS_REQ,
  MXL_TUNER_SIGNAL_STRENGTH_REQ,
  
  MAX_NUM_CMD
} MXL_CMD_TYPE_E;

// MXL_DEV_ID_VERSION_REQ
typedef struct
{
  UINT8 DevId;                    /* OUT, Device Id of MxL101SF Device */
  UINT8 DevVer;                   /* OUT, Device Version of MxL101SF Device */
} MXL_DEV_INFO_T, *PMXL_DEV_INFO_T;

// MXL_DEV_POWER_MODE_CFG 
typedef enum
{
  STANDBY_ON = 0,
  SLEEP_ON,
  STANDBY_OFF,
  SLEEP_OFF,
} MXL_PWR_MODE_E; 

typedef struct
{
  MXL_PWR_MODE_E PowerMode;       /* IN, Power mode for MxL101SF Device */
} MXL_PWR_MODE_CFG_T, *PMXL_PWR_MODE_CFG_T;

// MXL_TUNER_TOP_MASTER_CFG
typedef struct
{
  MXL_BOOL  TopMasterEnable;      /* IN, Enable or Disable MxL101SF Tuner */
}MXL_TOP_MASTER_CFG_T, *PMXL_TOP_MASTER_CFG_T;

// MXL_DEV_OPERATIONAL_MODE_CFG
typedef struct
{
  UINT8 DeviceMode;             /* IN, Operational mode of MxL101SF */
} MXL_DEV_MODE_CFG_T, *PMXL_DEV_MODE_CFG_T;

#if __FLOATING_POINT_STATISTICS__ 

// MXL_DEMOD_SNR_REQ
typedef struct
{
  REAL32 SNR;                   /* OUT, SNR data from MXL101SF */
} MXL_DEMOD_SNR_INFO_T, *PMXL_DEMOD_SNR_INFO_T;

// MXL_DEMOD_BER_REQ
typedef struct
{
  REAL32 BER;                 /* OUT, BER data from MXL101SF */
} MXL_DEMOD_BER_INFO_T, *PMXL_DEMOD_BER_INFO_T;

#else

// MXL_DEMOD_SNR_REQ
typedef struct
{
  UINT32 SNR;                   /* OUT, SNR data from MXL101SF */
} MXL_DEMOD_SNR_INFO_T, *PMXL_DEMOD_SNR_INFO_T;

// MXL_DEMOD_BER_REQ
typedef struct
{
  UINT32 BER;                 /* OUT, BER data from MXL101SF */
} MXL_DEMOD_BER_INFO_T, *PMXL_DEMOD_BER_INFO_T;

#endif

// MXL_DEMOD_RESET_PEC
typedef struct
{
  UINT32 PEC;               /* OUT, PEC data from MXL101SF */
}MXL_DEMOD_PEC_INFO_T, *PMXL_DEMOD_PEC_INFO_T;

// MXL_TUNER_CHAN_TUNE_CFG
typedef struct
{
  REAL32 Frequency;                       /* IN, Frequency in MHz */
  UINT8 Bandwidth;                        /* IN, Channel Bandwidth in MHz */
  MXL_BOOL TpsCellIdRbCtrl;               /* IN, Enable TPS Cell ID Read Back feature */
} MXL_RF_TUNE_CFG_T, *PMXL_RF_TUNE_CFG_T;

// Tune RF Lock Status MXL_TUNER_LOCK_STATUS_REQ
typedef struct
{
  MXL_BOOL RfSynthLock;               /* OUT, RF SYNT Lock status of MXL101SF Tuner */
  MXL_BOOL RefSynthLock;              /* OUT, REF SYNT Lock status of MXL101SF Tuner */
} MXL_TUNER_LOCK_STATUS_T, *PMXL_TUNER_LOCK_STATUS_T;

// MXL_DEMOD_TPS_CONSTELLATION_REQ
// MXL_DEMOD_TPS_CODE_RATE_REQ
// MXL_DEMOD_TPS_FFT_MODE_REQ
// MXL_DEMOD_TPS_GUARD_INTERVAL_REQ
// MXL_DEMOD_TPS_HIERARCHY_REQ
typedef struct 
{
  UINT8 TpsInfo;                      /* OUT, TPS data for respective TPS command */
}MXL_DEMOD_TPS_INFO_T, *PMXL_DEMOD_TPS_INFO_T;

// MXL_DEMOD_TPS_LOCK_REQ
// MXL_DEMOD_RS_LOCK_REQ
// MXL_DEMOD_CP_LOCK_REQ
typedef struct
{
  MXL_BOOL Status;                  /* OUT, Lock status of MxL101SF */ 
} MXL_DEMOD_LOCK_STATUS_T, *PMXL_DEMOD_LOCK_STATUS_T;

// MXL_DEMOD_TS_PRIORITY_CFG
typedef enum
{
  HP_STREAM = 0,
  LP_STREAM,
}MXL_HPORLP_E;

typedef struct
{
  MXL_HPORLP_E StreamPriority;         /* IN,  Value to select transport stream priority*/ 
} MXL_DEMOD_TS_PRIORITY_CFG_T, *PMXL_DEMOD_TS_PRIORITY_CFG_T;

// MXL_TUNER_IF_OUTPUT_FREQ_CFG
typedef enum
{
  IF_OTHER_12MHZ = 0,
  IF_4_0MHZ,
  IF_4_5MHZ,
  IF_4_57MHZ,
  IF_5_0MHZ,
  IF_5_38MHZ,
  IF_6_0MHZ,
  IF_6_28MHZ,
  IF_7_2MHZ,
  IF_35_250MHZ,
  IF_36_0MHZ,
  IF_36_15MHZ,
  IF_44_0MHZ,
  IF_OTHER_35MHZ_45MHZ = 0x0F,
} MXL_IF_FREQ_E; 

typedef struct
{
  MXL_IF_FREQ_E   IF_Index;               /* IN, Index for predefined IF frequency */
  UINT8           IF_Polarity;            /* IN, IF Spectrum - Normal or Inverted */
  REAL32          IF_Freq;                /* IN, IF Frequency in MHz for non-predefined frequencies */
} MXL_TUNER_IF_FREQ_T, *PMXL_TUNER_IF_FREQ_T;

// MXL_DEV_XTAL_SETTINGS_CFG
typedef enum
{
  XTAL_24MHz = 4,
  XTAL_27MHz = 7,
  XTAL_28_8MHz = 8,
  XTAL_48MHz = 12,
  XTAL_NA = 13
} MXL_XTAL_FREQ_E;

typedef enum
{
  I123_5uA = 0,
  I238_8uA,
  I351_4uA,
  I462_4uA,
  I572_0uA,
  I680_4uA,
  I787_8uA,
  I894_2uA,
  XTAL_BIAS_NA
} MXL_XTAL_BIAS_E;

typedef enum
{
  CLK_OUT_0dB = 0,
  CLK_OUT_1_25dB,
  CLK_OUT_2_50dB,
  CLK_OUT_3_75dB,
  CLK_OUT_5_00dB,
  CLK_OUT_6_25dB,
  CLK_OUT_7_50dB,
  CLK_OUT_8_75dB,
  CLK_OUT_10_0dB,
  CLK_OUT_11_25dB,
  CLK_OUT_12_50dB,
  CLK_OUT_13_75dB,
  CLK_OUT_15_00dB,
  CLK_OUT_16_25dB,
  CLK_OUT_17_50dB,
  CLK_OUT_18_75dB,
  CLK_OUT_NA
} MXL_XTAL_CLK_OUT_GAIN_E;

typedef struct
{
  MXL_XTAL_FREQ_E           XtalFreq;             /* IN, XTAL frequency */
  MXL_XTAL_BIAS_E           XtalBiasCurrent;      /* IN, XTAL Bias current */
  UINT8                     XtalCap;              /* IN, XTAL Capacitance value */
  MXL_BOOL                  XtalClkOutEnable;     /* IN, XTAL Clock out control */
  MXL_XTAL_CLK_OUT_GAIN_E   XtalClkOutGain;       /* IN, XTAL Clock out gain value */
  MXL_BOOL                  LoopThruEnable;       /* IN, Frequency loop thru */
} MXL_XTAL_CFG_T, *PMXL_XTAL_CFG_T;

// MXL_DEMOD_MPEG_OUT_CFG
typedef enum
{
  MPEG_CLOCK_36_571429MHz = 0,
  MPEG_CLOCK_2_285714MHz,
  MPEG_CLOCK_4_571429MHz,
  MPEG_CLOCK_6_857143MHz,
  MPEG_CLOCK_9_142857MHz,
  MPEG_CLOCK_13_714286MHz,
  MPEG_CLOCK_18_285714MHz,
  MPEG_CLOCK_27_428571MHz
} MPEG_CLOCK_FREQ_E;

typedef enum
{
  MPEG_ACTIVE_LOW = 0,
  MPEG_ACTIVE_HIGH,

  MPEG_CLK_POSITIVE  = 0,
  MPEG_CLK_NEGATIVE,

  MPEG_CLK_IN_PHASE = 0,
  MPEG_CLK_INVERTED,

  MPEG_CLK_EXTERNAL = 0, 
  MPEG_CLK_INTERNAL
} MPEG_CLK_FMT_E;

typedef enum
{
  MPEG_SERIAL_LSB_1ST = 0,
  MPEG_SERIAL_MSB_1ST,
  
  MPEG_DATA_SERIAL  = 0,
  MPEG_DATA_PARALLEL,

  MPEG_SYNC_WIDTH_BIT = 0,
  MPEG_SYNC_WIDTH_BYTE
} MPEG_DATA_FMT_E;

typedef struct
{ 
  MPEG_DATA_FMT_E   SerialOrPar;             /* IN, Serial or Parallel mode */
  MPEG_DATA_FMT_E   LsbOrMsbFirst;           /* IN, Serial mode MSB or LSB first */
  MPEG_CLOCK_FREQ_E MpegClkFreq;             /* IN, MPEG Clock frequency */ 
  MPEG_CLK_FMT_E    MpegValidPol;            /* IN, MPEG Valid polarity */
  MPEG_CLK_FMT_E    MpegClkPhase;            /* IN, MPEG Clock phase */
  MPEG_CLK_FMT_E    MpegSyncPol;             /* IN, MPEG SYNC Polarity */
} MXL_MPEG_CFG_T, *PMXL_MPEG_CFG_T;

// MXL_DEV_GPO_PINS_CFG
typedef enum
{
  MXL_GPO_0 = 0,
  MXL_GPO_1,
  MXL_GPO_NA
} MXL_GPO_E;

typedef struct
{
  MXL_GPO_E           GpoPinId;           /* IN, GPIO Pin ID */
  MXL_BOOL            GpoPinCtrl;         /* IN, GPIO Pin control */
} MXL_DEV_GPO_CFG_T, *PMXL_DEV_GPO_CFG_T;

// MXL_TUNER_SIGNAL_STRENGTH_REQ
typedef struct
{
  REAL32 SignalStrength;                    /* OUT, Tuner Signal strength in dBm */
} MXL_SIGNAL_STATS_T, *PMXL_SIGNAL_STATS_T;

// MXL_DEMOD_TPS_CELL_ID_REQ
typedef struct 
{
  UINT16 TpsCellId;                      /* OUT, TPS Cell ID Info */
}MXL_DEMOD_CELL_ID_INFO_T, *PMXL_DEMOD_CELL_ID_INFO_T;

/******************************************************************************
    Global Variable Declarations
******************************************************************************/

/******************************************************************************
    Prototypes
******************************************************************************/

MXL_STATUS MxLWare_API_ConfigDevice(MXL_CMD_TYPE_E CmdType, void *ParamPtr);
MXL_STATUS MxLWare_API_GetDeviceStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr);
MXL_STATUS MxLWare_API_ConfigDemod(MXL_CMD_TYPE_E CmdType, void *ParamPtr);
MXL_STATUS MxLWare_API_GetDemodStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr);
MXL_STATUS MxLWare_API_ConfigTuner(MXL_CMD_TYPE_E CmdType, void *ParamPtr);
MXL_STATUS MxLWare_API_GetTunerStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr);

#endif /* __MXL1X1SF_PHY_CTRL_API_H__*/




