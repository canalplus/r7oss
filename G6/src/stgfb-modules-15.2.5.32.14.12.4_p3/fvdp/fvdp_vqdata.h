#ifndef __FVDP_VQDATA_H
#define __FVDP_VQDATA_H
/***********************************************************************
 *
 * File: fvdp/fvdp_vqdata.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

/* Defines ----------------------------------------------------------------- */

#define MAX_HIST_BINS 64
#define SHARPNESS_STEPS_MAX_NUM 64

#define PACKED  __attribute__((packed))

/*
The version of the vqdata is split into a Firmware and Hardware version.
HW Version: determines the revision of the specific HW block for the vqdata.
FW Version: determines changes to the vqdata with the same HW version.

bit 0-7: FW Version
bit 8-23: HW Version
bit 24-31: Reserved
*/
#define FW_VERSION_OFFSET   0
#define FW_VERSION_MASK     0x000000ff
#define HW_VERSION_OFFSET   8
#define HW_VERSION_MASK     0x00ffff00

#define VQVERSION(X, Y) (((X << HW_VERSION_OFFSET) & HW_VERSION_MASK)|((Y << FW_VERSION_OFFSET) & FW_VERSION_MASK))
// following fvdp is defined in fvdp.h (to be deleted in final release)
/* PSI features selection
typedef	enum FVDP_PSIFeature_e
{
    FVDP_SCALER,
    FVDP_SHARPNESS,
    FVDP_MADI,
    FVDP_AFM,
    FVDP_MCTI,
    FVDP_TNR,
    FVDP_CCS,
    FVDP_DNR,
    FVDP_MNR,
    FVDP_ACC,
    FVDP_ACM,
    FVDP_IP_CSC,
    FVDP_OP_CSC
} FVDP_PSIFeature_t;
*/
/* PSI features enable, disable
typedef	enum
{
    FVDP_PSI_ON,
    FVDP_PSI_OFF,
    FVDP_PSI_DEMO_MODE_ON_LEFT,
    FVDP_PSI_DEMO_MODE_ON_RIGHT
} FVDP_PSIState_t;
*/
/*
    Public data structure

typedef struct PACKED FVDP_PSITuningData_s
{
    uint32_t           Size;         // Indicates the size of the entire data configuration
    FVDP_PSIFeature_t  Signature;    // This is a unique identificator of the type of the data configuration table.
    uint32_t           Version;      // Version of the data
} FVDP_PSITuningData_t;
*/
/*
    ACC data structures
*/
typedef struct PACKED VQ_ACCSWParameters_s
{
    uint8_t  HistStretchEnable;
    uint16_t Cumulative_Black_Pixels;
    uint16_t Cumulative_White_Pixels;
    uint16_t Max_Black_Bin;
    uint16_t Min_White_Bin;
    uint8_t  ACC_Strength;
    uint32_t WF[MAX_HIST_BINS];
    uint16_t BinLim[MAX_HIST_BINS];
} VQ_ACCSWParameters_t;         // software parameters

typedef struct PACKED VQ_ACCHWParameters_s
{
    uint8_t  IVPMeasureEn;
    uint8_t  IVPScCheckEn;
    uint8_t  IVPScSource;
    uint16_t IVPScThresh;
    uint16_t HpfAmpThreshY;
    uint16_t HpfAmpThreshUV;
    uint8_t  ClampBlack;
    uint8_t  LutTempFilterEnable;
    uint8_t  YclNlsatScale;
    uint8_t  YclNlsatEn;
    uint8_t  YclNlsatTable[8];
    uint8_t  YclUseKY;
    uint8_t  CompLutLpfEn;
    uint8_t  CheckScEnable;
    uint8_t  SceneChangeResetEnable;
    uint8_t  TfFrameNum;
    uint8_t  YCLinkEnable;
    uint8_t  SceneChangeSource;
    uint8_t  LeftSlope;      //ScaleMin
    uint8_t  RightSlope;    //ScaleMax
    uint8_t  ScaleThresh;
    uint8_t  YCLinkGain;    //ScaleFactor
    uint16_t SceneChangeThreshold;
    uint8_t  XvYccEnable;
    uint8_t  YLpf[5];
    uint8_t  CLpf[5];
    uint8_t  ACCYLpfFlt;
    uint8_t  ACCCLpfFlt;
    uint8_t  UseLpfY ;
    uint8_t  StaticLUTEn ;
    uint8_t  LACCEnable;
    uint16_t LACCTextureThresh;
    uint8_t  LACCDiffShift;
    uint8_t  LACCTextureOffset;
    uint8_t  LACCTextureGain;
    uint8_t  LACCSmoothThresh;
    uint8_t  LACCSmoothness;
} VQ_ACCHWParameters_t;          // hardware parameters

typedef struct PACKED VQ_ACCStaticLut_s
{
   uint8_t bin[64];
}VQ_ACCStaticLut_t;              // input static LUT

/*
    ACM data structures
*/
typedef struct PACKED VQ_ACMRegionDefinitions_s
{
    uint16_t HueCenter;
    uint16_t HueAperture;
    uint16_t R1;
    uint16_t R2;
    uint16_t Y1;
    uint16_t Y2;
    uint16_t HueFade;
    uint16_t SatFade;
    uint16_t LumiFade;
} VQ_ACMRegionDefinitions_t;

typedef struct PACKED VQ_ACMRegionCorrectionParameters_s
{
    int16_t  HueOffs;
    int16_t  HueGain;
    int16_t  SatOffs;
    uint16_t SatGain;
    int16_t  LumiOffs;
    uint16_t LumiGain;
    int16_t  U_Vect;
    int16_t  V_Vect;
    uint16_t Alpha;
} VQ_ACMRegionCorrectionParameters_t;

typedef struct PACKED VQ_ACMParameters_s
{
    uint8_t ThetaFadeLoRGainEnable;
    uint8_t ThetaFadeLoRValue;
    uint8_t ThetaFadeLoRCutoff;
} VQ_ACMParameters_t;

#define ACM_NUM_OF_HW_REGIONS 8
/*
    CSC data structures
*/
typedef struct PACKED VQ_CSCMatrixCoef_s
{
    int32_t MultCoef11;
    int32_t MultCoef12;
    int32_t MultCoef13;
    int32_t MultCoef21;
    int32_t MultCoef22;
    int32_t MultCoef23;
    int32_t MultCoef31;
    int32_t MultCoef32;
    int32_t MultCoef33;
    int32_t InOffset1;
    int32_t InOffset2;
    int32_t InOffset3;
    int32_t OutOffset1;
    int32_t OutOffset2;
    int32_t OutOffset3;
} VQ_CSCMatrixCoef_t; // CSC matrix coefficients
/*
    Madi data structures
*/
typedef struct PACKED VQ_MADiThreshold_s
{
	uint8_t Threshold0;
	uint8_t Threshold1;
	uint8_t Threshold2;
} VQ_MADiThreshold_t;

typedef struct PACKED VQ_MADiStaticPixFix_s
{
    uint8_t  Enable;
    uint16_t FrmDiff;
    uint16_t FldDiff;
    uint8_t  Depth;
    uint16_t DisThresh;
} VQ_MADiStaticPixFix_t;
/*
    Enhancer data structures
*/
typedef struct PACKED VQ_EnhancerNonLinearControl_s
{
    uint16_t Threshold1;
    uint8_t  Gain1;
    uint16_t Threshold2;
    uint8_t  Gain2;
} VQ_EnhancerNonLinearControl_t;

typedef struct PACKED VQ_EnhancerNoiseCoringControl_s
{
    uint8_t  Threshold1;
} VQ_EnhancerNoiseCoringControl_t;

