/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/DisplayPlane.h>
#include <display/generic/Output.h>

#include <display/ip/stmviewport.h>

#include "GenericGammaReg.h"
#include "GenericGammaDefs.h"
#include "GammaMixer.h"

// Default Background RGB888 color
#define DEFAULT_BKG_COLOR 0x101010

#define MIX_CTL_709_NOT_601 (1L<<25)

#define CURSOR_DEPTH 8

CGammaMixer::CGammaMixer(CDisplayDevice* pDev,
                         uint32_t        ulRegOffset,
                   const stm_mixer_id_t *planeMap,
                         stm_mixer_id_t  ulVBILinkedPlane): CDisplayMixer()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDev        = pDev;
  m_pGammaReg   = (uint32_t*)m_pDev->GetCtrlRegisterBase();
  m_ulRegOffset = ulRegOffset;
  m_lock        = vibe_os_create_resource_lock();

  m_ulCapabilities = OUTPUT_CAPS_PLANE_MIXER | OUTPUT_CAPS_MIXER_BACKGROUND;

  m_planeMap           = planeMap;
  m_validPlanes        = 0;
  m_planeEnables       = 0;
  m_planeActive        = 0;
  m_ulBackground       = DEFAULT_BKG_COLOR;
  m_bBackgroundVisible = true;

  vibe_os_zero_memory( &m_CurrentMode, sizeof( m_CurrentMode ));
  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;
  m_bIsSuspended        = false;

  m_crossbarSize    = 0;
  m_bHasHwYCbCrMatrix  = true;
  m_colorspaceMode  = STM_YCBCR_COLORSPACE_AUTO_SELECT;

  m_ulVBILinkedPlane = ulVBILinkedPlane;

  m_pEnabledPlanes = new const CDisplayPlane *[m_pDev->GetNumberOfPlanes()];
  for (uint32_t i = 0; i < m_pDev->GetNumberOfPlanes(); i++)
  {
    m_pEnabledPlanes[i] = 0L;
  }

  m_SavedCrossbarRegister = 0;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CGammaMixer::~CGammaMixer()
{
  delete [] m_pEnabledPlanes;

  vibe_os_delete_resource_lock(m_lock);
}


bool CGammaMixer::PlaneValid(const CDisplayPlane *p) const
{
  return (m_planeMap[p->GetID()] & m_validPlanes) != 0;
}


/* This can be overriden to provide more restrictions, e.g. mutually exclusive
 * planes on the 5528 aux mixer. Hence the result can be dynamic depending
 * on what other planes are enabled.
 */
bool CGammaMixer::CanEnablePlane(const CDisplayPlane *p)
{
  if((m_planeMap[p->GetID()] == MIXER_ID_VBI) && (m_ulVBILinkedPlane == MIXER_ID_NONE))
    return false;

  return PlaneValid(p);
}


bool CGammaMixer::UpdateFromOutput(COutput* pOutput, stm_plane_update_reason_t update_reason)
{
  for(uint32_t i=0; i < m_pDev->GetNumberOfPlanes(); i++)
  {
    CDisplayPlane* plane = m_pDev->GetPlane(i);
    if(plane)
      plane->UpdateFromMixer(pOutput, update_reason);
  }

  return true;
}


bool CGammaMixer::EnablePlane(const CDisplayPlane *p, stm_mixer_activation_t act)
{
  if(!IsStarted())
  {
    TRC( TRC_ID_ERROR, "no video mode active (plane = %s)",p->GetName() );
    return false;
  }

  if(!CanEnablePlane(p))
    return false;

  SetMixerForPlanes(m_planeMap[p->GetID()], true, act);

  m_pEnabledPlanes[p->GetID()] = p;

  return true;
}


bool CGammaMixer::DisablePlane(const CDisplayPlane *p)
{
  if(!IsStarted())
  {
    TRC( TRC_ID_MAIN_INFO, "no video mode active (plane = %s)",p->GetName() );
    return false;
  }

  if(!CanEnablePlane(p))
    return false;

  SetMixerForPlanes(m_planeMap[p->GetID()], false, STM_MA_NONE);

  m_pEnabledPlanes[p->GetID()] = NULL;

  return true;
}


bool CGammaMixer::HasEnabledPlanes(void) const
{
  return (m_planeEnables & ~(uint32_t)MIXER_ID_BKG) != 0;
}


