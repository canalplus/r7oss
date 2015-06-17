/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
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

#ifndef _PIXEL_CAPTURE_H__
#define _PIXEL_CAPTURE_H__

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include "stm_pixel_capture.h"
#include <capture_device_priv.h>

#include "Queue.h"

#include <stm_data_interface.h>

#define CAPTURE_MAX_NODES_VALUE     10

#define CAPTURE_NB_BUFFERS          2

/* Invalid CTL register value */
#define CAPTURE_HW_INVALID_CTL       0xFFFFFFFF

typedef struct stm_pixel_capture_input_s {
  const char   *input_name;
  unsigned int  input_id;
  unsigned int  pipeline_id;
} stm_pixel_capture_inputs_t;

struct CaptureStatistics {
    uint32_t PicVSyncNb;
    uint32_t PicQueued;
    uint32_t PicReleased;
    uint32_t PicInjected;
    uint32_t PicCaptured;
    uint32_t PicRepeated;
    uint32_t PicSkipped;
    uint32_t PicPushedToSink;
    uint32_t CurCapPicPTS;
};

/*
 * The following structure is a "memo" carried around during
 * a QueueBuffer operation. A lot of information and basic
 * setup logic can be shared regardless of the underlying
 * hardware implementation.
 */

struct CaptureQueueBufferInfo
{
  bool                            isInputInterlaced;
  bool                            isInputRGB;
  bool                            isInput422;

  stm_pixel_capture_rect_t        InputVisibleArea;                 // Input source visible area
  stm_pixel_capture_rational_t    InputPixelAspectRatio;            // Input source pixel aspect ratio

  stm_pixel_capture_rect_t        InputFrameRect;                   // Input source rectangle in Frame coordinates (x and y are in sixteenth of pixel unit)
  uint32_t                        InputHeight;                      // Input source height in Field or Frame coordinates (depending of source ScanType)

  stm_pixel_capture_rect_t        BufferFrameRect;                  // Buffer rectangle in Frame coordinates
  uint32_t                        BufferHeight;                     // Buffer height in Field or Frame coordinates (depending of buffer ScanType)

  stm_pixel_capture_buffer_format_t BufferFormat;

  /*
   * Basic information from system and buffer presentation flags
   */
  bool                            isBufferInterlaced;

  stm_pixel_capture_flags_t       firstFieldType;

  bool                            firstFieldOnly;
  bool                            interpolateFields;

  /*
   * Scaling information conversion increments
   */
  int32_t                         InputFrameRectFixedPointX;  //srcFrame x and y values in n.13 notation
  int32_t                         InputFrameRectFixedPointY;

  uint32_t                        verticalFilterInputSamples;
  uint32_t                        verticalFilterOutputSamples;
  uint32_t                        horizontalFilterOutputSamples;
  int                             maxYCoordinate;

  uint32_t                        hsrcinc;
  uint32_t                        chroma_hsrcinc;
  uint32_t                        vsrcinc;
  uint32_t                        chroma_vsrcinc;
  uint32_t                        line_step;

  /*
   * Non-linear scaling
   */
  uint32_t                        nlLumaStartInc;
  uint32_t                        nlChromaStartInc;
};

class CPixelCaptureDevice;
class CCaptureQueue;

class CCapture
{
public:
    // Construction/Destruction
    CCapture(const char                         *name,
                     uint32_t                               id,
                     const CPixelCaptureDevice             *pCaptureDevice,
                     const stm_pixel_capture_capabilities_flags_t   caps);

    virtual ~CCapture(void);

    // Capture Identification
    const char                         *GetName(void)                 const { return m_name; }
    uint32_t                            GetID(void)                   const { return m_ID; }
    const CPixelCaptureDevice          *GetParentCaptureDevice(void)  const { return m_pCaptureDevice; }
    uint32_t                            GetStatus(void)               const { return m_Status; }
    uint32_t                            GetTimingID()                 const { return m_ulTimingID; }
    virtual int                         GetFormats(const stm_pixel_capture_format_t** pFormats) const;
    bool                                IsAttached(void)              const { return (m_Sink ? true : false); }

    stm_pixel_capture_buffer_format_t   GetCaptureFormat(void)        const { return m_CaptureFormat; }
    bool                                GetQueueBufferInfo(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                                                      CaptureQueueBufferInfo              &qbinfo);
    virtual bool                        PrepareAndQueueNode(const stm_pixel_capture_buffer_descr_t* const pBuffer,
                                                                      CaptureQueueBufferInfo             &qbinfo) = 0;
    virtual void                        AdjustBufferInfoForScaling(const stm_pixel_capture_buffer_descr_t * const pBuffer,
                                                                      CaptureQueueBufferInfo              &qbinfo) const;

