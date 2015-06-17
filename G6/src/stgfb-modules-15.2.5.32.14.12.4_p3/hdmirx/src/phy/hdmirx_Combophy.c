/***********************************************************************
 *
 * File: hdmirx/src/phy/hdmirx_Combophy.c
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

#include <InternalTypes.h>
#include <hdmirx_Combophy.h>
#include <hdmirx_drv.h>
#include <hdmirx_RegOffsets.h>

/* Private Typedef -----------------------------------------------*/
#define INPUTCLK_LOW_FREQ_RANGE     42000UL /* 0 ~ 42MHz - Low Mode,  42MHz<=Fh<102MHz - Mid Mode, else High Mode */
#define INPUTCLK_HIGH_FREQ_RANGE    102000UL
#define CPHY_INPUTCLK_TRK_FREQ_XOVER_RANGE 160000UL

#define LOWGAIN_EQUALIZATION_DVIEQ          0x5
#define LOWGAIN_EQUALIZATION_DVIGAIN        0

#define MIDGAIN_EQUALIZATION_DVIEQ          0xE
#define MIDGAIN_EQUALIZATION_DVIGAIN        1

#define HIGHGAIN_EQUALIZATION_DVIEQ         0x5
#define HIGHGAIN_EQUALIZATION_DVIGAIN       2

/* Private macro's ------------------------------------------------*/

//#define ENABLE_HDRX_PHY_DBG
#ifdef ENABLE_HDRX_PHY_DBG
#define DBG_HDRX_PHY_INFO	HDMI_PRINT
#else
#define DBG_HDRX_PHY_INFO(a,b...)
#endif

/* Private Variables -----------------------------------------------*/

/* Private functions prototypes ------------------------------------*/
void sthdmirx_PHY_set_Channel_CPTIN(sthdmirx_PHY_context_t *PhyControl_p , U8 Phy_lane, U8 CPTIN_value);
U16 sthdmirx_PHY_get_channel_decoder_error(sthdmirx_PHY_context_t *PhyControl_p,
                                        U8 uDurationMsec, U8 Channel, U16 ErrMaskLevel);
/* Interface procedures/functions ----------------------------------*/

/******************************************************************************
 FUNCTION     :   sthdmirx_PHY_PLL_setup
 USAGE        :   PLL VCP & Word assembler is programmed as per input clock frq
 INPUT        :   Input Link clock Frequency
 RETURN       :   None
 USED_REGS    :   HDMI_RX_PHY_PLL & HDMI_RX_PHY_ACTRL
******************************************************************************/
void sthdmirx_PHY_PLL_setup(sthdmirx_PHY_context_t *PhyControl_p,
                            U32 LinkClkFreq, BOOL IsPLLInit)
{

  /* Initilialize the PLL in High mode- x2.5, this is done if clock changes,clock loss */
  U32 control_clk, control_ln, spd, EqValue = 0, EqValueClk = 0;
  U8 CPTIN_value, Phy_Channel ;
  BOOL Tracking_mode_sw= FALSE;

  if (IsPLLInit)
    {
      LinkClkFreq = CPHY_INPUTCLK_TRK_FREQ_XOVER_RANGE;
    }

  {
    /* Hold Phy in reset until configured */
    HDMI_SET_REG_BITS_BYTE(
      (U32)(PhyControl_p->BaseAddress_p +CPHY_PHY_CTRL),
      CPHY_L0_RESET | CPHY_L1_RESET |CPHY_L2_RESET | CPHY_L3_RESET);
    /* DP_BUS WIDTH is '0' for OVS mode by default */
    HDMI_CLEAR_REG_BITS_DWORD(
      (U32)(PhyControl_p->BaseAddress_p +CPHY_PHY_CTRL), CPHY_DP_BUS_WIDTH);

    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L0_CTRL_4), CPHY_L0_LUDO_TEST );
    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4), CPHY_L0_LUDO_TEST );
    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4), CPHY_L0_LUDO_TEST );
    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4), CPHY_L0_LUDO_TEST );
    /* Default Config */
    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_PHY_CTRL), CPHY_TMDS_MODE | CPHY_SPD_SEL_OW); // PLL in TMDS MODE

#ifdef ST_ORLY2
    /*Adding new init for Orly2 and ONLY for Orly2*/
    HDMI_SET_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_PHY_CTRL), CPHY_RCVRDATA_BITS_SWAP |CPHY_RCVRDATA_POLARITY | CPHY_SPD_SEL_OW);  // CPHY_RCVRDATA_BITS_SWAP=1, CPHY_RCVRDATA_POLARITY=1, CPHY_SPD_SEL_OW=1
