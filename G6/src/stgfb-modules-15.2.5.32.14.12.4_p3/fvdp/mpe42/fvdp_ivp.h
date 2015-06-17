/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_ivp.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_IVP_H
#define FVDP_IVP_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/* Exported Constants ------------------------------------------------------- */

// EricB ToDo:  This input source mapping should be moved to be part of a public interface
//               since it needs to be the same within FVDP and in the FVDP user SW.

#define VXI     FVDP_VMUX_INPUT1
#define VTAC1   FVDP_VMUX_INPUT2
#define VTAC2   FVDP_VMUX_INPUT3
#define DTG     FVDP_VMUX_INPUT4
#define MDTP1   FVDP_VMUX_INPUT5
#define MDTP2   FVDP_VMUX_INPUT7
#define MDTP0   FVDP_VMUX_INPUT6


/* Exported Types ----------------------------------------------------------- */

#if (ISM_HW_REV == REV_4_0)
typedef enum
{
    IVP_SRC_DIP = 0,            // 00000 = Select input from Digital Input Port.
                                // 00001 = NOT CONNECTED
                                // 00010 = NOT CONNECTED
                                // 00011 = NOT CONNECTED
                                // 00100 = NOT CONNECTED
                                // 00101 = NOT CONNECTED
                                // 00110 = NOT CONNECTED
                                // 00111 = NOT CONNECTED
                                // 01000 = NOT CONNECTED
                                // 01001 = NOT CONNECTED
    IVP_SRC_VTAC1 = 10,         // 01010 = Select input from VTAC1 Port
    IVP_SRC_VTAC2 = 11,         // 01011 = Select input from VTAC2 Port
    IVP_SRC_MDTP0_YUV = 12,     // 01100 = Select H.264/MPEG YUV input from MDTP 0
    IVP_SRC_MDTP0_RGB = 13,     // 01101 = Select H.264/MPEG RGB input from MDTP 0
    IVP_SRC_MDTP0_YUV_3D = 14,  // 01110 = Select H.264/MPEG YUV 3D input from MDTP 0
    IVP_SRC_MDTP1_YUV = 15,     // 01111 = Select H.264/MPEG YUV input from MDTP 1
    IVP_SRC_MDTP1_RGB = 16,     // 10000 = Select H.264/MPEG RGB input from MDTP 1
    IVP_SRC_MDTP2_YUV = 17,     // 10001 = Select H.264/MPEG YUV input from MDTP 2
    IVP_SRC_MDTP2_RGB = 18,     // 10010 = Select H.264/MPEG RGB input from MDTP 2
                                // 10011 = RESERVED
    IVP_SRC_DTG = 31,           // 11111 = Select input from Internal DTG
    IVP_SRC_NUM = 32            // Number of inputs
} IVP_Src_t;
#else
typedef enum
{
    IVP_SRC_DIP = 0,        //Digital Input Port
    IVP_SRC_HDMI = 1,       //HDMI Input Port
    IVP_SRC_ANALOG = 2,     //HD Video/Graphics input from Analog Input Port
    IVP_SRC_SD_VIDEO = 3,   //SD Video input from Analog Input Port
    IVP_SRC_STG = 4,        //Internal Source Timing Generator
    IVP_SRC_FB_OVERLAY = 5, //Fast Blank Overlay from Analog Input Port
    IVP_SRC_MDTP1_422 = 6,       //H.264 from MDTP1
    IVP_SRC_HDMI2 = 7,      //HDMI2 Input Port
    IVP_SRC_LVDS = 8,       //LVDS Input Port
    IVP_SRC_LVDS2 = 9,      //LVDS2 Input Port
    IVP_SRC_DPRX = 10,      //DPRX Input Port
    IVP_SRC_DPRX2 = 11,     //DPRX2 Input Port
    IVP_SRC_MDTP1_420 = 12,   //H.264 YUV420 from MDTP1
    IVP_SRC_MDTP2_422 = 13,   //H.264 YUV422 from MDTP2
    IVP_SRC_MDTP2_420 = 14,   //H.264 YUV420 from MDTP2
    IVP_SRC_MDTP1_RGB = 15,   //H.264 RGB from MDTP1
    IVP_SRC_MDTP2_RGB = 16,   //H.264 RGB from MDTP2
    IVP_SRC_DTG = 31,       //Internal DTG
    IVP_SRC_NUM = 32        //Number of inputs
} IVP_Src_t;
#endif

