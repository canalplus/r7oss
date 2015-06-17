/***********************************************************************
 *
 * File: hdmirx/src/core/hdmirx_core.c
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
#include <hdmirx_RegOffsets.h>
/* Private Typedef -----------------------------------------------*/

/* Private Defines ------------------------------------------------*/

/* Private macro's ------------------------------------------------*/

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/

/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_reset
 USAGE        :     Reset the Core Hardware as per requested blocks
 INPUT        :     sthdmirx_core_reset_ctrl_t modes.
 RETURN       :     None
 USED_REGS    :     HDRX_RESET_CTRL
******************************************************************************/
void sthdmirx_CORE_reset(const hdmirx_handle_t pHandle,
                         sthdmirx_core_reset_ctrl_t uReset)
{

  HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) + HDRX_RESET_CTRL),
                         uReset);

}

/******************************************************************************
 FUNCTION     :   sthdmirx_CORE_enable_reset
 USAGE        :   Reset the complete core hardware.
 INPUT        :   None
 RETURN       :   None
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_enable_reset(const hdmirx_handle_t pHandle, BOOL uReset)
{

  if (uReset == TRUE)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                              HDRX_RESET_CTRL), HDRX_RESET_ALL);
      sthdmirx_CORE_reset(pHandle, RESET_CORE_ALL_BLOCKS);

    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                                HDRX_RESET_CTRL), HDRX_RESET_ALL);

      STHDMIRX_DELAY_1ms(5);
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                                HDRX_RESET_CTRL), (HDRX_RESET_LINK | HDRX_RESET_HDMI_SDP |
                                    HDRX_RESET_VIDEO | HDRX_RESET_AUDIO | HDRX_RESET_TCLK));
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                                HDRX_RESET_CTRL), HDRX_RESET_TCLK | HDRX_RESET_AUDIO);

      STHDMIRX_DELAY_1ms(5);

    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_enable_auto_reset_on_no_clk
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_enable_auto_reset_on_no_clk(const hdmirx_handle_t pHandle,
    BOOL uReset)
{
  if (uReset == TRUE)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                              HDRX_RESET_CTRL), HDRX_NO_SIGNL_RESET_EN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                                HDRX_RESET_CTRL), HDRX_NO_SIGNL_RESET_EN);
    }
}

/******************************************************************************
 FUNCTION     :    sthdmirx_CORE_enable_auto_reset_on_port_change
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_enable_auto_reset_on_port_change(const hdmirx_handle_t
    pHandle, BOOL uReset)
{
  if (uReset == TRUE)
    {
      HDMI_SET_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                              HDRX_RESET_CTRL), HDRX_RESET_ON_PORT_CHNG_EN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_BYTE((GET_CORE_BASE_ADDRS(pHandle) +
                                HDRX_RESET_CTRL), HDRX_RESET_ON_PORT_CHNG_EN);
    }

}

/******************************************************************************
 FUNCTION     :    sthdmirx_CORE_config_clk
 USAGE        :    config the Clocks.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_config_clk(const hdmirx_handle_t pHandle,
                              sthdmirx_clk_selection_t eClkSel,
                              sthdmirx_clk_type_t eClktype, BOOL bIsPhaseInv)
{
  BOOL bRetStatus = TRUE;
  hdmirx_route_handle_t *pInpHandle;
  U32 ulCtrlWord = 0x0;
  pInpHandle = (hdmirx_route_handle_t *) pHandle;

  switch (eClkSel)
    {
    case HDMIRX_TCLK_SEL:
    {

      ulCtrlWord = HDMI_READ_REG_DWORD((pInpHandle->BaseAddress +
                                        HDRX_TCLK_SEL));

      if (eClktype == HDMIRX_TCLK)
        {
          ulCtrlWord |= HDRX_TCLK_SEL_MASK;
        }
      else if (eClktype == CLK_GND)
        {
          ulCtrlWord &= ~(HDRX_TCLK_SEL_MASK);
        }
      else
        {
          bRetStatus = FALSE;
          HDMI_PRINT
          ("Unknown Clock Type for HDMIRX_TCLK_SEL selection:%d\n",
           eClktype);
        }

      if (bRetStatus)
        {
          if (TRUE == bIsPhaseInv)
            {
              ulCtrlWord |= HDRX_TCLK_INV;
            }
          else
            {
              ulCtrlWord &= ~(HDRX_TCLK_INV);
            }
          HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                     HDRX_TCLK_SEL), ulCtrlWord);
          // HDMI_PRINT("Core Clock Configuration, TCLK is programmed to 0x%x\n",ulCtrlWord);
        }

    }
    break;
    case HDMIRX_LINK_CLK_SEL:
    {

      ulCtrlWord = HDMI_READ_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                             HDRX_LNK_CLK_SEL));
      ulCtrlWord &= ~(HDRX_LCLK_SEL);

      if (eClktype == HDMIRX_TMDS_CLK)
        {
          ulCtrlWord |= 0x01;
        }
      else if (eClktype == HDMIRX_TCLK)
        {
          ulCtrlWord |= 0x02;
        }
      else if (eClktype == CLK_GND)
        {
          ulCtrlWord |= 0x00;
        }
      else
        {
          bRetStatus = FALSE;
          HDMI_PRINT
          ("Unknown Clock Type for HDMIRX_LINK_CLK_SEL selection:%d\n",
           eClktype);
        }

      if (bRetStatus)
        {
          if (TRUE == bIsPhaseInv)
            {
              ulCtrlWord |= HDRX_LCLK_INV;
            }
          else
            {
              ulCtrlWord &= ~(HDRX_LCLK_INV);
            }

          HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                     HDRX_LNK_CLK_SEL), ulCtrlWord);
          //  HDMI_PRINT("Core Clock Configuration, Link clock is programmed to 0x%x\n",ulCtrlWord);

        }

    }
    break;

    case HDMIRX_VCLK_SEL:
    {

      ulCtrlWord = HDMI_READ_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                             HDRX_VCLK_SEL));
      ulCtrlWord &= ~(HDRX_VCLK_SEL_MASK);

      if (eClktype == HDMIRX_VDDS_CLK)
        {
          ulCtrlWord |= 0x01;
        }
      else if (eClktype == HDMIRX_LCLK)
        {
          ulCtrlWord |= 0x02;
        }
      else if (eClktype == HDMIRX_TCLK)
        {
          ulCtrlWord |= 0x03;
        }
      else if (eClktype == CLK_GND)
        {
          ulCtrlWord |= 0x00;
        }
      else
        {
          bRetStatus = FALSE;
          HDMI_PRINT
          ("Unknown Clock Type for HDMIRX_VCLK_SEL selection:%d\n",
           eClktype);
        }

      if (bRetStatus)
        {
          if (TRUE == bIsPhaseInv)
            {
              ulCtrlWord |= HDRX_VCLK_INV;
            }
          else
            {
              ulCtrlWord &= ~(HDRX_VCLK_INV);
            }

          HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                     HDRX_VCLK_SEL), ulCtrlWord);
          // HDMI_PRINT("Core Clock Configuration, Video clock is programmed to 0x%x\n",ulCtrlWord);

        }

    }
    break;

    case HDMIRX_PIX_CLK_OUT_SEL:
    {

      ulCtrlWord =
        HDMI_READ_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                  HDRX_OUT_PIX_CLK_SEL));
      ulCtrlWord &= ~(HDRX_PIX_CLK_OUT_SEL);

      if (eClktype == HDMIRX_VCLK)
        {
          ulCtrlWord |= 0x01;
          HDMI_PRINT("Pixel Clk Out is in VCLK domain\n");
        }
      else if (eClktype == HDMIRX_SUB_SAMPLER_CLK)
        {
          ulCtrlWord |= 0x02;
          HDMI_PRINT
          ("Pixel Clk Out is in Sub Sampler clock domain\n");
        }
      else if (eClktype == HDMIRX_TCLK)
        {
          ulCtrlWord |= 0x03;
        }
      else if (eClktype == CLK_GND)
        {
          ulCtrlWord |= 0x00;
        }
      else
        {
          bRetStatus = FALSE;
          HDMI_PRINT
          ("Unknown Clock Type for HDMIRX_PIX_CLK_OUT_SEL selection:%d\n",
           eClktype);
        }

      if (bRetStatus)
        {
          if (TRUE == bIsPhaseInv)
            {
              ulCtrlWord |= HDRX_PIX_CLK_OUT_INV;
            }
          else
            {
              ulCtrlWord &= ~(HDRX_PIX_CLK_OUT_INV);
            }

          HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                     HDRX_OUT_PIX_CLK_SEL), ulCtrlWord);
        }

    }
    break;
    case HDMIRX_AUCLK_SEL:
    {

      ulCtrlWord =
        HDMI_READ_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                  HDRX_AUCLK_SEL));
      ulCtrlWord &= ~(HDRX_AUCLK_SEL_MASK);

      if (eClktype == HDMIRX_AUDDS_CLK)
        {
          ulCtrlWord |= 0x01;
        }
      else if (eClktype == HDMIRX_TCLK)
        {
          ulCtrlWord |= 0x02;
        }
      else if (eClktype == CLK_GND)
        {
          ulCtrlWord |= 0x00;
        }
      else
        {
          bRetStatus = FALSE;
          HDMI_PRINT("Unknown Clock Type for TCLK selection:%d\n",
                     eClktype);
        }

      if (bRetStatus)
        {
          if (TRUE == bIsPhaseInv)
            {
              ulCtrlWord |= HDRX_AUCLK_INV;
            }
          else
            {
              ulCtrlWord &= ~(HDRX_AUCLK_INV);
            }

          HDMI_WRITE_REG_DWORD((U32)(pInpHandle->BaseAddress +
                                     HDRX_AUCLK_SEL), ulCtrlWord);
          //STTBX_Print(("Core Clock Configuration, Audio clock is programmed to 0x%x\n",ulCtrlWord));

        }

    }
    break;
    default:
      HDMI_PRINT("Error !!  Unknow Clock Selection Type :%d\n", eClkSel);
      break;
    }

  return bRetStatus;
}

/******************************************************************************
 FUNCTION     :    sthdmirx_CORE_initialize_clks
 USAGE        :    Intialize the clocks.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_CORE_initialize_clks(const hdmirx_handle_t pHandle)
{
  /* Select the Clock gen TCLK & Generate the HDMIRX_TCLK_tree */
  sthdmirx_CORE_config_clk(pHandle, HDMIRX_TCLK_SEL, HDMIRX_TCLK, FALSE);

  /*Enable the Fast Clock detection circuit for clock monitoring activity */
  HDMI_WRITE_REG_DWORD((GET_CORE_BASE_ADDRS(pHandle) +
                        HDRX_CLK_FST_MS_CTRL), 0x1f);
  HDMI_SET_REG_BITS_DWORD((GET_CORE_BASE_ADDRS(pHandle) +
                           HDRX_LNK_CLK_SEL), HDRX_LCLK_SLW_NO_CLK_EN);

  /*Configure the link clock */
  sthdmirx_CORE_config_clk(pHandle, HDMIRX_LINK_CLK_SEL, HDMIRX_TMDS_CLK,
                           FALSE);

  /*Configure the video clock */
  sthdmirx_CORE_config_clk(pHandle, HDMIRX_VCLK_SEL, HDMIRX_LCLK, FALSE);

  /*Configure the Audio clock */
  sthdmirx_CORE_config_clk(pHandle, HDMIRX_AUCLK_SEL, HDMIRX_AUDDS_CLK,
                           FALSE);

  /*Config the pixel output clock */
  sthdmirx_CORE_config_clk(pHandle, HDMIRX_PIX_CLK_OUT_SEL, HDMIRX_TCLK,
                           FALSE);

  return TRUE;
}

