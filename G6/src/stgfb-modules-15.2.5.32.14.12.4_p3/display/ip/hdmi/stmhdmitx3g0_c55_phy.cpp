/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmitx3g0_c55_phy.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>

#include <display/ip/hdmi/stmhdmiregs.h> // For Phy (P/D)LL Status which appears in the HDMI status

#include "stmhdmitx3g0_c55_phy.h"

#define STM_HDMI_SRZ_PLL_CFG       0x504
#define STM_HDMI_SRZ_TAP_1         0x508
#define STM_HDMI_SRZ_TAP_2         0x50C
#define STM_HDMI_SRZ_TAP_3         0x510
#define STM_HDMI_SRZ_CTRL          0x514

#define STM_HDMI_SRZ_PLL_CFG_POWER_DOWN         (1L<<0)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_SHIFT         (1)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_425MHZ        (0)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_850MHZ        (1)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_1700MHZ       (2)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_3000MHZ       (3)
#define STM_HDMI_SRZ_PLL_CFG_VCOR_MASK          (3)
#define STM_HDMI_SRZ_PLL_CFG_VCOR(x)            ((x)<<STM_HDMI_SRZ_PLL_CFG_VCOR_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT         (8)
#define STM_HDMI_SRZ_PLL_CFG_NDIV_MASK          (0x1f<<STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_MODE_SHIFT         (16)
#define STM_HDMI_SRZ_PLL_CFG_MODE_13_5_MHZ      (0x1)
#define STM_HDMI_SRZ_PLL_CFG_MODE_25_2_MHZ      (0x4)
#define STM_HDMI_SRZ_PLL_CFG_MODE_27_MHZ        (0x5)
#define STM_HDMI_SRZ_PLL_CFG_MODE_33_75_MHZ     (0x6)
#define STM_HDMI_SRZ_PLL_CFG_MODE_40_5_MHZ      (0x7)
#define STM_HDMI_SRZ_PLL_CFG_MODE_54_MHZ        (0x8)
#define STM_HDMI_SRZ_PLL_CFG_MODE_67_5_MHZ      (0x9)
#define STM_HDMI_SRZ_PLL_CFG_MODE_74_25_MHZ     (0xa)
#define STM_HDMI_SRZ_PLL_CFG_MODE_81_MHZ        (0xb)
#define STM_HDMI_SRZ_PLL_CFG_MODE_82_5_MHZ      (0xc)
#define STM_HDMI_SRZ_PLL_CFG_MODE_108_MHZ       (0xd)
#define STM_HDMI_SRZ_PLL_CFG_MODE_148_5_MHZ     (0xe)
#define STM_HDMI_SRZ_PLL_CFG_MODE_165_MHZ       (0xf)
#define STM_HDMI_SRZ_PLL_CFG_MODE_MASK          (0xf)
#define STM_HDMI_SRZ_PLL_CFG_MODE(x)            ((x)<<STM_HDMI_SRZ_PLL_CFG_MODE_SHIFT)
#define STM_HDMI_SRZ_PLL_CFG_SEL_12P5           (1L<<31)

#define STM_HDMI_SRZ_CTRL_POWER_DOWN            (1L<<0)
#define STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN      (1L<<1)


struct pllmode_s
{
  uint32_t min;
  uint32_t max;
  uint32_t mode;
};

static struct pllmode_s pllmodes[] = {
    { 13500000,  13513500,  STM_HDMI_SRZ_PLL_CFG_MODE_13_5_MHZ  },
    { 25174800,  25200000,  STM_HDMI_SRZ_PLL_CFG_MODE_25_2_MHZ  },
    { 27000000,  27027000,  STM_HDMI_SRZ_PLL_CFG_MODE_27_MHZ    },
    { 54000000,  54054000,  STM_HDMI_SRZ_PLL_CFG_MODE_54_MHZ    },
    { 72000000,  74250000,  STM_HDMI_SRZ_PLL_CFG_MODE_74_25_MHZ },
    { 108000000, 108108000, STM_HDMI_SRZ_PLL_CFG_MODE_108_MHZ   },
    { 148351648, 297000000, STM_HDMI_SRZ_PLL_CFG_MODE_148_5_MHZ }
};



