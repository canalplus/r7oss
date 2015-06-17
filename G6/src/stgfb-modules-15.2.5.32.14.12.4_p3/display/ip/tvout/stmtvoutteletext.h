/***********************************************************************
 *
 * File: display/ip/tvout/stmtvoutteletext.h
 * Copyright (c) 2008-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_TVOUT_TELETEXT_H
#define _STM_TVOUT_TELETEXT_H

/*
 * Teletext processing class for HDFormatter/TVOut based implementations that
 * uses the FDMA to read the teletext data from memory.
 */

#include <display/ip/stmteletext.h>


class CSTmTVOutTeletext: public CSTmTeletext
{
public:
  CSTmTVOutTeletext(CDisplayDevice* pDev,
                    uint32_t        uRegDENCOffset,
                    uint32_t        uRegTXTOffset,
                    uint32_t        uPhysRegisterBase,
                    STM_DMA_TRANSFER_PACING pacing);

  virtual ~CSTmTVOutTeletext(void);

  virtual bool Create();

  virtual void UpdateHW(void);

  void DMACompleted(void);

protected:
  uint32_t                m_uTXTOffset;
  STM_DMA_TRANSFER_PACING m_pacingLine;
  DMA_Channel            *m_teletextDMAChannel;
  DMA_Area                m_teletextDataRegister;
  int                     m_nTVOutDelay;

  virtual stm_display_metadata_result_t ConstructTeletextNode(stm_display_metadata_t *,int);
  virtual void ReleaseHardwareNode(stm_teletext_hw_node *);
  virtual void StopDMAEngine(void);

  void WriteTXTReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uTXTOffset+reg), val); }
  uint32_t ReadTXTReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uTXTOffset+reg)); }

private:
  struct stm_teletext_hw_node m_CompletedNode; // Private use in DMACompleted to keep it off the stack

  CSTmTVOutTeletext(const CSTmTVOutTeletext&);
  CSTmTVOutTeletext& operator=(const CSTmTVOutTeletext&);
};

#endif // _STM_TVOUT_TELETEXT_H
