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

#ifndef _DISPLAYSOURCE_H
#define _DISPLAYSOURCE_H

#include "stm_display_source.h"
#include "DisplayDevice.h"

#define VALID_SOURCE_HANDLE             0x01260126

class CDisplayDevice;
class CSourceInterface;
class CPixelStreamInterface;
class CQueueBufferInterface;
class CDisplaySource;
class CDisplayPlane;

/*
 * Control operation return values. Note we are not using errno
 * values internally, in case the mapping needs to change.
 */
enum SourceResults
{
  STM_SRC_OK,
  STM_SRC_NO_CTRL,
  STM_SRC_INVALID_VALUE,
  STM_SRC_BUSY
};

struct stm_display_source_s
{
  CDisplaySource * handle;
  uint32_t         magic;

  struct stm_display_device_s * parent_dev;
};

typedef struct stm_display_source_queue_s
{
  CQueueBufferInterface * handle;
  uint32_t                magic;

  struct stm_display_device_s * parent_dev;
} stm_display_source_queue_t;

typedef struct stm_display_source_pixelstream_s
{
  CPixelStreamInterface * handle;
  uint32_t                magic;

  struct stm_display_source_s * owner;
  struct stm_display_device_s * parent_dev;
} stm_display_source_pixelstream_t;


struct SourceStatistics {
    uint32_t PicOwned;
    uint32_t PicReleased;
    uint32_t PicQueued;
};

class CDisplaySource
{
public:
    // Construction/Destruction
    CDisplaySource(const char           *name,
                   uint32_t              sourceID,
                   const CDisplayDevice *pDev,
                   uint32_t              maxNumConnectedPlanes = 1);

    virtual ~CDisplaySource(void);
    bool     Create(void);

    // Source Identification
    const char           *GetName(void)                     const { return m_name; }
    uint32_t              GetID(void)                       const { return m_sourceID; }
    const CDisplayDevice *GetParentDevice(void)             const { return m_pDisplayDevice; }
    const uint32_t        GetNumConnectedPlanes(void)       const { return m_numConnectedPlanes; }      // Actual number of connected planes
    const uint32_t        GetMaxNumConnectedPlanes(void)    const { return m_maxNumConnectedPlanes; }   // Max number of connected planes
    uint32_t              GetStatus(void)                   const { return m_Status; }

    bool                  hasADeinterlacer(void)            const { return m_hasADeinterlacer; }
    bool                  isUsedByVideoPlanes(void)         const { return m_isUsedByVideoPlanes; }

    stm_display_timing_events_t GetCurrentVSync(void)       const { return m_LastSync;   }
    stm_time64_t                GetCurrentVSyncTime(void)   const { return m_LastVSyncTime; }

    void*                 GetCtrlRegisterBase(void)         const { return m_pDisplayDevice->GetCtrlRegisterBase(); }

    CSourceInterface     *GetInterface(const stm_display_source_interfaces_t);
    uint32_t              GetTimingID(void) const;                      // Get the unique ID of the clock pacing this source (if any)

    int32_t               GetSourceInterfaceHwId(void);

    int                   GetConnectedPlaneID(uint32_t *, uint32_t);    // Get the PlaneIds of all the planes connected to this source.
    int                   GetConnectedPlaneCaps(stm_plane_capabilities_t *caps, uint32_t max_caps);

    virtual uint32_t      GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const;
    virtual uint32_t      SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val);

    void                  SourceVSyncUpdateHW (void);
    void                  OutputVSyncThreadedIrqUpdateHW (bool isdisplayInterlaced, bool isTopFieldOnDisplay, const stm_time64_t &vsyncTime);

    bool                  CanConnectToPlane(CDisplayPlane *pPlane);
    bool                  ConnectPlane(CDisplayPlane *);
    bool                  DisconnectPlane(CDisplayPlane *);
    bool                  RegisterInterface(CSourceInterface *);
    bool                  HandleInterrupts(void);
    void                  SetStatus(stm_display_status_t status, bool set);      // set the status of this source

    virtual int           GetCapabilities(stm_display_source_caps_t *caps);
    virtual TuningResults SetTuning(uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize);

    /*
     * Power Managment stuff
     */
    virtual void Freeze(void);
    virtual void Suspend(void);
    virtual void Resume(void);

protected:
    void                   UpdateHasDeinterlacer(void);

    const char            *m_name;               // Human readable name of this object

    /*
    * An array of connected planes to this source.
    * This array will be updated when connecting a plane to this source
    * by calling stm_display_plane_connect_to_source API.
    */
    CDisplayPlane        **m_pConnectedPlanes;      // List of planes connected to this source
    uint32_t               m_numConnectedPlanes;    // Actual number of planes connected to this source
    uint32_t               m_maxNumConnectedPlanes; // Max number of planes that can be connected to this source

    CSourceInterface      *m_pSourceInterface;

    uint32_t               m_sourceID;              // SW Source identifier
    const CDisplayDevice  *m_pDisplayDevice;        // Parent Device// access to the plane's buffer queue.
    void                  *m_owner;                 // Token indicating the owner that has exclusive
    volatile stm_display_timing_events_t   m_LastSync;
    volatile stm_time64_t  m_LastVSyncTime;
    volatile uint32_t      m_Status;                // Public view of the current source status

    stm_display_source_caps_t m_capabilities;

    bool                   m_hasADeinterlacer;      // Indicates if there are at least a deinterlacer plane connected to this source
    bool                   m_isUsedByVideoPlanes;   // Indicates if there are at least a video plane plane connected to this source
};

#endif /* _DISPLAYSOURCE_H */
