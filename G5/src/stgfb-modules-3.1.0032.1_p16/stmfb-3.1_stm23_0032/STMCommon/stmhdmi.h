/***********************************************************************
 *
 * File: STMCommon/stmhdmi.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDMI_H
#define _STMHDMI_H

#include <Generic/Output.h>

class COutput;
class CSTmVTG;
class CSTmIFrameManager;

typedef enum {
  STM_HDMI_HW_V_1_2,
  STM_HDMI_HW_V_1_4,
  STM_HDMI_HW_V_2_9, // HDMI 1.3a including deepcolor
} stm_hdmi_hardware_version_t;


typedef enum {
  STM_HDMI_AUDIO_STOPPED = 0,
  STM_HDMI_AUDIO_RUNNING,
  STM_HDMI_AUDIO_DISABLED
} stm_hdmi_audio_status_t;


typedef struct
{
  stm_hdmi_audio_status_t status;
  ULONG clock_divide;
  ULONG fsynth_frequency;
  ULONG N;
} stm_hdmi_audio_state_t;


class CSTmHDMI: public COutput
{
public:
  CSTmHDMI(CDisplayDevice *,
           stm_hdmi_hardware_version_t,
           ULONG ulHDMIOffset,
           COutput *,
           CSTmVTG *);

  virtual ~CSTmHDMI(void);

  virtual bool Create(void);

  ULONG GetCapabilities(void) const;

  bool HandleInterrupts(void);

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  bool Stop(void);

  bool CanShowPlane(stm_plane_id_t planeID);
  bool ShowPlane   (stm_plane_id_t planeID);
  void HidePlane   (stm_plane_id_t planeID);
  bool SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate);
  bool GetPlaneDepth(stm_plane_id_t planeID, int *depth) const;

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

  stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  void FlushMetadata(stm_meta_data_type_t);

protected:
  stm_hdmi_hardware_version_t m_HWVersion;

  volatile bool  m_bSWResetCompleted;
  volatile ULONG m_ulLastCapturedPixel;

  bool m_bUseHotplugInt;
  bool m_bAVMute;
  bool m_bESS;

  bool  m_bSinkSupportsDeepcolour;
  ULONG m_ulDeepcolourGCP;

  ULONG m_maxAudioClockDivide;

  CSTmIFrameManager *m_pIFrameManager;
  ULONG m_ulIFrameManagerIntMask;

  stm_display_hdmi_phy_config_t *m_pPHYConfig;

  stm_hdmi_audio_state_t       m_audioState;
  stm_hdmi_audio_output_type_t m_ulAudioOutputType;

  ULONG    *m_pDevRegs;
  ULONG     m_ulHDMIOffset;
  COutput  *m_pMasterOutput;
  CSTmVTG  *m_pVTG;

  ULONG     m_statusLock;

  /*
   * Subclass step method called before any of the digital IP is configured.
   */
  virtual bool PreConfiguration(const stm_mode_line_t*, ULONG tvStandard) = 0;
  /*
   * Subclass step method called after the digital IP has been configured and
   * come out of software reset.
   */
  virtual bool PostConfiguration(const stm_mode_line_t*, ULONG tvStandard) = 0;
  /*
   * Subclass step for parts containing the new deepcolour PHY, configured
   * using the digital IP registers, called after the PHY PLL has been
   * configured and the serializer has been configured and powered up but before
   * the digital IP has been configured or reset.
   */
  virtual bool PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard);

  virtual void GetAudioHWState(stm_hdmi_audio_state_t *state) = 0;
  virtual int  GetAudioFrequency(void) = 0;

  virtual bool PreAuth(void) const = 0;
  virtual void PostAuth(bool auth) = 0;

  virtual bool SetOutputFormat(ULONG format);
  virtual void UpdateFrame(void);

  void InvalidateAudioPackets(void);

  void WriteHDMIReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulHDMIOffset+reg)>>2), val); }
  ULONG ReadHDMIReg(ULONG reg) const { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulHDMIOffset+reg)>>2)); }

private:
  void SWReset(void);
  void ProgramV1Config(ULONG &cfg, const stm_mode_line_t *const, ULONG tvStandard);
  void ProgramV29Config(ULONG &cfg, const stm_mode_line_t *const, ULONG tvStandard);
  bool StartV29PHY(const stm_mode_line_t *const, ULONG tvStandard);
  void StopV29PHY(void);
  void ProgramActiveArea(const stm_mode_line_t *const, ULONG tvStandard);
  void SendGeneralControlPacket(void);
  void UpdateAudioState(void);
  void SetAudioCTS(const stm_hdmi_audio_state_t &state);
  void EnableInterrupts(void);

  CSTmHDMI(const CSTmHDMI&);
  CSTmHDMI& operator=(const CSTmHDMI&);

};

#endif //_STMHDMI_H
