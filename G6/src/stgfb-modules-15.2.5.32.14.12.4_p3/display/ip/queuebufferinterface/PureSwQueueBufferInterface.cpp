/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2011-2014 STMicroelectronics - All Rights Reserved
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

#include "PureSwQueueBufferInterface.h"


///////////////////////////////////////////////////////////////////////////////
// Base class for G2 and G3 compositor queue buffer source interface
//

CPureSwQueueBufferInterface::CPureSwQueueBufferInterface ( uint32_t interfaceID, CDisplaySource * pSource, bool isConnectionToVideoPlaneAllowed )
    : CQueueBufferInterface(interfaceID, pSource)
{
  TRC( TRC_ID_MAIN_INFO, "Create PureSwQueueBufferInterface with Id = %d", interfaceID );

  m_isConnectionToVideoPlaneAllowed = isConnectionToVideoPlaneAllowed;

  m_user                   = 0;
  m_displayNextPictASAP    = false;
  m_IncomingPictureCounter = 0;
  m_lastPresentationTime   = 0;
  m_ulTimingID             = 0;
  m_LatencyParams          = 0;
  m_LatencyPlaneIds        = 0;


  ResetTriplet(&m_picturesPreparedForNextVSync);
  ResetTriplet(&m_picturesUsedByHw);

  vibe_os_zero_memory( &m_outputInfo, sizeof( m_outputInfo ));
  vibe_os_zero_memory( &m_currentPictureCharact, sizeof( m_currentPictureCharact ));

  TRC( TRC_ID_MAIN_INFO, "Created Queue = %p", this );
}


CPureSwQueueBufferInterface::~CPureSwQueueBufferInterface()
{
  TRC( TRC_ID_MAIN_INFO, "QueueBuffer Interface %p Destroyed", this );
  vibe_os_free_memory(m_LatencyParams);
  vibe_os_free_memory(m_LatencyPlaneIds);
}


void CPureSwQueueBufferInterface::UpdateOutputInfo(CDisplayPlane *pPlane)
{
    if( pPlane != 0 )
    {
        m_ulTimingID = pPlane->GetTimingID();
        m_outputInfo = pPlane->GetOutputInfo();
        TRC( TRC_ID_MAIN_INFO, "Source %d: New TimingID=%u", m_interfaceID, m_ulTimingID);
    }
}

bool CPureSwQueueBufferInterface::Create(void)
{
  uint32_t max_num_planes = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if (!CQueueBufferInterface::Create())
    return false;

  max_num_planes = GetParentSource()->GetMaxNumConnectedPlanes();

  m_LatencyParams = (stm_display_latency_params_t*)vibe_os_allocate_memory(max_num_planes * sizeof(stm_display_latency_params_t));
  if(!m_LatencyParams)
  {
    TRC( TRC_ID_ERROR, "failed to allocate m_LatencyParams" );
    return false;
  }
  vibe_os_zero_memory(m_LatencyParams, max_num_planes * sizeof(stm_display_latency_params_t));

  m_LatencyPlaneIds = (uint32_t*)vibe_os_allocate_memory(max_num_planes * sizeof(uint32_t));
  if(!m_LatencyPlaneIds)
  {
    TRC( TRC_ID_ERROR, "failed to allocate m_LatencyPlaneIds" );
    return false;
  }
  vibe_os_zero_memory(m_LatencyPlaneIds, max_num_planes * sizeof(uint32_t));

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

bool CPureSwQueueBufferInterface::QueueBuffer(const stm_display_buffer_t* const pBuffer,
                                         const void                * const user)
{
    CDisplayNode           *pDisplayNode;
    bool                    result = false;

    /* null pointer which shouldn't happen */
    if(!pBuffer)
    {
        TRC( TRC_ID_ERROR, "Invalid pBuffer!" );
        return false;
    }

    pDisplayNode  = new CDisplayNode;
    if(!pDisplayNode)
    {
        TRC( TRC_ID_ERROR, "Failed to allocate a CDisplayNode!" );
        return false;
    }

    if(m_user != user)
    {
        // Buffer received from someone that is not (or no more) the active user of this plane
        // This can happen during a swap of viewport

        // This buffer is put in a special queue called "m_picturesRefusedQueue" that will be flushed at next VSync
        pDisplayNode->m_bufferDesc = *pBuffer;
        pDisplayNode->m_isPictureValid = false;
        pDisplayNode->m_pictureId = 0;
        pDisplayNode->m_user = (void *) user;

        if ( m_pDisplayDevice->VSyncLock() != 0)
        {
            delete pDisplayNode;
            return false;
        }
        result = m_picturesRefusedQueue.QueueDisplayNode(pDisplayNode);
        m_pDisplayDevice->VSyncUnlock();

        TRC(TRC_ID_PICT_REFUSED, "Source %d: Invalid user (0x%p). Buffer 0x%p added to m_picturesRefusedQueue", m_interfaceID, user, pDisplayNode);
        return true;
    }

    GrantFrameBufferAccess(pBuffer);

    // Fill the node
    pDisplayNode->m_bufferDesc = *pBuffer;
    pDisplayNode->m_isPictureValid = true;
    pDisplayNode->m_pictureId  = ++m_IncomingPictureCounter;
    pDisplayNode->m_user = (void *) user;
    if(pDisplayNode->m_bufferDesc.info.nfields == 0)
    {
        pDisplayNode->m_isSkippedForFRC = true;
    }
    FillNodeInfo(pDisplayNode);

    if ( (pDisplayNode->m_bufferDesc.src.pixel_aspect_ratio.numerator   == 0) ||
         (pDisplayNode->m_bufferDesc.src.pixel_aspect_ratio.denominator == 0) )
    {
        // This can happen when using v4l2gam. Use default values in that case
        pDisplayNode->m_bufferDesc.src.pixel_aspect_ratio.numerator   = 1;
        pDisplayNode->m_bufferDesc.src.pixel_aspect_ratio.denominator = 1;
    }

    // Check if the source characteristic have changed
    if(AreSrcCharacteristicsChanged(pDisplayNode))
    {
        pDisplayNode->m_bufferDesc.src.flags |= STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY;
    }

    if(m_displayNextPictASAP)
    {
        pDisplayNode->m_bufferDesc.info.presentation_time = 0;
        m_displayNextPictASAP = false;
    }

    // And queue it
    if ( m_pDisplayDevice->VSyncLock() != 0)
    {
        goto error_queue_buffer;
    }
    result = m_displayQueue.QueueDisplayNode(pDisplayNode);
    m_pDisplayDevice->VSyncUnlock();

    if(!result)
    {
        goto error_queue_buffer;
    }

    // The picture has been successfully queued
    m_Statistics.PicQueued++;
    m_Statistics.PicOwned++;
    TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: %d%c nfields=%d 0x%p (Owned:%d) presentation_time=%lld user_data=0x%p",
            m_interfaceID,
            pDisplayNode->m_pictureId,
            pDisplayNode->m_srcPictureTypeChar,
            pDisplayNode->m_bufferDesc.info.nfields,
            pDisplayNode,
            m_Statistics.PicOwned,
            pDisplayNode->m_bufferDesc.info.presentation_time,
            pDisplayNode->m_bufferDesc.info.puser_data);

    // With Interlaced sources, it is usual that only the first field contains time stamps. The other one has a null "presentation_time" so it should be displayed ASAP
    if(pDisplayNode->m_bufferDesc.info.presentation_time != 0)
    {
        if(m_lastPresentationTime != 0)
        {
            TRC( TRC_ID_PICT_QUEUE_RELEASE, "Delta with last time-stamped picture : %lld us", pDisplayNode->m_bufferDesc.info.presentation_time - m_lastPresentationTime);
        }
        m_lastPresentationTime = pDisplayNode->m_bufferDesc.info.presentation_time;
    }

    if(IS_TRC_ID_ENABLED(TRC_ID_BUFFER_DESCRIPTOR))
    {
        PrintBufferDescriptor(pDisplayNode);
    }

    return true;

error_queue_buffer:
    TRC( TRC_ID_ERROR, "failed!" );

    RevokeFrameBufferAccess(pBuffer);

    if (pDisplayNode)
    {
        delete pDisplayNode;
    }

    return result;
}


