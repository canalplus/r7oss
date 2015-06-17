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
#include <display_device_priv.h>

#include "Output.h"
#include "DisplayDevice.h"
#include "DisplaySource.h"
#include "SourceInterface.h"

#include "DisplayQueue.h"
#include "DisplayNode.h"

#include "DisplayPlane.h"

CDisplayPlane::CDisplayPlane(const char           *name,
                             uint32_t              planeID,
                             const CDisplayDevice *pDev,
                             const stm_plane_capabilities_t caps,
                             uint32_t              ulNodeListSize)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_name           = name;
  m_planeID        = planeID;
  m_pDisplayDevice = pDev;
  m_capabilities = caps;

  m_ulTimingID = 0;
  m_pOutput    = 0;
  m_pSource    = 0;

  m_status       = 0;
  m_hasADeinterlacer = false;
  m_isEnabled    = false;
  m_isPaused     = false;
  m_isFlushing   = false;
  m_bIsSuspended = false;
  m_bIsFrozen    = false;
  m_areConnectionsChanged = false;
  m_isOutputModeChanged = false;

  m_ContextChanged = false;

  m_fixedpointONE = 1; // n.0 i.e. integer, overridden by subclass
  m_ulMaxHSrcInc  = 1;
  m_ulMinHSrcInc  = 1;
  m_ulMaxVSrcInc  = 1;
  m_ulMinVSrcInc  = 1;
  m_ulMaxLineStep = 1;

  m_pSurfaceFormats = 0;
  m_nFormats        = 0;

  m_pFeatures = 0;
  m_nFeatures = 0;
  m_planeLatency = 0;

  m_AspectRatioConversionMode = ASPECT_RATIO_CONV_LETTER_BOX;
  m_InputWindowMode  = AUTO_MODE;
  m_OutputWindowMode = AUTO_MODE;
  m_InputWindowValue.x       = 0;
  m_InputWindowValue.y       = 0;
  m_InputWindowValue.width   = 0;
  m_InputWindowValue.height  = 0;
  m_OutputWindowValue.x      = 0;
  m_OutputWindowValue.y      = 0;
  m_OutputWindowValue.width  = 0;
  m_OutputWindowValue.height = 0;

  ResetComputedInputOutputWindowValues();

  m_hideRequestedByTheApplication = false;
  m_updatePlaneVisibility         = false;
  m_eHideMode                     = PLANE_HIDE_BY_MASKING;
  m_planeVisibily                 = STM_PLANE_DISABLED;

  m_lastRequestedDepth = 0;
  m_updateDepth = false;

  m_isPlaneActive = false;

  vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));

  ResetDisplayInfo();

  m_Transparency = STM_PLANE_TRANSPARENCY_OPAQUE;
  m_TransparencyState = CONTROL_OFF;

  vibe_os_zero_memory( &m_rescale_caps, sizeof( m_rescale_caps ));
  m_numConnectedOutputs = 0;

  m_hasAScaler = true;

  vibe_os_zero_memory(m_ControlStatusArray, sizeof(m_ControlStatusArray));

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CDisplayPlane::~CDisplayPlane(void)
{
  CNode* p_Node;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Release all the queued controls
  p_Node = m_ControlQueue.GetFirstNode();
  while (p_Node != 0)
  {
    m_ControlQueue.ReleaseDisplayNode(p_Node);
    p_Node = m_ControlQueue.GetFirstNode();
  }

  // Release all the queued listeners
  p_Node = m_ListenerQueue.GetFirstNode();
  while (p_Node != 0)
  {
    m_ListenerQueue.ReleaseDisplayNode(p_Node);
    p_Node = m_ListenerQueue.GetFirstNode();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CDisplayPlane::SetScalingCapabilities(stm_plane_rescale_caps_t *caps) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * We do a simplification of the numbers if the maximum sample rate step
   * is >= 1 and is an integer. But if it isn't we just keep it all in the
   * fixed point format. The scaling value is 1/sample step.
   */
  if((m_ulMaxHSrcInc & (m_fixedpointONE-1)) != 0)
  {
    caps->minHorizontalRescale.numerator   = m_fixedpointONE;
    caps->minHorizontalRescale.denominator = m_ulMaxHSrcInc;
  }
  else
  {
    caps->minHorizontalRescale.numerator   = 1;
    caps->minHorizontalRescale.denominator = m_ulMaxHSrcInc/m_fixedpointONE;
  }

  if((m_ulMaxVSrcInc & (m_fixedpointONE-1)) != 0)
  {
    caps->minVerticalRescale.numerator   = m_fixedpointONE;
    caps->minVerticalRescale.denominator = m_ulMaxVSrcInc * m_ulMaxLineStep;
  }
  else
  {
    caps->minVerticalRescale.numerator   = 1;
    caps->minVerticalRescale.denominator = (m_ulMaxVSrcInc/m_fixedpointONE) * m_ulMaxLineStep;
  }

  /*
   * For the minimum sample rate step we simplify if the step <=1 and that 1/step
   * is an integer. We also assume that m_fixedpointONE*m_fixedpointONE is
   * representable in 32bits, which given the maximum value of fixedpointONE
   * we deal with is 2^13 is fine.
   */
  if((m_ulMinHSrcInc > (uint32_t)m_fixedpointONE)  ||
     (((m_fixedpointONE*m_fixedpointONE)/m_ulMinHSrcInc) & (m_fixedpointONE-1)) != 0)
  {
    caps->maxHorizontalRescale.numerator   = m_fixedpointONE;
    caps->maxHorizontalRescale.denominator = m_ulMinHSrcInc;
  }
  else
  {
    caps->maxHorizontalRescale.numerator   = m_fixedpointONE/m_ulMinHSrcInc;
    caps->maxHorizontalRescale.denominator = 1;
  }

  if((m_ulMinVSrcInc > (uint32_t)m_fixedpointONE)  ||
     (((m_fixedpointONE*m_fixedpointONE)/m_ulMinVSrcInc) & (m_fixedpointONE-1)) != 0)
  {
    caps->maxVerticalRescale.numerator   = m_fixedpointONE;
    caps->maxVerticalRescale.denominator = m_ulMinVSrcInc;
  }
  else
  {
    caps->maxVerticalRescale.numerator   = m_fixedpointONE/m_ulMinVSrcInc;
    caps->maxVerticalRescale.denominator = 1;
  }

  TRC( TRC_ID_MAIN_INFO, "horizontal scale %u/%u to %u/%u", caps->minHorizontalRescale.numerator, caps->minHorizontalRescale.denominator, caps->maxHorizontalRescale.numerator, caps->maxHorizontalRescale.denominator );
  TRC( TRC_ID_MAIN_INFO, "vertical scale   %u/%u to %u/%u", caps->minVerticalRescale.numerator, caps->minVerticalRescale.denominator, caps->maxVerticalRescale.numerator, caps->maxVerticalRescale.denominator );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CDisplayPlane::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


DisplayPlaneResults CDisplayPlane::ConnectToOutput(COutput* pOutput)
{
  char     ConnectionTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRC( TRC_ID_MAIN_INFO, "CDisplayPlane::ConnectToOutput 'planeID = %u pOutput = %p'", m_planeID,pOutput );
  uint32_t updateStatus = 0;

  if(!pOutput->CanShowPlane(this))
  {
    TRC( TRC_ID_MAIN_INFO, "CDisplayPlane::ConnectToOutput 'plane not displayable on output = %p'", m_pOutput );
    return STM_PLANE_NOT_SUPPORTED;
  }

  if(m_pOutput == pOutput)
  {
    TRC( TRC_ID_MAIN_INFO, "CDisplayPlane::ConnectToOutput 'already connected to requested output = %p'", m_pOutput );
    return STM_PLANE_ALREADY_CONNECTED;
  }

  if(m_pOutput != NULL)
  {
    TRC( TRC_ID_MAIN_INFO, "CDisplayPlane::ConnectToOutput 'already connected to another output = %p'", m_pOutput );
    return STM_PLANE_CANNOT_CONNECT;
  }

  ////////// Block the VSync ThreadedIRQ while we prepare the plane //////////
  if(m_pDisplayDevice->VSyncLock() != 0)
    return STM_PLANE_CANNOT_CONNECT;

  // Setting the output pointer will be sufficient to start the
  // process of displaying the plane.
  m_pOutput    = pOutput;
  m_numConnectedOutputs=1;
  m_ulTimingID = pOutput->GetTimingID();
  updateStatus = STM_STATUS_PLANE_CONNECTED_TO_OUTPUT;

  // If the plane has already been enabled then show it immediately on the
  // newly attached output.
  if(m_isEnabled)
  {
    m_pOutput->ShowPlane(this);
    updateStatus |= STM_STATUS_PLANE_VISIBLE;
  }

  m_status |= updateStatus;
  m_areConnectionsChanged = true;

  vibe_os_snprintf (ConnectionTag, sizeof(ConnectionTag), "connection_to_%s",
                    m_pOutput->GetName());
  if (stm_registry_add_connection((stm_object_h)this, ConnectionTag,(stm_object_h)m_pOutput))
  {
    TRC( TRC_ID_ERROR, "Registry : Add Connection Between Plane %s and output %s failed", GetName(), m_pOutput->GetName()  );
  }

  FillCurrentOuptutInfo(); // Get some info about the output connected to this plane

  m_pDisplayDevice->VSyncUnlock();
  /////////////////////////////////////////////////////////////////////////////

  TRC( TRC_ID_MAIN_INFO, "Plane %s is now connected to output %s", GetName(), m_pOutput->GetName()  );

  return STM_PLANE_OK;
}


void CDisplayPlane::DisconnectFromOutput(COutput* pOutput)
{
  char     ConnectionTag[STM_REGISTRY_MAX_TAG_SIZE];

  if(!pOutput)
  {
    TRC(TRC_ID_ERROR, "%s: Invalid output 0x%p!", GetName(), pOutput );
    return;
  }

  TRC( TRC_ID_MAIN_INFO, "Disconnect planeID = %u pOutput = %p", m_planeID, pOutput );

  // This covers also the case where m_pOutput is null
  if(m_pOutput != pOutput)
  {
    TRC(TRC_ID_ERROR, "Plane %s is NOT connected to Output %s!", GetName(), pOutput->GetName() );
    return;
  }

  ////////// Block the VSync ThreadedIRQ while we disconnect the plane //////////
  if(m_pDisplayDevice->VSyncLock() != 0)
    return;

  vibe_os_snprintf (ConnectionTag, sizeof(ConnectionTag), "connection_to_%s",
                    m_pOutput->GetName());
  if (stm_registry_remove_connection((stm_object_h)this, ConnectionTag))
  {
    TRC( TRC_ID_ERROR, "Registry : Remove Connection Between Plane %s and output %s failed", GetName(), m_pOutput->GetName()  );
  }

  DisableHW();

  TRC( TRC_ID_MAIN_INFO, "Plane %s is now disconnected from output %s", GetName(), m_pOutput->GetName()  );

  m_pOutput    = 0;
  m_numConnectedOutputs=0;
  m_ulTimingID = 0;

  m_status &= ~(STM_STATUS_PLANE_CONNECTED_TO_OUTPUT|STM_STATUS_PLANE_VISIBLE);
  m_areConnectionsChanged = true;

  // FillCurrentOuptutInfo() will see that this plane is no more connected to an output
  FillCurrentOuptutInfo();

  m_pDisplayDevice->VSyncUnlock();
  /////////////////////////////////////////////////////////////////////////////
}


bool CDisplayPlane::GetConnectedSourceID(uint32_t *id)
{
  if(m_pSource)
  {
    *id = m_pSource->GetID();
    TRC( TRC_ID_UNCLASSIFIED, "Plane %d is connected to source %d", m_planeID, *id  );
    return true;
  }
  else
  {
    TRC( TRC_ID_UNCLASSIFIED, "Plane %d is not connected to any source", m_planeID  );
    return false;
  }
}

DisplayPlaneResults CDisplayPlane::ConnectToSourceProtected(CDisplaySource *pSource)
{
  DisplayPlaneResults status;

  ////////// Block the VSync ThreadedIRQ while we connect the plane //////////
  if ( m_pDisplayDevice->VSyncLock() != 0 )
    return STM_PLANE_EINTR;

  status = ConnectToSource(pSource);

  m_pDisplayDevice->VSyncUnlock();
  /////////////////////////////////////////////////////////////////////////////

  return (status);
}

// This function is called by functions where m_pDisplayDevice->VSyncLock() is already taken
DisplayPlaneResults CDisplayPlane::ConnectToSource(CDisplaySource *pSource)
{
  char     ConnectionTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRC(TRC_ID_MAIN_INFO, "Requested connection plane:%s src:%s", this->GetName(), ((pSource)?pSource->GetName():"null") );

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  if ( !pSource )
  {
    TRC( TRC_ID_ERROR, "Plane %s: Invalid Source!", GetName() );
    return STM_PLANE_INVALID_VALUE;
  }

  if(m_pSource == pSource)
  {
    TRC( TRC_ID_ERROR, "Error! Plane %s already connected to the requested source %s!", GetName(), m_pSource->GetName() );
    return STM_PLANE_ALREADY_CONNECTED;
  }

  if(!pSource->CanConnectToPlane(this))
  {
    TRC( TRC_ID_MAIN_INFO, "Plane %s can not be connected to source %s", GetName(), pSource->GetName()  );
    return STM_PLANE_NOT_SUPPORTED;
  }

  if(m_pSource != 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Plane %s already connected to source %s. Disconnect it before connecting to the new resquested source %s!", GetName(), m_pSource->GetName(), pSource->GetName() );
    DisconnectFromSource(m_pSource);
    //m_pSource is now null
  }

  if (!pSource->ConnectPlane(this))
  {
    TRC( TRC_ID_ERROR, "Connection Between Plane %s and source %s failed", GetName(), pSource->GetName() );
    return STM_PLANE_CANNOT_CONNECT;
  }

  // The source is now considered as connected to this plane. Save the handle of the source which feeds this plane.
  m_pSource = pSource;

  m_status |= STM_STATUS_PLANE_CONNECTED_TO_SOURCE;
  m_areConnectionsChanged = true;

  vibe_os_snprintf (ConnectionTag, sizeof(ConnectionTag), "connection_to_%s",
                    pSource->GetName());
  if (stm_registry_add_connection((stm_object_h)this, ConnectionTag,(stm_object_h)m_pSource))
  {
    TRC( TRC_ID_ERROR, "Registry : Add Connection Between Plane %s and source %s failed", GetName(), m_pSource->GetName()  );
  }

  m_Statistics.SourceId = m_pSource->GetID();

  TRC( TRC_ID_MAIN_INFO, "Plane %s is now connected to source %s", GetName(), m_pSource->GetName()  );

  // This plane is now connected to a new source. We don't know the history of this source so it is
  // wise to reset all the data saved for each use case.
  m_ContextChanged = true;
  TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new source)", GetName() );

  return STM_PLANE_OK;
}


bool CDisplayPlane::DisconnectFromSourceProtected(CDisplaySource *pSource)
{
  bool status;

  ////////// Block the VSync ThreadedIRQ while we disconnect the plane from the source //////////
  if ( m_pDisplayDevice->VSyncLock() != 0 )
    return STM_PLANE_EINTR;

  status = DisconnectFromSource(pSource);

  m_pDisplayDevice->VSyncUnlock();
  /////////////////////////////////////////////////////////////////////////////

  return (status);
}

// This function is called by functions where m_pDisplayDevice->VSyncLock() is already taken
bool CDisplayPlane::DisconnectFromSource(CDisplaySource *pSource)
{
  char     ConnectionTag[STM_REGISTRY_MAX_TAG_SIZE];

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  TRC( TRC_ID_MAIN_INFO, "Requested disconnection plane %s source %s", this->GetName(), ((pSource)?pSource->GetName():"null") );

  if( (m_pSource != pSource) || (pSource == 0) )
  {
    TRC( TRC_ID_ERROR, "Plane %s is not connected to source %s", this->GetName(), ((pSource)?pSource->GetName():"null") );
    return false;
  }

    if (!pSource->DisconnectPlane(this))
      return false;

    vibe_os_snprintf (ConnectionTag, sizeof(ConnectionTag), "connection_to_%s",
                      m_pSource->GetName());
    if (stm_registry_remove_connection((stm_object_h)this, ConnectionTag))
    {
      TRC( TRC_ID_ERROR, "Registry : Remove Connection Between Plane %s and source %s failed", GetName(), m_pSource->GetName()  );
    }

  m_Statistics.SourceId = STM_INVALID_SOURCE_ID;

  TRC( TRC_ID_MAIN_INFO, "Plane %s is now disconnected from source %s", GetName(), m_pSource->GetName()  );
  m_pSource    = 0;

  m_status &= ~(STM_STATUS_PLANE_CONNECTED_TO_SOURCE);
  m_areConnectionsChanged = true;

  return true;
}

DisplayPlaneResults CDisplayPlane::SetControl(stm_display_plane_control_t ctrl, uint32_t newVal)
{
  DisplayPlaneResults result = STM_PLANE_NOT_SUPPORTED;

  switch(ctrl)
  {
    case PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE:
      if( newVal > (uint32_t)ASPECT_RATIO_CONV_IGNORE)
      {
        TRC( TRC_ID_ERROR, "Invalid PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE!");
        result = STM_PLANE_INVALID_VALUE;
      }
      else
      {
        if ( m_pDisplayDevice->VSyncLock() != 0 )
          return STM_PLANE_EINTR;
        // The VSync interrupt should be blocked while we change the Aspect Ratio Conversion Mode
        m_AspectRatioConversionMode = (stm_plane_aspect_ratio_conversion_mode_t) newVal;
        m_ContextChanged = true;
        m_pDisplayDevice->VSyncUnlock();

        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new AR conv mode)", GetName() );
        TRC( TRC_ID_MAIN_INFO, "New PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE=%u",  m_AspectRatioConversionMode );
        result = STM_PLANE_OK;
      }
      break;

    case PLANE_CTRL_INPUT_WINDOW_MODE:
      if( newVal > (uint32_t)AUTO_MODE)
      {
        TRC( TRC_ID_ERROR, "Invalid PLANE_CTRL_INPUT_WINDOW_MODE!");
        result = STM_PLANE_INVALID_VALUE;
      }
      /*
       * The plane should be supporting vertical and horizontal scaling
       * as we may have the input window values which doesn't match the
       * output ones. In that case setting input AUTO mode is not
       * supported too.
       */
      else if((newVal == AUTO_MODE) && (!m_hasAScaler))
      {
        TRC( TRC_ID_GDP_PLANE, "New PLANE_CTRL_INPUT_WINDOW_MODE mode=NOT SUPPORTED (No Scaler HW support)" );
        result = STM_PLANE_NOT_SUPPORTED;
      }
      else
      {
        if ( m_pDisplayDevice->VSyncLock() != 0 )
          return STM_PLANE_EINTR;
        // The VSync interrupt should be blocked while we change the Input Window Mode
        m_InputWindowMode = (stm_plane_mode_t)newVal;
        if(m_InputWindowMode==AUTO_MODE)
        {
          // Reset to their invalid values when AUTO mode is entered
          ResetComputedInputOutputWindowValues();
        }
        m_ContextChanged = true;
        m_pDisplayDevice->VSyncUnlock();

        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new IW mode)", GetName() );
        TRC( TRC_ID_MAIN_INFO, "New PLANE_CTRL_INPUT_WINDOW_MODE mode=%u",  newVal );
        result = STM_PLANE_OK;
      }
      break;

    case PLANE_CTRL_OUTPUT_WINDOW_MODE:
      if( newVal > (uint32_t)AUTO_MODE)
      {
        TRC( TRC_ID_ERROR, "Invalid PLANE_CTRL_OUTPUT_WINDOW_MODE!");
        result = STM_PLANE_INVALID_VALUE;
      }
      /*
       * The plane should be supporting vertical and horizontal scaling
       * as we may have the output window values which doesn't match the
       * input ones. In that case setting output AUTO mode is not
       * supported too.
       */
      else if((newVal == AUTO_MODE) && (!m_hasAScaler))
      {
        TRC( TRC_ID_GDP_PLANE, "New PLANE_CTRL_OUTPUT_WINDOW_MODE mode=NOT SUPPORTED (No Scaler HW support)" );
        result = STM_PLANE_NOT_SUPPORTED;
      }
      else
      {
        if ( m_pDisplayDevice->VSyncLock() != 0 )
          return STM_PLANE_EINTR;
        // The VSync interrupt should be blocked while we change the Output Window Mode
        m_OutputWindowMode = (stm_plane_mode_t)newVal;
        m_ContextChanged = true;
        m_pDisplayDevice->VSyncUnlock();

        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new OW mode)", GetName() );
        TRC( TRC_ID_MAIN_INFO, "New PLANE_CTRL_OUTPUT_WINDOW_MODE=%u",  newVal );
        result = STM_PLANE_OK;
      }
      break;
    case PLANE_CTRL_TRANSPARENCY_VALUE:
          if( newVal > STM_PLANE_TRANSPARENCY_OPAQUE)
          {
            TRC( TRC_ID_ERROR, "Invalid PLANE_CTRL_TRANSPARENCY_VALUE" );
            result = STM_PLANE_INVALID_VALUE;
          }
          else
          {
            if ( m_pDisplayDevice->VSyncLock() != 0 )
              return STM_PLANE_EINTR;
            // The VSync interrupt should be blocked while we change the Transparency
            m_Transparency = newVal&0xFF;
            m_ContextChanged = true;
            m_pDisplayDevice->VSyncUnlock();

            TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new transparency value)", GetName() );
            TRC( TRC_ID_MAIN_INFO, "New PLANE_CTRL_TRANSPARENCY_VALUE=%u",  m_Transparency );
            result = STM_PLANE_OK;
          }
          break;
    case PLANE_CTRL_TRANSPARENCY_STATE:
        if( (newVal != CONTROL_OFF) && (newVal != CONTROL_ON))
        {
          TRC(TRC_ID_MAIN_INFO, "Invalid PLANE_CTRL_TRANSPARENCY_STATE");
          result = STM_PLANE_INVALID_VALUE;
        }
        else
        {
          if ( m_pDisplayDevice->VSyncLock() != 0 )
            return STM_PLANE_EINTR;
          // The VSync interrupt should be blocked while we change the Transparency
          m_TransparencyState = (stm_plane_control_state_t)newVal;
          m_ContextChanged = true;
          m_pDisplayDevice->VSyncUnlock();

          TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new transparency state)", GetName() );
          TRC(TRC_ID_MAIN_INFO, "New PLANE_CTRL_TRANSPARENCY_STATE=%u",
                  m_TransparencyState);
          result = STM_PLANE_OK;
        }
        break;
    case PLANE_CTRL_DEPTH:
        // Do a call to SetDepth() with "activate=false" in order to see if the requested depth is valid
        if(SetDepth(m_pOutput, newVal, false))
        {
            // The requested depth is saved in a variable and will be applied at next VSync.
            if ( m_pDisplayDevice->VSyncLock() != 0 )
              return STM_PLANE_EINTR;
            m_lastRequestedDepth = (int32_t) newVal;
            m_updateDepth = true;
            m_pDisplayDevice->VSyncUnlock();
            result = STM_PLANE_OK;
        }
        else
        {
            TRC(TRC_ID_ERROR, "Error! SetDepth failed on output 0x%p", m_pOutput);
            result = STM_PLANE_INVALID_VALUE;
        }
        break;
    case PLANE_CTRL_TEST_DEPTH:
      if(SetDepth(m_pOutput, newVal, false))
      {
        result = STM_PLANE_OK;
      }
      else
      {
        result = STM_PLANE_NOT_SUPPORTED;
        TRC(TRC_ID_ERROR, "Error during PLANE_CTRL_TEST_DEPTH on output 0x%p", m_pOutput);
      }
      break;
    case PLANE_CTRL_VISIBILITY:
      if (newVal == PLANE_NOT_VISIBLE)
      {
        TRC(TRC_ID_MAIN_INFO, "New PLANE_CTRL_VISIBILITY hide");
        result = HideRequestedByTheApplication()?STM_PLANE_OK:STM_PLANE_INVALID_VALUE;
      }
      else if (newVal == PLANE_VISIBLE)
      {
        TRC(TRC_ID_MAIN_INFO, "New PLANE_CTRL_VISIBILITY show");
        result = ShowRequestedByTheApplication()?STM_PLANE_OK:STM_PLANE_INVALID_VALUE;
      }
      else
      {
        TRC(TRC_ID_ERROR, "Invalid PLANE_CTRL_VISIBILITY");
        result = STM_PLANE_NOT_SUPPORTED;
      }
      break;
    default:
        break;
  }

  return result;
}

