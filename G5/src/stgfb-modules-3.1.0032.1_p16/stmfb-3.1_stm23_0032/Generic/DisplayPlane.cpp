/***********************************************************************
 *
 * File: Generic/DisplayPlane.cpp
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include "IOS.h"
#include "IDebug.h"
#include "Output.h"
#include "DisplayDevice.h"
#include "DisplayPlane.h"


CDisplayPlane::CDisplayPlane(stm_plane_id_t planeID, ULONG ulNodeListSize)
{
  DENTRY();

  m_planeID    = planeID;
  m_pOutput    = 0;
  m_ulTimingID = 0;
  m_isEnabled  = false;
  m_ulStatus   = 0;
  m_user       = 0;
  m_lock       = 0;

  m_writerPos     = 0;
  m_readerPos     = 0;
  m_ulNodeEntries = ulNodeListSize;
  m_pNodeList     = 0;

  g_pIOS->ZeroMemory(&m_NodeListArea, sizeof(DMA_Area));

  g_pIOS->ZeroMemory(&m_currentNode,  sizeof(stm_plane_node));
  g_pIOS->ZeroMemory(&m_pendingNode,  sizeof(stm_plane_node));
  g_pIOS->ZeroMemory(&m_previousNode, sizeof(stm_plane_node));

  m_isActive = false;
  m_isPaused = false;
  m_keepHistoricBufferForDeinterlacing = false;

  m_fixedpointONE = 1; // n.0 i.e. integer, overridden by subclass
  m_ulMaxHSrcInc  = 1;
  m_ulMinHSrcInc  = 1;
  m_ulMaxVSrcInc  = 1;
  m_ulMinVSrcInc  = 1;
  m_ulMaxLineStep = 1;

  DEXIT();
}


CDisplayPlane::~CDisplayPlane(void)
{
  DENTRY();

  g_pIOS->FreeDMAArea(&m_NodeListArea);
  g_pIOS->DeleteResourceLock(m_lock);

  DEXIT();
}


void CDisplayPlane::SetScalingCapabilities(stm_plane_caps_t *caps) const
{
  DENTRY();

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
    caps->minVerticalRescale.denominator = m_ulMaxHSrcInc * m_ulMaxLineStep;
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
  if((m_ulMinHSrcInc > (ULONG)m_fixedpointONE)  ||
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

  if((m_ulMinVSrcInc > (ULONG)m_fixedpointONE)  ||
     (((m_fixedpointONE*m_fixedpointONE)/m_ulMinVSrcInc) & (m_fixedpointONE-1)) != 0)
  {
    caps->maxVerticalRescale.numerator   = m_fixedpointONE;
    caps->maxVerticalRescale.denominator = m_ulMinVSrcInc;
  }
  else
  {
    caps->maxVerticalRescale.numerator   = m_fixedpointONE/m_ulMinHSrcInc;
    caps->maxVerticalRescale.denominator = 1;
  }

  DEBUGF2(2,("%s: horizontal scale %lu/%lu to %lu/%lu\n",__PRETTY_FUNCTION__,
                                                         caps->minHorizontalRescale.numerator,
                                                         caps->minHorizontalRescale.denominator,
                                                         caps->maxHorizontalRescale.numerator,
                                                         caps->maxHorizontalRescale.denominator));
  DEBUGF2(2,("%s: vertical scale   %lu/%lu to %lu/%lu\n",__PRETTY_FUNCTION__,
                                                         caps->minVerticalRescale.numerator,
                                                         caps->minVerticalRescale.denominator,
                                                         caps->maxVerticalRescale.numerator,
                                                         caps->maxVerticalRescale.denominator));

  DEXIT();
}


bool CDisplayPlane::Create(void)
{
  DENTRY();

  m_lock = g_pIOS->CreateResourceLock();
  if(!m_lock)
  {
    DERROR("failed to allocate resource lock\n");
    return false;
  }

  DEBUGF2(2,("%s: node entries = %lu \n",__PRETTY_FUNCTION__,m_ulNodeEntries));

  g_pIOS->AllocateDMAArea(&m_NodeListArea, (m_ulNodeEntries * sizeof(stm_plane_node)), 0, SDAAF_NONE);
  g_pIOS->MemsetDMAArea(&m_NodeListArea, 0, 0, m_NodeListArea.ulDataSize);
  if(!m_NodeListArea.pMemory)
  {
    DERROR("failed to allocate node list\n");
    return false;
  }

  m_pNodeList = (stm_plane_node*)m_NodeListArea.pData;

  DEXIT();

  return true;
}


bool CDisplayPlane::ConnectToOutput(COutput* pOutput)
{
  DEBUGF2(2,("CDisplayPlane::ConnectToOutput 'planeID = %d pOutput = %p'\n",m_planeID,pOutput));
  ULONG updateStatus = 0;

  if(m_pOutput)
  {
    DEBUGF2(2,("CDisplayPlane::ConnectToOutput 'already connected to output = %p'\n",m_pOutput));
    return false;
  }

  if(!pOutput->CanShowPlane(m_planeID))
  {
    DEBUGF2(2,("CDisplayPlane::ConnectToOutput 'plane not displayable on output = %p'\n",m_pOutput));
    return false;
  }

  // Setting the output pointer will be sufficient to start the
  // process of displaying the plane.
  m_pOutput    = pOutput;
  m_ulTimingID = pOutput->GetTimingID();
  updateStatus = STM_PLANE_STATUS_CONNECTED;

  // If the plane has already been enabled then show it immediately on the
  // newly attached output.
  if(m_isEnabled)
  {
    m_pOutput->ShowPlane(m_planeID);
    updateStatus |= STM_PLANE_STATUS_VISIBLE;
  }

  g_pIOS->LockResource(m_lock);
  m_ulStatus |= updateStatus;
  g_pIOS->UnlockResource(m_lock);

  return true;
}


void CDisplayPlane::DisconnectFromOutput(COutput* pOutput)
{
  DEBUGF2(2,("CDisplayPlane::DisconnectFromOutput 'planeID = %d pOutput = %p'\n",m_planeID,pOutput));
  if(m_pOutput == pOutput)
  {
    m_pOutput->HidePlane(m_planeID);

    m_pOutput    = 0;
    m_ulTimingID = 0;

    g_pIOS->LockResource(m_lock);
    m_ulStatus &= ~(STM_PLANE_STATUS_CONNECTED|STM_PLANE_STATUS_VISIBLE);
    g_pIOS->UnlockResource(m_lock);
  }
}


bool CDisplayPlane::LockUse(void *user)
{
    /*
     * Allow multiple calls to lock with the same user handle
     */
    if(m_user == user)
        return true;

    if(!m_user)
    {
        DEBUGF2(2,("CDisplayPlane::LockUse user = %p using plane = %d\n",user, GetID()));
        m_user = user;

        g_pIOS->LockResource(m_lock);
        m_ulStatus |= STM_PLANE_STATUS_LOCKED;
        g_pIOS->UnlockResource(m_lock);

        return true;
    }

    /*
     * Another user has the plane locked, so fail
     */
    return false;
}