bool CPureSwQueueBufferInterface::Flush(bool bFlushCurrentNode,
                                     const void * const user,
                                     bool bCheckUser)
{
    CDisplayNode   *pTailNode;                 // The first node is going to be released in display Queue
    int             nodeReleasedNb = 0;
    CDisplayNode   *pMostRecentNodeUsedByHw;
    int             result;

    TRCIN( TRC_ID_MAIN_INFO, "" );

    if(bCheckUser && (m_user != user))
    {
        // Flush requested by a user that is not (or no more) the active user of this plane.
        // Flush all the nodes belonging to this user.
        result = FlushAllNodesFromUser(bFlushCurrentNode, user);
        TRCOUT( TRC_ID_MAIN_INFO, "" );
        return result;
    }

    if (bFlushCurrentNode)
    {
        TRC( TRC_ID_MAIN_INFO, "Source %d: Flush ALL nodes for user 0x%p", m_interfaceID, user );
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Source %d: Flush nodes not on display for user 0x%p", m_interfaceID, user );
    }

    ///////////////////////////////////////    Critical section      ///////////////////////////////////////////

    // Take the lock to ensure that OutputVSyncThreadedIrqUpdateHW() cannot be executed while we update the Queue
    if ( m_pDisplayDevice->VSyncLock() != 0)
        return false;

    FlushPicturesRefusedQueue();

    if (bFlushCurrentNode)
    {
        // Flush ALL nodes

        // Get the first node of the queue
        pTailNode = (CDisplayNode*)m_displayQueue.GetFirstNode();

        if(pTailNode)
        {
            // Cut the queue before the first node (so the queue will become empty)
            m_displayQueue.CutBeforeNode(pTailNode);
        }

        ResetTriplet(&m_picturesPreparedForNextVSync);
        ResetTriplet(&m_picturesUsedByHw);

        m_IncomingPictureCounter = 0;
    }
    else
    {
        /* Flush the nodes NOT ON DISPLAY (aka not used by the display HW)

           Example of pictures used at one time:

                                               10T -> 11B -> 12T -> 13B -> (14T) -> 15B -> 16T -> 17B
           Pictures used by HW:                               P      C       N
           Pictures prepared for next VSync:                                 P       C      N

           We're going to cut right after the most recent picture used by the HW (= Right after the Picture 16T in the above example)

           NB: m_picturesPreparedForNextVSync.pNextNode is null on planes without deinterlacer. In that case, the most recent field is m_picturesPreparedForNextVSync.pCurNode

           Legend:
             P  = Previous DEI field
             C  = Current DEI field
             N  = Next DEI field
            ( ) = Picture skipped by the Frame Rate Conversion. It can be used as reference by the Deinterlacer but it should not be displayed.
        */
        if(m_picturesPreparedForNextVSync.pNextNode)
        {
            pMostRecentNodeUsedByHw = m_picturesPreparedForNextVSync.pNextNode;
        }
        else
        {
            pMostRecentNodeUsedByHw = m_picturesPreparedForNextVSync.pCurNode;
        }

        if(pMostRecentNodeUsedByHw)
        {
            pTailNode = (CDisplayNode*) pMostRecentNodeUsedByHw->GetNextNode();
        }
        else
        {
            // There is no picture on display so we can cut from the very first node of the queue
            pTailNode = (CDisplayNode*)m_displayQueue.GetFirstNode();

            ResetTriplet(&m_picturesPreparedForNextVSync);
            ResetTriplet(&m_picturesUsedByHw);

            m_IncomingPictureCounter = 0;

            bFlushCurrentNode = true; // Forced to true to trigger a call to DisableHW()
        }

        if(pTailNode)
        {
            // Cut the queue before this node
            m_displayQueue.CutBeforeNode(pTailNode);
        }
    }

    m_pDisplayDevice->VSyncUnlock();
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////

    if ((bFlushCurrentNode) && (m_user == user))
    {
        if ( m_pDisplayDevice->VSyncLock() != 0)
            return false;

        // We have flushed all the nodes of the current user so there is nothing to display and the plane can be disabled
        NothingToDisplay();
        m_lastPresentationTime = 0;

        m_pDisplayDevice->VSyncUnlock();
    }

    // It is now safe to release all the nodes in "pTailNode"
    while(pTailNode != 0)
    {
        CDisplayNode *pReleasedNode = pTailNode;

        // Get the next node BEFORE releasing the current node (otherwise we lose the reference)
        pTailNode = (CDisplayNode*)pTailNode->GetNextNode();

        ReleaseDisplayNode(pReleasedNode, 0);
        nodeReleasedNb++;
    }

    TRC( TRC_ID_MAIN_INFO, "Source %d: %d nodes released", m_interfaceID, nodeReleasedNb );

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}

int CPureSwQueueBufferInterface::GetBufferFormats(const stm_pixel_format_t** formats) const
{
  int             format   = 0;
  uint32_t        num_planes_Ids = 0;
  uint32_t        PlanesID[/*CINTERFACE_MAX_PLANES_PER_SOURCE*/1];
  CDisplaySource       * pDS  = GetParentSource();
  const CDisplayDevice * pDev = pDS->GetParentDevice();

  // Get the connected planes ids
  num_planes_Ids = pDS->GetConnectedPlaneID(PlanesID, N_ELEMENTS(PlanesID));

  // Call Plane's QueueBuffer method
  for(uint32_t i=0; (i<num_planes_Ids) ; i++)
  {
    CDisplayPlane* pDP = pDev->GetPlane(PlanesID[i]);
    if (pDP)
    {
      TRC( TRC_ID_MAIN_INFO, "GetBufferFormats forwarded to Plane %s", pDP->GetName() );
      format = pDP->GetFormats(formats);
    }
  }

  return format;
}

bool CPureSwQueueBufferInterface::IsConnectionPossible(CDisplayPlane *pPlane) const
{
  bool isAllowed = false;

  if (GetParentSource()->GetNumConnectedPlanes() == 0)
  {
    // This source is connected to no plane, so this plane can be connected.
      isAllowed = true;
    }
  else
  {
    // This source is already connected to another plane, so only same TimingID planes are allowed.
    // Warning: The sourceTimingId can be null if his plane is not yet connected to an output
    //          The newPlaneTimingId can be null if it is not yet connected to an ouput
    // So we prevent both cases, else additionnal check should be done in stm_display_plane_connect_to_output()
    uint32_t sourceTimingId = GetParentSource()->GetTimingID();
    uint32_t newPlaneTimingId  = pPlane->GetTimingID();
    if ( (sourceTimingId != 0) &&
         (sourceTimingId == newPlaneTimingId)
       )
    {
      isAllowed = true;
    }
  }

  // Last check for video plane, this is perhaps useless all video planes can be connected to source
  if( isAllowed )
  {
    // Check if video plane can be connect to the source
    if (pPlane->isVideoPlane())
    {
      isAllowed = m_isConnectionToVideoPlaneAllowed;
    }
  }

  return isAllowed;
}

bool CPureSwQueueBufferInterface::LockUse(void *user, bool force)
{
  // Allow multiple calls to lock with the same user handle
    if(m_user == user)
        return true;

    if(!m_user)
    {
        TRC(TRC_ID_MAIN_INFO, "A new user 0x%p is locking Source %d", user, m_interfaceID );
        m_user = user;
        return true;
    }
    else
    {
        if(force)
        {
            TRC(TRC_ID_MAIN_INFO, "New user 0x%p is forcing the lock of Source %d (previous user was 0x%p)", user, m_interfaceID, m_user );
            m_user = user;
            // A Swap has happened. StreamingEngine is going to call the Flush function to get back all the buffers from the old stream.
            // The first picture of the new stream should be displayed ASAP to avoid a blue screen during the transition.
            m_displayNextPictASAP = true;
            return true;
        }
        else
        {
            // Another user has the queue locked, so fail
            return false;
        }
    }
}

void CPureSwQueueBufferInterface::Unlock(void *user)
{
    if((user != 0) && (m_user != user))
    {
        // This can happen if a Lock as been called in "force" mode (for Swap use case).
        // A Playstream X has overriden the lock from a Playstream Y. When Playstream Y will try to unlock, CDisplayPlane will
        // detect that it is no more the active user of this plane (= the one owning the lock).
        // In that case, we ignore the unlock request but we don't return an error.
        TRC(TRC_ID_MAIN_INFO, "Source %d is still locked by 0x%p", m_interfaceID, user);
        return;
    }

    TRC(TRC_ID_MAIN_INFO, "Source %d: Unlock user = 0x%p", m_interfaceID, user);
    m_user = 0;
    return;
}

