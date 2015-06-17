/***********************************************************************
 *
 * File: STMCommon/stmmasteroutput.h
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_MASTER_OUTPUT_H
#define _STM_MASTER_OUTPUT_H

#include <Generic/Output.h>

class CDisplayMixer;
class CSTmDENC;
class CSTmVTG;
class CSTmFSynth;

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
  CSTmMasterOutput(CDisplayDevice*, CSTmDENC*, CSTmVTG*, CDisplayMixer*, CSTmFSynth*);
  virtual ~CSTmMasterOutput();

  bool  Start(const stm_mode_line_t*, ULONG tvStandard);
  bool  Stop(void);
  void  Suspend(void);
  void  Resume(void);

  void  UpdateHW();

  bool  CanShowPlane(stm_plane_id_t planeID);
  bool  ShowPlane(stm_plane_id_t planeID);
  void  HidePlane(stm_plane_id_t planeID);

  bool  SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool  GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  void FlushMetadata(stm_meta_data_type_t);

  bool  SetFilterCoefficients(const stm_display_filter_setup_t *);

  void  SetClockReference(stm_clock_ref_frequency_t, int error_ppm);

  void  SoftReset(void);

  bool  HandleInterrupts(void);

protected:
  CSTmDENC             *m_pDENC;
  CSTmVTG              *m_pVTG;
  CDisplayMixer        *m_pMixer;
  CSTmFSynth           *m_pFSynth;
  ULONG                *m_pDevReg;

  const stm_mode_line_t * volatile m_pPendingMode;

  ULONG                 m_ulBrightness;
  ULONG                 m_ulContrast;
  ULONG                 m_ulSaturation;
  ULONG                 m_ulHue;

  bool                  m_bUsingDENC;

  ULONG                 m_ulLock;

  /*
   * Current DAC rescaling parameters for HD/ED analogue outputs, which can
   * be board dependent due to variation in output driver configurations.
   */
  ULONG m_maxDACVoltage;
  ULONG m_DACSaturation;
  ULONG m_DAC_43IRE;
  ULONG m_DAC_321mV;
  ULONG m_DAC_700mV;
  ULONG m_DAC_RGB_Scale;
  ULONG m_DAC_Y_Scale;
  ULONG m_DAC_C_Scale;
  ULONG m_DAC_Y_Offset;
  ULONG m_DAC_C_Offset;
  ULONG m_AWG_Y_C_Offset;

  virtual void ProgramPSIControls(void);

  virtual bool SetOutputFormat(ULONG) = 0;

  virtual void EnableDACs(void)       = 0;
  virtual void DisableDACs(void)      = 0;

  bool TryModeChange(const stm_mode_line_t*, ULONG tvStandard);
  void RecalculateDACSetup(void);

  void WriteDevReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevReg + (reg>>2), val); }
  ULONG ReadDevReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevReg + (reg>>2)); }

private:
  CSTmMasterOutput(const CSTmMasterOutput&);
  CSTmMasterOutput& operator=(const CSTmMasterOutput&);
};

#endif //_STM_MASTER_OUTPUT_H
