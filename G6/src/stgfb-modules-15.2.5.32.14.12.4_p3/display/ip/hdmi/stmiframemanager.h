/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2008-2014 STMicroelectronics - All Rights Reserved
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

#ifndef _STM_IFRAME_MANAGER_H
#define _STM_IFRAME_MANAGER_H

#include <display/generic/MetaDataQueue.h>

class CSTmHDMI;
class COutput;
class CDisplayDevice;

class CSTmIFrameManager
{
public:
  CSTmIFrameManager(CDisplayDevice *,uint32_t uHDMIOffset);
  virtual ~CSTmIFrameManager(void);

  virtual bool Create(CSTmHDMI *parent, COutput *master);

  /*
   * The interrupt mask which will result in ProcessInfoFrameComplete()
   * being called by the HDMI interrupt handler.
   */
  virtual uint32_t GetIFrameCompleteHDMIInterruptMask(void) = 0;

  bool Start(const stm_display_mode_t*);
  virtual bool Stop(void);

  bool IsStarted(void) { return m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED; }

  stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  void FlushMetadata(stm_display_metadata_type_t);

  virtual void UpdateFrame(void);
  virtual void SendFirstInfoFrame(void) = 0;
  virtual void ProcessInfoFrameComplete(uint32_t interruptStatus) = 0;

  virtual uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  virtual uint32_t GetControl(stm_output_control_t, uint32_t *val) const;

  /*
   * AVI frame configuration
   */
  void SetVICSelectMode(stm_avi_vic_selection_t mode);
  stm_avi_vic_selection_t GetVICSelectMode(void) const { return m_VICSelectMode;}

  void SetAVIContentType(uint32_t type);
  uint32_t GetAVIContentType(void) const { return m_ulContentType;}

  void SetOverscanMode(uint32_t mode);
  uint32_t GetOverscanMode(void) const { return m_ulOverscanMode;}

  void SetVCDBQuantization(stm_vcdb_quantization_t quantization);
  stm_vcdb_quantization_t GetVCDBQuantization(void) const { return m_VCDBQuantization;}

  void SetAVIQuantizationMode(stm_avi_quantization_mode_t quantization);
  stm_avi_quantization_mode_t GetAVIQuantizationMode(void) const { return m_AVIQuantizationMode;}

  void SetAVIExtendedColorimetry (bool valid);
  uint32_t GetAVIExtendedColorimetry(void) const { return m_bAVIExtendedColorimetryValid;}

  void ForceAVIUpdate(void) { m_bAVIFrameNeedsUpdate = true; }

  /*
   * Audio IFrame configuration
   */
  void SetAudioValid(bool valid)     { m_bAudioFrameValid = valid; }

  int  GetAudioFrequencyIndex(void) const { return m_nAudioFrequencyIndex; }

protected:
  CMetaDataQueue * m_pPictureInfoQueue;
  CMetaDataQueue * m_pAudioQueue;
  CMetaDataQueue * m_pISRCQueue;
  CMetaDataQueue * m_pACPQueue;
  CMetaDataQueue * m_pSPDQueue;
  CMetaDataQueue * m_pVendorQueue;
  CMetaDataQueue * m_pGamutQueue;
  CMetaDataQueue * m_pNTSCQueue;
  CMetaDataQueue * m_p3DEXTQueue;

  CSTmHDMI       * m_pParent;
  COutput        * m_pMaster;

  bool             m_bTransmitHDMIVSIFrame;
  bool             m_bAudioFrameValid;

  void           * m_lock;

  stm_display_mode_t m_CurrentMode;