typedef struct PACKED VQ_EnhancerGain_s
{
    uint8_t  LumaGain;
    uint8_t  ChromaGain;
} VQ_EnhancerGain_t;
/*
    Sharpness data structures
*/
typedef struct PACKED VQ_SharpnessStepGroup_s
{
    uint8_t  OverShoot;
    uint8_t  LargeGain;
    uint8_t  SmallGain;
    uint8_t  FinalGain;
    uint8_t  AGC;
} VQ_SharpnessStepGroup_t;

typedef struct PACKED VQ_EnhancerParameters_s
{
    uint16_t LOSHOOT_TOL_Value;
    uint16_t LUSHOOT_TOL_Value;
    uint16_t SOSHOOT_TOL_Value;
    uint16_t SUSHOOT_TOL_Value;
    uint16_t LGAIN_Value;
    uint16_t SGAIN_Value;
    uint16_t FINALLGAIN_Value;
    uint16_t FINALSGAIN_Value;
    uint16_t FINALGAIN_Value;
    uint8_t  DELTA_Value;
    uint8_t  SLOPE_Value;
    uint16_t THRESH_Value;
    uint8_t  HIGH_SLOPE_AGC_Value;
    uint8_t  LOW_SLOPE_AGC_Value;
    uint16_t HIGH_THRESH_AGC_Value;
    uint16_t LOW_THRESH_AGC_Value;
} VQ_EnhancerParameters_t;

typedef struct PACKED VQ_Sharpness_Step_Setting_s
{
    uint8_t NonLinearThreshold1;
    uint8_t NonLinearGain1;
    uint8_t NonLinearThreshold2;
    uint8_t NonLinearGain2;
    uint8_t NoiseCoring;

    VQ_SharpnessStepGroup_t Luma;
    VQ_SharpnessStepGroup_t Chroma;
} VQ_Sharpness_Step_Setting_t;
/*
    TNR data structures
*/
typedef struct PACKED VQ_TNR_Adaptive_Parameters_s
{
    uint8_t EnableSNR;                // TNR3_BYPASS_SNR bit
    uint8_t SNRLumaSensitivity;       // TNR3_Y_SNR_K_GRAD
    uint8_t SNRLumaStrength;          // TNR3_Y_H_COEF_SEL
    uint8_t SNRChromaSensitivity;     // TNR3_UV_SNR_K_GRAD
    uint8_t SNRChromaStrength;        // TNR3_UV_H_COEF_SEL
    uint8_t YKMin;                    // TNR3_Y_K_MIN
    uint8_t UVKMin;                   // TNR3_UV_K_MIN
    uint8_t YGrad;                    // TNR3_luma_grad
    uint8_t FleshtoneGrad;            // TNR3_fleshtone_grad
    uint8_t ChromaGrad;               // TNR3_chroma_grad
} VQ_TNR_Adaptive_Parameters_t;


/*
    ACC header data structures
*/
typedef struct PACKED VQ_ACC_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    VQ_ACCSWParameters_t ACCSWParams;
    VQ_ACCStaticLut_t    ACCStatic;
    VQ_ACCHWParameters_t ACCHWParams;
} VQ_ACC_Parameters_t;
/*
    ACM header data structures
*/
typedef struct PACKED VQ_ACM_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    VQ_ACMParameters_t   ACMParams;
    uint8_t              RegionsNum; //num of active regions
    VQ_ACMRegionDefinitions_t ACMRegionDefs[ACM_NUM_OF_HW_REGIONS];
    VQ_ACMRegionCorrectionParameters_t ACMRegionCorrectionParams[ACM_NUM_OF_HW_REGIONS];
} VQ_ACM_Parameters_t;
/*
    AFM data structures
*/
typedef struct PACKED VQ_AFM_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    uint8_t Mode;        // 0-enable all, 2-disable 3:2, 3-disable 2:2, 5-force MADI, 6-force 3:2, 8-force 2:2
    uint8_t RTh;
    uint8_t STh;
    uint8_t VMTh1;
    uint8_t VMTh2;
    uint8_t VMTh3;
    uint8_t EnableHWThNoiseAdaptive;
    uint8_t ThEnter32;   // Unsigned 8 bit integer
    uint8_t ThLeave32;   // Unsigned 8 bit integer
    uint8_t ThEnter22;   // Unsigned 8 bit integer
    uint8_t ThLeave22;   // Unsigned 8 bit integer
    uint8_t Enable32Persis;
    uint8_t AFM_32Persis;
    uint8_t Enable22Persis;
    uint8_t AFM_22Persis;
    uint8_t LowSignalThreshold;      // 0-2.55, step 0.01
    uint8_t SceneChangeThreshold;    // 0-255
    uint8_t Rej32ShortSeq;
    uint8_t Rej32LowMotion;
    uint8_t WinLeft;         // 0…100
    uint8_t WinRight;        // 0…100
    uint8_t WinTop;          // 0…100
    uint8_t WinBottom;       // 0…100
} VQ_AFM_Parameters_t;
/*
    CCS data structures
*/
typedef struct PACKED VQ_CCS_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    uint8_t  EnableGlobalMotionAdaptive;
    uint8_t  MotionInterpolation;
    uint8_t  LowMotionTh0;
    uint8_t  LowMotionTh1;
    uint8_t  StdMotionTh0;
    uint8_t  StdMotionTh1;
    uint8_t  HighMotionTh0;
    uint8_t  HighMotionTh1;
    uint16_t GlobalMotionStdTh;
    uint16_t GlobalMotionHighTh;
} VQ_CCS_Parameters_t;
/*
    Gamma data structures
*/
typedef struct PACKED VQ_Gamma_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    uint32_t Tbd;
} VQ_Gamma_Parameters_t;
/*
    Color data structures
*/
typedef struct PACKED VQ_COLOR_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    //uint32_t Tbd;
    uint8_t ColorSpaceConversion;
    VQ_CSCMatrixCoef_t CSCMatrixCoefs;
} VQ_COLOR_Parameters_t;
/*
    DNR data structures
*/
typedef struct PACKED VQ_DNR_Parameters_s
{
  FVDP_PSITuningData_t descriptor; // descriptor
  uint8_t DEBLKAutoBoundaryDetectThresh;   // automatic block boundary detection
                                        // threshold, to pass to block detection SW
                                        // range 0-10 with default of 5
  uint8_t DEBLKAdaptiveFilterSelectThresh; // Adaptive filter selection threshold
                                      // control, affects filter blockiness
                                      // thresholds
                                      // range 0-10 with default of 5
} VQ_DNR_Parameters_t;
/*
    MNR data structures
*/
typedef struct PACKED VQ_MNR_Parameters_s
{
  FVDP_PSITuningData_t descriptor; // descriptor
  uint8_t MSQNRFilterStrength;      // Mosquito noise reduction filter strength control
                               // interpreted to program the MSQNR filter strength
                               // registers
                               // range 0-10 with default of 5
} VQ_MNR_Parameters_t;
/*
    MADI data structures
*/
typedef struct PACKED VQ_MADI_Parameters_s
{
    FVDP_PSITuningData_t descriptor; // descriptor
    uint8_t EnableGlobalMotionAdaptive;
    VQ_MADiThreshold_t LowMotion;
    VQ_MADiThreshold_t StandardMotion;
    VQ_MADiThreshold_t HighMotion;
    VQ_MADiThreshold_t PanMotion;
    uint16_t GlobalMotionStandardThreshold;
    uint16_t GlobalMotionHighThreshold;
    uint8_t  GlobalMotionPanThreshold;
    uint8_t  GlobalMotionInterpolation;
    uint8_t  NoiseGain;
    uint8_t  ChromaGain;
    uint8_t  VerticalGain;
    uint8_t  LumaAdaptiveEnable;
    uint8_t  StaticHistGain;
    uint8_t  MVLHL;
    uint8_t  MVLHH;
    uint8_t  MVHHL;
    uint8_t  MVHHH;
    uint8_t  MCVLHL;
    uint8_t  MCVLHH;
    uint8_t  MCVHHL;
    uint8_t  MCVHHH;
    uint8_t  DeinterlacerDCDiEnable;
    uint8_t  DeinterlacerDCDiStrength;
    VQ_MADiStaticPixFix_t StaticPixFix;
} VQ_MADI_Parameters_t;
/*
    MCTI data structures
*/
typedef struct PACKED VQ_MCTI_FW_Parameters_s
{
    uint8_t  Mode_Switch_Force_Mode;
    uint8_t  Pan_Thr;
    uint16_t FW_BadCorr_Thr;
    uint16_t FW_HBadCorr_Thr;
    uint8_t  FW_Velocity_Thr_1;
    uint8_t  FW_Velocity_Thr_2;
    uint8_t  Lag_jmp;
    uint8_t  scnch_cnt;
    uint32_t OSD_Histogram_Local_Threshold;
    uint32_t OSD_Edge_Low_Threshold;
    uint32_t OSD_Edge_Threshold;
    uint32_t HW_Scene_Det_Thr;
    uint8_t  HW_Scene_Det_Pks_Ratio;
} VQ_MCTI_FW_Parameters_t;

