/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_meas.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Standard Includes ----------------------------------------------*/

/* Include of Other module interface headers --------------------------*/

/* Local Includes -------------------------------------------------*/

#include <hdmirx_drv.h>
#include <hdmirx_core.h>
#include <hdmirx_core_export.h>
#include <InternalTypes.h>
#include <hdmirx_utility.h>
#include <hdmirx_RegOffsets.h>

/* Private Typedef -----------------------------------------------*/
typedef struct
{
  U16 VTotal;		/* V total in lines */
  U8 DeltaVTotal;		/* V total tolerance in line unit */
  U16 HFreq100Hz;		/* HFreq in 100Hz */
  U8 DeltaHFreq100Hz;	/* HFreq tolerance in 100Hz */
  U8 VFreqHz;		/* VFreq in Hz */
  U8 DeltaVFreqHz;	/* VFreq tolerance in Hz */
} VideoTimingMode_t;

/* Private Defines ------------------------------------------------*/

//#define ENABLE_MEAS_DBG
#ifdef ENABLE_MEAS_DBG
#define DBG_MEAS_INFO	HDMI_PRINT
#else
#define DBG_MEAS_INFO(a,b...)
#endif

#define     DVI_DC_HT_MIN               1716
#define     DVI_DC_HT_MAX               1728
#define     DVI_DC_MIDDLE_HTOTAL        ((DVI_DC_HT_MAX-DVI_DC_HT_MIN)/2 + DVI_DC_HT_MIN)
#define     DVI_DC_HTOTAL_DELTA         (((DVI_DC_HT_MAX+10) - (DVI_DC_HT_MIN-10))/2)

#define     HDMI_MEAS_VSYNC_TIMEOUT                     50	/*50msec */

#define     HDRX_MEAS_LINK_CLOCK_THRESHOLD_KHZ          26

#define     DVI_HTOTAL_THRESHOLD        4
#define     DVI_HACTIVE_THRESHOLD       DVI_HTOTAL_THRESHOLD
#define     DVI_VTOTAL_THRESHOLD        4
#define     DVI_VACTIVE_THRESHOLD       DVI_VTOTAL_THRESHOLD
#define     DVI_MAX_ERROR_COUNT         3

#define     DVI_MEASUREMENTS_PERIOD             189 /*us reg val = 0x13EF, was 228, 0x180c */
#define     DVI_CLOCK_THRESH                    22  /*MHz */
#define     DVI_NUMBER_MEASUREMENTS             1

#define     VSYNC_ATTEMPT                       3
#define     H_DELTA                             2
#define     V_DELTA                             2
#define     VTOTAL_THRESH                       100
#define     HTOTAL_THRESH                       100
/* Private macro's ------------------------------------------------*/

/*#define HDRX_MEAS_STS_MASK       (gmd_STS_HTOTAL_MEAS | gmd_STS_VTOTAL_MEAS)*/

#define HDRX_MEAS_CLEAR_MASK   HDRX_MEAS_RD_MD	//0x70

#define HDRX_MEAS_CLOCKS_EN    ((0x00 << HDRX_MEAS_RD_MD_SHIFT) & HDRX_MEAS_RD_MD)	//0x00

#define HDRX_MEAS_VTOTAL_EN    ((0x04 << HDRX_MEAS_RD_MD_SHIFT) & HDRX_MEAS_RD_MD)	//0x40

#define HDRX_MEAS_VACTIVE_EN   ((0x05 << HDRX_MEAS_RD_MD_SHIFT) & HDRX_MEAS_RD_MD)	//0x50

#define HDRX_MEAS_HTOTAL_EN    ((0x02 << HDRX_MEAS_RD_MD_SHIFT) & HDRX_MEAS_RD_MD)	//0x20

#define HDRX_MEAS_HACTIVE_EN   ((0x03 << HDRX_MEAS_RD_MD_SHIFT) & HDRX_MEAS_RD_MD)	//0x30

/* Private Variables -----------------------------------------------*/