    bool                                Create(void);
    void                                CleanUp(void);
    void                                SetStatus(stm_pixel_capture_status_t status, bool set);      // set the status of this capture
    bool                                IsFormatSupported(const stm_pixel_capture_buffer_format_t format);
    int                                 SetCaptureFormat(const stm_pixel_capture_buffer_format_t  format);

    virtual int                         GetCapabilities(stm_pixel_capture_capabilities_flags_t *caps);
    virtual int                         GetInputParams(stm_pixel_capture_input_params_t *params);
    virtual int                         GetStreamParams(stm_pixel_capture_params_t *params);
    virtual int                         GetInputWindow(stm_pixel_capture_rect_t *input_window);
    virtual int                         GetInputWindowCaps(stm_pixel_capture_input_window_capabilities_t *input_window_capability);

    // Access to Captures inputs
    virtual int                         SetCurrentInput(uint32_t input);
    virtual int                         GetCurrentInput(uint32_t *input);
    virtual int                         GetCaptureName(const uint32_t input, const char **name);
    virtual int                         GetAvailableInputs(uint32_t *input, uint32_t max_inputs);

    virtual int                         SetInputWindow(stm_pixel_capture_rect_t input_window);
    virtual int                         SetInputParams(stm_pixel_capture_input_params_t params);
    virtual int                         SetStreamParams(stm_pixel_capture_params_t params);

    virtual int                         Start(void);
    virtual int                         Stop(void);
    virtual int                         Attach(const stm_object_h sink);
    virtual int                         Detach(const stm_object_h sink);

    void                                GrantCaptureBufferAccess(const stm_pixel_capture_buffer_descr_t *const pBuffer);
    void                                RevokeCaptureBufferAccess(const stm_pixel_capture_buffer_descr_t *const pBuffer);
    virtual int                         QueueBuffer(stm_pixel_capture_buffer_descr_t *pBuffer,
                                                         const void                * const user);
    virtual int                         ReleaseBuffer(stm_pixel_capture_buffer_descr_t *pBuffer) = 0;

    virtual void                        CaptureUpdateHW(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent) = 0;
    virtual void                        ProcessNewCaptureBuffer(stm_pixel_capture_time_t vsyncTime, uint32_t timingevent) = 0;
    virtual int                         GetCaptureNode(capture_node_h &capNode, bool free_node);
    virtual int                         PushCapturedNode(capture_node_h &capNode, stm_pixel_capture_time_t vsyncTime, int content_is_valid, bool release_node);

    virtual void                        Flush(bool bFlushCurrentNode) = 0;

    virtual bool                        SetPendingNode(capture_node_h &pendingNode);
    virtual void                        UpdateCurrentNode(const stm_pixel_capture_time_t &vsyncTime);

    virtual bool                        LockUse(void *user);
    virtual void                        Unlock (void *user);

    virtual int                         NotifyEvent(capture_node_h pNode, stm_pixel_capture_time_t vsyncTime);

    // Member Accessors
    bool isStarted(void)           const { return m_isStarted; }
    bool hasBuffersToRelease(void) const { return m_hasBuffersToRelease; }

    /*
    * Power Management stuff.
    */
    virtual void Freeze(void);
    virtual void Suspend(void);
    virtual void Resume(void);

    /*
     * Statics
     */
    virtual bool RegisterStatistics(void);

    /*
     * Interrupt handler.
     */
    virtual int HandleInterrupts(uint32_t *timingevent);

private:
    void CalculateHorizontalScaling(CaptureQueueBufferInfo &qbinfo) const;
    void CalculateVerticalScaling(CaptureQueueBufferInfo &qbinfo) const;
    int  InitializeEvent(void);
    int  TerminateEvent(void);

protected:
    const char                         *m_name;               // Human readable name of this object
    uint32_t                            m_ID;                 // SW Capture identifier
    const CPixelCaptureDevice          *m_pCaptureDevice;     // Parent Pixel Capture access to the input.
    void                               *m_user;               // Token indicating the user that has exclusive access to the capture's buffer queue
    void                              * m_lock;               // Capture device access locking
    void                              * m_QueueLock;          // Lock taken when removing nodes in m_pCaptureQueue or using m_currentNode and m_pendingNode
    // IO mapped pointer to the start of the register block
    uint32_t* m_pReg;

    // Capture parameters
    stm_pixel_capture_capabilities_flags_t      m_Capabilities;         // Capture Capabilities
    stm_pixel_capture_buffer_format_t           m_CaptureFormat;        // Capture output format
    stm_pixel_capture_input_params_t            m_InputParams;          // Capture Input parameters
    stm_pixel_capture_params_t                  m_StreamParams;         // Stream parameters
    volatile uint32_t                           m_Status;               // Public view of the current capture status
    volatile bool                               m_isStarted;            // Has the HW been started
    volatile bool                               m_hasBuffersToRelease;  // Has nodes to process or still on display
    uint32_t                                    m_NumberOfQueuedBuffer; // Number of queued buffer