void CDisplayPlane::Unlock(void *user)
{
    if((user != 0) && (m_user != user))
      return;

    DEBUGF2(2,("CDisplayPlane::Unlock user = %p plane = %d\n",user, GetID()));

    Flush();
    m_user = 0;

    g_pIOS->LockResource(m_lock);
    m_ulStatus &= ~STM_PLANE_STATUS_LOCKED;
    g_pIOS->UnlockResource(m_lock);
}


bool CDisplayPlane::SetControl(stm_plane_ctrl_t, ULONG)
{
  // Default, return failure for an unsupported control
  return false;
}


bool CDisplayPlane::GetControl(stm_plane_ctrl_t, ULONG *) const
{
  // Default, return failure for an unsupported control
  return false;
}


bool CDisplayPlane::SetDepth(COutput *pOutput, int depth, bool activate)
{
  if(!pOutput || !pOutput->CanShowPlane(m_planeID))
    return false;

  if(!pOutput->SetPlaneDepth(m_planeID, depth, activate))
    return false;

  return true;
}


bool CDisplayPlane::GetDepth(COutput *pOutput, int *depth) const
{
  if(!pOutput || !pOutput->CanShowPlane(m_planeID))
    return false;

  if(!pOutput->GetPlaneDepth(m_planeID, depth))
    return false;

  return true;
}