#endif

    /*Default config*/
    HDMI_CLEAR_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L0_CTRL_4), CPHY_L0_PDEN_TEST);
    HDMI_CLEAR_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4), CPHY_L1_PDEN_TEST);
    HDMI_CLEAR_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4), CPHY_L2_PDEN_TEST);
    HDMI_CLEAR_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4), CPHY_L3_PDEN_TEST);

    HDMI_SET_REG_BITS_DWORD(
      (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_4),
      (CPHY_L0_CAL_CNTL |CPHY_L0_SEL_DATA_OW |
       CPHY_L0_SEL_DATA_TEST | CPHY_L0_PDEN_OW | CPHY_L0_LUDO_OW));
    HDMI_SET_REG_BITS_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4),
      (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW |
       CPHY_L0_EQ_FPEAK | CPHY_L0_LUDO_OW));
    HDMI_SET_REG_BITS_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4),
      (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW|
       CPHY_L0_EQ_FPEAK | CPHY_L0_LUDO_OW));
    HDMI_SET_REG_BITS_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4),
      (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW |
       CPHY_L0_EQ_FPEAK | CPHY_L0_LUDO_OW));

    /*
     * TO DO : use vibe_os_get_chip_version(mag,min)
     * to determine cannes chip version, replace STHDMIRX_WA_VCore_1V.
     */
    if(STHDMIRX_WA_VCore_1V==1)
      /*modif for Cannes adjust the bias */
      HDMI_WRITE_REG_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_RSVRBITS), 0x3);
    else
      /*modif for Cannes adjust the bias */
      HDMI_WRITE_REG_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_RSVRBITS), 0x1);

    if (LinkClkFreq < INPUTCLK_LOW_FREQ_RANGE)  	// Low Freq band
      {
        HDMI_WRITE_REG_TRI_BYTES(
          (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CTRL),
          0x000000UL);
        spd = 0x000000UL;
      }
    else if (LinkClkFreq < INPUTCLK_HIGH_FREQ_RANGE)  	// Mid Freq band
      {
        HDMI_WRITE_REG_TRI_BYTES(
          (U32)(PhyControl_p->DeviceBaseAddress +  HDMI_RX_PHY_CTRL),
          0x000004UL);
        spd = 0x000001UL;
      }
    else if (LinkClkFreq < CPHY_INPUTCLK_TRK_FREQ_XOVER_RANGE)  	// HighFreq Band - OVS
      {
        HDMI_WRITE_REG_TRI_BYTES(
          (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CTRL),
          0x000008UL);
        spd = 0x000002UL;
      }
    else  	// HighFreq Band - Tracking
      {
        HDMI_WRITE_REG_TRI_BYTES(
          (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CTRL),
          0x00000CUL);
        spd = 0x000003UL;
        HDMI_SET_REG_BITS_DWORD(
          (U32)(PhyControl_p->BaseAddress_p + CPHY_PHY_CTRL),
          CPHY_DP_BUS_WIDTH);
      }

    EqValue = HDMI_READ_REG_DWORD(
                (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_0)) & HDMI_RX_PHY_EQ_GAIN;
    EqValueClk = HDMI_READ_REG_DWORD(
                   (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_0)) &
                 HDMI_RX_PHY_EQ_GAIN;
    control_ln = ((0x004 << CPHY_L0_ACRCY_SEL_0_SHIFT) | (0x004 << CPHY_L0_ACRCY_SEL_1_SHIFT) |
                  EqValue | (spd << CPHY_L0_SPD_SEL_SHIFT) | CPHY_L0_SWAP_CAL_DIR | (0x20 << CPHY_L0_CPTIN_SHIFT) | (0x3UL << CPHY_L0_DECR_BW_SHIFT) | CPHY_L0_DIS_TRIM_CAL );

    control_clk = ((0x004 << CPHY_L0_ACRCY_SEL_0_SHIFT) |
                   (0x004 << CPHY_L0_ACRCY_SEL_1_SHIFT) | (0x20 << CPHY_L0_CPTIN_SHIFT) | EqValueClk |
                   (spd << CPHY_L0_SPD_SEL_SHIFT) |CPHY_L0_SWAP_CAL_DIR | (0x3UL << CPHY_L0_DECR_BW_SHIFT) | CPHY_L0_DIS_TRIM_CAL );

    /*
     * TO DO : use vibe_os_get_chip_version(mag,min)
     * to determine cannes chip version, replace STHDMIRX_WA_VCore_1V.
     */
    if(STHDMIRX_WA_VCore_1V==1)
    {
      control_clk |= (0x2 << CPHY_L0_IB_CNTL_SHIFT);
      control_ln  |= (0x3 << CPHY_L0_IB_CNTL_SHIFT);
    }
    else
    {
      control_clk |= (0x1 << CPHY_L0_IB_CNTL_SHIFT);
      control_ln  |= (0x1 << CPHY_L0_IB_CNTL_SHIFT);
    }

    HDMI_WRITE_REG_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L0_CTRL_0), control_clk);
    HDMI_WRITE_REG_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_0), control_ln);
    HDMI_WRITE_REG_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_0), control_ln);
    HDMI_WRITE_REG_DWORD(
      (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_0), control_ln);

    HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L0_CTRL_4), CPHY_L0_LUDO_TEST );
    HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4), CPHY_L1_LUDO_TEST );
    HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4), CPHY_L2_LUDO_TEST );
    HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4), CPHY_L3_LUDO_TEST );

    /* Release lane resets and allow PLLs to lock */
    HDMI_CLEAR_REG_BITS_BYTE(
      (U32)(PhyControl_p->BaseAddress_p +CPHY_PHY_CTRL),
      CPHY_L0_RESET | CPHY_L1_RESET | CPHY_L2_RESET | CPHY_L3_RESET);

    STHDMIRX_DELAY_1ms(2);

    /*Treatment for 4k2k*/
    if (LinkClkFreq > CPHY_INPUTCLK_TRK_FREQ_XOVER_RANGE)
    {
      if( Tracking_mode_sw == TRUE)
      {
        /*Prog PDEN to 1 on Lx*/
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4), CPHY_L1_PDEN_TEST);
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4), CPHY_L2_PDEN_TEST);
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4), CPHY_L3_PDEN_TEST);

        STHDMIRX_DELAY_1ms(2);

        for (Phy_Channel=0; Phy_Channel<3;Phy_Channel++)
        {
          for (CPTIN_value=0x20; CPTIN_value<0x40; CPTIN_value++)
          {
            sthdmirx_PHY_set_Channel_CPTIN(PhyControl_p, Phy_Channel, CPTIN_value);
            STHDMIRX_DELAY_1ms(1);
            if(sthdmirx_PHY_get_channel_decoder_error(PhyControl_p, 1,Phy_Channel,0)==0)
              break;
          }
          if( CPTIN_value==0x40)
            sthdmirx_PHY_set_Channel_CPTIN(PhyControl_p, Phy_Channel, 0x20);
        }
      }
      else
      {
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4), CPHY_L1_PDEN_TEST);
        HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_0), CPHY_L1_DIS_TRIM_CAL );
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4), CPHY_L2_PDEN_TEST);
        HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_0), CPHY_L2_DIS_TRIM_CAL );
        HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4), CPHY_L3_PDEN_TEST);
        HDMI_CLEAR_REG_BITS_DWORD((U32) (PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_0), CPHY_L3_DIS_TRIM_CAL );

        STHDMIRX_DELAY_1ms(2);
      }
    }

    /* Clear CPT lock CRO status */
    HDMI_SET_REG_BITS_BYTE(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CPTRIM),
        (CPHY_L0_CPTOUT | CPHY_L0_OUT_OF_RANGE));
    HDMI_SET_REG_BITS_BYTE(
        (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CPTRIM),
        (CPHY_L0_CPTOUT | CPHY_L0_OUT_OF_RANGE));
    HDMI_SET_REG_BITS_BYTE(
        (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CPTRIM),
        (CPHY_L0_CPTOUT | CPHY_L0_OUT_OF_RANGE));
    HDMI_SET_REG_BITS_BYTE(
        (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CPTRIM),
        (CPHY_L0_CPTOUT | CPHY_L0_OUT_OF_RANGE));

    // MODE CHANGE INSTABILITY FIXed using correct path configuration sequence
    // Clear the SOC FIFO after a port change as well as input clock becomes
    // stable after mode change. generating the reset will re-align
    // the read/write pointers in the MUX FIFO. Two ways to do this -
    // 1. using the dvi_mux_fifo_reset in the soft_resets_2 register
    //    to generate a reset - set and clear the bit
    // 2. using mux selection bits dvi_port_sel in host_dvi_dp_setup register
    //    to generate a reset, simply change dvi_port_sel bits and write back
    //    the original value
    //
