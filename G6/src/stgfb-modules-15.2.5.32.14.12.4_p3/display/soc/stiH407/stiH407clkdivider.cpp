/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407clkdivider.cpp
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

#include <display/ip/displaytiming/stmflexclkgen_reg.h>

#include "stiH407device.h"
#include "stiH407reg.h"
#include "stiH407clkdivider.h"

/******************************************************************************
 *
 * IMPORTANT: This is a traditional stmfb implementation that controls the
 *            hardware directly. An abstraction to the Linux clock LLA will
 *            probably come later, but the LLA doesn't exist yet for STiH407.
 */


/*
 * Note: this is a mixture of STiH415 style TVOut and older style
 *       compositor clocks to allow the VDP and all four GDPs to
 *       switch between the main and aux mixers. Also note that the main
 *       pixel clock is shared with the HQVDP, it's video plug, the alpha
 *       plane (which we do not currently support) and the capture
 *       block; they are therefore tied to the main output path.
 */
static const stm_clk_divider_output_name_t flexclockgenD12_clk_map[] =
{
    STM_CLK_PIX_MAIN, /* Also HQVDP, VideoPlug 1, CAP, ALPHA */
    STM_CLK_PIP,      /* VDP, VideoPlug 2 */
    STM_CLK_GDP1,
    STM_CLK_GDP2,
    STM_CLK_GDP3,
    STM_CLK_GDP4,
    STM_CLK_PIX_AUX,
    STM_CLK_DENC,
    STM_CLK_PIX_HDDACS,
    STM_CLK_OUT_HDDACS,
    STM_CLK_OUT_SDDACS,
    STM_CLK_PIX_DVO,
    STM_CLK_OUT_DVO,
    STM_CLK_PIX_HDMI,
    STM_CLK_TMDS_HDMI, /* Input needs to be from external source (HDMI PHY TMDS Out) not fsynth */
    STM_CLK_HDMI_PHY_REJECTION
};



CSTiH407ClkDivider::CSTiH407ClkDivider( CDisplayDevice *pDev ): CSTmTVOutClkDivider(flexclockgenD12_clk_map, N_ELEMENTS(flexclockgenD12_clk_map))
{
  m_pFlexClkGenReg = ((uint32_t*)pDev->GetCtrlRegisterBase()) + (STiH407_CLKGEN_D_12>>2);
}


bool CSTiH407ClkDivider::Enable(stm_clk_divider_output_name_t   name,
                                stm_clk_divider_output_source_t src,
                                stm_clk_divider_output_divide_t div)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  int output;
  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::Enable: Clock %d NOT FOUND!!", name );
    return false;
  }

  if(src > STM_CLK_SRC_15)
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::Enable: Invalid clock source (%d) requested!!", src );
    return false;
  }

  if(div > STM_CLK_DIV_8)
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::Enable: Invalid clock divide (%d) requested!!", div );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "CSTiH407ClkDivider::Enable: Enabling Clock %d (output channel %d)", name, output );
  const uint32_t xbarreg = STM_FLEX_CLKGEN_CROSSBAR + ((output/4)*4);
  const uint32_t xbarshift = (output%4) * 8;
  uint32_t val = ReadFlexClkGenReg(xbarreg)  & ~(STM_FLEX_CLKGEN_CROSSBAR_SEL_MASK << xbarshift);

  val |= ((uint32_t)src | STM_FLEX_CLKGEN_CROSSBAR_ENABLE) << xbarshift;

  WriteFlexClkGenReg(xbarreg, val);

  const uint32_t predivreg = STM_FLEX_CLKGEN_PRE_DIV_0 + (output*4);
  WriteFlexClkGenReg(predivreg, 0); /* Always divide by one in this case */

  switch(div)
  {
    case STM_CLK_DIV_1:
      val = 0;
      break;
    case STM_CLK_DIV_2:
      val = 1;
      break;
    case STM_CLK_DIV_4:
      val = 3;
      break;
    case STM_CLK_DIV_8:
      val = 7;
      break;
  }

  /*
   * All video clocks derived from the same crossbar source need to form a
   * semi-synchronous group.
   *
   * TODO: work out how to program the semi-sync beat counter for specific
   *       clock divides, given we are using simple power of two divides will
   *       this just be the largest divide value in the group? How do we know
   *       what that is?
   */
  const uint32_t divreg = STM_FLEX_CLKGEN_FINAL_DIV_0 + (output*4);
  val |= (STM_FLEX_CLKGEN_FINALDIV_OUTPUT_EN | STM_FLEX_CLKGEN_FINALDIV_SEMI_SYNC_EN);
  WriteFlexClkGenReg(divreg, val);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407ClkDivider::Disable(stm_clk_divider_output_name_t name)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  int output;
  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::Disable: Clock %d NOT FOUND!!", name );
    return false;
  }

  const uint32_t reg = STM_FLEX_CLKGEN_FINAL_DIV_0 + (output*4);
  uint32_t val = ReadFlexClkGenReg(reg) & ~STM_FLEX_CLKGEN_FINALDIV_OUTPUT_EN;
  WriteFlexClkGenReg(reg, val);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407ClkDivider::isEnabled(stm_clk_divider_output_name_t name) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  int output;
  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::isEnabled: Clock %d NOT FOUND!!", name );
    return false;
  }

  const uint32_t reg = STM_FLEX_CLKGEN_FINAL_DIV_0 + (output*4);
  uint32_t val = ReadFlexClkGenReg(reg) & STM_FLEX_CLKGEN_FINALDIV_OUTPUT_EN;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return (val != 0);
}


bool CSTiH407ClkDivider::getDivide(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_divide_t *div) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  int output;
  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::getDivide: Clock %d NOT FOUND!!", name );
    return false;
  }

  const uint32_t reg = STM_FLEX_CLKGEN_FINAL_DIV_0 + (output*4);
  uint32_t val = (ReadFlexClkGenReg(reg) & STM_FLEX_CLKGEN_FINALDIV_DIVISOR_MASK) +1;

  switch(val)
  {
    case 1:
      *div = STM_CLK_DIV_1;
      break;
    case 2:
      *div = STM_CLK_DIV_2;
      break;
    case 4:
      *div = STM_CLK_DIV_4;
      break;
    case 8:
      *div = STM_CLK_DIV_8;
      break;
    default:
      TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::getDivide: Unsupported divide (%u)!!", val );
      return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407ClkDivider::getSource(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_source_t *src) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  int output;
  if(!lookupOutput(name,&output))
  {
    TRC( TRC_ID_ERROR, "CSTiH407ClkDivider::getSource: Clock %d NOT FOUND!!", name );
    return false;
  }

  const uint32_t reg = STM_FLEX_CLKGEN_CROSSBAR + ((output/4)*4);
  const uint32_t shift = (output%4) * 8;
  uint32_t val = (ReadFlexClkGenReg(reg) >> shift ) & STM_FLEX_CLKGEN_CROSSBAR_SEL_MASK;

  *src = (stm_clk_divider_output_source_t) val;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
