/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_ifm.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/
#include <stddefs.h>
#include <stm_hdmirx_os.h>

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/
#include <hdmirx_ifm.h>
#include <hdmirx_utility.h>
#include <InternalTypes.h>
#include <hdmirx_ifm_regoffsets.h>

/* Private Typedef -----------------------------------------------*/

//#define ENABLE_HDRX_IFM_DBG
#ifdef ENABLE_HDRX_IFM_DBG
#define DBG_HDRX_IFM_INFO	HDMI_PRINT
#else
#define DBG_HDRX_IFM_INFO(a,b...)
#endif

#define ENABLE_AFR_AFM_CTRL

/* Private macro's ------------------------------------------------*/

#define     TCLK_FREQ_HZ    30000000UL	/* T Clock */

#define     H_SYNC_MIN		15000UL	/* measured in Hz */
#define     V_SYNC_MIN		20	/* measured in Hz */

#define     H_WATCHDOG		    (((TCLK_FREQ_HZ/H_SYNC_MIN) + (TCLK_FREQ_HZ/H_SYNC_MIN)/2)>>6)
#define     V_WATCHDOG		    ((((TCLK_FREQ_HZ/511)/V_SYNC_MIN) + ((TCLK_FREQ_HZ/511)/V_SYNC_MIN)/2)>>7)
#define     IFMWATCHDOG_VAL     ((V_WATCHDOG<<8)|H_WATCHDOG)

#define OVERLAP_HFREQ 312
#define OVERLAP_VFREQ 500
#define DELTA_HFREQ     15
#define DELTA_VFREQ     15

#define HSHIFT 4
/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

BOOL sthdmirx_IFM_waitVSync_time(sthdmirx_IFM_context_t *pIfmCtrl,
                                 U8 NoOfVsyncs);

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_instrument_initialize()
 USAGE        	:   Initialize the Instrumentation block in HDMIRx
 INPUT        	:   Instrument Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_instrument_initialize(sthdmirx_IFM_context_t *pIfmCtrl)
{
  /* Initialize the Instrumentation Block of HDMI */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_AFR_CONTROL), (HDMI_RX_AFR_DISP_IFM_ERR_EN |
                                 HDMI_RX_AFR_DISP_IBD_ERR_EN | HDMI_RX_AFR_DISP_CLK_ERR_ENN));

#ifdef ENABLE_AFR_AFM_CTRL
  /* Enable the Auto Force BackGround */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_AFB_CONTROL), (HDMI_RX_AFB_IFM_ERR_EN |
                                 HDMI_RX_AFB_IBD_ERR_EN | HDMI_RX_AFB_CLK_ERR_EN));