#if 0
    // validated, works as expected
    // #define HOST_DVI_DP_SETUP   0xC864
    hdmi_port_sel = HDMI_READ_REG_DWORD(0xC864);
    HDMI_WRITE_REG_DWORD(0xC864, (hdmi_port_sel ^ 1));
    hdmi_port_sel = HDMI_READ_REG_DWORD(0xC864);
    HDMI_WRITE_REG_DWORD(0xC864, (hdmi_port_sel ^ 1));
#else
    // validated, works as expected - USE THIS [PREFERRED]
    // above could also be done using DVI_MUX_FIFO_RESET bit in SOFT_RESETS_2
    // #define SOFT_RESETS_2  0xC808
    //    HDMI_WRITE_REG_DWORD(SOFT_RESETS_2, DVI_MUX_FIFO_RESET);
    // STHDMIRX_DELAY_1ms(1);
    // HDMI_WRITE_REG_DWORD(SOFT_RESETS_2, 0x0000);
#endif
  }

  if (!IsPLLInit)
    DBG_HDRX_PHY_INFO("+!InputClkFreq:%d0 Khz\n",(U16) (LinkClkFreq / 10));

}				/*End of sthdmirx_PHY_PLL_setup() */

/******************************************************************************
 FUNCTION     : sthdmirx_MEAS_get_PHY_channel_decoder_error
 USAGE        : Get the phy decoder error status.
 INPUT        : uDurationMsec - duration for error measurement  ;ErrMaskLevel: ignore the defined error level
 RETURN       : Decoder Error
 USED_REGS    : None
******************************************************************************/
U16 sthdmirx_PHY_get_channel_decoder_error(sthdmirx_PHY_context_t *PhyControl_p,
                                        U8 uDurationMsec, U8 Channel, U16 ErrMaskLevel)
{
  U16 ulDecErr;

  /*Clear the previous Decoder Error Status */
  HDMI_SET_REG_BITS_BYTE((PhyControl_p->DeviceBaseAddress + HDRX_LINK_ERR_CTRL),
                         HDRX_PHY_DEC_ERR_CLR);
  HDMI_CLEAR_REG_BITS_BYTE((PhyControl_p->DeviceBaseAddress + HDRX_LINK_ERR_CTRL),
                           HDRX_PHY_DEC_ERR_CLR);

  /* Time Frame to check the decoder Error for a channel */
  if (uDurationMsec)
  {
    STHDMIRX_DELAY_1ms(uDurationMsec);
  }

  /* According to the channel ,  get the decoder Error */
  HDMI_WRITE_REG_BYTE((PhyControl_p->DeviceBaseAddress + HDRX_LINK_ERR_CTRL), Channel);
  ulDecErr = (U16) (HDMI_READ_REG_WORD((PhyControl_p->DeviceBaseAddress + HDRX_PHY_DEC_ERR_STATUS)) & 0x0fff);

  DBG_HDRX_PHY_INFO("\n MEAS_get_PHY_channel_decoder_error : Channel:%d, Errors: %d\n", Channel, ulDecErr );

  if (ulDecErr > ErrMaskLevel)
  {
    return (ulDecErr);
  }

  return 0;
}