VideoTimingMode_t const gSa_DoubleClkModeTable[] =
{
  /*CEA-861 Mode # */
  {625, 10, 156, 4, 50, 2},	/* 21, 22 */
  {312, 10, 156, 4, 50, 2},	/* 23, 24 */
  {525, 10, 157, 4, 59, 2},	/* 6, 7 */
  {262, 10, 157, 4, 59, 2},	/* 8, 9 */
  {625, 10, 312, 4, 100, 2},	/* 44, 45 */
  {525, 10, 314, 4, 120, 2},	/* 50, 51 */
  {625, 10, 625, 4, 200, 2},	/* 54, 55 */
  {525, 10, 629, 4, 239, 2},	/* 58, 59 */
};

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/
/******************************************************************************
 FUNCTION     :   STHDMIRXMEAS_MedianFilter
 USAGE        :   Median filter
 INPUT        :   Wp_Data - pointer to array of input data.
                  B_Size - the size of array data (shall be an odd number)
 RETURN       :   Sorted in icreasing order Wp_Data
 USED_REGS    :   None
******************************************************************************/
void sthdmirx_meas_median_filter(U16 *Wp_Data, U8 B_Size)
{
  U16 xValue;
  U8 i, j;

  /*Go through whole array */
  for (i = 1; i < B_Size; i++)
    {
      for (j = 0; j < B_Size - i; j++)
        {
          if (Wp_Data[j] > Wp_Data[j + 1])
            {
              xValue = Wp_Data[j];
              Wp_Data[j] = Wp_Data[j + 1];
              Wp_Data[j + 1] = xValue;
            }
        }
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_reset_measurement
 USAGE        :     Retrigger the measurement cycles
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_reset_measurement(sthdmirx_signal_meas_ctrl_t *pMeasCtrl)
{
  /* set to zero.   */
  pMeasCtrl->IsLinkClkAvailable = FALSE;
  pMeasCtrl->IsLinkClkStable = FALSE;
  pMeasCtrl->IsPLLSetupDone = FALSE;
  pMeasCtrl->LinkClockstatus = FALSE;
  pMeasCtrl->LinkClkMeasCount = 0;
  pMeasCtrl->mStatus = 0x0;	/* Need to check is it right place to clear all status */
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_HW_init
 USAGE        :     Hardware Initialization for Instrumentation block for measurement
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_HW_init(const hdmirx_handle_t Handle, U32 RefClkFreq)
{

  hdmirx_route_handle_t *pInpHandle;
  pInpHandle = (hdmirx_route_handle_t *) Handle;

  /*Instrumentation - Measrement blk is programmed to initial values */

  /*Enable the Signal & Link clock detection & measurement logic */
  HDMI_WRITE_REG_BYTE((U32)(pInpHandle->BaseAddress + HDRX_SIGNL_DET_CTRL),
                      (HDRX_SGNL_MEAS_EN | HDRX_LNK_BY_CLK_DET_EN));

  /*New link clock measurement ckt (0) & Media Filter during the mode change(1) */
  HDMI_CLEAR_REG_BITS_BYTE((U32)(pInpHandle->BaseAddress +
                                 HDRX_SIGNL_DET_CTRL), HDRX_CLK_CHNG_FILT_EN);

  /*Enable Audio and Video mute generation on link clock change */
  HDMI_SET_REG_BITS_WORD((U32)(pInpHandle->BaseAddress + HDRX_SIGNL_DET_CTRL),
                                 HDRX_AV_MUTE_ON_CLK_CHNG_EN);

  /* About 76 us for 45MHz */
  HDMI_WRITE_REG_WORD((U32)(pInpHandle->BaseAddress + HDRX_FREQ_MEAS_PERIOD),
                      DVI_MEASUREMENTS_PERIOD * (U16) ((U32) (RefClkFreq) / 1000000UL));

  /* No signal if frequency below 22 MHz */
  HDMI_WRITE_REG_WORD((U32) (pInpHandle->BaseAddress + HDRX_FREQ_TH),
                      (DVI_MEASUREMENTS_PERIOD * DVI_CLOCK_THRESH));

  /* No signal if clock below 32 times */
  HDMI_WRITE_REG_BYTE((U32) (pInpHandle->BaseAddress + HDRX_FREQ_MEAS_CT),
                      DVI_NUMBER_MEASUREMENTS);

  /*DVI_CLK_STBL_ZN : The difference threshold beyond which the input clock is considered to be changing
     Default value is 0xff, so need to program, if we really need then we can change here */

  /* Interrupt initialization HDRX_MEAS_IRQ_EN */
#if 0
  /*just make sw ready to test the Irq */
  HDMI_CLEAR_AND_SET_REG_BITS_WORD(HDRX_MEAS_IRQ_EN, (HDRX_MEAS_COUNT_IRQ_EN |
                                   HDRX_MEAS_DUR_IRQ_EN | HDRX_MEAS_CLK_IRQ_EN |
                                   HDRX_CLK_CHNG_IRQ_EN), (HDRX_CLK_LOST_IRQ_EN))
#endif
  /*Software variables are reset to default values */
  sthdmirx_MEAS_reset_measurement(&pInpHandle->sMeasCtrl);
  DBG_MEAS_INFO("Measurement Block Initialization Done!\n");

}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_linkclk_meas
 USAGE        :     Measure the Link clock frequency
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_linkclk_meas(const hdmirx_handle_t Handle)
{
  U32 DW_LinkClockMin, DW_LinkClockMax;

  hdmirx_route_handle_t *Handle_p;
  Handle_p = (hdmirx_route_handle_t *) Handle;

  if (HDMI_READ_REG_WORD((U32) (Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS)) &
      HDRX_MEAS_CLK_IRQ_STS)
    {
      /*ignore the first clock measurement data after the blk reset. so just discarded. */
      if (!(Handle_p->sMeasCtrl.mStatus & CLEAR_PREV_CLOCK_MEAS_DATA))
        {
          Handle_p->sMeasCtrl.mStatus |= CLEAR_PREV_CLOCK_MEAS_DATA;
          HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress +
                                    HDRX_MEAS_IRQ_STS), HDRX_MEAS_CLK_IRQ_STS);
          DBG_MEAS_INFO("Ignore the first clock measurement\n");
          return;
        }
      /* Get & Update the Clock measure data */
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)(Handle_p->BaseAddress +
                                             HDRX_SIGNL_DET_CTRL), HDRX_MEAS_CLEAR_MASK,
                                       HDRX_MEAS_CLOCKS_EN);

      Handle_p->sMeasCtrl.ClkMeasData[Handle_p->sMeasCtrl.LinkClkMeasCount] =
        HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_RESULT));
      /*DBG_MEAS_INFO("ClkMeasData[%d]:%d\n",Handle_p->sMeasCtrl.LinkClkMeasCount,Handle_p->sMeasCtrl.ClkMeasData[Handle_p->sMeasCtrl.LinkClkMeasCount]);*/
      HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS),
                          HDRX_MEAS_CLK_IRQ_STS);

      Handle_p->sMeasCtrl.LinkClkMeasCount =
        (Handle_p->sMeasCtrl.LinkClkMeasCount + 1) % COUNT_MEAS;

      if ((0 == Handle_p->sMeasCtrl.LinkClkMeasCount)
          && (FALSE == Handle_p->sMeasCtrl.IsLinkClkAvailable))
        {
          Handle_p->sMeasCtrl.IsLinkClkAvailable = TRUE;
        }

      if (Handle_p->sMeasCtrl.IsLinkClkAvailable)
        {
          U16 WA_MeasDataSorted[COUNT_MEAS];

          HDMI_MEM_CPY(&WA_MeasDataSorted, &Handle_p->sMeasCtrl.ClkMeasData,
                       (COUNT_MEAS * sizeof(U16)));
          sthdmirx_meas_median_filter(WA_MeasDataSorted, COUNT_MEAS);

          DW_LinkClockMin = ((U32) WA_MeasDataSorted[COUNT_MEAS / 2 - 1] *
                             1000UL) / (U32) DVI_MEASUREMENTS_PERIOD;
          DW_LinkClockMax = ((U32) WA_MeasDataSorted[COUNT_MEAS / 2 + 1] *
                             1000UL) / (U32) DVI_MEASUREMENTS_PERIOD;
          Handle_p->sMeasCtrl.CurrentTimingInfo.LinkClockKHz =
            ((U32) WA_MeasDataSorted[COUNT_MEAS / 2] * 1000UL)
            / (U32) DVI_MEASUREMENTS_PERIOD;

          /* Clock Meas Data is available */
          Handle_p->sMeasCtrl.mStatus |= SIG_STS_CLOCK_MEAS_DATA_AVBL;
          /*DBG_MEAS_INFO("link clock measurement :%d Khz\n",Handle_p->sMeasCtrl.CurrentTimingInfo.LinkClockKHz); */

          if (abs((S16)(DW_LinkClockMin -
                        Handle_p->sMeasCtrl.CurrentTimingInfo.LinkClockKHz)) >=
              HDRX_MEAS_LINK_CLOCK_THRESHOLD_KHZ || abs((S16)(DW_LinkClockMax -
                  Handle_p->sMeasCtrl.CurrentTimingInfo.LinkClockKHz)) >=
              HDRX_MEAS_LINK_CLOCK_THRESHOLD_KHZ)
            {
              Handle_p->sMeasCtrl.IsLinkClkStable = FALSE;
            }
          else
            {
              Handle_p->sMeasCtrl.IsLinkClkStable = TRUE;
            }
        }
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_horizontal_timing_meas
 USAGE        :     Measure the Horizontal timings.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_horizontal_timing_meas(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *Handle_p;
  Handle_p = (hdmirx_route_handle_t *) Handle;

  if (HDMI_READ_REG_WORD((U32) (Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS)) &
      HDRX_MEAS_DUR_IRQ_STS)
    {
      /*ignore the first clock measurement data after the blk reset. so just discarded. */
      if (!(Handle_p->sMeasCtrl.mStatus & CLEAR_PREV_HTIMING_MEAS_DATA))
        {
          Handle_p->sMeasCtrl.mStatus |= CLEAR_PREV_HTIMING_MEAS_DATA;
          HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress +
                                    HDRX_MEAS_IRQ_STS), HDRX_MEAS_DUR_IRQ_STS);
          /*Measuremens avalible. Restart timer */
          Handle_p->sMeasCtrl.MeasTimer = stm_hdmirx_time_now();
          DBG_MEAS_INFO("Ignore the first H measurement\n");
          return;
        }

      /* Update the Horizontal Total Timing */
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)(Handle_p->BaseAddress +
                                             HDRX_SIGNL_DET_CTRL), HDRX_MEAS_CLEAR_MASK,
                                       HDRX_MEAS_HTOTAL_EN);
      //HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress+HDRX_SIGNL_DET_CTRL), 0x27);

      Handle_p->sMeasCtrl.CurrentTimingInfo.HTotal =
        HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress +
                                 HDRX_MEAS_RESULT));

      /* Update the Horizontal Active Timing */
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)(Handle_p->BaseAddress +
                                             HDRX_SIGNL_DET_CTRL), HDRX_MEAS_CLEAR_MASK,
                                       HDRX_MEAS_HACTIVE_EN);
      //HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress+HDRX_SIGNL_DET_CTRL), 0x37);

      Handle_p->sMeasCtrl.CurrentTimingInfo.HActive =
        HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_RESULT));

      /*Clear the Measurement interrupt status */
      HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS),
                          HDRX_MEAS_DUR_IRQ_STS);
      /*DBG_MEAS_INFO("HTotal :%d\n",Handle_p->sMeasCtrl.CurrentTimingInfo.HTotal); */

      /*Set the status, Horizontal Timing Data is available */
      Handle_p->sMeasCtrl.mStatus |= SIG_STS_HTIMING_MEAS_DATA_AVBL;

    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_vertical_timing_meas
 USAGE        :     Measure the vertical timings.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_vertical_timing_meas(const hdmirx_handle_t Handle)
{
  hdmirx_route_handle_t *Handle_p;
  Handle_p = (hdmirx_route_handle_t *) Handle;
  if (HDMI_READ_REG_WORD(Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS) &
      HDRX_MEAS_COUNT_IRQ_STS)
    {

      /*ignore the first clock measurement data after the blk reset. so just discarded. */
      if (!(Handle_p->sMeasCtrl.mStatus & CLEAR_PREV_VTIMING_MEAS_DATA))
        {
          Handle_p->sMeasCtrl.mStatus |= CLEAR_PREV_VTIMING_MEAS_DATA;
          HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS),
                              HDRX_MEAS_COUNT_IRQ_STS);

          /*Measuremens avalible. Restart timer */
          Handle_p->sMeasCtrl.MeasTimer = stm_hdmirx_time_now();
          DBG_MEAS_INFO("Ignore the first V measurement\n");
          return;
        }

      /* Update the Vertical Total Timing */
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)(Handle_p->BaseAddress +
                                             HDRX_SIGNL_DET_CTRL), HDRX_MEAS_CLEAR_MASK,
                                       HDRX_MEAS_VTOTAL_EN);
      //HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress+HDRX_SIGNL_DET_CTRL), 0x47);

      Handle_p->sMeasCtrl.CurrentTimingInfo.VTotal =
        HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_RESULT));
      /* Update the Vertical Active Timing */
      HDMI_CLEAR_AND_SET_REG_BITS_BYTE((U32)(Handle_p->BaseAddress +
                                             HDRX_SIGNL_DET_CTRL), HDRX_MEAS_CLEAR_MASK,
                                       HDRX_MEAS_VACTIVE_EN);
      //HDMI_WRITE_REG_BYTE((U32)(Handle_p->BaseAddress+HDRX_SIGNL_DET_CTRL), 0x57);
      Handle_p->sMeasCtrl.CurrentTimingInfo.VActive =
        HDMI_READ_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_RESULT));

      /* Clear the Count measuremtn Interrupt status */
      HDMI_WRITE_REG_WORD((U32)(Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS),
                          HDRX_MEAS_COUNT_IRQ_STS);

      /*DBG_MEAS_INFO("VTotal :%d\n",Handle_p->sMeasCtrl.CurrentTimingInfo.VTotal); */

      /* Set the measurement data available flag */
      Handle_p->sMeasCtrl.mStatus |= SIG_STS_VTIMING_MEAS_DATA_AVBL;

    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_check_double_clk_mode
 USAGE        :     Checks the double clock mode
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_check_double_clk_mode(
  sthdmirx_signal_meas_ctrl_t *const pMeasCtrl)
{
  U8 B_Count, B_VFreqHz;

  U16 W_HTotal, W_VTotal, W_HFreq100Hz;

  W_HTotal = pMeasCtrl->CurrentTimingInfo.HTotal;
  W_VTotal = pMeasCtrl->CurrentTimingInfo.VTotal;

  if (abs(W_HTotal - DVI_DC_MIDDLE_HTOTAL) > DVI_DC_HTOTAL_DELTA)
    {
      pMeasCtrl->mStatus &= (~SIG_STS_DOUBLE_CLK_MODE_PRESENT);
      return;
    }

  W_HFreq100Hz =
    (U16)((pMeasCtrl->CurrentTimingInfo.LinkClockKHz * 10)/((U32)W_HTotal));
  B_VFreqHz = (U8) ((U32) W_HFreq100Hz * 100 / W_VTotal);

  for (B_Count = sizeof(gSa_DoubleClkModeTable) / sizeof(VideoTimingMode_t);
       B_Count; B_Count--)
    {
      if (abs(gSa_DoubleClkModeTable[B_Count - 1].VTotal - W_VTotal) >
          gSa_DoubleClkModeTable[B_Count - 1].DeltaVTotal)
        continue;

      if (abs(gSa_DoubleClkModeTable[B_Count - 1].HFreq100Hz - W_HFreq100Hz) >
          gSa_DoubleClkModeTable[B_Count - 1].DeltaHFreq100Hz)
        continue;

      if (abs(gSa_DoubleClkModeTable[B_Count - 1].VFreqHz - B_VFreqHz)
          > gSa_DoubleClkModeTable[B_Count - 1].DeltaVFreqHz)
        continue;

      pMeasCtrl->mStatus |= SIG_STS_DOUBLE_CLK_MODE_PRESENT;
      return;
    }

  pMeasCtrl->mStatus &= (~SIG_STS_DOUBLE_CLK_MODE_PRESENT);
  return;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_initialize_stabality_check_mode
 USAGE        :     Initialise the structure to monitors the input stability.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_initialize_stabality_check_mode(
  sthdmirx_signal_meas_ctrl_t *const pMeasCtrl)
{
  HDMI_MEM_CPY(&pMeasCtrl->SigStabilityCtrl.StableTimingInfo,
               &pMeasCtrl->CurrentTimingInfo, sizeof(signal_timing_info_t));
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_monitor_input_signal_stability
 USAGE        :     Monitors the input clock stabilities
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_MEAS_monitor_input_signal_stability(
  sthdmirx_signal_meas_ctrl_t *const pMeasCtrl)
{

  if ((abs(pMeasCtrl->SigStabilityCtrl.StableTimingInfo.HTotal -
           pMeasCtrl->CurrentTimingInfo.HTotal) >= DVI_HTOTAL_THRESHOLD) ||
      (abs(pMeasCtrl->SigStabilityCtrl.StableTimingInfo.HActive -
           pMeasCtrl->CurrentTimingInfo.HActive) >= DVI_HACTIVE_THRESHOLD))
    {
      pMeasCtrl->SigStabilityCtrl.HErrorCount++;
    }

  if ((abs(pMeasCtrl->SigStabilityCtrl.StableTimingInfo.VTotal -
           pMeasCtrl->CurrentTimingInfo.VTotal) >= DVI_VTOTAL_THRESHOLD) ||
      (abs(pMeasCtrl->SigStabilityCtrl.StableTimingInfo.VActive -
           pMeasCtrl->CurrentTimingInfo.VActive) >= DVI_VACTIVE_THRESHOLD))
    {
      pMeasCtrl->SigStabilityCtrl.VErrorCount++;
    }

  pMeasCtrl->SigStabilityCtrl.ErrorCountIdx++;

  if ((pMeasCtrl->SigStabilityCtrl.HErrorCount >= DVI_MAX_ERROR_COUNT) ||
      (pMeasCtrl->SigStabilityCtrl.VErrorCount >= DVI_MAX_ERROR_COUNT))
    {
      pMeasCtrl->SigStabilityCtrl.HErrorCount = 0;
      pMeasCtrl->SigStabilityCtrl.VErrorCount = 0;
      pMeasCtrl->SigStabilityCtrl.ErrorCountIdx = 0;

      /*make a concept to declare the unstable mode    */
      pMeasCtrl->mStatus |= SIG_STS_UNSTABLE_TIMING_PRESENT;

      /* Print the stable timing vs unstable timing */
      DBG_MEAS_INFO("****** Mode instability detected ******\n");
      DBG_MEAS_INFO("Previous   H Total : %d\n",
                    pMeasCtrl->SigStabilityCtrl.StableTimingInfo.
                    HTotal);
      DBG_MEAS_INFO("Current    H Total : %d\n",
                    pMeasCtrl->CurrentTimingInfo.HTotal);
      DBG_MEAS_INFO("Previous   V Total : %d\n",
                    pMeasCtrl->SigStabilityCtrl.StableTimingInfo.
                    VTotal);
      DBG_MEAS_INFO("Current    V Total : %d\n",
                    pMeasCtrl->CurrentTimingInfo.VTotal);
      DBG_MEAS_INFO("Previous   H Active: %d\n",
                    pMeasCtrl->SigStabilityCtrl.StableTimingInfo.
                    HActive);
      DBG_MEAS_INFO("Current    H Active: %d\n",
                    pMeasCtrl->CurrentTimingInfo.HActive);
      DBG_MEAS_INFO("Previous   V Active: %d\n",
                    pMeasCtrl->SigStabilityCtrl.StableTimingInfo.
                    VActive);
      DBG_MEAS_INFO("Current    V Active: %d\n",
                    pMeasCtrl->CurrentTimingInfo.VActive);

    }
  else if (pMeasCtrl->SigStabilityCtrl.ErrorCountIdx == COUNT_MEAS)
    {
      pMeasCtrl->SigStabilityCtrl.HErrorCount = 0;
      pMeasCtrl->SigStabilityCtrl.VErrorCount = 0;
      pMeasCtrl->SigStabilityCtrl.ErrorCountIdx = 0;

      /*make a concept to declare the stable mode    */
      pMeasCtrl->mStatus &= (~SIG_STS_UNSTABLE_TIMING_PRESENT);
    }

  /* check the mem cpy command */
  HDMI_MEM_CPY(&pMeasCtrl->SigStabilityCtrl.StableTimingInfo,
               &pMeasCtrl->CurrentTimingInfo, sizeof(signal_timing_info_t));
}

/******************************************************************************
 FUNCTION     :     sthdmirx_MEAS_is_linkclk_present
 USAGE        :     Link Clock Presence status
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_MEAS_is_linkclk_present(const hdmirx_handle_t Handle)
{
  BOOL uLinkClkstatus = FALSE;
  if (HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(Handle) +
                         HDRX_MAIN_LINK_STATUS) & HDRX_LINK_CLK_DETECTED)
    {
      uLinkClkstatus = TRUE;
    }
  return uLinkClkstatus;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_MEAS_monitor_HVtiming_statbility
 USAGE        :   Adaptive Equalizer algorithm to reconstruct the Hdmi Input signal.
 INPUT        :
 RETURN       :   stable, HTiming Unstable, VTiming Unstable, Link Clock Loss
 USED_REGS    :
******************************************************************************/
sthdmirx_HV_timing_meas_error_t sthdmirx_MEAS_monitor_HVtiming_statbility(const
    hdmirx_handle_t Handle)
{
  U8 i;
  BOOL FirstTimeHMeasDone = FALSE;
  BOOL FirstTimeVMeasDone = FALSE;
  signal_timing_info_t stInitialTmgData = { 0 };
  U32 ulBeginTime;

  hdmirx_route_handle_t *Handle_p;
  Handle_p = (hdmirx_route_handle_t *) Handle;

  /*Clear vertical/Horizonatl timing measurements in hardware */
  HDMI_WRITE_REG_WORD((Handle_p->BaseAddress + HDRX_MEAS_IRQ_STS),
                      (HDRX_MEAS_COUNT_IRQ_STS | HDRX_MEAS_DUR_IRQ_STS));

  /*Clear Software H Meas Data */
  CLEAR_BIT(Handle_p->sMeasCtrl.mStatus, SIG_STS_HTIMING_MEAS_DATA_AVBL);

  for (i = 0; i < VSYNC_ATTEMPT; i++)
    {
      /*Clear Software V Meas Data */
      CLEAR_BIT(Handle_p->sMeasCtrl.mStatus, SIG_STS_VTIMING_MEAS_DATA_AVBL);

      ulBeginTime = stm_hdmirx_time_now();

      DBG_MEAS_INFO("V sync Attempt = %d ", i);
      DBG_MEAS_INFO("time:0x%x\n", ulBeginTime);

      //Wait for V measurement completes
      do
        {
          /* Check the link clock if link clock disappers, abort the HV Measurement */
          if (FALSE == sthdmirx_MEAS_is_linkclk_present(Handle))
            {
              HDMI_PRINT("Link clock lost during equalization\n");
              DBG_MEAS_INFO("Fail. Time execution = %dms\n",
                            (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                   ulBeginTime) / (TICKS_PER_MSEC)));
              return MEAS_LINK_CLOCK_LOST;
            }

          sthdmirx_MEAS_horizontal_timing_meas(Handle);
          sthdmirx_MEAS_vertical_timing_meas(Handle);

          /* Check the Horizontal Timing information */
          if (CHECK_BIT(Handle_p->sMeasCtrl.mStatus,
                        SIG_STS_HTIMING_MEAS_DATA_AVBL))
            {
              CLEAR_BIT(Handle_p->sMeasCtrl.mStatus,
                        SIG_STS_HTIMING_MEAS_DATA_AVBL);
              if (FirstTimeHMeasDone == FALSE)
                {
                  FirstTimeHMeasDone = TRUE;
                  stInitialTmgData.HTotal =
                    Handle_p->sMeasCtrl.CurrentTimingInfo.HTotal;
                  stInitialTmgData.HActive =
                    Handle_p->sMeasCtrl.CurrentTimingInfo.HActive;

                  if ((stInitialTmgData.HTotal <
                       HTOTAL_THRESH) || (stInitialTmgData.HActive <
                                          HTOTAL_THRESH))
                    {
                      HDMI_PRINT("Abnormal small H Total or H Active\n");
                      DBG_MEAS_INFO("HTotal = %d  ",
                                    stInitialTmgData.HTotal);
                      DBG_MEAS_INFO("Hactive = %d\n",
                                    stInitialTmgData.HActive);
                      return MEAS_HORIZONTAL_TIMING_UNSTABLE;
                    }
                }
              else if (abs(stInitialTmgData.HTotal -
                           Handle_p->sMeasCtrl.CurrentTimingInfo.HTotal) > H_DELTA)
                {
                  HDMI_PRINT("Unstable H Total\n");
                  DBG_MEAS_INFO("HTotal = %d  ",stInitialTmgData.HTotal);
                  DBG_MEAS_INFO("NewHTotal = %d\n",
                                Handle_p->sMeasCtrl.CurrentTimingInfo.HTotal);
                  return MEAS_HORIZONTAL_TIMING_UNSTABLE;
                }
              else if (abs(stInitialTmgData.HActive -
                           Handle_p->sMeasCtrl.CurrentTimingInfo.HActive) > H_DELTA)
                {
                  HDMI_PRINT("Unstable H Active\n");
                  DBG_MEAS_INFO("HActive = %d  ", stInitialTmgData.HActive);
                  DBG_MEAS_INFO("NewHActive = %d\n", Handle_p->sMeasCtrl.
                                CurrentTimingInfo. HActive);
                  return MEAS_HORIZONTAL_TIMING_UNSTABLE;
                }
            }

          /* Check V Sync TimeOut */
          if ((stm_hdmirx_time_minus(stm_hdmirx_time_now(), ulBeginTime)) >=
              (M_NUM_TICKS_PER_MSEC(HDMI_MEAS_VSYNC_TIMEOUT)))
            {
              HDMI_PRINT("V Sync Time out 2:%d ms\n",
                         (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                                      ulBeginTime) / (TICKS_PER_MSEC)));
              DBG_MEAS_INFO("Fail. Time execution = %dms\n",
                            (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                   ulBeginTime) / (TICKS_PER_MSEC)));
              return MEAS_VERTICAL_TIMING_UNSTABLE;
            }
        }
      while (!(CHECK_BIT(Handle_p->sMeasCtrl.mStatus,
                         SIG_STS_VTIMING_MEAS_DATA_AVBL)));

      DBG_MEAS_INFO("V sync loop Time execution:%d ms\n",
                    (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                           ulBeginTime) / (TICKS_PER_MSEC)));

      /* Vertical timing stability */

      if (FirstTimeVMeasDone == FALSE)
        {
          FirstTimeVMeasDone = TRUE;
          stInitialTmgData.VTotal =
            Handle_p->sMeasCtrl.CurrentTimingInfo.VTotal;
          stInitialTmgData.VActive =
            Handle_p->sMeasCtrl.CurrentTimingInfo.VActive;
          if (stInitialTmgData.VTotal < VTOTAL_THRESH ||
              stInitialTmgData.VActive < VTOTAL_THRESH)
            {
              HDMI_PRINT("Abnormal small V Total or V Active\n");
              DBG_MEAS_INFO("HTotal = %d  ", stInitialTmgData.VTotal);
              DBG_MEAS_INFO("Hactive = %d\n", stInitialTmgData.VActive);
              DBG_MEAS_INFO("Fail. Time execution = %dms\n",
                            (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                   ulBeginTime) / (TICKS_PER_MSEC)));
              return MEAS_VERTICAL_TIMING_UNSTABLE;
            }
        }
      else if (abs(stInitialTmgData.VTotal -
                   Handle_p->sMeasCtrl.CurrentTimingInfo.VTotal) > V_DELTA)
        {
          HDMI_PRINT("Unstable V Total\n");
          DBG_MEAS_INFO("VTotal = %d  ", stInitialTmgData.VTotal);
          DBG_MEAS_INFO("New VTotal = %d\n",
                        Handle_p->sMeasCtrl.CurrentTimingInfo.
                        VTotal);
          DBG_MEAS_INFO("Fail. Time execution = %dms\n",
                        (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                               ulBeginTime) / (TICKS_PER_MSEC)));
          return MEAS_VERTICAL_TIMING_UNSTABLE;
        }
      else if (abs(stInitialTmgData.VActive -
                   Handle_p->sMeasCtrl.CurrentTimingInfo.VActive) > V_DELTA)
        {
          HDMI_PRINT("Unstable  VActive\n");
          DBG_MEAS_INFO("VActive = %d  ", stInitialTmgData.VActive);
          DBG_MEAS_INFO("New VActive = %d\n",
                        Handle_p->sMeasCtrl.CurrentTimingInfo.VActive);
          DBG_MEAS_INFO("Fail. Time execution = %dms\n",
                        (U16) (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                               ulBeginTime) / (TICKS_PER_MSEC)));
          return MEAS_VERTICAL_TIMING_UNSTABLE;
        }

    }
  return MEAS_HV_TIMINGS_STABLE;
}

