/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2005-2014 STMicroelectronics - All Rights Reserved
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

#ifndef _STMHDMI_H
#define _STMHDMI_H

#include <display/generic/Output.h>
#include <display/generic/MetaDataQueue.h>

class COutput;
class CSTmVTG;
class CSTmIFrameManager;
class CSTmHDMIPhy;


typedef enum {
  STM_HDMI_DEEP_COLOR             = (1L<<0),
  STM_HDMI_RX_SENSE               = (1L<<1),
  STM_HDMI_3D_FRAME_PACKED        = (1L<<2),
  STM_HDMI_3D_FIELD_ALTERNATIVE   = (1L<<3),
  STM_HDMI_297_TMDS_CLOCK         = (1L<<4),
}stm_hdmi_hardware_features_t;


typedef enum {
  STM_HDMI_AUDIO_STOPPED = 0,
  STM_HDMI_AUDIO_RUNNING,
  STM_HDMI_AUDIO_DISABLED
} stm_hdmi_audio_status_t;


typedef struct
{
  stm_hdmi_audio_status_t status;
  uint32_t clock_divide;
  uint32_t clock_cts_divide;
  uint32_t fsynth_frequency;
  uint32_t N;
} stm_hdmi_audio_state_t;


class CSTmHDMI: public COutput
{
public:
  CSTmHDMI(const char *name,
           uint32_t id,
           CDisplayDevice *,
           stm_hdmi_hardware_features_t,
           uint32_t ulHDMIOffset,
           COutput *);

  virtual ~CSTmHDMI(void);

  virtual void PowerOnSetup(void);
  virtual bool Create(void);
  virtual void CleanUp(void);

  bool HandleInterrupts(void);

  OutputResults Start(const stm_display_mode_t*);
  bool DisableHW(void);
  virtual void DisableClocks(void) { }
  bool Stop(void);
  void  Suspend(void);
  void  Resume(void);

  uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  uint32_t SetCompoundControl(stm_output_control_t, void *newVal);

  stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  void FlushMetadata(stm_display_metadata_type_t);

  void SetClockReference(stm_clock_ref_frequency_t, int error_ppm);

protected:
  stm_hdmi_hardware_features_t m_HWFeatures;

  volatile bool  m_bSWResetCompleted;
  volatile uint32_t m_ulLastCapturedPixel;

  bool m_bUseHotplugInt;
  bool m_bAVMute;

  bool  m_bSinkSupportsDeepcolour;
  uint32_t m_ulDeepcolourGCP;

  stm_clock_ref_frequency_t    m_audioClockReference;
  uint32_t                     m_maxAudioClockDivide;
  bool                         m_bUseFixedNandCTS;
  stm_hdmi_audio_state_t       m_audioState;
  stm_hdmi_audio_output_type_t m_ulAudioOutputType;
  uint32_t                     m_uCurrentTMDSClockFreq;

  CSTmIFrameManager          * m_pIFrameManager;
  uint32_t                     m_ulIFrameManagerIntMask;

  uint32_t                   * m_pDevRegs;
  uint32_t                     m_uHDMIOffset;
  COutput                    * m_pMasterOutput;

  CSTmHDMIPhy                * m_pPHY;

  void                       * m_statusLock;

  bool                         m_hdmi_decimation_bypass;

  void UpdateOutputMode(const stm_display_mode_t &);

  virtual const stm_display_mode_t* SupportedMode(const stm_display_mode_t *) const;

  /*
   * Subclass step method called before any of the digital IP is configured.
   */
  virtual bool PreConfiguration(const stm_display_mode_t*) = 0;
  /*
   * Subclass step method called after the digital IP has been configured and
   * come out of software reset.
   */
  virtual bool PostConfiguration(const stm_display_mode_t*) = 0;

  virtual void GetAudioHWState(stm_hdmi_audio_state_t *state) = 0;
  virtual int  GetAudioFrequency(void) = 0;

  virtual bool SetOutputFormat(uint32_t format);
  virtual void SetSignalRangeClipping(void);
  virtual bool SetAudioSource(stm_display_output_audio_source_t source);
  virtual void UpdateFrame(void);

  void InvalidateAudioPackets(void);

  void WriteHDMIReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uHDMIOffset+reg), val); }
  uint32_t ReadHDMIReg(uint32_t reg) const { return vibe_os_read_register(m_pDevRegs, (m_uHDMIOffset+reg)); }

private:
  void SWReset(void);
  bool IsModeHDMICompatible(const stm_display_mode_t &) const;
  bool IsPixelRepetitionCompatible(const stm_display_mode_t &baseMode, const stm_display_mode_t &repeatedMode, int repeat) const;
  void ProgramV1Config(uint32_t &cfg, const stm_display_mode_t *const);
  void ProgramV29Config(uint32_t &cfg, const stm_display_mode_t *const);
  void ProgramActiveArea(const stm_display_mode_t *const);
  void SendGeneralControlPacket(void);
  void UpdateAudioState(void);
  void SetAudioCTS(const stm_hdmi_audio_state_t &state);
  bool SetDeepcolorGCPState(const stm_display_mode_t *const pModeLine);
  void EnableInterrupts(void);

  CSTmHDMI(const CSTmHDMI&);
  CSTmHDMI& operator=(const CSTmHDMI&);

};

#endif //_STMHDMI_H
