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

#ifndef _DISPLAYPLANE_H
#define _DISPLAYPLANE_H

#include "stm_display_plane.h"
#include "DisplayDevice.h"
#include "DisplayNode.h"
#include "DisplayQueue.h"
#include "DisplayInfo.h"
#include "ControlNode.h"
#include "ListenerNode.h"


#define VALID_PLANE_HANDLE              0x01250125
#define CONTROL_STATUS_ARRAY_MAX_NB     10

class CDisplayDevice;
class CDisplaySource;
class COutput;
class CDisplayQueue;
class CDisplayNode;
class CDisplayInfo;
class CControlNode;
class CListenerNode;

const uint32_t default_node_list_size = 10;


/*
 * Control operation return values. Note we are not using errno
 * values internally, in case the mapping needs to change.
 */
enum DisplayPlaneResults
{
    STM_PLANE_OK,
    STM_PLANE_INVALID_VALUE,
    STM_PLANE_ALREADY_CONNECTED,
    STM_PLANE_CANNOT_CONNECT,
    STM_PLANE_NOT_SUPPORTED,
    STM_PLANE_NO_MEMORY,
    STM_PLANE_EINTR
};

struct stm_display_plane_s
{
  CDisplayPlane * handle;
  uint32_t        magic;

  struct stm_display_device_s   *parent_dev;
};

struct InputFrameInfo {
  stm_rect_t                SrcVisibleArea;
  stm_rational_t            SrcPixelAspectRatio;
  uint32_t                  SrcFlags;
  stm_display_3d_format_t   Src3DFormat;
};

enum DisplayPlaneVisibility
{
    STM_PLANE_ENABLED,  // Enable the plane and the mixer
    STM_PLANE_MASKED,   // The mixer is still pulling the data from the plane (so plane processing are done) but doesn’t blend them.
    STM_PLANE_DISABLED  // The plane is disabled and not contributing to the mixer, no data pulled from the plane.
};

#define STM_PLANE_TRANSPARENCY_OPAQUE 255

struct PlaneStatistics {
    uint32_t PicDisplayed;
    uint32_t PicRepeated;
    uint32_t CurDispPicPTS;                 /* current displaying picture presentation time come with the stream itself */
    uint32_t SourceId;                      /* Id of the source connected to this plane */
    uint32_t FieldInversions;               /* Incremented when an interlaced field is presented on the wrong output polarity */
    uint32_t UnexpectedFieldInversions;     /* Some field inversions are normal, for example in case of Frame Rate Conversion or
                                               slow motion. When STM_BUFFER_SRC_RESPECT_PICTURE_POLARITY is set, no field inversion
                                               is expected to happen. If it happens, this stat will be incremented.
                                             */
};

struct stm_plane_rescale_caps_t
{
  stm_rational_t minHorizontalRescale; /*!< Minimum horizontal rescale factor */
  stm_rational_t maxHorizontalRescale; /*!< Maximum horizontal rescale factor */
  stm_rational_t minVerticalRescale;   /*!< Minimum vertical rescale factor   */
  stm_rational_t maxVerticalRescale;   /*!< Maximum vertical rescale factor   */
};

typedef enum
{
  /*source to display*/
    TOP_TO_TOP,         // 0
    TOP_TO_BOTTOM,      // 1
    TOP_TO_FRAME,       // 2

    BOTTOM_TO_TOP,      // 3
    BOTTOM_TO_BOTTOM,   // 4
    BOTTOM_TO_FRAME,    // 5

    FRAME_TO_TOP,       // 6
    FRAME_TO_BOTTOM,    // 7
    FRAME_TO_FRAME,     // 8

    MAX_USE_CASES
} stm_display_use_cases_t;


typedef struct display_info_s
{
    bool                isDisplayInterlaced;
    stm_display_mode_t  currentMode;
    stm_rect_t          displayVisibleArea;
    uint32_t            display3DModeFlags;

    stm_rational_t      aspectRatio;
    stm_rational_t      pixelAspectRatio;
    stm_time64_t        outputVSyncDurationInUs;
    bool                isOutputStarted;
} output_info_t;