typedef struct PACKED VQ_MCTI_HW_Parameters_s
{
    uint8_t  Val_MVE_RPD_RP_TH;
    uint8_t  Val_MVE_RPD_PER_MATCH;
    uint8_t  Val_MVE_RPD_VAR_TH;
    uint8_t  Val_MVE_RPD_NUM_NEIGH_2;
    uint8_t  Val_MVE_RPD_VALLEY_TH;
    uint8_t  Val_MVE_CVC_RPA_SPERIOD_TH;
    uint8_t  Val_MVE_CVC_RPA_NGC_TH;
    uint8_t  Val_MVE_CVC_RPA_SSAD_TH;
    uint8_t  Val_VPI_BOCC_TYPE;
    uint8_t  Val_VPI_AFP_OPOSD_STHRES;
    uint16_t Val_VPI_AFP_OPOSD_UVGAIN;
    uint8_t  Val_VPI_AFP_OPOSD_PMEDIAN;
    uint8_t  Val_VPI_BOCC_EN;
    uint8_t  Val_VPI_OSWIM_BLEND_FACTOR;
} VQ_MCTI_HW_Parameters_t;

typedef struct PACKED VQ_MCTI_Parameters_s
{
    FVDP_PSITuningData_t    descriptor; // descriptor
    VQ_MCTI_FW_Parameters_t SWParams;
    VQ_MCTI_HW_Parameters_t HWParams;
} VQ_MCTI_Parameters_t;
/*
    Scaler data structures
*/
typedef struct PACKED VQ_SCALER_Parameters_s
{
    FVDP_PSITuningData_t    descriptor; // descriptor
    FVDP_ScalerMode_t       ScalerMode;
    uint16_t CentralRegionPanoramicPercentage;
} VQ_SCALER_Parameters_t;
/*
    Sharpness data structures
*/

typedef struct PACKED VQ_SharpnessDataRange_s
{
    // max data range
    uint8_t MainLumaHGainMax;
    VQ_EnhancerParameters_t MainLumaHMax;
    uint8_t MainLumaVGainMax;
    VQ_EnhancerParameters_t MainLumaVMax;
    uint8_t MainChromaHGainMax;
    VQ_EnhancerParameters_t MainChromaHMax;
    uint8_t MainChromaVGainMax;
    VQ_EnhancerParameters_t MainChromaVMax;
    VQ_EnhancerNonLinearControl_t MainNLHMax;
    VQ_EnhancerNonLinearControl_t MainNLVMax;
    VQ_EnhancerNoiseCoringControl_t MainNCHMax;
    VQ_EnhancerNoiseCoringControl_t MainNCVMax;

    // min data range
    uint8_t MainLumaHGainMin;
    VQ_EnhancerParameters_t MainLumaHMin;
    uint8_t MainLumaVGainMin;
    VQ_EnhancerParameters_t MainLumaVMin;
    uint8_t MainChromaHGainMin;
    VQ_EnhancerParameters_t MainChromaHMin;
    uint8_t MainChromaVGainMin;
    VQ_EnhancerParameters_t MainChromaVMin;
    VQ_EnhancerNonLinearControl_t MainNLHMin;
    VQ_EnhancerNonLinearControl_t MainNLVMin;
    VQ_EnhancerNoiseCoringControl_t MainNCHMin;
    VQ_EnhancerNoiseCoringControl_t MainNCVMin;
} VQ_SharpnessDataRange_t;

typedef struct PACKED VQ_SHARPNESS_Parameters_s
{
    FVDP_PSITuningData_t    descriptor; // descriptor
    VQ_SharpnessDataRange_t DataRange;
    uint8_t NumOfSteps;

    VQ_Sharpness_Step_Setting_t StepData[SHARPNESS_STEPS_MAX_NUM];
} VQ_SHARPNESS_Parameters_t;
/*
    TNR data structures
*/
typedef struct PACKED VQ_TNR_Parameters_s
{
    FVDP_PSITuningData_t  descriptor;    // descriptor
    uint8_t  EnableGlobalNoiseAdaptive;  // 0 - TNR_STATIC; 1 - TNR_ADAPTIVE
    uint8_t  EnableGlobalMotionAdaptive;
    uint16_t GlobalMotionStdTh;          // motion threshold for standard
    uint16_t GlobalMotionHighTh;         // motion threshold for high
    uint8_t  GlobalMotionPanTh;          // motion threshold for fast vertical pan
    int8_t   GlobalMotionStd;            // noise subtractor for standard
    int8_t   GlobalMotionHigh;           // noise subtractor for high
    int8_t   GlobalMotionPan;            // noise subtractor for fast vertical pan
    VQ_TNR_Adaptive_Parameters_t AdaptiveData[12];    // 12 noise ranges
} VQ_TNR_Parameters_t;


