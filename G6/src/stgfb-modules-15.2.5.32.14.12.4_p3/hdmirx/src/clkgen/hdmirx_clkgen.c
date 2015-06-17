/***********************************************************************
 *
 * File: hdmirx/src/clkgen/hdmirx_clkgen.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifdef STHDMIRX_CLOCK_GEN_ENABLE
/* Standard Includes --------------------------------------------------------*/
#include <stm_hdmirx_os.h>
/* Include of Other module interface headers --------------------------------*/

/* Local Includes -----------------------------------------------------------*/
#include <hdmirx_RegOffsets.h>
#include <hdmirx_clkgenreg.h>
#include <InternalTypes.h>
#include <hdmirx_clkgen.h>
#include <hdmirx_drv.h>

/* Private Typedef ----------------------------------------------------------*/

/* Private Defines ----------------------------------------------------------*/
//#define ENABLE_DBG_CLKGEN_INFO

#ifdef ENABLE_DBG_CLKGEN_INFO
#define DBG_CLKGEN_INFO  HDMI_PRINT
#else
#define DBG_CLKGEN_INFO(a,b...)
#endif

#define     RCLK_FREQ_HZ                    360000000UL /*360 Mhz */
#define     DDS_WAIT_STABLE_INTERATIONS     100 /* No of times, tracking error is check */
#define     AVDDS_TRK_FILTER                0x03  /* No of times, tracking is within limits */
#define     AVDDS_TRK_ERR_LMT               0x0E  /* tracking error limit */
#define     TEN_PERCENT                     0x1999  /* 0.1 * 65536 */
#define     TWENTY_PERCENT                  0x3333  /* 0.2 * 65536 */
#define     THRESHOLD_PERCENT               TEN_PERCENT
#define     DEFAULT_THRESHOLD               0x0E
/* Private macro's ---------------------------------------------------------------*/
#define     GET_ADDRS(ulClkGenBaseAddrs,RegOffset)       ((U32)(ulClkGenBaseAddrs+RegOffset))
/* Private Variables -------------------------------------------------------------*/
static stm_hdmirx_semaphore *ClkGenAccessControl_p = NULL;
/* Private functions prototypes ----------------------------------------------------*/
void sthdmirx_clkgen_initIFM(U32 ulBaseAddrs);
BOOL sthdmirx_clkgen_DDS_wait_stable(sthdmirx_AVDDStypes_t estAVdds,
                                     U32 ulBaseAddrs);
void sthdmirx_clkgen_out_clk_config(sthdmirx_AVDDStypes_t estAVdds,
                                    sthdmirx_input_clkSource_t estInpClkSrc,U32 ulBaseAddrs);
void sthdmirx_clkgen_initial_dds_setup(sthdmirx_AVDDStypes_t estAVdds,
                                       U32 ulBaseAddrs);
BOOL sthdmirx_clkgen_DDS_init_freqset(sthdmirx_AVDDStypes_t estAVdds,
                                      U32 ulDdsInitFreq, U32 ulBaseAddrs);
/* Interface procedures/functions --------------------------------------------*/

