/***********************************************************************
 *
 * File: display/ip/hdf/stmhdf.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDF_H
#define _STM_HDF_H

#include <stm_display.h>
#include <display/generic/Output.h>

#include <display/ip/analogsync/hdf/stmhdfsync.h>

/* AWG Delay */
/*
 * The AWG introduces a delay of some pixels:
 *   sync to external syncs (VTG): 3 clocks
 *   instruction fetch+execute   : 2 clocks
 *
 * However, the specification for the block appears to contradict itself and
 * also states that the delay from program start to the first pixel entering
 * the filter block is only 4 clocks. This value seems to work better when it
 * comes to getting all the pixels on the screen with a HD TV in exact scan
 * mode, for 1080i and 720p.
 *
 *   filter delay                : 5   clocks for HD
 *                                 4.5 clocks for ED
 *                                 3.5 clocks for SD
 *
 */
#define AWG_FILTER_DELAY_HD       (3)
#define AWG_FILTER_DELAY_ED       (2)
#define AWG_FILTER_DELAY_SD       (1)
#define AWG_FILTER_DELAY_DISABLED (0)

#define AWG_DELAY_HD        (-(AWG_FILTER_DELAY_DISABLED+AWG_FILTER_DELAY_HD))
#define AWG_DELAY_ED        (-(AWG_FILTER_DELAY_DISABLED+AWG_FILTER_DELAY_ED))
#define AWG_DELAY_SD        (-(AWG_FILTER_DELAY_DISABLED+AWG_FILTER_DELAY_SD))
#define AWG_DELAY_NO_FILTER (-(AWG_FILTER_DELAY_DISABLED+AWG_FILTER_DELAY_DISABLED))


typedef enum
{
  HDF_COEFF_PHASE1_123,
  HDF_COEFF_PHASE1_456,
  HDF_COEFF_PHASE1_7,
  HDF_COEFF_PHASE2_123,
  HDF_COEFF_PHASE2_456,
  HDF_COEFF_PHASE3_123,
  HDF_COEFF_PHASE3_456,
  HDF_COEFF_PHASE4_123,
  HDF_COEFF_PHASE4_456,
} stm_hdf_coeff_phases_t;


class COutput;

class CSTmHDFormatter
{
public:
  CSTmHDFormatter(CDisplayDevice      *pDev,
                  uint32_t             TVFmt);

  virtual ~CSTmHDFormatter();

  virtual bool Create(void);

  const COutput *GetOwner(void) const { return m_pOwner; }

  bool Start(const COutput *pOwner, const stm_display_mode_t*);
  void Stop(void);

  virtual void Resume(void);

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal);
  uint32_t GetCompoundControl(stm_output_control_t ctrl, void *val) const;

  void SetUpsampler(const int multiple);
  void SetAWGYCOffset(const uint32_t ulY_C_Offset);
  bool SetFilterCoefficients(const stm_display_filter_setup_t *f);
  bool SetOutputFormat(const uint32_t format);

  virtual bool SetInputParams(const bool IsMainInput, uint32_t format) = 0;
  virtual void SetDencSource(const bool IsMainInput) = 0;
  virtual void SetSignalRangeClipping(void) = 0;

  stm_display_mode_t GetCurrentMode(void) const { return m_CurrentMode; }
  bool IsStarted(void) { return m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED; }

protected:
  uint32_t                      *m_pDevRegs;
  uint32_t                       m_TVFmt;
  CSTmHDFSync                   *m_pHDFSync;

  const COutput                 *m_pOwner;
  stm_display_mode_t             m_CurrentMode;
  bool                           m_bMainInput;
  uint32_t                       m_ulInputFormat;
  uint32_t                       m_AWG_Y_C_Offset;
  stm_display_signal_range_t     m_signalRange;

  bool                           m_bHasSeparateCbCrRescale;
  bool                           m_bHas4TapSyncFilter;
  bool                           m_bUseAlternate2XFilter;
  stm_display_hdf_filter_setup_t m_filters[6];

  HDFParams_t                    m_pHDFParams;

  void ProgramYCbCrReScalers(void);

  virtual void SetYCbCrReScalers(void) = 0;
  virtual void EnableAWG (void) = 0;
  virtual void DisableAWG (void) = 0;

  void WriteHDFReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_TVFmt + reg), val); }
  uint32_t ReadHDFReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_TVFmt + reg)); }

private:
  CSTmHDFormatter(const CSTmHDFormatter&);
  CSTmHDFormatter& operator=(const CSTmHDFormatter&);
};


#endif //_STM_HDF_H