/******************************************************************************
 FUNCTION     :   STHDMIRX_PHY_SetupEqGain
 USAGE        :   Adjust the frequency response & Equalizer gain
 INPUT        :   EqValue - Frquency Response, EqGain - Equalizer gain
 RETURN       :   None
 USED_REGS    :   HDMI_RX_PHY_ACTRL
******************************************************************************/
void sthdmirx_PHY_set_eq_gain(sthdmirx_PHY_context_t *PhyControl_p, U8 EqValue)
{
  U32 EqCtrl = 0;
  U32 EqClkCtrl;

  /*
   * TO DO : use vibe_os_get_chip_version(mag,min)
   * to determine cannes chip version, replace STHDMIRX_WA_VCore_1V.
   */
  if(STHDMIRX_WA_VCore_1V==1)
    EqClkCtrl = ((0x7UL << HDMI_RX_PHY_EQ_GAIN_SHIFT));
  else
    EqClkCtrl = ((0x4UL << HDMI_RX_PHY_EQ_GAIN_SHIFT));

  EqCtrl = (((U32) (EqValue & 0x07) << HDMI_RX_PHY_EQ_GAIN_SHIFT));

  HDMI_CLEAR_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_0), HDMI_RX_PHY_EQ_GAIN);
  HDMI_CLEAR_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_0), HDMI_RX_PHY_EQ_GAIN);
  HDMI_CLEAR_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_0), HDMI_RX_PHY_EQ_GAIN);

   HDMI_SET_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L0_CTRL_0), EqClkCtrl);
  HDMI_SET_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_0), EqCtrl);
  HDMI_SET_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_0), EqCtrl);
  HDMI_SET_REG_BITS_DWORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_0), EqCtrl);
}				/*End of STHDMIRX_PHY_SetupEqGain() */

/******************************************************************************
 FUNCTION     :   sthdmirx_PHY_set_pk_freq
 USAGE        :   Adjust the frequency response & Equalizer gain
 INPUT        :   EqValue - Frquency Response, EqGain - Equalizer gain
 RETURN       :   None
 USED_REGS    :   HDMI_RX_PHY_ACTRL
******************************************************************************/
void sthdmirx_PHY_set_pk_freq(sthdmirx_PHY_context_t *PhyControl_p, U8 PkValue)
{
  U32 FrqCtrl = 0;

  FrqCtrl = (((U32) (PkValue & 0x07) << HDMI_RX_PHY_EQ_PEAK_SHIFT));
  HDMI_CLEAR_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4 + 2),
    HDMI_RX_PHY_EQ_PEAK);
  HDMI_CLEAR_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4 + 2),
    HDMI_RX_PHY_EQ_PEAK);
  HDMI_CLEAR_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4 + 2),
    HDMI_RX_PHY_EQ_PEAK);

  HDMI_SET_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L1_CTRL_4 + 2), FrqCtrl);
  HDMI_SET_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4 + 2), FrqCtrl);
  HDMI_SET_REG_BITS_WORD(
    (U32)(PhyControl_p->BaseAddress_p + CPHY_L3_CTRL_4 + 2), FrqCtrl);
}				/*End of STHDMIRX_PHY_SetupEqGain() */