typedef enum
{
    IVP_ENABLED = BIT0, //set input capture enabled
    IVP_DISABLED = BIT1, //set input capture disabled
    IVP_SET_INTERLACED = BIT2, //turn on Interlaced input
    IVP_SET_PROGRESSIVE = BIT3, //turn on Progressive input
    IVP_SET_DV_ON = BIT4, //turn Input to be slave to Data Valid (DVI/HDMI/DP)
    IVP_SET_DV_OFF = BIT5, //turn Input from DE slave to normal Sync
    IVP_FREEZE_ON = BIT6, //Freeze window
    IVP_FREEZE_OFF = BIT7, //unfreeze window
    IVP_SET_BLACK_OFFS_ON = BIT8, //add 0x40 offset to incoming luma data. set black level of data captured from ADC
    IVP_SET_BLACK_OFFS_OFF = BIT9 //turn black offset off
} IVP_Attr_t;

typedef enum
{
    IVP_CAPTURE_ALL_UPDATE = 0,
    IVP_CAPTURE_NO_HORIZ_UPDATE = BIT0, //HSyncDelay, HStart, Width are not updated (could be any value)
    IVP_CAPTURE_NO_VERT_UPDATE = BIT1, //VSyncDelay, VStart, Height are not updated
    IVP_CAPTURE_NO_POSITION_UPDATE = BIT3, //HDelay, HStart, VDelay, VStart are not updated
    IVP_CAPTURE_NO_SIZE_UPDATE = BIT4, //Width, Height are not updated
    IVP_CAPTURE_SYNC_EVEN_OFFSET = BIT7  //port related even-field-sync-alignment(legacy: gmd_SYNC_EVEN_OFFSET)
} IVP_CaptureFlag_t;

typedef struct
{
    uint16_t HSyncDelay; //Horizontal Sync delay
    uint16_t VSyncDelay; //Vertical Sync Delay
    uint16_t HStart; //Horizontal start
    uint16_t VStart; //Vertical start
    uint16_t Width; //Horizontal width
    uint16_t Height; //Vertical height
} IVP_Capture_t;

typedef enum
{
    IVP_PLT_CH_YG = 0,
    IVP_PLT_CH_UB = 1,
    IVP_PLT_CH_VR = 2,
    IVP_PLT_CH_SIZE = 3,
    IVP_PLT_ENABLE = BIT2,
    IVP_PLT_DISABLE = BIT3
} IVP_PltSetFlag_t;

typedef struct
{
    uint16_t SubOff1;
    uint16_t SubOff2;
    uint16_t Gain0;
    uint16_t Gain1;
    uint16_t Gain2;
    uint16_t AddOff0;
    uint16_t AddOff1;
    uint16_t AddOff2;
} IVP_Plt_t;

typedef enum
{
    //only one of the following 3 options should be selected
    IVP_420_YUV = BIT0, //Input is 4:2:2 YUV
    IVP_422_YUV = BIT2, //Input is 4:2:2 YUV
    IVP_444_RGB = BIT1, //Input is 4:4:4 RGB
    IVP_444_YUV = BIT1|BIT2, //Input is 4:4:4 YUV

    //only one of the following 3 options should be selected
    IVP_RGB_TO_YUV = BIT4, //set to convert RGB input to YUV color space (CSC is not used)
    IVP_RGB_TO_YUV_940 = BIT4|BIT5, //video is encoded as RGB with dynamic range of 64-940 as per CCIR861 (CSC is not used)
    IVP_RGB_TO_YUV_CSC = BIT6, //convert thru CSC matrix.
} IVP_CscFlag_t;

typedef enum
{
    IVP_PATGEN_TEST_INV = BIT0, //Inverts test pattern data
    IVP_PATGEN_ODD_POL_INV = BIT1, //Invert ODD polarity for new patterns sensitive to field ID
    IVP_PATGEN_NO_COLOR_UPDATE = BIT2, //colors fields are not updated
    IVP_PATGEN_ENABLE = BIT3, //Test pattern is used as data input.
    IVP_PATGEN_DISABLE = 0 //Test pattern is not used as data input.
} IVP_PatGenFlag_t;

typedef struct
{
    uint8_t pattern; /* set<<5 + pattern; */
    uint8_t noiseLevel; /* 0-7 */
    uint8_t blue, green, red; /*bit3:0 - background, bit7:4 - foreground */
} IVP_PatGen_t;

typedef struct
{
    uint16_t red, green, blue;
} IVP_PixGrab_t;