/******************************************************************************
 FUNCTION     :       sthdmirx_CORE_select_PHY_source
 USAGE        :
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_CORE_select_PHY_source(const hdmirx_handle_t pHandle, U8 Phy, BOOL bIsPolInv)
{
  HDMI_WRITE_REG_BYTE((GET_CORE_BASE_ADDRS(pHandle) + HDRX_INPUT_SEL),
                      (Phy & 0x30));

  if (bIsPolInv)
  {
    /* Orly 2 :inverted polarity for CH0,1,2*/
    HDMI_WRITE_REG_WORD((GET_CORE_BASE_ADDRS(pHandle) + HDRX_LINK_CONFIG),
                        0x1702);
  }
  else
  {
    HDMI_WRITE_REG_WORD((GET_CORE_BASE_ADDRS(pHandle) + HDRX_LINK_CONFIG),
                        0x1002);
  }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_is_HDMI_signal
 USAGE        :     Get the hardware status for HDMI or DVI signals
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     HDRX_MAIN_LINK_STATUS.HDRX_HDMI_SIGN_DETECTED
******************************************************************************/
BOOL sthdmirx_CORE_is_HDMI_signal(const hdmirx_handle_t pHandle)
{
  if ((HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(pHandle) +
                          HDRX_MAIN_LINK_STATUS)) & (HDRX_HDMI_SIGN_DETECTED))
    {
      return TRUE;
    }
  return FALSE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_is_HDMI1V3_signal
 USAGE        :     Get the hardware status for HDMI standard 1.1 or 1.3
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     HDRX_MAIN_LINK_STATUS.HDRX_HDMI_SIGN_DETECTED
******************************************************************************/
BOOL sthdmirx_CORE_is_HDMI1V3_signal(const hdmirx_handle_t pHandle)
{
  if ((HDMI_READ_REG_BYTE(GET_CORE_BASE_ADDRS(pHandle) +
                          HDRX_MAIN_LINK_STATUS)) & (HDRX_HDMI_1_3_DETECTED))
    {
      return TRUE;
    }
  return FALSE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_HDMI_signal_process_handler
 USAGE        :     Hdmi signal processing handler
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_CORE_HDMI_signal_process_handler(const hdmirx_handle_t pHandle)
{
  hdmirx_route_handle_t *pInpHandle;
  hdrx_input_signal_type_t tHdmiMode = HDRX_INPUT_SIGNAL_HDMI_1V1;
  sthdmirx_sub_sampler_mode_t tSubSamplerMode;

  pInpHandle = (hdmirx_route_handle_t *) pHandle;

  /* Get the Hdmi signal type, is it 1.3 or 1.1 */
  if (sthdmirx_CORE_is_HDMI1V3_signal(pHandle) == TRUE)
    {
      tHdmiMode = HDRX_INPUT_SIGNAL_HDMI_1V3;
    }
  /* if previous Hw mode is differnt than current mode, do the audio reset */
  if (tHdmiMode != pInpHandle->HwInputSigType)
    {
      /* Audio Block reset is required */
      HDMI_PRINT("Hdmi Signal Type changes Previous:%d  \n",
                 pInpHandle->HwInputSigType);
      HDMI_PRINT("Hdmi Signal Type changes Current tHdmiMode:%d\n",tHdmiMode);
      pInpHandle->HwInputSigType = tHdmiMode;
    }

  /* Do the Hdcp Noise check based on the noise detector block */
  if (pInpHandle->bIsPacketNoisedetected == TRUE)
    {
      HDMI_PRINT
      ("Video processing Block Stops, reason: Packet Noise detected\n");
      return FALSE;
    }

  /*Special Packet processing handles. */

  if (pInpHandle->stDataAvblFlags.bIsAviInfoAvbl == TRUE)
    {
      stm_hdmirx_pixel_repeat_factor_t stPixelRepeat;
      stm_hdmirx_color_format_t stColorSpace;

      stPixelRepeat = sthdmirx_video_HW_pixel_repeatfactor_get(pHandle);
      if (stPixelRepeat != (stm_hdmirx_pixel_repeat_factor_t)
          pInpHandle->stInfoPacketData.stAviInfo.PR)
        {
          pInpHandle->stInfoPacketData.stAviInfo.PR = stPixelRepeat;
          pInpHandle->stNewDataFlags.bIsNewAviInfo = TRUE;
          sthdmirx_CORE_select_pixel_clk_out_domain(pHandle);
          HDMI_PRINT
          ("sthdmirx_video_HW_pixel_repeatfactor_get PR got changed\n");

        }

      stColorSpace = sthdmirx_video_HW_color_space_get(pHandle);
      if (stColorSpace != (stm_hdmirx_color_format_t)
          pInpHandle->stInfoPacketData.stAviInfo.Y)
        {
          pInpHandle->stInfoPacketData.stAviInfo.Y = stColorSpace;
          pInpHandle->stNewDataFlags.bIsNewAviInfo = TRUE;
          HDMI_PRINT
          ("sthdmirx_video_HW_color_space_get Color space got changed\n");

        }
      /*check the double clock mode & pixel repeat factor; */
      tSubSamplerMode = pInpHandle->HdmiSubsamplerMode;
      if ((pInpHandle->sMeasCtrl.mStatus & SIG_STS_DOUBLE_CLK_MODE_PRESENT) &&
          (stPixelRepeat != STM_HDMIRX_PIXEL_REPEAT_FACTOR_2))
        {
          /* something is wrong, so we need to adjust the sub sampler */
          HDMI_PRINT
          ("Pixel Repeat factor Error! Need to correct subsampler\n");
          sthdmirx_video_subsampler_setup(pHandle, HDMI_SUBSAMPLER_DIV_2);
          if (!(tSubSamplerMode == HDMI_SUBSAMPLER_DIV_2))
            {
              pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
            }
        }
      else
        {
          /* program the auto mode, */
          sthdmirx_video_subsampler_setup(pHandle, HDMI_SUBSAMPLER_AUTO);
        }

    }

  if (pInpHandle->stDataAvblFlags.bIsGcpInfoAvbl == TRUE)
    {
      U8 stColorDepth;
      stColorDepth = sthdmirx_video_HW_color_depth_get(pHandle);
      if (stColorDepth != pInpHandle->stInfoPacketData.eColorDepth)
        {
          pInpHandle->stInfoPacketData.eColorDepth = stColorDepth;
          pInpHandle->stNewDataFlags.bIsNewAviInfo = TRUE;
          HDMI_PRINT
          ("sthdmirx_video_HW_color_depth_get Color Depth got changed:%d\n",
           stColorDepth);
        }
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_DVI_signal_process_handler
 USAGE        :     DVI signal processing handler
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_CORE_DVI_signal_process_handler(hdmirx_handle_t const pHandle)
{
  hdmirx_route_handle_t *pInpHandle;
  sthdmirx_sub_sampler_mode_t subsamplemode;
  sthdmirx_sub_sampler_mode_t tSubSamplerMode;

  pInpHandle = (hdmirx_route_handle_t *) pHandle;

  if (pInpHandle->HwInputSigType != HDRX_INPUT_SIGNAL_DVI)
    {
      /* Reset Audio Block */
      /* Reset the color space */
      /* reset the color depth */
      pInpHandle->HwInputSigType = HDRX_INPUT_SIGNAL_DVI;
    }

  /* Need to check, pixel clock out uses sub sampler or VDDS */
  /* check the config, which clk is used for pixel clock output ( VCLK or subsampler clock) */

  /*Program the subsampler clock division value */
  tSubSamplerMode = pInpHandle->HdmiSubsamplerMode;
  if (pInpHandle->sMeasCtrl.mStatus & SIG_STS_DOUBLE_CLK_MODE_PRESENT)
    {
      subsamplemode = HDMI_SUBSAMPLER_DIV_2;
    }
  else
    {
      subsamplemode = HDMI_SUBSAMPLER_DIV_1;
    }

  sthdmirx_video_subsampler_setup(pHandle, subsamplemode);
  if ((tSubSamplerMode != HDMI_SUBSAMPLER_AUTO) && !(tSubSamplerMode == subsamplemode))
    {
      pInpHandle->HdrxState = HDRX_FSM_STATE_UNSTABLE_TIMING;
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_CORE_select_pixel_clk_out_domain
 USAGE        :     Normally Pixel Clock Out runs in VDDS clock domain, if Pixel is repeated, switch to subsample clock domain.
 INPUT        :     None
 RETURN       :     None
 USED_REGS    :     None
******************************************************************************/
BOOL sthdmirx_CORE_select_pixel_clk_out_domain(const hdmirx_handle_t pHandle)
{
  if (((sthdmirx_video_HW_pixel_repeatfactor_get(pHandle) >
        STM_HDMIRX_PIXEL_REPEAT_FACTOR_1) &&
       (sthdmirx_CORE_is_HDMI_signal(pHandle) == TRUE)) ||
      ((((hdmirx_route_handle_t *) pHandle)->sMeasCtrl.mStatus &
        SIG_STS_DOUBLE_CLK_MODE_PRESENT)
       && (sthdmirx_video_HW_pixel_repeatfactor_get(pHandle) !=
           STM_HDMIRX_PIXEL_REPEAT_FACTOR_2)))
    {
      /*Config the pixel output clock in SubSampleClockDomain */
      sthdmirx_CORE_config_clk(pHandle, HDMIRX_PIX_CLK_OUT_SEL,
                               HDMIRX_SUB_SAMPLER_CLK, FALSE);
    }
  else
    {
      /*Config the pixel output clock in VDDS Clock Domain */
      sthdmirx_CORE_config_clk(pHandle, HDMIRX_PIX_CLK_OUT_SEL,
                               HDMIRX_VCLK, FALSE);
    }

  return TRUE;
}

/* End of file */
