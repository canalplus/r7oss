/***********************************************************************
 *
 * File: soc/sti7108/sti7108clkdivider.cpp
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

#include "sti7108device.h"
#include "sti7108clkdivider.h"

static const stm_clk_divider_output_name_t outputMap[] = {
    STM_CLK_PIX_HD,
    STM_CLK_DISP_HD,
    STM_CLK_DISP_PIP,
    STM_CLK_GDP1,
    STM_CLK_GDP2,
    STM_CLK_DISP_SD,
    STM_CLK_PIX_SD,
    STM_CLK_656
};


CSTi7108ClkDivider::CSTi7108ClkDivider(CSTi7108Device *pDev): CSTmTVOutClkDivider(outputMap, N_ELEMENTS(outputMap))
{
  m_pSysCfgReg = ((ULONG*)pDev->GetCtrlRegisterBase()) + (STi7108_SYSCFG_BANK3_BASE>>2);
}


bool CSTi7108ClkDivider::Enable(stm_clk_divider_output_name_t   name,
                                stm_clk_divider_output_source_t src,
                                stm_clk_divider_output_divide_t div)
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG1);
  val &= ~(SYS_CFG1_CLK_DIV_MASK << SYS_CFG1_CLK_DIV_SHIFT(output));

  switch(div)
  {
    case STM_CLK_DIV_1:
      val |= SYS_CFG1_CLK_DIV_BYPASS << SYS_CFG1_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_2:
      val |= SYS_CFG1_CLK_DIV_2 << SYS_CFG1_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_4:
      val |= SYS_CFG1_CLK_DIV_4 << SYS_CFG1_CLK_DIV_SHIFT(output);
      break;
    case STM_CLK_DIV_8:
      val |= SYS_CFG1_CLK_DIV_8 << SYS_CFG1_CLK_DIV_SHIFT(output);
      break;
    default:
      return false;
  }

  WriteSysCfgReg(SYS_CFG1, val);

  val = ReadSysCfgReg(SYS_CFG0);

  switch(src)
  {
    case STM_CLK_SRC_0:
      val &= ~SYS_CFG0_CLK_SEL_SD(output);
      break;
    case STM_CLK_SRC_1:
      val |= SYS_CFG0_CLK_SEL_SD(output);
      break;
    default:
      return false;
  }

  val &= ~SYS_CFG0_CLK_STOP(output);

  WriteSysCfgReg(SYS_CFG0, val);

  DEXIT();
  return true;
}


bool CSTi7108ClkDivider::Disable(stm_clk_divider_output_name_t name)
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG0) | SYS_CFG0_CLK_STOP(output);
  WriteSysCfgReg(SYS_CFG0, val);

  DEXIT();
  return true;
}


bool CSTi7108ClkDivider::isEnabled(stm_clk_divider_output_name_t name) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG0) & SYS_CFG0_CLK_STOP(output);

  DEXIT();
  return (val == 0);
}


bool CSTi7108ClkDivider::getDivide(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_divide_t *div) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG1);
  val &= (SYS_CFG1_CLK_DIV_MASK << SYS_CFG1_CLK_DIV_SHIFT(output));
  val >>= SYS_CFG1_CLK_DIV_SHIFT(output);
  switch(val)
  {
    case SYS_CFG1_CLK_DIV_BYPASS:
      *div = STM_CLK_DIV_1;
      break;
    case SYS_CFG1_CLK_DIV_2:
      *div = STM_CLK_DIV_2;
      break;
    case SYS_CFG1_CLK_DIV_4:
      *div = STM_CLK_DIV_4;
      break;
    case SYS_CFG1_CLK_DIV_8:
      *div = STM_CLK_DIV_8;
      break;
    default:
      return false; // Impossible but keep the compiler quiet.
  }

  DEXIT();
  return true;
}


bool CSTi7108ClkDivider::getSource(stm_clk_divider_output_name_t    name,
                                   stm_clk_divider_output_source_t *src) const
{
  DENTRY();

  int output;
  if(!lookupOutput(name,&output))
    return false;

  ULONG val = ReadSysCfgReg(SYS_CFG0) & SYS_CFG0_CLK_SEL_SD(output);

  *src = (val == 0)?STM_CLK_SRC_0:STM_CLK_SRC_1;

  DEXIT();
  return true;
}