/******************************************************************************
 FUNCTION     :   sthdmirx_PHY_set_Channel_CPTIN
 USAGE        :   Adjust the CPTIN in tracking mode
 INPUT        :   Phy_lane: number lane , CPTIN_value
 RETURN       :   None
 USED_REGS    :   CPHY_Lx_CTRL_0
******************************************************************************/
void sthdmirx_PHY_set_Channel_CPTIN(sthdmirx_PHY_context_t *PhyControl_p , U8 Phy_lane, U8 CPTIN_value)
{
  U32 FrqCtrl = 0, Reg_Addr, Read_reg, Write_reg;

  FrqCtrl = (((U32) (CPTIN_value & 0x3F) << CPHY_L0_CPTIN_SHIFT));

  switch(Phy_lane)
  {
  case 0:
    Reg_Addr = CPHY_L1_CTRL_0;
  break;

  case 1:
    Reg_Addr = CPHY_L2_CTRL_0;
  break;

  case 2:
    Reg_Addr = CPHY_L3_CTRL_0;
  break;

  default:
    Reg_Addr = CPHY_L1_CTRL_0;
  break;
  }

  Read_reg = HDMI_READ_REG_WORD((U32)(PhyControl_p->BaseAddress_p +Reg_Addr));
  Write_reg= ( Read_reg &0xFFFFC0FF) | FrqCtrl;

  HDMI_WRITE_REG_WORD(
      (U32)(PhyControl_p->BaseAddress_p + Reg_Addr), Write_reg);

  DBG_HDRX_PHY_INFO("\n Set_Lane_CPTIN: Lane:%x ,Addr: 0x%x CPTIN_Value: %d , Write_reg:0x%x \n", Phy_lane, Reg_Addr, CPTIN_value, Write_reg );
}				/*End of sthdmirx_PHY_set_Lane_CPTIN() */

/******************************************************************************
 FUNCTION     : sthdmirx_PHY_Rterm_calibration
 USAGE        : This fn sets the termination resistance value.
 INPUT        : rTermMode ( auto or manual), rTermValue ( if manual)
 RETURN       :
 USED_REGS    : HDMI_RX_PHY_ACTRL
******************************************************************************/
void sthdmirx_PHY_Rterm_calibration(sthdmirx_PHY_context_t *PhyControl_p,
                                    sthdmirx_phy_Rterm_calibration_mode_t rTermMode, U8 rTermValue)
{
  if (RTERM_CALIBRATION_AUTO == rTermMode)
    {
      HDMI_WRITE_REG_WORD(
        ((U32) (PhyControl_p->BaseAddress_p) + CPHY_TEST_CTRL), 0x0);
      HDMI_WRITE_REG_WORD(
        ((U32) (PhyControl_p->BaseAddress_p) + CPHY_PHY_CTRL),
        ((rTermValue & 0x0F) << CPHY_RTERM_SHIFT));
      HDMI_WRITE_REG_BYTE(
        ((U32) (PhyControl_p->BaseAddress_p) + CPHY_AUX_PHY_CTRL), 0x10);
      /*wait for 100 microsec. */
      STHDMIRX_DELAY_1ms(1);
      /* print the value of Register */
      HDMI_WRITE_REG_BYTE(
        ((U32) (PhyControl_p->BaseAddress_p) + CPHY_AUX_PHY_CTRL), 0x10);
    }
  else if (RTERM_CALIBRATION_MANUAL == rTermMode)
    {
      //    HDMI_WRITE_REG_BYTE(((U32)(PhyControl_p->BaseAddress_p)+CPHY_AUX_PHY_CTRL) ,0x7);
      HDMI_WRITE_REG_WORD(
        ((U32) (PhyControl_p->BaseAddress_p) + CPHY_PHY_CTRL),
        ((rTermValue & 0x0F) << CPHY_RTERM_SHIFT));
      HDMI_WRITE_REG_BYTE(
        ((U32) (PhyControl_p->BaseAddress_p) +CPHY_TEST_CTRL),
        (rTermValue & 0x10) | 0x02);
    }

}				/*End of sthdmirx_PHY_Rterm_calibration() */

/******************************************************************************
 FUNCTION     : sthdmirx_PHY_clk_selection
 USAGE        : Sets the user clock selection
 INPUT        : Clock selection
 RETURN       :
 USED_REGS    : HDMI_RX_PHY_CLK_SEL
******************************************************************************/
void sthdmirx_PHY_clk_selection(sthdmirx_PHY_context_t *PhyControl_p,
                                sthdmirx_PHY_clk_selection_t clk)
{
  //SWENG_0524 (2): HDMI_RX_PHY_RES_CTRL ~ HDMI_RX_PHY_TST_CTRL is aligned with PC_InitParams.DeviceBaseAddress_p.
  //             It cannot use Cphy_BaseAddress as the base.
  //             Because for HdmiRx, it can be

  //Manoranjani_0524 -- Corrected
  HDMI_CLEAR_AND_SET_REG_BITS_BYTE((
                                     (U32)(PhyControl_p->DeviceBaseAddress) + HDMI_RX_PHY_CLK_SEL),
                                   HDMI_RX_PHY_LCLK_SEL, clk);
}				/*End of sthdmirx_PHY_clk_selection() */

