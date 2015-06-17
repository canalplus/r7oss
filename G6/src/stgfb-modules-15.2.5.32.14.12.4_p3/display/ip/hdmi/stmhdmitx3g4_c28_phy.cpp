/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmitx3g4_c28_phy.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
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

#include "stmhdmitx3g4_c28_phy.h"

#define STM_HDMI_SRZ_CFG         0x504
#define STM_HDMI_SRZ_PLL_CFG     0x510
#define STM_HDMI_SRZ_ICNTL       0x518
#define STM_HDMI_SRZ_CALCODE_EXT 0x520

#define SRZ_CFG_EN                          (1L<<0)
#define SRZ_CFG_DISABLE_BYPASS_SINK_CURRENT (1L<<1)
#define SRZ_CFG_EXTERNAL_DATA               (1L<<16)
#define SRZ_CFG_RBIAS_EXT                   (1L<<17)
#define SRZ_CFG_EN_SINK_TERM_DETECTION      (1L<<18)
#define SRZ_CFG_EN_BIASRES_DETECTION        (1L<<19)
#define SRZ_CFG_EN_SRC_TERMINATION          (1L<<24)

#define SRZ_CFG_INTERNAL_MASK  (SRZ_CFG_EN                          | \
                                SRZ_CFG_DISABLE_BYPASS_SINK_CURRENT | \
                                SRZ_CFG_EXTERNAL_DATA               | \
                                SRZ_CFG_RBIAS_EXT                   | \
                                SRZ_CFG_EN_SINK_TERM_DETECTION      | \
                                SRZ_CFG_EN_BIASRES_DETECTION        | \
                                SRZ_CFG_EN_SRC_TERMINATION)

#define PLL_CFG_EN         (1L<<0)
#define PLL_CFG_NDIV_SHIFT (8)
#define PLL_CFG_IDF_SHIFT  (16)
#define PLL_CFG_ODF_SHIFT  (24)

#define ODF_DIV_1          (0)
#define ODF_DIV_2          (1)
#define ODF_DIV_4          (2)
#define ODF_DIV_8          (3)

struct plldividers_s
{
  uint32_t min;
  uint32_t max;
  uint32_t idf;
  uint32_t odf;
};


/*
 * Functional specification recommended values
 */
static struct plldividers_s plldividers[] = {
    { 0,         20000000,  1,  ODF_DIV_8 },
    { 20000000,  42500000,  2,  ODF_DIV_8 },
    { 42500000,  85000000,  4,  ODF_DIV_4 },
    { 85000000,  170000000, 8,  ODF_DIV_2 },
    { 170000000, 340000000, 16, ODF_DIV_1 }
};

CSTmHDMITx3G4_C28_Phy::CSTmHDMITx3G4_C28_Phy(CDisplayDevice *pDev,
                                                     uint32_t phyoffset): CSTmHDMIPhy()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevRegs = (uint32_t *)pDev->GetCtrlRegisterBase();
  m_uPhyOffset = phyoffset;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDMITx3G4_C28_Phy::~CSTmHDMITx3G4_C28_Phy()
{
}


