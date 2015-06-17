/***********************************************************************
 *
 * File: hdmirx/src/system/hdmirx_sys.c
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

#include "hdmirx_drv.h"
#include "hdmirx_core.h"
#include "hdmirx_utility.h"
#include "InternalTypes.h"
#include "hdmirx_core_export.h"
#include "hdmirx_system.h"
#include "stm_hdmirx_os.h"
#include "hdmirx_Combophy.h"
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
#include "hdmirx_clkgen.h"
#endif
#include <hdmirx_vtac.h>

/* Private Typedef -----------------------------------------------*/
//#define ENABLE_DBG_HDRX_SYSTEM_LAYER_INFO

#ifdef ENABLE_DBG_HDRX_SYSTEM_LAYER_INFO
#define DBG_HDRX_SYS_LYR_INFO	HDMI_PRINT
#else
#define DBG_HDRX_SYS_LYR_INFO(a,b...)
#endif

/* Private Defines ------------------------------------------------*/

/* Private macro's ------------------------------------------------*/

#define     HDMI_ABNORMAL_SYNC_TRESHOLD         200	/*msec. */
#define     MEASUREMENT_DONE_PERIOD             700	/*ms,  measurements have to be completed within this period */
#define     VTOTAL_THRESH                       100
#define     HTOTAL_THRESH                       100
#define     DVI_CLOCK_THRESH                    22	/*MHz */
#define     NO_SIGNAL_VIDEO_DDS_FREQ_HZ         96000000UL	/*96MHz */
#define     NO_SIGNAL_AUDIO_DDS_FREQ_HZ         12000000UL	/*12MHz =  128*fs = 128 * 48000 Hz */
#define     HDMI_AVI_INFOFRAME_AVBL_CHECK_TIMEOUT         200	/* 4x frame time msec. */

/* Private Variables -----------------------------------------------*/
BOOL overlapFlag = FALSE;

/* Private functions prototypes ------------------------------------*/
void sthdmirx_signal_monitoring_task(hdmirx_route_handle_t *const pInpHandle);
BOOL sthdmirx_is_signal_change_notification_required(
  hdmirx_route_handle_t *const pInpHandle);

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION     :     sthdmirx_reset_signal_process_handler
 USAGE        :     Reset the Signal processing handler, state machines.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_reset_signal_process_handler(
  hdmirx_route_handle_t *const pInpHandle)
{
  CLEAR_BIT(pInpHandle->HdrxStatus,
            (HDRX_STATUS_LINK_READY | HDRX_STATUS_UNSTABLE |HDRX_STATUS_STABLE));
  SET_BIT(pInpHandle->HdrxStatus, (HDRX_STATUS_NO_SIGNAL));
  pInpHandle->HdrxState = HDRX_FSM_STATE_IDLE;
  pInpHandle->sMeasCtrl.MeasTimer = stm_hdmirx_time_now();
//    pInpHandle->pHYControl.AdaptiveEqHandle.EqState = EQ_STATE_IDLE;
  /*make Rterm value back to 0x8 to make it 50ohm on no signal */
  pInpHandle->pHYControl.RTermValue = pInpHandle->rterm_val;

  /* Clear allsoftwae varaibles, which are related to packet memory acquition & storage */
  sthdmirx_CORE_clear_packet_memory(pInpHandle);

  /* Clear the Packet Noise detected Flag */
  pInpHandle->bIsPacketNoisedetected = FALSE;

  /* Clear HDCP status flag */
  pInpHandle->HDCPStatus = STM_HDMIRX_ROUTE_HDCP_STATUS_NOT_AUTHENTICATED;

  /*Reset the Audio Manager */
  sthdmirx_audio_reset(pInpHandle);

}

