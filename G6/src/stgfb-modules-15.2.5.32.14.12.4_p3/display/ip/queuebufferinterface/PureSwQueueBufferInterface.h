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

#ifndef PURE_SW_QUEUE_BUFFER_INTERFACE_H
#define PURE_SW_QUEUE_BUFFER_INTERFACE_H


#include <display/generic/QueueBufferInterface.h>


///////////////////////////////////////////////////////////////////////////////
// Base class for G2 and G3 compositor queue buffer source interface

typedef struct display_triplet_s
{
    CDisplayNode      *pPrevNode;
    CDisplayNode      *pCurNode;
    CDisplayNode      *pNextNode;
} display_triplet_t;

class CPureSwQueueBufferInterface: public CQueueBufferInterface
{
public:
    CPureSwQueueBufferInterface ( uint32_t interfaceID, CDisplaySource * pSource, bool isConnectionToVideoPlaneAllowed = true );
    virtual ~CPureSwQueueBufferInterface();

    bool                                Create(void);

    virtual uint32_t                    GetTimingID(void) const { return m_ulTimingID; }

    virtual void                        UpdateOutputInfo(CDisplayPlane *pPlane);


    virtual bool                        QueueBuffer(const stm_display_buffer_t * const ,
                                                    const void                 * const );

    virtual bool                        Flush(bool bFlushCurrentNode,
                                              const void * const user,
                                              bool bCheckUser);

    virtual bool                        LockUse(void *user, bool force);
    virtual void                        Unlock(void *user);

    int                                 GetBufferFormats(const stm_pixel_format_t **) const;
    bool                                IsConnectionPossible(CDisplayPlane *pPlane) const;

    virtual void                        OutputVSyncThreadedIrqUpdateHW(bool isDisplayInterlaced, bool isTopFieldOnDisplay, const stm_time64_t &vsyncTime);


protected:
    bool                                m_isConnectionToVideoPlaneAllowed;
    void                               *m_user;                     // Token indicating the user of this plane

    bool                                m_displayNextPictASAP;      // Boolean used only in case of SWAP use case. During the transition, 2 playstreams are connected to the same plane.
                                                                    // The previous playstream is detached while it is not yet time to display the first picture of the new playstream.
                                                                    // To avoid a blue screen, we force the display of the first picture of the new stream.

    CDisplayQueue                       m_displayQueue;
    CDisplayQueue                       m_picturesRefusedQueue;     // Special queue used to temporary store the pictures received from an invalid user.
                                                                    // This queue is flushed at every VSync

    display_triplet_t                   m_picturesPreparedForNextVSync;
    display_triplet_t                   m_picturesUsedByHw;

    uint32_t                            m_IncomingPictureCounter;   // Free running counter incremented each time that a new picture is queued
    stm_time64_t                        m_lastPresentationTime;

    stm_display_latency_params_t        *m_LatencyParams;
    uint32_t                            *m_LatencyPlaneIds;

    uint32_t                            m_ulTimingID;               // The timing ID of the VTG pacing the planes connected to this source
    output_info_t                       m_outputInfo;               // Information about the current display mode and display aspect ratio


    void FlushPicturesRefusedQueue      (void);
    bool FlushAllNodesFromUser          (bool bFlushCurrentNode, const void * const user);
    void SendDisplayCallback            (CDisplayNode *pNode, const stm_time64_t &vsyncTime);
    CDisplayNode * SelectPictureForNextVSync(bool                 isDisplayInterlaced,
                                              bool                isTopFieldOnDisplay,
                                              const stm_time64_t &vsyncTime);
    bool AreSrcCharacteristicsChanged   (CDisplayNode *pNode);
    bool FillSrcPictureType             (CDisplayNode *pNode);
    bool FillNodeInfo                   (CDisplayNode *pNode);
    void SetPreviousNode                (CDisplayNode *pNode, const stm_time64_t &vsyncTime);
    void ReleaseRefusedNode             (CDisplayNode *pNodeToRelease);
    void ReleaseDisplayNode             (CDisplayNode *pNodeToRelease, const stm_time64_t &vsyncTime);
    void ReleaseNodesNoMoreNeeded       (const stm_time64_t & vsyncTime);

    CDisplayNode*   GetPreviousNode     (CDisplayNode *pCurNode);
    void            ResetTriplet        (display_triplet_t *pTriplet);

    void ProcessLastVsyncStatus         (const stm_time64_t &vsyncTime, CDisplayNode *pNodeDisplayed);
    void NothingToDisplay               (void);
    void PresentDisplayNode             (CDisplayNode *pPrevNode,
                                         CDisplayNode *pCurrNode,
                                         CDisplayNode *pNextNode,
                                         bool isPictureRepeated,
                                         bool isDisplayInterlaced,
                                         bool isTopFieldOnDisplay,
                                         const stm_time64_t &vsyncTime);
    uint32_t                             FillParamLatencyInfo(uint16_t *output_change_status);

    void PrintBufferDescriptor          (const CDisplayNode *pNode);

    void GrantFrameBufferAccess(const stm_display_buffer_t* const pFrame);
    void RevokeFrameBufferAccess(const stm_display_buffer_t* const pFrame);

private:
    CPureSwQueueBufferInterface(const CPureSwQueueBufferInterface&);
    CPureSwQueueBufferInterface& operator=(const CPureSwQueueBufferInterface&);
    stm_display_buffer_t        m_currentPictureCharact;

};

#endif /* PURE_SW_QUEUE_BUFFER_INTERFACE_H */