DisplayPlaneResults CDisplayPlane::AddSimpleControlToQueue(stm_display_plane_control_t ctrl, uint32_t newVal)
{
  CControlNode* p_CurrentNode = 0;
  ControlParams ControlParam;

  ControlParam.value = newVal;
  p_CurrentNode =  new CControlNode(ctrl, &ControlParam);
  if (p_CurrentNode == 0)
  {
    return STM_PLANE_NO_MEMORY;
  }

  if ( m_pDisplayDevice->VSyncLock() != 0 )
  {
    delete p_CurrentNode;
    return STM_PLANE_EINTR;
  }
  m_ControlQueue.QueueDisplayNode(p_CurrentNode);
  m_pDisplayDevice->VSyncUnlock();

  return STM_PLANE_OK;
}

DisplayPlaneResults CDisplayPlane::AddCompoundControlToQueue(stm_display_plane_control_t ctrl, void * newVal)
{
  CControlNode* p_CurrentNode = 0;
  ControlParams ControlParam;

  switch (ctrl)
  {
    case PLANE_CTRL_CONNECT_TO_SOURCE:
    case PLANE_CTRL_DISCONNECT_FROM_SOURCE:
      ControlParam.display_source = (stm_display_source_h)newVal;
      break;
    default:
      return STM_PLANE_NOT_SUPPORTED;
  }

  p_CurrentNode =  new CControlNode(ctrl, &ControlParam);
  if (p_CurrentNode == 0)
  {
    return STM_PLANE_NO_MEMORY;
  }

  if ( m_pDisplayDevice->VSyncLock() != 0 )
  {
    delete p_CurrentNode;
    return STM_PLANE_EINTR;
  }
  m_ControlQueue.QueueDisplayNode(p_CurrentNode);
  m_pDisplayDevice->VSyncUnlock();

  return STM_PLANE_OK;
}

int CDisplayPlane::ConvertDisplayPlaneResult(const DisplayPlaneResults result)
{
  int ret;

  switch (result)
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_ALREADY_CONNECTED:
      ret = -EALREADY;
      break;
    case STM_PLANE_CANNOT_CONNECT:
      ret = -EBUSY;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  return ret;
}


DisplayPlaneResults CDisplayPlane::ProcessControlQueue(const stm_time64_t &vsyncTime)
{
  DisplayPlaneResults result = STM_PLANE_OK;
  CControlNode* p_CurrentNode = 0;
  CListenerNode* p_CurrentListenerNode = 0;
  uint32_t status_cnt = 0;
  int status = 0;

  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  p_CurrentNode = (CControlNode*)m_ControlQueue.GetFirstNode();

  while (p_CurrentNode != 0)
  {
    switch (p_CurrentNode->m_control)
    {
      case PLANE_CTRL_CONNECT_TO_SOURCE:
        result = ConnectToSource(p_CurrentNode->m_params.display_source->handle);
        status = ConvertDisplayPlaneResult(result);
        break;
      case PLANE_CTRL_DISCONNECT_FROM_SOURCE:
        if(DisconnectFromSource(p_CurrentNode->m_params.display_source->handle))
        {
            // SUCCESS
            status = 0;
        }
        else
        {
            status = -ENOTSUP;
        }
        break;
      default:
        TRC( TRC_ID_ERROR, "Unknown control !!");
        status = ConvertDisplayPlaneResult(STM_PLANE_NOT_SUPPORTED);
        break;
    }

    if (status_cnt < CONTROL_STATUS_ARRAY_MAX_NB)
    {
      m_ControlStatusArray[status_cnt].ctrl_id = p_CurrentNode->m_control;
      m_ControlStatusArray[status_cnt].error_code = status;
      status_cnt++;
    }
    else
    {
      TRC( TRC_ID_ERROR, "Control status array is full: notification not possible !!!");
    }

    m_ControlQueue.ReleaseDisplayNode((CNode*)p_CurrentNode);
    p_CurrentNode = (CControlNode*)m_ControlQueue.GetFirstNode();
  }

  if (status_cnt > 0)
  {
    p_CurrentListenerNode = (CListenerNode*)m_ListenerQueue.GetFirstNode();

    while (p_CurrentListenerNode != 0)
    {
      p_CurrentListenerNode->m_params.notify(p_CurrentListenerNode->m_params.data, vsyncTime,
        (stm_asynch_ctrl_status_t *)m_ControlStatusArray, status_cnt);

      p_CurrentListenerNode = (CListenerNode*)p_CurrentListenerNode->GetNextNode();
    }
  }

  return STM_PLANE_OK;
}

