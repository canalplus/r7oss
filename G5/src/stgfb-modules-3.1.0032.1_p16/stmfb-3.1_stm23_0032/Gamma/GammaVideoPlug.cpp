/*******************************************************************************
 *
 * File: Gamma/GammaVideoPlug.cpp
 * Copyright (c) 2006 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ******************************************************************************/
#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "GenericGammaDefs.h"
#include "GenericGammaReg.h"

#include "GammaCompositorPlane.h"
#include "GammaVideoPlug.h"

//Video PLUG Register offsets
#define PLUG_CTL          0x000
#define PLUG_ALP          0x004
#define PLUG_VPO          0x00c
#define PLUG_VPS          0x010
#define PLUG_KEY1         0x028
#define PLUG_KEY2         0x02c
#define PLUG_MPR1         0x030
#define PLUG_MPR2         0x034
#define PLUG_MPR3         0x038
#define PLUG_MPR4         0x03c
//Addition registers for 7100&7109
#define PLUG_BC           0x070
#define PLUG_TINT         0x074
#define PLUG_CSAT         0x078

//Video PLUG register bit definitions
#define PLUG_CTL__IGNORE_MX2                        0x80000000
#define PLUG_CTL__IGNORE_MX1                        0x40000000
#define PLUG_CTL__SIGNED_CHROMA                     0x04000000
#define PLUG_CTL__709_COLORIMETRY                   0x02000000
#define PLUG_CTL__601_COLORIMETRY                   0x00000000
#define PLUG_CTL__KEY_CR_IGNORE                     0x00000000
#define PLUG_CTL__KEY_CR_INRANGE_MATCH              0x00100000
#define PLUG_CTL__KEY_CR_OUTRANGE_MATCH             0x00300000
#define PLUG_CTL__KEY_Y_IGNORE                      0x00000000
#define PLUG_CTL__KEY_Y_INRANGE_MATCH               0x00040000
#define PLUG_CTL__KEY_Y_OUTRANGE_MATCH              0x000c0000
#define PLUG_CTL__KEY_CB_IGNORE                     0x00000000
#define PLUG_CTL__KEY_CB_INRANGE_MATCH              0x00010000
#define PLUG_CTL__KEY_CB_OUTRANGE_MATCH             0x00030000
#define PLUG_CTL__KEY_ENABLE                        0x00004000
#define PLUG_CTL__ALPHA_V_BORDER_ENABLE             0x00002000
#define PLUG_CTL__ALPHA_H_BORDER_ENABLE             0x00001000
// 7100 and 7109 additions to the Video PLUG
#define PLUG_CTL__PSI_SAT_ENABLE                    0x00000004
#define PLUG_CTL__PSI_TINT_ENABLE                   0x00000002
#define PLUG_CTL__PSI_BC_ENABLE                     0x00000001

#define PLUG_ALP__ALPHA_TRANSPARENT                 0x00000000
#define PLUG_ALP__ALPHA_OPAQUE                      0x00000080

#define PLUG_BRIGHTNESS_MASK                        0xff
#define PLUG_BRIGHTNESS_SHIFT                       0x0
#define PLUG_CONTRAST_MASK                          0xff
#define PLUG_CONTRAST_SHIFT                         0x8
#define PLUG_TINT_MASK                              0x3f
#define PLUG_TINT_SHIFT                             0x2
#define PLUG_CSAT_MASK                              0x3f
#define PLUG_CSAT_SHIFT                             0x2

/*
 * Explicit color conversion matricies.
 *
 * The coefficients are as close to the required values as is possible given the
 * available representation.
 */
static const ULONG mpr601_table[4] = {
  0x0a800000,
  0x0aaf0000, /* R = Y+1.3672Cr          */
  0x094d0754, /* G = Y-0.6992Cr-0.3359Cb */
  0x00000ade  /* B = Y+1.7344Cb          */
};


static const ULONG mpr709_table[4] = {
  0x0a800000,
  0x0ac50000, /* R = Y+1.5391Cr          */
  0x07150545, /* G = Y-0.4590Cr-0.1826Cb */
  0x00000ae8  /* B = Y+1.8125Cb          */
};