/******************************************************************************
 FUNCTION     :   sthdmirx_clkgen_init
 USAGE        :   Initialize the Clock generator module for HDMIRx IP
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_init(U32 ulBaseAddrs, const void *Handle)
{
  hdmirx_route_handle_t *RouteHandle = (hdmirx_route_handle_t *) Handle;

  /* don't change the programming sequence */

  DBG_CLKGEN_INFO("Clock Gen Reg Base Address:0x%x\n", ulBaseAddrs);

  /* RCLFORM_CFG, BIT1=0, 30 MHz Frequency is selected */
  HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, RCLKFORM_CFG), 0x2f1);

  /*Power On all DDS blocks */

  // HDMI_SET_REG_BITS_DWORD(GET_ADDRS(ulBaseAddrs,FC_PD_CTRL),(0x01|FC_PD_CTRL_AVDDS3_EN_MSK|FC_PD_CTRL_AVDDS4_EN_MSK));
  HDMI_SET_REG_BITS_DWORD(GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),
                          ((0x1) |
                           (0x1 <<(RouteHandle->stDdsConfigInfo.estAudDds +FC_PD_CTRL_AVDDS_EN_SHIT)) |
                           (0x1 <<(RouteHandle->stDdsConfigInfo.estVidDds +FC_PD_CTRL_AVDDS_EN_SHIT))));

  /* configure the IFM clocks */
  sthdmirx_clkgen_initIFM(ulBaseAddrs);

  /* config the ouput clock */

  sthdmirx_clkgen_out_clk_config(RouteHandle->stDdsConfigInfo.estVidDds,
                                 SEL_INPUT_CLOCK_FROM_DDS, ulBaseAddrs);
  sthdmirx_clkgen_out_clk_config(RouteHandle->stDdsConfigInfo.estAudDds,
                                 SEL_INPUT_CLOCK_FROM_DDS, ulBaseAddrs);

  /* DDS Units Initialization */
  sthdmirx_clkgen_initial_dds_setup(
    RouteHandle->stDdsConfigInfo.estVidDds, ulBaseAddrs);
  sthdmirx_clkgen_initial_dds_setup(
    RouteHandle->stDdsConfigInfo.estAudDds, ulBaseAddrs);

  /*Power Down The DDS Modules */
  HDMI_CLEAR_REG_BITS_DWORD(GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),
                            ((0x1 <<(RouteHandle->stDdsConfigInfo.estAudDds +FC_PD_CTRL_AVDDS_EN_SHIT)) |
                             (0x1 <<(RouteHandle->stDdsConfigInfo.estVidDds +FC_PD_CTRL_AVDDS_EN_SHIT))));

  /* Initialise the clk-gen call protection semaphore */
  if(! ClkGenAccessControl_p)
    stm_hdmirx_sema_init(&ClkGenAccessControl_p, 1);

  DBG_CLKGEN_INFO("Clock Gen Programming is done!!\n");
}

