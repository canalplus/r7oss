/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutteletext.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
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

#include <STMCommon/stmteletext.h>


class CSTmTVOutTeletext: public CSTmTeletext
{
public:
  CSTmTVOutTeletext(CDisplayDevice* pDev,
                    ULONG           ulRegDENCOffset,
                    ULONG           ulRegTVOutOffset,
                    ULONG           ulPhysRegisterBase,
                    STM_DMA_TRANSFER_PACING pacing);

  virtual ~CSTmTVOutTeletext(void);

  virtual bool Create();

  void FlushMetadata(stm_meta_data_type_t);

  virtual void UpdateHW(void);

  void DMACompleted(void);

protected:
  ULONG                   m_ulTVOutOffset;
  STM_DMA_TRANSFER_PACING m_pacingLine;
  DMA_Channel            *m_teletextDMAChannel;
  DMA_Area                m_teletextDataRegister;
  int                     m_nTVOutDelay;

  virtual stm_meta_data_result_t ConstructTeletextNode(stm_meta_data_t *,int);
  virtual void ReleaseHardwareNode(stm_teletext_hw_node *);
  virtual void StopDMAEngine(void);

  void WriteTVOutReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulTVOutOffset+reg)>>2), val); }
  ULONG ReadTVOutReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulTVOutOffset+reg)>>2)); }

private:
  CSTmTVOutTeletext(const CSTmTVOutTeletext&);
  CSTmTVOutTeletext& operator=(const CSTmTVOutTeletext&);
};

#endif // _STM_TVOUT_TELETEXT_H