CGammaVideoPlug::CGammaVideoPlug(ULONG baseAddr, bool bEnablePSI, bool bUseMPR)
{
  m_plugBaseAddr = baseAddr;
  m_bEnablePSI   = bEnablePSI;
  m_bUseMPR      = bUseMPR;
  m_bVisible     = true;
  m_ulBrightness = 128;
  m_ulContrast   = 128;
  m_ulSaturation = 128;
  m_ulTint       = 128;

  m_ColorKeyConfig.flags  = static_cast<StmColorKeyConfigFlags>(SCKCF_ENABLE | SCKCF_FORMAT | SCKCF_R_INFO | SCKCF_G_INFO | SCKCF_B_INFO);
  m_ColorKeyConfig.enable = 0;
  m_ColorKeyConfig.format = SCKCVF_RGB;
  m_ColorKeyConfig.r_info = SCKCCM_DISABLED;
  m_ColorKeyConfig.g_info = SCKCCM_DISABLED;
  m_ColorKeyConfig.b_info = SCKCCM_DISABLED;

  CreatePSISetup();
}


CGammaVideoPlug::~CGammaVideoPlug(void) {}


void CGammaVideoPlug::updateColorKeyState (stm_color_key_config_t       * const dst,
					   const stm_color_key_config_t * const src) const
{
  /* argh, this should be possible in a nicer way... */
  dst->flags = static_cast<StmColorKeyConfigFlags>(src->flags | dst->flags);
  if (src->flags & SCKCF_ENABLE)
    dst->enable = src->enable;
  if (src->flags & SCKCF_FORMAT)
    dst->format = src->format;
  if (src->flags & SCKCF_R_INFO)
    dst->r_info = src->r_info;
  if (src->flags & SCKCF_G_INFO)
    dst->g_info = src->g_info;
  if (src->flags & SCKCF_B_INFO)
    dst->b_info = src->b_info;
  if (src->flags & SCKCF_MINVAL)
    dst->minval = src->minval;
  if (src->flags & SCKCF_MAXVAL)
    dst->maxval = src->maxval;
}