/*
 * Generic handling of OutputVSync in threaded IRQ.
 * This may be overloaded in CDisplayPlane daughter classes
 * depending if OutputVsync should be handled in IRQ.
 */
void CPureSwQueueBufferInterface::OutputVSyncThreadedIrqUpdateHW(bool isDisplayInterlaced, bool isTopFieldOnDisplay, const stm_time64_t &vsyncTime)
{
    bool                       isPictureRepeated;

    TRC(TRC_ID_VSYNC, "VSync %c on Source %d at %llu",
            isDisplayInterlaced ? (isTopFieldOnDisplay ? 'T' : 'B') : ' ',
            m_interfaceID,
            vsyncTime);

    /* Process the CRC computed during the last VSync period.
       Those CRC are related to the picture displayed during the previous period (=  m_picturesUsedByHw.pCurNode) */
    ProcessLastVsyncStatus(vsyncTime, m_picturesUsedByHw.pCurNode);

    // Flush the pictures refused during the last VSync period
    FlushPicturesRefusedQueue();

    // A new VSync has happened: The pictures prepared for next VSync become the pictures used by the HW
    m_picturesUsedByHw = m_picturesPreparedForNextVSync;

    // A new triplet is going to be chosen for next VSync
    ResetTriplet(&m_picturesPreparedForNextVSync);

    if (m_picturesUsedByHw.pCurNode != 0)
    {
        SendDisplayCallback(m_picturesUsedByHw.pCurNode, vsyncTime);
    }

    // Now find the picture(s) to use at next VSync
    m_picturesPreparedForNextVSync.pCurNode = SelectPictureForNextVSync(isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);

    // Only if at least one deinterlacer plane is connected to this source
    if(GetParentSource()->hasADeinterlacer() && m_picturesPreparedForNextVSync.pCurNode)
    {
        // Look for the previous and next nodes
        m_picturesPreparedForNextVSync.pPrevNode = GetPreviousNode(m_picturesPreparedForNextVSync.pCurNode);
        m_picturesPreparedForNextVSync.pNextNode = (CDisplayNode*)m_picturesPreparedForNextVSync.pCurNode->GetNextNode();
    }

    ReleaseNodesNoMoreNeeded(vsyncTime);

    if (m_picturesPreparedForNextVSync.pCurNode)
    {
        if(m_bIsFrozen)
        {
            // When display is (or is going to) low power state, we should stop all plane activity
            // At generic CDisplayPlane level we continue to work normally but the picture is not presented to the HW plane
            NothingToDisplay();
            ResetTriplet(&m_picturesPreparedForNextVSync);
        }
        else
        {
            isPictureRepeated = (m_picturesPreparedForNextVSync.pCurNode == m_picturesUsedByHw.pCurNode);

            PresentDisplayNode( m_picturesPreparedForNextVSync.pPrevNode,   // Previous node (Optional)
                                m_picturesPreparedForNextVSync.pCurNode,    // Current node
                                m_picturesPreparedForNextVSync.pNextNode,   // Next node (Optional)
                                isPictureRepeated,
                                isDisplayInterlaced,
                                isTopFieldOnDisplay,
                                vsyncTime);

            if(m_picturesPreparedForNextVSync.pCurNode->m_bufferDesc.info.nfields > 0)
                m_picturesPreparedForNextVSync.pCurNode->m_bufferDesc.info.nfields--;
        }
    }
    else
    {
        // No valid candidate was found
        NothingToDisplay();
        ResetTriplet(&m_picturesPreparedForNextVSync);
    }

    return;
}


// !!! m_vsyncLock should be taken when this function is called !!!
void CPureSwQueueBufferInterface::FlushPicturesRefusedQueue(void)
{
    CDisplayNode   *pNode;
    int             nodesReleasedNb = 0;

    // Check that VSyncLock is already taken before accessing to shared variables
    DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

    pNode = (CDisplayNode*) m_picturesRefusedQueue.GetFirstNode();

    while(pNode != 0)
    {
        CDisplayNode *pReleasedNode = pNode;

        // Get the next node BEFORE releasing the current node (otherwise we lose the reference)
        pNode = (CDisplayNode*)pNode->GetNextNode();

        ReleaseRefusedNode(pReleasedNode);
        nodesReleasedNb++;
    }

    if (nodesReleasedNb)
    {
        TRC(TRC_ID_PICT_REFUSED, "Source %d: Released %d refused pictures", m_interfaceID, nodesReleasedNb);
    }
}

// Flush all the nodes belonging to the user indicated in argument
bool CPureSwQueueBufferInterface::FlushAllNodesFromUser(bool bFlushCurrentNode, const void * const user)
{
    CDisplayNode   *pNode;
    int             nodeReleasedNb = 0;

    if(!bFlushCurrentNode)
    {
        // We currently do the flush only if the playstream asks to flush everything
        return true;
    }

    TRC(TRC_ID_MAIN_INFO, "Source %d: Flush all nodes user 0x%p",m_interfaceID, user);

    ///////////////////////////////////////    Critical section      ///////////////////////////////////////////

    // Take the lock to ensure that OutputVSyncThreadedIrqUpdateHW() cannot be executed while we update the Queue
    if ( m_pDisplayDevice->VSyncLock() != 0)
        return false;

    FlushPicturesRefusedQueue();

    // Start from the beginning of the queue and release all the nodes corresponding to the indicated user
    pNode = (CDisplayNode*)m_displayQueue.GetFirstNode();

    while(pNode != 0)
    {
        CDisplayNode *pReleasedNode = pNode;

        // Get the next node BEFORE releasing the current node (otherwise we lose the reference)
        pNode = (CDisplayNode*)pNode->GetNextNode();

        if (pReleasedNode->m_user == user)
        {
            ReleaseDisplayNode(pReleasedNode, 0);
            nodeReleasedNb++;
        }
    }

    if((m_picturesUsedByHw.pPrevNode)&&(m_picturesUsedByHw.pPrevNode->m_user == user))
    {
        m_picturesUsedByHw.pPrevNode = 0;
    }

    if((m_picturesUsedByHw.pCurNode)&&(m_picturesUsedByHw.pCurNode->m_user == user))
    {
        m_picturesUsedByHw.pCurNode = 0;
    }

    if((m_picturesUsedByHw.pNextNode)&&(m_picturesUsedByHw.pNextNode->m_user == user))
    {
        m_picturesUsedByHw.pNextNode = 0;
    }

    if((m_picturesPreparedForNextVSync.pPrevNode)&&(m_picturesPreparedForNextVSync.pPrevNode->m_user == user))
    {
        m_picturesPreparedForNextVSync.pPrevNode = 0;
    }

    if((m_picturesPreparedForNextVSync.pCurNode)&&(m_picturesPreparedForNextVSync.pCurNode->m_user == user))
    {
        m_picturesPreparedForNextVSync.pCurNode = 0;
        NothingToDisplay();
    }

    if((m_picturesPreparedForNextVSync.pNextNode)&&(m_picturesPreparedForNextVSync.pNextNode->m_user == user))
    {
        m_picturesPreparedForNextVSync.pNextNode = 0;
    }

    m_pDisplayDevice->VSyncUnlock();
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////

    TRC(TRC_ID_MAIN_INFO, "%d nodes released",nodeReleasedNb);

    return true;
}