bool CDisplayPlane::GetNextNodeFromDisplayList(stm_plane_node &frame)
{
    /*
     * This assumes that the only caller is the interrupt/bottom-half
     * handler or when the plane is flushed and that it cannot be re-entered.
     * There is no need to lock against the writer. Flushing the queue is
     * done with interrupt processing turned off.
     */
    if(!m_pNodeList[m_readerPos].isValid)
        return false;

    m_ulStatus &= ~STM_PLANE_STATUS_QUEUE_FULL;

    frame = m_pNodeList[m_readerPos];
    m_pNodeList[m_readerPos].isValid = false;

    m_readerPos++;
    if(m_readerPos == m_ulNodeEntries)
        m_readerPos = 0;

    g_pIOS->CheckDMAAreaGuards(&m_NodeListArea);

    return true;
}


bool CDisplayPlane::PeekNextNodeFromDisplayList(const TIME64 &vsyncTime, stm_plane_node &frame)
{
    /*
     * This looks to see if we have a display node that is ready for display,
     * i.e. one exists at all and its presentation time makes it valid to
     * display it.
     */
    if(!m_pNodeList[m_readerPos].isValid)
        return false;

    /*
     * Only return the next buffer if it is time to present it. This is
     * complicated by the fact that we have to program the hardware one vsync
     * ahead of the presentation, so we have to look ahead. Additionally we
     * allow some slack to allow for variable interrupt latency which means we
     * cannot time vsyncs precisely, currently set to half the vertical refresh
     * duration.
     */
    if((m_pNodeList[m_readerPos].info.presentationTime != 0LL) &&
       (m_pNodeList[m_readerPos].info.presentationTime > (vsyncTime+(m_pOutput->GetFieldOrFrameDuration()*3/2))))
    {
        return false;
    }

    frame = m_pNodeList[m_readerPos];

    return true;
}


bool CDisplayPlane::PopNextNodeFromDisplayList(void)
{
    if(!m_pNodeList[m_readerPos].isValid)
        return false;

    m_ulStatus &= ~STM_PLANE_STATUS_QUEUE_FULL;

    m_pNodeList[m_readerPos].isValid = false;

    m_readerPos++;
    if(m_readerPos == m_ulNodeEntries)
        m_readerPos = 0;

    g_pIOS->CheckDMAAreaGuards(&m_NodeListArea);

    return true;
}


bool CDisplayPlane::AddToDisplayList(const stm_plane_node &newFrame)
{
    /*
     * This assumes that the only caller (QueueBuffer) has been locked
     * against re-enterancy. There is no need to lock this against the
     * reader, that is the point of this scheme.
     */

    if(m_pNodeList[m_writerPos].isValid)
    {
        /*
         * TODO, implement blocking stratergy
         */
        DEBUGF2(2,("CDisplayPlane::AddToDisplayList 'List Full'"));
        return false;
    }

    m_pNodeList[m_writerPos] = newFrame;
    m_pNodeList[m_writerPos].isValid = true;

    m_writerPos++;
    if(m_writerPos == m_ulNodeEntries)
        m_writerPos = 0;

    /*
     * We have added a node to be displayed so we must be active and we
     * automatically undo any paused state.
     */
    g_pIOS->LockResource(m_lock);
    m_isPaused = false;
    m_isActive = true;
    m_ulStatus &= ~STM_PLANE_STATUS_PAUSED;
    m_ulStatus |= STM_PLANE_STATUS_ACTIVE;
    if(m_pNodeList[m_writerPos].isValid)
        m_ulStatus |= STM_PLANE_STATUS_QUEUE_FULL;

    g_pIOS->UnlockResource(m_lock);

    g_pIOS->CheckDMAAreaGuards(&m_NodeListArea);

    return true;
}