/******************************************************************************
 FUNCTION     : sthdmirx_MEAS_get_PHY_decoder_error
 USAGE        : Get the phy decoder error status.
 INPUT        : uDurationMsec - duration for error measurement  ;ErrMaskLevel: ignore the defined error level
 RETURN       : Decoder Error
 USED_REGS    : None
******************************************************************************/
U16 sthdmirx_MEAS_get_PHY_decoder_error(const hdmirx_handle_t Handle,
                                        U8 uDurationMsec, U16 ErrMaskLevel)
{
  U16 ulDecErr_0, ulDecErr_1, ulDecErr_2;
  hdmirx_route_handle_t *Handle_p;
  Handle_p = (hdmirx_route_handle_t *) Handle;

  /*Clear the previous Decoder Error Status */
  HDMI_SET_REG_BITS_BYTE((Handle_p->BaseAddress + HDRX_LINK_ERR_CTRL),
                         HDRX_PHY_DEC_ERR_CLR);
  HDMI_CLEAR_REG_BITS_BYTE((Handle_p->BaseAddress + HDRX_LINK_ERR_CTRL),
                           HDRX_PHY_DEC_ERR_CLR);

  /* Time Frame to check the decoder Error for all channels */
  if (uDurationMsec)
    {
      STHDMIRX_DELAY_1ms(uDurationMsec);
    }

  /* Set the channel 0,1,2 & get the decoder Error */

  HDMI_WRITE_REG_BYTE((Handle_p->BaseAddress + HDRX_LINK_ERR_CTRL), 0x00);
  ulDecErr_0 = (U16) (HDMI_READ_REG_WORD
                      ((Handle_p->BaseAddress + HDRX_PHY_DEC_ERR_STATUS)) & 0x0fff);

  HDMI_WRITE_REG_BYTE((Handle_p->BaseAddress + HDRX_LINK_ERR_CTRL), 0x01);
  ulDecErr_1 = (U16) (HDMI_READ_REG_WORD
                      ((Handle_p->BaseAddress + HDRX_PHY_DEC_ERR_STATUS)) & 0x0fff);

  HDMI_WRITE_REG_BYTE((Handle_p->BaseAddress + HDRX_LINK_ERR_CTRL), 0x02);
  ulDecErr_2 = (U16) (HDMI_READ_REG_WORD
                      ((Handle_p->BaseAddress + HDRX_PHY_DEC_ERR_STATUS)) & 0x0fff);

  if ((ulDecErr_0 > ErrMaskLevel) || (ulDecErr_1 > ErrMaskLevel) ||
      (ulDecErr_2 > ErrMaskLevel))
    {
      return (ulDecErr_0 + ulDecErr_1 + ulDecErr_2);
    }

  return 0;
}