/******************************************************************************
 FUNCTION     :     sthdmirx_PHY_clk_inverse
 USAGE        :     Enable/Disable the clock inversion
 INPUT        :     clk- select the clock, Inverse- TRUE( inverse) FALSE (Normal, No inverse)
 RETURN       :
 USED_REGS    :     HDMI_RX_PHY_CLK_SEL
******************************************************************************/
void sthdmirx_PHY_clk_inverse(sthdmirx_PHY_context_t *PhyControl_p,
                              sthdmirx_PHY_sel_clk_inverse_t clk, U8 Inverse)
{
  switch (clk)
    {
    case INVERSE_LCLK:
      if (Inverse)
        {
          HDMI_SET_REG_BITS_BYTE(
            (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CLK_SEL),
            HDMI_RX_PHY_LCLK_INV);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_BYTE(
            (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CLK_SEL),
            HDMI_RX_PHY_LCLK_INV);
        }
      break;

    case INVERSE_OVR_SAMPLE_CLK:
      if (Inverse)
        {
          HDMI_SET_REG_BITS_BYTE(
            (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CLK_SEL),
            HDMI_RX_PHY_LCLK_OVR_INV);
        }
      else
        {
          HDMI_CLEAR_REG_BITS_BYTE(
            (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CLK_SEL),
            HDMI_RX_PHY_LCLK_OVR_INV);
        }
      break;
    default:
      break;

    }
}				/*end of STHDMIRX__PHY_ClkInvere() */

/******************************************************************************
 FUNCTION     :     sthdmirx_PHY_power_mode_setup
 USAGE        :     Sets the PHY Power modes
 INPUT        :     pMode ( Active,standby or shutdown)
 RETURN       :
 USED_REGS    :     HDMI_RX_PHY_CTRL
******************************************************************************/
void sthdmirx_PHY_power_mode_setup(sthdmirx_PHY_context_t *PhyControl_p,
                                   sthdmirx_PHY_pwr_modes_t pMode)
{
  U32 VCore_setting= 0;
  switch (pMode)
    {
    case PHY_POWER_OFF:
      /* Set the PHY digital block in Power Off mode */
      HDMI_CLEAR_AND_SET_REG_BITS_WORD(
        ((U32)(PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_CTRL),
        HDMI_RX_PHY_PWR_CTRL, pMode);
      //SWENG_0524 (3): CPHY_L0_IDDQEN need to be shifted 16 bit.
      //Manoranjani_0524 -- CPHY_L0_IDDQEN is BIT19 as per register spec, so shift is not required. Please comment
      //HDMI_SET_REG_BITS_DWORD((U32)(PhyControl_p->BaseAddress_p+CPHY_L0_CTRL_0), CPHY_L0_IDDQEN);

      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_0), CPHY_L0_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L1_CTRL_0), CPHY_L1_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L2_CTRL_0), CPHY_L2_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L3_CTRL_0), CPHY_L3_IDDQEN);

      break;

    case PHY_STANDBY:
      /* Set the PHY digital block in Power Off mode */
      HDMI_CLEAR_AND_SET_REG_BITS_WORD(
        ((U32)(PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_CTRL),
        HDMI_RX_PHY_PWR_CTRL, pMode);
      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_0), CPHY_L0_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L1_CTRL_0), CPHY_L1_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L2_CTRL_0), CPHY_L2_IDDQEN);
      HDMI_SET_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L3_CTRL_0), CPHY_L3_IDDQEN);

      break;

    case PHY_POWER_ON:

      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_0), CPHY_L0_IDDQEN);
      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L1_CTRL_0), CPHY_L1_IDDQEN);
      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L2_CTRL_0), CPHY_L2_IDDQEN);
      HDMI_CLEAR_REG_BITS_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L3_CTRL_0), CPHY_L3_IDDQEN);
      STHDMIRX_DELAY_1ms(5);
      HDMI_SET_REG_BITS_BYTE((U32) (PhyControl_p->BaseAddress_p + CPHY_PHY_CTRL), CPHY_TMDS_MODE);	// PLL in TMDS MODE

      /*
       * TO DO : use vibe_os_get_chip_version(mag,min)
       * to determine cannes chip version, replace STHDMIRX_WA_VCore_1V.
       */
      if(STHDMIRX_WA_VCore_1V==1)
        VCore_setting = (BIT20 | BIT29);
      else
        VCore_setting = BIT29;

      HDMI_WRITE_REG_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L0_CTRL_4),
        (CPHY_L0_CAL_CNTL |CPHY_L0_SEL_DATA_OW |
         CPHY_L0_SEL_DATA_TEST | CPHY_L0_PDEN_OW | VCore_setting));
      HDMI_WRITE_REG_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L1_CTRL_4),
        (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW |CPHY_L0_SEL_DATA_TEST | CPHY_L0_PDEN_OW | VCore_setting));
      HDMI_WRITE_REG_DWORD(
        (U32)(PhyControl_p->BaseAddress_p + CPHY_L2_CTRL_4),
        (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW |CPHY_L0_SEL_DATA_TEST | CPHY_L0_PDEN_OW | VCore_setting));
      HDMI_WRITE_REG_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_L3_CTRL_4),
        (CPHY_L0_CAL_CNTL | CPHY_L0_SEL_DATA_OW |CPHY_L0_SEL_DATA_TEST | CPHY_L0_PDEN_OW | VCore_setting));

      HDMI_WRITE_REG_DWORD(
        (U32)(PhyControl_p->BaseAddress_p +CPHY_AUX_PHY_CTRL),
        BIT22 | BIT1 | BIT2);

      /* Set the DVI digital block in Power On mode */
      HDMI_CLEAR_AND_SET_REG_BITS_WORD(
        ((U32)(PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_CTRL),
        HDMI_RX_PHY_PWR_CTRL, pMode);
      break;

    case PHY_POWERMODE_RSVD:
    default:
      DBG_HDRX_PHY_INFO("Reserved power Mode...Not supported\n");
      break;

    }

}				/*End of sthdmirx_PHY_power_mode_setup() */