CSTmHDMITx3G0_C55_Phy::CSTmHDMITx3G0_C55_Phy(CDisplayDevice *pDev,
                                                     uint32_t phyoffset): CSTmHDMIPhy()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevRegs = (uint32_t *)pDev->GetCtrlRegisterBase();
  m_uPhyOffset = phyoffset;
  m_bMultiplyTMDSClockForPixelRepetition = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDMITx3G0_C55_Phy::~CSTmHDMITx3G0_C55_Phy()
{
}


bool CSTmHDMITx3G0_C55_Phy::Start(const stm_display_mode_t *const pModeLine,
                                      uint32_t outputFormat)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * We have arranged in the chip specific PreConfiguration for the input
   * clock to the PHY PLL to be the normal TMDS clock for 24bit colour, taking
   * into account pixel repetition. We have been passed a modeline where the
   * pixelclock parameter is in fact that TMDS clock in the case of the
   * pixel doubled/quadded modes.
   */
  uint32_t ckpxpll = pModeLine->mode_timing.pixel_clock_freq;
  uint32_t tmdsck;
  uint32_t freqvco;
  uint32_t pllctrl = 0;
  uint32_t pixrep = 1;
  unsigned i;

  if(m_bMultiplyTMDSClockForPixelRepetition)
  {
    switch(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_MASK)
    {
      case STM_MODE_FLAGS_HDMI_PIXELREP_2X:
        pixrep = 2;
        ckpxpll /= 2;
        break;
      case STM_MODE_FLAGS_HDMI_PIXELREP_4X:
        pixrep = 4;
        ckpxpll /= 4;
        break;
      default:
        pixrep = 1;
        break;
    }
  }

  bool bDeepColor = false;

  switch(outputFormat & STM_VIDEO_OUT_DEPTH_MASK)
  {
    case STM_VIDEO_OUT_30BIT:
      tmdsck = (ckpxpll * pixrep * 5) / 4; // 1.25x
      /*
       * With no pixel repetition we need to do x25 with the additional config
       * bit to divide that by two to get *12.5. When we have pixel repetition
       * then the required TMDS multiplier is either x25 or x50.
       */
      if(pixrep == 1)
        pllctrl = (5 << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT) | STM_HDMI_SRZ_PLL_CFG_SEL_12P5;
      else
        pllctrl = (5 * (pixrep/2)) << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;

      bDeepColor = true;
      TRC( TRC_ID_MAIN_INFO, "Selected 30bit colour" );
      break;
    case STM_VIDEO_OUT_36BIT:
      tmdsck = (ckpxpll * pixrep * 3) / 2; // 1.5x
      pllctrl = (3 * pixrep) << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;
      bDeepColor = true;

      TRC( TRC_ID_MAIN_INFO, "Selected 36bit colour" );
      break;
    case STM_VIDEO_OUT_48BIT:
      tmdsck = ckpxpll * pixrep * 2;
      pllctrl = (4 * pixrep) << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;
      bDeepColor = true;

      TRC( TRC_ID_MAIN_INFO, "Selected 48bit colour" );
      break;
    default:
    case STM_VIDEO_OUT_24BIT:
      tmdsck = ckpxpll * pixrep;
      pllctrl = (2 * pixrep) << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;

      TRC( TRC_ID_MAIN_INFO, "Selected 24bit colour" );
      break;
  }

  /*
   * Setup the PLL mode parameter based on the ckpxpll. If we haven't got
   * a clock frequency supported by one of the specific PLL modes then we
   * will end up using the generic mode (0) which only supports a 10x
   * multiplier, hence only 24bit color.
  */
  bool bHaveSpecificPLLMode = false;
  for(i=0;i<N_ELEMENTS(pllmodes);i++)
  {
    if(ckpxpll >= pllmodes[i].min && ckpxpll <= pllmodes[i].max)
    {
      pllctrl |= STM_HDMI_SRZ_PLL_CFG_MODE(pllmodes[i].mode);
      bHaveSpecificPLLMode = true;
    }
  }

  if(!bHaveSpecificPLLMode && bDeepColor)
  {
    TRC( TRC_ID_MAIN_INFO, "deepcolour not supported in this mode" );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Setting serializer VCO for 10 * %uHz", tmdsck );

  freqvco = tmdsck * 10;
  if(freqvco <= 425000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_425MHZ);
  else if (freqvco <= 850000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_850MHZ);
  else if (freqvco <= 1700000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_1700MHZ);
  else if (freqvco <= 2970000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_3000MHZ);
  else
  {
    TRC( TRC_ID_MAIN_INFO, "PHY serializer clock out of range" );
    return false;
  }

  /*
   * Configure and power up the PHY PLL
   */
  TRC( TRC_ID_MAIN_INFO, "pllctrl = 0x%08x", pllctrl );
  WritePhyReg(STM_HDMI_SRZ_PLL_CFG, pllctrl| STM_HDMI_SRZ_PLL_CFG_POWER_DOWN);

  vibe_os_sleep_ms(1);
  WritePhyReg(STM_HDMI_SRZ_PLL_CFG, ReadPhyReg(STM_HDMI_SRZ_PLL_CFG)&~STM_HDMI_SRZ_PLL_CFG_POWER_DOWN);
  WritePhyReg(STM_HDMI_SRZ_CTRL, ReadPhyReg(STM_HDMI_SRZ_CTRL)& ~ STM_HDMI_SRZ_CTRL_POWER_DOWN);
  TRC( TRC_ID_MAIN_INFO, "got PHY PLL Lock" );


  /*
   * To configure the source termination and pre-emphasis appropriately for
   * different high speed TMDS clock frequencies a phy configuration
   * table must be provided, tailored to the SoC and board combination.
   */
  if(m_pPHYConfig != 0)
  {
    i = 0;
    while(!((m_pPHYConfig[i].min_tmds_freq == 0) && (m_pPHYConfig[i].max_tmds_freq == 0)))
    {
      if((m_pPHYConfig[i].min_tmds_freq <= tmdsck) && (m_pPHYConfig[i].max_tmds_freq >= tmdsck))
      {
        WritePhyReg(STM_HDMI_SRZ_TAP_1, m_pPHYConfig[i].config[0]);
        WritePhyReg(STM_HDMI_SRZ_TAP_2, m_pPHYConfig[i].config[1]);
        WritePhyReg(STM_HDMI_SRZ_TAP_3, m_pPHYConfig[i].config[2]);
        WritePhyReg(STM_HDMI_SRZ_CTRL, ((m_pPHYConfig[i].config[3] | STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN) & ~STM_HDMI_SRZ_CTRL_POWER_DOWN));
        while((ReadPhyReg(STM_HDMI_STA) & STM_HDMI_STA_PLL_LCK) == 0);
        vibe_os_sleep_ms(1);
        TRC( TRC_ID_MAIN_INFO, "Setting serializer config 0x%08x 0x%08x 0x%08x 0x%08x", m_pPHYConfig[i].config[0], m_pPHYConfig[i].config[1], m_pPHYConfig[i].config[2], m_pPHYConfig[i].config[3] );
        TRCOUT( TRC_ID_MAIN_INFO, "" );
        return true;
      }
      i++;
    }
  }

  /*
   * Default, power up the serializer with no pre-emphasis or source termination
   */
  WritePhyReg(STM_HDMI_SRZ_TAP_1, 0);
  WritePhyReg(STM_HDMI_SRZ_TAP_2, 0);
  WritePhyReg(STM_HDMI_SRZ_TAP_3, 0);
  WritePhyReg(STM_HDMI_SRZ_CTRL,  STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDMITx3G0_C55_Phy::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WritePhyReg(STM_HDMI_SRZ_CTRL, STM_HDMI_SRZ_CTRL_POWER_DOWN);

  WritePhyReg(STM_HDMI_SRZ_PLL_CFG, STM_HDMI_SRZ_PLL_CFG_POWER_DOWN);
  while((ReadPhyReg(STM_HDMI_STA) & STM_HDMI_STA_PLL_LCK) != 0);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
