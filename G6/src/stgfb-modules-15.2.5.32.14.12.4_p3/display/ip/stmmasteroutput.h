/***********************************************************************
 *
 * File: display/ip/stmmasteroutput.h
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_MASTER_OUTPUT_H
#define _STM_MASTER_OUTPUT_H

#include <display/generic/Output.h>

class CDisplayMixer;
class CSTmDENC;
class CSTmVTG;

/*
 * This is a common starting point for SoC specific "master" output classes.
 *
 * A "master" output is one that owns a timing generator (i.e. controls
 * display output timing) and a mixer (can have planes attached to it). In this
 * case it is also expected to be outputting video via analogue DACs.
 */
class CSTmMasterOutput : public COutput
{
public:
  CSTmMasterOutput(const char *name, uint32_t id, CDisplayDevice*, CSTmDENC*, CSTmVTG*, CDisplayMixer*);
  virtual ~CSTmMasterOutput();

  OutputResults Start(const stm_display_mode_t*);
  bool  Stop(void);
  void  Suspend(void);
  void  Resume(void);

  void  UpdateHW();
  bool  CanShowPlane(const CDisplayPlane *);
  bool  ShowPlane(const CDisplayPlane *);
  void  HidePlane(const CDisplayPlane *);

  bool  SetPlaneDepth(const CDisplayPlane *, int depth, bool activate);
  bool  GetPlaneDepth(const CDisplayPlane *, int *depth) const;

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal);

  stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  void FlushMetadata(stm_display_metadata_type_t);

  void  SetClockReference(stm_clock_ref_frequency_t, int error_ppm);

  void  SoftReset(void);

  bool  HandleInterrupts(void);

protected:
  CSTmDENC             *m_pDENC;
  CSTmVTG              *m_pVTG;
  CDisplayMixer        *m_pMixer;
  uint32_t             *m_pDevReg;

  stm_display_mode_t    m_PendingMode;
  bool                  m_bPendingStart;

  uint32_t              m_ulBrightness;
  uint32_t              m_ulContrast;
  uint32_t              m_ulSaturation;
  uint32_t              m_ulHue;

  bool                  m_bUsingDENC;

  /*
   * Current DAC rescaling parameters for HD/ED analogue outputs, which can
   * be board dependent due to variation in output driver configurations.
   */
  uint32_t m_maxDACVoltage;
  uint32_t m_DACSaturation;
  uint32_t m_DAC_43IRE;
  uint32_t m_DAC_321mV;
  uint32_t m_DAC_700mV;
  uint32_t m_DAC_RGB_Scale;
  uint32_t m_DAC_Y_Scale;
  uint32_t m_DAC_C_Scale;
  uint32_t m_DAC_Y_Offset;
  uint32_t m_DAC_C_Offset;
  uint32_t m_AWG_Y_C_Offset;

  virtual void ProgramPSIControls(void);

  virtual bool SetOutputFormat(uint32_t) = 0;

  virtual void EnableDACs(void)       = 0;
  virtual void DisableDACs(void)      = 0;

  bool TryModeChange(const stm_display_mode_t*);
  void RecalculateDACSetup(void);

  virtual bool SetFilterCoefficients(const stm_display_filter_setup_t *);

  void  UpdateOutputMode(const stm_display_mode_t &);

  virtual const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;
  void WriteDevReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevReg, reg, val); }
  uint32_t ReadDevReg(uint32_t reg) { return vibe_os_read_register(m_pDevReg, reg); }

private:
  int m_VTGErrorCount;

  virtual void SetMisrData(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt);
  virtual void UpdateMisrCtrl(void);

  CSTmMasterOutput(const CSTmMasterOutput&);
  CSTmMasterOutput& operator=(const CSTmMasterOutput&);
};

#endif //_STM_MASTER_OUTPUT_H