typedef enum
{
  STM_PLANE_OUTPUT_STOPPED   = (1L<<0),   /*!< Master Output is stopped */
  STM_PLANE_OUTPUT_UPDATED   = (1L<<1),   /*!< Master Output is updated */
  STM_PLANE_OUTPUT_STARTED   = (1L<<2),   /*!< Master Output is started */
} stm_plane_update_reason_t;




class CDisplayPlane
{
public:
    // Construction/Destruction
    CDisplayPlane(const char           *name,
                  uint32_t              planeID,
                  const CDisplayDevice *pDev,
                  const stm_plane_capabilities_t Caps,
                  uint32_t              ulNodeListSize = default_node_list_size);

    virtual ~CDisplayPlane(void);
    virtual bool Create(void);

    // Plane Identification
    const char           *GetName(void)         const { return m_name; }
    uint32_t              GetID(void)           const { return m_planeID; }
    const CDisplayDevice *GetParentDevice(void) const { return m_pDisplayDevice; }
    uint32_t              GetTimingID(void)     const { return m_ulTimingID; }
    output_info_t         GetOutputInfo(void)   const { return m_outputInfo; }
    stm_time64_t          GetPlaneLatency(void) const { return m_planeLatency; }

    // Plane Behaviour
    stm_plane_capabilities_t GetCapabilities(void) const { return m_capabilities; }
    int                      GetFormats(const stm_pixel_format_t** pData) const;

    int                      GetListOfFeatures( stm_plane_feature_t *list, uint32_t number);
    virtual bool             IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const;

    virtual DisplayPlaneResults SetControl(stm_display_plane_control_t ctrl, uint32_t newVal);
    virtual DisplayPlaneResults GetControl(stm_display_plane_control_t ctrl, uint32_t *currentVal) const;
    virtual bool GetControlRange(stm_display_plane_control_t selector,
                                 stm_display_plane_control_range_t *range);
    virtual bool GetControlMode(stm_display_control_mode_t *mode);
    virtual bool SetControlMode(stm_display_control_mode_t mode);
    virtual bool ApplySyncControls();

    virtual DisplayPlaneResults SetCompoundControl(stm_display_plane_control_t  ctrl, void * newVal);
    virtual DisplayPlaneResults GetCompoundControl(stm_display_plane_control_t  ctrl, void * currentVal);
    virtual bool GetCompoundControlRange(stm_display_plane_control_t selector,
                                         stm_compound_control_range_t *range);

    // Manage delayed control notification
    DisplayPlaneResults RegisterAsynchCtrlListener(const stm_ctrl_listener_t *listener, int *listener_id);
    DisplayPlaneResults UnregisterAsynchCtrlListener(int listener_id);


