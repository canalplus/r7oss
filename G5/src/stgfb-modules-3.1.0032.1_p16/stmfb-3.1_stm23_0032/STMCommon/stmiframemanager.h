/***********************************************************************
 *
 * File: STMCommon/stmiframemanager.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_IFRAME_MANAGER_H
#define _STM_IFRAME_MANAGER_H

class CMetaDataQueue;
class CSTmHDMI;
class COutput;
class CDisplayDevice;

class CSTmIFrameManager
{
public:
  CSTmIFrameManager(CDisplayDevice *,ULONG ulHDMIOffset);
  virtual ~CSTmIFrameManager(void);

  virtual bool Create(CSTmHDMI *parent, COutput *master);

  /*
   * The interrupt mask which will result in ProcessInfoFrameComplete()
   * being called by the HDMI interrupt handler.
   */
  virtual ULONG GetIFrameCompleteHDMIInterruptMask(void) = 0;

  bool Start(const stm_mode_line_t*, ULONG tvStandard);
  virtual bool Stop(void);

  stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  void FlushMetadata(stm_meta_data_type_t);

  virtual void UpdateFrame(void);
  virtual void SendFirstInfoFrame(void) = 0;
  virtual void ProcessInfoFrameComplete(ULONG interruptStatus) = 0;

  /*
   * AVI frame configuration
   */
  void SetCEAModeSelection(stm_hdmi_cea_mode_selection_t mode)  { m_CEAModeSelect = mode; m_bAVIFrameNeedsUpdate = true; }
  stm_hdmi_cea_mode_selection_t GetCEAModeSelection(void) const { return m_CEAModeSelect;}

  void SetAVIContentType(ULONG type)  { m_ulContentType = type; m_bAVIFrameNeedsUpdate = true; }
  ULONG GetAVIContentType(void) const { return m_ulContentType;}

  void SetOverscanMode(ULONG mode)   { m_ulOverscanMode = mode; m_bAVIFrameNeedsUpdate = true; }
  ULONG GetOverscanMode(void)  const { return m_ulOverscanMode;}

  void SetColorspaceMode(stm_ycbcr_colorspace_t mode)   { m_colorspaceMode = mode; m_bAVIFrameNeedsUpdate = true; }
  stm_ycbcr_colorspace_t GetColorspaceMode(void)  const { return m_colorspaceMode;}

  void SetQuantization(stm_hdmi_avi_quantization_t quantization) { m_quantization = quantization; m_bAVIFrameNeedsUpdate = true; }
  stm_hdmi_avi_quantization_t GetQuantization(void) const { return m_quantization;}

  void ForceAVIUpdate(void) { m_bAVIFrameNeedsUpdate = true; }

  /*
   * Audio IFrame configuration
   */
  void SetAudioValid(bool valid)     { m_bAudioFrameValid = valid; }

  int  GetAudioFrequencyIndex(void) const { return m_nAudioFrequencyIndex; }

protected:
  CMetaDataQueue *m_pPictureInfoQueue;
  CMetaDataQueue *m_pAudioQueue;
  CMetaDataQueue *m_pISRCQueue;
  CMetaDataQueue *m_pACPQueue;
  CMetaDataQueue *m_pSPDQueue;
  CMetaDataQueue *m_pVendorQueue;
  CMetaDataQueue *m_pGamutQueue;
  CMetaDataQueue *m_pNTSCQueue;

  stm_picture_format_info_t m_pictureInfo;
  stm_hdmi_info_frame_t     m_AVIFrame;
  stm_hdmi_info_frame_t     m_ACPFrame;

  stm_hdmi_cea_mode_selection_t m_CEAModeSelect;
  stm_hdmi_avi_quantization_t   m_quantization;
  stm_ycbcr_colorspace_t        m_colorspaceMode;

  ULONG m_ulOverscanMode;
  ULONG m_ulContentType;
  int   m_nAudioFrequencyIndex;

  volatile bool m_bAVIFrameNeedsUpdate;
  bool          m_bAudioFrameValid;
  volatile int  m_nACPTransmissionCount;
  int           m_nACPTransmissionFrameDelay;
  bool          m_bISRCTransmissionInProgress;

  ULONG    *m_pDevRegs;
  ULONG     m_ulHDMIOffset;
  CSTmHDMI *m_pParent;
  COutput  *m_pMaster;

  ULONG     m_ulLock;

  const stm_mode_line_t*m_pCurrentMode;
  ULONG                 m_ulTVStandard;

  virtual void WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *) = 0;
  virtual void EnableTransmissionSlot(int transmissionSlot) = 0;
  virtual void DisableTransmissionSlot(int transmissionSlot) = 0;

  void ProgramAVIFrame(void);
  void InfoFrameChecksum(stm_hdmi_info_frame_t *);
  void PrintInfoFrame(stm_hdmi_info_frame_t *);
  void SetDefaultAudioFrame(void);
  void SetDefaultPictureInformation(void);

  void WriteHDMIReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulHDMIOffset+reg)>>2), val); }
  ULONG ReadHDMIReg(ULONG reg) const { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulHDMIOffset+reg)>>2)); }

private:
  CSTmIFrameManager(const CSTmIFrameManager&);
  CSTmIFrameManager& operator=(const CSTmIFrameManager&);
};


/*
 * A simple CPU interrupt driven IFrame manager.
 */
typedef struct stm_hw_iframe_s
{
  ULONG header;
  ULONG data[7];
  ULONG enable;
} stm_hw_iframe_t;


class CSTmCPUIFrames: public CSTmIFrameManager
{
public:
  CSTmCPUIFrames(CDisplayDevice *,ULONG ulHDMIOffset);
  virtual ~CSTmCPUIFrames(void);

  bool Create(CSTmHDMI *parent, COutput *master);

  ULONG GetIFrameCompleteHDMIInterruptMask(void);

protected:
  DMA_Area m_IFrameSlots;
  int m_nSlots;
  stm_hw_iframe_t *m_slots;
  int m_nNextSlot;

  void WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *);
  void EnableTransmissionSlot(int transmissionSlot);
  void DisableTransmissionSlot(int transmissionSlot);
  void SendFirstInfoFrame(void);
  void ProcessInfoFrameComplete(ULONG interruptStatus);

private:
  void SendNextInfoFrame(void);

  CSTmCPUIFrames(const CSTmCPUIFrames&);
  CSTmCPUIFrames& operator=(const CSTmCPUIFrames&);
};

#endif //_STM_IFRAME_MANAGER_H