/******************************************************************************
 FUNCTION     :  sthdmirx_PHY_set_fast_clk_meas
 USAGE        :  Fn used to detect TMDS Clock,TCLK & TMDS Link clock have an internal count register.
              :  when the mask time expires, the counts are compared & if TMDS linkClock>TCLK, Interrupt
              :  is generated.
 INPUT        :
 RETURN       :
 USED_REGS    :  HDMI_RX_PHY_CLK_MS_CTRL
******************************************************************************/
void sthdmirx_PHY_set_fast_clk_meas(sthdmirx_PHY_context_t *PhyControl_p,
                                    U8 fmCtrl, U8 mDuration)
{
  if (fmCtrl)
    {
      HDMI_CLEAR_AND_SET_REG_BITS_WORD(
        ((U32)(PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_CLK_MS_CTRL),
        HDMI_RX_PHY_CLK_MS_MSK,(mDuration & 0x0f));
      HDMI_SET_REG_BITS_WORD(
        ((U32) (PhyControl_p->DeviceBaseAddress)+ HDMI_RX_PHY_CLK_MS_CTRL),
        HDMI_RX_PHY_CLK_MS_EN);
    }
  else
    {
      HDMI_CLEAR_REG_BITS_WORD(
        ((U32)(PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_CLK_MS_CTRL),
        HDMI_RX_PHY_CLK_MS_EN);
    }

}				/*End of sthdmirx_PHY_set_fast_clk_meas() */

/******************************************************************************
 FUNCTION     :  sthdmirx_PHY_clear_sigqual_score
 USAGE        :  Strobe to Clear the Sigqual score value, which is stored in the PHY hardware.
 INPUT        :  None
 RETURN       :
 USED_REGS    :  HDMI_RX_PHY_ACTRL
******************************************************************************/
void sthdmirx_PHY_clear_sigqual_score(sthdmirx_PHY_context_t *PhyControl_p)
{
  HDMI_SET_REG_BITS_DWORD(
    (U32)(PhyControl_p->DeviceBaseAddress + HDMI_RX_PHY_CTRL),
    HDMI_RX_PHY_CTRL_CLR_SCR);
  //CPU_DELAY(10);
  HDMI_CLEAR_REG_BITS_DWORD(
    ((U32)PhyControl_p->DeviceBaseAddress +HDMI_RX_PHY_CTRL),
    HDMI_RX_PHY_CTRL_CLR_SCR);
  /* Need to check with Analog Team, Is it require to clear this bit or can be clear immediately */
}				/*End of sthdmirx_PHY_clear_sigqual_score() */

/******************************************************************************
 FUNCTION     :     sthdmirx_PHY_get_signalquality_score
 USAGE        :     Get the Signal Quality score value
 INPUT        :     Duration: Signal Qual capture timing
 RETURN       :     None
 USED_REGS    :     HDMI_RX_PHY_SIGQUAL
******************************************************************************/
U32 sthdmirx_PHY_get_signalquality_score(sthdmirx_PHY_context_t *PhyControl_p,
    U32 uDurationMsec)
{
  U32 SigQual;
  if (uDurationMsec)
    {
      sthdmirx_PHY_clear_sigqual_score(PhyControl_p);
      STHDMIRX_DELAY_1ms((U16) uDurationMsec);
      sthdmirx_PHY_clear_sigqual_score(PhyControl_p);
    }
  SigQual = HDMI_READ_REG_DWORD(
              (U32)(PhyControl_p->DeviceBaseAddress) + HDMI_RX_PHY_SIGQUAL) & 0x00ffffffUL;
  return SigQual;

}				/*End of sthdmirx_PHY_get_signalquality_score() */

/******************************************************************************
 FUNCTION     :  sthdmirx_PHY_get_phy_status
 USAGE        :  Get the PHY status - clock presence, Active source selected
 INPUT        :  Requested status Type
 RETURN       :  HW register status
 USED_REGS    :  HDMI_RX_PHY_STATUS
******************************************************************************/
U8 sthdmirx_PHY_get_phy_status(sthdmirx_PHY_context_t *PhyControl_p,
                               sthdmirx_PHY_HW_status_t CheckStatus)
{
  U32 pHyStatus = 0;

  pHyStatus = HDMI_READ_REG_DWORD(
                (U32) (PhyControl_p->DeviceBaseAddress) +HDMI_RX_PHY_STATUS);

  switch (CheckStatus)
    {
    case PHY_ANALOG_REFERENCE_CLOCK_PRESENT:
      pHyStatus = (pHyStatus & HDMI_RX_PHY_ANA_LCLK_PRSNT) ? 1 : 0;
      break;

    case PHY_OVR_SAMPLE_LINK_CLOCK_PRESENT:
      pHyStatus = (pHyStatus & HDMI_RX_PHY_OVR_LCLK_PRSNT) ? 1 : 0;
      break;

    case PHY_REGENERATED_LINK_CLOCK_PRESENT:
      pHyStatus = (pHyStatus & HDMI_RX_PHY_LCLK_PRSNT) ? 1 : 0;
      break;

    default:
      pHyStatus = 0xff;
      break;
    }
  return (U8) pHyStatus;

}				/*End of sthdmirx_PHY_get_phy_status() */

/******************************************************************************
 FUNCTION     :    sthdmirx_PHY_setup_equalization_scheme
 USAGE        :    Setup the different Equalization schemes
 INPUT        :    Equalization scheme
 RETURN       :    None
 USED_REGS    :    None
******************************************************************************/
void sthdmirx_PHY_setup_equalization_scheme(
  sthdmirx_PHY_context_t *PhyControl_p)
{
  switch (PhyControl_p->EqualizationType)
    {

    case SIGNAL_EQUALIZATION_LOWGAIN:
      sthdmirx_PHY_set_eq_gain(PhyControl_p, 0x7);
      break;

    case SIGNAL_EQUALIZATION_MIDGAIN:
      sthdmirx_PHY_set_eq_gain(PhyControl_p, 0x4);
      break;

    case SIGNAL_EQUALIZATION_HIGHGAIN:
      sthdmirx_PHY_set_eq_gain(PhyControl_p, 0x0);
      break;

    case SIGNAL_EQUALIZATION_CUSTOM:
      DBG_HDRX_PHY_INFO("Custom Equalization scheme is selected %d\n",
                        PhyControl_p->CustomEqSetting.HighFreqGain);
      sthdmirx_PHY_set_eq_gain(PhyControl_p,
                               (U8) PhyControl_p->CustomEqSetting.HighFreqGain);
      break;

    case SIGNAL_EQUALIZATION_DISABLED:
    case SIGNAL_EQUALIZATION_ADAPTIVE:
    default:
      DBG_HDRX_PHY_INFO("Adaptive Equalization scheme is selected\n");
      sthdmirx_PHY_set_eq_gain(PhyControl_p,
                               PhyControl_p->DefaultEqSetting.DviEq);
      break;
    }

}				/*End of STHDMIRX_PHY_SelectActiveSource() */

/******************************************************************************
 FUNCTION     :    sthdmirx_PHY_init
 USAGE        :    Initialize the PHY Hardware
 INPUT        :    None
 RETURN       :    None
 USED_REGS    :    None
******************************************************************************/
void sthdmirx_PHY_init(sthdmirx_PHY_context_t *PhyControl_p)
{
  /*Need to change the phy config as per phy control param */

  /*Termination Resistance Setup */

  sthdmirx_PHY_Rterm_calibration(PhyControl_p,
                                 PhyControl_p->RTermCalibrationMode,PhyControl_p->RTermValue);

  /* PowerDown the PHY Blocks */
  sthdmirx_PHY_power_mode_setup(PhyControl_p, PHY_POWER_OFF);

  /* Clock selection */
  sthdmirx_PHY_clk_selection(PhyControl_p, LCLK_SEL_OVERSAMPLED_CLK);

  /* invert the oversample clock */
  //sthdmirx_PHY_clk_inverse(PhyControl_p,INVERSE_OVR_SAMPLE_CLK, 1);

  /* SetUp PLL in High Frequency mode as default */
  sthdmirx_PHY_PLL_setup(PhyControl_p, INPUTCLK_HIGH_FREQ_RANGE, TRUE);

  /*Setup Fast Clock Measurement Blk for clk present detection */
  sthdmirx_PHY_set_fast_clk_meas(PhyControl_p, 1, 15);

  /*SetUp The Equalizer Scheme */
  sthdmirx_PHY_setup_equalization_scheme(PhyControl_p);

  STHDMIRX_DELAY_1ms(1);

  DBG_HDRX_PHY_INFO("PHY Initialization done!\n");

}				/*End of STHDMIRX_PHY_Init() */

/* End of file */