void CDisplayPlane::UpdateCurrentNode(const TIME64 &vsyncTime)
{

    if(m_currentNode.isValid && m_currentNode.info.nfields > 0)
    {
        m_currentNode.info.nfields--;
        if(m_currentNode.info.nfields != 0)
            return;
    }

    if(m_currentNode.isValid)
    {
        if(!m_pendingNode.isValid)
        {
            // Nothing is going to be displayed next, so deal with either
            // keeping the current node on display or shuting down the plane
            if(m_isPaused || (m_currentNode.info.ulFlags & STM_PLANE_PRESENTATION_PERSISTENT))
            {
                // It is up to the specific updateHW implementation to make sure
                // the node keeps on displaying.
                return;
            }

            DEBUGF2(2,("CDisplayPlane::UpdateCurrentNode plane = %d disabling plane, no more nodes\n",GetID()));

            this->DisableHW();
            // Thats it, we have shut down so we are no longer active and should
            // not get any more updateHW calls until another node is queued.
            m_isActive = false;
            m_ulStatus &= ~STM_PLANE_STATUS_ACTIVE;
        }

        /*
         * We can now say the current node has completed and is no
         * longer on display, but it may still be referenced.
         */
        if(m_previousNode.isValid)
        {
            if(m_previousNode.info.pCompletedCallback)
            {
                m_previousNode.info.stats.vsyncTime = vsyncTime;
                m_previousNode.info.stats.ulStatus |= m_ulStatus;
                m_previousNode.info.pCompletedCallback(m_previousNode.info.pUserData,
                                                       &m_previousNode.info.stats);
            }

            DEBUGF2(3,("%s plane = %d releasing previous node\n",
                       __PRETTY_FUNCTION__, GetID()));

            this->ReleaseNode(m_previousNode);
            m_previousNode.isValid = false;
        }

        if(m_isActive && m_keepHistoricBufferForDeinterlacing)
        {
            m_previousNode = m_currentNode;
        }
        else
        {
            /*
             * Either in the non-deinterlacing case or the case of the last
             * node in a stream and the HW has just been shutdown, just release
             * the current node instead of moving it to a "previous" state.
             */
            if(m_currentNode.info.pCompletedCallback)
            {
                m_currentNode.info.stats.vsyncTime = vsyncTime;
                m_currentNode.info.stats.ulStatus |= m_ulStatus;
                m_currentNode.info.pCompletedCallback(m_currentNode.info.pUserData,
                                                      &m_currentNode.info.stats);
            }

            this->ReleaseNode(m_currentNode);
            m_currentNode.isValid = false;
        }
    }

    if(m_pendingNode.isValid)
    {
        m_currentNode = m_pendingNode;

        if(m_isPaused)
        {
          /*
           * If the plane has just been paused, then set the display fields to
           * zero. When the plane gets resumed the next queued buffer will
           * display on the next valid field. This prevents the buffer sticking
           * on the display for several frames when in very slow motion.
           */
          m_currentNode.info.nfields = 0;
        }

        m_pendingNode.isValid = false;

        // We can now say that this node is on the display
        if(m_currentNode.info.pDisplayCallback)
        {
            m_currentNode.info.stats.ulStatus |= STM_PLANE_STATUS_BUF_DISPLAYED;
            m_currentNode.info.pDisplayCallback(m_currentNode.info.pUserData, vsyncTime);
        }
    }
}


void CDisplayPlane::SetPendingNode(stm_plane_node &pendingNode)
{
    /*
     * Note: this can be called from an interrupt context, hence it must
     *       not block.
     */
    DEBUGF2(3,("CDisplayPlane::SetPendingNode plane = %d node = %p\n",GetID(),&pendingNode));
    m_pendingNode = pendingNode;
    m_pendingNode.isValid = true;
    m_pendingNode.info.stats.ulStatus = 0;
}


void CDisplayPlane::Pause(bool bFlushQueue)
{
    stm_plane_node frame;

    /*
     * This allows us to pause video immediately rather than waiting
     * for the queue to complete (which may take a long time if doing slow
     * motion). You have the choice of flushing any queued buffers.
     */

    g_pIOS->LockResource(m_lock);
    if(!m_isActive)
    {
      g_pIOS->UnlockResource(m_lock);
      return;
    }

    m_isPaused = true;
    m_ulStatus |= STM_PLANE_STATUS_PAUSED;
    g_pIOS->UnlockResource(m_lock);

    if(!bFlushQueue)
        return;

    this->ResetQueueBufferState();

    while(GetNextNodeFromDisplayList(frame))
    {
        if(frame.info.pCompletedCallback)
        {
            frame.info.stats.ulStatus = m_ulStatus;
            frame.info.pCompletedCallback(frame.info.pUserData, 0);
        }

        ReleaseNode(frame);
    }

    /*
     * Nobble the field count on current and pending node so we can restart
     * the stream ASAP when new buffers are queued. This allows the fastest
     * possible transition from slow motion to normal playback, by issuing
     * a pause+flush and then re-queueing the flushed buffers at normal speed.
     */
    g_pIOS->LockResource(m_lock);
    m_currentNode.info.nfields = 0;
    m_pendingNode.info.nfields = 0;
    g_pIOS->UnlockResource(m_lock);
}


