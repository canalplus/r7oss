/***********************************************************************
 *
 * File: display/soc/stiH416/stiH416_vtac.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "../hdmirx_vtac.h"
#include "stiH416_hdmirx_reg.h"

typedef struct stm_hdmirx_vtac_params_s
{
  U8  Phyts_Per_Pixel;
  U8  Phyt_Width;
  U8  VidIn_Width;

  BOOL  even_parity_en;
  BOOL  odd_parity_en;
  BOOL  alpha_en;
  BOOL  gfx_en;
  BOOL  vid_en;
} stm_hdmirx_vtac_params_t;

typedef struct stm_hdmirx_vtac_base_s
{
  void *pvtac_rxphy_base;
  void *pvtac_txphy_base;
  void *pvtacTX_hdmirx_base;
  void *pvtacRX_hdmirx_base;
} stm_hdmirx_vtac_base_t;

#define ABSOLUTE_ADDRESS(Offset, reg) (gW_HdmiRegBaseAddress + ((volatile U32)Offset+reg))

void setVTACDefaultParams (stm_hdmirx_vtac_params_t * VTAC_Params);
void programVTACSettings (const stm_hdmirx_vtac_base_t * pvtac_base, const stm_hdmirx_vtac_params_t * pVTAC_Params);
void configVTACTXPHYParams(const stm_hdmirx_vtac_base_t * pvtac_base);
void configVTACRXPHYParams(const stm_hdmirx_vtac_base_t * pvtac_base);

BOOL hdmirx_powerdown_vtac(void)
{
  stm_hdmirx_vtac_base_t vtac_base;

  /* map vtac virtual nocache base address */
  vtac_base.pvtac_rxphy_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_SYSCFG_VIDEO_BASE, 0xFFFF);
  vtac_base.pvtac_txphy_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_SYSCFG_FRONT_BASE, 0x10000);
  vtac_base.pvtacTX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_TX_SAS_BASE, 0x1000);
  vtac_base.pvtacRX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_RX_MPE_BASE, 0x400);

  /* power down VTAC TX PHY */
  HDMI_WRITE_REG_DWORD(vtac_base.pvtac_txphy_base+VTAC_TX_PHY_CONFIG1, 0);
  HDMI_WRITE_REG_DWORD(vtac_base.pvtac_txphy_base+VTAC_TX_PHY_CONFIG0, 0);

  /* power down VTAC RX PHY */
  HDMI_SET_REG_BITS_DWORD(vtac_base.pvtac_rxphy_base+VTAC_RX_PHY_CONFIG0, (CONFIG_VTAC_RX_PDD|CONFIG_VTAC_RX_PDR));
  HDMI_SET_REG_BITS_DWORD(vtac_base.pvtac_rxphy_base+VTAC_RX_PHY_CONFIG1, (CONFIG_VTAC_RX_PDD|CONFIG_VTAC_RX_PDR));

  /* disable VTAC RX */
  HDMI_CLEAR_REG_BITS_DWORD(vtac_base.pvtacRX_hdmirx_base+VTAC_RX_CONFIG, VTAC_ENABLE_BIT);
  HDMI_CLEAR_REG_BITS_DWORD(vtac_base.pvtacTX_hdmirx_base+VTAC_TX_CONFIG, VTAC_ENABLE_BIT);

  /* un map vtac virtual base address */
  iounmap((void *)vtac_base.pvtac_rxphy_base);
  iounmap((void *)vtac_base.pvtac_txphy_base);
  iounmap((void *)vtac_base.pvtacTX_hdmirx_base);
  iounmap((void *)vtac_base.pvtacRX_hdmirx_base);
  return FALSE;
}

BOOL hdmirx_configure_vtac(void)
{
  stm_hdmirx_vtac_params_t VTAC_Params;
  stm_hdmirx_vtac_base_t vtac_base;

  setVTACDefaultParams(&VTAC_Params);
  HDMI_PRINT("%s: %d-%d-%d-%d-%d\n",__PRETTY_FUNCTION__,VTAC_Params.even_parity_en,
             VTAC_Params.odd_parity_en,
             VTAC_Params.alpha_en,
             VTAC_Params.gfx_en,
             VTAC_Params.vid_en);

  /* map vtac virtual nocache base address */
  vtac_base.pvtac_rxphy_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_SYSCFG_VIDEO_BASE, 0xFFFF);
  vtac_base.pvtac_txphy_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_SYSCFG_FRONT_BASE, 0x10000);
  vtac_base.pvtacTX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_TX_SAS_BASE, 0x1000);
  vtac_base.pvtacRX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_RX_MPE_BASE, 0x400);

  configVTACRXPHYParams(&vtac_base);
  configVTACTXPHYParams(&vtac_base);
  programVTACSettings(&vtac_base, &VTAC_Params);

  /* un map vtac virtual base address */
  iounmap((void *)vtac_base.pvtac_rxphy_base);
  iounmap((void *)vtac_base.pvtac_txphy_base);
  iounmap((void *)vtac_base.pvtacTX_hdmirx_base);
  iounmap((void *)vtac_base.pvtacRX_hdmirx_base);
  return FALSE;
}