typedef enum
{
    IVP_MEASURE_ENABLE = BIT0, //Enable the IP_MEASURE functionality (does not affect IBD/MinMax/SumDif/Feat)
    IVP_MEASURE_DISABLE = BIT1, //Disable the IP_MEASURE functionality (does not affect IBD/MinMax/SumDif/Feat)

    IVP_MEASURE_WIN_ENABLE = BIT2, //turn on mode: to measure only-measure-window-data (does not affect IBD/MinMax/SumDif/Feat)
    IVP_MEASURE_WIN_DISABLE = BIT3, //turn on mode:  to measure all-active-data (does not affect IBD/MinMax/SumDif/Feat)

    IVP_MEASURE_CLK_DISABLE = BIT4, /* Disable the Clock per Frame measure functionality */
    IVP_MEASURE_CLK_ENABLE = BIT5, /* Enable the Clock per Frame measure functionality */
    IVP_MEASURE_CLK_INTLC = BIT4|BIT5, /* Count CLK's per frame between every two input Vsync pulses */

    IVP_MEASURE_SC_ENABLE = BIT6, /* Scene Change checking enable */
    IVP_MEASURE_SC_DISABLE = BIT7, /* Scene Change checking disable */
    IVP_MEASURE_SC_SRC_EXT = BIT8, /* Scene Change Source = external detector */
    IVP_MEASURE_SC_SRC_INT = BIT9, /* Scene Change Source = internal detector */
    IVP_MEASURE_SC_EXT = BIT10, /* Scene change bit used for external control */
    IVP_MEASURE_SC_INT = BIT11 /* Scene change bit used for internal control */
} IVP_MeasureFlag_t;

enum
{
    IVP_SC_KEEP_THRESHOLD = (uint32_t)(-1) /* Scene change threshold will not be changed */
};

typedef struct
{
    uint16_t HStart; //Input Processor Horizontal Measure Window Start
    uint16_t VStart; //Input Processor Vertical Measure Window Start
    uint16_t Width; //Input Processor Horizontal Measure Window Width
    uint16_t Height; //Input Processor Vertical Measure Window Length
} IVP_Measure_t;

typedef enum
{
    IVP_INTC_INSTALL = BIT0,
    IVP_INTC_UNINSTALL = BIT1,
    IVP_INTC_CHECK_STATUS = BIT2,
    IVP_INTC_CLEAR_STATUS = BIT3
} IVP_IntrptAction_t;

typedef enum
{
    IVP_INT_VS, /* Input vertical sync has occurred */
    IVP_INT_ODD, /* The current field is ODD */
    IVP_INT_VACTIVE, /* Input timing has passed the vertical active start */
    IVP_INT_VBLANK, /* Input timing  has passed the vertical active end */
    IVP_INT_EOF, /* Input timing has passed the last active pixel */
    IVP_INT_FLAGLINE_1, /* Input timing has passed the event programmed by FLAGLINE_1 */
    IVP_INT_FLAGLINE_2, /* Input timing has passed the event programmed by FLAGLINE_2 */
    IVP_INT_FLAGLINE_3, /* Input timing has passed the event programmed by FLAGLINE_3 */
    IVP_INT_FLAGLINE_4, /* Input timing has passed the event programmed by FLAGLINE_4 */
    IVP_INT_FLAGLINE_5, /* Input timing has passed the event programmed by FLAGLINE_5 */
    IVP_INT_FLAGLINE_6, /* Input timing has passed the event programmed by FLAGLINE_6 */
    IVP_INT_FLAGLINE_7, /* Input timing has passed the event programmed by FLAGLINE_7 */
    IVP_INT_FLAGLINE_8, /* Input timing has passed the event programmed by FLAGLINE_8 */
    IVP_INT_SYNC_UPDATE_DONE, /* A synchronous update has finished */
    IVP_INT_FRAME_RESET, /* A frame reset is or has occurred */
    IVP_INT_HISTOGRAM_READY, /* The Input Measurement Luminence histogram is ready for read back */
    IVP_INT_SCENE_CHANGE, /* A scene change has been detected */
    IVP_INT_COMP_VBI_READY, /* VBI Data is ready */
    IVP_INT_SIZE
} IVP_IntrptId_t;

typedef enum
{
    IVP_SUMDIFF_DISABLE = BIT0, /* Disable the sum of differences function */
    IVP_SUMDIFF_ENABLE = BIT1 /* Enable the sum of differences function. */
} IVP_SumDiff_t;

