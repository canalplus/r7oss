/***********************************************************************
 *
 * File: Gamma/GammaMixer.cpp
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
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

#include <STMCommon/stmviewport.h>

#include "GenericGammaReg.h"
#include "GenericGammaDefs.h"
#include "GammaMixer.h"

// Default Background RGB888 color
#define DEFAULT_BKG_COLOR 0x101060

#define MIX_CTL_709_NOT_601 (1L<<25)

#define CURSOR_DEPTH 8

CGammaMixer::CGammaMixer(CDisplayDevice* pDev,
                         ULONG           ulRegOffset,
                         stm_plane_id_t  ulVBILinkedPlane): CDisplayMixer(pDev)
{
  DENTRY();
  m_ulRegOffset = ulRegOffset;
  m_pGammaReg   = (ULONG*)m_pDev->GetCtrlRegisterBase();

  m_bHasYCbCrMatrix = true;
  m_colorspaceMode  = STM_YCBCR_COLORSPACE_AUTO_SELECT;

  m_crossbarSize = 0;
  m_ulBackground = DEFAULT_BKG_COLOR;

  m_lock = g_pIOS->CreateResourceLock();

  m_ulVBILinkedPlane = ulVBILinkedPlane;

  m_planesMask = ~0;

  DEXIT();
}


CGammaMixer::~CGammaMixer()
{
  g_pIOS->DeleteResourceLock(m_lock);
}

/* This can be overriden to provide more restrictions, e.g. mutually exclusive
 * planes on the 5528 aux mixer. Hence the result can be dynamic depending
 * on what other planes are enabled.
 */
bool CGammaMixer::CanEnablePlane(stm_plane_id_t planeID)
{
  if(planeID == OUTPUT_VBI)
    planeID = m_ulVBILinkedPlane;

  return PlaneValid(planeID);
}


bool CGammaMixer::EnablePlane(stm_plane_id_t planeID, stm_mixer_activation_t act)
{
  if(m_pCurrentMode == 0)
  {
    g_pIDebug->IDebugPrintf("%s: ERROR no video mode active (planeID = %d)\n",__PRETTY_FUNCTION__,(int)planeID);
    return false;
  }

  if(!CanEnablePlane(planeID))
    return false;

  SetMixerForPlanes(planeID, true, act);
  return true;
}


bool CGammaMixer::DisablePlane(stm_plane_id_t planeID)
{
  if(m_pCurrentMode == 0)
  {
    g_pIDebug->IDebugPrintf("%s: ERROR no video mode active (planeID = %d)\n",__PRETTY_FUNCTION__,(int)planeID);
    return false;
  }

  if(!CanEnablePlane(planeID))
    return false;

  SetMixerForPlanes(planeID, false, STM_MA_NONE);
  return true;
}

bool CGammaMixer::EnablePlaneForce(stm_plane_id_t planeID, stm_mixer_activation_t act)
{
  m_planesMask |= planeID;
  g_pIDebug->IDebugPrintf("%s: planeID = %d, m_planesMask = %08lX\n",__PRETTY_FUNCTION__, planeID, m_planesMask);
  EnablePlane(planeID, act);
}

bool CGammaMixer::DisablePlaneForce(stm_plane_id_t planeID)
{
  m_planesMask &= ~planeID;
  g_pIDebug->IDebugPrintf("%s: planeID = %d, m_planesMask = %08lX\n",__PRETTY_FUNCTION__, planeID, m_planesMask);
  DisablePlane(planeID);
}

