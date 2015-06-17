/***********************************************************************
 *
 * File: soc/sti7111/sti7111dvo.cpp
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

#include <Generic/DisplayDevice.h>

#include <HDTVOutFormatter/stmtvoutreg.h>

#include "sti7111dvo.h"
#include "sti7111reg.h"


CSTi7111DVO::CSTi7111DVO(CDisplayDevice *pDev,
                         COutput        *pMasterOutput,
                         ULONG           ulDVOBase,
                         ULONG           ulHDFormatterBase): CSTmHDFDVO(pDev,ulDVOBase,ulHDFormatterBase,pMasterOutput)
{
  DENTRY();

  m_pTVOutRegs   = (ULONG*)pDev->GetCtrlRegisterBase() + (STi7111_MAIN_TVOUT_BASE>>2);
  m_pClkGenBRegs = (ULONG*)pDev->GetCtrlRegisterBase() + (STi7111_CLKGEN_BASE>>2);

  DEXIT();
}


CSTi7111DVO::~CSTi7111DVO() {}


bool CSTi7111DVO::SetOutputFormat(ULONG format,const stm_mode_line_t* pModeLine)
{
  DENTRY();

  if(!CSTmHDFDVO::SetOutputFormat(format,pModeLine))
    return false;

  if(pModeLine)
  {
    ULONG tvoutclkconfig = ReadTVOutReg(TVOUT_CLK_SEL) & ~TVOUT_MAIN_CLK_SEL_FDVO(TVOUT_CLK_MASK);
    /*
     * Note: the pixel clock setup is at the bottom of the clock config
     *       register, using it like this is OK.
     */
    ULONG tvoutpixclkdiv = (tvoutclkconfig & TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_MASK));

    /*
     * This is another case of apparently needing two completely separate clocks
     * set to the same speed for the same IP. In this case we derive from the
     * HDMI TMDS clock setup, which will match the pixel clock as we do not
     * have deepcolor on these devices and is at the bottom of the configuration
     * register.
     */
    ULONG clkgbconfig = ReadClkGenBReg(CKGB_DISPLAY_CFG) & ~CKGB_CFG_656(CKGB_CFG_MASK);
    ULONG clkgtmdsclkdiv = (clkgbconfig & CKGB_CFG_TMDS_HDMI(CKGB_CFG_MASK));

    switch(format)
    {
      case STM_VIDEO_OUT_RGB:
      case STM_VIDEO_OUT_YUV_24BIT:
      case STM_VIDEO_OUT_422:
      {
        /*
         * DVO clock same as pixel clock
         */
        DEBUGF2(2,("%s: FDVO clock == PIX clock\n",__PRETTY_FUNCTION__));
        tvoutclkconfig |= TVOUT_MAIN_CLK_SEL_FDVO(tvoutpixclkdiv);
        clkgbconfig    |= CKGB_CFG_656(clkgtmdsclkdiv);
        break;
      }
      case STM_VIDEO_OUT_YUV:
      case STM_VIDEO_OUT_ITUR656:
      {
        /*
         * DVO clock 2x pixel clock, not possible if pixel clock == reference clock
         */
        if((tvoutpixclkdiv == TVOUT_CLK_BYPASS) || (clkgtmdsclkdiv == CKGB_CFG_BYPASS))
        {
          DEBUGF2(1,("%s: Cannot set 2xPIX clock\n",__PRETTY_FUNCTION__));
          return false;
        }

        tvoutclkconfig |= TVOUT_MAIN_CLK_SEL_FDVO(tvoutpixclkdiv-1);

        switch(clkgtmdsclkdiv)
        {
          case CKGB_CFG_DIV2:
            clkgbconfig |= CKGB_CFG_656(CKGB_CFG_BYPASS);
            break;
          case CKGB_CFG_DIV4:
            clkgbconfig |= CKGB_CFG_656(CKGB_CFG_DIV2);
            break;
          case CKGB_CFG_DIV8:
            clkgbconfig |= CKGB_CFG_656(CKGB_CFG_DIV4);
            break;
          default:
            DERROR("bad clockgenb clock divide?\n");
            return false;
        }
        DEBUGF2(2,("%s: FDVO clock == 2xPIX clock\n",__PRETTY_FUNCTION__));
        break;
      }
      default:
        DEBUGF2(2,("%s: Invalid format?\n",__PRETTY_FUNCTION__));
        return false;
    }

    WriteTVOutReg(TVOUT_CLK_SEL,tvoutclkconfig);
    WriteClkGenBReg(CKGB_DISPLAY_CFG, clkgbconfig);
  }

  return true;
}