/******************************************************************************
 FUNCTION     : sthdmirx_MEAS_input_wait_Vsync
 USAGE        : Wait for V Sync Timings.
 INPUT        : B_NumSync -  No of V YSnc
 RETURN       : None
 USED_REGS    : None
******************************************************************************/
U8 sthdmirx_MEAS_input_wait_Vsync(const hdmirx_handle_t Handle, U8 B_NumSync)
{
  U32 DW_BeginTime;

  if (B_NumSync == 0)
    {
      B_NumSync++;
    }

  while (B_NumSync--)
    {
      DW_BeginTime = stm_hdmirx_time_now();

      HDMI_SET_REG_BITS_DWORD((GET_CORE_BASE_ADDRS(Handle) +
                               HDRX_MEAS_IRQ_STS), HDRX_VSYNC_EDGE_IRQ_STS);
      /*Need to check the implementation of positive or negative sync transition [HDRX_VSYNC_LEVEL] */
      while (!(HDMI_READ_REG_WORD(GET_CORE_BASE_ADDRS(Handle) +
                                  HDRX_MEAS_IRQ_STS) & HDRX_VSYNC_EDGE_IRQ_STS))
        {
          if ((stm_hdmirx_time_minus(stm_hdmirx_time_now(), DW_BeginTime)) >=
              (M_NUM_TICKS_PER_MSEC(HDMI_MEAS_VSYNC_TIMEOUT)))
            {
              return FALSE;
            }
        }
    }
  return TRUE;
}