void CGammaMixer::SetMixerForPlanes(stm_plane_id_t planeID, bool isEnabled, stm_mixer_activation_t act)
{
  ULONG tmp;

  DENTRY();

  /*
   * We have to do all of this with interrupts disabled so another plane's
   * vsync processing doesn't come in the middle and confuse the state.
   */
  g_pIOS->LockResource(m_lock);

  if(isEnabled)
    m_planeEnables |= planeID;
  else
    m_planeEnables &= ~planeID;

  DEBUGF2(2,("CGammaMixer::SetMixerForPlanes m_planeEnables  = 0x%lx\n",m_planeEnables));

  /*
   * Do not change the mixer state for VBI if its linked GDP is also enabled
   */
  if((planeID == OUTPUT_VBI) && (m_planeEnables & m_ulVBILinkedPlane))
    goto unlock_and_exit;

  /*
   * Do not change the mixer state for a linked GDP if its linked VBI plane is
   * also enabled.
   */
  if((planeID == m_ulVBILinkedPlane) && (m_planeEnables & OUTPUT_VBI))
    goto unlock_and_exit;

  if(planeID == OUTPUT_VBI)
    planeID = m_ulVBILinkedPlane;

  tmp = ReadMixReg(MXn_CTL_REG_OFFSET) & 0xffff0000;
  WriteMixReg(MXn_CTL_REG_OFFSET, (tmp | (m_planeEnables & 0xffff & m_planesMask)));

  if((ULONG)act & STM_MA_GRAPHICS)
    m_planeActive |= planeID;
  else
    m_planeActive &= ~planeID;

  if((ULONG)act & STM_MA_VIDEO)
    m_planeActive |= (ULONG)planeID<<16;
  else
    m_planeActive &= ~((ULONG)planeID<<16);

  DEBUGF2(2,("CGammaMixer::SetMixerForPlanes m_planeActive  = 0x%lx\n",m_planeActive));
  WriteMixReg(MXn_ACT_REG_OFFSET, m_planeActive);

unlock_and_exit:
  g_pIOS->UnlockResource(m_lock);

  DEXIT();
}


ULONG CGammaMixer::SupportedControls(void) const
{
  ULONG ctrlCaps = 0;

  if(PlaneValid(OUTPUT_BKG))
    ctrlCaps |= STM_CTRL_CAPS_BACKGROUND;

  return ctrlCaps;
}


void CGammaMixer::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch(ctrl)
  {
    case STM_CTRL_BACKGROUND_ARGB:
    {
      if(PlaneValid(OUTPUT_BKG))
      {
        m_ulBackground = ulNewVal;
        DEBUGF2(2,("%s: new colour = 0x%08lx\n", __PRETTY_FUNCTION__, m_ulBackground));
        WriteMixReg(MXn_BKC_REG_OFFSET, m_ulBackground);
      }
      break;
    }
    case STM_CTRL_YCBCR_COLORSPACE:
    {
      if(ulNewVal > STM_YCBCR_COLORSPACE_709)
        return;

      m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(ulNewVal);

      DEBUGF2(2,("%s: new colourspace mode = %d\n", __PRETTY_FUNCTION__, (int)m_colorspaceMode));

      if(m_pCurrentMode)
      {
        ULONG tmp = ReadMixReg(MXn_CTL_REG_OFFSET) & ~MIX_CTL_709_NOT_601;
        tmp |= GetColorspaceCTL(m_pCurrentMode);

        WriteMixReg(MXn_CTL_REG_OFFSET, tmp);
      }
      break;
    }
    case STM_CTRL_SET_MIXER_PLANES:
    {
      DEBUGF2(2,("%s: STM_CTRL_SET_MIXER_PLANES %08lX\n", __PRETTY_FUNCTION__, ulNewVal));
      int bits = sizeof(ulNewVal) * 8;
      int en = ulNewVal >> (bits - 1); /* msb = en */
      for(int i = 0; i < bits - 1; i++) {
        if(ulNewVal & 1) {
          DEBUGF2(2,("%s: %s plane %d\n", __PRETTY_FUNCTION__, en ? "enable" : "disable", i));
          stm_plane_id_t plane_id = (stm_plane_id_t)(1 << i);
          if(en) {
            EnablePlaneForce(plane_id);
          } else {
            DisablePlaneForce(plane_id);
          }
        }
        ulNewVal >>= 1;
      }
      break;
    }
    default:
      break;
  }
}


ULONG CGammaMixer::GetControl(stm_output_control_t ctrl) const
{
  switch(ctrl)
  {
    case STM_CTRL_BACKGROUND_ARGB:
      return m_ulBackground;
    case STM_CTRL_YCBCR_COLORSPACE:
      return m_colorspaceMode;
    default:
      return 0;
  }
}


ULONG CGammaMixer::GetColorspaceCTL(const stm_mode_line_t *pModeLine) const
{
  switch(m_colorspaceMode)
  {
    case STM_YCBCR_COLORSPACE_AUTO_SELECT:
      return (pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK)?MIX_CTL_709_NOT_601:0;
    case STM_YCBCR_COLORSPACE_709:
      return MIX_CTL_709_NOT_601;
    case STM_YCBCR_COLORSPACE_601:
    default:
      return 0;
  }
}