#endif

  /* disable all IFM related interrupts */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_IRQ_MASK), 0x00);
  /* Clear all pending status bits */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_IRQ_STATUS), 0xFFFF);

  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_IFM_CLK_CONTROL), 0x00000000);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INSTRUMENT_POLARITY_CONTROL), 0x00000000);
  /* Reset the IFM & IBD Block  after the system powerUp */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INST_SOFT_RESETS), 0x03);
  STHDMIRX_DELAY_1ms(1);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INST_SOFT_RESETS), 0x00);

  DBG_HDRX_IFM_INFO
  ("sthdmirx_IFM_instrument_initialize : Initialization done \n");

  return;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_init()
 USAGE        	:   Initialize the IFM module in HDMIRx
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_init(sthdmirx_IFM_context_t *pIfmCtrl)
{

  /* Enable HDMI IFM - HDMI_IFM_EN */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_IFM_CTRL),
                       (HDMI_IFM_EN | HDMI_IFM_MEASEN | HDMI_IFM_HOFFSETEN
                        | HDMI_IFM_INT_ODD_EN | HDMI_IFM_FIELD_DETECT_MODE));
  /* IFM Watchdog Configuration */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_IFM_WATCHDOG), IFMWATCHDOG_VAL);

  /* IFM H Line Configuration */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_IFM_HLINE), 0x30);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INST_HARD_RESETS), 0x00);

  /* IFM Input Timing Change detection thresholds */
  HDMI_WRITE_REG_DWORD((U32) ((U32) pIfmCtrl->ulBaseAddress +
                              HDMI_RX_IFM_CHANGE), 0x2D);	/*16 TCLKS */

  /* IFM Input Control Configuration - IMD - 0x05 Enables Input Capture and Interlaced input */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_CONTROL), 0x05);

  /* When input timing reaches to programmed line, input path is reseted & prepared for next field */
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_FRAME_RESET_LINE), 0x04);

  sthdmirx_IFM_IBD_config(pIfmCtrl, STHDRX_IBD_MEASURE_SOURCE,
                          STHDRX_IBD_MEASURE_DE);
  sthdmirx_IFM_IBD_config(pIfmCtrl, STHDRX_IBD_MEASURE_WINDOW, 0);
  sthdmirx_IFM_IBD_config(pIfmCtrl, STHDRX_IBD_ENABLE, 1);

  /* HDMI Instrumentation Block initialization */
  sthdmirx_IFM_instrument_initialize(pIfmCtrl);

  DBG_HDRX_IFM_INFO("IFM_IBD : Initialization done !!\n");

}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_signal_timing_get()
 USAGE        	:   Records the IBD Parameters
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
BOOL sthdmirx_IFM_signal_timing_get(sthdmirx_IFM_context_t *pIfmCtrl,
                                    sthdmirx_IFM_timing_params_t *pIfmParams)
{
  U32 ulHperiod, ulVperiod, ulHpulse, ulVpulse;

  pIfmParams->VPulse = 1;
  pIfmParams->HPulse = 1;
  pIfmParams->HFreq_Hz = 1;
  pIfmParams->VFreq_Hz = 1;
  pIfmParams->H_SyncPolarity = pIfmParams->V_SyncPolarity =
                                 STM_HDMIRX_SIGNAL_POLARITY_POSITIVE;

  ulHperiod = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                        HDMI_RX_IFM_HS_PERIOD)) & HDMI_RX_IFM_HS_PERIODBitFieldMask;
  ulVperiod = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                        HDMI_RX_IFM_VS_PERIOD)) & HDMI_RX_IFM_VS_PERIODBitFieldMask;
  ulHpulse =
    HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                              HDMI_RX_IFM_HS_PULSE)) & HDMI_RX_IFM_HS_PULSEBitFieldMask;
  ulVpulse = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                       HDMI_RX_IFM_VS_PULSE)) & HDMI_RX_IFM_VS_PULSEBitFieldMask;

  /* Check and determine H & V sync polarity based on pulse width */
  if (ulHpulse > (ulHperiod / 2))
    {
      pIfmParams->H_SyncPolarity = STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE;
      ulHpulse = ulHperiod - ulHpulse;
    }
  if (ulVpulse > (ulVperiod / 2))
    {
      pIfmParams->V_SyncPolarity = STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE;
      ulVpulse = ulVperiod - ulVpulse;
    }

  if (ulHperiod && ulVperiod)	//Check whether divider is zero
    {
      pIfmParams->HPeriod = (U16) ulHperiod;
      pIfmParams->VPeriod = (U16) ulVperiod;
      pIfmParams->HPulse = (U16) ulHpulse;
      pIfmParams->VPulse = (U16) ulVpulse;

      pIfmParams->HFreq_Hz = (U32) ((TCLK_FREQ_HZ / (ulHperiod)));
      pIfmParams->VFreq_Hz = (U16) ((U32) (TCLK_FREQ_HZ * 10) /
                                    (U32) (ulVperiod * ulHperiod));
      DBG_HDRX_IFM_INFO("IFM: HFeq:%d    ", pIfmParams->HFreq_Hz);
      DBG_HDRX_IFM_INFO("VFreq:%d Hz \n", (pIfmParams->VFreq_Hz / 10));
      DBG_HDRX_IFM_INFO("IFM: HPulse:%d  ", pIfmParams->HPulse);
      DBG_HDRX_IFM_INFO("HPerios:%d    ", pIfmParams->HPeriod);
      DBG_HDRX_IFM_INFO("Vpulse:%d", pIfmParams->VPulse);
      DBG_HDRX_IFM_INFO("VPeriod:%d  \n", pIfmParams->VPeriod);

      return TRUE;
    }

  return FALSE;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_active_window_set()
 USAGE        	:   Sets the Active Window
 INPUT        	:   IFM Parameter structure, VStart, HStart, VLength, HWidth
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_active_window_set(sthdmirx_IFM_context_t *pIfmCtrl,
                                    sthdmirx_timing_window_param_t *mWindow)
{
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_H_ACT_START), mWindow->Hstart);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_H_ACT_WIDTH), mWindow->Width);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_V_ACT_START_ODD), mWindow->Vstart);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_V_ACT_START_EVEN), mWindow->Vstart);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_V_ACT_LENGTH), mWindow->Length);
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_IBD_meas_window_set()
 USAGE        	:   Sets the Measurement Window for Input Boundary Detection
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_IBD_meas_window_set(sthdmirx_IFM_context_t *pIfmCtrl,
                                      sthdmirx_timing_window_param_t *mWindow)
{
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_HMEASURE_START), mWindow->Hstart);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_HMEASURE_WIDTH), mWindow->Width);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_VMEASURE_START), mWindow->Vstart);
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_VMEASURE_LENGTH), mWindow->Length);
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_IBD_config()
 USAGE        	:   Records the IBD Parameters
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_IBD_config(sthdmirx_IFM_context_t *pIfmCtrl,
                             sthdmirx_IBD_ctrl_option_t Option, U8 uValue)
{
  U32 uIbdControlWord;

  uIbdControlWord = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                        HDMI_RX_INPUT_IBD_CONTROL)) & 0x01ff;

  switch (Option)
    {
    case STHDRX_IBD_MEASURE_SOURCE:
    {
      if (uValue == STHDRX_IBD_MEASURE_DATA)
        CLEAR_BIT(uIbdControlWord, HDMI_RX_INPUT_MEASURE_DE_nDATA);
      else if (uValue == STHDRX_IBD_MEASURE_DE)
        SET_BIT(uIbdControlWord, HDMI_RX_INPUT_MEASURE_DE_nDATA);
    }
    break;

    case STHDRX_IBD_MEASURE_WINDOW:
    {
      if (uValue == 0)
        CLEAR_BIT(uIbdControlWord, HDMI_RX_INPUT_IBD_MEASWINDOW_EN);
      else
        SET_BIT(uIbdControlWord, HDMI_RX_INPUT_IBD_MEASWINDOW_EN);
    }
    break;

    case STHDRX_IBD_RGB_SELECT:
    {
      CLEAR_BIT(uIbdControlWord, HDMI_RX_INPUT_IBD_RGB_SEL_MASK);

      if (uValue == STHDRX_IBD_RED_FIELD_SEL)
        SET_BIT(uIbdControlWord, (U16) (0x1 << 2));
      else if (uValue == STHDRX_IBD_GREEN_FIELD_SEL)
        SET_BIT(uIbdControlWord, (U16) (0x2 << 2));
      else if (uValue == STHDRX_IBD_BLUE_FIELD_SEL)
        SET_BIT(uIbdControlWord, (U16) (0x3 << 2));
    }
    break;

    case STHDRX_IBD_THRESHOLD:
    {
      CLEAR_BIT(uIbdControlWord, HDMI_RX_INPUT_DET_THOLD_SEL_MASK);
      SET_BIT(uIbdControlWord,
              ((uValue & HDMI_RX_INPUT_DET_THOLD_SEL_MASK) << 4));
    }
    break;

    case STHDRX_IBD_ENABLE:
      if (uValue == 0)
        CLEAR_BIT(uIbdControlWord, HDMI_RX_INPUT_IBD_EN);
      else
        SET_BIT(uIbdControlWord, HDMI_RX_INPUT_IBD_EN);
      break;

    default:
      break;
    }

  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_IBD_CONTROL), uIbdControlWord);

}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_IBD_timing_get()
 USAGE        	:   Records the IBD Parameters
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
BOOL sthdmirx_IFM_IBD_timing_get(sthdmirx_IFM_context_t *pIfmCtrl,
                                 sthdmirx_sig_timing_info_t *pIbdParam)
{
  U16 vstart;

  pIbdParam->HTotal =
    (U16)
    HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                              HDMI_RX_INPUT_IBD_HTOTAL)) & HDMI_INPUT_IBD_HTOTALBitFieldMask;
  pIbdParam->VTotal = (U16)HDMI_READ_REG_DWORD((U32)
                      ((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_INPUT_IBD_VTOTAL)) &
                      HDMI_INPUT_IBD_VTOTALBitFieldMask;
  pIbdParam->Window.Hstart = (U16)(HDMI_READ_REG_DWORD((U32)
                             ((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_INPUT_IBD_HSTART)) &
                             HDMI_INPUT_IBD_HSTARTBitFieldMask)+HSHIFT;
  pIbdParam->Window.Width = (U16)HDMI_READ_REG_DWORD((U32)
                            ((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_INPUT_IBD_HWIDTH)) &
                            HDMI_INPUT_IBD_HWIDTHBitFieldMask;
  pIbdParam->Window.Vstart = (U16)HDMI_READ_REG_DWORD((U32)
                             ((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_INPUT_IBD_VSTART)) &
                             HDMI_INPUT_IBD_VSTARTBitFieldMask;
  pIbdParam->Window.Length = (U16)HDMI_READ_REG_DWORD((U32)
                             ((U32) pIfmCtrl->ulBaseAddress + HDMI_RX_INPUT_IBD_VLENGTH)) &
                             HDMI_INPUT_IBD_VLENGTHBitFieldMask;

  /*wait for one frame time */
  vstart = (U16)HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                          HDMI_RX_INPUT_IBD_VSTART)) & HDMI_INPUT_IBD_VSTARTBitFieldMask;

  if (vstart < pIbdParam->Window.Vstart)
    {
      /* use the minimum value of v start */
      pIbdParam->Window.Vstart = vstart;
    }

  if ((pIbdParam->HTotal == 0) || (pIbdParam->VTotal == 0))
    return FALSE;

  DBG_HDRX_IFM_INFO("IBD: Start   H =%d x %d \n",
                    pIbdParam->Window.Hstart);
  DBG_HDRX_IFM_INFO("IBD: Start   V =%d x %d \n",
                    pIbdParam->Window.Vstart);
  DBG_HDRX_IFM_INFO("IBD: Active  H =%d x %d \n",
                    pIbdParam->Window.Width);
  DBG_HDRX_IFM_INFO("IBD: Active  V =%d x %d \n",
                    pIbdParam->Window.Length);
  DBG_HDRX_IFM_INFO("IBD: Total   H =%d x %d \n", pIbdParam->HTotal);
  DBG_HDRX_IFM_INFO("IBD: Total   V =%d x %d \n", pIbdParam->VTotal);

  return TRUE;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_HSync_polarity_get()
 USAGE        	:   Returns the Polarity of the HSync
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
stm_hdmirx_signal_polarity_t
sthdmirx_IFM_HSync_polarity_get(sthdmirx_IFM_context_t *pIfmCtrl)
{
  U16 H_Pulse, H_Period;
  stm_hdmirx_signal_polarity_t SigPolarity =
    STM_HDMIRX_SIGNAL_POLARITY_POSITIVE;

  H_Pulse = (U16) (HDMI_READ_REG_DWORD((U32) (pIfmCtrl->ulBaseAddress +
                                       HDMI_RX_IFM_HS_PULSE)) & (HDMI_RX_IFM_HS_PULSEBitFieldMask));
  H_Period = (U16) (HDMI_READ_REG_DWORD((U32) (pIfmCtrl->ulBaseAddress +
                                        HDMI_RX_IFM_HS_PERIOD)) & (HDMI_RX_IFM_HS_PERIODBitFieldMask));
  if (H_Pulse > (H_Period / 2))
    {
      SigPolarity = STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE;
    }

  DBG_HDRX_IFM_INFO("IFM: HSync Polarity :%d \n", SigPolarity);
  return SigPolarity;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_VSync_polarity_get()
 USAGE        	:   Returns the Polarity of the VSync
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
stm_hdmirx_signal_polarity_t
sthdmirx_IFM_VSync_polarity_get(sthdmirx_IFM_context_t *pIfmCtrl)
{
  U16 V_Pulse, V_Period;
  stm_hdmirx_signal_polarity_t SigPolarity =
    STM_HDMIRX_SIGNAL_POLARITY_POSITIVE;

  V_Pulse = (U16) (HDMI_READ_REG_DWORD((U32) (pIfmCtrl->ulBaseAddress +
                                       HDMI_RX_IFM_VS_PULSE)) & (HDMI_RX_IFM_VS_PULSEBitFieldMask));
  V_Period = (U16) (HDMI_READ_REG_DWORD((U32) (pIfmCtrl->ulBaseAddress +
                                        HDMI_RX_IFM_VS_PERIOD)) & (HDMI_RX_IFM_VS_PERIODBitFieldMask));
  if (V_Pulse > (V_Period / 2))
    {
      SigPolarity = STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE;
    }

  DBG_HDRX_IFM_INFO("IFM: VSync Polarity :%d \n", SigPolarity);
  return SigPolarity;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_pixel_clk_freq_get()
 USAGE        	:   Calculate the pixel clock
 INPUT        	:   IFM Parameter structure
 RETURN       	:   Pixel Clock.
 USED_REGS	    :
******************************************************************************/
U32 sthdmirx_IFM_pixel_clk_freq_get(sthdmirx_IFM_context_t *pIfmCtrl)
{
  sthdmirx_IFM_timing_params_t IfmParam;
  U32 ulPixelClock = 0;
  U32 HTotal, VTotal;

  if (sthdmirx_IFM_signal_timing_get(pIfmCtrl, &IfmParam) == FALSE)
    {
      DBG_HDRX_IFM_INFO("IFM: Timing Info Parameters are not available\n");
      return ulPixelClock;
    }

  HTotal = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                     HDMI_RX_INPUT_IBD_HTOTAL)) & HDMI_INPUT_IBD_HTOTALBitFieldMask;
  VTotal = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                     HDMI_RX_INPUT_IBD_VTOTAL)) & HDMI_INPUT_IBD_VTOTALBitFieldMask;

  ulPixelClock = (U32) ((HTotal) * (VTotal) * (IfmParam.VFreq_Hz)) / 10;

  DBG_HDRX_IFM_INFO("IFM: Pixel Clock Freq : %d\n", ulPixelClock);

  return ulPixelClock;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_signal_scantype_get()
 USAGE        	:   Returns Signal Scan Type ( Progressive or interlace)
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
#define VTOTAL_DELTA_CHECK      5

stm_hdmirx_signal_scan_type_t
sthdmirx_IFM_signal_scantype_get(sthdmirx_IFM_context_t *pIfmCtrl)
{
  sthdmirx_IFM_timing_params_t pIfmParams;
  sthdmirx_IFM_signal_timing_get(pIfmCtrl, &pIfmParams);

//      sthdmirx_IFM_waitVSync_time(pIfmCtrl,3);

  if (sthdmirx_IFM_is_mode_overlapping
      ((U16) pIfmParams.HFreq_Hz / 100, pIfmParams.VFreq_Hz) == TRUE)
    {
      DBG_HDRX_IFM_INFO
      ("\n  Overlap modes found .......................... !!!!!!!! \n");
      if (HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                    HDMI_RX_INPUT_IRQ_STATUS)) & HDMI_RX_INPUT_INTLC_ERR_MASK)
        {
          return STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE;
        }
      return STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED;
    }
  else
    {

#if 1
      U32 VTotal;
      VTotal = HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                         HDMI_RX_INPUT_IBD_VTOTAL)) &
               HDMI_INPUT_IBD_VTOTALBitFieldMask;
      if ((abs(VTotal - 262) <= VTOTAL_DELTA_CHECK) ||
          (abs(VTotal - 312) <= VTOTAL_DELTA_CHECK) ||
          (abs(VTotal - 562) <= VTOTAL_DELTA_CHECK))
        {
          return STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED;
        }
      return STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE;
