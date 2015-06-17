/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfhdmi.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMHDFHDMI_H
#define _STMHDFHDMI_H

/*
 * This class is for HDMI cells that are fed via the HDFormatter IP digital
 * output path. This is common to a number of chips including
 * 7200, 7111, 7141 & 7105.
 */

#include <STMCommon/stmhdmi.h>

class COutput;
class CSTmSDVTG;

typedef enum { STM_HDF_HDMI_REV1, STM_HDF_HDMI_REV2, STM_HDF_HDMI_REV3 } stm_hdf_hdmi_hardware_version_t;

class CSTmHDFormatterHDMI: public CSTmHDMI
{
public:
  CSTmHDFormatterHDMI(CDisplayDevice *,
                      stm_hdmi_hardware_version_t,
                      stm_hdf_hdmi_hardware_version_t,
                      ULONG ulHDMIOffset,
                      ULONG ulFormatterOffset,
                      ULONG ulPCMPOffset,
                      ULONG ulSPDIFOffset,
                      COutput *,
                      CSTmVTG *);

  virtual ~CSTmHDFormatterHDMI(void);
  virtual bool Create(void);

  ULONG SupportedControls(void) const;
  void  SetControl(stm_output_control_t, ULONG ulNewVal);
  ULONG GetControl(stm_output_control_t) const;

protected:
  stm_hdf_hdmi_hardware_version_t m_Revision;

  virtual bool PostConfiguration(const stm_mode_line_t*, ULONG tvStandard);

  /*
   * Possibly overridden by subclasses to route audio into the HDMI cell
   */
  virtual bool SetInputSource(ULONG  source);

private:
  bool PreAuth() const;
  void PostAuth(bool auth);

  void GetAudioHWState(stm_hdmi_audio_state_t *);
  bool SetOutputFormat(ULONG format);
  void SetSPDIFPlayerMode(void);

  ULONG m_ulFmtOffset;
  ULONG m_ulPCMPOffset;
  ULONG m_ulSPDIFOffset;

  ULONG ReadHDFmtReg(ULONG reg)            { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulFmtOffset+reg)>>2)); }
  void WriteHDFmtReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulFmtOffset+reg)>>2), val); }

  ULONG ReadPCMPReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulPCMPOffset+reg)>>2)); }
  ULONG ReadSPDIFReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulSPDIFOffset+reg)>>2)); }

  CSTmHDFormatterHDMI(const CSTmHDFormatterHDMI&);
  CSTmHDFormatterHDMI& operator=(const CSTmHDFormatterHDMI&);
};


/*
 * Defines for the glue logic between HDMI and HDF for 7111,7200Cut2 etc
 * 7200Cut1 uses a cut down version plus an extra bit for DTS audio formatting.
 */
#define STM_HDMI_GPOUT_I2S_EN                  (1L<<0)
#define STM_HDMI_GPOUT_SPDIF_EN                (1L<<1)
#define STM_HDMI_GPOUT_I2S_N_SPDIF_DATA        (1L<<2)
#define STM_HDMI_GPOUT_EXTERNAL_PCMPLAYER_EN   (1L<<3)
#define STM_HDMI_GPOUT_HDCP_VSYNC_DISABLE      (1L<<4)
#define STM_HDMI_GPOUT_DTS_FORMAT_EN           (1L<<5) /* Rev1 only           */
#define STM_HDMI_GPOUT_CTS_CLK_DIV_BYPASS      (1L<<6) /* Rev1 only           */
#define STM_HDMI_GPOUT_DTS_SPDIF_MODE_EN       (1L<<7) /* Rev1 and Rev2       */
#define STM_HDMI_GPOUT_DROP_CB_N_CR            (1L<<8) /* Rev2 only from here */
#define STM_HDMI_GPOUT_DROP_EVEN_N_ODD_SAMPLES (1L<<9)
#define STM_HDMI_GPOUT_IN_LCD                  (0)
#define STM_HDMI_GPOUT_IN_COMPOSITOR           (1L<<10)
#define STM_HDMI_GPOUT_IN_HDFORMATTER          (2L<<10)
#define STM_HDMI_GPOUT_IN_MASK                 (3L<<10)
#define STM_HDMI_GPOUT_I2S_N_SPDIF_CLOCK       (1L<<14) /* Rev2 and Rev3 */


#endif //_STMHDFHDMI_H