uint32_t CPureSwQueueBufferInterface::FillParamLatencyInfo(uint16_t *output_change_status)
{
    const CDisplayDevice *pDD               = 0;
    CDisplaySource *pDS                     = 0;
    CDisplayPlane  *pDP                     = 0;
    int             num_planes              = 0;
    int             index                   = 0;
    uint32_t        output_id               = STM_INVALID_OUTPUT_ID;
    bool            is_output_mode_changed  = false;
    bool            are_connections_changed = false;


    // Fill m_LatencyParams information
    pDS = GetParentSource();
    if( pDS != 0 )
    {
        pDD = pDS->GetParentDevice();
        if ( pDD != 0)
        {
            num_planes = pDS->GetConnectedPlaneID(m_LatencyPlaneIds, pDS->GetMaxNumConnectedPlanes() );
            for ( index = 0; index < num_planes; index++)
            {
                pDP = pDD->GetPlane(m_LatencyPlaneIds[index]);
                if( pDP != 0 )
                {
                    if ( pDP->GetConnectedOutputID(&output_id, 1) == 0)
                    {
                        output_id = STM_INVALID_OUTPUT_ID;
                    }

                    m_LatencyParams[index].output_id            = output_id;
                    m_LatencyParams[index].plane_id             = m_LatencyPlaneIds[index];
                    m_LatencyParams[index].output_latency_in_us = 0;
                    m_LatencyParams[index].output_change        = pDP->isOutputModeChanged();   // NB: This call will reset "m_isOutputModeChanged" on the plane

                    TRC( TRC_ID_DISPLAY_CB_LATENCY, "Source %d: m_LatencyParams[%d] output_id=%u  plane_id=%u latency=%u output_change=%s",
                                                    m_interfaceID,
                                                    index,
                                                    m_LatencyParams[index].output_id,
                                                    m_LatencyParams[index].plane_id,
                                                    m_LatencyParams[index].output_latency_in_us,
                                                    m_LatencyParams[index].output_change?"true":"false");

                    if (m_LatencyParams[index].output_change)
                    {
                        TRC( TRC_ID_DISPLAY_CB_LATENCY, "is_output_mode_changed");
                        is_output_mode_changed = true;
                    }
                    if ( pDP->AreConnectionsChanged() )     // NB: This call will reset "m_areConnectionsChanged" on the plane
                    {
                        TRC( TRC_ID_DISPLAY_CB_LATENCY, "are_connections_changed");
                        are_connections_changed = true;
                    }
                }
            }
        }
    }

    // Special case of num_planes equal to 0
    if ( num_planes == 0 )
    {
        m_LatencyParams[0].output_id            = STM_INVALID_OUTPUT_ID;
        m_LatencyParams[0].plane_id             = STM_INVALID_OUTPUT_ID;
        m_LatencyParams[0].output_latency_in_us = 0;
        m_LatencyParams[0].output_change        = 0;

        TRC( TRC_ID_DISPLAY_CB_LATENCY, "Source %d: m_LatencyParams[%d] output_id=%u  plane_id=%u latency=%u output_change=%s",
                                        m_interfaceID,
                                        0,
                                        m_LatencyParams[0].output_id,
                                        m_LatencyParams[0].plane_id,
                                        m_LatencyParams[0].output_latency_in_us,
                                        m_LatencyParams[0].output_change?"true":"false");
    }

    // Update output_change_status
    if (output_change_status)
    {
        // Reset flags
        *output_change_status = 0;

        // Check if output mode changed
        if(is_output_mode_changed)
        {
            *output_change_status |= OUTPUT_DISPLAY_MODE_CHANGE;
            TRC( TRC_ID_MAIN_INFO, "Source %d: OUTPUT_DISPLAY_MODE_CHANGE", m_interfaceID);
        }

        // Check if the connections changed
        if (are_connections_changed)
        {
            *output_change_status |= OUTPUT_CONNECTION_CHANGE;
            TRC( TRC_ID_MAIN_INFO, "Source %d: OUTPUT_CONNECTION_CHANGE", m_interfaceID);
        }

        TRC( TRC_ID_DISPLAY_CB_LATENCY, "Source %d: is_output_mode_changed=%s are_connections_changed=%s",
                                        m_interfaceID,
                                        is_output_mode_changed?"true":"false",
                                        are_connections_changed?"true":"false");
    }

    return ( num_planes );
}

void CPureSwQueueBufferInterface::SendDisplayCallback(CDisplayNode *pNode, const stm_time64_t &vsyncTime)
{
  uint32_t         nb_element           = 0;
  uint16_t         output_change_status = 0;

  // A valid picture (= m_picturesUsedByHw.curNode) is displayed by the HW. Check if it is the first time that we display this picture
  if ( (pNode->m_bufferDesc.info.display_callback) &&
       (!(pNode->m_bufferDesc.info.stats.status & STM_STATUS_BUF_DISPLAYED) ) )
  {
    TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: %d%c at %llu",  m_interfaceID, pNode->m_pictureId, pNode->m_srcPictureTypeChar, vsyncTime);

    nb_element = FillParamLatencyInfo(&output_change_status);

    pNode->m_bufferDesc.info.stats.status |= STM_STATUS_BUF_DISPLAYED;
    pNode->m_bufferDesc.info.display_callback(pNode->m_bufferDesc.info.puser_data,
                                            vsyncTime,
                                            output_change_status,
                                            nb_element,
                                            m_LatencyParams);
  }
}


// !!! m_vsyncLock should be taken when this function is called !!!
CDisplayNode * CPureSwQueueBufferInterface::SelectPictureForNextVSync(bool                  isDisplayInterlaced,
                                                                      bool                  isTopFieldOnDisplay,
                                                                      const stm_time64_t   &vsyncTime)
{
    CDisplayNode *pNodeCandidateForDisplay = 0;
    CDisplayNode *pNodeToRelease = 0;
    bool          isNextVSyncTop;

    // Check that VSyncLock is already taken before accessing to shared variables
    DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

    if (m_picturesUsedByHw.pCurNode == 0)
    {
        // No picture is currently displayed: Get the first picture of the queue (if any)
        pNodeCandidateForDisplay = (CDisplayNode*)m_displayQueue.GetFirstNode();
        if (pNodeCandidateForDisplay)
        {
            TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: No picture currently displayed. Try the 1st node of the queue %d%c (0x%p)",
                        m_interfaceID,
                        pNodeCandidateForDisplay->m_pictureId,
                        pNodeCandidateForDisplay->m_srcPictureTypeChar,
                        pNodeCandidateForDisplay );
        }
    }
    else
    {
        // A picture is currently displayed. Check if should continue to display it
        if (m_picturesUsedByHw.pCurNode->m_bufferDesc.info.nfields > 0)
        {
            pNodeCandidateForDisplay = m_picturesUsedByHw.pCurNode;
            TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Keep same node %d%c (0x%p) on display (nfields=%d)",
                        m_interfaceID,
                        m_picturesUsedByHw.pCurNode->m_pictureId,
                        m_picturesUsedByHw.pCurNode->m_srcPictureTypeChar,
                        m_picturesUsedByHw.pCurNode,
                        m_picturesUsedByHw.pCurNode->m_bufferDesc.info.nfields );
        }
        else
        {
            // Get the next picture in the queue
            pNodeCandidateForDisplay = (CDisplayNode*) m_picturesUsedByHw.pCurNode->GetNextNode();
            if (pNodeCandidateForDisplay)
            {
                TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Try next node: %d%c (0x%p)",
                    m_interfaceID,
                    pNodeCandidateForDisplay->m_pictureId,
                    pNodeCandidateForDisplay->m_srcPictureTypeChar,
                    pNodeCandidateForDisplay );
            }
        }
    }

    // Check if pNodeCandidateForDisplay belongs to the active user of this plane.
    // Otherwise, ignore it, release it and continue to go through the linked list
    while ( (pNodeCandidateForDisplay != 0) &&  (pNodeCandidateForDisplay->m_user != m_user))
    {
        // pNodeCandidateForDisplay is from a user that is no more the active one.
        // Skip it and release it immediately
        pNodeToRelease = pNodeCandidateForDisplay;
        TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Skip node %d%c (0x%p) from invalid user",
            m_interfaceID,
            pNodeToRelease->m_pictureId,
            pNodeToRelease->m_srcPictureTypeChar,
            pNodeToRelease );

        pNodeCandidateForDisplay = (CDisplayNode*) pNodeCandidateForDisplay->GetNextNode();

        // We make an exception for the Current Node. We don't want to release it because this is our backup plan
        // if we don't find an appropriate picture.
        if(pNodeToRelease != m_picturesUsedByHw.pCurNode)
        {
            ReleaseDisplayNode(pNodeToRelease, vsyncTime);
        }
    }

    /*
     * In order to handle FRC conversion of interlaced streams, SE will send the field to be skipped to
     * display engine, so it will allow DEI based on the right field without displaying this skipped field.
     * But it should be only used by Video Display Plane, not for other planes (because so far, some GDP planes
     * like Main-GDP1 and Aux-GDP1 still send nodes with "nfield = 0".
     */
    if( (pNodeCandidateForDisplay) && (GetParentSource()->isUsedByVideoPlanes()) )
    {
        TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Check if node %d%c (0x%p) is skipped by FRC",
            m_interfaceID,
            pNodeCandidateForDisplay->m_pictureId,
            pNodeCandidateForDisplay->m_srcPictureTypeChar,
            pNodeCandidateForDisplay);

        while ((pNodeCandidateForDisplay != 0) && (pNodeCandidateForDisplay->m_isSkippedForFRC))
        {
            /* It is the tempory solution for SE to test its FRC implementation, it should be removed for Main-VID in future */
            TRC( TRC_ID_PICTURE_SCHEDULING, "Source %d: Skip node %d%c (nfields=%d 0x%p) for FRC",
                m_interfaceID,
                pNodeCandidateForDisplay->m_pictureId,
                pNodeCandidateForDisplay->m_srcPictureTypeChar,
                pNodeCandidateForDisplay->m_bufferDesc.info.nfields,
                pNodeCandidateForDisplay);

            pNodeCandidateForDisplay = (CDisplayNode*) pNodeCandidateForDisplay->GetNextNode();
        }
    }

    if( (pNodeCandidateForDisplay) && (pNodeCandidateForDisplay->m_user == m_user) )
    {
        TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Check if it is time to display %d%c (0x%p)",
            m_interfaceID,
            pNodeCandidateForDisplay->m_pictureId,
            pNodeCandidateForDisplay->m_srcPictureTypeChar,
            pNodeCandidateForDisplay);

        // We have found a picture from the active user. Check if it is time to display it
        stm_time64_t presentation_time = pNodeCandidateForDisplay->m_bufferDesc.info.presentation_time;

        if( (presentation_time != 0LL) &&
            (presentation_time > (vsyncTime + (m_outputInfo.outputVSyncDurationInUs * 3/2) ) ) )
        {
            TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Too early to display the node %d%c (0x%p)",
                m_interfaceID,
                pNodeCandidateForDisplay->m_pictureId,
                pNodeCandidateForDisplay->m_srcPictureTypeChar,
                pNodeCandidateForDisplay);
            TRC(TRC_ID_PICTURE_SCHEDULING, "presentation_time=%llu vsyncTime=%llu, deltaInMs=%llu, VSyncDurationInUs=%llu",
                    presentation_time,
                    vsyncTime,
                    vibe_os_div64(presentation_time - vsyncTime,1000),
                    m_outputInfo.outputVSyncDurationInUs );
            pNodeCandidateForDisplay = 0;
        }
    }

    if (pNodeCandidateForDisplay)
    {
        // The flag STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY has been set by StreamingEngine when the picture has been queued.
        // The output mode may have changed in the meantime so we should check if we are still in I to I use case
        if ( (pNodeCandidateForDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) &&
             (m_outputInfo.isDisplayInterlaced)                                             &&
             (pNodeCandidateForDisplay->m_bufferDesc.src.flags & STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY))
        {
            // In case of I->I use case, fields should be presented on the right output polarities (to avoid field inversions)
            TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: Picture %d%c (0x%p) selected. Check polarities",
                m_interfaceID,
                pNodeCandidateForDisplay->m_pictureId,
                pNodeCandidateForDisplay->m_srcPictureTypeChar,
                pNodeCandidateForDisplay);

            // The next VSync will have the opposite polarity of current polarity
            isNextVSyncTop = !isTopFieldOnDisplay;

            if ( ( (pNodeCandidateForDisplay->m_srcPictureType == GNODE_BOTTOM_FIELD) &&  isNextVSyncTop) ||
                 ( (pNodeCandidateForDisplay->m_srcPictureType == GNODE_TOP_FIELD)    && !isNextVSyncTop) )
            {
                TRC(TRC_ID_PICTURE_SCHEDULING, "Repeat current field to avoid a field inversion");
                pNodeCandidateForDisplay = 0;
            }
        }
    }

    // Warning: pNodeCandidateForDisplay needs to be checked again as it might have been set to zero in previous if.
    if (!pNodeCandidateForDisplay)
    {
        // No valid candidate was found. If we have a CurrentNode, continue to display it
        if (m_picturesUsedByHw.pCurNode)
        {
            pNodeCandidateForDisplay = m_picturesUsedByHw.pCurNode;
            TRC(TRC_ID_PICTURE_SCHEDULING, "Source %d: No valid candidate found. Keep node %d%c (0x%p) on display",
                m_interfaceID,
                pNodeCandidateForDisplay->m_pictureId,
                pNodeCandidateForDisplay->m_srcPictureTypeChar,
                pNodeCandidateForDisplay );
        }
        else
        {
            // Nothing to display!
            pNodeCandidateForDisplay = 0;
        }
    }

    return pNodeCandidateForDisplay;
}