/******************************************************************************
 FUNCTION     :   sthdmirx_clkgen_term
 USAGE        :   term the Clock generator module for HDMIRx IP
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_term(void)
{
  /* Delete the clk-gen call protection semaphore */
  stm_hdmirx_sema_delete(ClkGenAccessControl_p);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_initial_dds_setup
 USAGE        :     Initial setup of DDS unit.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_initial_dds_setup(sthdmirx_AVDDStypes_t estAVdds,
                                       U32 ulBaseAddrs)
{
  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS1_CONTROL1),0x00);
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS1_CONTROL0),AVDDS1_K_MAIN_MASK,(4 << AVDDS1_K_MAIN_SHIFT));
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS1_CONTROL0),AVDDS1_K_DIFF_MASK,(4 << AVDDS1_K_DIFF_SHIFT));
      break;
    case HDMIRX_AUD_AVDDS2:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS2_CONTROL1),0x02);
      //HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs,AVDDS2_CONTROL1),0x00);

      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS2_CONTROL0),AVDDS2_K_MAIN_MASK,(4 << AVDDS2_K_MAIN_SHIFT));
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS2_CONTROL0),AVDDS2_K_DIFF_MASK,(4 << AVDDS2_K_DIFF_SHIFT));
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS3_CONTROL1),0x00);
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS3_CONTROL0),AVDDS3_K_MAIN_MASK,(4 << AVDDS3_K_MAIN_SHIFT));
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS3_CONTROL0),AVDDS3_K_DIFF_MASK,(4 << AVDDS3_K_DIFF_SHIFT));
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS4_CONTROL1),0x02);
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS4_CONTROL0),AVDDS4_K_MAIN_MASK,(4 << AVDDS4_K_MAIN_SHIFT));
      HDMI_CLEAR_AND_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs,AVDDS4_CONTROL0),AVDDS4_K_DIFF_MASK,(4 << AVDDS4_K_DIFF_SHIFT));
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...Not supported\n");
      break;
    }
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_initIFM
 USAGE        :     Initialize the IFM clock
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_initIFM(U32 ulBaseAddrs)
{
  HDMI_SET_REG_BITS_DWORD(
    GET_ADDRS(ulBaseAddrs, DSTCLK_POWER_DOWN),(POWER_DOWN_IFM_CLK_N | POWER_DOWN_TCLK_N));
  HDMI_CLEAR_REG_BITS_DWORD(
    GET_ADDRS(ulBaseAddrs, BYPASS_CLK_EN),BYPASS_EXT_IFM_CLK_EN);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_out_clk_config
 USAGE        :     Configures the Clockgen Output clock module
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_out_clk_config(sthdmirx_AVDDStypes_t estAVdds,
                                    sthdmirx_input_clkSource_t estInpClkSrc,U32 ulBaseAddrs)
{
  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:

      if (estInpClkSrc == SEL_INPUT_CLOCK_FROM_TMDS_LINK)
        {
          HDMI_SET_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI_PIX_DVI_CLK_SEL);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI_PIX_DVI_CLK_SEL);
        }
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, BYPASS_CLK_EN),BYPASS_EXT_HDMI_PIX_CLK_EN);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, DSTCLK_POWER_DOWN),POWER_DOWN_HDMI_PIX_CLK_N);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, SELECT_CLK_FROM_DDS),SEL_HDMI_PIX_AVDDS1);
      break;

    case HDMIRX_AUD_AVDDS2:
      if (estInpClkSrc == SEL_INPUT_CLOCK_FROM_TMDS_LINK)
        {
          HDMI_SET_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI_AUD_DVI_CLK_SEL);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI_AUD_DVI_CLK_SEL);
        }
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, BYPASS_CLK_EN),BYPASS_EXT_HDMI_AUD_CLK_EN);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, DSTCLK_POWER_DOWN),POWER_DOWN_HDMI_AUD_CLK_N);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, SELECT_CLK_FROM_DDS),SEL_HDMI_AUD_AVDDS2);
      break;
    case HDMIRX_PIX_AVDDS3:
      if (estInpClkSrc == SEL_INPUT_CLOCK_FROM_TMDS_LINK)
        {
          HDMI_SET_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI2_PIX_DVI_CLK_SEL);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI2_PIX_DVI_CLK_SEL);
        }
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, BYPASS_CLK_EN),BYPASS_EXT_HDMI2_PIX_CLK_EN);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, DSTCLK_POWER_DOWN),POWER_DOWN_HDMI2_PIX_CLK_N);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, SELECT_CLK_FROM_DDS),SEL_HDMI2_PIX_AVDDS1);
      break;
    case HDMIRX_AUD_AVDDS4:
      if (estInpClkSrc == SEL_INPUT_CLOCK_FROM_TMDS_LINK)
        {
          HDMI_SET_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI2_AUD_DVI_CLK_SEL);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_DWORD(
            GET_ADDRS(ulBaseAddrs, PHY_CLK_SEL),HDMI2_AUD_DVI_CLK_SEL);
        }
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, BYPASS_CLK_EN),BYPASS_EXT_HDMI2_AUD_CLK_EN);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, DSTCLK_POWER_DOWN),POWER_DOWN_HDMI2_AUD_CLK_N);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, SELECT_CLK_FROM_DDS),SEL_HDMI2_AUD_AVDDS2);
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...Not supported\n");
      break;
    }

}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_openloop_force
 USAGE        :     Force DDS to oprerate in open loop.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_DDS_openloop_force(sthdmirx_AVDDStypes_t estAVdds,
                                        U32 ulDdsInitFreq, U32 ulBaseAddrs)
{
  U32 tGetFreq;

  stm_hdmirx_sema_wait(ClkGenAccessControl_p);

  DBG_CLKGEN_INFO(" Desired Open Loop Freq:%ld\n", ulDdsInitFreq);
  /* set the open loop frequency */
  sthdmirx_clkgen_DDS_init_freqset(estAVdds, ulDdsInitFreq, ulBaseAddrs);

  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS1_CONTROL0),AVDDS1_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS1_INIT),AVDDS1_INIT_TRIGGER);
      break;
    case HDMIRX_AUD_AVDDS2:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS2_CONTROL0),AVDDS2_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS2_INIT),AVDDS2_INIT_TRIGGER);
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS3_CONTROL0),AVDDS3_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS3_INIT),AVDDS3_INIT_TRIGGER);
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS4_CONTROL0),AVDDS4_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS4_INIT),AVDDS4_INIT_TRIGGER);
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...Not supported\n");
      break;
    }
  tGetFreq = sthdmirx_clkgen_DDS_current_freq_get(estAVdds, ulBaseAddrs);
  HDMI_PRINT("Open Loop:   DDS No :%d  Freq:%d.%02d MHz\n", estAVdds,
             tGetFreq / 10, tGetFreq % 10);

  stm_hdmirx_sema_signal(ClkGenAccessControl_p);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_closeloop_force
 USAGE        :     Force DDS to oprerate in close loop
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_clkgen_DDS_closeloop_force(sthdmirx_AVDDStypes_t estAVdds,
    U32 ulDdsInitFreq, U32 ulBaseAddrs)
{
  U32 ulDeltaRegOffset;
  U32 tGetFreq;
  BOOL uLockStaus = TRUE;

  stm_hdmirx_sema_wait(ClkGenAccessControl_p);

  DBG_CLKGEN_INFO(" Desired Close Loop Freq:%ld\n", ulDdsInitFreq);

  if (ulDdsInitFreq)
    {
      /* set the Init Frequency so DDS will start Tracking from this freq, try to program the desired DDS freq, it locks the DDS faster */
      sthdmirx_clkgen_DDS_init_freqset(estAVdds, ulDdsInitFreq,ulBaseAddrs);
    }

  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS1_FREQ_DELTA),0x00);
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS1_CONTROL0),AVDDS1_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS1_INIT),AVDDS1_INIT_TRIGGER);
      ulDeltaRegOffset = AVDDS1_FREQ_DELTA;

      break;
    case HDMIRX_AUD_AVDDS2:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS2_FREQ_DELTA),0x00);
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS2_CONTROL0),AVDDS2_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS2_INIT),AVDDS2_INIT_TRIGGER);
      ulDeltaRegOffset = AVDDS2_FREQ_DELTA;
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS3_FREQ_DELTA),0x00);
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS3_CONTROL0),AVDDS3_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS3_INIT),AVDDS3_INIT_TRIGGER);
      ulDeltaRegOffset = AVDDS3_FREQ_DELTA;
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS4_FREQ_DELTA),0x00);
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS4_CONTROL0),AVDDS4_FORCE_OPLOOP);
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS4_INIT),AVDDS4_INIT_TRIGGER);
      ulDeltaRegOffset = AVDDS4_FREQ_DELTA;
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...Not supported\n");
      stm_hdmirx_sema_signal(ClkGenAccessControl_p);
      return FALSE;
    }

  /* DDS Tracking Error */
  if (sthdmirx_clkgen_DDS_wait_stable(estAVdds, ulBaseAddrs))
    {
      /* DDS is stable, Put the 10% of Current Freq Threshold */

      U16 ulFreqThreshold;
      ulFreqThreshold =
        (U16) (((U16)(sthdmirx_clkgen_DDS_current_freq_get(
                        estAVdds,ulBaseAddrs) >> 14) * THRESHOLD_PERCENT) >> 16);
      HDMI_WRITE_REG_DWORD(
        GET_ADDRS(ulBaseAddrs, ulDeltaRegOffset),/*DEFAULT_THRESHOLD */ ulFreqThreshold);
      tGetFreq =sthdmirx_clkgen_DDS_current_freq_get(estAVdds, ulBaseAddrs);
      HDMI_PRINT("Close Loop:   DDS No :%d   Freq:%d.%02d MHz\n",
                 estAVdds, tGetFreq / 10, tGetFreq % 10);
    }
  else
    {
      HDMI_PRINT("DDS is not locked properly....estAVdds:%d\n",estAVdds);
      uLockStaus = FALSE;
    }

  stm_hdmirx_sema_signal(ClkGenAccessControl_p);
  return uLockStaus;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_init_freqset
 USAGE        :     Set the Dds Init Frequency.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_clkgen_DDS_init_freqset(sthdmirx_AVDDStypes_t estAVdds,
                                      U32 ulDdsInitFreq, U32 ulBaseAddrs)
{
  U32 uldivider = RCLK_FREQ_HZ;
  U32 ulClockRatio = 0;
  U8 ushift = 24;

  if ((ulDdsInitFreq == 0) || (ulDdsInitFreq > RCLK_FREQ_HZ))
    {
      DBG_CLKGEN_INFO
      ("DddsInitFreq is greater than Reference Clock RCLK\n", 0);
      return FALSE;
    }

  /*D_Ratio = (ulDdsInitFreq << 24} / RCLK_FREQ_HZ */

  while (ushift--)
    {
      uldivider >>= 1;
      ulClockRatio <<= 1;
      if (ulDdsInitFreq > uldivider)
        {
          ulDdsInitFreq -= uldivider;
          ulClockRatio |= 1;
        }
    }
  /*round */
  if (ulDdsInitFreq > (uldivider >> 1))
    {
      ulClockRatio++;
    }
//    DBG_CLKGEN_INFO(" DDS No : %d    InitFreq:0x%x\n",estAVdds,ulClockRatio);

  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_WRITE_REG_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS1_INIT_FREQ),ulClockRatio);
      break;
    case HDMIRX_AUD_AVDDS2:
      HDMI_WRITE_REG_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS2_INIT_FREQ),ulClockRatio);
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_WRITE_REG_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS3_INIT_FREQ),ulClockRatio);
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_WRITE_REG_DWORD(
        GET_ADDRS(ulBaseAddrs, AVDDS4_INIT_FREQ),ulClockRatio);
      break;
    default:
      DBG_CLKGEN_INFO
      ("Selected Clk is not supported, unable to set InitFreq\n");
      break;
    }

  return TRUE;
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_current_freq_get
 USAGE        :     Get the Current Digital PLL frequency.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