typedef struct PACKED VQ_Mcdi_Parameters_s
{

    uint16_t    MCMADI_Blender_KLUT_0;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_0
    uint16_t    MCMADI_Blender_KLUT_1;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_1
    uint16_t    MCMADI_Blender_KLUT_2;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_2
    uint16_t    MCMADI_Blender_KLUT_3;//	2 bytes	0	512	1	255 display as 100%; 0 as 0%; linear; link to MCDI_BLENDER_LUT->MCDI_KLUT_3

    uint8_t     MCDI_LDME_VTAP_SEL;//	1 byte	0	3	1	Select one of these: 0 = no filter; 1 = 2 Tap filter; 2 = 5 Tap filter*/
    uint8_t     MCDI_LDME_BOFFSET;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_HME_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_VME_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_DME_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_BST;//	1 byte	0	31	1
    uint16_t    MCDI_LDME_BSP;//	2 bytes	0	4095	1
    int16_t     MCDI_LDME_HBM_OST0;//	2 bytes	-10	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on X axis. Typical value is: -5 -5 -4 -4 -3 -3 -2 -2 -1 -1 0
    int16_t     MCDI_LDME_HBM_OST1;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST2;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST3;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST4;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST5;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST6;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST7;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST8;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST9;//	2 bytes	-10	0	1
    int16_t     MCDI_LDME_HBM_OST10;//	2 bytes	-10	0	1
    uint8_t     MCDI_LDME_CNST_THR_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_CNST_THR_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_HSTILL_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_CNST_GAIN;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_ST;//	1 byte	0	7	1
    uint8_t     MCDI_LDME_HME_BM_GAIN;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_VME_BM_GAIN;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_DME_BM_GAIN;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_HME_NOISE;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_VME_NOISE;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_VCNST_NKL;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_SF_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_SF_THR_LO;//	1 byte	0	255	1
    uint16_t    MCDI_LDME_SF_THR_HI;//	2 bytes	0	1023	1
    int16_t     MCDI_LDME_VBM_OST0;//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost4 is at the center. There are 9 entries on y axis.Typical value is: -16 -8 -2 -1 0
    int16_t     MCDI_LDME_VBM_OST1;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_VBM_OST2;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_VBM_OST3;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_VBM_OST4;//	2 bytes	-32	0	1
    uint8_t     MCDI_LDME_STILL_NKL;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_STILL_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_HME_DIFF_THR;//	1 byte	0	3	1
    uint8_t     MCDI_LDME_VME_DIFF_THR;//	1 byte	0	3	1
    uint8_t     MCDI_LDME_DME_HME_DIFF_THR;//	1 byte	0	3	1
    uint8_t     MCDI_LDME_DME_VME_DIFF_THR;//	1 byte	0	3	1
    uint8_t     MCDI_LDME_DME_ST;//	1 byte	0	15	1	LDME_DME_ST
    uint8_t     MCDI_LDME_VME_ST;//	1 byte	0	15	1	LDME_VME_ST
    uint8_t     MCDI_LDME_SF_HST;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_SF_VST;//	1 byte	0	15	1
    uint8_t     MCDI_LDME_GSTILL_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_BSTILL_THR;//	1 byte	0	255	1
    bool        MCDI_LDME_HME_G3_EN;//	bool
    bool        MCDI_LDME_VME_G3_EN;//	bool
    bool        MCDI_LDME_DME_G3_EN;//	bool
    bool        MCDI_LDME_GSTILL_ENABLE;//	bool
    bool        MCDI_LDME_FILM_ENABLE;//	bool
    bool        MCDI_LDME_GHF_EN;//	bool
    bool        MCDI_LDME_GMOVE_EN;//	bool
    bool        MCDI_LDME_SCENE_EN;//	bool
    uint8_t     MCDI_LDME_GMOVE_HACC_GAIN_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_GMOVE_HACC_GAIN_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_GMOVE_VACC_GAIN_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_GMOVE_VACC_GAIN_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_ACC_THR_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_ACC_THR_LO;//	1 byte	0	255	1
    uint16_t    MCDI_LDME_GHF_ACC_THR;//	2 bytes	0	1023	1
    uint8_t     MCDI_LDME_LGM_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LDME_LGM_OFF;//	1 byte	0	255	1
    bool        MCDI_LDME_FIX_MIX_FILM_EN;//	bool			1	Mix_film_en; the register is 1 byte but only use bit0
    uint8_t     MCDI_LMVD_HLG_BTHR;//	1 byte	0	127	1
    uint8_t     MCDI_LDME_HLG_BTHR;//	1 byte	0	63	1
    uint16_t    MCDI_LMVD_HLG_LTHR;//	2 bytes	0	1023	1
    uint8_t     MCDI_LMVD_HLG_GTHR;//	1 byte	0	255	1
    int16_t     MCDI_LMVD_HLG_OST0;//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and os10 is at the center. There are 21 entries on x axis.Typical value is:-10 -9 -8 -7 -6 -5 -4 -3 -2 -1 0
    int16_t     MCDI_LMVD_HLG_OST1;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST2;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST3;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST4;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST5;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST6;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST7;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST8;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST9;//	2 bytes	-32	0	1
    int16_t     MCDI_LMVD_HLG_OST10;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST0;//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on x axis.Typical value is:-20 -20 -18 -16 -14 -12 -10 -8 -2 -1 0
    int16_t     MCDI_LDME_HLG_OST1;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST2;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST3;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST4;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST5;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST6;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST7;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST8;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST9;//	2 bytes	-32	0	1
    int16_t     MCDI_LDME_HLG_OST10;//	2 bytes	-32	0	1
    uint8_t     MCDI_LDME_HLG_NKL;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_HLG_NOISE;//	1 byte	0	31	1
    uint8_t     MCDI_LMVD_HLG_THR_LO;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_HLG_THR_HI;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_HLG_NKL;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_HLG_CNST_NKL;//	1 byte	0	63	1
    uint8_t     MCDI_LMVD_HLG_STILL_NKL;//	1 byte	0	63	1
    uint8_t     MCDI_LMVD_LMD_LV_ENABLE;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_LMD_HLV_THR_LO;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_LMD_HLV_THR_HI;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_LMD_VLV_THR_LO;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_LMD_VLV_THR_HI;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_VLVD_GAIN_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_VLVD_GAIN_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_HLVD_GAIN_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_HLVD_GAIN_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_HLVD_OFFSET;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_VLVD_OFFSET;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_VMV_THR_LO;//	1 byte	0	7	1
    uint8_t     MCDI_LMVD_VMV_THR_HI;//	1 byte	0	7	1
    uint8_t     MCDI_LMVD_HMV_THR_LO;//	1 byte	0	7	1
    uint8_t     MCDI_LMVD_HMV_THR_HI;//	1 byte	0	7	1
    bool        MCDI_LMVD_LHMV_EN;//	bool
    uint8_t     MCDI_LMVD_HST;//	1 byte	0	63	1
    uint8_t     MCDI_LMVD_VST;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_GSTILL_THR;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_GSTILL_THR_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_GSTILL_THR_HI;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_GSTILL_GAIN_LO;//	1 byte	0	255	1
    uint8_t     MCDI_LMVD_GSTILL_GAIN_HI;//	1 byte	0	255	1
    uint8_t     MCDI_MVD_GSTIL_COUNT_CTRL;//	1 byte	0	15	1
    uint8_t     MCDI_LMVD_DIV_MORE;//	1 byte	0	3	1
    uint8_t     MCDI_LDME_DIV_MORE;//	1 byte	0	3	1
    uint8_t     MCDI_SDI_NOISE;//	1 byte	0	63	1
    uint8_t     MCDI_SDI_THR;//	1 byte	0	63	1
    uint8_t     MCDI_SDI_HI_THR;//	1 byte	0	255	1
    uint8_t     MCDI_SDI_CNST_THR;//	1 byte	0	255	1
    uint16_t    MCDI_SDI_OST0;//	2 bytes	0	32	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost5 is at the center. There are 9 entries on x axis.Typical value is:0 0 0 0 0 25
    uint16_t    MCDI_SDI_OST1;//	2 bytes	0	32	1
    uint16_t    MCDI_SDI_OST2;//	2 bytes	0	32	1
    uint16_t    MCDI_SDI_OST3;//	2 bytes	0	32	1
    uint16_t    MCDI_SDI_OST4;//	2 bytes	0	32	1
    uint16_t    MCDI_SDI_OST5;//	2 bytes	0	32	1
    uint8_t     MCDI_SDI_OFFSET;//	1 byte	0	255	1
    uint8_t     MCDI_SDI_CNST_GAIN;//	1 byte	0	255	1	Link to the register
    uint8_t     MCDI_SDI_HI_GAIN;//	1 byte	0	255	1	Link to the register
    bool        MCDI_HSDME_Frame_Bypass;//	bool				SDME_HOR_EN button
    bool        MCDI_HSDME_Field_Bypass;//	bool				SDME_VER_EN button

    uint8_t     MCDI_HSDME_FRAME_NOISE;//	1 byte	0	31	1
    uint8_t     MCDI_HSDME_FIELD_NOISE;//	1 byte	0	31	1
    uint8_t     MCDI_HSDME_ST;//	1 byte	0	3	1
    uint8_t     MCDI_HSDME_STILL_NOISE;//	1 byte	0	63	1
    uint8_t     MCDI_VSDME_FRAME_NOISE;//	1 byte	0	255	1
    bool        MCDI_VSDME_Bypass;//	bool				VSDME_EN button
    int16_t     MCDI_HSDME_FRAME_OST0;//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center. There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    int16_t     MCDI_HSDME_FRAME_OST1;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FRAME_OST2;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FRAME_OST3;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FRAME_OST4;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FRAME_OST5;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FRAME_OST6;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST0;//	2 bytes	-32	0	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost6 is at the center. There are 11 entries on x axis.Typical value is:-16 -16 -14 -12 -10 -8 0
    int16_t     MCDI_HSDME_FIELD_OST1;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST2;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST3;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST4;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST5;//	2 bytes	-32	0	1
    int16_t     MCDI_HSDME_FIELD_OST6;//	2 bytes	-32	0	1
    uint16_t    MCDI_VSDME_FRAME_OST0;//	2 bytes	0	255	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost2 is at the center. There are 5 entries on y axis.Typical value is:-16 -8 0
    uint16_t    MCDI_VSDME_FRAME_OST1;//	2 bytes	0	255	1
    uint16_t    MCDI_VSDME_FRAME_OST2;//	2 bytes	0	255	1

    bool        MCDI_TDI_TNR_BYPASS;//	bool
    bool        MCDI_TDI_INT_TNR_BYPASS;//	bool
    bool        MCDI_TDI_SNR_BYPASS;//	bool
    uint8_t     MCDI_TDI_OFFSET;//	1 byte	0	3	1
    uint8_t     MCDI_TDI_HBST;//	1 byte	0	31	1
    uint16_t    MCDI_TDI_HBSP;//	2 bytes	0	4095	1
    uint8_t     MCDI_TDI_VBST;//	1 byte	0	255	1
    uint16_t    MCDI_TDI_VBSP;//	2 bytes	0	4095	1
    uint8_t     MCDI_TDI_IMPULSE_NKL;//	1 byte	0	255	1
    uint16_t    MCDI_TDI_IMPULSE_THR;//	2 bytes	0	1023	1
    uint8_t     MCDI_TDI_KLUT0;//	1 byte	0	255	1	Map to graphics. Typical value: 255 200 150 100
    uint8_t     MCDI_TDI_KLUT1;//	1 byte	0	255	1
    uint8_t     MCDI_TDI_KLUT2;//	1 byte	0	255	1
    uint8_t     MCDI_TDI_KLUT3;//	1 byte	0	255	1
    uint8_t     MCDI_TDI_SNR_KLUT0;//	1 byte	0	255	1	Map to graphics. Typical value: 255 128 0 0
    uint8_t     MCDI_TDI_SNR_KLUT1;//	1 byte	0	255	1
    uint8_t     MCDI_TDI_SNR_KLUT2;//	1 byte	0	255	1
    uint8_t     MCDI_TDI_SNR_KLUT3;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MDET_THR0;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MDET_THR1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MDET_THR2;//	1 byte	0	255	1
    uint8_t     MCDI_CL_VMV_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_HMV_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_NMV_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_SMALL_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_SMALL_GAIN_H;//I	1 byte	0	255	1
    uint8_t     MCDI_CL_SMALL_GAIN_L;//O	1 byte	0	255	1
    uint8_t     MCDI_CL_LARGE_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_DEFAULT_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_CNST_NKL;//	1 byte	0	255	1
    uint8_t     MCDI_CL_CNST_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_CNST_OFF;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_OFF;//	1 byte	0	15	1
    uint8_t     MCDI_CL_ANG_OFF;//	1 byte	0	15	1
    uint8_t     MCDI_CL_MAX_ANG_OFF;//	1 byte	0	15	1
    uint8_t     MCDI_CL_SMALL_OFF;//	1 byte	0	15	1
    uint8_t     MCDI_CL_SMALL_OFF_LO;//	1 byte	0	15	1
    uint8_t     MCDI_CL_SMALL_OFF_HI;//	1 byte	0	15	1
    uint8_t     MCDI_CL_HTRAN_OFF;//	1 byte	0	255	1
    uint8_t     MCDI_CL_VTRAN_OFF;//	1 byte	0	255	1
    uint8_t     MCDI_CL_GSTILL_OFF;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LAMD_NKL;//1 byte	0	63	1
    uint8_t     MCDI_CL_LAMD_SMALL_NKL;//	1 byte	0	15	1
    uint8_t     MCDI_CL_LAMD_BIG_NKL;//	1 byte	0	63	1
    uint8_t     MCDI_CL_LAMD_HTHR;//	1 byte	0	31	1
    uint8_t     MCDI_CL_LAMD_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_DIAG_GAIN;//	1 byte	0	15	1
    uint8_t     MCDI_CL_DIAG_GAIN1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_DIAG_GAIN2;//	1 byte	0	255	1
    uint8_t     MCDI_CL_HTRAN_THR_LO;//	1 byte	0	255	1
    uint16_t    MCDI_CL_HTRAN_THR_HI;//	2 bytes	0	1023	1
    uint8_t     MCDI_CL_VTRAN_THR_LO;//	1 byte	0	255	1
    uint16_t    MCDI_CL_VTRAN_THR_HI;//	2 bytes	0	1023	1
    uint8_t     MCDI_CL_HTRAN_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_HTRAN_GAIN1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_VTRAN_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_VTRAN_GAIN1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_GSTILL_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_GHF_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_GSTILL_GAIN0AND1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_GSTILL_GAIN0OR1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_ANG_FILM_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_FILM_GAIN3AND5;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LASD_NKL;//	1 byte	0	63	1
    uint8_t     MCDI_CL_LASD_THR;//	1 byte	0	63	1
    uint8_t     MCDI_CL_LASD_GAIN;//	1 byte	0	255	1
    uint8_t     MCDI_CL_DMV_GAIN;//	1 byte	0	255	1
    int8_t     MCDI_CL_STILL_ANG_OFF;//	1 byte	0	15	1
    bool        MCDI_CL_STILL_ANG_EN;//	bool
    uint16_t    MCDI_CL_STILL_ANG_GAIN;//	2 bytes	0	1023	1
    uint8_t     MCDI_CL_DIAG_HTHR;//	1 byte	0	7	1
    uint8_t     MCDI_CL_DIAG_VTHR;//	1 byte	0	7	1
    uint8_t     MCDI_CL_DIAG_HTHR_HI;//	1 byte	0	7	1
    uint8_t     MCDI_CL_DIAG_VTHR_LO;//	1 byte	0	7	1
    uint8_t     MCDI_CL_GSTILL_THR_HI;//	1 byte	0	15	1
    uint8_t     MCDI_CL_GSTILL_THR_LO;//	1 byte	0	15	1
    bool        MCDI_CL_DIAG_FILM_EN;//	bool
    bool        MCDI_CL_OK_PATCH_EN;//	bool
    bool        MCDI_CL_DIAG_STILL_EN;//	bool
    bool        MCDI_CL_GHF_EN;//	bool
    uint8_t     MCDI_CL_MVCL_THR0;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MVCL_THR1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MVCL_THR2;//	1 byte	0	255	1
    uint8_t     MCDI_CL_MVCL_LUT0;//	1 byte	0	3	1	AR_CTRL18-> CL_MVCL_LUT0
    uint8_t     MCDI_CL_MVCL_LUT1;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT2;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT3;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT4;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT5;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT6;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT7;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT8;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT9;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT10;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT11;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT12;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT13;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT14;//	1 byte	0	3	1
    uint8_t     MCDI_CL_MVCL_LUT15;//	1 byte	0	3	1
    uint8_t     MCDI_CL_LMD_LUT0;//	1 byte	0	255	1	AR_CTRL18-> CL_LMD_LUT0
    uint8_t     MCDI_CL_LMD_LUT1;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT2;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT3;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT4;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT5;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT6;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT7;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT8;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT9;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT10;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT11;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT12;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT13;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT14;//	1 byte	0	255	1
    uint8_t     MCDI_CL_LMD_LUT15;//	1 byte	0	255	1
}VQ_Mcdi_Parameters_t;