BOOL hdmirx_reset_vtac(void)
{
  stm_hdmirx_vtac_base_t vtac_base;

  /* map vtac virtual nocache base address */
  vtac_base.pvtacTX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_TX_SAS_BASE, 0x1000);
  vtac_base.pvtacRX_hdmirx_base = ioremap_nocache(STiH416_REGISTER_BASE+STiH416_VTAC_HDMIRX_RX_MPE_BASE, 0x400);

  HDMI_SET_REG_BITS_DWORD(vtac_base.pvtacRX_hdmirx_base+VTAC_RX_CONFIG, VTAC_SW_RST_BIT );
  HDMI_SET_REG_BITS_DWORD(vtac_base.pvtacTX_hdmirx_base+VTAC_TX_CONFIG, VTAC_SW_RST_BIT );

  /* un map vtac virtual base address */
  iounmap((void *)vtac_base.pvtacTX_hdmirx_base);
  iounmap((void *)vtac_base.pvtacRX_hdmirx_base);
  return FALSE;
}

void setVTACDefaultParams (stm_hdmirx_vtac_params_t * pVTAC_Params)
{
  pVTAC_Params->even_parity_en  = true;
  pVTAC_Params->odd_parity_en   = true;
  pVTAC_Params->alpha_en        = false;
  pVTAC_Params->gfx_en          = true;
  pVTAC_Params->vid_en          = true;

  pVTAC_Params->Phyts_Per_Pixel = 0xa;
  pVTAC_Params->Phyt_Width      = 0x2;
  pVTAC_Params->VidIn_Width     = 0x2;
}

void programVTACSettings (const stm_hdmirx_vtac_base_t * pvtac_base, const stm_hdmirx_vtac_params_t * pVTAC_Params)
{
  uint32_t val;

  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_CONFIG, 0 );
  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_FIFO_CONFIG, 0);
  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacTX_hdmirx_base+VTAC_TX_CONFIG, 0 );

  val  = (VTAC_ENABLE_BIT | VTAC_SW_RST_BIT);

  val |= (  (pVTAC_Params->even_parity_en << 13) | (pVTAC_Params->odd_parity_en << 12)
            | (pVTAC_Params->gfx_en         << 10) | (pVTAC_Params->vid_en        <<  9)
            | (pVTAC_Params->alpha_en       <<  8) );

  val |= (  (pVTAC_Params->Phyts_Per_Pixel << 23)
            | (pVTAC_Params->Phyt_Width      << 16)
            | (pVTAC_Params->VidIn_Width     <<  4) );


  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_CONFIG, val );
  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_FIFO_CONFIG, VTAC_FIFO_MAIN_CONFIG_VAL);
  HDMI_WRITE_REG_DWORD(pvtac_base->pvtacTX_hdmirx_base+VTAC_TX_CONFIG, val );

  HDMI_PRINT("CSTmVTAC::ProgramVTACSettings: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtacRX_hdmirx_base, VTAC_RX_CONFIG),      HDMI_READ_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_CONFIG));
  HDMI_PRINT("CSTmVTAC::ProgramVTACSettings: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtacRX_hdmirx_base, VTAC_RX_FIFO_CONFIG), HDMI_READ_REG_DWORD(pvtac_base->pvtacRX_hdmirx_base+VTAC_RX_FIFO_CONFIG));
  HDMI_PRINT("CSTmVTAC::ProgramVTACSettings: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtacTX_hdmirx_base, VTAC_TX_CONFIG),      HDMI_READ_REG_DWORD(pvtac_base->pvtacTX_hdmirx_base+VTAC_TX_CONFIG));
}

void configVTACTXPHYParams(const stm_hdmirx_vtac_base_t * pvtac_base)
{
  /*
   * This should be the SoC default, i.e. ODT disabled, SSTL and powered up
   * but lets make sure.
   */
  HDMI_WRITE_REG_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG1, 0);

  HDMI_WRITE_REG_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG0, CONFIG_VTAC_TX_ENABLE_CLK_PHY);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG0,CONFIG_VTAC_TX_ENABLE_CLK_DLL);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG0,CONFIG_VTAC_TX_RST_N_DLL_SWITCH);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG0,CONFIG_VTAC_TX_PLL_NOT_OSC_MODE);

  HDMI_PRINT("CSTmVTAC::ConfigVTACTXPHYParams: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtac_txphy_base,VTAC_TX_PHY_CONFIG0), HDMI_READ_REG_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG0));
  HDMI_PRINT("CSTmVTAC::ConfigVTACTXPHYParams: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtac_txphy_base,VTAC_TX_PHY_CONFIG1), HDMI_READ_REG_DWORD(pvtac_base->pvtac_txphy_base+VTAC_TX_PHY_CONFIG1));
}

void configVTACRXPHYParams(const stm_hdmirx_vtac_base_t * pvtac_base)
{
  /*
   * This should be the SoC default, i.e. ODT disabled, SSTL and powered up
   * but lets make sure.
   */

  HDMI_WRITE_REG_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG0, CONFIG_VTAC_RX_DISABLE_ODT);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG0,CONFIG_VTAC_RX_CK_DIFF_CAPTURE_INV);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG0,CONFIG_VTAC0_RX_CONF);

  HDMI_WRITE_REG_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG1,CONFIG_VTAC_RX_DISABLE_ODT);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG1,CONFIG_VTAC_RX_CK_DIFF_CAPTURE_INV);
  HDMI_SET_REG_BITS_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG1,CONFIG_VTAC1_RX_CONF);

  HDMI_PRINT("CSTmVTAC::ConfigVTACRXPHYParams: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtac_rxphy_base,VTAC_RX_PHY_CONFIG0), HDMI_READ_REG_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG0));
  HDMI_PRINT("CSTmVTAC::ConfigVTACRXPHYParams: 0x%08x = 0x%08x\n", ABSOLUTE_ADDRESS(pvtac_base->pvtac_rxphy_base,VTAC_RX_PHY_CONFIG1), HDMI_READ_REG_DWORD(pvtac_base->pvtac_rxphy_base+VTAC_RX_PHY_CONFIG1));
}