DisplayPlaneResults CDisplayPlane::RegisterAsynchCtrlListener(const stm_ctrl_listener_t *listener, int *listener_id)
{
  CListenerNode* p_CurrentNode = 0;
  bool           status        = false;

  if ((listener == 0) || (listener_id == 0))
  {
    TRC( TRC_ID_ERROR, "invalid input parameter" );
    return STM_PLANE_INVALID_VALUE;
  }

  *listener_id = 0;
  if (listener->notify == 0)
  {
    TRC( TRC_ID_ERROR, "invalid notify callback" );
    return STM_PLANE_INVALID_VALUE;
  }

  p_CurrentNode =  new CListenerNode(listener);
  if (p_CurrentNode == 0)
  {
    return STM_PLANE_NO_MEMORY;
  }

  if ( m_pDisplayDevice->VSyncLock() != 0 )
  {
    delete p_CurrentNode;
    return STM_PLANE_EINTR;
  }
  status = m_ListenerQueue.QueueDisplayNode(p_CurrentNode);
  m_pDisplayDevice->VSyncUnlock();

  if (status == false)
  {
    return STM_PLANE_NOT_SUPPORTED;
  }

  // Warning: this code will not work if pointers are bigger than 32 bits.
  *listener_id = (int)p_CurrentNode;
  return STM_PLANE_OK;
}

DisplayPlaneResults CDisplayPlane::UnregisterAsynchCtrlListener(int listener_id)
{
  DisplayPlaneResults result = STM_PLANE_OK;
  CListenerNode* p_CurrentNode = 0;

  if (listener_id == 0)
  {
    TRC( TRC_ID_ERROR, "invalid listener_id" );
    return STM_PLANE_INVALID_VALUE;
  }

  // Warning: this code will not work if pointers are bigger than 32 bits.
  p_CurrentNode = (CListenerNode*)listener_id;

  if ( m_pDisplayDevice->VSyncLock() != 0 )
  {
    return STM_PLANE_EINTR;
  }
  m_ListenerQueue.ReleaseDisplayNode((CNode*)p_CurrentNode);
  m_pDisplayDevice->VSyncUnlock();

  return result;
}

DisplayPlaneResults CDisplayPlane::GetControl(stm_display_plane_control_t ctrl, uint32_t *currentVal) const
{
  DisplayPlaneResults result = STM_PLANE_NOT_SUPPORTED;

  /* NB: "m_pDisplayDevice->VSyncLock()" is not taken when reading the variables below because they can be
         modified ONLY by stm_display_plane_set_compound_control() and stm_display_plane_set_control().
         Thanks to the STKPI semaphore, we are sure that the GetControl() and GetCompoundControl() functions
         are not executed at the same time as the set() action.
  */

  switch(ctrl)
  {
    case PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE:
      *currentVal = (uint32_t)m_AspectRatioConversionMode;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE currentVal=%u",  *currentVal );
      result = STM_PLANE_OK;
      break;
    case PLANE_CTRL_INPUT_WINDOW_MODE:
      *currentVal = m_InputWindowMode;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_INPUT_WINDOW_MODE mode=%u",  *currentVal );
      result = STM_PLANE_OK;
      break;
    case PLANE_CTRL_OUTPUT_WINDOW_MODE:
      *currentVal = m_OutputWindowMode;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_OUTPUT_WINDOW_MODE mode=%u",  *currentVal );
      result = STM_PLANE_OK;
      break;
    case PLANE_CTRL_TRANSPARENCY_VALUE:
      *currentVal = m_Transparency;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_TRANSPARENCY_VALUE mode=%u",  *currentVal );
      result = STM_PLANE_OK;
      break;
    case PLANE_CTRL_TRANSPARENCY_STATE:
      *currentVal = m_TransparencyState;
      TRC(TRC_ID_MAIN_INFO,"PLANE_CTRL_TRANSPARENCY_STATE mode=%u", *currentVal);
      result = STM_PLANE_OK;
      break;
    case PLANE_CTRL_DEPTH:
      if(m_lastRequestedDepth == 0)
      {
        int depth;
        // The Depth has never been set for this plane: Ask to the mixer what is the default depth for this plane
        if(GetDepth(m_pOutput, &depth))
        {
          *currentVal = depth;
          result = STM_PLANE_OK;
          TRC(TRC_ID_MAIN_INFO,"PLANE_CTRL_DEPTH value=%u", *currentVal);
        }
        else
        {
          result = STM_PLANE_NOT_SUPPORTED;
          TRC(TRC_ID_ERROR,"Error while reading the plane depth!");
        }
      }
      else
      {
          *currentVal = m_lastRequestedDepth;
          result = STM_PLANE_OK;
          TRC(TRC_ID_MAIN_INFO,"PLANE_CTRL_DEPTH value=%u", *currentVal);
      }
      break;
    case PLANE_CTRL_VISIBILITY:
      *currentVal = m_hideRequestedByTheApplication;
      TRC(TRC_ID_MAIN_INFO,"PLANE_CTRL_VISIBILITY value=%u", *currentVal);
      result = STM_PLANE_OK;
      break;
    default:
      break;
  }

  return result;
}

bool CDisplayPlane::GetControlRange(stm_display_plane_control_t selector, stm_display_plane_control_range_t *range)
{
  bool result = false;

  switch(selector)
  {
    case PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE:
      range->min_val = ASPECT_RATIO_CONV_LETTER_BOX;
      range->max_val = ASPECT_RATIO_CONV_IGNORE;
      range->default_val = ASPECT_RATIO_CONV_LETTER_BOX;
      range->step = 1;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE" );
      result = true;
      break;
    case PLANE_CTRL_INPUT_WINDOW_MODE:
      range->min_val = MANUAL_MODE;
      range->max_val = AUTO_MODE;
      range->default_val = AUTO_MODE;
      range->step = 1;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_INPUT_WINDOW_MODE" );
      result = true;
      break;
    case PLANE_CTRL_OUTPUT_WINDOW_MODE:
      range->min_val = MANUAL_MODE;
      range->max_val = AUTO_MODE;
      range->default_val = AUTO_MODE;
      range->step = 1;
      TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_OUTPUT_WINDOW_MODE" );
      result = true;
      break;
    default:
      break;
  }

  return result;
}

bool CDisplayPlane::GetControlMode(stm_display_control_mode_t *mode)
{
  // Default, return failure for an unsupported control
  return false;
}

bool CDisplayPlane::SetControlMode(stm_display_control_mode_t mode)
{
  // Default, return failure for an unsupported control
  return false;
}

bool CDisplayPlane::ApplySyncControls()
{
  // Default, return failure for an unsupported control
  return false;
}

DisplayPlaneResults CDisplayPlane::SetCompoundControl(stm_display_plane_control_t ctrl, void * newVal)
{
  DisplayPlaneResults result = STM_PLANE_NOT_SUPPORTED;
  switch(ctrl)
  {
    case PLANE_CTRL_INPUT_WINDOW_VALUE:
      if ( m_pDisplayDevice->VSyncLock() != 0 )
        return STM_PLANE_EINTR;
      // The VSync interrupt should be blocked while we update the IO Windows
      m_InputWindowValue = *((stm_rect_t *)newVal);
      m_ContextChanged = true;
      m_pDisplayDevice->VSyncUnlock();

      TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new IW)", GetName() );
      TRC( TRC_ID_MAIN_INFO, "New IW: x=%d y=%d w=%u h=%u",  m_InputWindowValue.x, m_InputWindowValue.y, m_InputWindowValue.width, m_InputWindowValue.height );
      result = STM_PLANE_OK;
      break;

    case PLANE_CTRL_OUTPUT_WINDOW_VALUE:
      if ( m_pDisplayDevice->VSyncLock() != 0 )
        return STM_PLANE_EINTR;
      // The VSync interrupt should be blocked while we update the IO Windows
      m_OutputWindowValue = *((stm_rect_t *)newVal);
      m_ContextChanged = true;
      m_pDisplayDevice->VSyncUnlock();

      TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new OW)", GetName() );
      TRC( TRC_ID_MAIN_INFO, "New OW: x=%d y=%d w=%u h=%u",  m_OutputWindowValue.x, m_OutputWindowValue.y, m_OutputWindowValue.width, m_OutputWindowValue.height );
      result = STM_PLANE_OK;
      break;

    case PLANE_CTRL_CONNECT_TO_SOURCE:
    case PLANE_CTRL_DISCONNECT_FROM_SOURCE:

      result = AddCompoundControlToQueue(ctrl, newVal);
      if (!m_pOutput && (result == STM_PLANE_OK) )
      {
        if (m_pDisplayDevice->VSyncLock() != 0)
          return STM_PLANE_EINTR;
        result = ProcessControlQueue(0);
        m_pDisplayDevice->VSyncUnlock();
      }


      break;

    default:
      break;
  }

  // Default, return failure for an unsupported control
  return result;
}

DisplayPlaneResults CDisplayPlane::GetCompoundControl(stm_display_plane_control_t ctrl, void * currentVal)
{
  stm_compound_control_latency_t *pDisplayLatency;
  DisplayPlaneResults result = STM_PLANE_NOT_SUPPORTED;

  /* NB: "m_pDisplayDevice->VSyncLock()" is not taken when reading the variables below because they can be
         modified ONLY by stm_display_plane_set_compound_control() and stm_display_plane_set_control().
         Thanks to the STKPI semaphore, we are sure that the GetControl() and GetCompoundControl() functions
         are not executed at the same time as the set() action.
  */

  switch(ctrl)
  {
    case PLANE_CTRL_INPUT_WINDOW_VALUE:

      result = STM_PLANE_INVALID_VALUE;
      if(m_InputWindowMode==MANUAL_MODE && m_OutputWindowMode==MANUAL_MODE)
      {
          // In manual mode, return last user value programmed if valid
          if(!(m_InputWindowValue.x==0 && m_InputWindowValue.y==0 &&
               m_InputWindowValue.width==0 && m_InputWindowValue.height==0))
          {
            *((stm_rect_t *)currentVal) = m_InputWindowValue;
            result = STM_PLANE_OK;
          }
      }
      else
      {
          // At least input or output window is in automatic mode,
          // so return last computed value if valid
          // else return last user value programmed if valid and manual mode
          if(!(m_ComputedInputWindowValue.x==0 && m_ComputedInputWindowValue.y==0 &&
               m_ComputedInputWindowValue.width==0 && m_ComputedInputWindowValue.height==0))
          {
            *((stm_rect_t *)currentVal) = m_ComputedInputWindowValue;
            result = STM_PLANE_OK;
          }
          else if(!(m_InputWindowValue.x==0 && m_InputWindowValue.y==0 &&
                    m_InputWindowValue.width==0 && m_InputWindowValue.height==0)
                    && m_InputWindowMode==MANUAL_MODE)
          {
            *((stm_rect_t *)currentVal) = m_InputWindowValue;
            result = STM_PLANE_OK;
          }
      }

      if(result==STM_PLANE_OK)
      {
        TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_INPUT_WINDOW_VALUE x=%d y=%d w=%u h=%u",  ((stm_rect_t *)currentVal)->x, ((stm_rect_t *)currentVal)->y, ((stm_rect_t *)currentVal)->width, ((stm_rect_t *)currentVal)->height );
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_INPUT_WINDOW_VALUE Result=%d", result );
      }

      break;
    case PLANE_CTRL_OUTPUT_WINDOW_VALUE:

      result = STM_PLANE_INVALID_VALUE;
      if(m_InputWindowMode==MANUAL_MODE && m_OutputWindowMode==MANUAL_MODE)
      {
          // In manual mode, return last user value programmed if valid
          if(!(m_OutputWindowValue.x==0 && m_OutputWindowValue.y==0 &&
               m_OutputWindowValue.width==0 && m_OutputWindowValue.height==0))
          {
            *((stm_rect_t *)currentVal) = m_OutputWindowValue;
            result = STM_PLANE_OK;
          }
      }
      else
      {
          // At least input or output window is in automatic mode,
          // so return last computed value if valid
          // else return last user value programmed if valid and manual mode
          if(!(m_ComputedOutputWindowValue.x==0 && m_ComputedOutputWindowValue.y==0 &&
               m_ComputedOutputWindowValue.width==0 && m_ComputedOutputWindowValue.height==0))
          {
            *((stm_rect_t *)currentVal) = m_ComputedOutputWindowValue;
            result = STM_PLANE_OK;
          }
          else if(!(m_OutputWindowValue.x==0 && m_OutputWindowValue.y==0 &&
                    m_OutputWindowValue.width==0 && m_OutputWindowValue.height==0)
                    && m_OutputWindowMode==MANUAL_MODE)
          {
            *((stm_rect_t *)currentVal) = m_OutputWindowValue;
            result = STM_PLANE_OK;
          }
      }

      if(result==STM_PLANE_OK)
      {
        TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_OUTPUT_WINDOW_VALUE x=%d y=%d w=%u h=%u",  ((stm_rect_t *)currentVal)->x, ((stm_rect_t *)currentVal)->y, ((stm_rect_t *)currentVal)->width, ((stm_rect_t *)currentVal)->height );
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "PLANE_CTRL_OUTPUT_WINDOW_VALUE Result=%d", result );
      }

      break;


    case PLANE_CTRL_MIN_VIDEO_LATENCY:
    case PLANE_CTRL_MAX_VIDEO_LATENCY:
        // On GDP, VDP or HQVDP planes, the latency is null because the pixel are read in memory and immediately output to the TV. This is the default latency.
        // On FVDP planes, the latency is not null because the picture is stored in internal FVDP buffers before being displayed.
        pDisplayLatency = (stm_compound_control_latency_t *) currentVal;

        pDisplayLatency->nb_input_periods  = 0;
        pDisplayLatency->nb_output_periods = 0;

        if (ctrl == PLANE_CTRL_MIN_VIDEO_LATENCY)
            TRC( TRC_ID_UNCLASSIFIED, "Min plane latency : In %d, Out %d", pDisplayLatency->nb_input_periods, pDisplayLatency->nb_output_periods );
        else
            TRC( TRC_ID_UNCLASSIFIED, "Max plane latency : In %d, Out %d", pDisplayLatency->nb_input_periods, pDisplayLatency->nb_output_periods );

        result = STM_PLANE_OK;
        break;

    default:
      break;
  }

  // Default, return failure for an unsupported control
  return result;
}