bool CGammaMixer::Start(const stm_mode_line_t *pModeLine)
{
  ULONG ystart, ystop, xstart, xstop;

  DEBUGF2(2,("CGammaMixer::Start in this = %p m_pDev = %p\n",this, m_pDev));

  ULONG ctl = 0;

  if(m_bHasYCbCrMatrix)
    ctl |= GetColorspaceCTL(pModeLine);

  WriteMixReg(MXn_CTL_REG_OFFSET, ctl);

  /*
   * Set Mixer viewport start, start on the first blanking line
   */
  xstart = STCalculateViewportPixel(pModeLine, 0);
  ystart = STCalculateViewportLine(pModeLine,  pModeLine->TimingParams.VSyncPulseWidth - pModeLine->ModeParams.FullVBIHeight);

  m_ulAVO = xstart | (ystart << 16);

  //Set Mixer background to start at the first active video line
  ystart = STCalculateViewportLine(pModeLine, 0);

  m_ulBCO = xstart | (ystart << 16);

  WriteMixReg(MXn_AVO_REG_OFFSET, m_ulAVO);

  DEBUGF2(2,("CGammaMixer::Start - AVO = %#08lx\n",m_ulAVO));

  /*
   * Set Active video stop, this is also the background stop as well
   */
  xstop = STCalculateViewportPixel(pModeLine, pModeLine->ModeParams.ActiveAreaWidth - 1);
  ystop = STCalculateViewportLine(pModeLine, pModeLine->ModeParams.ActiveAreaHeight - 1);

  m_ulAVS = xstop | (ystop << 16);
  m_ulBCS = xstop | (ystop << 16);

  WriteMixReg(MXn_AVS_REG_OFFSET, m_ulAVS);

  DEBUGF2(2,("CGammaMixer::Start - AVS = %#08lx\n",m_ulAVS));

  /*
   * Set current mode now so that enabling the background plane will succeed.
   */
  m_pCurrentMode = pModeLine;

  if((m_validPlanes & OUTPUT_BKG) == OUTPUT_BKG)
  {
    DEBUGF2(2,("CGammaMixer::Start - Enabling Background Colour\n"));
    DEBUGF2(2,("CGammaMixer::Start - BCO = %#08lx\n",m_ulBCO));
    DEBUGF2(2,("CGammaMixer::Start - BCS = %#08lx\n",m_ulBCS));
    DEBUGF2(2,("CGammaMixer::Start - BKC = %#08lx\n",m_ulBackground));
    WriteMixReg(MXn_BKC_REG_OFFSET, m_ulBackground);
    WriteMixReg(MXn_BCO_REG_OFFSET, m_ulBCO);
    WriteMixReg(MXn_BCS_REG_OFFSET, m_ulBCS);

    EnablePlane(OUTPUT_BKG);
  }

  DEBUGF2(2,("CGammaMixer::Start out\n"));

  return true;
}


void CGammaMixer::Stop(void)
{
  DEBUGF2(2,("CGammaMixer::Stop in m_pDev = %p\n",m_pDev));

  g_pIOS->LockResource(m_lock);

  if((m_planeEnables & ~(ULONG)OUTPUT_BKG) != 0)
  {
    g_pIDebug->IDebugPrintf("%s: ERROR stopping while planes (0x%08lx) still active \n",__PRETTY_FUNCTION__,m_planeEnables);
  }

  WriteMixReg(MXn_CTL_REG_OFFSET, 0);
  WriteMixReg(MXn_ACT_REG_OFFSET, 0);
  m_planeEnables = 0;
  m_planeActive = 0;
  m_pCurrentMode = 0;

  g_pIOS->UnlockResource(m_lock);

  DEBUGF2(2,("CGammaMixer::Stop out\n"));
}


/*
 * This implements a mixer with a fully configuration crossbar.
 * For chips that have partially or fully fixed crossbars these
 * methods may be overriden.
 */

static stm_plane_id_t crossbar_numbering[] = {
  OUTPUT_BKG, /* Dummy value, zero actually means no plane at this level */
  OUTPUT_VID1,
  OUTPUT_VID2,
  OUTPUT_GDP1,
  OUTPUT_GDP2,
  OUTPUT_GDP3,
  OUTPUT_GDP4
};


inline static ULONG crossbar_plane_lookup(stm_plane_id_t planeID)
{
  switch(planeID)
  {
    case OUTPUT_VID1:
      return MIX_CRB_VID1;
    case OUTPUT_VID2:
      return MIX_CRB_VID2;
    case OUTPUT_GDP1:
      return MIX_CRB_GDP1;
    case OUTPUT_GDP2:
      return MIX_CRB_GDP2;
    case OUTPUT_GDP3:
      return MIX_CRB_GDP3;
    case OUTPUT_GDP4:
      return MIX_CRB_GDP4;
    default:
      return MIX_CRB_NOTHING;
  }
}