    // Capture inputs
    const stm_pixel_capture_inputs_t*           m_pCaptureInputs;       // Available capture inputs
    stm_pixel_capture_inputs_t                  m_pCurrentInput;        // Current capture inputs
    uint32_t                                    m_numInputs;            // Number of Captures Inputs available

    /* Capture CTL register value for last programmed node */
    uint32_t                                    m_ulCaptureCTL;           // Used to avoid programming same CTL register for each VSync.
    bool                                        m_AreInputParamsChanged;  // Used to program the SYCHRO block just when enabling IRQ

    // Capture surface format capabilities
    const stm_pixel_capture_format_t*           m_pSurfaceFormats;
    uint32_t                                    m_nFormats;

    uint32_t                                        m_ulTimingID;       // Unique ID of the VTG pacing this capture
    stm_pixel_capture_rect_t                        m_InputWindow;
    stm_pixel_capture_input_window_capabilities_t   m_InputWindowCaps;

    int32_t        m_fixedpointONE;  // Value of 1 in the n.m fixed point format
                                     // used for hardware rescaling.

    // Min and Max sample rate converter steps for image rescaling
    uint32_t       m_ulMaxHSrcInc;
    uint32_t       m_ulMinHSrcInc;
    uint32_t       m_ulMaxVSrcInc;
    uint32_t       m_ulMinVSrcInc;

    // The max line stepping for vertical resize
    uint32_t       m_ulMaxLineStep;

    // The node currently begin processed
    capture_node_h m_currentNode;

    // The node that has been programmed on the hardware but
    // will not be processed until the next VSync
    capture_node_h m_pendingNode;

    // The previous processed node
    capture_node_h m_previousNode;

    /* Capture Queued nodes */
    CCaptureQueue                              *m_CaptureQueue;

    /* Tunneling support */
    stm_object_h                                m_Sink;
    stm_data_interface_push_get_sink_t          m_PushGetInterface;
    stm_pixel_capture_time_t                    m_FrameDuration;
    // Power Managment state
    bool                                        m_bIsSuspended; // Has the HW been powered off
    bool                                        m_bIsFrozen;

    /* Plane statistics */
    struct CaptureStatistics                    m_Statistics;
    stm_pixel_capture_time_t                    m_lastPresentationTime;

    // Capture event data
    stm_event_t                         m_CaptureEvent;         // Capture event
    stm_event_subscription_h            m_EventSubscription;    // Capture event subscription
    VIBE_OS_WaitQueue_t                 m_WaitQueueEvent;       // Queue Event

    virtual bool                 CheckInputWindow(stm_pixel_capture_rect_t  input_window);
    bool                         CheckStreamParams(stm_pixel_capture_params_t params);
    bool                         CheckInputParams(stm_pixel_capture_input_params_t params);

    // Useful helpers for setting up hardware
    inline int32_t ValToFixedPoint(int32_t val, unsigned multiple = 1) const;
    inline int FixedPointToInteger(int32_t fp_val, int *frac=0) const;
    inline int LimitSizeToRegion(int start_coord, int invalid_coord, int size) const;

    void WriteDevReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg , reg, val); }
    uint32_t ReadDevReg(uint32_t reg) { return vibe_os_read_register(m_pReg , reg); }
};

inline int32_t CCapture::ValToFixedPoint(int32_t val, unsigned multiple) const
{
    /*
     * Convert integer val to n.m fixed point defined by the value of
     * m_fixedpointONE. The result is then divided by multiple, allowing the
     * direct conversion of for example N 16ths to a fixed point number
     * without the calling worrying about 32bit overflows. This is used
     * in resize filter setup calculations.
     *
     * Note that we have to take care with negative values as we only
     * have an unsigned 64bit divide available on Linux through the OS
     * abstraction.
     */
    int32_t tmp = 1;
    if(val < 0)
    {
      tmp = -1;
      val = -val;
    }
    tmp *= (int32_t)vibe_os_div64(((uint64_t)val * m_fixedpointONE), multiple);
    return tmp;
}


inline int CCapture::FixedPointToInteger(int32_t fp_val, int *frac) const
{
    int integer = fp_val / m_fixedpointONE;
    if(frac)
        *frac = fp_val - (integer * m_fixedpointONE);

    return integer;
}


inline int CCapture::LimitSizeToRegion(int start_val, int max_val, int size) const
{
    if ((start_val + size) > max_val)
    {
        size = max_val - start_val + 1;
        if(size<0)
            size = 0;

        TRC( TRC_ID_UNCLASSIFIED, "adjusting size to %d", size );
    }

    return size;
}

#endif /* _PIXEL_CAPTURE_H__ */