bool CDisplayPlane::GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range)
{
  bool result = false;

  switch(selector)
  {
    case PLANE_CTRL_INPUT_WINDOW_VALUE:
      range->min_val.x = 0;
      range->min_val.y = 0;
      range->min_val.width = 32;  // 2 macroblocks for smallest rectangle
      range->min_val.height = 32;
      range->max_val.x = 0;
      range->max_val.y = 0;
      range->max_val.width = 1920;
      range->max_val.height = 1080;
      range->default_val.x = 0;
      range->default_val.y = 0;
      range->default_val.width = 1280;
      range->default_val.height = 720;
      range->step.x = 0;
      range->step.y = 0;
      range->step.width = 1;
      range->step.height = 1;
      TRC( TRC_ID_MAIN_INFO, "IOW CDisplayPlane::GetCompoundControlRange selector=PLANE_CTRL_INPUT_WINDOW_VALUE" );
      result = true;
      break;

    case PLANE_CTRL_OUTPUT_WINDOW_VALUE:
      range->min_val.x = 0;
      range->min_val.y = 0;
      range->min_val.width = 32;  // 2 macroblocks for smallest rectangle
      range->min_val.height = 32;
      range->max_val.x = 0;
      range->max_val.y = 0;
      range->max_val.width = 1920;
      range->max_val.height = 1080;
      range->default_val.x = 0;
      range->default_val.y = 0;
      range->default_val.width = 1280;
      range->default_val.height = 720;
      range->step.x = 0;
      range->step.y = 0;
      range->step.width = 1;
      range->step.height = 1;
      TRC( TRC_ID_MAIN_INFO, "IOW CDisplayPlane::GetCompoundControlRange selector=PLANE_CTRL_OUTPUT_WINDOW_VALUE" );
      result = true;
      break;
    default:
      break;
  }

  // Default, return failure for an unsupported control
  return result;
}

bool CDisplayPlane::GetTuningDataRevision(stm_display_plane_tuning_data_control_t ctrl, uint32_t * revision)
{
  // Default, return failure for an unsupported control
  return false;
}

DisplayPlaneResults CDisplayPlane::GetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * currentVal)
{
  // Default, return failure for an unsupported control
  return STM_PLANE_NOT_SUPPORTED;
}

DisplayPlaneResults CDisplayPlane::SetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * newVal)
{
  // Default, return failure for an unsupported control
  return STM_PLANE_NOT_SUPPORTED;
}

bool CDisplayPlane::SetDepth(COutput *pOutput, int32_t depth, bool activate)
{
  if(!pOutput || !pOutput->CanShowPlane(this))
    return false;

  if(!pOutput->SetPlaneDepth(this, depth, activate))
    return false;

  return true;
}


bool CDisplayPlane::GetDepth(COutput *pOutput, int *depth) const
{
  if(!pOutput || !pOutput->CanShowPlane(this))
    return false;

  if(!pOutput->GetPlaneDepth(this, depth))
    return false;

  return true;
}


bool CDisplayPlane::Pause(void)
{
    // The current "implementation" of m_isPaused will freeze the display, but it will
    // also prevent the buffer queue from being processed. So there is some work to do
    // to implement the required semantics of plane "pause" for the legacy SoCs.
    #if 0
    /*
     * This allows us to pause the current content on the screen, while the
     * queue still gets processed behind the scenes.
     */

    m_pDisplayDevice->VSyncLock();
    if(!hasBuffersToDisplay())
    {
        m_pDisplayDevice->VSyncUnlock();
        return false;
    }

    m_isPaused = true;
    m_status  |= STM_STATUS_PLANE_PAUSED;

    m_pDisplayDevice->VSyncUnlock();

    return true;
    #endif

    /* Not yet supported */
    return false;
}

bool CDisplayPlane::UnPause(void)
{
#if 0
    /* Not yet supported */
    /*
     * We only resume a paused plane if there are new buffers
     * to display in the queue.
     */
    m_isPaused = false;
    m_status  &= ~STM_STATUS_PLANE_PAUSED;

    return true;
#endif

    /* Not yet supported */
    return false;
}

void CDisplayPlane::EnableHW(void)
{
  if(!m_isEnabled)
  {
    TRC( TRC_ID_MAIN_INFO, "" );
    m_isEnabled = true;
    UpdatePlaneVisibility();
  }
}


void CDisplayPlane::DisableHW(void)
{
  if(m_isEnabled)
  {
    TRC( TRC_ID_MAIN_INFO, "" );
    m_isEnabled = false;
    UpdatePlaneVisibility();
  }
}


bool CDisplayPlane::HideRequestedByTheApplication(void)
{
  bool status = false;

  if ( m_pDisplayDevice->VSyncLock() != 0 )
    return false;

  if(!m_hideRequestedByTheApplication)
  {
    m_hideRequestedByTheApplication = true;
    m_updatePlaneVisibility         = true;

    status = true;
  }

  m_pDisplayDevice->VSyncUnlock();

  return(status);
}


bool CDisplayPlane::ShowRequestedByTheApplication(void)
{
  bool status = false;

  if ( m_pDisplayDevice->VSyncLock() != 0 )
    return false;

  if(m_hideRequestedByTheApplication)
  {
    m_hideRequestedByTheApplication = false;
    m_updatePlaneVisibility         = true;

    status = true;
  }

  m_pDisplayDevice->VSyncUnlock();

  return(status);
}


bool CDisplayPlane::UpdateFromMixer(COutput* pOutput, stm_plane_update_reason_t update_reason)
{
  /*
   * Do nothing if Output is not the same connected on this plane
   */
  if(m_pOutput != pOutput)
    return true;

  // Check that this function is called from thread context and not from interrupt context
  DEBUG_CHECK_THREAD_CONTEXT();

  ////////// Block the VSync ThreadedIRQ while update the output info //////////
  if(m_pDisplayDevice->VSyncLock() != 0)
    return false;

  switch(update_reason)
  {
    case STM_PLANE_OUTPUT_STOPPED:
    {
      TRC( TRC_ID_MAIN_INFO, "Plane %d %s: STM_PLANE_OUTPUT_STOPPED", GetID(), GetName() );
      DisableHW();
      ResetDisplayInfo();
    }
    break;

    case STM_PLANE_OUTPUT_UPDATED:
    {
      TRC( TRC_ID_MAIN_INFO, "Plane %d %s: STM_PLANE_OUTPUT_UPDATED", GetID(), GetName() );
      // The output has changed. Get some info about the new display mode
      FillCurrentOuptutInfo();
    }
    break;

    case STM_PLANE_OUTPUT_STARTED:
    {
      TRC( TRC_ID_MAIN_INFO, "Plane %d %s: STM_PLANE_OUTPUT_STARTED", GetID(), GetName() );
      // The output has changed. Get some info about the new display mode
      FillCurrentOuptutInfo();
    }
    break;

    default:
      TRC( TRC_ID_ERROR, "Plane %s: Invalid update_reason (%d)", GetName(), update_reason);
      m_pDisplayDevice->VSyncUnlock();
      return false;
  }

  m_pDisplayDevice->VSyncUnlock();
  /////////////////////////////////////////////////////////////////////////////

  return true;
}


void CDisplayPlane::UpdatePlaneVisibility(void)
{
  // Check that VSyncLock is already taken before accessing to shared variables
  DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

  // Reset flag to indicate that it has been executed
  m_updatePlaneVisibility = false;

  // Check all the INTERNAL conditions that could lead to disable this plane:
  if ( (!m_pOutput)                       ||  // The plane is connected to no output
       (!m_pSource)                       ||  // The plane is connected to no source
       (!m_isEnabled)                     ||  // This plane is not enabled so it can not be visible
       (!m_outputInfo.isOutputStarted) )     // This plane output is not started so it can not be visible
  {
    goto disable_plane;
  }

  if (m_hideRequestedByTheApplication)   // The application is requesting to hide this plane
  {
    if(isVideoPlane())
    {
      // For video plane, check user hiding mode
      switch (m_eHideMode)
      {
        case PLANE_HIDE_BY_MASKING:
          goto mask_plane;
          break;

        case PLANE_HIDE_BY_DISABLING:
        default:
          goto disable_plane;
      }
    }
    else
    {
      // For GDP, always disable at mixer level
      goto disable_plane;
    }
  }

  // Show plane

  m_planeVisibily = STM_PLANE_ENABLED;

  // At this point we are sure that m_pOutput is not NULL
  if(!m_pOutput->ShowPlane(this))
  {
    TRC( TRC_ID_ERROR, "Unable to enable hardware for plane %u", GetID()  );
    return;
  }

  m_status |= STM_STATUS_PLANE_VISIBLE;

  TRC( TRC_ID_MAIN_INFO, "Plane %d %s is enabled", GetID(), GetName()  );
  return;

disable_plane:
  // Hide plane by disabling it

  m_planeVisibily = STM_PLANE_DISABLED;

  // Warning: m_pOutput can be null if the plane is not connected to an output!
  if(m_pOutput)
    m_pOutput->HidePlane(this);

  m_status &= ~STM_STATUS_PLANE_VISIBLE;

  TRC( TRC_ID_MAIN_INFO, "Plane %d %s is disabled", GetID(), GetName()  );
  return;

mask_plane:
  // Hide plane by masking it

  m_planeVisibily = STM_PLANE_MASKED;

  m_status &= ~STM_STATUS_PLANE_VISIBLE;

  TRC( TRC_ID_MAIN_INFO, "Plane %d %s is masked", GetID(), GetName()  );
}


int CDisplayPlane::GetFormats(const stm_pixel_format_t** pData) const
{
    *pData = m_pSurfaceFormats;
    return m_nFormats;
}

int CDisplayPlane::GetListOfFeatures( stm_plane_feature_t *list, uint32_t number)
{
    if(number > m_nFeatures)
        return -ERANGE;

    if(number > 0 && list != NULL )
    {
        for(uint32_t f=0; f<number; f ++)
            list[f]=m_pFeatures[f] ;
    }

    return m_nFeatures;
}

bool CDisplayPlane::IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const
{
    bool isFeatureSupported = false;

    // Go trough list of feature to check the requested one is available
    for(uint32_t f=0; f<m_nFeatures; f++)
        if(m_pFeatures[f] == feature)
        {
            isFeatureSupported=true;
            break;
        }

    // By default, a supported feature is applicable but it can be customized in a derived method
    if(applicable)
        *applicable = isFeatureSupported;

    return (isFeatureSupported);
}


void CDisplayPlane::ResetQueueBufferState(void) {}

////////////////////////////////////////////////////////////////////////////////
//

bool CDisplayPlane::GetRGBYCbCrKey(uint8_t&            ucRCr,
                                   uint8_t&            ucGY,
                                   uint8_t&            ucBCb,
                                   uint32_t            ulPixel,
                                   stm_pixel_format_t colorFmt,
                                   bool                pixelIsRGB)
{
  bool  bResult = true;

  uint32_t R = (ulPixel & 0xFF0000) >> 16;
  uint32_t G = (ulPixel & 0xFF00) >> 8;
  uint32_t B = (ulPixel & 0xFF);

  switch(colorFmt)
  {
    case SURF_YCBCR422MB:
    case SURF_YCBCR422R:
    case SURF_YCBCR420MB:
    case SURF_YUYV:
    case SURF_YUV420:
    case SURF_YVU420:
    case SURF_YUV422P:
    case SURF_YCbCr420R2B:
    case SURF_YCbCr422R2B:
      if (pixelIsRGB)
      {
        ucRCr = (uint8_t)(128 + ((131 * R) / 256) - (((110 * G) / 256) + ((21 * B) / 256)));
        ucGY  = (uint8_t)(((77 * R) /256) + ((150 * G) / 256) + ((29 * B) / 256));
        ucBCb = (uint8_t)(128 + ((131 * B) / 256) - (((44 * R) / 256) + ((87 * G) / 256)));
      }
      else
      {
    ucRCr = R;
    ucGY  = G;
    ucBCb = B;
      }
    break;

    case SURF_RGB565:
    case SURF_RGB888:
    case SURF_ARGB8565:
    case SURF_ARGB8888:
    case SURF_BGRA8888:
    case SURF_ARGB1555:
    case SURF_ARGB4444:
      if (pixelIsRGB)
      {
        ucRCr = (uint8_t)R;
        ucGY  = (uint8_t)G;
        ucBCb = (uint8_t)B;
      }
      else
      {
        TRC( TRC_ID_MAIN_INFO, "Error: pixel format can only be RGB for surface %u.", (uint32_t)colorFmt );
        bResult = false;
      }
    break;

    default:
      TRC( TRC_ID_MAIN_INFO, "Error: unsupported surface format %u.", (uint32_t)colorFmt );
      bResult = false;
    break;
  }

  //Fill the LSBs with zero
  switch(colorFmt)
  {
    case SURF_RGB565:
    case SURF_ARGB8565:
      ucRCr <<= 3;
      ucGY <<= 2;
      ucBCb <<= 3;
    break;

    case SURF_ARGB1555:
      ucRCr <<= 3;
      ucGY <<= 3;
      ucBCb <<= 3;
    break;

    case SURF_ARGB4444:
      ucRCr <<= 4;
      ucGY <<= 4;
      ucBCb <<= 4;
    break;

    default:
      break;
  }

  return bResult;
}

TuningResults CDisplayPlane::SetTuning(uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize)
{
    TuningResults res = TUNING_SERVICE_NOT_SUPPORTED;
    tuning_service_type_t  serviceType = (tuning_service_type_t)service;

    switch(serviceType)
    {
        case RESET_STATISTICS:
        {
            vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));
            res = TUNING_OK;
            break;
        }
        default:
            break;
    }
    return res;
}

int CDisplayPlane::GetConnectedOutputID(uint32_t *id, uint32_t max_ids)
{
  uint32_t        numId = 0;

  if (id == 0)
    return m_numConnectedOutputs;

  for(uint32_t i=0; (i<m_numConnectedOutputs) && (numId<max_ids) ; i++)
  {
  // only one output is supported for the moment.
  // Need to be updated when multiple output support is available
  //    COutput       *pOutput  = m_pConnectedOutputs[i];
      COutput       *pOutput  = m_pOutput;
      if (pOutput)
      {
        id[numId++] = pOutput->GetID();
      }
  }

  return numId;
}