bool CGammaVideoPlug::CreatePlugSetup(VideoPlugSetup                &plugSetup,
                                      const stm_display_buffer_t    * const pFrame,
                                      const GAMMA_QUEUE_BUFFER_INFO &qbinfo)
{
  UCHAR ucRCR = 0;
  UCHAR ucGY  = 0;
  UCHAR ucBCB = 0;

  plugSetup.VIDn_VPO = qbinfo.viewportStartPixel | (qbinfo.viewportStartLine << 16);
  plugSetup.VIDn_VPS = qbinfo.viewportStopPixel  | (qbinfo.viewportStopLine  << 16);

  DEBUGF2(3,("CGammaVideoPlug::CreatePlugSetup VPO = %#08lx\n", plugSetup.VIDn_VPO));
  DEBUGF2(3,("CGammaVideoPlug::CreatePlugSetup VPS = %#08lx\n", plugSetup.VIDn_VPS));

  plugSetup.VIDn_CTL = 0;

  if(pFrame->src.ulFlags & STM_PLANE_SRC_COLORSPACE_709)
  {
    DEBUGF2(3,("CGammaVideoPlug::CreatePlugSetup 'colorspace set to ITU-R.709'\n"));
    if(m_bUseMPR)
      plugSetup.mpr = mpr709_table;
    else
      plugSetup.VIDn_CTL |= PLUG_CTL__709_COLORIMETRY;
  }
  else
  {
    DEBUGF2(3,("CGammaVideoPlug::CreatePlugSetup 'colorspace set to ITU-R.601'\n"));
    if(m_bUseMPR)
      plugSetup.mpr = mpr601_table;
    else
      plugSetup.VIDn_CTL |= PLUG_CTL__601_COLORIMETRY;
  }

  if(pFrame->src.ulFlags & STM_PLANE_SRC_CONST_ALPHA)
    plugSetup.VIDn_ALP = (pFrame->src.ulConstAlpha+1)>>1;
  else
    plugSetup.VIDn_ALP = PLUG_ALP__ALPHA_OPAQUE;

  // The video composition does not support destination colour keying
  if((pFrame->dst.ulFlags & (STM_PLANE_DST_COLOR_KEY | STM_PLANE_DST_COLOR_KEY_INV)) != 0)
    return false;

  updateColorKeyState (&m_ColorKeyConfig, &pFrame->src.ColorKey);

  // If colour keying not required, do nothing.
  if (m_ColorKeyConfig.enable == 0)
    return true;

  //Get Min Key value
  if (!(m_ColorKeyConfig.flags & SCKCF_MINVAL)
      || !CGammaCompositorPlane::GetRGBYCbCrKey (ucRCR, ucGY, ucBCB,
						 m_ColorKeyConfig.minval,
						 pFrame->src.ulColorFmt,
						 m_ColorKeyConfig.format == SCKCVF_RGB))
  {
    DEBUGF2(2,("%s: 'Min key value not obtained'.\n", __PRETTY_FUNCTION__));
    return false;
  }
  plugSetup.VIDn_KEY1 = (ucBCB | (ucGY<<8) | (ucRCR<<16));

  //Get Max Key value
  if (!(m_ColorKeyConfig.flags & SCKCF_MAXVAL)
      || !CGammaCompositorPlane::GetRGBYCbCrKey (ucRCR, ucGY, ucBCB,
						 m_ColorKeyConfig.maxval,
						 pFrame->src.ulColorFmt,
						 m_ColorKeyConfig.format == SCKCVF_RGB))
  {
    DEBUGF2(2,("%s: 'Max key value not obtained'.\n", __PRETTY_FUNCTION__));
    return false;
  }
  plugSetup.VIDn_KEY2 = (ucBCB | (ucGY<<8) | (ucRCR<<16));

  ULONG ulCtrl = PLUG_CTL__KEY_ENABLE;

  switch (m_ColorKeyConfig.r_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= PLUG_CTL__KEY_CR_INRANGE_MATCH; break;
  case SCKCCM_INVERSE: ulCtrl |= PLUG_CTL__KEY_CR_OUTRANGE_MATCH; break;
  default: return false;
  }

  switch (m_ColorKeyConfig.g_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= PLUG_CTL__KEY_Y_INRANGE_MATCH; break;
  case SCKCCM_INVERSE: ulCtrl |= PLUG_CTL__KEY_Y_OUTRANGE_MATCH; break;
  default: return false;
  }

  switch (m_ColorKeyConfig.b_info) {
  case SCKCCM_DISABLED: break;
  case SCKCCM_ENABLED: ulCtrl |= PLUG_CTL__KEY_CB_INRANGE_MATCH; break;
  case SCKCCM_INVERSE: ulCtrl |= PLUG_CTL__KEY_CB_OUTRANGE_MATCH; break;
  default: return false;
  }

  plugSetup.VIDn_CTL |= ulCtrl;

  return true;
}


bool CGammaVideoPlug::SetControl(stm_plane_ctrl_t control, ULONG value)
{
  DEBUGF2(2,("%s\n", __PRETTY_FUNCTION__));

  switch(control)
  {
    case PLANE_CTRL_PSI_BRIGHTNESS:
      if(!m_bEnablePSI || value > 255)
	return false;
      m_ulBrightness = value;
      break;

    case PLANE_CTRL_PSI_SATURATION:
      if(!m_bEnablePSI || value > 255)
        return false;
      m_ulSaturation = value;
      break;

    case PLANE_CTRL_PSI_CONTRAST:
      if(!m_bEnablePSI || value > 255)
        return false;
      m_ulContrast = value;
      break;

    case PLANE_CTRL_PSI_TINT:
      if(!m_bEnablePSI || value > 255)
        return false;
      m_ulTint = value;
      break;

    case PLANE_CTRL_COLOR_KEY:
      {
      const stm_color_key_config_t * const config = reinterpret_cast<stm_color_key_config_t *> (value);
      if (!config)
	return false;
      updateColorKeyState (&m_ColorKeyConfig, config);
      }
      break;

    default:
      return false;
  }

  CreatePSISetup();

  return true;
}