/******************************************************************************
 FUNCTION     :   sthdmirx_MEAS_print_mode
 USAGE        :   Print the current video mode timing information
 INPUT        :

 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_MEAS_print_mode(signal_timing_info_t const *vMode_p)
{
  U16 W_HTotal, W_VTotal, W_HFreq100Hz;

  U32 DW_LinkClock;

  W_HTotal = vMode_p->HTotal;
  W_VTotal = vMode_p->VTotal;
  DW_LinkClock = vMode_p->LinkClockKHz;

  if (W_VTotal == 0 || W_HTotal == 0)
    {
      HDMI_PRINT("HTotal =0 & VTotal =0 \n");
      return;
    }

  W_HFreq100Hz = (U16) ((DW_LinkClock * 100) / ((U32) W_HTotal));

  HDMI_PRINT("\n****** Current Input Mode ******\n");
  HDMI_PRINT("Link Clock  : %dMHz\n", (U16) (DW_LinkClock / 1000));
  HDMI_PRINT("H Total     : %d\n", W_HTotal);
  HDMI_PRINT("V Total     : %d\n", W_VTotal);
  HDMI_PRINT("H Active    : %d\n", vMode_p->HActive);
  HDMI_PRINT("V Active    : %d\n", vMode_p->VActive);
  HDMI_PRINT("H Freq.     : %dKHz\n", W_HFreq100Hz / 100);
  HDMI_PRINT("V Freq.     : %dHz\n\n",
             (U8) ((U32) W_HFreq100Hz * 10 / W_VTotal));

}

/* End of file */