int CDisplayPlane::GetAvailableSourceID(uint32_t *id, uint32_t max_ids)
{
  uint32_t        numId = 0;
  uint32_t        numSources;
  CDisplaySource* source = 0;

  numSources= m_pDisplayDevice->GetNumberOfSources();
  if (id == 0)
    return numSources;

  for(uint32_t i=0; (i<numSources) && (numId<max_ids) ; i++)
  {
    source = m_pDisplayDevice->GetSource(i);
    id[numId++] = (source)?source->GetID():0xFFFF;
  }

  return numId;
}

void CDisplayPlane::Suspend(void)
{
  TRC( TRC_ID_MAIN_INFO, "%s",GetName() );
  m_bIsSuspended = true;
}

void CDisplayPlane::Freeze(void)
{
  TRC( TRC_ID_MAIN_INFO, "%s",GetName() );
  m_bIsFrozen = true;
}

void CDisplayPlane::Resume(void)
{
  TRC( TRC_ID_MAIN_INFO, "%s",GetName() );

  m_bIsFrozen = false;
  m_bIsSuspended = false;

  // We exit from suspended state so it is better to reset the history
  if ( m_pDisplayDevice->VSyncLock() != 0 )
    return;
  m_ContextChanged = true;
  TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (leaving low power mode)", GetName() );
  m_pDisplayDevice->VSyncUnlock();

}

void CDisplayPlane::ResetComputedInputOutputWindowValues(void)
{
  m_ComputedInputWindowValue.x      = 0;
  m_ComputedInputWindowValue.y      = 0;
  m_ComputedInputWindowValue.width  = 0;
  m_ComputedInputWindowValue.height = 0;

  m_ComputedOutputWindowValue.x      = 0;
  m_ComputedOutputWindowValue.y      = 0;
  m_ComputedOutputWindowValue.width  = 0;
  m_ComputedOutputWindowValue.height = 0;
}

// Return true when some connections to the sources or output have changed.
bool CDisplayPlane::AreConnectionsChanged(void)
{
    if (m_areConnectionsChanged)
    {
        // The flag is reset after being read
        m_areConnectionsChanged = false;
        return true;
    }
    else
    {
        return false;
    }
}

bool CDisplayPlane::isOutputModeChanged(void)
{
    if (m_isOutputModeChanged)
    {
        // The flag is reset after being read
        m_isOutputModeChanged = false;
        return true;
    }
    else
    {
        return false;
    }
}

stm_display_use_cases_t CDisplayPlane::GetCurrentUseCase(BufferNodeType srcType, bool isDisplayInterlaced, bool isTopFieldOnDisplay)
{
    stm_display_use_cases_t useCase = MAX_USE_CASES;

    /* to find the current use case */
    switch(srcType)
    {
        case GNODE_PROGRESSIVE:
        {
            if(isDisplayInterlaced)
            {
                if(isTopFieldOnDisplay)
                    useCase = FRAME_TO_TOP;
                else
                    useCase = FRAME_TO_BOTTOM;
            }
            else
            {
                useCase = FRAME_TO_FRAME;
            }
            break;
        }
        case GNODE_TOP_FIELD:
        {
            if(isDisplayInterlaced)
            {
                if(isTopFieldOnDisplay)
                    useCase = TOP_TO_TOP;
                else
                    useCase = TOP_TO_BOTTOM;
            }
            else
            {
                useCase = TOP_TO_FRAME;
            }
            break;
        }
        case GNODE_BOTTOM_FIELD:
        {
            if(isDisplayInterlaced)
            {
                if(isTopFieldOnDisplay)
                    useCase = BOTTOM_TO_TOP;
                else
                    useCase = BOTTOM_TO_BOTTOM;
            }
            else
            {
                useCase = BOTTOM_TO_FRAME;
            }
            break;
        }
    }

    TRC( TRC_ID_UNCLASSIFIED, "Use case = %d", useCase );
    return useCase;
}

bool CDisplayPlane::IsContextChanged(CDisplayNode *pNodeToBeDisplayed, bool isPictureRepeated)
{

    if(m_ContextChanged)
    {
        TRCBL(TRC_ID_CONTEXT_CHANGE);
        TRC(TRC_ID_CONTEXT_CHANGE, "%s: *** Context has changed ***", GetName() );
        m_ContextChanged = false;
        return true;
    }

    // If the same picture is repeated, no need to check if the "source characteristics" have changed because references have
    // already been recomputed when this picture has been displayed for the first time.
    // If we don't do that, we have a problem when there is a pause on a picture with new characteristics.
    // This picture will be repeated many times and the driver will think that the context has changed all the time.
    if (!isPictureRepeated)
    {
        // This is a new picture. Check if the source characteristics have changed compare to the previous picture
        if(pNodeToBeDisplayed->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY)
        {
            TRCBL(TRC_ID_CONTEXT_CHANGE);
            TRC(TRC_ID_CONTEXT_CHANGE, "%s: *** Src picture characteristics have changed ***", GetName() );
            return true;
        }
    }

    /* The context is unchanged */
    return false;
}

uint32_t CDisplayPlane::GetHorizontalDecimationFactor(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    uint32_t horizontalDecimationFactor;

    switch(pNodeToDisplay->m_bufferDesc.src.horizontal_decimation_factor)
    {
        case STM_NO_DECIMATION:
        {
            TRC(TRC_ID_MAIN_INFO, "H1");
            horizontalDecimationFactor = 1;
            break;
        }
        case STM_DECIMATION_BY_TWO:
        {
            TRC(TRC_ID_MAIN_INFO, "H2");
            horizontalDecimationFactor = 2;
            break;
        }
        case STM_DECIMATION_BY_FOUR:
        {
            TRC(TRC_ID_MAIN_INFO, "H4");
            horizontalDecimationFactor = 4;
            break;
        }
        case STM_DECIMATION_BY_EIGHT:
        {
            TRC(TRC_ID_MAIN_INFO, "H8");
            horizontalDecimationFactor = 8;
            break;
        }
        default:
        {
            TRC(TRC_ID_ERROR, "Invalid horizontal decimation factor");
            horizontalDecimationFactor = 1;
            break;
        }
    }

    return horizontalDecimationFactor;
}

uint32_t CDisplayPlane::GetVerticalDecimationFactor(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    uint32_t verticalDecimationFactor;

    switch(pNodeToDisplay->m_bufferDesc.src.vertical_decimation_factor)
    {
        case STM_NO_DECIMATION:
        {
            TRC(TRC_ID_MAIN_INFO, "V1");
            verticalDecimationFactor = 1;
            break;
        }
        case STM_DECIMATION_BY_TWO:
        {
            TRC(TRC_ID_MAIN_INFO, "V2");
            verticalDecimationFactor = 2;
            break;
        }
        case STM_DECIMATION_BY_FOUR:
        {
            TRC(TRC_ID_MAIN_INFO, "V4");
            verticalDecimationFactor = 4;
            break;
        }
        case STM_DECIMATION_BY_EIGHT:
        {
            TRC(TRC_ID_MAIN_INFO, "V8");
            verticalDecimationFactor = 8;
            break;
        }
        default:
        {
            TRC(TRC_ID_ERROR, "Invalid vertical decimation factor");
            verticalDecimationFactor = 1;
            break;
        }
    }

    return verticalDecimationFactor;
}

bool CDisplayPlane::IsScalingPossible(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    pDisplayInfo->m_isSecondaryPictureSelected = false;
    return true;
}


void CDisplayPlane::FillSelectedPictureFormatInfo(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    switch(pDisplayInfo->m_selectedPicture.colorFmt)
    {
        case SURF_YCBCR420MB:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = false;
            pDisplayInfo->m_selectedPicture.isSrc420 = true;
            pDisplayInfo->m_selectedPicture.isSrc422 = false;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = true;
            break;
        }
        case SURF_YCBCR422MB:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = false;
            pDisplayInfo->m_selectedPicture.isSrc420 = false;
            pDisplayInfo->m_selectedPicture.isSrc422 = true;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = true;
            break;
        }
        case SURF_YUV420:
        case SURF_YVU420:
        case SURF_YCbCr420R2B:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = false;
            pDisplayInfo->m_selectedPicture.isSrc420 = true;
            pDisplayInfo->m_selectedPicture.isSrc422 = false;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = false;
            break;
        }
        case SURF_YCBCR422R:
        case SURF_YUYV:
        case SURF_YUV422P:
        case SURF_YCbCr422R2B:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = false;
            pDisplayInfo->m_selectedPicture.isSrc420 = false;
            pDisplayInfo->m_selectedPicture.isSrc422 = true;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = false;
            break;
        }

        case SURF_YCbCr422R2B10:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = true;
            pDisplayInfo->m_selectedPicture.isSrc420 = false;
            pDisplayInfo->m_selectedPicture.isSrc422 = true;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = false;
            break;
        }

        case SURF_CRYCB101010:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = true;
            pDisplayInfo->m_selectedPicture.isSrc420 = false;
            pDisplayInfo->m_selectedPicture.isSrc422 = false;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = false;
            break;
        }

        case SURF_CRYCB888:
        case SURF_ACRYCB8888:
        {
            pDisplayInfo->m_selectedPicture.isSrcOn10bits = false;
            pDisplayInfo->m_selectedPicture.isSrc420 = false;
            pDisplayInfo->m_selectedPicture.isSrc422 = false;
            pDisplayInfo->m_selectedPicture.isSrcMacroBlock = false;
            break;
        }
        default:
            break;
    }

}


void CDisplayPlane::FillSelectedPictureDisplayInfoFromPrimary(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    TRC(TRC_ID_MAIN_INFO, "Primary Pict selected");

    pDisplayInfo->m_selectedPicture.width        = pNodeToDisplay->m_bufferDesc.src.primary_picture.width;
    pDisplayInfo->m_selectedPicture.height       = pNodeToDisplay->m_bufferDesc.src.primary_picture.height;
    pDisplayInfo->m_selectedPicture.pitch        = pNodeToDisplay->m_bufferDesc.src.primary_picture.pitch;

    pDisplayInfo->m_selectedPicture.colorFmt     = pNodeToDisplay->m_bufferDesc.src.primary_picture.color_fmt;
    pDisplayInfo->m_selectedPicture.pixelDepth   = pNodeToDisplay->m_bufferDesc.src.primary_picture.pixel_depth;

    pDisplayInfo->m_selectedPicture.srcFrameRect = pDisplayInfo->m_primarySrcFrameRect;

    pDisplayInfo->m_selectedPicture.srcHeight    = (pDisplayInfo->m_isSrcInterlaced) ?
                                                    pDisplayInfo->m_selectedPicture.srcFrameRect.height / 2 :
                                                    pDisplayInfo->m_selectedPicture.srcFrameRect.height;

    // Now that pDisplayInfo->m_selectedPicture.colorFmt is set, we can know if the source picture is 4:2:0, MB, 10bits...etc
    FillSelectedPictureFormatInfo(pNodeToDisplay, pDisplayInfo);
}


void CDisplayPlane::FillSelectedPictureDisplayInfoFromSecondary(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    TRC(TRC_ID_MAIN_INFO, "Secondary Pict selected");

    pDisplayInfo->m_selectedPicture.width        = pNodeToDisplay->m_bufferDesc.src.secondary_picture.width;
    pDisplayInfo->m_selectedPicture.height       = pNodeToDisplay->m_bufferDesc.src.secondary_picture.height;
    pDisplayInfo->m_selectedPicture.pitch        = pNodeToDisplay->m_bufferDesc.src.secondary_picture.pitch;

    pDisplayInfo->m_selectedPicture.colorFmt     = pNodeToDisplay->m_bufferDesc.src.secondary_picture.color_fmt;
    pDisplayInfo->m_selectedPicture.pixelDepth   = pNodeToDisplay->m_bufferDesc.src.secondary_picture.pixel_depth;

    pDisplayInfo->m_selectedPicture.srcFrameRect.x      = pDisplayInfo->m_primarySrcFrameRect.x      / pDisplayInfo->m_horizontalDecimationFactor;
    pDisplayInfo->m_selectedPicture.srcFrameRect.y      = pDisplayInfo->m_primarySrcFrameRect.y      / pDisplayInfo->m_verticalDecimationFactor;
    pDisplayInfo->m_selectedPicture.srcFrameRect.width  = pDisplayInfo->m_primarySrcFrameRect.width  / pDisplayInfo->m_horizontalDecimationFactor;
    pDisplayInfo->m_selectedPicture.srcFrameRect.height = pDisplayInfo->m_primarySrcFrameRect.height / pDisplayInfo->m_verticalDecimationFactor;

    /*
     * Horizontal advance decimation is using a ponderated formula based on current and 7
     * following pixels for calculating current pixel value. So at the end of horizontal line,
     * the 7 pixel are coming from next line that's why we reduce visible area of some pixels.
     * Vertical advance decimation is using a ponderated formula based on current and 1
     * following pixel. So only the last vertical line could be bad, but it is not visible
     * that's why no need to remove it.
     */
    const uint32_t SUPP_PIXEL_HORIZONTALLY = 4;
    const uint32_t SUPP_PIXEL_VERTICALLY   = 0;

    if(pDisplayInfo->m_horizontalDecimationFactor != 1)
    {
        /* Reduce visible area for removing last pixels that could be averaged with pixels of following line.*/
        if(pDisplayInfo->m_selectedPicture.srcFrameRect.width > SUPP_PIXEL_HORIZONTALLY)
        {
            pDisplayInfo->m_selectedPicture.srcFrameRect.width -= SUPP_PIXEL_HORIZONTALLY;
        }
    }

    if(pDisplayInfo->m_verticalDecimationFactor != 1)
    {
        /* Reduce visible area for removing last pixels that could be averaged with pixels of following lines.*/
        if(pDisplayInfo->m_selectedPicture.srcFrameRect.height > SUPP_PIXEL_VERTICALLY)
        {
            pDisplayInfo->m_selectedPicture.srcFrameRect.height -= SUPP_PIXEL_VERTICALLY;
        }
    }

    pDisplayInfo->m_selectedPicture.srcHeight = (pDisplayInfo->m_isSrcInterlaced) ?
                                 pDisplayInfo->m_selectedPicture.srcFrameRect.height / 2 :
                                 pDisplayInfo->m_selectedPicture.srcFrameRect.height;

    // Now that pDisplayInfo->m_selectedPicture.colorFmt is set, we can know if the source picture is 4:2:0, MB, 10bits...etc
    FillSelectedPictureFormatInfo(pNodeToDisplay, pDisplayInfo);
}