typedef struct PACKED VQ_McMadi_Parameters_s
{

    bool        MCMADI_DeinterlacerDcdiEnable;//	Bool				DEINTC_DCDI_EBALE bit in DEINTC_DCDI_CTRL
    uint8_t     MCMADI_DeinterlacerDcdiStrength;//	1 Byte	0 (display as 0%)	15 (display as 100%)	1	DEINTC_DCDI_PERLCORR_TH in DEINTC_DCDI_CTRL3
    uint8_t     MADI_LowMotionTh0;//	1 Byte	0	64 (always <=th1)	1	MADI_QUANT_THRESH0
    uint8_t     MADI_LowMotionTh1;//	1 Byte	0	64(always<th2)	1	MADI_QUANT_THRESH1
    uint8_t     MADI_LowMotionTh2;//	1 Byte	0	64	1	MADI_QUANT_THRESH2
    uint8_t     MADI_ChromaGain;//	1 Byte	0	3	1	Link to MADI_CHROMA_GAIN in MADI_GAIN register; the mapping of display vs register is:0-0 33-11 66-10 100-01
    uint8_t     MADI_MVLHL;//	1 Byte	0	63	1	Map to register MADI_W00
    uint8_t     MADI_MVLHH;//	1 Byte	0	63	1	Map to register MADI_W01
    uint8_t     MADI_MVHHL;//	1 Byte	0	63	1	Map to register MADI_W10
    uint8_t     MADI_MVHHH;//	1 Byte	0	63	1	Map to register MADI_W11
    uint8_t     MADI_MCVLHL;//	1 Byte	0	63	1	Map to register MADI_MC_W00
    uint8_t     MADI_MCVLHH;//	1 Byte	0	63	1	Map to register MADI_MC_W01
    uint8_t     MADI_MCVHHL;//	1 Byte	0	63	1	Map to register MADI_MC_W10
    uint8_t     MADI_MCVHHH;//	1 Byte	0	63	1	Map to register MADI_MC_W11
    bool        MADI_STATIC_VDGAIN_ENABLE;//	Bool				MADI_STATIC_VDGAIN_EN
    bool        MADI_STATIC_CLEAN_EN;//	Bool				MADI_STATIC_CLEAN_EN
    uint8_t     MADI_STATIC_VDGAIN_MAX_DIFF;//	1 Byte	0	127	1	MADI_STATIC_VDGAIN_MAX_DIFF
    uint16_t    MADI_STATIC_VDGAIN_MAX_DIFF_SQ;//_SQ	2 bytes	0	32767	1	MADI_STATIC_VDGAIN_MAX_DIFF_SQ
    uint8_t     MADI_STATIC_VDGAIN_RSHIFT;//	1 byte	0	15	1
    uint16_t    MADI_STATIC_VDGAIN_OFFSET;//	2 bytes	0	16383	1
    uint8_t     MADI_STATIC_DIFF_DEPTH;//	1 byte	0	15	1	Static history
    uint16_t    MADI_STATIC_LV_FRM_DIFF_THRESH;//	2 bytes	0	1023	1	LV static threshold
    uint16_t    MADI_STATIC_FRM_DIFF_THRESH;//	2 bytes	0	1023	1	static threshold
    uint8_t     MADI_STATIC_LARGE_MOTION_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_STATIC_LOW_CONTRAST_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_STATIC_LOW_CONTRAST_OFFSET;//	1 byte	0	255	1
    uint16_t    MADI_MDET_LO_THR;//	2 bytes	0	1023	1
    uint16_t    MADI_MDET_HI_THR;//	2 bytes	0	1023	1
    uint8_t     MADI_MDET_LO_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_MDET_HI_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_MDET_NORM_GAIN;//	1 byte	0	255	1
    uint16_t    MADI_MDET_THR0;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_THR1;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_THR2;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_LV_LO_THR;//	2 bytes	0	1023	1
    uint16_t    MADI_MDET_LV_HI_THR;//	2 bytes	0	1023	1
    uint8_t     MADI_MDET_LV_LO_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_MDET_LV_HI_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_MDET_LV_NORM_GAIN;//	1 byte	0	255	1
    uint16_t    MADI_MDET_LV_THR0;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_LV_THR1;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_LV_THR2;//	2 bytes	0	2047	1
    uint8_t     MADI_MDET_TDIFF_BLEND;//	1 byte	0	31	1
    uint16_t    MADI_MDET_HTRAN_THR;//	2 bytes	0	2047	1
    uint16_t    MADI_MDET_FIELDDIFF_THR;//	2 bytes	0	4095	1
    uint8_t     MADI_MDET_MDP_GAIN;//	1 byte	0	255	1
    uint16_t    MADI_LASD_NKL;//	2 bytes	0	1023	1
    uint8_t     MADI_LASD_THR;//	1 byte	0	255	1
    uint8_t     MADI_LASD_GAIN;//	1 byte	0	255	1
    uint16_t    MADI_LASD_LV_NKL;//	2 bytes	0	1023	1
    uint16_t    MADI_LAMD_NKL;//	2 bytes	0	1023	1
    uint16_t    MADI_LAMD_SMALL_NKL;//	2 bytes	0	1023	1
    uint8_t     MADI_LAMD_BIG_NKL;//	1 byte	0	255	1
    uint8_t     MADI_LAMD_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_LAMD_HTHR;//	1 byte	0	31	1
    uint8_t     MADI_LAMD_HSP;//	1 byte	0	7	1
    uint8_t     MADI_LAMD_VTHR;//	1 byte	0	31	1
    uint8_t     MADI_LAMD_VSP;//	1 byte	0	7	1
    uint16_t    MADI_LAMD_LV_NKL;//	2 bytes	0	1023	1
    uint16_t    MADI_LAMD_LV_SMALL_NKL;//	2 bytes	0	1023	1
    uint8_t     MADI_LAMD_LV_BIG_NKL;//	1 byte	0	255	1
    uint8_t     MADI_LAMD_LV_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_LAMD_LV_HTHR;//	1 byte	0	31	1
    uint8_t     MADI_LAMD_LV_HSP;//	1 byte	0	7	1
    uint8_t     MADI_LAMD_LV_VTHR;//	1 byte	0	31	1
    uint8_t     MADI_LAMD_LV_VSP;//	1 byte	0	7	1
    uint8_t     MADI_LAMD_LARGE_MOTION_GAIN;//	1 byte	0	255	1	MADI_LAMD_HD_GAIN
    uint16_t    MADI_CNST_NKL_LO;//	2 bytes	0	8191	1
    uint16_t    MADI_CNST_NKL_HI;//	2 bytes	0	8191	1
    uint16_t    MADI_CNST_NKL;//	2 bytes	0	8191	1
    uint8_t     MADI_CNST_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_CNST_Y_OFF;//	1 byte	0	255	1
    uint16_t    MADI_CNST_LV_NKL;//	2 bytes	0	8191	1
    uint8_t     MADI_CNST_LV_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_CNST_LV_Y_OFF;//	1 byte	0	255	1
    uint16_t    MADI_CNST_ST_NKL;//	2 bytes	0	8191	1
    uint8_t     MADI_CNST_ST_GAIN;//	1 byte	0	255	1
    uint8_t     MADI_CNST_ST_Y_OFF;//	1 byte	0	255	1
    uint16_t    MADI_LMVD_HLG_OST0;//	2 bytes	0	64	1	Map to the graphics. The curve is symmetric: ost0 is at the beginning/end and ost10 is at the center. There are 21 entries on X axis.Typical value is: 0 0 0 0 4 8 12 16 20 24 28
    uint16_t    MADI_LMVD_HLG_OST1;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST2;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST3;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST4;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST5;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST6;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST7;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST8;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST9;//	2 bytes	0	64	1
    uint16_t    MADI_LMVD_HLG_OST10;//	2 bytes	0	64	1
    uint8_t     MADI_LMVD_HLG_MD_NOISE;//	1 byte	0	255	1
    uint8_t     MADI_LMVD_HLG_NKL;//	1 byte	0	255	1
    uint16_t    MADI_LMVD_HLG_MD_CNST_NKL;//	2 bytes	0	4095	1
    uint16_t    MADI_LMVD_HLG_MD_STILL_NKL;//	2 bytes	0	1023	1
    uint8_t     MADI_LMVD_HLG_MD_THR_LO;//	1 byte	0	15	1
    uint16_t    MADI_LMVD_HLG_MD_gthr_ratio;// 2 byte	0	65535	1	Ratio for global_lv threshold. The value write to LMVD_hlg_md_gthr=input pixels*ratio/100000
                                            //For example, set ratio=145, for NTSC (720*240) input, the final register value is: 720*240*145/100000=250.56, round to 250.
}VQ_McMadi_Parameters_t;

