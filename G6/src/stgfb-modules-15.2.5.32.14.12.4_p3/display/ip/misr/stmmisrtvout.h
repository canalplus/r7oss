/***********************************************************************
 *
 * File: display/ip/misr/stmmisrtvout.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_MISR_TVOUT_H
#define _STM_MISR_TVOUT_H

#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/ip/stmmisrviewport.h>

#define TVO_VPORT_MIN_OFFSET        0x004
#define TVO_VPORT_MAX_OFFSET        0x008
#define TVO_STATUS_OFFSET           0x010
#define TVO_REG_1_OFFSET            0x014
#define TVO_REG_2_OFFSET            0x018
#define TVO_REG_3_OFFSET            0x01C

#define TVO_ENABLE_MISR             (0xE<<0)
#define TVO_CAPTURE_EVERY_VSYNC     (0x0<<8)
#define TVO_CAPTURE_ALTERNATE_VSYNC (0x1<<8)
#define TVO_CAPTURE_4TH_VSYNC       (0x2<<8)
#define TVO_CAPTURE_VSYNC_MASK      (0x3<<8)
#define TVO_COUNTER_INTERNAL        (0x1<<12)
#define TVO_PROGRESSIVE_MISR        (0x1<<13)
#define TVO_SYNC_EXT                (0x1<<4)
#define TVO_MISR_STATUS_RSLT_VALID  (1L<<0)
#define TVO_MISR_STATUS_DATA_LOSS   (1L<<4)


typedef struct MisrConfig_s{
  bool      isMisrCaptureStarted;
  uint32_t  misrCtrlVal;
  uint32_t  misrVportMax;
  uint32_t  misrVportMin;
  uint8_t   misrStoreIndex;
  MisrControl_t misrCtrlParams;
}MisrConfig_t;

/*
 * Base class for MISRs present in ST devices starting from STiH415
 */
class CSTmMisrTVOut
{
public:
  CSTmMisrTVOut(CDisplayDevice *pDev, uint32_t MisrPFCtrlReg, uint32_t MisrOutputCtrlReg);
  virtual ~CSTmMisrTVOut(void);

  uint32_t  *m_pDevReg;

  Misr_t    m_MisrData;
  bool      m_isMisrConfigured;

  MisrConfig_t  m_MisrOutput;
  MisrConfig_t  m_MisrPF;
  uint32_t      m_MisrPFCtrlReg;
  uint32_t      m_MisrOutputCtrlReg;

  void          ResetMisrState(void);
  TuningResults CollectMisrValues(void *output);

  void ReadMisrSign(MisrConfig_t *misr, uint32_t reg);
  TuningResults SetMisrCtrl(MisrConfig_t *misr, uint32_t ctrlReg, SetTuningInputData_t *input, const stm_display_mode_t *pCurrentMode);
  void UpdateMisrCtrl(MisrConfig_t *misr, const stm_display_mode_t *pCurrentMode);
  void MisrConfigVsyncValue(uint32_t *misrControlVal, MisrControl_t* MisrCtrlVal);
  void MisrConfigViewPort(const stm_display_mode_t *pCurrentMode,uint32_t *VportMax, uint32_t *VportMin, MisrControl_t* MisrCtrlVal);
  void MisrConfigScanningMode(const stm_display_mode_t *pCurrentMode, uint32_t *misrControlVal);

  // Hardware specific register access
  void     WriteMisrReg(uint32_t reg, uint32_t val) { TRC( TRC_ID_MISR_REG, "%08x <-  %08x", reg, val ); vibe_os_write_register(m_pDevReg, reg, val); }
  uint32_t ReadMisrReg(uint32_t reg)                { uint32_t _val = vibe_os_read_register(m_pDevReg, reg); TRC( TRC_ID_MISR_REG, " %08x  -> %08x", reg, _val ); return _val; }
 private:
  void ResetMisrCtrlParams(MisrConfig_t *ctrlParams, uint8_t index);
  CSTmMisrTVOut(const CSTmMisrTVOut&);
  CSTmMisrTVOut& operator=(const CSTmMisrTVOut&);
};
#endif //_STM_MISR_TVOUT_H