#else
      if (HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                    HDMI_RX_INPUT_IRQ_STATUS)) & HDMI_RX_INPUT_INTLC_ERR_MASK)
        {
          return STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE;
        }
      return STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED;

#endif
    }
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_waitVSync_time()
 USAGE        	:   Wait for requested VSync Time.
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
BOOL sthdmirx_IFM_waitVSync_time(sthdmirx_IFM_context_t *pIfmCtrl,
                                 U8 NoOfVsyncs)
{
  U32 DW_BeginTime;

  if (NoOfVsyncs == 0)
    {
      NoOfVsyncs++;
    }

  while (NoOfVsyncs--)
    {
      DW_BeginTime = stm_hdmirx_time_now();

      HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                 HDMI_RX_INPUT_IRQ_STATUS), HDMI_RX_INPUT_VS_MASK);
      while (!(HDMI_READ_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                         HDMI_RX_INPUT_IRQ_STATUS)) & HDMI_RX_INPUT_VS_MASK))
        {
          if ((stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                     DW_BeginTime)) >= (M_NUM_TICKS_PER_MSEC(50)))
            {
              return FALSE;
            }
        }
    }
  return TRUE;

}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_get_AFRsignal_detect_status()
 USAGE        	:   return the AFR signal detect status, if AFR is triggered
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
BOOL sthdmirx_IFM_get_AFRsignal_detect_status(sthdmirx_IFM_context_t *pIfmCtrl)
{
  if (HDMI_READ_REG_DWORD((U32) ((U32) pIfmCtrl->ulBaseAddress +
                                 HDMI_RX_INPUT_IRQ_STATUS)) & HDMI_RX_INPUT_AFR_DETECT)
    {
      return TRUE;
    }
  return FALSE;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_clear_AFRsignal_detect_status()
 USAGE        	:   Clear the Auto Free Run Detect status
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_clear_AFRsignal_detect_status(
  sthdmirx_IFM_context_t *pIfmCtrl)
{
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_IRQ_STATUS), HDMI_RX_INPUT_AFR_DETECT);
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_clear_interlace_decode_error_status()
 USAGE        	:   Clear the Auto Free Run Detect status
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_clear_interlace_decode_error_status(
  sthdmirx_IFM_context_t *pIfmCtrl)
{
  HDMI_WRITE_REG_DWORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                             HDMI_RX_INPUT_IRQ_STATUS), HDMI_RX_INPUT_INTLC_ERR);
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_set_australianmode_interlace_detect(sthdmirx_IFM_context_t *pIfmCtrl)
 USAGE        	:   Australian mode detection
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_set_australianmode_interlace_detect(
  sthdmirx_IFM_context_t *pIfmCtrl)
{
  // Use the raw measurement, Re-Generated
  HDMI_CLEAR_REG_BITS_WORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                 HDMI_RX_IFM_CTRL), HDMI_IFM_FIELD_DETECT_MODE);
  HDMI_SET_REG_BITS_WORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                               HDMI_RX_IFM_CTRL), HDMI_IFM_ODD_INV);
  //This bit programming is added as the Australian mode LH was not getting detected .
  HDMI_WRITE_REG_DWORD(((U32) pIfmCtrl->ulBaseAddress +
                        HDMI_RX_IFM_CHANGE), 0x5267);

  return;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_reset_australianmode_interlace_detect(sthdmirx_IFM_context_t *pIfmCtrl)
 USAGE        	:   Reset australian mode detection
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
void sthdmirx_IFM_reset_australianmode_interlace_detect(
  sthdmirx_IFM_context_t *pIfmCtrl)
{
  // Use the raw measurement, Re-Generated
  HDMI_SET_REG_BITS_WORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                               HDMI_RX_IFM_CTRL), HDMI_IFM_FIELD_DETECT_MODE);
  HDMI_CLEAR_REG_BITS_WORD((U32)((U32) pIfmCtrl->ulBaseAddress +
                                 HDMI_RX_IFM_CTRL), HDMI_IFM_ODD_INV);
  //This bit programming is added as the Australian mode LH was not getting detected .
  HDMI_WRITE_REG_DWORD(((U32) pIfmCtrl->ulBaseAddress +
                        HDMI_RX_IFM_CHANGE), 0x67);

  return;
}

