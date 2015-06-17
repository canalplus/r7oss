/***********************************************************************
 *
 * File: Generic/DisplayPlane.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _DISPLAYPLANE_H
#define _DISPLAYPLANE_H

#include "stmdisplayplane.h"

class CDisplayDevice;
class COutput;

typedef enum { GNODE_BOTTOM_FIELD, GNODE_TOP_FIELD, GNODE_PROGRESSIVE } stm_plane_node_type;

struct stm_plane_node
{
  DMA_Area dma_area;

  stm_buffer_presentation_t info;
  stm_plane_node_type       nodeType;

  bool           useAltFields;
  volatile bool  isValid;
};


const ULONG default_node_list_size = 10;


class CDisplayPlane
{
public:
    CDisplayPlane(stm_plane_id_t planeID, ULONG ulNodeListSize = default_node_list_size);
    virtual ~CDisplayPlane(void);

    virtual bool Create(void);

    virtual bool ConnectToOutput(COutput* pOutput);
    virtual void DisconnectFromOutput(COutput* pOutput);

    stm_plane_id_t GetID(void)       const { return m_planeID; }
    ULONG          GetTimingID(void) const { return m_ulTimingID; }
    ULONG          GetStatus(void)   const { return m_ulStatus; }

    virtual bool LockUse(void *user);
    virtual void Unlock (void *user);

    virtual bool QueueBuffer(const stm_display_buffer_t * const pBufferInfo,
                             const void                 * const user) = 0;

    virtual void Pause(bool bFlushQueue);
    virtual void Resume(void);
    virtual void Flush(void);

    bool isEnabled(void) const { return m_isEnabled; }
    bool isActive(void)  const { return m_isActive; }
    bool isPaused(void)  const { return m_isPaused; }

    virtual stm_plane_caps_t GetCapabilities(void) const = 0;

    virtual int GetFormats(const SURF_FMT** pData) const = 0;

    virtual bool SetControl(stm_plane_ctrl_t, ULONG);
    virtual bool GetControl(stm_plane_ctrl_t, ULONG *) const;

    virtual bool Hide(void);
    virtual bool Show(void);

    bool SetDepth(COutput *pOutput, int depth, bool activate);
    bool GetDepth(COutput *pOutput, int *depth) const;

    /*
     * Helper method to setup colour keying, note it is static and public
     * so it can be used directly by the video plug class as well as the
     * plane implementations.
     */
    static bool GetRGBYCbCrKey(UCHAR& ucRCr,
                               UCHAR& ucGY,
                               UCHAR& ucBCb,
                               ULONG ulPixel,
                               SURF_FMT colorFmt,
			       bool     pixelIsRGB);


protected:
    virtual void EnableHW (void);
    virtual void DisableHW(void);
    virtual bool AllocateNodeMemory(stm_plane_node &node,
                                    ULONG           ulSize,
                                    ULONG           alignment      = 0,
                                    STM_DMA_AREA_ALLOC_FLAGS flags = SDAAF_NONE)
    {
      g_pIOS->AllocateDMAArea(&node.dma_area, ulSize, alignment, flags);

      return (node.dma_area.pMemory != 0);
    }

    virtual void ReleaseNode(stm_plane_node &node);

    /*
     * Called during Flush or Pause with flush to give subclasses a chance to
     * reset any queuing state kept between calls to QueueBuffer.
     */
    virtual void ResetQueueBufferState(void);

    // Useful helpers for setting up hardware
    inline LONG ValToFixedPoint(LONG val, int multiple = 1) const;
    inline int FixedPointToInteger(LONG fp_val, int *frac=0) const;
    inline int LimitSizeToRegion(int start_coord, int invalid_coord, int size) const;

    // Internal buffer queue management
    bool AddToDisplayList(const stm_plane_node      &newFrame);
    bool GetNextNodeFromDisplayList(stm_plane_node  &frame);
    bool PeekNextNodeFromDisplayList(const TIME64 &vsyncTime, stm_plane_node &frame);
    bool PopNextNodeFromDisplayList(void);

    // Management of the nodes on display
    void UpdateCurrentNode(const TIME64 &vsyncTime);
    void SetPendingNode(stm_plane_node &pendingFrame);

    // Helper to set scaling limits in the capability structure
    void SetScalingCapabilities(stm_plane_caps_t *caps) const;

    COutput       *m_pOutput;    // Currently only one, need to support
                                 // attachment to multiple mixer outputs.
    stm_plane_id_t m_planeID;    // HW Plane identifier
    ULONG          m_ulTimingID; // The timing ID of m_pOutput when connected
    ULONG          m_lock;       // Queue lock
    void          *m_user;       // Token indicating the user that has exclusive
                                 // access to the plane's buffer queue.
    bool           m_keepHistoricBufferForDeinterlacing;
    bool           m_isEnabled;  // has the HW been enabled
    volatile bool  m_isActive;   // Has nodes to process or still on display
    volatile bool  m_isPaused;   // Don't process any new nodes in the queue
    volatile ULONG m_ulStatus;   // Public view of the current plane status

    LONG           m_fixedpointONE; // Value of 1 in the n.m fixed point format
                                    // used for hardware rescaling.

    // Min and Max sample rate converter steps for image rescaling
    ULONG          m_ulMaxHSrcInc;
    ULONG          m_ulMinHSrcInc;
    ULONG          m_ulMaxVSrcInc;
    ULONG          m_ulMinVSrcInc;

    // for GDPs, the code does not deal with this one being overridden
    // by children! (and it makes no sense for GDPs, at least at the moment)
    ULONG          m_ulMaxLineStep;

    // The node currently on display
    stm_plane_node m_currentNode;

    // The node that has been programmed on the hardware but
    // will not be displayed until the next VSync
    stm_plane_node m_pendingNode;

    // The previous node on display, but may still be referenced
    // for de-interlacing purposes.
    stm_plane_node m_previousNode;

    // Queued nodes
    ULONG m_writerPos;
    ULONG m_readerPos;

    ULONG m_ulNodeEntries;
    stm_plane_node *m_pNodeList;
    DMA_Area m_NodeListArea;

};


inline LONG CDisplayPlane::ValToFixedPoint(LONG val, int multiple) const
{
    /*
     * Convert integer val to n.m fixed point defined by the value of
     * m_fixedpointONE. The result is then divded by multiple, allowing the
     * direct conversion of for example N 16ths to a fixed point number
     * without the calling worrying about 32bit overflows. This is used
     * in resize filter setup calculations.
     */
    LONGLONG tmp = ((LONGLONG)val * m_fixedpointONE)/multiple;
    return (long)tmp;
}


inline int CDisplayPlane::FixedPointToInteger(LONG fp_val, int *frac) const
{
    int integer = fp_val / m_fixedpointONE;
    if(frac)
        *frac = fp_val - (integer * m_fixedpointONE);

    return integer;
}


inline int CDisplayPlane::LimitSizeToRegion(int start_val, int max_val, int size) const
{
    if ((start_val + size) > max_val)
    {
        size = max_val - start_val + 1;
        if(size<0)
            size = 0;

        DEBUGF2 (3, ("%s: adjusting size to %d\n", __PRETTY_FUNCTION__, size));
    }

    return size;
}

#endif
