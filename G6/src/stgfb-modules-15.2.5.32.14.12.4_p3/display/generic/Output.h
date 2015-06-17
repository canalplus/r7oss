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

#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "stm_display_output.h"
#include "stm_display_plane.h"
#include "DisplayDevice.h"
#include "MetaDataQueue.h"

#define VALID_OUTPUT_HANDLE 0x01240124

class CDisplayDevice;
class CDisplayPlane;
class CDisplayVTG;

/*
 * Control operation return values. Note we are not using errno
 * values internally, in case the mapping needs to change.
 */
enum OutputResults
{
  STM_OUT_OK,
  STM_OUT_NO_CTRL,
  STM_OUT_INVALID_VALUE,
  STM_OUT_BUSY
};

struct stm_display_output_s
{
  COutput  * handle;
  uint32_t   magic;

  struct stm_display_device_s *parent_dev;
};


class COutput
{
public:
  // Construction/Destruction
  COutput(const char     *name,
          uint32_t        id,
          CDisplayDevice* pDev,
          uint32_t        timingID);

  virtual ~COutput(void);

  // Hooks for Device creation and destruction.
  virtual bool Create(void);
  virtual void CleanUp(void);

  // Output Identification
  const char            *GetName(void)         const { return m_name; }
  uint32_t               GetID(void)           const { return m_ID; }
  const CDisplayDevice  *GetParentDevice(void) const { return m_pDisplayDevice; }
  uint32_t               GetTimingID()         const { return m_ulTimingID; }


  // Output Behaviour
  uint32_t GetCapabilities(void) const { return m_ulCapabilities; }

  virtual OutputResults Start(const stm_display_mode_t*);
  virtual bool Stop(void);
  virtual void Suspend(void);
  virtual void Resume(void);
  virtual void PowerDownDACs(void) { m_bDacPowerDown = true; }

  virtual bool HandleInterrupts(void) = 0;

  virtual void UpdateHW();
  virtual TuningResults SetTuning( uint16_t service,
                                   void *inputList,
                                   uint32_t inputListSize,
                                   void *outputList,
                                   uint32_t outputListSize);


  virtual bool CanShowPlane(const CDisplayPlane *);
  virtual bool ShowPlane   (const CDisplayPlane *);
  virtual void HidePlane   (const CDisplayPlane *);

  virtual bool SetPlaneDepth(const CDisplayPlane *, int depth, bool activate);
  virtual bool GetPlaneDepth(const CDisplayPlane *, int *depth) const;

  virtual uint32_t SetControl(stm_output_control_t, uint32_t newVal) = 0;
  virtual uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  virtual uint32_t SetCompoundControl(stm_output_control_t, void *newVal);
  virtual uint32_t GetCompoundControl(stm_output_control_t, void *val) const;

  virtual stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  virtual void FlushMetadata(stm_display_metadata_type_t);

  virtual void SetClockReference(stm_clock_ref_frequency_t, int error_ppm);

  virtual void SoftReset(void);

  virtual bool SetVTGSyncs(const stm_display_mode_t *mode) = 0;

  virtual const stm_display_mode_t* GetModeParamsLine(stm_display_mode_id_t mode) const;
  virtual const stm_display_mode_t* FindMode(uint32_t uXRes, uint32_t uYRes, uint32_t uMinLines, uint32_t uMinPixels, uint32_t uPixClock, stm_scan_type_t) const;

  // Member Accessors
  bool                   IsStarted(void) { return m_bIsStarted; }
  const stm_display_mode_t* GetCurrentDisplayMode(void) const { return m_bIsStarted?&m_CurrentOutputMode:0; }
  stm_time64_t           GetFieldOrFrameDuration(void)  const { return m_fieldframeDuration; }
  static stm_time64_t    GetFieldOrFrameDurationFromMode(const stm_display_mode_t *);

  uint32_t               GetOutputFormat(void)          const { return m_ulOutputFormat;     }

  bool                   GetDisplayStatus(stm_display_output_connection_status_t *status) const;
  void                   SetDisplayStatus(stm_display_output_connection_status_t status);

  stm_display_signal_range_t GetSignalRange(void)       const { return m_signalRange; }

  // VTG Interrupt management for outputs that are timing masters.
  uint32_t     GetCurrentVTGEvent()     const { return m_LastVTGEvent;     }
  stm_time64_t GetCurrentVTGEventTime() const { return m_LastVTGEventTime; }

