/***********************************************************************
 *
 * File: soc/sti7108/sti7108dvo.cpp
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

#include "sti7108dvo.h"
#include "sti7108clkdivider.h"
#include "sti7108reg.h"

CSTi7108DVO::CSTi7108DVO(CDisplayDevice     *pDev,
                         COutput            *pMasterOutput,
                         CSTi7108ClkDivider *pClkDivider,
                         ULONG               ulDVOBase,
                         ULONG               ulHDFormatterBase): CSTmHDFDVO(pDev, ulDVOBase, ulHDFormatterBase, pMasterOutput)
{
  DENTRY();
  m_pClkDivider = pClkDivider;
  DEXIT();
}


bool CSTi7108DVO::SetOutputFormat(ULONG format,const stm_mode_line_t* pModeLine)
{
  DENTRY();

  if(!CSTmHDFDVO::SetOutputFormat(format,pModeLine))
    return false;

  if(pModeLine)
  {
    stm_clk_divider_output_divide_t hddispdiv;
    stm_clk_divider_output_divide_t fdvodiv;

    m_pClkDivider->getDivide(STM_CLK_DISP_HD,&hddispdiv);

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
        fdvodiv = hddispdiv;
        break;
      }
      case STM_VIDEO_OUT_YUV:
      case STM_VIDEO_OUT_ITUR656:
      {
        switch(hddispdiv)
        {
          case STM_CLK_DIV_2:
            fdvodiv = STM_CLK_DIV_1;
            break;
          case STM_CLK_DIV_4:
            fdvodiv = STM_CLK_DIV_2;
            break;
          case STM_CLK_DIV_8:
            fdvodiv = STM_CLK_DIV_4;
            break;
          case STM_CLK_DIV_1:
          default:
            /*
             * DVO clock 2x pixel clock, not possible if pixel clock == reference clock
             */
            DEBUGF2(1,("%s: Cannot set 2xPIX clock\n",__PRETTY_FUNCTION__));
            return false;
        }

        DEBUGF2(2,("%s: FDVO clock == 2xPIX clock\n",__PRETTY_FUNCTION__));
        break;
      }
      default:
        DERROR("Invalid format requested\n");
        return false;
    }

    m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, fdvodiv);
  }

  DEXIT();
  return true;
}