void CPureSwQueueBufferInterface::ResetTriplet(display_triplet_t *pTriplet)
{
    pTriplet->pPrevNode = 0;
    pTriplet->pCurNode  = 0;
    pTriplet->pNextNode = 0;
}


/* To search the previous node for de-interlacer:
 * The node should be in front of the current display node in the queue
 */
CDisplayNode* CPureSwQueueBufferInterface::GetPreviousNode(CDisplayNode *pCurNode)
{
    CDisplayNode *pNode = 0;
    CDisplayNode *pPrevNode = 0;

    if(!pCurNode)
        return pPrevNode;

    /* Get the first node in the queue (if any) */
    pNode = (CDisplayNode*)m_displayQueue.GetFirstNode();

    /* Set the previous node used by DEI which should be one node in front of the current node in the queue */
    while (pNode != 0)
    {
        if(pNode == pCurNode)
            break;
        pPrevNode = pNode;
        pNode = (CDisplayNode*) pNode->GetNextNode();
    }

    TRC( TRC_ID_PICTURE_SCHEDULING, "pPrevNode 0x%p", pPrevNode);
    return pPrevNode;
}

void CPureSwQueueBufferInterface::PresentDisplayNode(CDisplayNode *pPrevNode,
                                                     CDisplayNode *pCurrNode,
                                                     CDisplayNode *pNextNode,
                                                     bool isPictureRepeated,
                                                     bool isDisplayInterlaced,
                                                     bool isTopFieldOnDisplay,
                                                     const stm_time64_t &vsyncTime)
{
    uint32_t               num_planes_ids;
    uint32_t               planes_ids[CINTERFACE_MAX_PLANES_PER_SOURCE];
    CDisplaySource*        pDS  = GetParentSource();

    // Present the picture to every connected planes
    if ( pDS != 0 )
    {
        const CDisplayDevice* pDD = pDS->GetParentDevice();
        if ( pDD != 0)
        {
            num_planes_ids = pDS->GetConnectedPlaneID(planes_ids, N_ELEMENTS(planes_ids));
            for(uint32_t i=0; (i<num_planes_ids) ; i++)
            {
                CDisplayPlane* pDP = pDD->GetPlane(planes_ids[i]);
                if (pDP)
                {
                    TRC( TRC_ID_UNCLASSIFIED, "Source %d: PresentDisplayNode on %s", m_interfaceID, pDP->GetName() );
                    pDP->PresentDisplayNode(pPrevNode,
                                        pCurrNode,
                                        pNextNode,
                                        isPictureRepeated,
                                        isDisplayInterlaced,
                                        isTopFieldOnDisplay,
                                        vsyncTime);
                }
            }
        }
    }
}

void CPureSwQueueBufferInterface::NothingToDisplay(void)
{
    uint32_t               num_planes_ids;
    uint32_t               planes_ids[CINTERFACE_MAX_PLANES_PER_SOURCE];
    CDisplaySource*        pDS  = GetParentSource();

    // Call NothingToDisplay() for every connected planes
    if ( pDS != 0 )
    {
        const CDisplayDevice* pDD = pDS->GetParentDevice();
        if ( pDD != 0)
        {
            num_planes_ids = pDS->GetConnectedPlaneID(planes_ids, N_ELEMENTS(planes_ids));
            for(uint32_t i=0; (i<num_planes_ids) ; i++)
            {
                CDisplayPlane* pDP = pDD->GetPlane(planes_ids[i]);
                if (pDP)
                {
                    TRC( TRC_ID_UNCLASSIFIED, "Source %d: NothingToDisplay on %s", m_interfaceID, pDP->GetName() );
                    pDP->NothingToDisplay();
                }
            }
        }
    }
}

void CPureSwQueueBufferInterface::ProcessLastVsyncStatus(const stm_time64_t &vsyncTime, CDisplayNode *pNodeDisplayed)
{
    uint32_t               num_planes_ids;
    uint32_t               planes_ids[CINTERFACE_MAX_PLANES_PER_SOURCE];
    CDisplaySource*        pDS  = GetParentSource();

    // Call ProcessLastVsyncStatus() for every connected planes
    if ( pDS != 0 )
    {
        const CDisplayDevice* pDD = pDS->GetParentDevice();
        if ( pDD != 0)
        {
            num_planes_ids = pDS->GetConnectedPlaneID(planes_ids, N_ELEMENTS(planes_ids));
            for(uint32_t i=0; (i<num_planes_ids) ; i++)
            {
                CDisplayPlane* pDP = pDD->GetPlane(planes_ids[i]);
                if (pDP)
                {
                    TRC( TRC_ID_UNCLASSIFIED, "Source %d: ProcessLastVsyncStatus for %s", m_interfaceID, pDP->GetName() );
                    pDP->ProcessLastVsyncStatus(vsyncTime, pNodeDisplayed);
                }
            }
        }
    }
}


