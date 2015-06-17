/***********************************************************************
 *
 * File: Generic/Output.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "stmdisplayoutput.h"
#include "stmdisplayplane.h"

class CDisplayDevice;
class CDisplayVTG;

class COutput
{
public:
  COutput(CDisplayDevice* pDev, ULONG timingID);
  virtual ~COutput(void);

  virtual ULONG GetCapabilities(void) const = 0;

  virtual bool Start(const stm_mode_line_t*, ULONG tvStandard);
  virtual bool Stop(void);
  virtual void Suspend(void);
  virtual void Resume(void);

  // VTG Interrupt management for outputs that are timing masters.
  stm_field_t GetCurrentVTGVSync()  const { return m_LastVTGSync;   }
  TIME64      GetCurrentVSyncTime() const { return m_LastVSyncTime; }

  // This returns a unique ID for the timing generator pacing this output
  ULONG       GetTimingID()         const { return m_ulTimingID; }

  virtual bool HandleInterrupts(void) = 0;

  // UpdateHW by CDisplayDevice::UpdateDisplay before any planes get updated
  // in interrupt context, for example to update mixer viewports.
  virtual void UpdateHW();

  virtual bool CanShowPlane(stm_plane_id_t planeID) = 0;
  virtual bool ShowPlane   (stm_plane_id_t planeID) = 0;
  virtual void HidePlane   (stm_plane_id_t planeID) = 0;

  virtual bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) = 0;
  virtual bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const = 0;

  virtual ULONG SupportedControls(void) const = 0;
  virtual void  SetControl(stm_output_control_t, ULONG ulNewVal) = 0;
  virtual ULONG GetControl(stm_output_control_t) const = 0;

  virtual stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  virtual void FlushMetadata(stm_meta_data_type_t);

  virtual bool SetFilterCoefficients(const stm_display_filter_setup_t *);

  virtual void SetClockReference(stm_clock_ref_frequency_t, int error_ppm);

  virtual void SoftReset(void);

  const stm_mode_line_t* GetModeParamsLine(stm_display_mode_t mode) const;
  const stm_mode_line_t* FindMode(ULONG ulXRes, ULONG ulYRes, ULONG ulMinLines, ULONG ulMinPixels, ULONG ulpixclock, stm_scan_type_t) const;

  const stm_mode_line_t* GetCurrentDisplayMode(void)    const { return m_pCurrentMode;       }
  ULONG                  GetCurrentTVStandard(void)     const { return m_ulTVStandard;       }
  TIME64                 GetFieldOrFrameDuration(void)  const { return m_fieldframeDuration; }
  static TIME64          GetFieldOrFrameDurationFromMode(const stm_mode_line_t *);

  ULONG                  GetOutputFormat(void)          const { return m_ulOutputFormat;     }

  stm_display_status_t   GetDisplayStatus(void)         const { return m_displayStatus; }
  void                   SetDisplayStatus(stm_display_status_t s) { m_displayStatus = s; }

  stm_display_signal_range_t GetSignalRange(void)       const { return m_signalRange; }

protected:
  CDisplayDevice        *m_pDisplayDevice;

  const stm_mode_line_t * volatile m_pCurrentMode;
  volatile TIME64                  m_fieldframeDuration; /* frame rate in real time (us) */

  volatile stm_field_t          m_LastVTGSync;
  volatile TIME64               m_LastVSyncTime;
  volatile stm_display_status_t m_displayStatus;

  ULONG                  m_ulMaxPixClock;
  ULONG                  m_ulTVStandard;
  ULONG                  m_ulInputSource;  // Main, Aux, Video
  ULONG                  m_ulOutputFormat; // RGB, Y/C, CVBS

  ULONG                  m_ulTimingID;     // Unique ID of the VTG pacing this output

  bool                   m_bIsSuspended;

  stm_display_signal_range_t m_signalRange;

  virtual const stm_mode_line_t* SupportedMode(const stm_mode_line_t *) const;
};

#endif /* _OUTPUT_H */