/******************************************************************************
 FUNCTION     :     sthdmirx_is_signal_change_notification_required
 USAGE        :     Check the condition to notify the signal chanhe notification.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_is_signal_change_notification_required(
  hdmirx_route_handle_t *const pInpHandle)
{
  BOOL bReturnStatus = FALSE;
  BOOL bCheckAudioStatus = FALSE;

  if (pInpHandle->bIsSigPresentNotified == FALSE)
    {
      if (pInpHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)
        {
          bReturnStatus = TRUE;
        }
      else
        {
          if (pInpHandle->stDataAvblFlags.bIsAviInfoAvbl == TRUE)
            {
              bCheckAudioStatus = TRUE;
            }
          else
            {
              if (stm_hdmirx_time_minus(
                    stm_hdmirx_time_now(),
                    pInpHandle->ulAVIInfoFrameAvblTMO) >(M_NUM_TICKS_PER_MSEC(HDMI_AVI_INFOFRAME_AVBL_CHECK_TIMEOUT)))
                {
                  /* fill the properties with the default parameters */
                  pInpHandle->stInfoPacketData.eColorDepth = HDRX_GCP_CD_24BPP;
                  HDMI_MEM_SET(
                    &pInpHandle->stInfoPacketData.stAviInfo,0,sizeof(HDRX_AVI_INFOFRAME));
                  bCheckAudioStatus = TRUE;
                  HDMI_PRINT
                  ("HDMI Signal without AVI infoFrame..TMO\n");
                }
            }

          if (bCheckAudioStatus == TRUE)
            {
              if ((pInpHandle->stAudioMngr.bIsAudioPktsPresent == FALSE)||
                  ((pInpHandle->stAudioMngr.bIsAudioPktsPresent == TRUE)&&
                   ((pInpHandle->stAudioMngr.stAudioMngrState == HDRX_AUDIO_MUTE) ||
                    (pInpHandle->stAudioMngr.stAudioMngrState == HDRX_AUDIO_OUTPUTTED))))
                {
                  bReturnStatus = TRUE;
                }
            }
        }

    }

  return bReturnStatus;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_signal_monitoring_task
 USAGE        :     Monitors the Hdmi Hardware to get the input signal status,
                    which updates all hardware measurement results
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_signal_monitoring_task(hdmirx_route_handle_t *const pInpHandle)
{
  /* Check the link clock status */
  pInpHandle->sMeasCtrl.LinkClockstatus =sthdmirx_MEAS_is_linkclk_present(pInpHandle);
  if (pInpHandle->sMeasCtrl.LinkClockstatus == TRUE)  	/*if link clock present, go for signal monitor */
    {
      /* check the link clock availability & stability */
      sthdmirx_MEAS_linkclk_meas(pInpHandle);
      /*if link clock is stable, then we have to go for next level signal testing */
      if (pInpHandle->sMeasCtrl.IsLinkClkStable)
        {
          /* set the pll as per input link freq */
          if (pInpHandle->sMeasCtrl.IsPLLSetupDone == FALSE)
            {
              sthdmirx_PHY_PLL_setup(
                &pInpHandle->pHYControl,
                pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz, FALSE);
              pInpHandle->sMeasCtrl.IsPLLSetupDone = TRUE;
              return;	/* do the adaptive Equalization in next cycle */
            }

          /*Run the Adaptive signal Equalizer */
          sthdmirx_PHY_adaptive_signal_equalizer_handler(pInpHandle);

          /* if Equalization is done, start the measurement. */
          if ((pInpHandle->pHYControl.EqualizationType != SIGNAL_EQUALIZATION_ADAPTIVE) ||
              ((pInpHandle->pHYControl.AdaptiveEqHandle.EqState == EQ_STATE_DONE) &&
               (pInpHandle->pHYControl.EqualizationType == SIGNAL_EQUALIZATION_ADAPTIVE)))
            {

              sthdmirx_MEAS_vertical_timing_meas(pInpHandle);
              sthdmirx_MEAS_horizontal_timing_meas(pInpHandle);
              /*Measurement data is available */
              if (pInpHandle->sMeasCtrl.mStatus &
                  (SIG_STS_HTIMING_MEAS_DATA_AVBL | SIG_STS_VTIMING_MEAS_DATA_AVBL))
                {
                  /* update the measurement timeout */
                  pInpHandle->sMeasCtrl.MeasTimer = stm_hdmirx_time_now();
                  /*check the abnormal sync */
                  if((pInpHandle->sMeasCtrl.CurrentTimingInfo.HTotal <HTOTAL_THRESH) ||
                      (pInpHandle->sMeasCtrl.CurrentTimingInfo.VTotal <VTOTAL_THRESH) ||
                      (pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz/1000 <DVI_CLOCK_THRESH))
                    {
                      if(!CHECK_BIT(pInpHandle->sMeasCtrl.mStatus,SIG_STS_ABNORMAL_SYNC_TIMER_STARTED))
                        {
                          SET_BIT(pInpHandle->sMeasCtrl.mStatus,
                                  SIG_STS_ABNORMAL_SYNC_TIMER_STARTED);
                          pInpHandle->sMeasCtrl.ulAbnormalSyncTimer = stm_hdmirx_time_now();
                          DBG_HDRX_SYS_LYR_INFO
                          (" Abnormal sync Tmr started :0x%x\n",
                           pInpHandle->sMeasCtrl.ulAbnormalSyncTimer);
                        }
                      else if (stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                                     pInpHandle->sMeasCtrl.ulAbnormalSyncTimer) > (M_NUM_TICKS_PER_MSEC(HDMI_ABNORMAL_SYNC_TRESHOLD)))
                        {
                          DBG_HDRX_SYS_LYR_INFO
                          (" Abnormal sync Time:0x%x\n",(U32)stm_hdmirx_time_now());
                          SET_BIT(
                            pInpHandle->sMeasCtrl.mStatus,SIG_STS_UNSTABLE_TIMING_PRESENT);
                        }
                    }
                  else
                    {
                      if (CHECK_BIT(
                            pInpHandle->sMeasCtrl.mStatus,SIG_STS_ABNORMAL_SYNC_TIMER_STARTED))
                        {
                          HDMI_PRINT
                          ("Abnormal sync disappears\n");
                          CLEAR_BIT(pInpHandle->sMeasCtrl.mStatus,
                                    (SIG_STS_UNSTABLE_TIMING_PRESENT |
                                     SIG_STS_ABNORMAL_SYNC_TIMER_STARTED));
                        }
                    }
                  /* check the H, V Stability */
                  if (!CHECK_BIT(pInpHandle->sMeasCtrl.mStatus,
                                 SIG_STS_SYNC_STABILITY_CHECK_STARTED))
                    {
                      sthdmirx_MEAS_initialize_stabality_check_mode(
                        &pInpHandle->sMeasCtrl);
                      SET_BIT(pInpHandle->sMeasCtrl.mStatus,
                              SIG_STS_SYNC_STABILITY_CHECK_STARTED);
                    }
                  else
                    {
                      sthdmirx_MEAS_monitor_input_signal_stability(
                        &pInpHandle->sMeasCtrl);
                    }

                  /*check the double clock mode */
                  sthdmirx_MEAS_check_double_clk_mode(&pInpHandle->sMeasCtrl);
                  /*re-trigger the H&V Timing measurement */
                  CLEAR_BIT(pInpHandle->sMeasCtrl.mStatus,
                            (SIG_STS_HTIMING_MEAS_DATA_AVBL |
                             SIG_STS_VTIMING_MEAS_DATA_AVBL));

                  SET_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_STABLE);
                }
              else if ((stm_hdmirx_time_minus(stm_hdmirx_time_now(),
                                              pInpHandle->sMeasCtrl.MeasTimer)) >(M_NUM_TICKS_PER_MSEC(MEASUREMENT_DONE_PERIOD)))
                {
                  /* check the measurement timeout. */
                  DBG_HDRX_SYS_LYR_INFO
                  ("HdmiRx:Timeout...Measurement Data is not available\n");
                  SET_BIT(pInpHandle->sMeasCtrl.mStatus, SIG_STS_UNSTABLE_TIMING_PRESENT);
                }

            }
        }
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_main_signal_process_handler
 USAGE        :     This fn is main Hdmi Signal process handler, it monitors all input signal activities.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