void CDisplayPlane::Resume(void)
{
    g_pIOS->LockResource(m_lock);
    if(!m_isActive || !m_isPaused)
    {
      g_pIOS->UnlockResource(m_lock);
      return;
    }

    /*
     * We only resume a paused plane if there are new buffers
     * to display in the queue.
     */
    if(m_pNodeList[m_readerPos].isValid)
    {
      m_isPaused = false;
      m_ulStatus &= ~STM_PLANE_STATUS_PAUSED;
    }
    g_pIOS->UnlockResource(m_lock);

    return;
}


void CDisplayPlane::Flush(void)
{
    stm_plane_node frame;

    /*
     * First ensure that no more interrupt processing is done for this plane.
     * This means we can clean up without disabling interrupts for a long
     * period and potentially breaking the update of another plane. We rely
     * on a higher level driver mutex to ensure that another user task cannot
     * start to queue on the plane again until we are finished.
     */
    g_pIOS->LockResource(m_lock);
    m_isActive = false;
    m_isPaused = false;
    m_ulStatus &= ~(STM_PLANE_STATUS_ACTIVE | STM_PLANE_STATUS_PAUSED);
    g_pIOS->UnlockResource(m_lock);

    // Turn the plane off, remove it from the display
    this->DisableHW();

    this->ResetQueueBufferState();

    /*
     * For each previous, current, pending and queued frames clear them from the plane
     * and call their "completed" callback in the order they were originally
     * queued. This allows the higher levels to clean up their buffer usage
     * safely.
     */
    if(m_previousNode.isValid)
    {
        if(m_previousNode.info.pCompletedCallback)
        {
            m_previousNode.info.stats.ulStatus |= m_ulStatus;
            m_previousNode.info.pCompletedCallback(m_previousNode.info.pUserData, 0);
        }

        ReleaseNode(m_previousNode);
        m_previousNode.isValid = false;
    }

    if(m_currentNode.isValid)
    {
        if(m_currentNode.info.pCompletedCallback)
        {
            m_currentNode.info.stats.ulStatus |= m_ulStatus;
            m_currentNode.info.pCompletedCallback(m_currentNode.info.pUserData, 0);
        }

        ReleaseNode(m_currentNode);
        m_currentNode.isValid = false;
    }

    if(m_pendingNode.isValid)
    {
        if(m_pendingNode.info.pCompletedCallback)
        {
            /*
             * Note that this never got onto the display, so we do not preserve
             * anything in the stats status field.
             */
            m_pendingNode.info.stats.ulStatus = m_ulStatus;
            m_pendingNode.info.pCompletedCallback(m_pendingNode.info.pUserData, 0);
        }

        ReleaseNode(m_pendingNode);
        m_pendingNode.isValid = false;
    }

    while(GetNextNodeFromDisplayList(frame))
    {
        if(frame.info.pCompletedCallback)
        {
            frame.info.stats.ulStatus = m_ulStatus;
            frame.info.pCompletedCallback(frame.info.pUserData, 0);
        }

        ReleaseNode(frame);
    }
}


void CDisplayPlane::EnableHW(void)
{
    if(!m_isEnabled && m_pOutput)
    {
        m_isEnabled = true;
        if(!CDisplayPlane::Show())
        {
          m_isEnabled = false;
        }
        else
        {
          DEBUGF2(2,("%s (%p) 'enabled plane id %d'\n",
                   __PRETTY_FUNCTION__, this, GetID ()));
        }
    }
}


void CDisplayPlane::DisableHW(void)
{
    CDisplayPlane::Hide();

    if(m_isEnabled)
    {
        m_isEnabled = false;
        DEBUGF2(2,("%s (%p) 'disabled plane id %d'\n",
                   __PRETTY_FUNCTION__, this, GetID ()));
    }
}


