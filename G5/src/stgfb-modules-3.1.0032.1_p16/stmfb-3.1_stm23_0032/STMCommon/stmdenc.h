/***********************************************************************
 *
 * File: STMCommon/stmdenc.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMDENC_H
#define _STMDENC_H

class CDisplayDevice;
class COutput;
class CMetaDataQueue;
class CSTmTeletext;

struct stm_denc_idfs {
  UCHAR idfs0;
  UCHAR idfs1;
  UCHAR idfs2;
};

struct stm_denc_cfg {
  UCHAR  cfg[13];
  UCHAR  ttxcfg;
  const struct stm_denc_idfs *idfs;
};

class CSTmDENC
{
public:
  CSTmDENC(CDisplayDevice* pDev, ULONG ulDencOffset, CSTmTeletext* pTeletext=0);

  virtual ~CSTmDENC();

  virtual bool Create();

  virtual bool Start(COutput *,const stm_mode_line_t*, ULONG tvStandard);
  virtual void Stop(void);

  virtual bool SetMainOutputFormat(ULONG);
  virtual bool SetAuxOutputFormat(ULONG);

  virtual ULONG SupportedControls(void) const;
  virtual void  SetControl(stm_output_control_t, ULONG ucNewVal);
  virtual ULONG GetControl(stm_output_control_t) const;

  stm_meta_data_result_t QueueMetadata(stm_meta_data_t *);
  void FlushMetadata(stm_meta_data_type_t);

  bool  SetFilterCoefficients(const stm_display_filter_setup_t *);

  void UpdateHW(stm_field_t);

protected:
  ULONG m_mainOutputFormat;
  ULONG m_auxOutputFormat;

  CDisplayDevice *m_pDev;
  COutput        *m_pParent;
  CMetaDataQueue *m_pPictureInfoQueue;
  CSTmTeletext   *m_pTeletext;

  ULONG* m_pDENCReg;

  struct stm_denc_cfg m_currentCfg;

  const stm_mode_line_t *m_pCurrentMode;
  ULONG m_ulTVStandard;

  /*
   * DENC encoding setup
   */
  bool  m_bEnableTrapFilter;
  bool  m_bUseCVBSDelay;
  bool  m_bFreerunEnabled;

  UCHAR m_CVBSDelay;
  UCHAR m_RGBDelay;

  bool  m_bEnableLumaFilter;
  stm_display_denc_filter_setup_t m_lumaFilterConfig;

  bool  m_bEnableChromaFilter;
  stm_display_denc_filter_setup_t m_chromaFilterConfig;

  /*
   * VBI control state
   */
  stm_picture_format_info_t m_currentPictureInfo;
  stm_open_subtitles_t      m_OpenSubtitles;

  bool  m_bTeletextSubtitles;
  ULONG m_CGMSAnalog;
  bool  m_bPrerecordedAnalogueSource;
  bool  m_bUpdateWSS;
  UCHAR m_teletextStandard;
  ULONG m_teletextLineSize;
  UCHAR m_CGMSA_XDS_Packet[6];
  ULONG m_CGMSA_XDS_SendIndex;
  bool  m_bEnableCCXDS;

  /*
   * DAC scaling setup
   */
  ULONG m_DisplayStandardMaxVoltage;
  ULONG m_MinRGBScalePercentage;
  ULONG m_RGBScaleStep;
  ULONG m_MinYScalePercentage;
  ULONG m_YScaleStep;

  ULONG m_maxDAC123Voltage;
  ULONG m_DAC123Saturation;
  ULONG m_maxDAC456Voltage;
  ULONG m_DAC456Saturation;

  UCHAR m_DAC123RGBScale;
  UCHAR m_DAC123YScale;
  UCHAR m_DAC456RGBScale;
  UCHAR m_DAC456YScale;

  ULONG m_ulBrightness;
  ULONG m_ulSaturation;
  ULONG m_ulContrast;
  bool  m_bHasHueControl;
  ULONG m_ulHue;

  stm_display_signal_range_t m_signalRange;

  void ProgramDENCConfig(void);
  void Program625LineWSS(void);
  void Program525LineWSS(void);
  void ProgramCGMSAXDS(void);

  void SetTeletextLineSize(void);

  void SetPSIControl(ULONG *ctrl, ULONG newVal);

  virtual void ProgramOutputFormats(void) = 0;
  virtual void ProgramPSIControls(void);
  virtual void ProgramLumaFilter(void);
  virtual void ProgramChromaFilter(void);

  UCHAR GetSyncConfiguration(const stm_mode_line_t*,
                             UCHAR syncMode,
                             stm_vtg_sync_type_t href,
                             stm_vtg_sync_type_t vref);

  void CalculateDACScaling(void);

  bool Start(COutput *,
             const stm_mode_line_t*,
             ULONG tvStandard,
             UCHAR syncMode,
             stm_vtg_sync_type_t href=STVTG_SYNC_TIMING_MODE,
             stm_vtg_sync_type_t vref=STVTG_SYNC_TIMING_MODE);

  /*
   * Unlike the other components the DENC registers are defined
   * by their offset/4 and are only 8bits wide. Therefore we add the
   * register value to m_pDENCReg (ULONG *) without an additional shift,
   * then convert it to a UCHAR *.
   */
  void WriteDENCReg(ULONG reg, UCHAR val) { g_pIOS->WriteByteRegister((UCHAR *)(m_pDENCReg + reg), val); }
  UCHAR ReadDENCReg(ULONG reg) const { return g_pIOS->ReadByteRegister((UCHAR *)(m_pDENCReg + reg)); }

private:
  CSTmDENC(const CSTmDENC&);
  CSTmDENC& operator=(const CSTmDENC&);
};

#endif // _STMDENC_H
