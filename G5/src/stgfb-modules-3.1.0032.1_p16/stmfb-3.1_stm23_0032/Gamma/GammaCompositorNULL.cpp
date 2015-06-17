/***********************************************************************\
 *
 * File: Gamma/GammaCompositorNULL.cpp
 * Copyright (c) 2004-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Generic/Output.h>

#include "GenericGammaDefs.h"
#include "GenericGammaReg.h"

#include "GammaCompositorNULL.h"

static const SURF_FMT g_surfaceFormats[] = {
  SURF_RGB565,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_ARGB8888,
  SURF_BGRA8888,
  SURF_RGB888,
  SURF_ARGB8565,
  SURF_YCBCR422R,
  SURF_CRYCB888,
  SURF_ACRYCB8888
};


static const SURF_FMT g_surfaceFormatsWithClut[] = {
  SURF_RGB565,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_ARGB8888,
  SURF_BGRA8888,
  SURF_RGB888,
  SURF_ARGB8565,
  SURF_YCBCR422R,
  SURF_CRYCB888,
  SURF_ACRYCB8888,
  SURF_CLUT8,
  SURF_ACLUT88
};

CGammaCompositorNULL::CGammaCompositorNULL(stm_plane_id_t NULLid):CGammaCompositorPlane(NULLid)
{
  DEBUGF2(2,("CGammaCompositorNULL::CGammaCompositorNULL id = %d\n",NULLid));

  m_pSurfaceFormats = g_surfaceFormatsWithClut;
  m_nFormats = sizeof(g_surfaceFormatsWithClut)/sizeof(SURF_FMT);

  /*
   * The NULL sample rate converters have an n.8 fixed point format,
   * but to get better registration with video planes we do the maths in n.13
   * and then round it before use to reduce the fixed point error between the
   * two. Not doing this is particularly noticeable with DVD menu highlights
   * when upscaling to HD output.
   */
  m_fixedpointONE     = 1<<13;

  /*
   * Do not assume scaling is available, SoC specific subclasses will
   * override this in their constructors.
   */
  m_ulMaxHSrcInc   = m_fixedpointONE;
  m_ulMinHSrcInc   = m_fixedpointONE;
  m_ulMaxVSrcInc   = m_fixedpointONE;
  m_ulMinVSrcInc   = m_fixedpointONE;

  m_capabilities.ulCaps      = (PLANE_CAPS_RESIZE                 |
                                PLANE_CAPS_SRC_COLOR_KEY          |
                                PLANE_CAPS_PREMULTIPLED_ALPHA     |
                                PLANE_CAPS_RESCALE_TO_VIDEO_RANGE);// |
  //                                PLANE_CAPS_GLOBAL_ALPHA);

  m_capabilities.ulControls  = 0;//(PLANE_CTRL_CAPS_GAIN |
				 // PLANE_CTRL_CAPS_ALPHA_RAMP);

  m_capabilities.ulMinFields =    1;
  m_capabilities.ulMinWidth  =   32;
  m_capabilities.ulMinHeight =   24;
  m_capabilities.ulMaxWidth  = 1920;
  m_capabilities.ulMaxHeight = 1088;

  /*
   * Note: the scaling capabilities are calculated in the Create method as
   *       the sample rate limits may be overriden.
   */

  DEBUGF2(2,("CGammaCompositorNULL::CGammaCompositorNULL out\n"));
}


CGammaCompositorNULL::~CGammaCompositorNULL(void)
{
  DENTRY();

  DENTRY();
}


bool CGammaCompositorNULL::Create(void)
{
  DENTRY();

  if(!CDisplayPlane::Create())
    return false;

  SetScalingCapabilities(&m_capabilities);

  DEXIT();
  return true;
}

bool CGammaCompositorNULL::QueueBuffer(const stm_display_buffer_t* const pBuffer,
                                      const void                * const user)
{
  if (pBuffer->info.pDisplayCallback)
    pBuffer->info.pDisplayCallback(pBuffer->info.pUserData, 0);

  if (pBuffer->info.pCompletedCallback)
    pBuffer->info.pCompletedCallback(pBuffer->info.pUserData, 0);

  return true;
}


void CGammaCompositorNULL::UpdateHW(bool isDisplayInterlaced,
                                   bool isTopFieldOnDisplay,
                                   const TIME64 &vsyncTime)
{

}

bool CGammaCompositorNULL::SetControl(stm_plane_ctrl_t control, ULONG value)
{
  DEBUGF2(2,(FENTRY ": control = %d ulNewVal = %lu (0x%08lx)\n",__PRETTY_FUNCTION__,(int)control,value,value));

  return CGammaCompositorPlane::SetControl(control,value);
}


bool CGammaCompositorNULL::GetControl(stm_plane_ctrl_t control, ULONG *value) const
{
  DEBUGF2(2,(FENTRY ": control = %d\n",__PRETTY_FUNCTION__,(int)control));

  return CGammaCompositorPlane::GetControl(control,value);
}


void CGammaCompositorNULL::EnableHW(void)
{
  DENTRY();

  //    CDisplayPlane::EnableHW();

  DEXIT();
}