bool CDisplayPlane::Hide(void)
{
  if(m_pOutput && m_isEnabled)
  {
    m_pOutput->HidePlane(GetID());
    g_pIOS->LockResource(m_lock);
    m_ulStatus &= ~STM_PLANE_STATUS_VISIBLE;
    g_pIOS->UnlockResource(m_lock);
    return true;
  }

  return false;
}


bool CDisplayPlane::Show(void)
{
  if(m_pOutput && m_isEnabled)
  {
    if(!m_pOutput->ShowPlane(GetID()))
    {
      DEBUGF2(1,(FERROR "Unable to enable hardware for plane %d\n",__PRETTY_FUNCTION__,GetID()));
      return false;
    }
    g_pIOS->LockResource(m_lock);
    m_ulStatus |= STM_PLANE_STATUS_VISIBLE;
    g_pIOS->UnlockResource(m_lock);
    return true;
  }

  return false;
}


void CDisplayPlane::ReleaseNode(stm_plane_node &node)
{
  DEBUGF2(3,("%s 'releasing %p'\n",__PRETTY_FUNCTION__,node.dma_area.pMemory));

  g_pIOS->FreeDMAArea(&node.dma_area);
}


void CDisplayPlane::ResetQueueBufferState(void) {}

////////////////////////////////////////////////////////////////////////////////
//