U32 sthdmirx_clkgen_DDS_current_freq_get(sthdmirx_AVDDStypes_t estAVdds,
    U32 ulBaseAddrs)
{
  U32 ulCurrentRatio = 0;
  U32 ulDdsOpFreq;

  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS1_CUR_FREQ), 0xfffffff); /*writing just lock the register */
      ulCurrentRatio =
        HDMI_READ_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS1_CUR_FREQ));
      break;
    case HDMIRX_AUD_AVDDS2:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS2_CUR_FREQ), 0xffff);  /*writing just lock the register */
      ulCurrentRatio =
        HDMI_READ_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS2_CUR_FREQ));
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS3_CUR_FREQ), 0xffff);  /*writing just lock the register */
      ulCurrentRatio =
        HDMI_READ_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS3_CUR_FREQ));
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_WRITE_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS4_CUR_FREQ), 0xffff);  /*writing just lock the register */
      ulCurrentRatio =
        HDMI_READ_REG_DWORD(GET_ADDRS(ulBaseAddrs, AVDDS4_CUR_FREQ));
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...not supported\n");
      break;
    }

  DBG_CLKGEN_INFO("Get Current Freq Value :0x%x\n", ulCurrentRatio);
  ulDdsOpFreq =
    (U32) ((((ulCurrentRatio >> 12) *((U16) (RCLK_FREQ_HZ / 100000L))) + (1 << 12)) >> 12);

  return (ulDdsOpFreq);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_tracking_error_get
 USAGE        :     Get the Dds Tracking Error- Servo Error.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