void sthdmirx_main_signal_process_handler(
  hdmirx_route_handle_t *const pInpHandle)
{

  sthdmirx_IFM_timing_params_t pIfmParams;

  /*Is Signal Monitoring is needed */
  if (pInpHandle->bIsSignalDetectionStarted == FALSE)
    {
      if (pInpHandle->HdrxState != HDRX_FSM_STATE_IDLE)
        {
          if (pInpHandle->HdrxState == HDRX_FSM_STATE_SIGNAL_STABLE)
            {
              pInpHandle->HdrxState = HDRX_FSM_STATE_TRANSITION_TO_NOSIGNAL;
            }
        }
      return;
    }
  sthdmirx_signal_monitoring_task(pInpHandle);

  /* Check the Signal Transition */
  if (pInpHandle->sMeasCtrl.LinkClockstatus == TRUE)
    {
      if (CHECK_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_NO_SIGNAL))
        {
          if (!hdmirx_get_src_plug_status((hdmirx_port_handle_t *)pInpHandle->PortHandle))
          {
            /* Transistion to NOSIGNAL TO SIGNAL */
            CLEAR_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_NO_SIGNAL);
            pInpHandle->HdrxState = HDRX_FSM_STATE_TRANSITION_TO_SIGNAL;
          }
          goto STATE_MACHINE;
        }
    }
  else
    {
      if (!CHECK_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_NO_SIGNAL))
        {
          /* Transistion to SIGNAL TO NO_SIGNAL */
          SET_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_NO_SIGNAL);
          pInpHandle->HdrxState = HDRX_FSM_STATE_TRANSITION_TO_NOSIGNAL;
          goto STATE_MACHINE;
        }
      /* if no signal state, remains in the idle state or i will decide one more state, NO_SIGNAL */
      if (CHECK_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_LINK_INIT))
        {
          CLEAR_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_LINK_INIT);
        }
      goto STATE_MACHINE;
    }

  /*check the clock measurement */
  if (pInpHandle->sMeasCtrl.IsLinkClkAvailable == FALSE)
    {
      //HDMI_PRINT("HdmiRx:Link clock measurement not completed yet\n");
      pInpHandle->HdrxState = HDRX_FSM_STATE_IDLE;
      pInpHandle->sMeasCtrl.MeasTimer = stm_hdmirx_time_now();
      goto STATE_MACHINE;
    }
  else if (pInpHandle->sMeasCtrl.IsLinkClkStable == FALSE)
    {
      //HDMI_PRINT("HdmiRx:Link Clock becomes Unstable\n");
      if (pInpHandle->HdrxState != HDRX_FSM_STATE_IDLE)
        {
          pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
          goto STATE_MACHINE;
        }
    }
  else if (pInpHandle->sMeasCtrl.IsLinkClkStable == TRUE)
    {
      /*check the condition to start Equalization Process */
      if (pInpHandle->HdrxState == HDRX_FSM_STATE_IDLE)
        {
          stm_hdmirx_sema_wait(pInpHandle->hDevice->hdmirx_pm_sema);
          if (! pInpHandle->hDevice->is_standby)
          {
            pInpHandle->HdrxState = HDRX_FSM_STATE_START;
            HDMI_PRINT("HdmiRx: SM started\n");
          }
          stm_hdmirx_sema_signal(pInpHandle->hDevice->hdmirx_pm_sema);
        }
    }

  /*Need to check the unstability flags, if signal become unstable, move to unstable state directly */
  if (CHECK_BIT(
        pInpHandle->sMeasCtrl.mStatus, SIG_STS_UNSTABLE_TIMING_PRESENT))
    {
      HDMI_PRINT
      ("HdmiRx:LClk becomes Unstable..Measurement declares\n");
      pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
      goto STATE_MACHINE;
    }

  /* State Machine starts.   */