/******************************************************************************
 FUNCTION       :   sthdmirx_IFM_is_mode_overlapping()
 USAGE        	:   Set window mode detection
 INPUT        	:   IFM Parameter structure
 RETURN       	:
 USED_REGS	    :
******************************************************************************/
BOOL sthdmirx_IFM_is_mode_overlapping(U16 Hfreq, U16 Vfreq)
{
  /* Check is the mode is overlapping (576p , 1080i Aus modes) */
  if (((Hfreq >= (OVERLAP_HFREQ - DELTA_HFREQ)) &&
       (Hfreq <= (OVERLAP_HFREQ + DELTA_HFREQ))) &&
      ((Vfreq >= (OVERLAP_VFREQ - DELTA_VFREQ)) &&
       (Vfreq <= (OVERLAP_VFREQ + DELTA_VFREQ))))
    {
      DBG_HDRX_IFM_INFO("\n\n Overlap True \n");
      return (TRUE);
    }
  else
    {
      DBG_HDRX_IFM_INFO("\n\n Hfreq %d", Hfreq);
      DBG_HDRX_IFM_INFO("   Vfreq %d\n", Vfreq);
      DBG_HDRX_IFM_INFO("\n\n Overlap FALSE \n");
      return (FALSE);
    }

}

/* End of file */