void CDisplayPlane::FillSelectedPictureDisplayInfo(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    if (pDisplayInfo->m_isSecondaryPictureSelected)
    {
        FillSelectedPictureDisplayInfoFromSecondary(pNodeToDisplay, pDisplayInfo);
    }
    else
    {
        FillSelectedPictureDisplayInfoFromPrimary(pNodeToDisplay, pDisplayInfo);
    }
}


/*
    PrepareIOWindows() will determine the IO Windows to use and decide if the decimated picture should be used (to achieve a bigger downscaling).
    The analyzis is always started with the primary picture. Then, when the IO windows are computed, the driver will check if it is necessary
    to use the secondary (aka decimated) picture.

    The IO Windows will be prepared with the following sequence:
    - We set the "SrcFrameRect". It can be either the  source_visible_area or the  Input Window requested by the application
    - We set the "dstFrameRect". It can be either the display_visible_area or the Output Window requested by the application

    - TruncateIOWindowsToLimits will then ensure that "SrcFrameRect" and "dstFrameRect" are not out of bounds.
    - AdjustIOWindowsForAspectRatioConv will reduce the size of "SrcFrameRect" and/or "dstFrameRect" to preserve the aspect ratio

    - At this stage the IO Windows are calculated so we can then check if a decimation is needed. This is done by a HW specific function.

    - A HW specific function will then be called to adapt the rectangles to the constraint specific to this HW (ex: rounding to even values, or multiple of N...)

    During each of those steps, the IO rectangles will be updated (NB: They can only become smaller).
    In case of error, "areIOWindowsValid" will be set and the rectangles set to null.
*/
bool CDisplayPlane::PrepareIOWindows(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
     pDisplayInfo->m_horizontalDecimationFactor = GetHorizontalDecimationFactor(pNodeToDisplay, pDisplayInfo);
     pDisplayInfo->m_verticalDecimationFactor   = GetVerticalDecimationFactor  (pNodeToDisplay, pDisplayInfo);

    if (!SetSrcFrameRect(pNodeToDisplay, pDisplayInfo, pDisplayInfo->m_primarySrcFrameRect))
        goto error;

    if (!SetDstFrameRect(pNodeToDisplay, pDisplayInfo, pDisplayInfo->m_dstFrameRect))
        goto error;

    if (!TruncateIOWindowsToLimits(pNodeToDisplay,
                                   pDisplayInfo,
                                   pDisplayInfo->m_primarySrcFrameRect,
                                   pDisplayInfo->m_dstFrameRect))
        goto error;

    if (!AdjustIOWindowsForAspectRatioConv(pNodeToDisplay,
                                           pDisplayInfo,
                                           pDisplayInfo->m_primarySrcFrameRect,
                                           pDisplayInfo->m_dstFrameRect) )
        goto error;

    // Now that the srcFrameRect and dstFrameRect are known, check whether the scaling is possible
    // This function will select the secondary picture if necessary
    if (!IsScalingPossible(pNodeToDisplay,
                           pDisplayInfo) )
        goto error;

    if(!AdjustIOWindowsForHWConstraints(pNodeToDisplay, pDisplayInfo))
        goto error;

    // AdjustIOWindowsForHWConstraints() may have slightly changed "m_primarySrcFrameRect" so we should update the info contained in pDisplayInfo->m_selectedPicture
    FillSelectedPictureDisplayInfo(pNodeToDisplay, pDisplayInfo);

    if(m_outputInfo.isDisplayInterlaced)
    {
      // Interlaced output

      /*
       * Note that:
       * - the start line must also be located on a top field to prevent fields
       *   getting accidentally swapped so "y" must be even.
       * - we do not support an "odd" number of lines on an interlaced display,
       *   there must always be a matched top and bottom field line.
       */
      pDisplayInfo->m_dstFrameRect.y = pDisplayInfo->m_dstFrameRect.y & ~(0x1);
      pDisplayInfo->m_dstHeight      = pDisplayInfo->m_dstFrameRect.height / 2;
    }
    else
    {
      // Progressive output
      pDisplayInfo->m_dstHeight      = pDisplayInfo->m_dstFrameRect.height;
    }

    // IO Windows calculated successfully!
    pDisplayInfo->m_areIOWindowsValid = true;

    // Save computed input and output window value to return to user in AUTO mode
    m_ComputedInputWindowValue = pDisplayInfo->m_primarySrcFrameRect;
    m_ComputedOutputWindowValue = pDisplayInfo->m_dstFrameRect;

    TRC(TRC_ID_MAIN_INFO, "Plane %s: New IO Windows", GetName() );

    TRC(TRC_ID_MAIN_INFO, "Selected picture: W=%d H=%d P=%d",
        pDisplayInfo->m_selectedPicture.width,
        pDisplayInfo->m_selectedPicture.height,
        pDisplayInfo->m_selectedPicture.pitch);

    TRC(TRC_ID_MAIN_INFO, "srcFrameRect: x=%4d, y=%4d, w=%4d, h=%4d",
        pDisplayInfo->m_selectedPicture.srcFrameRect.x,
        pDisplayInfo->m_selectedPicture.srcFrameRect.y,
        pDisplayInfo->m_selectedPicture.srcFrameRect.width,
        pDisplayInfo->m_selectedPicture.srcFrameRect.height);

    TRC(TRC_ID_MAIN_INFO, "srcHeight=%4d", pDisplayInfo->m_selectedPicture.srcHeight);

    TRC(TRC_ID_MAIN_INFO, "dstFrameRect: x=%4d, y=%4d, w=%4d, h=%4d",
        pDisplayInfo->m_dstFrameRect.x,
        pDisplayInfo->m_dstFrameRect.y,
        pDisplayInfo->m_dstFrameRect.width,
        pDisplayInfo->m_dstFrameRect.height);

    TRC(TRC_ID_MAIN_INFO, "dstHeight=%4d", pDisplayInfo->m_dstHeight);

    return true;

error:
    // This reset will set m_areIOWindowsValid to false and all the rectangles to null
    pDisplayInfo->Reset();

    ResetComputedInputOutputWindowValues();

    TRC( TRC_ID_ERROR, "Failed to set the IO Windows!" );

    return false;
}

bool CDisplayPlane::TruncateIOWindowsToLimits(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect, stm_rect_t &dstFrameRect) const
{
  int32_t       srcX         = srcFrameRect.x / 16;
  int32_t       srcY         = srcFrameRect.y / 16;
  stm_rect_t    visible_area = pNodeToDisplay->m_bufferDesc.src.visible_area;
  stm_rect_t    display_visible_area = m_outputInfo.displayVisibleArea;
  int32_t       nbr_of_bytes_truncated;

  /* This function is going to check the overlap between various rectangles. Here are the rectangles involved:

     For the source:
     - First there is the full picture size in memory. This info is not needed here.
     - Then there is "SrcVisibleArea" which specifies the only area that should be displayed.
       The content outside this area should be ignored by the display.
     - Then there is "srcFrameRect" which corresponds to what part of the source picture we are asked to display.

     For the output:
     - There is the "DisplayVisibleArea": Nothing can be displayed outside this area.
     - There is "dstFrameRect" which corresponds to where on the output screen we are asked to display.
  */

  if ( (srcFrameRect.width  == 0) ||
       (srcFrameRect.height == 0) ||
       (dstFrameRect.width  == 0) ||
       (dstFrameRect.height == 0) )
  {
    goto nothing_to_display;
  }

  if( IsSrcRectFullyOutOfBounds(pNodeToDisplay, pDisplayInfo, srcFrameRect) ||
      IsDstRectFullyOutOfBounds(pNodeToDisplay, pDisplayInfo, dstFrameRect) )
  {
    goto nothing_to_display;
  }

  //Truncate srcFrameRect to srcVisibleArea
  if (srcX < visible_area.x)
  {
    nbr_of_bytes_truncated = visible_area.x - srcX;    // We are sure that "nbr_of_bytes_truncated" is positive
    srcFrameRect.width -= nbr_of_bytes_truncated;

    srcX = visible_area.x;
    srcFrameRect.x = visible_area.x;
  }
  if(srcY < visible_area.y)
  {
    nbr_of_bytes_truncated = visible_area.y - srcY;    // We are sure that "nbr_of_bytes_truncated" is positive
    srcFrameRect.height -= nbr_of_bytes_truncated;

    srcY = visible_area.y;
    srcFrameRect.y = visible_area.y;
  }

  if(srcX + (int32_t)srcFrameRect.width > visible_area.x + (int32_t)visible_area.width)
  {
    nbr_of_bytes_truncated = (srcX + (int32_t)srcFrameRect.width) - (visible_area.x + (int32_t)visible_area.width);    // We are sure that "nbr_of_bytes_truncated" is positive
    srcFrameRect.width -= nbr_of_bytes_truncated;
  }
  if(srcY + (int32_t)srcFrameRect.height > visible_area.y + (int32_t)visible_area.height)
  {
    nbr_of_bytes_truncated = (srcY + (int32_t)srcFrameRect.height) - (visible_area.y + (int32_t)visible_area.height);  // We are sure that "nbr_of_bytes_truncated" is positive
    srcFrameRect.height -= nbr_of_bytes_truncated;
  }


  //Truncate dstFrameRect to DisplayVisibleArea
  if (dstFrameRect.x < display_visible_area.x)
  {
    nbr_of_bytes_truncated = display_visible_area.x - dstFrameRect.x;    // We are sure that "nbr_of_bytes_truncated" is positive
    dstFrameRect.width -= nbr_of_bytes_truncated;
    dstFrameRect.x = display_visible_area.x;
  }
  if(dstFrameRect.y < display_visible_area.y)
  {
    nbr_of_bytes_truncated = display_visible_area.y - dstFrameRect.y;    // We are sure that "nbr_of_bytes_truncated" is positive
    dstFrameRect.height -= nbr_of_bytes_truncated;
    dstFrameRect.y = display_visible_area.y;
  }

  if(dstFrameRect.x + (int32_t)dstFrameRect.width > display_visible_area.x + (int32_t)display_visible_area.width)
  {
    nbr_of_bytes_truncated = (dstFrameRect.x + (int32_t)dstFrameRect.width) - (display_visible_area.x + (int32_t)display_visible_area.width);    // We are sure that "nbr_of_bytes_truncated" is positive
    dstFrameRect.width -= nbr_of_bytes_truncated;
  }
  if(dstFrameRect.y + (int32_t)dstFrameRect.height > display_visible_area.y + (int32_t)display_visible_area.height)
  {
    nbr_of_bytes_truncated = (dstFrameRect.y + (int32_t)dstFrameRect.height) - (display_visible_area.y + (int32_t)display_visible_area.height);  // We are sure that "nbr_of_bytes_truncated" is positive
    dstFrameRect.height -= nbr_of_bytes_truncated;
  }

  return true;

nothing_to_display:
  // Indicate that IO windows are invalids but incoming picture will not be refused.
  TRC(TRC_ID_ERROR, "Plane %s: Invalid src or dst rect! Src=(%d,%d,%d,%d) Dst=(%d,%d,%d,%d)",
        GetName(),
        srcFrameRect.x,
        srcFrameRect.y,
        srcFrameRect.width,
        srcFrameRect.height,
        dstFrameRect.x,
        dstFrameRect.y,
        dstFrameRect.width,
        dstFrameRect.height);

  return false;
}

bool CDisplayPlane::IsSrcRectFullyOutOfBounds(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect) const
{
  stm_rect_t    visible_area = pNodeToDisplay->m_bufferDesc.src.visible_area;

  int32_t x = srcFrameRect.x/16;
  int32_t y = srcFrameRect.y/16;

  if( (x > (visible_area.x + (int32_t)visible_area.width) )  ||
      (x + (int32_t)srcFrameRect.width < visible_area.x)     ||
      (y > (visible_area.y + (int32_t)visible_area.height) ) ||
      (y + (int32_t)srcFrameRect.height < visible_area.y) )
    return true;
  else
    return false;
}

bool CDisplayPlane::IsDstRectFullyOutOfBounds(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &dstFrameRect) const
{
  stm_rect_t    display_visible_area = m_outputInfo.displayVisibleArea;

  if( (dstFrameRect.x > (display_visible_area.x + (int32_t)display_visible_area.width) )  ||
      (dstFrameRect.x + (int32_t)dstFrameRect.width < display_visible_area.x)             ||
      (dstFrameRect.y > (display_visible_area.y + (int32_t)display_visible_area.height) ) ||
      (dstFrameRect.y + (int32_t)dstFrameRect.height < display_visible_area.y) )
    return true;
  else
    return false;
}