  virtual void WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *) = 0;
  virtual void EnableTransmissionSlot(int transmissionSlot) = 0;
  virtual void DisableTransmissionSlot(int transmissionSlot) = 0;

  void         InfoFrameChecksum(stm_hdmi_info_frame_t *);
  void         PrintInfoFrame(stm_hdmi_info_frame_t *);
  void         SetDefaultAudioFrame(void);
  void         SetDefaultPictureInformation(void);

  void WriteHDMIReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uHDMIOffset+reg), val); }
  uint32_t ReadHDMIReg(uint32_t reg) const { return vibe_os_read_register(m_pDevRegs, (m_uHDMIOffset+reg)); }

private:
  uint32_t                  * m_pDevRegs;
  uint32_t                    m_uHDMIOffset;

  stm_picture_format_info_t   m_pictureInfo;
  stm_hdmi_info_frame_t       m_AVIFrame;
  stm_hdmi_info_frame_t       m_ACPFrame;
  stm_hdmi_info_frame_t       m_HDMIVSIFrame;
  stm_hdmi_info_frame_t       m_3DEXTData;
  stm_hdmi_info_frame_t       m_SPDFrame;
  stm_hdmi_info_frame_t       m_VSIFrame;

  stm_avi_vic_selection_t     m_VICSelectMode;
  stm_vcdb_quantization_t     m_VCDBQuantization;
  stm_avi_quantization_mode_t m_AVIQuantizationMode;
  stm_ycbcr_colorspace_t      m_colorspaceMode;

  uint32_t                    m_VICCode;
  uint32_t                    m_ulOverscanMode;
  uint32_t                    m_ulContentType;
  int                         m_nAudioFrequencyIndex;

  volatile bool               m_bAVIFrameNeedsUpdate;
  volatile bool               m_bHDMIVSIFrameNeedsUpdate;
  volatile bool               m_bAVIExtendedColorimetryValid;
  volatile int                m_nACPTransmissionCount;
  volatile int                m_nMuxedTransmissionCount;
  int                         m_nACPTransmissionFrameDelay;
  bool                        m_bISRCTransmissionInProgress;


  void     ProcessPictureInfoQueue(void);
  void     ProgramAVIFrame(void);
  uint32_t SelectVICCode(void);
  uint32_t SelectPICAR(void);
  void     ProgramAVIActiveFormatInfo(void);
  stm_avi_quantization_mode_t SelectRGBQuantization(void);
  void     ProgramAVIColorspaceInfo(void);
  void     ProgramAVIContentType(void);
  void     ProgramAVIBarInfo(void);

  void     ProcessAudioQueues(void);
  void     UpdateMuxedIFrameSlots(void);

  void     Process3DEXTDataQueue(void);
  void     SetHDMIVSIData(const stm_display_mode_t *);

  CSTmIFrameManager(const CSTmIFrameManager&);
  CSTmIFrameManager& operator=(const CSTmIFrameManager&);
};


/*
 * A simple CPU interrupt driven IFrame manager.
 */
typedef struct stm_hw_iframe_s
{
  uint32_t header;
  uint32_t data[7];
  uint32_t enable;
} stm_hw_iframe_t;


class CSTmCPUIFrames: public CSTmIFrameManager
{
public:
  CSTmCPUIFrames(CDisplayDevice *,uint32_t uHDMIOffset);
  virtual ~CSTmCPUIFrames(void);

  bool Create(CSTmHDMI *parent, COutput *master);

  uint32_t GetIFrameCompleteHDMIInterruptMask(void);

protected:
  DMA_Area         m_IFrameSlots;
  int              m_nSlots;
  stm_hw_iframe_t *m_slots;
  int              m_nNextSlot;

  void WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *);
  void EnableTransmissionSlot(int transmissionSlot);
  void DisableTransmissionSlot(int transmissionSlot);
  void SendFirstInfoFrame(void);
  void ProcessInfoFrameComplete(uint32_t interruptStatus);

private:
  void SendNextInfoFrame(void);

  CSTmCPUIFrames(const CSTmCPUIFrames&);
  CSTmCPUIFrames& operator=(const CSTmCPUIFrames&);
};

#endif //_STM_IFRAME_MANAGER_H
