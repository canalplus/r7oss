/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc8vtg.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMC8VTG_H
#define _STMC8VTG_H

#include "stmvtg.h"

#define STM_C8VTG_MAX_SYNC_OUTPUTS 6

class CDisplayDevice;

typedef struct stm_vtg_reg_offsets_s {
  uint32_t h_hd;
  uint32_t top_v_vd;
  uint32_t bot_v_vd;
  uint32_t top_v_hd;
  uint32_t bot_v_hd;
} stm_vtg_reg_offsets_t;

/*
 * CSTmC8VTG is a base class for all variants of the C8VTG IP block.
 */
class CSTmC8VTG: public CSTmVTG
{
public:
  CSTmC8VTG(CDisplayDevice      *pDev,
             uint32_t            uRegOffset,
             uint32_t            uNumSyncOutputs,
             CSTmFSynth         *pFSynth,
             bool                bDoubleClocked,
             stm_vtg_sync_type_t refpol,
             bool                bDisableSyncsOnStop,
             bool                bUseSlaveInterrupts = false);

  virtual ~CSTmC8VTG(void);

  bool Start(const stm_display_mode_t*);
  void Stop(void);

  void RestoreSyncs(void);

protected:
  uint32_t *m_pDevRegs;
  uint32_t  m_uVTGOffset;
  bool      m_bDoSoftResetInInterrupt;
  bool      m_bDisableSyncsOnStop;
  bool      m_bUseOddEvenHSyncFreerun;
  uint32_t  m_uVTGStartModeConfig;
  bool      m_bUseSlaveInterrupts;

  const stm_vtg_reg_offsets_t *m_syncRegOffsets;

  virtual void EnableInterrupts(void) = 0;
  virtual void DisableInterrupts(void) = 0;
  virtual void EnableSyncs(void) = 0;

  bool Start(const stm_display_mode_t*, int externalid);

  void GetHSyncPositions(int                        outputid,
                   const stm_display_mode_timing_t &TimingParams,
                         uint32_t                  &HSync);

  void GetInterlacedVSyncPositions(int                        outputid,
                             const stm_display_mode_timing_t &TimingParams,
                                   uint32_t                   clocksperline,
                                   uint32_t                  &VSyncLineTop,
                                   uint32_t                  &VSyncLineBot,
                                   uint32_t                  &VSyncPixOffTop,
                                   uint32_t                  &VSyncPixOffBot);

  void GetProgressiveVSyncPositions(int                        outputid,
                              const stm_display_mode_timing_t &TimingParams,
                                    uint32_t                   clocksperline,
                                    uint32_t                  &VSyncLineTop,
                                    uint32_t                  &VSyncPixOffTop);

  void ProgramSyncOutputs(const stm_display_mode_t*);

  void WriteVTGReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uVTGOffset+reg), val); }
  uint32_t ReadVTGReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uVTGOffset+reg)); }

private:
  CSTmC8VTG(const CSTmC8VTG&);
  CSTmC8VTG& operator=(const CSTmC8VTG&);
};


/*
 * Common VTG Mode Register Definitions for variants of C8VTG
 */
#define VTG_MOD_MASTER_MODE            (0x0L)
#define VTG_MOD_SLAVE_EXT0             (0x1L)
#define VTG_MOD_SLAVE_EXT1             (0x2L)
#define VTG_MOD_MODE_MASK              (0x3L)
#define VTG_MOD_SLAVE_ODDEVEN_NVSYNC   (1L<<4)
#define VTG_MOD_SLAVE_ODDEVEN_HFREERUN (1L<<5)
#define VTG_MOD_HSYNC_PAD_IN_NOUT      (1L<<7)
#define VTG_MOD_VSYNC_PAD_IN_NOUT      (1L<<8)
#define VTG_MOD_DISABLE                (1L<<9)
#define VTG_MOD_HREF_INV_POLARITY      (1L<<11)
#define VTG_MOD_VREF_INV_POLARITY      (1L<<12)
#define VTG_MOD_HINPUT_INV_POLARITY    (1L<<13)
#define VTG_MOD_VINPUT_INV_POLARITY    (1L<<14)
#define VTG_MOD_EXT_BNOTTREF_INV       (1L<<15)

#endif // _STMC8VTG_H