STATE_MACHINE:
  switch (pInpHandle->HdrxState)
    {

    case HDRX_FSM_STATE_IDLE:
      /* doing nothing, makes everything ready. free running mode. */
      break;

    case HDRX_FSM_STATE_UNSTABLE_TIMING:
      HDMI_PRINT("HdmiRx: Tr2Unstable\n");
      sthdmirx_CORE_reset(pInpHandle,
                          (RESET_CORE_LINK_CLOCK_DOMAIN | RESET_CORE_PACKET_PROCESSOR |
                           RESET_CORE_VIDEO_LOGIC | RESET_CORE_AUDIO_OUTPUT));
      /*make fall throug.. don't edit the states */
    case HDRX_FSM_STATE_AFR_AFB_TRIGGERED:
      /*Send the no signal notification when AFB/AFR Triggered */
    case HDRX_FSM_STATE_TRANSITION_TO_NOSIGNAL:
      /*Reset all hardware, measurement logic,state machine */
      HDMI_PRINT("HdmiRx: Tr2NoSignal\n");
      /* if previous PLL mode is x10, if  225 Mhz Link clock inputed after clock loss, so PLL will be out of operating range i.e 225x10 MHz so keep in x2.5 mode */
      sthdmirx_PHY_PLL_setup(&pInpHandle->pHYControl, 0, TRUE);
      sthdmirx_reset_signal_process_handler(pInpHandle);
      sthdmirx_MEAS_reset_measurement(&pInpHandle->sMeasCtrl);

#ifdef STHDMIRX_CLOCK_GEN_ENABLE
      sthdmirx_clkgen_DDS_openloop_force(pInpHandle->stDdsConfigInfo.estVidDds,
                                         VDDS_DEFAULT_INIT_FREQ, pInpHandle->MappedClkGenAddress);
      sthdmirx_clkgen_DDS_openloop_force(pInpHandle->stDdsConfigInfo.estAudDds,
                                         ADDS_DEFAULT_INIT_FREQ,pInpHandle->MappedClkGenAddress);
#endif
      if (pInpHandle->bIsNoSignalNotified == FALSE)
      {
        pInpHandle->bIsNoSignalNotify = TRUE;
        pInpHandle->bIsNoSignalNotified = TRUE;
      }
      /*Reset Australian mode detection to allow for other modes */
      if (overlapFlag &&
          (!(pInpHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)))
        {
          overlapFlag = FALSE;
          sthdmirx_IFM_reset_australianmode_interlace_detect((sthdmirx_IFM_context_t *) & pInpHandle->IfmControl);
          sthdmirx_video_enable_regen_sync(pInpHandle);
          HDMI_PRINT
          ("\n  Australian modes disabled .......................... !!!!!!!! \n");
        }
      break;

    case HDRX_FSM_STATE_TRANSITION_TO_SIGNAL:
      HDMI_PRINT("HdmiRx:Tr2Signal\n");
      /*reset all measurement logics. */
      sthdmirx_MEAS_reset_measurement(&pInpHandle->sMeasCtrl);
      /*reset core link */
      hdmirx_reset_vtac();

      /* if previous PLL mode is x10, if  225 Mhz Link clock inputed after clock loss, so PLL will be out of operating range i.e 225x10 MHz so keep in x2.5 mode */
      pInpHandle->HdrxState = HDRX_FSM_STATE_IDLE;
      break;

    case HDRX_FSM_STATE_START:
      pInpHandle->bIsSigPresentNotified = FALSE;
      /*prepare for Equalization & Set for Abnormalities & double clock mode checks. */

      if (pInpHandle->pHYControl.EqualizationType == SIGNAL_EQUALIZATION_ADAPTIVE)
        {
          pInpHandle->pHYControl.RTermValue = 0xC;
          pInpHandle->HdrxState = HDRX_FSM_STATE_EQUALIZATION_UNDER_PROGRESS;
        }
      else
        {
          pInpHandle->HdrxState = HDRX_FSM_STATE_VERIFY_LINK_HEALTH;
        }

      break;

    case HDRX_FSM_STATE_EQUALIZATION_UNDER_PROGRESS:
      /* checks the Eq state machine, if completed, then Move to stable otherwise take actions as per states */
      if (pInpHandle->pHYControl.AdaptiveEqHandle.EqState == EQ_STATE_DONE)
        {
          pInpHandle->HdrxState = HDRX_FSM_STATE_VERIFY_LINK_HEALTH;
          HDMI_PRINT("HdmiRx: Eq done!!\n");
        }
      else if (pInpHandle->pHYControl.AdaptiveEqHandle.EqState == EQ_STATE_FAIL)
        {
          /* Take some hardware action if needed */
          HDMI_PRINT("HdmiRx: unstable due to Equalizer...\n");
          pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
        }
      else
        {
          HDMI_PRINT("HdmiRx: Adaptive Equalizer...\n");
        }
      break;

    case HDRX_FSM_STATE_VERIFY_LINK_HEALTH:	//transition to
      /* check sync abnormality , if found ok, declare as stable link */
      if(!CHECK_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_STABLE))
      {
        pInpHandle->HdrxState = HDRX_FSM_STATE_VERIFY_LINK_HEALTH;
        break;
      }
      pInpHandle->HdrxState = HDRX_FSM_STATE_SIGNAL_STABLE;