    virtual bool GetTuningDataRevision(stm_display_plane_tuning_data_control_t ctrl, uint32_t * revision);
    virtual DisplayPlaneResults GetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * currentVal);
    virtual DisplayPlaneResults SetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * newVal);

    virtual DisplayPlaneResults ConnectToOutput(COutput* pOutput);
    virtual void DisconnectFromOutput(COutput* pOutput);
    virtual int  GetConnectedOutputID(uint32_t *id, uint32_t max_ids);

    virtual bool GetConnectedSourceID(uint32_t *id);
    virtual DisplayPlaneResults ConnectToSource(CDisplaySource *pSource);
    virtual bool DisconnectFromSource(CDisplaySource *pSource);
    virtual int  GetAvailableSourceID(uint32_t *id, uint32_t max_ids);
    virtual DisplayPlaneResults ConnectToSourceProtected(CDisplaySource *pSource);
    virtual bool                DisconnectFromSourceProtected(CDisplaySource *pSource);

    virtual bool Pause(void);
    virtual bool UnPause(void);
    virtual bool HideRequestedByTheApplication(void);
    virtual bool ShowRequestedByTheApplication(void);
    void         UpdatePlaneVisibility(void);
    bool         SetDepth(COutput *pOutput, int32_t depth, bool activate);
    bool         GetDepth(COutput *pOutput, int *depth) const;
    virtual TuningResults SetTuning(uint16_t service,
                                    void *inputList,
                                    uint32_t inputListSize,
                                    void *outputList,
                                    uint32_t outputListSize);

    bool           AreConnectionsChanged(void);
    bool           isOutputModeChanged(void);

    // Member Accessors
    bool        isEnabled(void)                 const { return m_isEnabled; }
    bool        isPaused(void)                  const { return m_isPaused; }
    bool        isVideoPlane(void)              const { return (m_capabilities & PLANE_CAPS_VIDEO); }
    bool        isVbiPlane(void)                const { return (m_capabilities & PLANE_CAPS_VBI); }
    bool        isVisible(void)                 const { return ((m_planeVisibily==STM_PLANE_ENABLED)?true:false); }
    uint32_t    GetStatus(void)                 const { return m_status; }
    bool        hasADeinterlacer(void)          const { return m_hasADeinterlacer; }

    /*
     * Helper method to setup colour keying, note it is static and public
     * so it can be used directly by the video plug class as well as the
     * plane implementations.
     */
    static bool GetRGBYCbCrKey(uint8_t& ucRCr,
                               uint8_t& ucGY,
                               uint8_t& ucBCb,
                               uint32_t ulPixel,
                    stm_pixel_format_t colorFmt,
                               bool     pixelIsRGB);

    virtual bool UpdateFromMixer(COutput* pOutput, stm_plane_update_reason_t update_reason);

    virtual void OutputVSyncThreadedIrqUpdateHW(bool isDisplayInterlaced, bool isTopFieldOnDisplay, const stm_time64_t &vsyncTime);

    // pCurrNode is the node to display at next VSync.
    // pPrevNode and pNextNode are optional and can be set in case of Deinterlacer HW
    virtual void PresentDisplayNode             (CDisplayNode *pPrevNode,
                                                 CDisplayNode *pCurrNode,
                                                 CDisplayNode *pNextNode,
                                                 bool isPictureRepeated,
                                                 bool isDisplayInterlaced,
                                                 bool isTopFieldOnDisplay,
                                                 const stm_time64_t &vsyncTime){};
    virtual void    ProcessLastVsyncStatus      (const stm_time64_t &vsyncTime, CDisplayNode *pNodeDisplayed){};
    virtual bool    NothingToDisplay            (void);

    /*
     * Power Managment stuff
     */
    virtual void Freeze(void);
    virtual void Suspend(void);
    virtual void Resume(void);

    virtual bool RegisterStatistics(void);