/*
    TNR 5 data structures
*/
typedef struct PACKED VQ_TNR_Advanced_Parameters_s
{
    uint8_t  TNR_TNRK_GAIN_0;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_TNRK_GAIN_1;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_TNRK_GAIN_2;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_TNRK_GAIN_3;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=1 case
    uint16_t TNR_TNRK_LIMIT_0;//	2 bytes	0	1024	750	K limit for highnoise=0 and fleshtone=0 case
    uint16_t TNR_TNRK_LIMIT_1;//	2 bytes	0	1024	100	K limit for highnoise=0 and fleshtone=1 case
    uint16_t TNR_TNRK_LIMIT_2;//	2 bytes	0	1024	900	K limit for highnoise=1 and fleshtone=0 case
    uint16_t TNR_TNRK_LIMIT_3;//	2 bytes	0	1024	100	K limit for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_TNRK_GAIN_UV_0;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_TNRK_GAIN_UV_1;//	Byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_TNRK_GAIN_UV_2;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_TNRK_GAIN_UV_3;//	Byte	0	255	80	Motion gain for highnoise=1 and fleshtone=1 case
    uint16_t TNR_TNRK_LIMIT_UV_0;//	2 bytes	0	1023	750	K limit for highnoise=0 and fleshtone=0 case
    uint16_t TNR_TNRK_LIMIT_UV_1;//	2 bytes	0	1023	100	K limit for highnoise=0 and fleshtone=1 case
    uint16_t TNR_TNRK_LIMIT_UV_2;//	2 bytes	0	1023	900	K limit for highnoise=1 and fleshtone=0 case
    uint16_t TNR_TNRK_LIMIT_UV_3;//	2 bytes	0	1023	100	K limit for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_TNRK_M2_GAIN;//	Byte	0	255	16	Extra gain on top for M2 case: i.e the pixel is identified as noise
    uint8_t  TNR_TNRK_MULT_M1;//	Byte	0	3	2	Noise gain1 under M1 case
    uint8_t  TNR_TNRK_GAIN_M1;//	Byte	0	31	16	Noise gain2 under M1 case
    int16_t  TNR_TNRK_OFFSET_M1;//	2 bytes	-255	255	-12	Noise offset under M1 case for Y
    int16_t  TNR_TNRK_OFFSET_UV_M1;//	2 bytes	-255	255	-18	Noise offset under M1 case for UV
    uint8_t  TNR_TNRK_MULT_M2;//	Byte	0	3	2	Noise gain1 under M2 case
    uint8_t  TNR_TNRK_GAIN_M2;//	Byte	0	31	16	Noise gain2 under M2 case
    int16_t  TNR_TNRK_OFFSET_M2;//	2 bytes	-255	255	-12	Noise offset under M2 case for Y
    int16_t  TNR_TNRK_OFFSET_UV_M2;//	2 bytes	-255	255	-18	Noise offset under M2 case for UV
    bool     TNR_TNRK_CLIP_LUT;//	Bool	 	 	0	Clamp K to limit
    uint16_t TNR_TNRK_FIRST_LIMIT;//	2 bytes	0	1023	950	Limit the motion->K LUT
    uint16_t TNR_TNRK_SECOND_LIMIT;//	2 bytes	0	1023	850	Start position for a smooth transition to limit in the motion->K LUT
    uint8_t  TNR_KLOGIC_LPF_SEL;//	1 bytes	0	2	2	Final filter for K between current and previous
    uint8_t  TNR_STNRK_GAIN_0;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_STNRK_GAIN_1;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_STNRK_GAIN_2;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_STNRK_GAIN_3;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_STNRK_LIMIT_0;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_STNRK_LIMIT_1;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_STNRK_LIMIT_2;//	1 byte	0	255	0	K limit for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_STNRK_LIMIT_3;//	1 byte	0	255	255	K limit for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_STNRK_GAIN_UV_0;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_STNRK_GAIN_UV_1;//	1 byte	0	255	80	Motion gain for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_STNRK_GAIN_UV_2;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_STNRK_GAIN_UV_3;//	1 byte	0	255	255	Motion gain for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_STNRK_LIMIT_UV_0;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=0 case
    uint8_t  TNR_STNRK_LIMIT_UV_1;//	1 byte	0	255	255	K limit for highnoise=0 and fleshtone=1 case
    uint8_t  TNR_STNRK_LIMIT_UV_2;//	1 byte	0	255	0	K limit for highnoise=1 and fleshtone=0 case
    uint8_t  TNR_STNRK_LIMIT_UV_3;//	1 byte	0	255	255	K limit for highnoise=1 and fleshtone=1 case
    uint8_t  TNR_STNRK_M2_GAIN;//	Byte	0	255	16	Extra gain on top for M2 case: i.e the pixel is identified as noise
    uint8_t  TNR_STNRK_GAIN_M1;//	Byte	0	31	32	Noise gain1 under M1 case
    int16_t  TNR_STNRK_OFFSET_M1;//	2 bytes	-255	255	-12	Noise offset under M1 case for Y
    int16_t  TNR_STNRK_OFFSET_UV_M1;//	2 bytes	-255	255	-10	Noise offset under M1 case for UV
    uint8_t  TNR_STNRK_GAIN_M2;//	Byte	0	31	32	Noise gain1 under M2 case
    int16_t  TNR_STNRK_OFFSET_M2;//	2 bytes	-255	255	-12	Noise offset under M2 case for Y
    int16_t  TNR_STNRK_OFFSET_UV_M2;//	2 bytes	-255	255	-10	Noise offset under M2 case for UV
    bool     TNR_SNR_en;//	Bool	 	 	1	Enable SNR luma noise reduction
    bool     TNR_SNR_UV_EN;//	Bool	 	 	0	Enable SNR Chroma noise reduction
    bool     TNR_SNR_MED_EN;//	Bool	 	 	1	Enable Median filter in Luma channel
    bool     TNR_SNR_MED_BIT;//	Bool	 	 	1	Select the direction of Luma Median filter
    bool     TNR_SNR_DIAG_DISP_FLAG;//	Bool	 	 	0	Enable disable diagonal median.
    bool     TNR_SNR_FORCE_THRESHOLD;//	Bool	 	 	0	Force correlated averager coefficient
    uint8_t  TNR_SNR_SIGMA_P;//	Byte	0	31	0	Forced correlated averager coefficient Sigma_p
    uint8_t  TNR_SNR_SIGMA_N;//	Byte	0	31	0	Forced correlated averager coefficient Sigma_n
    bool     TNR_SNR_DITHER_ENABLE;//	Bool	 	 	0	Luma dithering enable
    bool     TNR_SNR_NO_BITS;//	Bool	 	 	2	Luma dithering bit select
    uint8_t  TNR_SNR_NOISE_THRES_BASED;//	Byte	0	255	0	Base noise in Luma Median filter. If system smaller than this value, force Noise_low_thr to 0
    uint8_t  TNR_SNR_BIG_THRESHOLD;//	Byte	0	255	80	Big threshold to determine the direction. When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    uint8_t  TNR_SNR_LOW_THRESHOLD;//	Byte	0	255	4	Low threshold to determine the flat region. It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    bool     TNR_SNR_UV_LPF_SEL;//	bool	 	 	1	Chroma low pass filter coefficient select
    bool     TNR_SNR_UV_DIV_SEL;//	bool	 	 	1	Chroma differential data low pass filter coefficient select
    uint8_t  TNR_SNR_UV_BIT_SEL;//	1 byte	0	7	0	Chroma bit select
    uint8_t  TNR_SNR_UV_OFFSET;//	1 byte	0	63	5	SNR chroma K logic offset
    uint8_t  TNR_SNR_UV_GAIN;//	1 byte	0	127	60	SNR chroma K logic gain
    uint8_t  TNR_SNR_UV_LIMIT;//	1 byte	0	255	128	SNR chroma K logic limit
    uint16_t TNR_SNR_UV_BIG_THRESHOLD;//	2 bytes	0	1023	325	Big threshold to determine the direction. When there are edges passing through the small neighborhood (3*3), the center pixel is highly non-correlated with some of the surrounding pixels and correlated with few pixels.
    uint16_t TNR_SNR_UV_LOW_THRESHOLD;//	2 bytes	0	1023	50	Low threshold to determine the flat region. It is observed that pixels affected by impulse noise are highly non-correlated with the surrounding pixels.
    bool     TNR_MDET_LUMA_BITS;//     	Bool	 	 	1	Select luma adaptive control data path bits
    uint8_t  TNR_MDET_LUMA_GAIN;// 	Byte	0	63	20	Special gain for low luma area
    bool     TNR_MDET_LUMA_GAIN_SEL;// 	Bool	 	 	1	Luma calculation block weighted or not
    uint8_t  TNR_MDET_LUMA_BIT_SEL;//  	Byte	0	7	2	Division fact for luma calculation
    uint8_t  TNR_MDET_LUMA_CTL_GAIN;//	Byte	0	15	1	Luma calculation gain
    uint8_t  TNR_MDET_LUMA_LOW_THRES;//	Byte	0	63	0	Low luma threshold
    uint8_t  TNR_MDET_MEDIAN_SELECT;//	Byte	0	3	3	Luma motion median filter select
    bool     TNR_MDET_AVG_SEL;//	Bool	 	 	0	MOT_1 (motion case) average method select
    bool     TNR_MDET_NC_WINDOW_SIZE;//	Bool	 	 	9	MOT_2(noise case) window size select
    bool     TNR_MDET_MOT2_RND_EN;//	Bool	 	 	0	Enable rounding in MOT_2 path
    uint8_t  TNR_MDET_MOT2_SHIFT;//	Byte	0	3	0	Left shift for MOT_2 path
    bool     TNR_MDET_LPF_EN;//	Bool	 	 	1	Enable final low pass filter for motion data
    bool     TNR_MDET_ND_ENABLE;//    	Bool	 	 	1	Enable Noise Discriminator
    bool     TNR_MDET_ND_FORCE_FLAG;//	Bool	 	 	0	Force noise discriminator output to noise instead of motion
    bool     TNR_MDET_ND_PRECISION;//	Bool	 	 	1	Noise Discriminator precision
    uint8_t  TNR_MDET_ND_CNT_THRES;//	Byte	0	7	3	Noise Discriminator counter threshold
    uint8_t  TNR_FT_u_min;//	Byte	0	255	7	Define the flesh tone U region minimum
    uint8_t  TNR_FT_u_max;//	Byte	0	255	30	Define the flesh tone U region maximum
    uint8_t  TNR_FT_v_min;//	Byte	0	255	9	Define the flesh tone V region minimum
    uint8_t  TNR_FT_v_max;//	Byte	0	255	32	Define the flesh tone V region maximum
    uint8_t  TNR_FT_uv_diff_max;//	Byte	0	255	26	Define the UV difference maximum threshold
    bool     TNR_FT_lpf_bypass;//	Bool	 	 	0	Enable bypass low pass filter for fleshtone
    bool     TNR_FT_Y_FIX_EN;//	Bool	 	 	0	Enable fleshtone detection on the luma path
    bool     TNR_FT_UV_FIX_EN;//	Bool	 	 	0	Enable fleshtone detection on th chroma path
    bool     TNR_EDGE_ENABLE;//	Bool	 	 	1	Enable edge detection
    bool     TNR_EDGE_TAPS;//	Bool	 	 	3	Select edge detection taps
    bool     TNR_EDGE_DIFF_SEL;//	Bool	 	 	2	Max control for luma difference
    uint8_t  TNR_Edge_gain_m1;//	Byte	0	63	11	M1 case edge gain
    uint8_t  TNR_Edge_gain_m2;//	Byte	0	63	11	M2 case edge gain
    bool     TNR_Edge_exp_en;//	Bool	 	 	1	Enable expansion filtering
    bool     TNR_Edge_exp_tap;//	Bool	 	 	0	Number of taps for exp filtering
    bool     TNR_Edge_lpf_en;//	Bool	 	 	3	Enable low pass filter (when expansion filtering is disabled)
    bool     TNR_EDGE_LPF_TAPS;//	Bool	 	 	0	Low pass filter tap selection
    bool     TNR_DITHER_BYPASS;//	Bool	 	 	1	Bypass anti dithering filter
    uint16_t TNR_DITHER_K;//	2 Byte	0	1023	128	Coefficient for anti dithering filter
    bool     TNR_STICKY_ENABLE;//	Bool	 	 	1	Sticky pixel control
    uint8_t  TNR_STICKY_THRES_Y;//	Byte	0	15	0	Threshold for sticky pixel control in Y channel. Filtered difference smaller than this value will be considered as sticky pixel
    uint8_t  TNR_STICKY_THRES_UV;//	Byte	0	15	0	Threshold for sticky pixel control in UV channel.

} VQ_TNR_Advanced_Parameters_t;
#endif /* #ifndef ___FVDP_VQDATA_H */

/* End of fvdp_vqdata.h */