void CGammaMixer::SetMixerForPlanes(stm_mixer_id_t mixerID, bool isEnabled, stm_mixer_activation_t act)
{
  uint32_t tmp;

  /*
   * We have to do all of this with interrupts disabled so another plane's
   * vsync processing doesn't come in the middle and confuse the state.
   */
  vibe_os_lock_resource(m_lock);

  if(isEnabled)
    m_planeEnables |= mixerID;
  else
    m_planeEnables &= ~mixerID;

  TRC( TRC_ID_MAIN_INFO, "m_planeEnables = 0x%x", m_planeEnables );

  /*
   * Do not change the mixer state for VBI if its linked GDP is also enabled
   */
  if((mixerID == MIXER_ID_VBI) && (m_planeEnables & m_ulVBILinkedPlane))
    goto unlock_and_exit;

  /*
   * Do not change the mixer state for a linked GDP if its linked VBI plane is
   * also enabled.
   */
  if((mixerID == m_ulVBILinkedPlane) && (m_planeEnables & MIXER_ID_VBI))
    goto unlock_and_exit;

  tmp = ReadMixReg(MXn_CTL_REG_OFFSET) & 0xffff0000;

  if(mixerID == MIXER_ID_VBI)
  {
    mixerID = m_ulVBILinkedPlane;
    if(isEnabled)
      tmp |= (mixerID & 0xffff);
    else
      tmp &= ~mixerID;
  }

  WriteMixReg(MXn_CTL_REG_OFFSET, (tmp | (m_planeEnables & 0xffff)));

  if((uint32_t)act & STM_MA_GRAPHICS)
    m_planeActive |= mixerID;
  else
    m_planeActive &= ~mixerID;

  if((uint32_t)act & STM_MA_VIDEO)
    m_planeActive |= (uint32_t)mixerID<<16;
  else
    m_planeActive &= ~((uint32_t)mixerID<<16);

  TRC( TRC_ID_MAIN_INFO, "CGammaMixer::SetMixerForPlanes m_planeActive  = 0x%x", m_planeActive );
  WriteMixReg(MXn_ACT_REG_OFFSET, m_planeActive);

unlock_and_exit:
  vibe_os_unlock_resource(m_lock);
}


uint32_t CGammaMixer::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_BACKGROUND_ARGB:
    {
      if(HasBackground())
      {
        m_ulBackground = newVal;
        TRC( TRC_ID_MAIN_INFO, "new colour = 0x%08x", m_ulBackground );
        WriteMixReg(MXn_BKC_REG_OFFSET, m_ulBackground);
      }
      else
        return STM_OUT_NO_CTRL;

      break;
    }
    case OUTPUT_CTRL_BACKGROUND_VISIBILITY:
    {
      if(HasBackground())
      {
        m_bBackgroundVisible = (newVal != 0);
        if(IsStarted())
          SetMixerForPlanes(MIXER_ID_BKG, m_bBackgroundVisible, STM_MA_NONE);
      }
      else
        return STM_OUT_NO_CTRL;

      break;
    }
    case OUTPUT_CTRL_YCBCR_COLORSPACE:
    {
      if(newVal > STM_YCBCR_COLORSPACE_709)
        return STM_OUT_INVALID_VALUE;

      m_colorspaceMode = static_cast<stm_ycbcr_colorspace_t>(newVal);

      TRC( TRC_ID_MAIN_INFO, "new colourspace mode = %d", (int)m_colorspaceMode );

      if(IsStarted())
        UpdateColorspace(&m_CurrentMode);

      break;
    }
    default:
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CGammaMixer::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_BACKGROUND_ARGB:
    {
      if(HasBackground())
      {
        *val = m_ulBackground;
      }
      else
      {
        *val = 0;
        return STM_OUT_NO_CTRL;
      }

      break;
    }
    case OUTPUT_CTRL_BACKGROUND_VISIBILITY:
    {
      if(HasBackground())
      {
        *val = m_bBackgroundVisible?1:0;
      }
      else
      {
        *val = 0;
        return STM_OUT_NO_CTRL;
      }

      break;
    }
    case OUTPUT_CTRL_YCBCR_COLORSPACE:
      *val = m_colorspaceMode;
      break;
    default:
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CGammaMixer::GetColorspaceCTL(const stm_display_mode_t *pModeLine) const
{
  switch(m_colorspaceMode)
  {
    case STM_YCBCR_COLORSPACE_AUTO_SELECT:
      return (pModeLine->mode_params.output_standards & STM_OUTPUT_STD_HD_MASK)?MIX_CTL_709_NOT_601:0;
    case STM_YCBCR_COLORSPACE_709:
      return MIX_CTL_709_NOT_601;
    case STM_YCBCR_COLORSPACE_601:
    default:
      return 0;
  }
}