bool CPureSwQueueBufferInterface::AreSrcCharacteristicsChanged(CDisplayNode *pNode)
{

    if((pNode->m_bufferDesc.src.horizontal_decimation_factor != m_currentPictureCharact.src.horizontal_decimation_factor)
     ||(pNode->m_bufferDesc.src.vertical_decimation_factor   != m_currentPictureCharact.src.vertical_decimation_factor))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.pixel_aspect_ratio.numerator   != m_currentPictureCharact.src.pixel_aspect_ratio.numerator)
     ||(pNode->m_bufferDesc.src.pixel_aspect_ratio.denominator != m_currentPictureCharact.src.pixel_aspect_ratio.denominator))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.primary_picture.color_fmt   != m_currentPictureCharact.src.primary_picture.color_fmt)
     ||(pNode->m_bufferDesc.src.primary_picture.height      != m_currentPictureCharact.src.primary_picture.height)
     ||(pNode->m_bufferDesc.src.primary_picture.width       != m_currentPictureCharact.src.primary_picture.width)
     ||(pNode->m_bufferDesc.src.primary_picture.pitch       != m_currentPictureCharact.src.primary_picture.pitch)
     ||(pNode->m_bufferDesc.src.primary_picture.pixel_depth != m_currentPictureCharact.src.primary_picture.pixel_depth))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.secondary_picture.color_fmt   != m_currentPictureCharact.src.secondary_picture.color_fmt)
     ||(pNode->m_bufferDesc.src.secondary_picture.height      != m_currentPictureCharact.src.secondary_picture.height)
     ||(pNode->m_bufferDesc.src.secondary_picture.width       != m_currentPictureCharact.src.secondary_picture.width)
     ||(pNode->m_bufferDesc.src.secondary_picture.pitch       != m_currentPictureCharact.src.secondary_picture.pitch)
     ||(pNode->m_bufferDesc.src.secondary_picture.pixel_depth != m_currentPictureCharact.src.secondary_picture.pixel_depth))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.visible_area.height != m_currentPictureCharact.src.visible_area.height)
      ||(pNode->m_bufferDesc.src.visible_area.width != m_currentPictureCharact.src.visible_area.width)
      ||(pNode->m_bufferDesc.src.visible_area.x     != m_currentPictureCharact.src.visible_area.x)
      ||(pNode->m_bufferDesc.src.visible_area.y     != m_currentPictureCharact.src.visible_area.y))
        goto characteristics_changing;

    if(pNode->m_bufferDesc.src.linear_center_percentage != m_currentPictureCharact.src.linear_center_percentage)
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.src_frame_rate.numerator   != m_currentPictureCharact.src.src_frame_rate.numerator)
     ||(pNode->m_bufferDesc.src.src_frame_rate.denominator != m_currentPictureCharact.src.src_frame_rate.denominator))
        goto characteristics_changing;

    if(( (pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_CONST_ALPHA) != (m_currentPictureCharact.src.flags&STM_BUFFER_SRC_CONST_ALPHA))
     ||(pNode->m_bufferDesc.src.ulConstAlpha != m_currentPictureCharact.src.ulConstAlpha))
        goto characteristics_changing;

    if(pNode->m_bufferDesc.src.config_3D.formats != m_currentPictureCharact.src.config_3D.formats)
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.config_3D.parameters.field_alt.is_left_right_format != m_currentPictureCharact.src.config_3D.parameters.field_alt.is_left_right_format)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.field_alt.vactive              != m_currentPictureCharact.src.config_3D.parameters.field_alt.vactive)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.field_alt.vblank               != m_currentPictureCharact.src.config_3D.parameters.field_alt.vblank))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.config_3D.parameters.frame_packed.is_left_right_format != m_currentPictureCharact.src.config_3D.parameters.frame_packed.is_left_right_format)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.frame_packed.vactive_space1 != m_currentPictureCharact.src.config_3D.parameters.frame_packed.vactive_space1)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.frame_packed.vactive_space2 != m_currentPictureCharact.src.config_3D.parameters.frame_packed.vactive_space2))
        goto characteristics_changing;

    if(pNode->m_bufferDesc.src.config_3D.parameters.frame_seq != m_currentPictureCharact.src.config_3D.parameters.frame_seq)
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.config_3D.parameters.l_d.vact_space != m_currentPictureCharact.src.config_3D.parameters.l_d.vact_space)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.l_d.vact_video != m_currentPictureCharact.src.config_3D.parameters.l_d.vact_video))
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.config_3D.parameters.l_d__g_g.vact_space != m_currentPictureCharact.src.config_3D.parameters.l_d__g_g.vact_space)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.l_d__g_g.vact_video != m_currentPictureCharact.src.config_3D.parameters.l_d__g_g.vact_video))
        goto characteristics_changing;

    if(pNode->m_bufferDesc.src.config_3D.parameters.picture_interleave != m_currentPictureCharact.src.config_3D.parameters.picture_interleave)
        goto characteristics_changing;

    if((pNode->m_bufferDesc.src.config_3D.parameters.sbs.is_left_right_format != m_currentPictureCharact.src.config_3D.parameters.sbs.is_left_right_format)
     ||(pNode->m_bufferDesc.src.config_3D.parameters.sbs.sbs_sampling_mode    != m_currentPictureCharact.src.config_3D.parameters.sbs.sbs_sampling_mode))
        goto characteristics_changing;

    // Characteristics not changing
    return false;

characteristics_changing:

       TRCBL(TRC_ID_MAIN_INFO);
       TRC( TRC_ID_MAIN_INFO, "Source %d: Node %p with new pict charact:", m_interfaceID, pNode );

       TRC( TRC_ID_MAIN_INFO, "Picture size in memory: W%d H%d P%d",
                pNode->m_bufferDesc.src.primary_picture.width,
                pNode->m_bufferDesc.src.primary_picture.height,
                pNode->m_bufferDesc.src.primary_picture.pitch);

       TRC( TRC_ID_MAIN_INFO, "Visible_area: x=%d, y=%d, w=%d, h=%d",
                pNode->m_bufferDesc.src.visible_area.x,
                pNode->m_bufferDesc.src.visible_area.y,
                pNode->m_bufferDesc.src.visible_area.width,
                pNode->m_bufferDesc.src.visible_area.height);

       TRC( TRC_ID_MAIN_INFO, "FR%d %c",
                ( pNode->m_bufferDesc.src.src_frame_rate.denominator ? (1000 * pNode->m_bufferDesc.src.src_frame_rate.numerator) / pNode->m_bufferDesc.src.src_frame_rate.denominator : 0),
                ( (pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) ? 'I' : 'P')  );

   /* If m_isSkippedForFRC is true for pNode, we will set the flag STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY
       but we will NOT copy pNode->m_bufferDesc in m_currentPictureCharact.
       By this way, the flag will be set again on next picture until we get a picture with m_isSkippedForFRC == false. */
    if(! (pNode->m_isSkippedForFRC))
    {
        m_currentPictureCharact = (pNode->m_bufferDesc);
    }

    return true;
}

bool CPureSwQueueBufferInterface::FillSrcPictureType(CDisplayNode *pNode)
{
    if((pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED))
    {
        if((pNode->m_bufferDesc.src.flags &
           (STM_BUFFER_SRC_TOP_FIELD_ONLY|STM_BUFFER_SRC_BOTTOM_FIELD_ONLY)) == STM_BUFFER_SRC_TOP_FIELD_ONLY)
        {
            pNode->m_srcPictureType = GNODE_TOP_FIELD;
            pNode->m_srcPictureTypeChar = 'T';
        }
        else if ((pNode->m_bufferDesc.src.flags &
                 (STM_BUFFER_SRC_TOP_FIELD_ONLY|STM_BUFFER_SRC_BOTTOM_FIELD_ONLY)) == STM_BUFFER_SRC_BOTTOM_FIELD_ONLY)
        {
            pNode->m_srcPictureType   = GNODE_BOTTOM_FIELD;
            pNode->m_srcPictureTypeChar = 'B';
        }
        else
        {
            DASSERTF(false,("Invalid Picture Type!\n"), false);
        }
    }
    else
    {
        pNode->m_srcPictureType = GNODE_PROGRESSIVE;
        pNode->m_srcPictureTypeChar = 'F';
    }

    return true;
}