typedef enum
{
    IVP_SUMDIFF_RED, /* Red color channel to read */
    IVP_SUMDIFF_GREEN, /* Green color channel to read */
    IVP_SUMDIFF_BLUE, /* Blue color channel to read */
    IVP_SUMDIFF_RGB /* Red+Green+Blue color channel to read */
} IVP_SumDiffColor_t;

typedef enum
{
    IVP_MINMAX_DISABLE = 0, /* Disable the minmax search feature */
    IVP_MINMAX_MIN = 1, /* Enable minmax search feature. Search for minimum RGB pixel values */
    IVP_MINMAX_MAX = 2 /* Enable minmax search feature. Search for maximum RGB pixel values */
} IVP_MinMax_t;

typedef enum
{
    IVP_MINMAX_RED, /* Red color channel to read */
    IVP_MINMAX_GREEN, /* Green color channel to read */
    IVP_MINMAX_BLUE, /* Blue color channel to read */
    IVP_MINMAX_COLORS
} IVP_MinMaxColor_t;

typedef enum
{
    IVP_IBD_ENABLE = BIT0, /* IBD is enabled */
    IVP_IBD_DISABLE = BIT1, /* IBD is disabled */

    IVP_IBD_DATA = BIT2, /* IBD measures DATA */
    IVP_IBD_DE = BIT3, /* IBD measures DE */

    IVP_IBD_MEASWIN_EN = BIT4, /* IBD monitors all incoming data (not confined by the measure window) */
    IVP_IBD_MEASWIN_DIS = BIT5, /* IBD monitors incoming data within the programmed measure window */
} IVP_Ibd_t;

enum
{
    IVP_IBD_KEEP_THRESHOLD = (uint32_t)(0x10) /* threshold will not be changed */
};

typedef struct IVP_CSCMatrixCoef_s
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
} IVP_CSCMatrixCoef_t; // CSC matrix coefficients

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

FVDP_Result_t IVP_Init (FVDP_CH_t CH);
FVDP_Result_t IVP_Term (FVDP_CH_t CH);
FVDP_Result_t IVP_Update(FVDP_CH_t          CH,
                        FVDP_VideoInfo_t*  InputVideoInfo_p,
                        FVDP_CropWindow_t* CropWindow_p,
                        FVDP_RasterInfo_t* InputRasterInfo_p,
                        FVDP_ProcessingType_t ProcessignType);
FVDP_FieldType_t IVP_GetPrevFieldType(FVDP_CH_t CH);
FVDP_Result_t IVP_WindowAdjust(FVDP_CH_t          CH,
                              FVDP_VideoInfo_t*  InputVideoInfo_p,
                              FVDP_CropWindow_t* CropWindow_p,
                              FVDP_RasterInfo_t* InputRasterInfo_p);
FVDP_Result_t IVP_SourceSet(FVDP_CH_t CH, IVP_Src_t Source);
IVP_Src_t IVP_SourceGet(FVDP_CH_t CH);
FVDP_Result_t IVP_AttrSet(FVDP_CH_t CH, IVP_Attr_t Flag);
FVDP_Result_t IVP_CaptureSet(FVDP_CH_t CH,
                           IVP_CaptureFlag_t Flag,
                           IVP_Capture_t const* pVal);
FVDP_Result_t IVP_CscSet(FVDP_CH_t CH, IVP_CscFlag_t Flag, IVP_CSCMatrixCoef_t* CSC_p);
FVDP_Result_t IVP_PatGenSet(FVDP_CH_t CH, IVP_PatGenFlag_t Flag, IVP_PatGen_t const* pVal);
FVDP_Result_t IVP_PixGrabSet(FVDP_CH_t CH, uint16_t X, uint16_t Y);
FVDP_Result_t IVP_PixGrabRead(FVDP_CH_t CH, IVP_PixGrab_t* const Pixel);
FVDP_Result_t IVP_MeasureSet(FVDP_CH_t CH,
                            IVP_MeasureFlag_t Flag,
                            IVP_Measure_t const* pVal,
                            uint32_t SCThreshold);
FVDP_Result_t IVP_MinMaxSet(FVDP_CH_t CH, IVP_MinMax_t Flag);
FVDP_Result_t IVP_IbdSet(FVDP_CH_t CH, IVP_Ibd_t Flag, uint32_t Threshold);

FVDP_Result_t IVP_FieldControl(FVDP_CH_t CH, uint32_t Val);

FVDP_Result_t IVP_VStartSetOffset (FVDP_CH_t CH, FVDP_VideoInfo_t* InputVideoInfo_p,  FVDP_VideoInfo_t*  OutputVideoInfo_p);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_IVP_H */