bool CSTmHDMITx3G4_C28_Phy::Start(const stm_display_mode_t *const pModeLine,
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
  uint32_t idf;
  uint32_t odf;
  uint32_t pixrep=0;
  uint32_t pllctrl = 0;
  unsigned i;

  bool bFoundPLLDivides = false;

  switch(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_MASK)
  {
      case STM_MODE_FLAGS_HDMI_PIXELREP_2X:
        ckpxpll /= 2;
        pixrep = 1;
        break;
      case STM_MODE_FLAGS_HDMI_PIXELREP_4X:
        ckpxpll /= 4;
        pixrep = 2;
        break;
      default:
        pixrep = 0;
        break;
  }

  if((ckpxpll < plldividers[0].max) && (pixrep > 0))
      pixrep -= 1;

  for(i=0;i<N_ELEMENTS(plldividers);i++)
  {
    if(ckpxpll >= plldividers[i].min && ckpxpll < plldividers[i].max)
    {
      idf = plldividers[i].idf;
      odf = plldividers[i].odf-pixrep;
      bFoundPLLDivides = true;
      break;
    }
  }

  if(!bFoundPLLDivides)
  {
    TRC( TRC_ID_MAIN_INFO, "input TMDS clock speed (%u) not supported",ckpxpll );
    return false;
  }

  switch(outputFormat & STM_VIDEO_OUT_DEPTH_MASK)
  {
    case STM_VIDEO_OUT_30BIT:
      tmdsck = (ckpxpll * 5) / 4; // 1.25x
      pllctrl |= 50 << PLL_CFG_NDIV_SHIFT; // (50*4)/16 = x12.5
      TRC( TRC_ID_MAIN_INFO, "Selected 30bit colour" );
      break;
    case STM_VIDEO_OUT_36BIT:
      tmdsck = (ckpxpll * 3) / 2; // 1.5x
      pllctrl |= 60 << PLL_CFG_NDIV_SHIFT; // (60*4)/16 = x15
      TRC( TRC_ID_MAIN_INFO, "Selected 36bit colour" );
      break;
    case STM_VIDEO_OUT_48BIT:
      tmdsck = ckpxpll * 2;
      pllctrl |= 16 << PLL_CFG_NDIV_SHIFT;
      if(odf > 0)
      {
        odf--; // reduce the output divide by a factor of 2
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "Input clock out of range for 48bit color" );
        return false;
      }

      TRC( TRC_ID_MAIN_INFO, "Selected 48bit colour" );
      break;
    default:
    case STM_VIDEO_OUT_24BIT:
      tmdsck = ckpxpll;
      pllctrl |= 40 << PLL_CFG_NDIV_SHIFT;
      TRC( TRC_ID_MAIN_INFO, "Selected 24bit colour" );
      break;
  }

  if(tmdsck > 340000000)
  {
    TRC( TRC_ID_MAIN_INFO, "Output TMDS clock (%u) out of range",tmdsck );
    return false;
  }

  pllctrl |= idf << PLL_CFG_IDF_SHIFT;
  pllctrl |= odf << PLL_CFG_ODF_SHIFT;

  uint32_t cfg = (SRZ_CFG_EN                   |
                  SRZ_CFG_EXTERNAL_DATA        |
                  SRZ_CFG_RBIAS_EXT            |
                  SRZ_CFG_EN_BIASRES_DETECTION |
                  SRZ_CFG_EN_SINK_TERM_DETECTION);

  if(tmdsck > 165000000)
    cfg |= SRZ_CFG_EN_SRC_TERMINATION;

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
        /*
         * Or in the externally provided configuration bits, ensuring the bits
         * internally set are not changed.
         */
        cfg |= (m_pPHYConfig[i].config[0] & ~SRZ_CFG_INTERNAL_MASK);
        WritePhyReg(STM_HDMI_SRZ_CFG, cfg);

        WritePhyReg(STM_HDMI_SRZ_ICNTL,       m_pPHYConfig[i].config[1]);
        WritePhyReg(STM_HDMI_SRZ_CALCODE_EXT, m_pPHYConfig[i].config[2]);
        TRC( TRC_ID_MAIN_INFO, "Setting serializer config 0x%08x 0x%08x 0x%08x", cfg, m_pPHYConfig[i].config[1], m_pPHYConfig[i].config[2] );
        TRCOUT( TRC_ID_MAIN_INFO, "" );
        /*
         * Configure and power up the PHY PLL
         */
        TRC( TRC_ID_MAIN_INFO, "pllctrl = 0x%08x", pllctrl );
        WritePhyReg(STM_HDMI_SRZ_PLL_CFG, (pllctrl | PLL_CFG_EN));


        while((ReadPhyReg(STM_HDMI_STA) & STM_HDMI_STA_PLL_LCK) == 0);

        TRC( TRC_ID_MAIN_INFO, "got PHY PLL Lock" );
        return true;
      }
      i++;
    }
  }

  /*
   * Default, power up the serializer with no pre-emphasis or
   * output swing correction
   */
  WritePhyReg(STM_HDMI_SRZ_CFG, cfg);
  WritePhyReg(STM_HDMI_SRZ_ICNTL, 0);
  WritePhyReg(STM_HDMI_SRZ_CALCODE_EXT, 0);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDMITx3G4_C28_Phy::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WritePhyReg(STM_HDMI_SRZ_CFG, (SRZ_CFG_EN_SINK_TERM_DETECTION | SRZ_CFG_EN_BIASRES_DETECTION));

  WritePhyReg(STM_HDMI_SRZ_PLL_CFG, 0);
  while((ReadPhyReg(STM_HDMI_STA) & STM_HDMI_STA_PLL_LCK) != 0);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