// Information filled when the buffer is received (QueueBuffer)
bool CPureSwQueueBufferInterface::FillNodeInfo(CDisplayNode *pNode)
{
    bool isSrcInterlaced = ((pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) == STM_BUFFER_SRC_INTERLACED);

    if (!FillSrcPictureType(pNode))
    {
        TRC( TRC_ID_ERROR, "Failed to get the Src Picture type!" );
        return false;
    }

    /* set field info */
    if(isSrcInterlaced)
    {
        /* set repeatFirstField */
        pNode->m_repeatFirstField = ((pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_REPEAT_FIRST_FIELD) != 0);

        /* set firstFieldType and firstFieldOnly flag */
        if((pNode->m_bufferDesc.src.flags & (STM_BUFFER_SRC_TOP_FIELD_ONLY|STM_BUFFER_SRC_BOTTOM_FIELD_ONLY)) != 0)
        {
            pNode->m_firstFieldType   = ((pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_TOP_FIELD_ONLY) != 0)?GNODE_TOP_FIELD:GNODE_BOTTOM_FIELD;
            pNode->m_firstFieldOnly   = true;
        }
        else
        {
            pNode->m_firstFieldType   = ((pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_BOTTOM_FIELD_FIRST) != 0)?GNODE_BOTTOM_FIELD:GNODE_TOP_FIELD;
        }
    }

    if( (pNode->m_bufferDesc.src.pixel_aspect_ratio.numerator == 0) ||
        (pNode->m_bufferDesc.src.pixel_aspect_ratio.denominator == 0) )
    {
        // Invalid source pixel aspect ratio: Use a default one
        pNode->m_bufferDesc.src.pixel_aspect_ratio.numerator = 1;
        pNode->m_bufferDesc.src.pixel_aspect_ratio.denominator = 1;
    }

    return true;
}

void CPureSwQueueBufferInterface::ReleaseRefusedNode(CDisplayNode *pNodeToRelease)
{
    stm_buffer_presentation_t *pInfo;

    if(!pNodeToRelease)
    {
        TRC( TRC_ID_ERROR, "Invalid pNodeToRelease!" );
        return;
    }

    pInfo = &pNodeToRelease->m_bufferDesc.info;
    if(pInfo)
    {
        pInfo->stats.vsyncTime = vibe_os_get_system_time();
        pInfo->stats.status = 0;
        if(pInfo->completed_callback)
        {
            TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: completed_callback node %d%c (0x%p) from user 0x%p (user_data=0x%p)",
                m_interfaceID,
                pNodeToRelease->m_pictureId,
                pNodeToRelease->m_srcPictureTypeChar,
                pNodeToRelease,
                pNodeToRelease->m_user,
                pInfo->puser_data);
            pInfo->completed_callback(pInfo->puser_data, &pInfo->stats);
        }
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Node with null pInfo: Impossible to notify the owner of this buffer" );
    }

    TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: Released refused node %d%c (0x%p)",
        m_interfaceID,
        pNodeToRelease->m_pictureId,
        pNodeToRelease->m_srcPictureTypeChar,
        pNodeToRelease);

    if (!m_picturesRefusedQueue.ReleaseDisplayNode(pNodeToRelease))
    {
        TRC( TRC_ID_ERROR, "Failed to release node 0x%p from m_picturesRefusedQueue!", pNodeToRelease );
    }
}


/*
    Example of pictures used at one time:

                                        10T -> 11B -> 12T -> 13B -> (14T) -> 15B -> 16T -> 17B
    Pictures used by HW:                               P      C       N
    Pictures prepared for next VSync:                                 P       C      N

    In this example, all the pictures older than m_picturesUsedByHw.pPrevNode (= 12T) will not be used anymore and can be released.
    So 10T and 11B are going to be released by CDisplayPlane::ReleaseNodesNoMoreNeeded()

    Legend:
     P = Previous DEI field
     C = Current DEI field
     N = Next DEI field
     ( ) = Picture skipped by the Frame Rate Conversion. It can be used as reference by the Deinterlacer but it should not be displayed.
*/
void CPureSwQueueBufferInterface::ReleaseNodesNoMoreNeeded(const stm_time64_t & vsyncTime)
{
    CDisplayNode *pNode = 0;
    CDisplayNode *pOldestNodeUsedByTheDisplay = 0;
    CDisplayNode *pNodeToRelease = 0;

    // The releases should be done only if the display is started (m_picturesUsedByHw.pCurNode not null)
    if(!m_picturesUsedByHw.pCurNode)
    {
        return;
    }

    // Determine what is the oldest picture used by the Display
    if(m_picturesUsedByHw.pPrevNode)
    {
        pOldestNodeUsedByTheDisplay = m_picturesUsedByHw.pPrevNode;
    }
    else
    {
        // NB: We have already tested that m_picturesUsedByHw.pCurNode is not null
        pOldestNodeUsedByTheDisplay = m_picturesUsedByHw.pCurNode;
    }

    // All the pictures present in the queue and older than "pOldestNodeUsedByTheDisplay" will not be needed anymore and can be released now

    /* Get the first picture of the queue (if any) */
    pNode = (CDisplayNode*)m_displayQueue.GetFirstNode();

    while (pNode != 0)
    {
        /* Release all the nodes until "pOldestNodeUsedByTheDisplay" is found */
        if(pNode == pOldestNodeUsedByTheDisplay)
        {
            break;
        }

        pNodeToRelease = pNode;

        // Get next node before releasing the current one
        pNode = (CDisplayNode*) pNode->GetNextNode();

        TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: ReleaseNode %d%c (0x%p)",
                m_interfaceID,
                pNodeToRelease->m_pictureId,
                pNodeToRelease->m_srcPictureTypeChar,
                pNodeToRelease);
        ReleaseDisplayNode(pNodeToRelease, vsyncTime);
    }
}


// This function will send the completed_callback and release a node from the queue
void CPureSwQueueBufferInterface::ReleaseDisplayNode(CDisplayNode *pNodeToRelease, const stm_time64_t &vsyncTime)
{
    stm_buffer_presentation_t *pInfo;

    if(!pNodeToRelease)
    {
        TRC( TRC_ID_ERROR, "Invalid pNodeToRelease!" );
        return;
    }

    RevokeFrameBufferAccess(&pNodeToRelease->m_bufferDesc);

    pInfo = &pNodeToRelease->m_bufferDesc.info;
    if(pInfo)
    {
        pInfo->stats.vsyncTime = vsyncTime;
        pInfo->stats.status = 0;
        if(pInfo->completed_callback)
        {
            TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: completed_callback node %d%c (0x%p) from user 0x%p (user_data=0x%p)",
                m_interfaceID,
                pNodeToRelease->m_pictureId,
                pNodeToRelease->m_srcPictureTypeChar,
                pNodeToRelease,
                pNodeToRelease->m_user,
                pInfo->puser_data);
            pInfo->completed_callback(pInfo->puser_data, &pInfo->stats);
        }
        else // a NULL CompletedCallback is normal with a buffer taqged persistent
            if(!(pInfo->ulFlags & STM_BUFFER_PRESENTATION_PERSISTENT))
                TRC( TRC_ID_ERROR, "Invalid PresentationData! Missing CompletedCallback with a non persistent presentation!" );
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "Node with null pInfo: Impossible to notify the owner of this buffer" );
    }

    m_Statistics.PicOwned--;
    m_Statistics.PicReleased++;
    TRC( TRC_ID_PICT_QUEUE_RELEASE, "Source %d: Release %d%c 0x%p (Owned:%d)",
                m_interfaceID,
                pNodeToRelease->m_pictureId,
                pNodeToRelease->m_srcPictureTypeChar,
                pNodeToRelease,
                m_Statistics.PicOwned);

    // This ReleaseDisplayNode() function can be called in 2 contexts:
    // - During the OutputVSyncThreadedIrqUpdateHW. In that case "m_vsyncLock" is taken to ensure that nobody else is updating the queue.
    // - During the flush function. In that case there is a mechanism in the Flush function to do all the critical code in a protected section. The queue is cut
    //   in 2 parts. The head of the queue is still in use (and can be used by the OutputVSyncThreadedIrqUpdateHW) whereas the tail is no more seen by the OutputVSyncThreadedIrqUpdateHW
    //   so it is safe to release it without taking the "m_vsyncLock"
    if (!m_displayQueue.ReleaseDisplayNode(pNodeToRelease))
    {
        TRC( TRC_ID_ERROR, "Failed to release node 0x%p!", pNodeToRelease );
    }
}

