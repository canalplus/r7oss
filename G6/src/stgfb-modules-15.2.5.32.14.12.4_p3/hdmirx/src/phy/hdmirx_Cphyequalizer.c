/***********************************************************************
 *
 * File: hdmirx/src/phy/hdmirx_Cphyequalizer.c
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
#include <hdmirx_Combophy.h>
#include <hdmirx_core_export.h>
#include <stm_hdmirx_os.h>

/* Private Typedef -----------------------------------------------*/

/* Private Defines ------------------------------------------------*/

#ifdef ENABLE_DBG_EQ_INFO
#define DBG_EQ_INFO	HDMI_PRINT
#else
#define DBG_EQ_INFO(a,b...)
#endif

#define EQ_LOW_FREQ_RANGE    40000UL
#define EQ_MID_FREQ_RANGE    80000UL
#define EQ_HIGH_FREQ_RANGE   160000UL
/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes --------------------------------------*/

#define     SIGNAL_QUAL_SCORE_TH    0x0
#define     TOTAL_GAIN_EQS          8
#define     TOTAL_PKFREQ_EQS        7

/* Interface procedures/functions -----------------------------------*/

/******************************************************************************
 FUNCTION     :   sthdmirx_PHY_adaptive_signal_equalizer_handler
 USAGE        :   Adaptive Equalizer algorithm to reconstruct the Hdmi Input signal.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_PHY_adaptive_signal_equalizer_handler(const hdmirx_handle_t
    Handle, U8 ActiveLink)
{
  hdmirx_route_handle_t *pInpHandle;
  sthdmirx_adaptiveEQ_t *EqHandle_p;
  sthdmirx_PHY_context_t *CPHYControl;

  U32 SigQual = 0;
  U16 ulDecErr;
  U8 pkFreq = 7, uhighFreqGainIdx = 0, presentSigQualCount =0,
     presentHighEqGain = 0, count = 0, debugCnt = TRUE;
  U8 pastSigQualCount = 0, maxSigQualCount = 0, pastHighEqGain =0,
     maxHighEqGain = 0;

  pInpHandle = (hdmirx_route_handle_t *) Handle;

  EqHandle_p =
    (sthdmirx_adaptiveEQ_t *) & pInpHandle->pHYControl.AdaptiveEqHandle;
  CPHYControl = (sthdmirx_PHY_context_t *) & pInpHandle->pHYControl;
  if (CPHYControl->EqualizationType != SIGNAL_EQUALIZATION_ADAPTIVE)
    {
      /* if non-Idle, take some action. */
      EqHandle_p->EqState = EQ_STATE_IDLE;
      return;
    }

  /*Start Eq State Machine */

  switch (EqHandle_p->EqState)
    {
    case EQ_STATE_IDLE:

      /* Clear the Good marked Eq Candidates */
      EqHandle_p->MarkedBestEqCandidate = 0x0;
      EqHandle_p->EqState = EQ_STATE_RUN;
      DBG_EQ_INFO("Equalization Algorithm is started!!\n");
      break;

    case EQ_STATE_RUN:

Eqlzn:
#if 1
      sthdmirx_PHY_set_pk_freq(CPHYControl, pkFreq);
      DBG_EQ_INFO("PEAK FREQUENCY :%d   ", pkFreq);
      for (uhighFreqGainIdx = 0; uhighFreqGainIdx < TOTAL_GAIN_EQS; uhighFreqGainIdx++)
        {
          sthdmirx_PHY_set_eq_gain(CPHYControl, uhighFreqGainIdx);
          STHDMIRX_DELAY_1ms(1);	/*Settling Time */
          DBG_EQ_INFO("Eq_Gain :%d   ", uhighFreqGainIdx);
          ulDecErr =
            sthdmirx_MEAS_get_PHY_decoder_error(pInpHandle, 1,0);
          DBG_EQ_INFO("  DecErr:0x%x", ulDecErr);
          if (pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz < EQ_HIGH_FREQ_RANGE)
            {
              SigQual = sthdmirx_PHY_get_signalquality_score(CPHYControl, 1);	//Donot consider Sigqual in Tracking mode
              DBG_EQ_INFO("SigQual:0x%x ", SigQual);
            }

          if ((SigQual <= SIGNAL_QUAL_SCORE_TH)&& (ulDecErr == 0))
            {
              EqHandle_p->MarkedBestEqCandidate |= (1 << uhighFreqGainIdx);
              presentSigQualCount += 1;
              presentHighEqGain = uhighFreqGainIdx;
              count++;
            }
          else
            {
              if (debugCnt == TRUE)
                {
                  pastSigQualCount = presentSigQualCount;
                  maxSigQualCount = pastSigQualCount;
                  pastHighEqGain = presentHighEqGain;
                  maxHighEqGain = presentHighEqGain;
                  presentSigQualCount = 0;
                  debugCnt = FALSE;
                }
              else
                {
                  if (presentSigQualCount >=pastSigQualCount)
                    {
                      pastSigQualCount = presentSigQualCount;
                      maxSigQualCount = pastSigQualCount;
                      pastHighEqGain = presentHighEqGain;
                      maxHighEqGain = presentHighEqGain;
                      presentSigQualCount = 0;
                    }
                  else
                    {
                      maxSigQualCount = pastSigQualCount;
                      maxHighEqGain = pastHighEqGain;
                      presentSigQualCount = 0;
                    }
                }
            }
        }

      if (presentSigQualCount >= maxSigQualCount)
        {
          maxSigQualCount = presentSigQualCount;
          maxHighEqGain = presentHighEqGain;
        }

      /* Decide the next Eq state based on quick scan search results */

      if (EqHandle_p->MarkedBestEqCandidate == 0)
        {
          EqHandle_p->MarkedBestEqCandidate = 0x0;
          if (pkFreq > 0)
            {
              pkFreq--;
              goto Eqlzn;
            }
          EqHandle_p->EqState = EQ_STATE_FAIL;
          break;
        }

      if (maxHighEqGain > 1)
        {
          maxHighEqGain = maxHighEqGain / 2;
        }
#endif
      DBG_EQ_INFO("Final gains : 0x%x\n", maxHighEqGain);

      sthdmirx_PHY_set_eq_gain(CPHYControl, maxHighEqGain);
      EqHandle_p->EqState = EQ_STATE_DONE;
      break;

    case EQ_STATE_DONE:

      SigQual = 0;
      ulDecErr =
        sthdmirx_MEAS_get_PHY_decoder_error(pInpHandle, 1, 0);

      if (pInpHandle->sMeasCtrl.CurrentTimingInfo.LinkClockKHz < EQ_HIGH_FREQ_RANGE)
        {
          SigQual = sthdmirx_PHY_get_signalquality_score(CPHYControl, 1);	//Donot consider Sigqual in Tracking mode
        }
      if ((SigQual > SIGNAL_QUAL_SCORE_TH) || (!(ulDecErr == 0)))
        {
          EqHandle_p->EqState = EQ_STATE_IDLE;
          DBG_EQ_INFO
          ("\nEntered idle state because of decode error\n");
        }
      break;
    case EQ_STATE_FAIL:
      DBG_EQ_INFO("\nEntered fail state\n");
      EqHandle_p->EqState = EQ_STATE_IDLE;
      pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
      break;
    }
}

/* End of file */