bool CDisplayPlane::AdjustIOWindowsForAspectRatioConv(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect, stm_rect_t &dstFrameRect) const
{
  // SAR = Source  Aspect Ratio
  // DAR = Display Aspect Ratio
  uint64_t       sar_numerator, sar_denominator;
  uint64_t       dar_numerator, dar_denominator;
  uint64_t       ratioNum, ratioDen;
  stm_rational_t srcPixelAspectRatio, displayPixelAspectRatio;
  stm_plane_aspect_ratio_conversion_mode_t aspectRatioConversionMode;

  // Aspect ratio conversion is always done with information from the PRIMARY picture
  srcPixelAspectRatio = pNodeToDisplay->m_bufferDesc.src.pixel_aspect_ratio;
  displayPixelAspectRatio = m_outputInfo.pixelAspectRatio;

  if( (m_InputWindowMode == MANUAL_MODE) && (m_OutputWindowMode == MANUAL_MODE) )
  {
    TRC( TRC_ID_MAIN_INFO, "Input and Output Windows in manual mode: Aspect Ratio cannot be preserved");
    return true;
  }

  // NB: uint32_t is not big enough for all those calculations
  sar_numerator   = ((uint64_t)srcPixelAspectRatio.numerator       * (uint64_t)srcFrameRect.width );
  sar_denominator = ((uint64_t)srcPixelAspectRatio.denominator     * (uint64_t)srcFrameRect.height);

  dar_numerator   = ((uint64_t)displayPixelAspectRatio.numerator   * (uint64_t)dstFrameRect.width );
  dar_denominator = ((uint64_t)displayPixelAspectRatio.denominator * (uint64_t)dstFrameRect.height);

  if ( (sar_numerator   == 0) ||
       (sar_denominator == 0) ||
       (dar_numerator   == 0) ||
       (dar_denominator == 0) )
  {
    TRC( TRC_ID_ERROR, "Invalid sar or dar!" );
    return false;
  }

  // RATIO = SAR / DAR = (sar_numerator * dar_denominator) / (dar_numerator * sar_denominator)
  ratioNum = (uint64_t)sar_numerator * (uint64_t)dar_denominator;
  ratioDen = (uint64_t)dar_numerator * (uint64_t)sar_denominator;

  vibe_os_rational_reduction(&ratioNum, &ratioDen);
  TRC( TRC_ID_MAIN_INFO, "ratioNum=%llu, ratioDen=%llu", ratioNum, ratioDen);

  // Sanity check to prevent division by 0 in the following code
  if ( (ratioNum == 0) ||
       (ratioDen == 0) )
  {
    TRC( TRC_ID_ERROR, "Invalid ratioNum or ratioDen! (0x%llx / 0x%llx)", ratioNum, ratioDen);
    return false;
  }

  aspectRatioConversionMode = m_AspectRatioConversionMode;

  /* In case of 3D, the aspect ratio conversion is disabled because there are strong constraints on horizontal
     and vertical cropping (to avoid a risk of mismatch between the 2 views) */
  if ( (pNodeToDisplay->m_bufferDesc.src.config_3D.formats != FORMAT_3D_NONE) &&
       (m_outputInfo.display3DModeFlags != STM_MODE_FLAGS_NONE) )
  {
    /* 3D source and 3D display: Force the "Ignore" mode */
    aspectRatioConversionMode = ASPECT_RATIO_CONV_IGNORE;
  }

  // Resize input/output window according to aspect ratio conversion mode selected
  switch(aspectRatioConversionMode)
  {
    case ASPECT_RATIO_CONV_LETTER_BOX:
      TRC( TRC_ID_MAIN_INFO, "AR Conv: LetterBox");
      if(ratioNum < ratioDen)
      {
        uint32_t new_width;
        new_width           = (uint32_t) vibe_os_div64( (ratioNum * dstFrameRect.width), ratioDen );
        dstFrameRect.x      = dstFrameRect.x + (dstFrameRect.width - new_width) / 2;
        dstFrameRect.width  = new_width;
      }
      else
      {
        uint32_t new_height;
        new_height          = (uint32_t) vibe_os_div64( (ratioDen * dstFrameRect.height), ratioNum );
        dstFrameRect.y      = dstFrameRect.y + (dstFrameRect.height - new_height) / 2;
        dstFrameRect.height = new_height;
      }
      break;
    case ASPECT_RATIO_CONV_PAN_AND_SCAN:
      TRC( TRC_ID_MAIN_INFO, "AR Conv: Pan&Scan");
      if(ratioNum < ratioDen)
      {
        uint32_t new_height;
        new_height          = (uint32_t) vibe_os_div64( (ratioNum * srcFrameRect.height), ratioDen );
        // srcFrameRect.y is equal to ( srcFrameRect.y/16 + (srcFrameRect.height - new_height) / 2 ) * 16;
        srcFrameRect.y      = srcFrameRect.y + (srcFrameRect.height - new_height) * 8;
        srcFrameRect.height = new_height;
      }
      else
      {
        uint32_t new_width;
        new_width           = (uint32_t) vibe_os_div64( (ratioDen * srcFrameRect.width), ratioNum );
        // srcFrameRect.x is equal to ( srcFrameRect.x/16 + (srcFrameRect.width - new_width) / 2 ) * 16;
        srcFrameRect.x      = srcFrameRect.x + (srcFrameRect.width - new_width) * 8;
        srcFrameRect.width  = new_width;
      }
      break;
    case ASPECT_RATIO_CONV_COMBINED:
      TRC( TRC_ID_MAIN_INFO, "AR Conv: Combined");
      if(ratioNum < ratioDen)
      {
        uint32_t new_height;
        uint32_t new_width;
        new_height          = (uint32_t) vibe_os_div64( (srcFrameRect.height * (ratioDen + ratioNum)), (2L * ratioDen) );
        // srcFrameRect.y is equal to ( srcFrameRect.y/16 + (srcFrameRect.height - new_height) / 2 ) * 16;
        srcFrameRect.y      = srcFrameRect.y + (srcFrameRect.height - new_height) * 8;
        srcFrameRect.height = new_height;

        new_width           = (uint32_t) vibe_os_div64( (dstFrameRect.width * (ratioDen + ratioNum)), (2L * ratioDen) );
        dstFrameRect.x      = dstFrameRect.x + (dstFrameRect.width - new_width) / 2;
        dstFrameRect.width  = new_width;
      }
      else
      {
        uint32_t new_height;
        uint32_t new_width;
        new_width           = (uint32_t) vibe_os_div64( (srcFrameRect.width * 2 * ratioDen), (ratioDen + ratioNum) );
        // srcFrameRect.x is equal to ( srcFrameRect.x/16 + (srcFrameRect.width - new_width) / 2 ) * 16;
        srcFrameRect.x      = srcFrameRect.x + (srcFrameRect.width - new_width) * 8;
        srcFrameRect.width  = new_width;

        new_height          = (uint32_t) vibe_os_div64( (dstFrameRect.height * 2 * ratioDen), (ratioDen + ratioNum) );
        dstFrameRect.y      = dstFrameRect.y + (dstFrameRect.height - new_height) / 2;
        dstFrameRect.height = new_height;
      }
      break;
    case ASPECT_RATIO_CONV_IGNORE:
      TRC( TRC_ID_MAIN_INFO, "AR Conv: Ignore");
      break;
    default:
      break;
  }

  return true;
}


bool CDisplayPlane::AdjustIOWindowsForHWConstraints(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const
{
    return true;
}


// Set source rectangle (in Frame notation)
bool CDisplayPlane::SetSrcFrameRect(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect) const
{
  if (m_InputWindowMode == AUTO_MODE)
  {
    // Take the full input picture
    srcFrameRect = pNodeToDisplay->m_bufferDesc.src.visible_area;
  }
  else
  {
    // Take the requested input window
    srcFrameRect = m_InputWindowValue;
  }

  // Switch x and y in sixteenth of pixel unit
  srcFrameRect.x *= 16;
  srcFrameRect.y *= 16;

  return true;
}


// Set destination rectangle (in Frame notation)
bool CDisplayPlane::SetDstFrameRect(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &dstFrameRect) const
{
  if (m_OutputWindowMode == AUTO_MODE)
  {
    // Take the full screen
    dstFrameRect = m_outputInfo.displayVisibleArea;
  }
  else
  {
  // Take the requested output window
    dstFrameRect = m_OutputWindowValue;
  }

  return true;
}


// Information filled during the VSync interrupt when the buffer is going to be displayed
bool CDisplayPlane::FillDisplayInfo(CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo)
{
    /* set isSrcInterlaced */
    pDisplayInfo->m_isSrcInterlaced  = ((pNodeToDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) == STM_BUFFER_SRC_INTERLACED);

    /* set VSync Frequency */
    if ( (pNodeToDisplay->m_bufferDesc.src.src_frame_rate.numerator == 0) ||
         (pNodeToDisplay->m_bufferDesc.src.src_frame_rate.denominator == 0) )
    {
        // Invalid SrcFrameRate: Use a default one
        pDisplayInfo->m_srcVSyncFreq = 50000;
    }
    else
    {
        uint32_t SrcFrameRate = (1000 * pNodeToDisplay->m_bufferDesc.src.src_frame_rate.numerator) / pNodeToDisplay->m_bufferDesc.src.src_frame_rate.denominator;
        if(pDisplayInfo->m_isSrcInterlaced)
        {
            pDisplayInfo->m_srcVSyncFreq = 2 * SrcFrameRate;
        }
        else
        {
            pDisplayInfo->m_srcVSyncFreq = SrcFrameRate;
        }
    }
    return true;
}


void CDisplayPlane::ResetDisplayInfo(void)
{
    vibe_os_zero_memory(&m_outputInfo, sizeof(m_outputInfo));
}


// Fill m_outputInfo structure
void CDisplayPlane::FillCurrentOuptutInfo(void)
{
    const stm_display_mode_t*   pCurrentMode;
    stm_rational_t              displayAspectRatio;

    // Check that VSyncLock is already taken before accessing to shared variables
    DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

    if(!m_pOutput)
    {
        // Plane not connected to an output
        goto display_info_not_available;
    }

    m_outputInfo.outputVSyncDurationInUs = m_pOutput->GetFieldOrFrameDuration();
    pCurrentMode = m_pOutput->GetCurrentDisplayMode();

    if(!pCurrentMode)
    {
        // Output not started
        goto display_info_not_available;
    }

    // Checking "mode_id" is enough to know if the mode has changed
    if (m_outputInfo.currentMode.mode_id != pCurrentMode->mode_id)
    {
        // The display mode has changed: Save the info about the new one
        m_outputInfo.isDisplayInterlaced = (pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN);

        // m_isOutputModeChanged will be reset when StreamingEngine has been notified that the Output mode has changed.
        m_isOutputModeChanged = true;

        m_outputInfo.currentMode = *pCurrentMode;

        m_outputInfo.displayVisibleArea.x      = 0;
        m_outputInfo.displayVisibleArea.y      = 0;
        m_outputInfo.displayVisibleArea.width  = pCurrentMode->mode_params.active_area_width;
        m_outputInfo.displayVisibleArea.height = pCurrentMode->mode_params.active_area_height;

        m_outputInfo.display3DModeFlags = pCurrentMode->mode_params.flags & STM_MODE_FLAGS_3D_MASK;

        // (flush history)
        m_ContextChanged = true;
        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new output mode)", GetName() );

        TRC( TRC_ID_MAIN_INFO, "Plane %s: New display mode: %dx%d%c%d (Mode %d)",
                GetName(),
                m_outputInfo.displayVisibleArea.width,
                m_outputInfo.displayVisibleArea.height,
                m_outputInfo.isDisplayInterlaced?'I':'P',
                m_outputInfo.currentMode.mode_params.vertical_refresh_rate,
                m_outputInfo.currentMode.mode_id );
    }

    // Get the Display Aspect Ratio...
    if(m_pOutput->GetCompoundControl(OUTPUT_CTRL_DISPLAY_ASPECT_RATIO, &(displayAspectRatio)) != STM_OUT_OK)
    {
        TRC( TRC_ID_ERROR, "Impossible to get the current display aspect ratio!" );
        goto display_info_not_available;
    }

    // ...and check if it has changed
    if((displayAspectRatio.denominator != m_outputInfo.aspectRatio.denominator)
     ||(displayAspectRatio.numerator   != m_outputInfo.aspectRatio.numerator))
    {
        // Save the new aspect ratio
        m_outputInfo.aspectRatio = displayAspectRatio;

        // (flush history)
        m_ContextChanged = true;
        TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed (new output AR)", GetName() );
    }

    // Warning: The pixelAspectRatio can change even if the displayAspectRatio didn't change.
    // Example: Output mode changed from 1920x1080 (16/9) to 720x576 (16/9)
    //          Both are 16/9 but:
    //          In the 1st case, the pixelAspectRatio is 1:1
    //          In the 2nd case, the pixelAspectRatio is (16*576)/(9*720) = 1.4222
    m_outputInfo.pixelAspectRatio.numerator   = m_outputInfo.aspectRatio.numerator * pCurrentMode->mode_params.active_area_height;
    m_outputInfo.pixelAspectRatio.denominator = m_outputInfo.aspectRatio.denominator * pCurrentMode->mode_params.active_area_width;
    TRC( TRC_ID_MAIN_INFO, "Plane %s: New display aspect ratio: %d/%d",  GetName(), m_outputInfo.aspectRatio.numerator, m_outputInfo.aspectRatio.denominator );

    m_outputInfo.isOutputStarted = true;

    // isOutputStarted has changed so the visbility of the plane may have changed
    UpdatePlaneVisibility();

    UpdateOutputInfoSavedInSource();

    return;

display_info_not_available:
    // This plane is not connected to an output or this output is currently not started
    ResetDisplayInfo();
    m_outputInfo.isOutputStarted = false;

    // isOutputStarted has changed so the visbility of the plane may have changed
    UpdatePlaneVisibility();

    UpdateOutputInfoSavedInSource();

    return;
}


void CDisplayPlane::UpdateOutputInfoSavedInSource(void)
{
    // Update source saved information about the output info that are just changing: TimingID, ...
    CSourceInterface *source_interface;

    if (m_pSource)
    {
        // A source is connected to this plane so get its interface
        source_interface = m_pSource->GetInterface(STM_SOURCE_QUEUE_IFACE);
        // for update output info saved in the source interface
        if (source_interface)
        {
            source_interface->UpdateOutputInfo(this);
        }
    }
}

bool  CDisplayPlane::RegisterStatistics(void)
{
    char Tag[STM_REGISTRY_MAX_TAG_SIZE];

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "Stat_PicDisplayed" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicDisplayed, sizeof( m_Statistics.PicDisplayed )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "Stat_PicRepeated" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicRepeated, sizeof( m_Statistics.PicRepeated )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "Stat_CurDispPicPTS" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.CurDispPicPTS, sizeof( m_Statistics.CurDispPicPTS )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "SourceId" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.SourceId, sizeof( m_Statistics.SourceId )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "Stat_FieldInversions" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.FieldInversions, sizeof( m_Statistics.FieldInversions )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    vibe_os_zero_memory( &Tag, STM_REGISTRY_MAX_TAG_SIZE );
    vibe_os_snprintf( Tag, sizeof( Tag ), "Stat_UnexpectedFieldInversions" );
    if ( stm_registry_add_attribute(( stm_object_h )this, Tag, STM_REGISTRY_UINT32, &m_Statistics.UnexpectedFieldInversions, sizeof( m_Statistics.UnexpectedFieldInversions )) != 0 )
    {
        TRC( TRC_ID_ERROR, "Cannot register '%s' attribute ( %d )", Tag, m_planeID );
        return false;
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' object ( %d )", Tag, m_planeID );
    }

    return true;
}


/*
 * Generic handling of OutputVSync in threaded IRQ.
 * This may be overloaded in CDisplayPlane daughter classes
 * depending if OutputVsync should be handled in IRQ.
 */
void CDisplayPlane::OutputVSyncThreadedIrqUpdateHW(bool isDisplayInterlaced, bool isTopFieldOnDisplay, const stm_time64_t &vsyncTime)
{
    TRC(TRC_ID_VSYNC, "%s at %llu", GetName(), vsyncTime);

    if ( m_updatePlaneVisibility )
    {
        UpdatePlaneVisibility();
    }

    if(m_updateDepth)
    {
        // Reset flag to indicate that it has been executed
        m_updateDepth = false;

        if(!SetDepth(m_pOutput, m_lastRequestedDepth, true))
        {
            TRC(TRC_ID_ERROR, "SetDepth failed on output 0x%p!", m_pOutput);
        }
    }

    ProcessControlQueue(vsyncTime);

    return;
}