protected:
    virtual void EnableHW (void);
    virtual void DisableHW(void);

    virtual bool   PrepareIOWindows                     (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    bool           SetSrcFrameRect                      (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect) const;
    bool           SetDstFrameRect                      (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &dstFrameRect) const;
    bool           IsSrcRectFullyOutOfBounds            (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect) const;
    bool           IsDstRectFullyOutOfBounds            (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &dstFrameRect) const;
    bool           TruncateIOWindowsToLimits            (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect, stm_rect_t &dstFrameRect) const;
    bool           AdjustIOWindowsForAspectRatioConv    (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo, stm_rect_t &srcFrameRect, stm_rect_t &dstFrameRect) const;
    virtual bool   AdjustIOWindowsForHWConstraints      (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo) const;
    virtual bool   IsScalingPossible                    (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    void           FillSelectedPictureDisplayInfoFromSecondary(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    void           FillSelectedPictureDisplayInfoFromPrimary  (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    virtual void   FillSelectedPictureDisplayInfo       (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    void           FillSelectedPictureFormatInfo        (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    uint32_t       GetHorizontalDecimationFactor        (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    uint32_t       GetVerticalDecimationFactor          (CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);

    void            ReleaseRefusedNode                  (CDisplayNode *pNodeToRelease);
    void            ReleaseDisplayNode                  (CDisplayNode *pNodeToRelease, const stm_time64_t &vsyncTime);

    bool            FillDisplayInfo                     (CDisplayNode *pNodeToDisplay, CDisplayInfo *pDisplayInfo);
    bool            FillNodeInfo                        (CDisplayNode *pNode);
    bool            AreSrcCharacteristicsChanged        (CDisplayNode *pNode);

    void            ResetDisplayInfo                    (void);
    void            FillCurrentOuptutInfo              (void);
    void            UpdateOutputInfoSavedInSource       (void);

    stm_display_use_cases_t GetCurrentUseCase           (BufferNodeType srcType, bool isDisplayInterlaced, bool isTopFieldOnDisplay);
    virtual void    ResetEveryUseCases                  (void) { return; };
    virtual bool    IsContextChanged                    (CDisplayNode *pNodeToBeDisplayed, bool isPictureRepeated);
    CDisplayNode *  SelectPictureForNextVSync           (const stm_time64_t &vsyncTime);

    void            UpdatePictureDisplayedStatistics    (CDisplayNode       *pCurrNode,
                                                         bool                isPictureRepeated,
                                                         bool                isDisplayInterlaced,
                                                         bool                isTopFieldOnDisplay);

    /*
     * Called during Flush or Pause with flush to give subclasses a chance to
     * reset any queuing state kept between calls to QueueBuffer.
     */
    virtual void ResetQueueBufferState(void);

    // Useful helpers for setting up hardware
    inline int32_t ValToFixedPoint(int32_t val, unsigned multiple = 1) const;
    inline int FixedPointToInteger(int32_t fp_val, int *frac=0) const;
    inline int LimitSizeToRegion(int start_coord, int invalid_coord, int size) const;

    // Helper to set scaling limits in the capability structure
    void SetScalingCapabilities(stm_plane_rescale_caps_t *caps) const;

    // Reset m_ComputedInputWindowValue and m_ComputedOutputWindowValue to their invalid value
    void ResetComputedInputOutputWindowValues(void);

    // Manage delayed controls
    DisplayPlaneResults AddSimpleControlToQueue(stm_display_plane_control_t ctrl, uint32_t newVal);
    DisplayPlaneResults AddCompoundControlToQueue(stm_display_plane_control_t ctrl, void * newVal);
    DisplayPlaneResults ProcessControlQueue(const stm_time64_t &vsyncTime);
    int ConvertDisplayPlaneResult(const DisplayPlaneResults result);

    const char     * m_name;           // Human readable name of this object
    const CDisplayDevice *m_pDisplayDevice; // Parent Device
    uint32_t         m_numConnectedOutputs;
    COutput        * m_pOutput;        // Currently only one, need to support attachment to multiple mixer outputs.
    CDisplaySource * m_pSource;        // Source attached to this plane.

    uint32_t         m_planeID;        // HW Plane identifier
    uint32_t         m_ulTimingID;     // The timing ID of m_pOutput when connected


    /*** Variables read by the VSync Threaded IRQ and protected by m_vsyncLock ***/
    // To know if the context changed. In that case, all the parameters already computed based on previous pictures
    // must be re-computed to display this new picture.
    bool           m_ContextChanged;

    // Plane aspect ratio conversion mode
    stm_plane_aspect_ratio_conversion_mode_t m_AspectRatioConversionMode;

    // Information about the current display mode and display aspect ratio
    output_info_t       m_outputInfo;

    stm_plane_mode_t    m_InputWindowMode;
    stm_plane_mode_t    m_OutputWindowMode;
    stm_rect_t          m_InputWindowValue;
    stm_rect_t          m_OutputWindowValue;

    // Transparency 0 (transparent) .. 255 (opaque)
    uint32_t            m_Transparency;
    stm_plane_control_state_t m_TransparencyState; /* CONTROL_ON = m_Transparency has precedence over buffer */

    // Queued controls
    CDisplayQueue       m_ControlQueue;

    bool           m_hideRequestedByTheApplication; // Is the application requesting to hide this plane? We can save this request whatever the current plane status
    bool           m_updatePlaneVisibility;         // Request to update plane visibility at next VSync

    int32_t             m_lastRequestedDepth;            // Last requested depth (will be applied at next VSync)
    bool                m_updateDepth;                   // Request to update plane's depth at next VSync

    bool                m_isPlaneActive;

    // Listener parameters
    CDisplayQueue       m_ListenerQueue;

    /**************************************************************************************/


    bool             m_hasADeinterlacer;   // If the plane has a deinterlacer, the Previous field should be kept
                                           // because the deinterlacer will use it as reference

    volatile bool  m_isEnabled;           // Has the HW been enabled
    volatile bool  m_isPaused;       // Don't process any new nodes in the queue
    volatile bool  m_isFlushing;     // Queue is being flushed (aid for SMP safety)
    volatile uint32_t m_status;      // Public view of the current plane status
    bool           m_areConnectionsChanged; // True when some connections to the sources or output have changed. This flag is reset after being read.
    bool           m_isOutputModeChanged;   // True when the output VTG mode has changed. This flag is reset after being read.

    stm_time64_t   m_planeLatency;   // Latency of the display pipe. The timer is started when the first pixel is read
                                     //  in memory and stopped when the same pixel starts to be output by the plane

    stm_plane_ctrl_hide_mode_policy_t  m_eHideMode;     // Hiding mode requested by user throught specifig control
    DisplayPlaneVisibility             m_planeVisibily; // Plane visibility state.

    int32_t        m_fixedpointONE;  // Value of 1 in the n.m fixed point format
                                     // used for hardware rescaling.

    // Min and Max sample rate converter steps for image rescaling
    uint32_t       m_ulMaxHSrcInc;
    uint32_t       m_ulMinHSrcInc;
    uint32_t       m_ulMaxVSrcInc;
    uint32_t       m_ulMinVSrcInc;

    // If the plane has a vertical and horizontal scaler we can be supporting
    // automatic IO Windows.
    bool           m_hasAScaler;

    stm_plane_rescale_caps_t m_rescale_caps;

    // Display plane surface format capabilities
    const stm_pixel_format_t*  m_pSurfaceFormats;
    uint32_t                   m_nFormats;
    stm_plane_capabilities_t   m_capabilities;
    uint32_t                   m_nFeatures;
    const stm_plane_feature_t* m_pFeatures;

    // for GDPs, the code does not deal with this one being overridden
    // by children! (and it makes no sense for GDPs, at least at the moment)
    uint32_t           m_ulMaxLineStep;

    // In AUTO mode, save input and output windows value to return to user
    // (taking into account hardware limits and aspect ratio conversion)
    stm_rect_t         m_ComputedInputWindowValue;
    stm_rect_t         m_ComputedOutputWindowValue;

    // Power Managment state
    bool               m_bIsSuspended; // Has the HW been powered off
    bool               m_bIsFrozen;

    /* Plane statistics */
    struct PlaneStatistics m_Statistics;

    // Listener parameters
    stm_asynch_ctrl_status_t m_ControlStatusArray[CONTROL_STATUS_ARRAY_MAX_NB];
};


inline int32_t CDisplayPlane::ValToFixedPoint(int32_t val, unsigned multiple) const
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


inline int CDisplayPlane::FixedPointToInteger(int32_t fp_val, int *frac) const
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

        TRC( TRC_ID_UNCLASSIFIED, "adjusting size to %d", size );
    }

    return size;
}

#endif