  /*
   * Management of outputs that are slaved to another output's timing generator
   */
  bool RegisterSlavedOutput(COutput *);
  bool UnRegisterSlavedOutput(COutput *);
  bool SetSlavedOutputsVTGSyncs(const stm_display_mode_t *mode);

protected:
  // Identification
  const char     * m_name;
  uint32_t         m_ID;
  CDisplayDevice * m_pDisplayDevice;
  uint32_t         m_ulTimingID;     // Unique ID of the VTG pacing this output
  uint32_t         m_ulCapabilities;
  void           * m_lock;

  // Control state
  uint32_t         m_ulMaxPixClock;

  stm_display_output_video_source_t m_VideoSource;
  stm_display_output_audio_source_t m_AudioSource;

  uint32_t         m_ulOutputFormat; // RGB, Y/C, CVBS
  bool             m_bForceColor;
  uint32_t         m_uForcedOutputColor; // 0xXXRRGGBB format

  bool             m_bDacPowerDown;
  bool             m_bIsSuspended;
  bool             m_bIsStarted;

  stm_display_mode_t m_CurrentOutputMode;

  stm_display_signal_range_t m_signalRange;

  // Active display mode state
  volatile stm_time64_t            m_fieldframeDuration; // frame rate in real time (us)
  volatile uint32_t                m_LastVTGEvent;
  volatile stm_time64_t            m_LastVTGEventTime;
  volatile stm_display_output_connection_status_t m_displayStatus;

  stm_rational_t                   m_DisplayAspectRatio;

  virtual const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const = 0;
  const stm_display_mode_t* GetSlavedOutputsSupportedMode(const stm_display_mode_t *mode) const;

  /*
   * Management of outputs that are slaved to another output's timing generator
   */
  COutput **m_pSlavedOutputs;

  void StopSlavedOutputs(void);

  /*
   * Called on slaves to inform them of a fast mode change or a change in
   * 3D flags. Can be overridden to provide output specific functionality.
   */
  virtual void UpdateOutputMode(const stm_display_mode_t &) = 0;

private:

  COutput(const COutput&);
  COutput& operator=(const COutput&);
};


/*
 * Mode structure comparisons
 */

inline bool AreModeTimingsIdentical(const stm_display_mode_t &m1, const stm_display_mode_t &m2)
{
  if(m1.mode_timing.pixels_per_line       == m2.mode_timing.pixels_per_line
  && m1.mode_timing.lines_per_frame       == m2.mode_timing.lines_per_frame
  && m1.mode_timing.pixel_clock_freq      == m2.mode_timing.pixel_clock_freq
  && m1.mode_timing.hsync_polarity        == m2.mode_timing.hsync_polarity
  && m1.mode_timing.hsync_width           == m2.mode_timing.hsync_width
  && m1.mode_timing.vsync_polarity        == m2.mode_timing.vsync_polarity
  && m1.mode_timing.vsync_width           == m2.mode_timing.vsync_width
  && m1.mode_params.vertical_refresh_rate == m2.mode_params.vertical_refresh_rate
  && m1.mode_params.scan_type             == m2.mode_params.scan_type)
    return true;

  return false;
}


inline bool AreModeTimingsCompatible(const stm_display_mode_t &m1, const stm_display_mode_t &m2)
{
  if(m1.mode_timing.pixels_per_line  == m2.mode_timing.pixels_per_line
  && m1.mode_timing.lines_per_frame  == m2.mode_timing.lines_per_frame
  && m1.mode_timing.hsync_polarity   == m2.mode_timing.hsync_polarity
  && m1.mode_timing.hsync_width      == m2.mode_timing.hsync_width
  && m1.mode_timing.vsync_polarity   == m2.mode_timing.vsync_polarity
  && m1.mode_timing.vsync_width      == m2.mode_timing.vsync_width
  && m1.mode_params.scan_type        == m2.mode_params.scan_type)
    return true;

  return false;
}


inline bool AreModeActiveAreasIdentical(const stm_display_mode_t &m1, const stm_display_mode_t &m2)
{
  if(m1.mode_params.active_area_start_line  == m2.mode_params.active_area_start_line
  && m1.mode_params.active_area_start_pixel == m2.mode_params.active_area_start_pixel
  && m1.mode_params.active_area_height      == m2.mode_params.active_area_height
  && m1.mode_params.active_area_width       == m2.mode_params.active_area_width)
    return true;

  return false;
}


inline bool AreModesIdentical(const stm_display_mode_t &m1, const stm_display_mode_t &m2)
{
  return AreModeTimingsIdentical(m1,m2) && AreModeActiveAreasIdentical(m1,m2);
}


inline bool AreModesCompatible(const stm_display_mode_t &m1, const stm_display_mode_t &m2)
{
  return AreModeTimingsCompatible(m1,m2) && AreModeActiveAreasIdentical(m1,m2);
}

#endif /* _OUTPUT_H */