bool CGammaVideoPlug::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
  DEBUGF2(2,("%s\n", __PRETTY_FUNCTION__));

  switch(control)
  {
    case PLANE_CTRL_PSI_BRIGHTNESS:
      if(!m_bEnablePSI)
	return false;
      *value = m_ulBrightness;
      break;
    case PLANE_CTRL_PSI_SATURATION:
      if(!m_bEnablePSI)
	return false;
      *value = m_ulSaturation;
      break;
    case PLANE_CTRL_PSI_CONTRAST:
      if(!m_bEnablePSI)
	return false;
      *value = m_ulContrast;
      break;
    case PLANE_CTRL_PSI_TINT:
      if(!m_bEnablePSI)
	return false;
      *value = m_ulTint;
      break;

    case PLANE_CTRL_COLOR_KEY:
      {
      stm_color_key_config_t * const config = reinterpret_cast<stm_color_key_config_t *> (value);
      if (!config)
	return false;
      config->flags = SCKCF_NONE;
      updateColorKeyState (config, &m_ColorKeyConfig);
      }
      break;

    default:
      return false;
  }

  return true;
}


void CGammaVideoPlug::CreatePSISetup(void)
{
  signed long brightness = (signed long)m_ulBrightness - 128; // Convert unsigned 8bit to signed 8bit
  signed long tint       = (signed long)(m_ulTint/4) - 32;    // Convert unsigned 8bit to signed 6bit

  DEBUGF2(2,("%s\n", __PRETTY_FUNCTION__));

  m_VIDn_BC   = brightness & PLUG_BRIGHTNESS_MASK;
  m_VIDn_BC  |= (m_ulContrast & PLUG_CONTRAST_MASK) << PLUG_CONTRAST_SHIFT;
  m_VIDn_CSAT = ((m_ulSaturation/4) & PLUG_CSAT_MASK) << PLUG_CSAT_SHIFT; // Convert to unsigned 6bit
  m_VIDn_TINT = (tint & PLUG_TINT_MASK) << PLUG_TINT_SHIFT;
}


void CGammaVideoPlug::WritePlugSetup(const VideoPlugSetup &setup)
{
  ULONG ctl = setup.VIDn_CTL;

  if(!m_bVisible)
    ctl |= (PLUG_CTL__IGNORE_MX1 | PLUG_CTL__IGNORE_MX2);

  if(m_bEnablePSI)
  {
     ctl |= (PLUG_CTL__PSI_SAT_ENABLE |
             PLUG_CTL__PSI_BC_ENABLE  |
             PLUG_CTL__PSI_TINT_ENABLE);
  }

  WritePLUGReg(PLUG_CTL, ctl);


  WritePLUGReg(PLUG_VPO,  setup.VIDn_VPO);
  WritePLUGReg(PLUG_VPS,  setup.VIDn_VPS);
  WritePLUGReg(PLUG_ALP,  setup.VIDn_ALP);
  WritePLUGReg(PLUG_KEY1, setup.VIDn_KEY1);
  WritePLUGReg(PLUG_KEY2, setup.VIDn_KEY2);

  if(m_bEnablePSI)
  {
    WritePLUGReg(PLUG_BC,   m_VIDn_BC);
    WritePLUGReg(PLUG_CSAT, m_VIDn_CSAT);
    WritePLUGReg(PLUG_TINT, m_VIDn_TINT);
  }

  if(setup.mpr)
  {
    WritePLUGReg(PLUG_MPR1, setup.mpr[0]);
    WritePLUGReg(PLUG_MPR2, setup.mpr[1]);
    WritePLUGReg(PLUG_MPR3, setup.mpr[2]);
    WritePLUGReg(PLUG_MPR4, setup.mpr[3]);
  }
}