bool CDisplayPlane::GetRGBYCbCrKey(UCHAR&  ucRCr,
                                   UCHAR&  ucGY,
                                   UCHAR&  ucBCb,
                                   ULONG   ulPixel,
                                   SURF_FMT colorFmt,
				   bool     pixelIsRGB)
{
  bool  bResult = true;

  ULONG R = (ulPixel & 0xFF0000) >> 16;
  ULONG G = (ulPixel & 0xFF00) >> 8;
  ULONG B = (ulPixel & 0xFF);

  switch(colorFmt)
  {
    case SURF_YCBCR422MB:
    case SURF_YCBCR422R:
    case SURF_YCBCR420MB:
    case SURF_YUYV:
    case SURF_YUV420:
    case SURF_YVU420:
    case SURF_YUV422P:
      if (pixelIsRGB)
      {
	ucRCr = (UCHAR)(128 + ((131 * R) / 256) - (((110 * G) / 256) + ((21 * B) / 256)));
	ucGY  = (UCHAR)(((77 * R) /256) + ((150 * G) / 256) + ((29 * B) / 256));
	ucBCb = (UCHAR)(128 + ((131 * B) / 256) - (((44 * R) / 256) + ((87 * G) / 256)));
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
	ucRCr = (UCHAR)R;
	ucGY  = (UCHAR)G;
	ucBCb = (UCHAR)B;
      }
      else
      {
	DEBUGF2(2,("Error: pixel format can only be RGB for surface %lu.\n",(ULONG)colorFmt));
	bResult = false;
      }
    break;

    default:
      DEBUGF2(2,("Error: unsupported surface format %lu.\n",(ULONG)colorFmt));
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

////////////////////////////////////////////////////////////////////////////////
// C Display plane interface
//

extern "C" {

static int connect_to_output(stm_display_plane_t *plane, struct stm_display_output_s *output)
{
  CDisplayPlane* pDP = (CDisplayPlane*)(plane->handle);
  COutput * pOut = (COutput*)(output->handle);
  int res;

  DEBUGF2(2,("%s: plane = %p output = %p\n",__FUNCTION__,plane,output));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  res = pDP->ConnectToOutput(pOut)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return res;
}


static int disconnect_from_output(stm_display_plane_t *plane, struct stm_display_output_s *output)
{
  CDisplayPlane* pDP = (CDisplayPlane*)(plane->handle);
  COutput * pOut = (COutput*)(output->handle);

  DEBUGF2(2,("%s: plane = %p output = %p\n",__FUNCTION__,plane,output));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  pDP->DisconnectFromOutput(pOut);

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int lock_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP = (CDisplayPlane*)(plane->handle);
  int res;

  DEBUGF2(3,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  res = pDP->LockUse(plane)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return res;
}


static int unlock_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP = (CDisplayPlane*)(plane->handle);

  DEBUGF2(3,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  pDP->Unlock(plane);

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int queue_buffer(stm_display_plane_t *plane, stm_display_buffer_t *pBuffer)
{
  CDisplayPlane* pDP = (CDisplayPlane*)(plane->handle);
  int res;

  DEBUGF2(3,("%s: plane: %p, buffer: %p\n", __FUNCTION__, plane, pBuffer));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  res = pDP->QueueBuffer(pBuffer, plane)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return res;
}


static int pause_plane(stm_display_plane_t *plane, int bFlushQueue)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  pDP->Pause(bFlushQueue != 0);

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int resume_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  pDP->Resume();

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int flush_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  pDP->Flush();

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int show_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  int ret;

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->Show()?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static int hide_plane(stm_display_plane_t *plane)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  int ret;

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->Hide()?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static int get_plane_caps(stm_display_plane_t *plane, stm_plane_caps_t *caps)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);

  DEBUGF2(2,("%s: plane = %p caps = %p\n",__FUNCTION__,plane,caps));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  *caps = pDP->GetCapabilities();

  g_pIOS->UpSemaphore(plane->lock);

  return 0;
}


static int get_image_formats(stm_display_plane_t *plane, const SURF_FMT** formats)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  int n;

  DEBUGF2(2,("%s: plane = %p formats = %p\n",__FUNCTION__,plane,formats));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  n = pDP->GetFormats(formats);

  g_pIOS->UpSemaphore(plane->lock);

  return n;
}


static int set_plane_control(stm_display_plane_t *plane, stm_plane_ctrl_t control, ULONG value)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  int ret;

  DEBUGF2(2,("%s: plane: %p control/value: %d/0x%.8lx\n",__FUNCTION__,plane,control,value));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->SetControl(control,value)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static int get_plane_control(stm_display_plane_t *plane, stm_plane_ctrl_t control, ULONG *value)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  int ret;

  DEBUGF2(2,("%s: plane = %p control = %d value = %p\n",__FUNCTION__,plane,(int)control,value));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->GetControl(control,value)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static int set_depth(stm_display_plane_t *plane, struct stm_display_output_s *output, int depth, int activate)
{
  CDisplayPlane* pDP  = (CDisplayPlane*)(plane->handle);
  COutput *pOut = (COutput*)(output->handle);
  int ret;

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->SetDepth(pOut,depth,(activate != 0))?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static int get_depth(stm_display_plane_t *plane, struct stm_display_output_s *output, int *depth)
{
  CDisplayPlane *pDP = (CDisplayPlane*)(plane->handle);
  COutput *pOut = (COutput*)(output->handle);
  int ret;

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  ret = pDP->GetDepth(pOut,depth)?0:-1;

  g_pIOS->UpSemaphore(plane->lock);

  return ret;
}


static void get_status(stm_display_plane_t *plane, ULONG *status)
{
  CDisplayPlane *pDP = (CDisplayPlane*)(plane->handle);

  *status = pDP->GetStatus();
}


static int release_plane(stm_display_plane_t *plane)
{
  CDisplayPlane*  pDP  = (CDisplayPlane*)(plane->handle);

  DEBUGF2(2,("%s: plane = %p\n",__FUNCTION__,plane));

  if(g_pIOS->DownSemaphore(plane->lock) != 0)
    return -1;

  /*
   * If this instance was the registered user of the underlying hardware
   * plane, then we need to clear that state and flush any queued content.
   */
  pDP->Unlock(plane);

  g_pIOS->UpSemaphore(plane->lock);

  delete plane;

  return 0;
}


stm_display_plane_ops_t stm_display_plane_ops = {
  ConnectToOutput      : connect_to_output,
  DisconnectFromOutput : disconnect_from_output,
  Lock                 : lock_plane,
  Unlock               : unlock_plane,
  QueueBuffer          : queue_buffer,
  Pause                : pause_plane,
  Resume               : resume_plane,
  Flush                : flush_plane,
  Hide                 : hide_plane,
  Show                 : show_plane,
  GetCapabilities      : get_plane_caps,
  GetImageFormats      : get_image_formats,
  SetControl           : set_plane_control,
  GetControl           : get_plane_control,
  SetDepth             : set_depth,
  GetDepth             : get_depth,
  GetStatus            : get_status,
  Release              : release_plane
};

} // extern "C"