bool CGammaMixer::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate)
{
  int original_order[7],new_order[7];

  DEBUGF2(2,("CGammaMixer::SetPlaneDepth planeID = %d depth = %d\n",(int)planeID,depth));

  if(!PlaneValid(planeID))
    return false;

  /*
   * The cursor is fixed at the highest possible level
   */
  if(planeID == OUTPUT_CUR)
  {
    return (depth == CURSOR_DEPTH)?true:false;
  }

  if(planeID == OUTPUT_NULL)
  {
    //return (depth == 0)?true:false;
    return true; // The NULL plane always succeeds
  }

  if(m_crossbarSize == 0)
  {
    int currentdepth;
    /*
     * No ability to change the position, but don't fail if the requested
     * depth is the current depth.
     */
    this->GetPlaneDepth(planeID, &currentdepth);

    return (depth == currentdepth);
  }

  if(depth < 1)
    depth = 1;

  if(depth > m_crossbarSize)
    depth = m_crossbarSize;

  /*
   * change the depth to an array index starting at 0
   */
  depth--;

  /*
   * This is a bit long winded, and the individual steps could probably
   * be merged together. However, that may impair readability and
   * performance is not a key issue for this operation.
   */
  ULONG crossbar_setup = ReadMixReg(MXn_CRB_REG_OFFSET);

  DEBUGF2(2,("CGammaMixer::SetPlaneDepth current crossbar_setup = 0x%08lx\n",crossbar_setup));

  /*
   * Extract the current plane order from the hardware register
   */
  for(int i=0;i<m_crossbarSize;i++)
  {
    int entry_shift = i*3;
    original_order[i] = (crossbar_setup >> entry_shift) & 0x7;
    DEBUGF2(2,("CGammaMixer::SetPlaneDepth current depth %d = %d\n",i+1,original_order[i]));
  }

  /*
   * Check to see if the plane is already at the requested depth.
   */
  if(crossbar_numbering[original_order[depth]] == planeID)
    return true;

  /*
   * Construct the new plane order required.
   */
  int originalindex = 0;
  int newindex = 0;
  while(newindex < m_crossbarSize)
  {
    if(newindex == depth)
    {
      /*
       * Insert the plane we are moving here.
       */
      new_order[newindex++] = crossbar_plane_lookup(planeID);
    }
    else
    {
      /*
       * Copy the next plane in the original order into the next
       * slot in the new order, skipping the plane we are moving.
       */
      if(crossbar_numbering[original_order[originalindex]] != planeID)
        new_order[newindex++] = original_order[originalindex];

      originalindex++;
    }
  }

  /*
   * Construct the register setup for the new order
   */
  crossbar_setup = 0;

  for(int i=0;i<m_crossbarSize;i++)
  {
    int entry_shift = i*3;

    DEBUGF2(2,("CGammaMixer::SetPlaneDepth new depth %d = %d\n",i+1,new_order[i]));
    crossbar_setup |= (new_order[i] & 0x7) << entry_shift;
  }

  if(activate)
  {
    DEBUGF2(2,("CGammaMixer::SetPlaneDepth new crossbar_setup = 0x%08lx\n",crossbar_setup));
    WriteMixReg(MXn_CRB_REG_OFFSET, crossbar_setup);
  }

  return true;
}


bool CGammaMixer::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const
{
  if(m_crossbarSize == 0 || !PlaneValid(planeID))
    return false;

  /*
   * Background and cursor are always at fixed depths
   */
  if(planeID == OUTPUT_BKG)
  {
    *depth = 0;
    return true;
  }

  if(planeID == OUTPUT_CUR)
  {
    *depth = CURSOR_DEPTH;
    return true;
  }

  /* Null plane is always in the same place */
  if(planeID == OUTPUT_NULL)
  {
    *depth = 0;
    return true;
  }

  /*
   * Search through the cross bar level setup register for en entry
   * corresponding to the plane we are interested in.
   */
  ULONG crossbar_setup = ReadMixReg(MXn_CRB_REG_OFFSET);

  for(int i=0;i<m_crossbarSize;i++)
  {
    int   entry_shift = i*3;
    ULONG val = (crossbar_setup >> entry_shift) & 0x7;

    if(val == 0 || val == 7)
      continue;

    if(crossbar_numbering[val] == planeID)
    {
      *depth = i+1;
      return true;
    }
  }

  DEBUGF2(1,("CGammaMixer::GetPlaneDepth cannot find plane %d\n",(int)planeID));

  return false;
}