void CGammaMixer::UpdateColorspace(const stm_display_mode_t *pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bHasHwYCbCrMatrix)
    return;

  uint32_t tmp = ReadMixReg(MXn_CTL_REG_OFFSET) & ~MIX_CTL_709_NOT_601;
  tmp |= GetColorspaceCTL(pModeLine);
  WriteMixReg(MXn_CTL_REG_OFFSET, tmp);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CGammaMixer::Start(const stm_display_mode_t *pModeLine)
{
  uint32_t ystart, ystop, xstart, xstop;
  uint32_t uAVO, uAVS, uBCO, uBCS;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return false;

  WriteMixReg(MXn_CTL_REG_OFFSET, 0);

  UpdateColorspace(pModeLine);

  /*
   * Set Mixer viewport start, start on the first blanking line (negative offset
   * from the start of the active area).
   */
  xstart = STCalculateViewportPixel(pModeLine, 0);

  int blankline = pModeLine->mode_timing.vsync_width - (pModeLine->mode_params.active_area_start_line-1);
  if(pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)
    blankline *= 2;

  ystart = STCalculateViewportLine(pModeLine, blankline);

  uAVO = xstart | (ystart << 16);

  //Set Mixer background to start at the first active video line
  ystart = STCalculateViewportLine(pModeLine, 0);

  uBCO = xstart | (ystart << 16);

  WriteMixReg(MXn_AVO_REG_OFFSET, uAVO);

  TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - AVO = %#08x", uAVO );

  /*
   * Set Active video stop, this is also where the background stop as well
   */
  xstop = STCalculateViewportPixel(pModeLine, pModeLine->mode_params.active_area_width - 1);
  ystop = STCalculateViewportLine(pModeLine, pModeLine->mode_params.active_area_height - 1);

  uAVS = xstop | (ystop << 16);
  uBCS = xstop | (ystop << 16);

  WriteMixReg(MXn_AVS_REG_OFFSET, uAVS);

  TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - AVS = %#08x", uAVS );

  /*
   * Set current mode now so that enabling the background plane will succeed.
   */
  m_CurrentMode = *pModeLine;

  if(HasBackground())
  {
    TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - Enabling Background Colour" );
    TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - BCO = %#08x", uBCO );
    TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - BCS = %#08x", uBCS );
    TRC( TRC_ID_MAIN_INFO, "CGammaMixer::Start - BKC = %#08x", m_ulBackground );
    WriteMixReg(MXn_BKC_REG_OFFSET, m_ulBackground);
    WriteMixReg(MXn_BCO_REG_OFFSET, uBCO);
    WriteMixReg(MXn_BCS_REG_OFFSET, uBCS);

    SetMixerForPlanes(MIXER_ID_BKG, m_bBackgroundVisible, STM_MA_NONE);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


void CGammaMixer::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  vibe_os_lock_resource(m_lock);

  WriteMixReg(MXn_CTL_REG_OFFSET, 0);
  WriteMixReg(MXn_ACT_REG_OFFSET, 0);
  m_planeEnables = 0;
  m_planeActive  = 0;
  m_CurrentMode.mode_id = STM_TIMING_MODE_RESERVED;

  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CGammaMixer::Suspend(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return;

  vibe_os_lock_resource(m_lock);
  {
    m_SavedCrossbarRegister = ReadMixReg(MXn_CRB_REG_OFFSET);
    m_bIsSuspended = true;
  }
  vibe_os_unlock_resource(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CGammaMixer::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
  {
    m_bIsSuspended = false; // First so we can call Start() to but the main configuration back

    if(m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED)
      Start(&m_CurrentMode);

    /*
     * Restore crossbar settings.
     */
    WriteMixReg(MXn_CRB_REG_OFFSET, m_SavedCrossbarRegister);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


/*
 * This implements a mixer with a fully configuration crossbar.
 * For chips that have partially or fully fixed crossbars these
 * methods may be overriden.
 */

static stm_mixer_id_t crossbar_numbering[] = {
  MIXER_ID_BKG, /* Dummy value, zero actually means no plane at this level */
  MIXER_ID_VID1,
  MIXER_ID_VID2,
  MIXER_ID_GDP1,
  MIXER_ID_GDP2,
  MIXER_ID_GDP3,
  MIXER_ID_GDP4
};


inline static uint32_t crossbar_plane_lookup(stm_mixer_id_t mixerID)
{
  switch(mixerID)
  {
    case MIXER_ID_VID1:
      return MIX_CRB_VID1;
    case MIXER_ID_VID2:
      return MIX_CRB_VID2;
    case MIXER_ID_GDP1:
      return MIX_CRB_GDP1;
    case MIXER_ID_GDP2:
      return MIX_CRB_GDP2;
    case MIXER_ID_GDP3:
      return MIX_CRB_GDP3;
    case MIXER_ID_GDP4:
      return MIX_CRB_GDP4;
    default:
      return MIX_CRB_NOTHING;
  }
}


bool CGammaMixer::SetPlaneDepth(const CDisplayPlane *p, int depth, bool activate)
{
  int original_order[7],new_order[7];

  TRC( TRC_ID_MAIN_INFO, "plane \"%s\" ID:%d depth = %d", p->GetName(), p->GetID(), depth );

  vibe_os_zero_memory( original_order, sizeof( original_order ));
  vibe_os_zero_memory( new_order, sizeof( new_order ));

  if(!PlaneValid(p))
    return false;

  stm_mixer_id_t mixerID = m_planeMap[p->GetID()];

  /*
   * The cursor is fixed at the highest possible level
   */
  if(mixerID == MIXER_ID_CUR)
    return (depth == CURSOR_DEPTH)?true:false;

  if(m_crossbarSize == 0)
  {
    int currentdepth;
    /*
     * No ability to change the position, but don't fail if the requested
     * depth is the current depth.
     */
    this->GetPlaneDepth(p, &currentdepth);

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
  uint32_t crossbar_setup = ReadMixReg(MXn_CRB_REG_OFFSET);

  TRC( TRC_ID_MAIN_INFO, "current crossbar_setup = 0x%08x", crossbar_setup );

  /*
   * Extract the current plane order from the hardware register
   */
  for(int i=0;i<m_crossbarSize;i++)
  {
    int entry_shift = i*3;
    original_order[i] = (crossbar_setup >> entry_shift) & 0x7;
    TRC( TRC_ID_MAIN_INFO, "current depth %d = %d", i+1, original_order[i] );
  }

  /*
   * Check to see if the plane is already at the requested depth.
   */
  if(crossbar_numbering[original_order[depth]] == mixerID)
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
      new_order[newindex++] = crossbar_plane_lookup(mixerID);
    }
    else
    {
      /*
       * Copy the next plane in the original order into the next
       * slot in the new order, skipping the plane we are moving.
       */
      if(crossbar_numbering[original_order[originalindex]] != mixerID)
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

    TRC( TRC_ID_MAIN_INFO, "new depth %d = %d", i+1, new_order[i] );
    crossbar_setup |= (new_order[i] & 0x7) << entry_shift;
  }

  if(activate)
  {
    TRC( TRC_ID_MAIN_INFO, "new crossbar_setup = 0x%08x", crossbar_setup );
    WriteMixReg(MXn_CRB_REG_OFFSET, crossbar_setup);
  }

  return true;
}


bool CGammaMixer::GetPlaneDepth(const CDisplayPlane *p, int *depth) const
{
  if(m_crossbarSize == 0 || !PlaneValid(p))
    return false;

  stm_mixer_id_t mixerID = m_planeMap[p->GetID()];

  if(mixerID == MIXER_ID_CUR)
  {
    *depth = CURSOR_DEPTH;
    return true;
  }

  /*
   * Search through the cross bar level setup register for en entry
   * corresponding to the plane we are interested in.
   */
  uint32_t crossbar_setup = ReadMixReg(MXn_CRB_REG_OFFSET);

  for(int i=0;i<m_crossbarSize;i++)
  {
    int   entry_shift = i*3;
    uint32_t val = (crossbar_setup >> entry_shift) & 0x7;

    if(val == 0 || val == 7)
      continue;

    if(crossbar_numbering[val] == mixerID)
    {
      *depth = i+1;
      return true;
    }
  }

  TRC( TRC_ID_ERROR, "cannot find plane \"%s\" %d", p->GetName(), (int)mixerID );

  return false;
}