void CPureSwQueueBufferInterface::GrantFrameBufferAccess(const stm_display_buffer_t* const pFrame)
{
  if(pFrame->src.primary_picture.video_buffer_addr)
  {
    vibe_os_grant_access((void*)pFrame->src.primary_picture.video_buffer_addr,
                         pFrame->src.primary_picture.video_buffer_size,
                         VIBE_ACCESS_TID_DISPLAY,
                         VIBE_ACCESS_O_READ);
  } else {
    TRC(TRC_ID_SDP, "pFrame->src.primary_picture.video_buffer_addr is NULL");
  }

  if(pFrame->src.secondary_picture.video_buffer_addr)
  {
    vibe_os_grant_access((void*)pFrame->src.secondary_picture.video_buffer_addr,
                         pFrame->src.secondary_picture.video_buffer_size,
                         VIBE_ACCESS_TID_DISPLAY,
                         VIBE_ACCESS_O_READ);
  } else {
    TRC(TRC_ID_SDP, "pFrame->src.secondary_picture.video_buffer_addr is NULL");
  }
}

void CPureSwQueueBufferInterface::RevokeFrameBufferAccess(const stm_display_buffer_t* const pFrame)
{
  if(pFrame->src.primary_picture.video_buffer_addr)
  {
    vibe_os_revoke_access((void*)pFrame->src.primary_picture.video_buffer_addr,
                          pFrame->src.primary_picture.video_buffer_size,
                          VIBE_ACCESS_TID_DISPLAY,
                          VIBE_ACCESS_O_READ);
  }

  if(pFrame->src.secondary_picture.video_buffer_addr)
  {
    vibe_os_revoke_access((void*)pFrame->src.secondary_picture.video_buffer_addr,
                          pFrame->src.secondary_picture.video_buffer_size,
                          VIBE_ACCESS_TID_DISPLAY,
                          VIBE_ACCESS_O_READ);
  }
}

// This function is called only if the trace TRC_ID_BUFFER_DESCRIPTOR gets enabled
void CPureSwQueueBufferInterface::PrintBufferDescriptor(const CDisplayNode *pNode)
{
    char Htext[25];
    char Vtext[25];

    if(!pNode)
    {
        return;
    }

    // Print a selection of interesting info about this node
    // Group them to reduce size of the log
    TRCBL(TRC_ID_BUFFER_DESCRIPTOR);

    TRC( TRC_ID_BUFFER_DESCRIPTOR, "Source %d: Received Picture %d %c",
            m_interfaceID,
            pNode->m_pictureId,
            pNode->m_srcPictureTypeChar);

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "Primary picture: W=%d H=%d P=%d color_fmt=%d",
        pNode->m_bufferDesc.src.primary_picture.width,
        pNode->m_bufferDesc.src.primary_picture.height,
        pNode->m_bufferDesc.src.primary_picture.pitch,
        pNode->m_bufferDesc.src.primary_picture.color_fmt);

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "Secondary picture: W=%d H=%d P=%d color_fmt=%d",
        pNode->m_bufferDesc.src.secondary_picture.width,
        pNode->m_bufferDesc.src.secondary_picture.height,
        pNode->m_bufferDesc.src.secondary_picture.pitch,
        pNode->m_bufferDesc.src.secondary_picture.color_fmt);

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "visible_area: X=%d Y=%d W=%d H=%d",
        pNode->m_bufferDesc.src.visible_area.x,
        pNode->m_bufferDesc.src.visible_area.y,
        pNode->m_bufferDesc.src.visible_area.width,
        pNode->m_bufferDesc.src.visible_area.height);

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "pixel_aspect_ratio: %d / %d",
        pNode->m_bufferDesc.src.pixel_aspect_ratio.numerator,
        pNode->m_bufferDesc.src.pixel_aspect_ratio.denominator);

    switch(pNode->m_bufferDesc.src.horizontal_decimation_factor)
    {
        case STM_NO_DECIMATION:
        {
            vibe_os_snprintf(Htext, sizeof(Htext), "H1");
            break;
        }
        case STM_DECIMATION_BY_TWO:
        {
            vibe_os_snprintf(Htext, sizeof(Htext), "H2");
            break;
        }
        case STM_DECIMATION_BY_FOUR:
        {
            vibe_os_snprintf(Htext, sizeof(Htext), "H4");
            break;
        }
        case STM_DECIMATION_BY_EIGHT:
        {
            vibe_os_snprintf(Htext, sizeof(Htext), "H8");
            break;
        }
        default:
        {
            TRC(TRC_ID_ERROR, "Invalid horizontal decimation factor");
            vibe_os_snprintf(Htext, sizeof(Htext), "H?");
            break;
        }
    }

    switch(pNode->m_bufferDesc.src.vertical_decimation_factor)
    {
        case STM_NO_DECIMATION:
        {
            vibe_os_snprintf(Vtext, sizeof(Vtext), "V1");
            break;
        }
        case STM_DECIMATION_BY_TWO:
        {
            vibe_os_snprintf(Vtext, sizeof(Vtext), "V2");
            break;
        }
        case STM_DECIMATION_BY_FOUR:
        {
            vibe_os_snprintf(Vtext, sizeof(Vtext), "V4");
            break;
        }
        case STM_DECIMATION_BY_EIGHT:
        {
            vibe_os_snprintf(Vtext, sizeof(Vtext), "V8");
            break;
        }
        default:
        {
            TRC(TRC_ID_ERROR, "Invalid vertical decimation factor");
            vibe_os_snprintf(Vtext, sizeof(Vtext), "V?");
            break;
        }
    }

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "src_frame_rate=%d %c hdecim=%s, vdecim=%s",
        (pNode->m_bufferDesc.src.src_frame_rate.denominator ? (1000 * pNode->m_bufferDesc.src.src_frame_rate.numerator) / pNode->m_bufferDesc.src.src_frame_rate.denominator : 0),
        ( (pNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_INTERLACED) ? 'I' : 'P'),
        Htext,
        Vtext);

    TRC(TRC_ID_BUFFER_DESCRIPTOR, "flags:");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_INTERLACED)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_INTERLACED");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_BOTTOM_FIELD_FIRST)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_BOTTOM_FIELD_FIRST");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_REPEAT_FIRST_FIELD)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_REPEAT_FIRST_FIELD");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_TOP_FIELD_ONLY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_TOP_FIELD_ONLY");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_BOTTOM_FIELD_ONLY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_BOTTOM_FIELD_ONLY");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_INTERPOLATE_FIELDS)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_INTERPOLATE_FIELDS");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_COLORSPACE_709)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_COLORSPACE_709");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_PREMULTIPLIED_ALPHA)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_PREMULTIPLIED_ALPHA");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_LIMITED_RANGE_ALPHA)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_LIMITED_RANGE_ALPHA");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_PAN_AND_SCAN)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_PAN_AND_SCAN");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_CONST_ALPHA)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_CONST_ALPHA");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_REPEATED_PICTURE)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_REPEATED_PICTURE");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_VC1_POSTPROCESS_LUMA)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_VC1_POSTPROCESS_LUMA");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_VC1_POSTPROCESS_CHROMA)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_VC1_POSTPROCESS_CHROMA");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_FORCE_DISPLAY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_FORCE_DISPLAY");
    if(pNode->m_bufferDesc.src.flags&STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY)
        TRC(TRC_ID_BUFFER_DESCRIPTOR, "STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY");


    TRC(TRC_ID_BUFFER_DESCRIPTOR, "presentation_time=%lld, PTS=%lld, nfields=%d, ulFlags=0x%x, puser_data=0x%p",
        pNode->m_bufferDesc.info.presentation_time,
        pNode->m_bufferDesc.info.PTS,
        pNode->m_bufferDesc.info.nfields,
        pNode->m_bufferDesc.info.ulFlags,
        pNode->m_bufferDesc.info.puser_data);

    TRCBL(TRC_ID_BUFFER_DESCRIPTOR);
}


/* end of file */

