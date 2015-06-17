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

#ifndef _STMDENC_H
#define _STMDENC_H

#include <display/generic/MetaDataQueue.h>
#include <display/generic/Output.h>

#include <display/ip/displaytiming/stmvtg.h> // For stm_vtg_sync_type_t

class CDisplayDevice;
class COutput;
class CSTmTeletext;

struct stm_denc_idfs {
  uint8_t idfs0;
  uint8_t idfs1;
  uint8_t idfs2;
};

struct stm_denc_cfg {
  uint8_t  cfg[13];
  uint8_t  ttxcfg;
  uint16_t pdfs;
  const struct stm_denc_idfs *idfs;
};

class CSTmDENC
{
public:
  CSTmDENC(CDisplayDevice* pDev, uint32_t ulDencOffset, CSTmTeletext* pTeletext=0);

  virtual ~CSTmDENC();

  virtual bool Create();

  const COutput *GetOwner(void) const { return m_pParent; }

  virtual bool Start(COutput *,const stm_display_mode_t*);
  virtual void Stop(void);

  stm_display_mode_t GetCurrentMode(void) const { return m_CurrentMode; }
  bool IsStarted(void) { return m_CurrentMode.mode_id != STM_TIMING_MODE_RESERVED; }

  virtual bool SetMainOutputFormat(uint32_t);
  virtual bool SetAuxOutputFormat(uint32_t);

  uint32_t GetCapabilities(void) const { return m_ulCapabilities; }

  virtual uint32_t SetControl(stm_output_control_t, uint32_t newVal);
  virtual uint32_t GetControl(stm_output_control_t, uint32_t *val) const;
  virtual uint32_t SetCompoundControl(stm_output_control_t ctrl, void *newVal) { return STM_OUT_NO_CTRL; }
  virtual uint32_t GetCompoundControl(stm_output_control_t ctrl, void *val) const { return STM_OUT_NO_CTRL; }

  stm_display_metadata_result_t QueueMetadata(stm_display_metadata_t *);
  void FlushMetadata(stm_display_metadata_type_t);

  bool SetFilterCoefficients(const stm_display_filter_setup_t *);
  bool SetVPS(stm_display_vps_t *);
  void UpdateHW(uint32_t timingevent);

protected:
  uint32_t m_ulCapabilities;
  uint32_t m_mainOutputFormat;
  uint32_t m_auxOutputFormat;

  CDisplayDevice *m_pDev;
  COutput        *m_pParent;
  CMetaDataQueue *m_pPictureInfoQueue;
  CMetaDataQueue *m_pClosedCaptionQueue;
  CSTmTeletext   *m_pTeletext;

  uint32_t * m_pDENCReg;
  void     * m_lock;

  struct stm_denc_cfg m_currentCfg;

  stm_display_mode_t m_CurrentMode;

  /*
   * DENC encoding setup
   */
  bool  m_bEnableTrapFilter;
  bool  m_bEnableLumaNotchFilter;
  bool  m_bEnableIFLumaDelay;
  bool  m_bEnableCVBSChromaDelay;
  bool  m_bEnableComponentChromaDelay;
  bool  m_bFreerunEnabled;

  uint8_t m_CVBSChromaDelay;
  uint8_t m_ComponentChromaDelay;
  uint8_t m_RGBDelay;

  bool  m_bEnableLumaFilter;
  stm_display_denc_filter_setup_t m_lumaFilterConfig;

  bool  m_bEnableChromaFilter;
  stm_display_denc_filter_setup_t m_chromaFilterConfig;

  /*
   * VBI control state
   */
  stm_picture_format_info_t m_currentPictureInfo;

  bool     m_bEnableWSSInsertion;
  uint32_t m_CGMSAnalog;
  bool     m_bUpdateWSS;
  uint8_t  m_teletextStandard;
  uint32_t m_teletextLineSize;
  uint32_t m_bEnableCCField;

  /*
   * DAC scaling setup
   */
  uint32_t m_DisplayStandardMaxVoltage;
  uint32_t m_MinRGBScalePercentage;
  uint32_t m_RGBScaleStep;
  uint32_t m_MinYScalePercentage;
  uint32_t m_YScaleStep;

  uint32_t m_maxDAC123Voltage;
  uint32_t m_DAC123Saturation;
  uint32_t m_maxDAC456Voltage;
  uint32_t m_DAC456Saturation;

  uint8_t m_DAC123RGBScale;
  uint8_t m_DAC123YScale;
  uint8_t m_DAC456RGBScale;
  uint8_t m_DAC456YScale;

  uint32_t m_C_MultiplyStep;
  uint32_t m_C_MultiplyMin;
  uint32_t m_C_MultiplyMax;
  uint8_t m_C_Multiply;

  uint32_t m_ulBrightness;
  uint32_t m_ulSaturation;
  uint32_t m_ulContrast;
  bool     m_bHasHueControl;
  uint32_t m_ulHue;

  stm_display_signal_range_t m_signalRange;

  void ProgramDENCConfig(void);
  void Program625LineWSS(void);
  void Program525LineWSS(void);
  void ProgramPDFSConfig(void);
  void ProgramPDFSPALConfig(void);

  void SetTeletextLineSize(void);

  void SetPSIControl(uint32_t *ctrl, uint32_t newVal);

  virtual void ProgramOutputFormats(void) = 0;
  virtual void ProgramPSIControls(void);
  virtual void ProgramLumaFilter(void);
  virtual void ProgramChromaFilter(void);

  uint8_t GetSyncConfiguration(const stm_display_mode_t*,
                               uint8_t syncMode,
                               stm_vtg_sync_type_t href,
                               stm_vtg_sync_type_t vref);

  void CalculateDACScaling(void);

  bool Start(COutput *,
             const stm_display_mode_t*,
             uint8_t syncMode,
             stm_vtg_sync_type_t href=STVTG_SYNC_TIMING_MODE,
             stm_vtg_sync_type_t vref=STVTG_SYNC_TIMING_MODE);

  /*
   * Unlike the other components the DENC registers are defined
   * by their offset/4 and are only 8bits wide. Therefore we add the
   * register value to m_pDENCReg (uint32_t *) without an additional shift,
   * then convert it to a uint8_t *.
   */
  void WriteDENCReg(uint32_t reg, uint8_t val) { vibe_os_write_byte_register((uint8_t *)(m_pDENCReg + reg), val); }
  uint8_t ReadDENCReg(uint32_t reg) const { return vibe_os_read_byte_register((uint8_t *)(m_pDENCReg + reg)); }

private:
  CSTmDENC(const CSTmDENC&);
  CSTmDENC& operator=(const CSTmDENC&);
};

#endif // _STMDENC_H