#ifdef STHDMIRX_CLOCK_GEN_ENABLE
      sthdmirx_clkgen_DDS_closeloop_force(pInpHandle->stDdsConfigInfo.estVidDds,
                                          (pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz * 1000),
                                          pInpHandle->MappedClkGenAddress);
#if 0
      sthdmirx_clkgen_DDS_closeloop_force(pInpHandle->stDdsConfigInfo.estAudDds,
                                          pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz * 1000,pInpHandle->MappedClkGenAddress);
#endif
#endif
      sthdmirx_CORE_select_pixel_clk_out_domain(pInpHandle);

      /* Prints the detected Timing Data */
      sthdmirx_MEAS_print_mode(&pInpHandle->sMeasCtrl.CurrentTimingInfo);
      sthdmirx_IFM_clear_interlace_decode_error_status(&pInpHandle->IfmControl);
      pInpHandle->ulAVIInfoFrameAvblTMO = stm_hdmirx_time_now();
      sthdmirx_IFM_signal_timing_get((sthdmirx_IFM_context_t *) &
                                     pInpHandle->IfmControl, &pIfmParams);
      if (sthdmirx_IFM_is_mode_overlapping (
            (U16) (pIfmParams.HFreq_Hz / 100), pIfmParams.VFreq_Hz) == TRUE &&
          (!(pInpHandle->HwInputSigType == HDRX_INPUT_SIGNAL_DVI)))
        {
          overlapFlag = TRUE;
          sthdmirx_IFM_set_australianmode_interlace_detect((sthdmirx_IFM_context_t *) & pInpHandle->IfmControl);
#if 0
          sthdmirx_video_enable_raw_sync(pInpHandle);
#endif
          HDMI_PRINT
          ("\n Australian modes found .......................... !!!!!!!! \n");
        }
      break;

    case HDRX_FSM_STATE_SIGNAL_STABLE:
      pInpHandle->bIsNoSignalNotified = FALSE;
      if (CHECK_BIT(pInpHandle->HdrxStatus, HDRX_STATUS_STABLE))
        {
          sthdmirx_CORE_ISR_SDP_client(pInpHandle);
          sthdmirx_CORE_ISR_SDP_audio_client(pInpHandle);

          if (
            (pInpHandle->HdmiRxMode == STM_HDMIRX_ROUTE_OP_MODE_DVI) ||
            (sthdmirx_CORE_is_HDMI_signal(pInpHandle) == FALSE))
            {
              sthdmirx_CORE_DVI_signal_process_handler
              (pInpHandle);
            }

          if ((pInpHandle->HdmiRxMode != STM_HDMIRX_ROUTE_OP_MODE_DVI) &&
              (sthdmirx_CORE_is_HDMI_signal(pInpHandle) == TRUE))
            {
              sthdmirx_CORE_HDMI_signal_process_handler(pInpHandle);
            }
          sthdmirx_audio_manager(pInpHandle);
          /* first time signal notification, when Audio/video is completely ready */
          if (sthdmirx_is_signal_change_notification_required(
                pInpHandle) == TRUE)
            {
              /* Clears the AFR detect status */
              sthdmirx_IFM_clear_AFRsignal_detect_status(&pInpHandle->IfmControl);
              pInpHandle->bIsSignalPresentNotify = TRUE;
              pInpHandle->bIsSigPresentNotified = TRUE;

            }
          if ((sthdmirx_IFM_get_AFRsignal_detect_status(&pInpHandle->IfmControl) == TRUE) &&
              (pInpHandle->bIsSigPresentNotified == TRUE))
            {
              HDMI_PRINT("HdmiRx: Trigger AFB!!\n");
              pInpHandle->HdrxState = HDRX_FSM_STATE_AFR_AFB_TRIGGERED;
              break;
            }
          if (sthdmirx_CORE_HDCP_Authentication_Detection(pInpHandle) == TRUE)
            {
              //pInpHandle->bIsSignalPresentNotify = TRUE;
              pInpHandle->bIsHDCPAuthenticationDetected = TRUE;
            }
          if ((pInpHandle->bIsSignalPresentNotify == FALSE) &&
              (pInpHandle->bIsSigPresentNotified == TRUE))
            {
              pInpHandle->HDCPStatus = sthdmirx_CORE_HDCP_status_get(pInpHandle);
            }
          //HDMI_PRINT("HDRX_FSM_STATE_SIGNAL_STABLE\n");
        }
      break;

    }
  return;

}