U16 sthdmirx_clkgen_DDS_tracking_error_get(sthdmirx_AVDDStypes_t estAVdds,
    U32 ulBaseAddrs)
{
  U32 RegOffset;

  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      RegOffset = AVDDS1_TRACK_ERR;
      break;
    case HDMIRX_AUD_AVDDS2:
      RegOffset = AVDDS2_TRACK_ERR;
      break;
    case HDMIRX_PIX_AVDDS3:
      RegOffset = AVDDS3_TRACK_ERR;
      break;
    case HDMIRX_AUD_AVDDS4:
      RegOffset = AVDDS4_TRACK_ERR;
      break;

    default:
      DBG_CLKGEN_INFO("Future clock option...not supported\n");
      return 0;
    }

  return (U16) HDMI_READ_REG_DWORD(GET_ADDRS(ulBaseAddrs, RegOffset));
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_DDS_wait_stable
 USAGE        :     Wait to get DDS locked to desired operation frequency.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
BOOL sthdmirx_clkgen_DDS_wait_stable(sthdmirx_AVDDStypes_t estAVdds,
                                     U32 ulBaseAddrs)
{
  U8 uFilter = 0;
  U16 ulTrackErr;
  U8 uNoOfIterations = 0;

  while ((uNoOfIterations < DDS_WAIT_STABLE_INTERATIONS)
         && (uFilter < AVDDS_TRK_FILTER))
    {
      /* Delay to allow update between iterations  (should be at least 2 horizontal scan lines) */
      STHDMIRX_DELAY_1ms(1);

      /* Get Accumulated Tracking Error (Filter for transients) */
      ulTrackErr =sthdmirx_clkgen_DDS_tracking_error_get(estAVdds,ulBaseAddrs);

      if (ulTrackErr <= AVDDS_TRK_ERR_LMT)
        ++uFilter;
      else
        uFilter = 0;

      ++uNoOfIterations;
    }

  /* DDS Stable */
  if (uFilter >= AVDDS_TRK_FILTER)
    return TRUE;

  return FALSE;

}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_powerdownDDS
 USAGE        :     Power Down the DDS Modules.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_powerdownDDS(sthdmirx_AVDDStypes_t estAVdds,U32 ulBaseAddrs)
{
  stm_hdmirx_sema_wait(ClkGenAccessControl_p);
  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS1_EN_MSK);
      break;

    case HDMIRX_AUD_AVDDS2:
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS2_EN_MSK);
      break;
    case HDMIRX_PIX_AVDDS3:
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS3_EN_MSK);
      break;
    case HDMIRX_AUD_AVDDS4:
      HDMI_CLEAR_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS4_EN_MSK);
      break;

    default:
      DBG_CLKGEN_INFO("sthdmirx_clkgen_powerdownDDS...Not supported\n");
      break;
    }

  stm_hdmirx_sema_signal(ClkGenAccessControl_p);
}