bool CDisplayPlane::NothingToDisplay(void)
{
    DisableHW();

    // This will cause the next presented picture to be displayed from scratch (history flushed)
    m_ContextChanged = true;
    TRC(TRC_ID_CONTEXT_CHANGE, "%s: Context changed", GetName() );

    return true;
}

void CDisplayPlane::UpdatePictureDisplayedStatistics(CDisplayNode       *pCurrNode,
                                                     bool                isPictureRepeated,
                                                     bool                isDisplayInterlaced,
                                                     bool                isTopFieldOnDisplay)
{
    bool isNextVSyncTop;

    if (not isPictureRepeated)
    {
        m_Statistics.PicDisplayed++;
    }
    else
    {
        m_Statistics.PicRepeated++;
    }

    if(isDisplayInterlaced & (pCurrNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED))
    {
        // Source and Display are Interlaced: Check if there is a Field Inversion

        // NB: Field Inversions are normal in some cases like FRC or Trickmode.
        //     StreamingEngine is setting the flag STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY when no
        //     FieldInversion is expected.

        // The next VSync will have the opposite polarity of current polarity
        isNextVSyncTop = !isTopFieldOnDisplay;

        if ( ( (pCurrNode->m_srcPictureType == GNODE_BOTTOM_FIELD) &&  isNextVSyncTop) ||
             ( (pCurrNode->m_srcPictureType == GNODE_TOP_FIELD)    && !isNextVSyncTop) )
        {
            m_Statistics.FieldInversions++;

            // Check if this field inversion is expected
            if(pCurrNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY)
            {
                // Field inversion while STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY is set.
                // This can happen at the end of a playback when the plane has nothing else to display
                // (so it keeps repeating the last picture)
                m_Statistics.UnexpectedFieldInversions++;
                TRC(TRC_ID_FIELD_INVERSION, "%s: UnexpectedFieldInversion", GetName() );
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// C Display plane interface
//

extern "C" {

int stm_display_plane_get_name(stm_display_plane_h plane, const char **name)
{
  CDisplayPlane* pDP = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(name))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  *name = pDP->GetName();

  STKPI_UNLOCK(plane->parent_dev);

  return 0;
}

int stm_display_plane_get_device_id(stm_display_plane_h plane, uint32_t *id)
{
  CDisplayPlane* pDP = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  *id = pDP->GetParentDevice()->GetID();

  STKPI_UNLOCK(plane->parent_dev);

  return 0;
}

int stm_display_plane_connect_to_output(stm_display_plane_h plane, stm_display_output_h output)
{
  CDisplayPlane * pDP  = NULL;
  COutput       * pOut = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch( pDP->ConnectToOutput(pOut))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_ALREADY_CONNECTED:
      ret = -EALREADY;
      break;
    case STM_PLANE_CANNOT_CONNECT:
      ret = -EBUSY;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_disconnect_from_output(stm_display_plane_h plane, stm_display_output_h output)
{
  CDisplayPlane * pDP  = NULL;
  COutput       * pOut = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  pDP->DisconnectFromOutput(pOut);

  STKPI_UNLOCK(plane->parent_dev);

  return 0;
}


int stm_display_plane_get_connected_output_id(stm_display_plane_h plane, uint32_t *id, uint32_t number )
{
  CDisplayPlane* pDP = NULL;
  int res = -1;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  res = pDP->GetConnectedOutputID(id, number);

  STKPI_UNLOCK(plane->parent_dev);

  return res;
}



int stm_display_plane_get_available_source_id(stm_display_plane_h plane, uint32_t *id, uint32_t number )
{
  CDisplayPlane* pDP = NULL;
  int res = -1;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  res = pDP->GetAvailableSourceID(id, number);

  STKPI_UNLOCK(plane->parent_dev);

  return res;
}

int stm_display_plane_pause(stm_display_plane_h plane)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->Pause()?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_resume(stm_display_plane_h plane)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->UnPause()?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_show(stm_display_plane_h plane)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->ShowRequestedByTheApplication()?0:-EDEADLK;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_hide(stm_display_plane_h plane)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->HideRequestedByTheApplication()?0:-EDEADLK;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_get_capabilities(stm_display_plane_h plane, stm_plane_capabilities_t *caps)
{
  CDisplayPlane* pDP  = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  *caps = pDP->GetCapabilities();
  TRC(TRC_ID_API_PLANE, "plane : %s, caps : 0x%x", pDP->GetName(), *caps);

  STKPI_UNLOCK(plane->parent_dev);

  return 0;
}


int stm_display_plane_get_image_formats(stm_display_plane_h plane, const stm_pixel_format_t** formats)
{
  CDisplayPlane* pDP  = NULL;
  int n;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  n = pDP->GetFormats(formats);

  STKPI_UNLOCK(plane->parent_dev);

  return n;
}

int stm_display_plane_get_list_of_features(stm_display_plane_h plane, stm_plane_feature_t* list, uint32_t number )
{
  CDisplayPlane* pDP  = NULL;
  int n;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  n = pDP->GetListOfFeatures(list, number);

  STKPI_UNLOCK(plane->parent_dev);

  return n;
}

int stm_display_plane_is_feature_applicable(stm_display_plane_h plane, stm_plane_feature_t feature, bool *applicable )
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->IsFeatureApplicable( feature, applicable) ? 0 : -ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_control(stm_display_plane_h plane, stm_display_plane_control_t control, uint32_t value)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch( pDP->SetControl(control,value))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -ERANGE;
      break;
    case STM_PLANE_EINTR:
      ret = -EINTR;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_get_control(stm_display_plane_h plane, stm_display_plane_control_t control, uint32_t *current_val)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(current_val))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch( pDP->GetControl(control,(uint32_t*)current_val))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_set_depth(stm_display_plane_h plane, stm_display_output_h output, int32_t depth, int32_t activate)
{
  CDisplayPlane * pDP  = NULL;
  COutput       * pOut = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->SetDepth(pOut,depth,(activate != 0))?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_connected_source_id(stm_display_plane_h plane, uint32_t *id)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(id))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->GetConnectedSourceID(id)?0:-ENOTCONN;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_mixer_bypass(stm_display_plane_h  plane,
                                       stm_display_output_h output,
                                       int32_t              exclusive)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  // By waiting futur implementation
  ret = -ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_connect_to_source(stm_display_plane_h plane, stm_display_source_h source)
{
  CDisplayPlane  *pDP = NULL;
  CDisplaySource *pDS = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s source: %s", pDP->GetName(), pDS->GetName() );

  switch(pDP->ConnectToSourceProtected(pDS))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_ALREADY_CONNECTED:
      ret = -EALREADY;
      break;
    case STM_PLANE_CANNOT_CONNECT:
      ret = -EBUSY;
      break;
    case STM_PLANE_EINTR:
      ret = -EINTR;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_disconnect_from_source(stm_display_plane_h plane, stm_display_source_h source)
{
  CDisplayPlane  *pDP = NULL;
  CDisplaySource *pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s source: %s", pDP->GetName(), pDS->GetName() );

 /* Do not test the returned error on purpose to skip error if plane is not */
 /* currently connected to the given source */
  pDP->DisconnectFromSourceProtected(pDS);

  STKPI_UNLOCK(plane->parent_dev);

  return 0;
}


int stm_display_plane_get_depth(stm_display_plane_h plane, stm_display_output_h output, int32_t *depth)
{
  CDisplayPlane * pDP  = NULL;
  COutput       * pOut = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!stm_coredisplay_is_handle_valid(output, VALID_OUTPUT_HANDLE))
    return -EINVAL;

  pOut = output->handle;

  if(!CHECK_ADDRESS(depth))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->GetDepth(pOut,(int *) depth)?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}


int stm_display_plane_get_status(stm_display_plane_h plane, uint32_t *status)
{
  CDisplayPlane *pDP = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;
  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  if(!CHECK_ADDRESS(status))
    return -EFAULT;

  *status = pDP->GetStatus();

  return 0;
}

int stm_display_plane_get_control_range(stm_display_plane_h plane, stm_display_plane_control_t selector, stm_display_plane_control_range_t *range)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(range))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->GetControlRange(selector, range)?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_compound_control (stm_display_plane_h plane, stm_display_plane_control_t  ctrl, void * current_val)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(current_val))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s, ctrl : %d", pDP->GetName(), ctrl);

  switch(pDP->GetCompoundControl(ctrl, current_val))
  {
    case STM_PLANE_OK:
      ret = 0;
      switch ( ctrl ) {
      case PLANE_CTRL_MIN_VIDEO_LATENCY:
      case PLANE_CTRL_MAX_VIDEO_LATENCY:
        TRC(TRC_ID_API_PLANE, "nb_input_periods : %u, nb_output_periods : %u", ((stm_compound_control_latency_t *)current_val)->nb_input_periods, ((stm_compound_control_latency_t *)current_val)->nb_output_periods);
        break;
      default:
        break;
      }
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -EINVAL;
      break;
    case STM_PLANE_NOT_SUPPORTED:
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_compound_control(stm_display_plane_h plane, stm_display_plane_control_t  ctrl, void * new_val)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(new_val))
    return -ERANGE;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch(pDP->SetCompoundControl(ctrl, new_val))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -ERANGE;
      break;
    case STM_PLANE_EINTR:
      ret = -EINTR;
      break;
    case STM_PLANE_NOT_SUPPORTED:
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_compound_control_range(stm_display_plane_h plane, stm_display_plane_control_t selector,
                                                 stm_compound_control_range_t * range)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(range))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s, selector : %d", pDP->GetName(), selector);
  ret = pDP->GetCompoundControlRange(selector, range)?0:-ENOTSUP;

  if ( ret == 0 ) {
    TRC(TRC_ID_API_PLANE, "range.min_val.x : %d, range.min_val.y : %d, range.min_val.width : %u, range.min_val.height : %u", range->min_val.x, range->min_val.y, range->min_val.width, range->min_val.height);
    TRC(TRC_ID_API_PLANE, "range.max_val.x : %d, range.max_val.y : %d, range.max_val.width : %u, range.max_val.height : %u", range->max_val.x, range->max_val.y, range->max_val.width, range->max_val.height);
    TRC(TRC_ID_API_PLANE, "range.default_val.x : %d, range.default_val.y : %d, range.default_val.width : %u, range.default_val.height : %u", range->default_val.x, range->default_val.y, range->default_val.width, range->default_val.height);
    TRC(TRC_ID_API_PLANE, "range.step.x : %d, range.step.y : %d, range.step.width : %u, range.step.height : %u", range->step.x, range->step.y, range->step.width, range->step.height);
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_timing_identifier(stm_display_plane_h plane, uint32_t *timingID)
{
  CDisplayPlane *pDP = NULL;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(timingID))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName() );

  *timingID = pDP->GetTimingID();

  STKPI_UNLOCK(plane->parent_dev);

  return (*timingID == 0)?-ENOTSUP:0;
}

int stm_display_plane_get_tuning_data_revision (stm_display_plane_h  plane,
                                                stm_display_plane_tuning_data_control_t ctrl,
                                                uint32_t * revision)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(revision))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->GetTuningDataRevision(ctrl, revision)?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_tuning_data_control (stm_display_plane_h  plane,
                                               stm_display_plane_tuning_data_control_t ctrl,
                                               stm_tuning_data_t * currentVal )
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(currentVal))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch(pDP->GetTuningDataControl(ctrl, currentVal))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_tuning_data_control (stm_display_plane_h  plane,
                                               stm_display_plane_tuning_data_control_t ctrl,
                                               stm_tuning_data_t * newVal)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(newVal))
    return -ERANGE;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch(pDP->SetTuningDataControl(ctrl, newVal))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -ERANGE;
      break;
    case STM_PLANE_NOT_SUPPORTED:
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_get_control_mode (stm_display_plane_h  plane, stm_display_control_mode_t *mode)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  pDP = plane->handle;

  if(!CHECK_ADDRESS(mode))
    return -EFAULT;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->GetControlMode(mode)?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_control_mode (stm_display_plane_h  plane, stm_display_control_mode_t mode)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->SetControlMode(mode)?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}



int stm_display_plane_apply_sync_controls (stm_display_plane_h  plane)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE)) {
    return -EINVAL;
  }

  if(stm_display_device_is_suspended(plane->parent_dev)) {
    return -EAGAIN;
  }

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0) {
    return -EINTR;
  }

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  ret = pDP->ApplySyncControls()?0:-ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_apply_modesetup (stm_display_plane_h  plane)
{
  CDisplayPlane *pDP = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());
  // By waiting futur implementation
  ret = -ENOTSUP;

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_set_asynch_ctrl_listener(stm_display_plane_h plane, const stm_ctrl_listener_t *listener, int *listener_id)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch( pDP->RegisterAsynchCtrlListener(listener, listener_id))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -ERANGE;
      break;
    case STM_PLANE_EINTR:
      ret = -EINTR;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

int stm_display_plane_unset_asynch_ctrl_listener(stm_display_plane_h plane, int listener_id)
{
  CDisplayPlane* pDP  = NULL;
  int ret;

  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(plane->parent_dev))
    return -EAGAIN;

  pDP = plane->handle;

  if(STKPI_LOCK(plane->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_PLANE, "plane : %s", pDP->GetName());

  switch( pDP->UnregisterAsynchCtrlListener(listener_id))
  {
    case STM_PLANE_OK:
      ret = 0;
      break;
    case STM_PLANE_NOT_SUPPORTED:
      ret = -ENOTSUP;
      break;
    case STM_PLANE_INVALID_VALUE:
      ret = -ERANGE;
      break;
    case STM_PLANE_EINTR:
      ret = -EINTR;
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  STKPI_UNLOCK(plane->parent_dev);

  return ret;
}

void stm_display_plane_close(stm_display_plane_h plane)
{
  if(!stm_coredisplay_is_handle_valid(plane, VALID_PLANE_HANDLE))
    return;

  TRC(TRC_ID_API_PLANE, "plane : %s", plane->handle->GetName());
  stm_coredisplay_magic_clear(plane);

  /*
   * Remove object instance from the registry before exiting.
   */
  if(stm_registry_remove_object(plane) < 0)
    TRC( TRC_ID_ERROR, "failed to remove plane instance from the registry" );

  delete plane;
}

} // extern "C"
