/***********************************************************************
 *
 * File: soc/sti5206/sti5206clkdivider.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti5206device.h"
#include "sti5206clkdivider.h"

static const stm_clk_divider_output_name_t outputMap[] = {
    STM_CLK_PIX_ED,
    STM_CLK_DISP_ED,
    STM_CLK_PIX_SD,
    STM_CLK_DISP_SD,
    STM_CLK_656,
    STM_CLK_GDP1,
    STM_CLK_PIP
};


CSTi5206ClkDivider::CSTi5206ClkDivider(CSTi5206Device *pDev): CSTmTVOutClkDivider(outputMap, N_ELEMENTS(outputMap))
{
  m_pSysCfgReg = ((ULONG*)pDev->GetCtrlRegisterBase()) + (STi5206_SYSCFG_BASE>>2);
}


bool CSTi5206ClkDivider::Enable(stm_clk_divider_output_name_t   name,
                                stm_clk_divider_output_source_t src,
                                stm_clk_divider_output_divide_t div)
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG15);
  val &= ~(SYS_CFG15_CLK_DIV_MASK << SYS_CFG15_CLK_DIV_SHIFT(output));

  switch(div)
  {
    case STM_CLK_DIV_1:
      val |= SYS_CFG15_CLK_DIV_BYPASS << SYS_CFG15_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_2:
      val |= SYS_CFG15_CLK_DIV_2 << SYS_CFG15_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_4:
      val |= SYS_CFG15_CLK_DIV_4 << SYS_CFG15_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_8:
      val |= SYS_CFG15_CLK_DIV_8 << SYS_CFG15_CLK_DIV_SHIFT(output);
      break;
    default:
      return false;
  }

  switch(src)
  {
    case STM_CLK_SRC_0:
      val &= ~SYS_CFG15_CLK_SEL_SD(output);
      break;
    case STM_CLK_SRC_1:
      val |= SYS_CFG15_CLK_SEL_SD(output);
      break;
    default:
      return false;
  }

  val |= SYS_CFG15_CLK_EN(output);

  WriteSysCfgReg(SYS_CFG15, val);

  DEXIT();
  return true;
}


bool CSTi5206ClkDivider::Disable(stm_clk_divider_output_name_t name)
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG15) & ~SYS_CFG15_CLK_EN(output);
  WriteSysCfgReg(SYS_CFG15, val);

  DEXIT();
  return true;
}


bool CSTi5206ClkDivider::isEnabled(stm_clk_divider_output_name_t name) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG15) & SYS_CFG15_CLK_EN(output);

  DEXIT();
  return (val != 0);
}


bool CSTi5206ClkDivider::getDivide(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_divide_t *div) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG15);
  val &= (SYS_CFG15_CLK_DIV_MASK << SYS_CFG15_CLK_DIV_SHIFT(output));
  val >>= SYS_CFG15_CLK_DIV_SHIFT(output);
  switch(val)
  {
    case SYS_CFG15_CLK_DIV_BYPASS:
      *div = STM_CLK_DIV_1;
      break;
    case SYS_CFG15_CLK_DIV_2:
      *div = STM_CLK_DIV_2;
      break;
    case SYS_CFG15_CLK_DIV_4:
      *div = STM_CLK_DIV_4;
      break;
    case SYS_CFG15_CLK_DIV_8:
      *div = STM_CLK_DIV_8;
      break;
    default:
      return false; // Impossible but keep the compiler quiet.
  }

  DEXIT();
  return true;
}


bool CSTi5206ClkDivider::getSource(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_source_t *src) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG15) & SYS_CFG15_CLK_SEL_SD(output);

  *src = (val == 0)?STM_CLK_SRC_0:STM_CLK_SRC_1;

  DEXIT();
  return true;
}