/******************************************************************************
 FUNCTION     :     sthdmirx_clkgen_powerupDDS
 USAGE        :     PowerUp the DDS Modules.
 INPUT        :
 RETURN       :
 USED_REGS    :
******************************************************************************/
void sthdmirx_clkgen_powerupDDS(sthdmirx_AVDDStypes_t estAVdds, U32 ulBaseAddrs)
{
  stm_hdmirx_sema_wait(ClkGenAccessControl_p);
  switch (estAVdds)
    {
    case HDMIRX_PIX_AVDDS1:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS1_EN_MSK);
      break;

    case HDMIRX_AUD_AVDDS2:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS2_EN_MSK);
      break;

    case HDMIRX_PIX_AVDDS3:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS3_EN_MSK);
      break;

    case HDMIRX_AUD_AVDDS4:
      HDMI_SET_REG_BITS_DWORD(
        GET_ADDRS(ulBaseAddrs, FC_PD_CTRL),FC_PD_CTRL_AVDDS4_EN_MSK);
      break;

    default:
      DBG_CLKGEN_INFO("sthdmirx_clkgen_powerdownDDS...Not supported\n");
      break;
    }
  stm_hdmirx_sema_signal(ClkGenAccessControl_p);
}

/********************************End of file*********************************/
#endif /*STHDMIRX_CLOCK_GEN_ENABLE */
/* End of file */
